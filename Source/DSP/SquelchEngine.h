#pragma once
// R-Squelch — the synth engine: voice allocation (mono/legato/glide or poly),
// parameter smoothing and the output stage. Pure C++ — no JUCE — so it can be tested offline.

#include "Voice.h"
#include <array>
#include <atomic>

namespace rsq
{
class SquelchEngine
{
public:
    static constexpr int kMaxVoices = 8;

    void prepare (float sr)
    {
        sampleRate = sr;
        for (auto& v : voices) v.prepare (sr);
        for (auto* sm : allSmoothers()) sm->setTime (0.012f, sr);
        dcL.prepare (sr); dcR.prepare (sr);
        snapSmoothers();
        heldCount = 0;
    }

    void setParams (const EngineParams& p)
    {
        params = p;
        params.unison = std::max (1, std::min (Voice::kMaxUnison, p.unison));
        for (auto& v : voices) v.updateEnvelopes (params);
    }

    void snapSmoothers()
    {
        smCutoff.reset (std::log2 (std::max (20.0f, params.cutoffHz)));
        smReso.reset (params.resonance);   smWidth.reset (params.width);
        smFDrive.reset (params.filterDrive); smDrive.reset (params.drive);
        smMorph.reset (params.morph);      smPw.reset (params.pulseWidth);
        smSub.reset (params.subLevel);     smDetune.reset (params.detune);
        smSync.reset (1.0f + 7.0f * params.sync);
        smFm.reset (params.fm);
        smSpread.reset (params.spread);    smGain.reset (params.outputGain);
    }

    // ---------------- MIDI ----------------
    void noteOn (int note, float velocity01)
    {
        if (params.voiceMode == 0) monoNoteOn (note, velocity01);
        else polyNoteOn (note, velocity01);
        lastNote = note;
    }

    void noteOff (int note)
    {
        if (params.voiceMode == 0) { monoNoteOff (note); return; }
        for (auto& v : voices)
            if (v.isActive() && ! v.isReleasing() && v.getNote() == note) v.release();
    }

    void allNotesOff()
    {
        heldCount = 0;
        for (auto& v : voices) if (v.isActive()) v.release();
    }

    void setPitchBend (float minus1to1) { bend = clampf (minus1to1, -1.0f, 1.0f); }

    // Song position for BAR SYNC motion (call once per block, before render)
    void setTransport (double bpmIn, bool isPlaying, double ppqAtBlockStart)
    {
        bpm = bpmIn > 1.0 ? bpmIn : 145.0;
        hostPlaying = isPlaying;
        blockPpq = ppqAtBlockStart;
        samplesIntoBlock = 0;
    }

    // ---------------- Audio ----------------
    void render (float* left, float* right, int numSamples)
    {
        const float bendSemis = bend * params.bendRange;
        const float cutoffTarget = std::log2 (clampf (params.cutoffHz, 20.0f, 20000.0f));

        for (int i = 0; i < numSamples; ++i)
        {
            SmoothedFrame f;
            f.cutoffLog2  = smCutoff.process (cutoffTarget);
            f.resonance   = smReso.process (params.resonance);
            f.width       = smWidth.process (params.width);
            f.filterDrive = smFDrive.process (params.filterDrive);
            f.drive       = smDrive.process (params.drive);
            f.morph       = smMorph.process (params.morph);
            f.pulseWidth  = smPw.process (params.pulseWidth);
            f.subLevel    = smSub.process (params.subLevel);
            f.detune      = smDetune.process (params.detune);
            f.syncRatio   = smSync.process (1.0f + 7.0f * clampf (params.sync, 0.0f, 1.0f));
            f.fm          = smFm.process (clampf (params.fm, 0.0f, 1.0f));
            f.spread      = smSpread.process (params.spread);
            const float gain = smGain.process (params.outputGain);

            float l = 0.0f, r = 0.0f;
            for (auto& v : voices) v.renderSample (f, params, bendSemis, l, r);
            // the FREE clock keeps running whether a note plays or not
            freeFilterPos += (double) clampf (params.motionRate, 0.0f, 40.0f) / (double) sampleRate;
            freePitchPos  += (double) clampf (params.pitchRate, 0.0f, 40.0f) / (double) sampleRate;
            if (freeFilterPos > 1.0e6) freeFilterPos -= 1.0e6;
            if (freePitchPos > 1.0e6) freePitchPos -= 1.0e6;
            ++samplesIntoBlock;

            l = softClip (dcL.process (l * gain));
            r = softClip (dcR.process (r * gain));
            left[i] = l;
            right[i] = r;
        }

        // publish live state for the UI (lock-free)
        const Voice* newest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;
        if (params.voiceMode == 0) newest = voices[0].isActive() ? &voices[0] : nullptr;
        uiCutoffHz.store (newest ? newest->getLastCutoff() : 0.0f, std::memory_order_relaxed);
        uiAmp.store (newest ? newest->getLastAmp() : 0.0f, std::memory_order_relaxed);
        uiFilterPhase.store (newest ? newest->getFilterPhase() : -1.0f, std::memory_order_relaxed);
        uiPitchPhase.store (newest ? newest->getPitchPhase() : -1.0f, std::memory_order_relaxed);
        uiFilterMove.store (newest ? newest->getFilterMove() : -1.0f, std::memory_order_relaxed);
    }

