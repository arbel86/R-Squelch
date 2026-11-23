#pragma once
// R-Squelch — one synth voice: unison oscillators -> drive -> band-pass -> amp.

#include "Oscillator.h"
#include "BandPassFilter.h"
#include "Envelope.h"
#include "EngineParams.h"
#include "MotionShapes.h"
#include "CombFilter.h"

namespace rsq
{
// Per-sample smoothed values shared by all voices.
struct SmoothedFrame
{
    float cutoffLog2, resonance, width, filterDrive, drive, morph, pulseWidth, subLevel, detune, spread, syncRatio, fm;
};

class Voice
{
public:
    static constexpr int kMaxUnison = 7;

    void prepare (float sr)
    {
        sampleRate = sr;
        filterL.prepare (sr); filterR.prepare (sr);
        combL.prepare (sr); combR.prepare (sr);
        fEnv.prepare (sr); aEnv.prepare (sr); pEnv.prepare (sr);
        static uint32_t seedCounter = 0x9E3779B9u;
        for (auto& o : osc) { o.reset (rngPhase.next01()); o.setSeed (seedCounter += 0x6D2B79F5u); }   // each osc drifts its own way
        active = false;
    }

    void updateEnvelopes (const EngineParams& p)
    {
        fEnv.setParameters (p.fA, p.fD, p.fS, p.fR, p.fEnvCurve);
        aEnv.setParameters (p.aA, p.aD, p.aS, p.aR, 0.45f);
        pEnv.setParameters (p.pA, p.pD, p.pEnvCurve);
    }

    // retrigger = restart envelopes; glide = slide from the previous pitch instead of jumping.
    // startFilterPos / startPitchPos: where the motion LFOs begin, in cycles (0 = RETRIG; FREE / BAR SYNC pass the running clock)
    void start (int midiNote, float velocity01, bool retrigger, bool glide, const EngineParams& p, Rng& rng,
                double startFilterPos = 0.0, double startPitchPos = 0.0)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
        if (! glide || ! hasPlayed) currentPitch = targetPitch;
        glideCoeff = (glide && p.glideTime > 0.0f) ? std::exp (-4.6f / (p.glideTime * sampleRate)) : 0.0f;

        if (retrigger)
        {
            velocity = clampf (velocity01, 0.0f, 1.0f);
            accent = velocity >= p.accentThreshold ? p.accentAmount : 0.0f;

            // WARM (Moog): free-running oscillators, like analog VCOs.
            // BRIGHT (digital psy): every note starts at the same point of the wave -> identical, punchy attack.
            // Only reset from (near) silence, so overlapping notes never click.
            if (p.oscCharacter == 1 && aEnv.getLevel() < 0.02f)
                for (int k = 0; k < kMaxUnison; ++k)   // unison voices start spread out, so the attack doesn't flange
                    osc[k].reset (k == 0 ? 0.0f : std::fmod ((float) k * 0.6180339f, 1.0f));
            (void) rng;
            pitchSeed = motion::pitchSeedFor (p.randomSeed);    // RANDOM / S&H: the pattern chosen by the seed (dice)
            if (! active) { filterL.reset(); filterR.reset(); combL.reset(); combR.reset(); }

            fEnv.noteOn();
            aEnv.noteOn();
            pEnv.trigger();
            // motion position in cycles (the shapes below set where each starts); cycle counts drive the RANDOM loop
            const double fp = std::max (0.0, startFilterPos), pp = std::max (0.0, startPitchPos);
            filterPhase = (float) (fp - std::floor (fp));
            pitchPhase = (float) (pp - std::floor (pp));
            filterCycle = (uint32_t) ((int64_t) std::floor (fp) & 0x3fffffff);
            pitchCycle = (uint32_t) ((int64_t) std::floor (pp) & 0x3fffffff);
            prevPitchPhase = pitchPhase;
            oneShotDone = false;
            freshMove = true;
            filterSeed = motion::filterSeedFor (p.randomSeed);
        }
        active = true;
        hasPlayed = true;
    }

    void release() { fEnv.noteOff(); aEnv.noteOff(); }

    bool isActive() const { return active; }
    bool isReleasing() const { return aEnv.getStage() == AdsrEnvelope::Stage::Release; }
    int getNote() const { return note; }
    float getCurrentPitch() const { return currentPitch; }

    uint64_t age = 0;   // for voice stealing

    // last values, read by the UI (filter display shows the live, modulated cutoff)
    float getLastCutoff() const { return lastCutoff; }
    float getLastAmp() const { return lastAmp; }
    float getFilterPhase() const { return filterPhase; }
    float getPitchPhase() const { return pitchPhase; }
    float getFilterMove() const { return lastFilterMove; }   // 0..1 position inside the filter range