    std::atomic<float> uiCutoffHz { 0.0f }, uiAmp { 0.0f }, uiFilterPhase { -1.0f }, uiPitchPhase { -1.0f }, uiFilterMove { -1.0f };

    int getNumActiveVoices() const
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

    static float softClip (float x)
    {
        const float t = 0.9f, a = std::abs (x);
        if (a <= t) return x;
        const float y = t + (1.0f - t) * fastTanh ((a - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

private:
    // ---- mono: last-note priority with a held-note stack (returns to held notes on release) ----
    void monoNoteOn (int note, float vel)
    {
        auto& v = voices[0];
        const bool wasHeld = heldCount > 0 && v.isActive();
        removeHeld (note);
        if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;
        lastVelocity = vel;

        const bool legatoNow = params.legato && wasHeld;
        const bool glide = params.glideTime > 0.0f && (params.glideMode == 0 ? v.isActive() : wasHeld);
        double fp = 0.0, pp = 0.0;
        motionStart (fp, pp);
        v.start (note, vel, ! legatoNow, glide, params, rng, fp, pp);
    }

    // where the motion LFOs start for a new note, depending on the motion mode
    void motionStart (double& filterPos, double& pitchPos) const
    {
        filterPos = pitchPos = 0.0;                                         // RETRIG / ONE SHOT
        if (params.motionMode == 1 || (params.motionMode == 2 && ! hostPlaying))
        {
            filterPos = freeFilterPos; pitchPos = freePitchPos;            // FREE: the clock that never stops
        }
        else if (params.motionMode == 2)
        {
            // BAR SYNC: cycles elapsed since the song start at the current song position
            const double beats = blockPpq + (double) samplesIntoBlock * bpm / 60.0 / (double) sampleRate;
            const double cyclesPerBeat = 60.0 / bpm;                        // x rate (Hz) = cycles per beat
            filterPos = beats * cyclesPerBeat * (double) params.motionRate;
            pitchPos = beats * cyclesPerBeat * (double) params.pitchRate;
        }
        if (params.pitchLinked) pitchPos = filterPos;
    }

    void monoNoteOff (int note)
    {
        auto& v = voices[0];
        const bool wasPlaying = heldCount > 0 && held[(size_t) heldCount - 1] == note;
        removeHeld (note);
        if (heldCount == 0) { v.release(); return; }
        if (wasPlaying)
        {
            const int back = held[(size_t) heldCount - 1];
            double fp = 0.0, pp = 0.0;
            motionStart (fp, pp);
            v.start (back, lastVelocity, ! params.legato, params.glideTime > 0.0f, params, rng, fp, pp);
        }
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        heldCount = w;
    }

    // ---- poly: reuse same note, else free voice, else steal the oldest ----
    void polyNoteOn (int note, float vel)
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (v.isActive() && v.getNote() == note) { target = &v; break; }
        if (! target) for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (! target)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age < target->age) target = &v;
        }
        const bool glide = params.glideTime > 0.0f && params.glideMode == 0;
        double fp = 0.0, pp = 0.0;
        motionStart (fp, pp);
        target->start (note, vel, true, glide, params, rng, fp, pp);
        target->age = ++ageCounter;
    }

    std::array<OnePole*, 13> allSmoothers()
    {
        return { &smCutoff, &smReso, &smWidth, &smFDrive, &smDrive, &smMorph, &smPw, &smSub, &smDetune, &smSpread, &smGain, &smSync, &smFm };
    }

    float sampleRate = 44100.0f;
    double bpm = 145.0, blockPpq = 0.0, freeFilterPos = 0.0, freePitchPos = 0.0;
    bool hostPlaying = false;
    int64_t samplesIntoBlock = 0;
    EngineParams params;
    std::array<Voice, kMaxVoices> voices;
    std::array<int, 32> held {};
    int heldCount = 0, lastNote = 60;
    float lastVelocity = 1.0f, bend = 0.0f;
    uint64_t ageCounter = 0;
    Rng rng;
    DcBlocker dcL, dcR;
    OnePole smCutoff, smReso, smWidth, smFDrive, smDrive, smMorph, smPw, smSub, smDetune, smSpread, smGain, smSync, smFm;
};
} // namespace rsq