    inline void renderSample (const SmoothedFrame& s, const EngineParams& p, float bendSemis, float& outL, float& outR)
    {
        if (! active) return;

        currentPitch = targetPitch + glideCoeff * (currentPitch - targetPitch);

        const float fe = fEnv.process();
        const float ae = aEnv.process();
        const float pe = pEnv.process();
        // ---- motion: triangle 0 -> 1 -> 0, repeating ----
        // ONE SHOT: a single cycle from the note, then the motion holds where the cycle ends
        if (! (p.motionMode == 3 && oneShotDone))
        {
            filterPhase += clampf (p.motionRate, 0.0f, 40.0f) / sampleRate;
            if (filterPhase >= 1.0f)
            {
                if (p.motionMode == 3) { filterPhase = 0.99999f; oneShotDone = true; }
                else { filterPhase -= 1.0f; ++filterCycle; }
            }
            if (p.pitchLinked) pitchPhase = filterPhase;
            else
            {
                pitchPhase += clampf (p.pitchRate, 0.0f, 40.0f) / sampleRate;
                if (pitchPhase >= 1.0f) pitchPhase = p.motionMode == 3 ? 0.99999f : pitchPhase - 1.0f;
            }
        }
        // the filter starts at the TOP of its range (right where RANGE is set) and moves down and back from there;
        // the pitch starts on the played note and moves up and back
        // a ~2 ms slew rounds off the jumps of JUMP / STEPS / S&H so the filter never clicks
        const float moveTarget = motion::filterShapeValue (p.filterShape, filterPhase, filterCycle, filterSeed);
        if (freshMove) { lastFilterMove = moveTarget; freshMove = false; }
        lastFilterMove += (1.0f - std::exp (-1.0f / (0.002f * sampleRate))) * (moveTarget - lastFilterMove);
        const float filterMove = lastFilterMove;
        if (pitchPhase < prevPitchPhase - 0.5f) ++pitchCycle;   // a new cycle began (counts RANDOM steps)
        prevPitchPhase = pitchPhase;
        const float pitchMove = motion::shapeValue (p.pitchShape, pitchPhase, pitchCycle, pitchSeed);

        if (! aEnv.isActive()) { active = false; lastAmp = 0.0f; return; }

        // ---- pitch ----
        const float pitch = currentPitch + (float) (p.octave * 12 + p.semitone) + p.fineCents * 0.01f
                          + bendSemis + pe * p.pEnvAmount + fe * p.filterEnvToPitch
                          + pitchMove * p.pitchRange;

        // ---- oscillators (unison) ----
        const int n = std::max (1, std::min (kMaxUnison, p.unison));
        float l = 0.0f, r = 0.0f, mono = 0.0f;
        float subSample = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float pos = n == 1 ? 0.0f : ((float) i / (float) (n - 1)) * 2.0f - 1.0f;   // -1..1
            const float f = midiToHz (pitch + pos * s.detune * 0.5f);
            float m, sub;
            osc[i].setDriftEnabled (p.analogDrift && p.oscCharacter == 0);   // digital = rock-steady pitch
            osc[i].setBright (p.oscCharacter == 1);
            osc[i].setFm (s.fm, p.fmRatio);
            osc[i].setAcid (p.acid);
            osc[i].process (f, sampleRate, s.morph, s.pulseWidth, s.syncRatio, m, sub);
            if (i == 0) subSample = sub;
            if (n == 1) { mono = m; }
            else
            {
                const float pan = pos * s.spread;
                l += m * std::sqrt (1.0f - pan);
                r += m * std::sqrt (1.0f + pan);
            }
        }

        // ---- velocity / accent ----
        const float velF = 1.0f - p.velToFilter * (1.0f - velocity);
        const float velA = 1.0f - p.velToAmp * (1.0f - velocity);

        // ---- filter modulation (in octaves) ----
        const float envOct = p.fEnvAmount * 6.0f * fe * velF * (1.0f + accent);
        const float keyOct = (currentPitch - 60.0f) / 12.0f * p.keyTrack;
        const float cutoff = std::exp2 (s.cutoffLog2 + envOct + keyOct + filterMove * p.filterRange);
        const float reso = clampf (s.resonance + accent * 0.12f, 0.0f, 1.0f);
        const auto slope = p.slope == 3 ? BandPassFilter::Slope::tb303
                         : p.slope == 2 ? BandPassFilter::Slope::sh5
                         : (p.slope == 1 ? BandPassFilter::Slope::dB24 : BandPassFilter::Slope::dB12);
        // GROWL: the oscillator itself wobbles the cutoff at audio rate (up to +-2.5 octaves) — vocal, growling squelch
        const float oscSig = n == 1 ? mono : (l + r) * 0.5f / std::sqrt ((float) n);
        const float fCut = p.growl > 0.001f ? cutoff * std::exp2 (p.growl * 2.5f * clampf (oscSig, -1.0f, 1.0f)) : cutoff;
        // FOLD: sine wavefolder after the filter — the resonant peak folds over into metallic overtones
        // FLANGE +/-: the comb frequency FOLLOWS THE MAIN FILTER CUTOFF (squelch motion, envelope, S&H, even GROWL),
        // offset by FREQ, bent further by its own LFO; SPREAD pulls left and right apart
        float combDelayL = 0.0f, combDelayR = 0.0f;
        if (p.combOn)
        {
            combLfoPhase += clampf (p.combLfoHz, 0.0f, 40.0f) / sampleRate;
            if (combLfoPhase >= 1.0f) combLfoPhase -= 1.0f;
            auto tri = [] (float ph) { ph -= std::floor (ph); return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; };   // -1..1
            const float lfoL = tri (combLfoPhase), lfoR = tri (combLfoPhase + 0.5f * p.combSpread);
            auto delayFor = [&] (float lfo, float detune)
            {
                const float hz = fCut * std::exp2 ((p.combPitch + p.combDepth * 24.0f * lfo + detune) / 12.0f);
                return sampleRate / clampf (hz, 16.0f, sampleRate * 0.45f);
            };
            combDelayL = delayFor (lfoL, 0.0f);
            combDelayR = delayFor (lfoR, p.combSpread * 0.5f);
        }
        auto comb = [&] (CombFilter& c, float y, bool right)
        {
            if (! p.combOn) return y;
            const int mode = p.combMode == CombFilter::flangeBoth ? (right ? CombFilter::flangeMinus : CombFilter::flangePlus) : p.combMode;
            return c.process (y, right ? combDelayR : combDelayL, p.combFeedback, p.combDamp, p.combMix, p.combDrive, mode);
        };
        auto fold = [&] (float y)
        {
            if (p.fold < 0.001f) return y;
            const float folded = sd::sineFold (y * (1.0f + 4.0f * p.fold));
            return (y + p.fold * (folded - y)) * (1.0f - 0.45f * p.fold);   // folding adds level: take it back
        };

        // ---- drive (pre-filter) ----
        const float dg = 1.0f + s.drive * 5.0f;
        const float dNorm = 1.0f / fastTanh (dg);
        const float amp = ae * velA * (1.0f + accent * 0.3f);
        lastCutoff = cutoff;
        lastAmp = ae;

        if (n == 1)
        {
            float x = mono + subSample * s.subLevel;
            x = fastTanh (x * dg) * dNorm;
            const float y = fold (filterL.process (x, fCut, reso, s.width, s.filterDrive, slope));
            if (p.combOn)   // the comb turns the mono line into stereo (left and right sweep apart)
            {
                outL += comb (combL, y, false) * amp;
                outR += comb (combR, y, true) * amp;
            }
            else
            {
                outL += y * amp;
                outR += y * amp;
            }
        }
        else
        {
            const float g = 1.0f / std::sqrt ((float) n);
            float xl = l * g + subSample * s.subLevel;
            float xr = r * g + subSample * s.subLevel;
            xl = fastTanh (xl * dg) * dNorm;
            xr = fastTanh (xr * dg) * dNorm;
            outL += comb (combL, fold (filterL.process (xl, fCut, reso, s.width, s.filterDrive, slope)), false) * amp;
            outR += comb (combR, fold (filterR.process (xr, fCut, reso, s.width, s.filterDrive, slope)), true) * amp;
        }
    }

private:
    float sampleRate = 44100.0f;
    Oscillator osc[kMaxUnison];
    Rng rngPhase;
    BandPassFilter filterL, filterR;
    CombFilter combL, combR;
    float combLfoPhase = 0.0f;
    AdsrEnvelope fEnv, aEnv;
    AdEnvelope pEnv;

    int note = 60;
    float currentPitch = 60.0f, targetPitch = 60.0f, glideCoeff = 0.0f;
    float velocity = 1.0f, accent = 0.0f;
    bool active = false, hasPlayed = false;
    float lastCutoff = 1000.0f, lastAmp = 0.0f, filterPhase = 0.0f, pitchPhase = 0.0f, prevPitchPhase = 0.0f;
    uint32_t pitchCycle = 0, pitchSeed = 0x51ED27u, filterCycle = 0, filterSeed = 0xF17E5u;
    float lastFilterMove = 1.0f;
    bool freshMove = true, oneShotDone = false;

};
} // namespace rsq
