#pragma once
// R-Squelch — effects chain: Tube -> Formant -> Bitcrusher -> Trance Gate -> Ping-pong Delay -> Reverb.
// Pure C++ (no JUCE) so every effect can be unit-tested offline. All buffers are allocated in prepare().

#include "DspUtils.h"
#include "BandPassFilter.h"
#include "synth_dsp.h"
#include <array>
#include <atomic>
#include <vector>

namespace rsq
{
struct FxParams
{
    // Tube distortion (one knob: 0 = off)
    float tubeDrive = 0.0f;      // 0..1
    int   tubeType = 0;          // 0 Tube, 1 Fuzz, 2 Square

    // Formant
    bool  formantOn = false;
    float vowel = 1.5f;          // 0..4  (A, E, I, O, U — in between = morph)
    float formantMix = 0.6f;
    float formantLfoDepth = 0.0f;   // 0..1: how far the vowel swings around VOWEL (1 = +-2 vowels)
    int   formantLfoRate = 4;       // index into lfoBeats(): 1/16 1/8 1/4 1/2 1 bar 2 bars 4 bars

    static constexpr int numLfoRates = 7;
    static double lfoBeats (int index)                                  // length of one LFO cycle in beats
    {
        constexpr double beats[numLfoRates] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
        return beats[std::max (0, std::min (numLfoRates - 1, index))];
    }

    // Bitcrusher
    bool  crushOn = false;
    float bits = 8.0f;           // 1..16
    float downsample = 1.0f;     // 1..32 (sample-rate reduction factor)
    float crushMix = 1.0f;

    // Trance gate
    bool  gateOn = false;
    int   gateRate = 1;          // 0 = 1/8, 1 = 1/16, 2 = 1/32
    float gateLength = 0.7f;     // 0.1..1 of each step
    float gateDepth = 1.0f;      // 0..1
    std::array<bool, 16> gateSteps { true, false, true, true,  true, false, true, true,
                                     true, false, true, true,  true, false, true, false };

    // Phaser: 6 all-pass stages swept by a tempo-synced LFO (left and right a quarter cycle apart)
    bool  phaserOn = false;
    int   phaserRate = 5;           // index into lfoBeats(): 1/16 1/8 1/4 1/2 1 bar 2 bars 4 bars
    float phaserDepth = 0.7f;       // 0..1 sweep width (up to 6 octaves)
    float phaserFeedback = 0.6f;    // -0.9..0.9 (more = sharper, whistling notches)
    float phaserMix = 0.8f;         // 0..1

    // Delay
    bool  delayOn = true;
    int   delayTime = 2;         // index into delayBeats()
    int   delayFeel = 0;         // 0 = normal, 1 = dotted (x 1.5), 2 = triplet (x 2/3)
    float delayFeedback = 0.35f;
    float delayMix = 0.18f;

    // Reverb
    bool  reverbOn = true;
    float reverbSize = 0.55f;    // 0..1
    float reverbTone = 0.6f;     // 0 = dark, 1 = bright
    float reverbMix = 0.15f;

    static constexpr int numDelayTimes = 6;
    static float delayBeats (int index)
    {
        constexpr float beats[numDelayTimes] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };   // 1/16 1/8 3/16 1/4 3/8 1/2
        return beats[std::max (0, std::min (numDelayTimes - 1, index))];
    }
};

//==============================================================================
// Distortion, three types (all anti-aliased with first-order ADAA, all level-compensated so turning the
// knob up changes the character, not just the volume; sub-lows below ~70 Hz bypass the clipper so the
// low end stays tight):
//   Tube   - biased (asymmetric) tanh -> even + odd harmonics, warm, darkens as you push it.
//   Fuzz   - octave fuzz: a rectified copy is mixed in before an asymmetric hard clip -> lopsided pulse,
//            strong octave-up and a sputtery, gated bite.
//   Square - the most destructive one: absurd gain (up to ~2000x) into a brick-wall hard clip. Whatever
//            goes in comes out as a full-scale square wave that only keeps the zero crossings.
class TubeDistortion
{
public:
    enum Type { tube = 0, fuzz = 1, square = 2 };

    void prepare (float sr)
    {
        sampleRate = sr;
        for (auto& c : ch) c = {};
        hpCoeff = std::exp (-kTwoPi * 70.0f / sr);
        envCoeff = 1.0f - std::exp (-kTwoPi * 15.0f / sr);
        driveSm.setTime (0.02f, sr);
        driveSm.reset (0.0f);
        typeMix = 1.0f;
    }

    void process (float& l, float& r, float drive01, int type = tube)
    {
        type = std::max (0, std::min (2, type));
        if (type != curType)                       // short duck while switching type: no click
        {
            typeMix = std::max (0.0f, typeMix - 1.0f / (0.004f * sampleRate));
            if (typeMix <= 0.0f) { curType = type; for (auto& c : ch) c.prevU = 0.0f; }
        }
        else if (typeMix < 1.0f)
            typeMix = std::min (1.0f, typeMix + 1.0f / (0.004f * sampleRate));

        const float d = driveSm.process (clampf (drive01, 0.0f, 1.0f));
        if (d < 1.0e-4f) return;
        Stage s;
        if (curType == tube)
        {
            s.gain = std::exp2 (d * 5.0f) * (1.0f + d * 2.0f);                    // ~1x .. 96x
            s.lpCoeff = std::exp (-kTwoPi * (16000.0f - 9000.0f * d) / sampleRate);
            s.comp = 1.0f / (0.55f + 0.45f * std::sqrt (s.gain)) * levelFix (tubeFix, d) * 1.6f;
        }
        else if (curType == fuzz)
        {
            s.gain = std::exp2 (2.0f + d * 7.0f);                                  // 4x .. 512x
            s.lpCoeff = std::exp (-kTwoPi * (13000.0f - 5000.0f * d) / sampleRate);
            s.comp = levelFix (fuzzFix, d);
        }
        else
        {
            s.gain = std::exp2 (3.0f + d * 8.0f);                                  // 8x .. 2048x
            s.lpCoeff = std::exp (-kTwoPi * 19000.0f / sampleRate);               // no mercy: stays bright
            s.comp = levelFix (squareFix, d);
        }
        s.wet = std::min (1.0f, d * 12.0f) * typeMix;                             // fades in from "off"
        l = shape (ch[0], l, s);
        r = shape (ch[1], r, s);
    }

private:
    struct Channel { float hp = 0.0f, prevU = 0.0f, lp = 0.0f, dcX = 0.0f, dcY = 0.0f, env = 0.0f; };
    struct Stage { float gain = 1.0f, lpCoeff = 0.0f, comp = 1.0f, wet = 0.0f; };

    static constexpr float kBias = 0.35f;   // tube asymmetry -> even harmonics
    static constexpr float kFuzzLo = 0.55f; // fuzz clips its negative half earlier -> even harmonics + sputter

    // measured on the squelch itself: keeps the loudness steady (~+2 dB over clean) across the whole knob
    static constexpr float tubeFix[11]   = { 1.0f, 0.686f, 0.513f, 0.424f, 0.388f, 0.394f, 0.429f, 0.488f, 0.567f, 0.669f, 0.795f };
    static constexpr float fuzzFix[11]   = { 0.375f, 0.375f, 0.352f, 0.340f, 0.334f, 0.330f, 0.329f, 0.328f, 0.328f, 0.328f, 0.328f };
    static constexpr float squareFix[11] = { 0.274f, 0.274f, 0.259f, 0.251f, 0.246f, 0.243f, 0.242f, 0.241f, 0.240f, 0.240f, 0.240f };

    static float levelFix (const float* table, float d)
    {
        const float x = clampf (d, 0.0f, 1.0f) * 10.0f;
        const int i = std::min (9, (int) x);
        return table[i] + (x - (float) i) * (table[i + 1] - table[i]);
    }

    // log(cosh(u)), numerically safe: the antiderivative of tanh
    static float logCosh (float u) { const float a = std::abs (u); return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f; }

    // hard clip to [-lo, hi] and its antiderivative (for ADAA)
    static float clip (float u, float lo, float hi) { return std::max (-lo, std::min (hi, u)); }
    static float clipAD (float u, float lo, float hi)
    {
        if (u > hi)  return hi * u - 0.5f * hi * hi;
        if (u < -lo) return -lo * u - 0.5f * lo * lo;
        return 0.5f * u * u;
    }

    float nonlinear (float u, float prev) const
    {
        const float du = u - prev;
        const bool tiny = std::abs (du) < 1.0e-4f;
        if (curType == tube)
            return (tiny ? std::tanh (0.5f * (u + prev)) : (logCosh (u) - logCosh (prev)) / du) - std::tanh (kBias);
        const float lo = curType == fuzz ? kFuzzLo : 1.0f;
        return tiny ? clip (0.5f * (u + prev), lo, 1.0f) : (clipAD (u, lo, 1.0f) - clipAD (prev, lo, 1.0f)) / du;
    }

    float shape (Channel& c, float x, const Stage& s)
    {
        c.hp = x + hpCoeff * (c.hp - x);
        const float pre = x - c.hp;                                // keep the sub-lows out of the clipper (no mud)
        float u = pre * s.gain;
        if (curType == tube) u += kBias;
        else if (curType == fuzz)
        {
            // add a rectified copy (minus its average) before clipping: shifts the zero crossings, so the
            // clipped wave becomes a lopsided pulse -> strong octave (2nd harmonic) and that sputtery fuzz bite
            const float a = std::abs (pre);
            c.env = undenormalise (c.env + envCoeff * (a - c.env));
            u += s.gain * 0.9f * (a - c.env);
        }
        const float y = nonlinear (u, c.prevU);
        c.prevU = u;
        c.lp = y + s.lpCoeff * (c.lp - y);                         // speaker roll-off
        const float dc = c.lp - c.dcX + 0.995f * c.dcY;           // remove the DC the asymmetry creates
        c.dcX = c.lp;
        c.dcY = undenormalise (dc);
        const float out = dc * s.comp + (x - pre);                 // put the clean sub-lows back underneath
        return x + s.wet * (out - x);
    }

    float sampleRate = 44100.0f, hpCoeff = 0.0f, envCoeff = 0.002f, typeMix = 1.0f;
    int curType = tube;
    Channel ch[2];
    OnePole driveSm;
};

//==============================================================================
class FormantFilter
{
public:
    void prepare (float sr)
    {
        sampleRate = sr;
        for (auto& f : bands) f.reset();
        for (auto& f : bandsR) f.reset();
        powCoeff = std::exp (-1.0f / (0.15f * sr));     // ~150 ms loudness followers
        gainCoeff = std::exp (-1.0f / (0.05f * sr));
        inPow = outPow = 0.0f;
        makeup = 1.0f;
    }

    // Classic vowel formants (F1, F2, F3) and relative gains.
    static void formantsFor (float vowel, float out[3])
    {
        static constexpr float table[5][3] = {
            { 730.0f, 1090.0f, 2440.0f },   // A
            { 530.0f, 1840.0f, 2480.0f },   // E
            { 270.0f, 2290.0f, 3010.0f },   // I
            { 570.0f,  840.0f, 2410.0f },   // O
            { 300.0f,  870.0f, 2240.0f } }; // U
        vowel = clampf (vowel, 0.0f, 4.0f);
        const int i0 = std::min (3, (int) vowel);
        const float t = vowel - (float) i0;
        for (int k = 0; k < 3; ++k)   // interpolate in log-frequency so the morph sounds even
            out[k] = std::exp (std::log (table[i0][k]) * (1.0f - t) + std::log (table[i0 + 1][k]) * t);
    }

    void process (float& l, float& r, float vowel, float mix)
    {
        float f[3];
        formantsFor (vowel, f);
        static constexpr float gains[3] = { 1.0f, 0.6f, 0.35f };
        constexpr float k = 0.12f;   // fairly narrow bands (Q ~ 8)
        float wl = 0.0f, wr = 0.0f, lo, bp, hi;
        for (int i = 0; i < 3; ++i)
        {
            const float g = std::tan (kPi * std::min (f[i], sampleRate * 0.45f) / sampleRate);
            bands[(size_t) i].process (l * kIn, g, k, lo, bp, hi);  wl += bp * k * gains[i];
            bandsR[(size_t) i].process (r * kIn, g, k, lo, bp, hi); wr += bp * k * gains[i];
        }
        wl *= kOut; wr *= kOut;

        // auto make-up gain: the vowel bands pass only part of the signal, so match the loudness of
        // the formant sound to what came in — it changes the colour, not the volume
        const float pin = 0.5f * (l * l + r * r), pout = 0.5f * (wl * wl + wr * wr);
        inPow = pin + powCoeff * (inPow - pin);
        outPow = pout + powCoeff * (outPow - pout);
        const float target = clampf (std::sqrt ((inPow + 1.0e-9f) / (outPow + 1.0e-9f)), 0.5f, 8.0f);
        makeup = target + gainCoeff * (makeup - target);
        wl *= makeup; wr *= makeup;

        // blend halfway between linear and equal-power (the dry and formant sounds are partly alike),
        // so the level stays flat across the whole MIX knob
        const float m = clampf (mix, 0.0f, 1.0f);
        const float dryG = 0.5f * ((1.0f - m) + std::cos (m * kPi * 0.5f));
        const float wetG = 0.5f * (m + std::sin (m * kPi * 0.5f));
        l = l * dryG + wl * wetG;
        r = r * dryG + wr * wetG;
    }

private:
    static constexpr float kIn = 0.25f, kOut = 4.0f * 2.6f;   // headroom inside the (saturating) SVF, make-up after
    float sampleRate = 44100.0f;
    std::array<Svf, 3> bands, bandsR;
    float powCoeff = 0.0f, gainCoeff = 0.0f, inPow = 0.0f, outPow = 0.0f, makeup = 1.0f;
};

//==============================================================================
class Bitcrusher
{
public:
    void reset() { phase = 1.0f; heldL = heldR = 0.0f; }

    void process (float& l, float& r, float bits, float downsample, float mix)
    {
        phase += 1.0f;
        if (phase >= std::max (1.0f, downsample))
        {
            phase -= std::max (1.0f, downsample);
            const float levels = std::exp2 (clampf (bits, 1.0f, 16.0f) - 1.0f);
            heldL = std::round (clampf (l, -1.0f, 1.0f) * levels) / levels;
            heldR = std::round (clampf (r, -1.0f, 1.0f) * levels) / levels;
        }
        l += mix * (heldL - l);
        r += mix * (heldR - r);
    }

private:
    float phase = 1.0f, heldL = 0.0f, heldR = 0.0f;
};

//==============================================================================
// 16-step trance gate locked to the beat position (ppq). Smoothed so steps never click.
class TranceGate
{
public:
    void prepare (float sr) { sampleRate = sr; smoothCoeff = std::exp (-1.0f / (0.0015f * sr)); gain = 1.0f; }

    // ppq = position in quarter notes for this sample
    float process (double ppq, const FxParams& p)
    {
        const double stepsPerBeat = p.gateRate == 0 ? 2.0 : (p.gateRate == 1 ? 4.0 : 8.0);
        const double stepPos = ppq * stepsPerBeat;
        const double fl = std::floor (stepPos);
        const int step = (int) (((long long) fl % 16 + 16) % 16);
        const float within = (float) (stepPos - fl);
        currentStep = step;
        const bool open = p.gateSteps[(size_t) step] && within < clampf (p.gateLength, 0.05f, 1.0f);
        const float target = open ? 1.0f : 1.0f - clampf (p.gateDepth, 0.0f, 1.0f);
        gain = target + smoothCoeff * (gain - target);
        return gain;
    }

    int currentStep = 0;

private:
    float sampleRate = 44100.0f, smoothCoeff = 0.0f, gain = 1.0f;
};

//==============================================================================
class PingPongDelay
{
public:
    void prepare (float sr)
    {
        sampleRate = sr;
        size = (int) (sr * 4.1f) + 4;                    // up to 4 s (a 1/2 note at 30 BPM)
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        write = 0;
        lpL = lpR = hpL = hpR = 0.0f;
        smoothedDelay = -1.0f;
        lpCoeff = std::exp (-kTwoPi * 5500.0f / sr);
        hpCoeff = std::exp (-kTwoPi * 160.0f / sr);
    }

    void process (float& l, float& r, float delaySamples, float feedback, float mix)
    {
        delaySamples = clampf (delaySamples, 1.0f, (float) size - 3.0f);
        if (smoothedDelay < 0.0f) smoothedDelay = delaySamples;
        smoothedDelay += 0.0005f * (delaySamples - smoothedDelay);   // tempo changes glide instead of clicking

        const float dl = read (bufL, smoothedDelay), dr = read (bufR, smoothedDelay);

        // ping-pong: the input enters the left line, each line feeds the other one
        const float fb = clampf (feedback, 0.0f, 0.95f);
        float inL = (l + r) * 0.5f + dr * fb;
        float inR = dl * fb;
        inL = tone (inL, lpL, hpL);
        inR = tone (inR, lpR, hpR);
        bufL[(size_t) write] = undenormalise (fastTanh (inL));
        bufR[(size_t) write] = undenormalise (fastTanh (inR));
        if (++write >= size) write = 0;

        l = l * (1.0f - mix * 0.5f) + dl * mix;   // dry stays strong, echoes are added on top
        r = r * (1.0f - mix * 0.5f) + dr * mix;
    }

private:
    float read (const std::vector<float>& buf, float delay) const
    {
        float pos = (float) write - delay;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float t = pos - (float) i0;
        return buf[(size_t) i0] + t * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    float tone (float x, float& lp, float& hp) const
    {
        lp = x + lpCoeff * (lp - x);          // darker repeats
        hp = lp + hpCoeff * (hp - lp);        // no low-end build-up
        return lp - hp;
    }

    float sampleRate = 44100.0f, smoothedDelay = -1.0f, lpCoeff = 0.0f, hpCoeff = 0.0f;
    float lpL = 0.0f, lpR = 0.0f, hpL = 0.0f, hpR = 0.0f;
    std::vector<float> bufL, bufR;
    int size = 1, write = 0;
};

//==============================================================================
// 8-line feedback delay network reverb with input diffusion and damping.
class FdnReverb
{
public:
    void prepare (float sr)
    {
        sampleRate = sr;
        static constexpr float baseMs[kLines] = { 31.3f, 37.9f, 41.7f, 46.1f, 53.9f, 59.3f, 67.1f, 73.7f };
        for (int i = 0; i < kLines; ++i)
        {
            lines[(size_t) i].assign ((size_t) (baseMs[i] * 0.001f * sr * 1.8f) + 4, 0.0f);
            pos[(size_t) i] = 0;
            damp[(size_t) i] = 0.0f;
        }
        static constexpr float apMs[kDiff] = { 4.7f, 3.6f, 12.7f, 9.3f };
        for (int i = 0; i < kDiff; ++i)
        {
            diff[(size_t) i].assign ((size_t) (apMs[i] * 0.001f * sr) + 1, 0.0f);
            diffPos[(size_t) i] = 0;
        }
        preDelay.assign ((size_t) (0.02f * sr) + 1, 0.0f);
        prePos = 0;
        setParams (0.5f, 0.5f);
    }

    // once per block: line lengths, decay gains and damping from SIZE and TONE
    void setParams (float size01, float tone01)
    {
        static constexpr float baseMs[kLines] = { 31.3f, 37.9f, 41.7f, 46.1f, 53.9f, 59.3f, 67.1f, 73.7f };
        size01 = clampf (size01, 0.0f, 1.0f);
        tone01 = clampf (tone01, 0.0f, 1.0f);
        const float scale = 0.6f + 1.2f * size01;                                  // room size
        const float rt60 = 0.4f + 7.0f * size01 * size01;                          // decay time (s)
        dampCoeff = std::exp (-kTwoPi * (1500.0f + 12000.0f * tone01 * tone01) / sampleRate);
        for (int i = 0; i < kLines; ++i)
        {
            len[(size_t) i] = std::min ((int) lines[(size_t) i].size() - 1, (int) (baseMs[i] * 0.001f * sampleRate * scale));
            gain[(size_t) i] = std::pow (10.0f, -3.0f * (float) len[(size_t) i] / (rt60 * sampleRate));
        }
    }

    void process (float& l, float& r, float mix)
    {
        // pre-delay + input diffusion (series allpasses)
        float x = (l + r) * 0.5f;
        const float pd = preDelay[(size_t) prePos];
        preDelay[(size_t) prePos] = x;
        if (++prePos >= (int) preDelay.size()) prePos = 0;
        x = pd;
        for (int i = 0; i < kDiff; ++i)
        {
            auto& buf = diff[(size_t) i];
            const float d = buf[(size_t) diffPos[(size_t) i]];
            const float v = x + 0.6f * d;
            buf[(size_t) diffPos[(size_t) i]] = undenormalise (v);
            x = d - 0.6f * v;
            if (++diffPos[(size_t) i] >= (int) buf.size()) diffPos[(size_t) i] = 0;
        }

        // read the delay lines
        std::array<float, kLines> out {};
        for (int i = 0; i < kLines; ++i)
        {
            auto& buf = lines[(size_t) i];
            int rp = pos[(size_t) i] - len[(size_t) i];
            if (rp < 0) rp += (int) buf.size();
            const float y = buf[(size_t) rp] * gain[(size_t) i];
            damp[(size_t) i] = y + dampCoeff * (damp[(size_t) i] - y);                     // high-frequency damping
            out[(size_t) i] = damp[(size_t) i];
        }

        // Hadamard mixing (energy preserving) and write back with the new input
        std::array<float, kLines> m = out;
        for (int h = 1; h < kLines; h <<= 1)
            for (int i = 0; i < kLines; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = m[(size_t) j], b = m[(size_t) j + (size_t) h];
                    m[(size_t) j] = a + b;
                    m[(size_t) j + (size_t) h] = a - b;
                }
        const float norm = 1.0f / std::sqrt ((float) kLines);
        for (int i = 0; i < kLines; ++i)
        {
            auto& buf = lines[(size_t) i];
            buf[(size_t) pos[(size_t) i]] = undenormalise (m[(size_t) i] * norm + x * ((i & 1) ? -0.5f : 0.5f));
            if (++pos[(size_t) i] >= (int) buf.size()) pos[(size_t) i] = 0;
        }

        const float wl = (out[0] + out[2] + out[4] + out[6]) * 0.5f;
        const float wr = (out[1] + out[3] + out[5] + out[7]) * 0.5f;
        l = l * (1.0f - mix * 0.5f) + wl * mix;
        r = r * (1.0f - mix * 0.5f) + wr * mix;
    }

private:
    static constexpr int kLines = 8, kDiff = 4;
    float sampleRate = 44100.0f;
    std::array<std::vector<float>, kLines> lines;
    std::array<int, kLines> pos {};
    std::array<float, kLines> damp {}, gain {};
    std::array<int, kLines> len {};
    float dampCoeff = 0.0f;
    std::array<std::vector<float>, kDiff> diff;
    std::array<int, kDiff> diffPos {};
    std::vector<float> preDelay;
    int prePos = 0;
};

//==============================================================================
class FxChain
{
public:
    void prepare (float sr)
    {
        sampleRate = sr;
        tube.prepare (sr);
        formant.prepare (sr);
        crusher.reset();
        gate.prepare (sr);
        delay.prepare (sr);
        reverb.prepare (sr);
        phaserL = {}; phaserR = {};
        internalPpq = 0.0;
    }

    void setParams (const FxParams& p) { params = p; }

    // Tempo info for this block. If the host is not playing, the gate/delay run on an internal clock.
    void setTransport (double bpmIn, bool hostPlaying, double hostPpqAtBlockStart)
    {
        bpm = bpmIn > 1.0 ? bpmIn : 145.0;
        playing = hostPlaying;
        if (playing) internalPpq = hostPpqAtBlockStart;
    }

    // Restart the internal beat clock (on a new note when the host isn't playing), so step 1 lands on the note.
    void restartClock() { if (! playing) internalPpq = 0.0; }

    void process (float* left, float* right, int numSamples)
    {
        const double ppqPerSample = bpm / 60.0 / (double) sampleRate;
        if (params.reverbOn) reverb.setParams (params.reverbSize, params.reverbTone);
        const double feel = params.delayFeel == 1 ? 1.5 : (params.delayFeel == 2 ? 2.0 / 3.0 : 1.0);   // dotted / triplet
        const float delaySamples = (float) (FxParams::delayBeats (params.delayTime) * feel * 60.0 / bpm * sampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i], r = right[i];
            tube.process (l, r, params.tubeDrive, params.tubeType);
            if (params.formantOn)
            {
                // formant LFO: triangle locked to the beat, sweeps the vowel back and forth around VOWEL
                float vowel = params.vowel;
                if (params.formantLfoDepth > 0.001f)
                {
                    double ph = internalPpq / FxParams::lfoBeats (params.formantLfoRate);
                    ph -= std::floor (ph);
                    const float tri = (float) (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph);   // -1..1
                    vowel += tri * params.formantLfoDepth * 2.0f;
                    // reflect at the ends so it keeps moving smoothly instead of sticking on A or U
                    if (vowel < 0.0f) vowel = -vowel;
                    if (vowel > 4.0f) vowel = 8.0f - vowel;
                    vowel = clampf (vowel, 0.0f, 4.0f);
                }
                uiFormantVowel.store (vowel, std::memory_order_relaxed);
                formant.process (l, r, vowel, params.formantMix);
            }
            if (params.crushOn)   crusher.process (l, r, params.bits, params.downsample, params.crushMix);
            if (params.gateOn)
            {
                const float g = gate.process (internalPpq, params);
                l *= g; r *= g;
            }
            if (params.phaserOn)
            {
                double ph = internalPpq / FxParams::lfoBeats (params.phaserRate);
                ph -= std::floor (ph);
                auto sweep = [&] (double p01)
                {
                    p01 -= std::floor (p01);
                    const float tri = (float) (p01 < 0.5 ? 2.0 * p01 : 2.0 - 2.0 * p01);       // 0..1..0
                    return 140.0f * std::exp2 (tri * 6.0f * clampf (params.phaserDepth, 0.0f, 1.0f));
                };
                const float fb = clampf (params.phaserFeedback, -0.9f, 0.9f);
                phaserL.fbk = fb; phaserR.fbk = fb;
                const float m = clampf (params.phaserMix, 0.0f, 1.0f);
                const float pl = phaserL.process (l, sweep (ph), sampleRate);
                const float pr = phaserR.process (r, sweep (ph + 0.25), sampleRate);
                // the phaser's notches remove some energy: a little make-up keeps the level steady
                l += m * (pl * 1.1f - l);
                r += m * (pr * 1.1f - r);
                phaserL.last = undenormalise (phaserL.last);
                phaserR.last = undenormalise (phaserR.last);
            }
            if (params.delayOn)   delay.process (l, r, delaySamples, params.delayFeedback, params.delayMix);
            if (params.reverbOn)  reverb.process (l, r, params.reverbMix);
            internalPpq += ppqPerSample;

            left[i] = SoftClip::process (l);
            right[i] = SoftClip::process (r);
        }
        uiGateStep.store (params.gateOn ? gate.currentStep : -1, std::memory_order_relaxed);
    }

    std::atomic<int> uiGateStep { -1 };
    std::atomic<float> uiFormantVowel { 0.0f };

private:
    struct SoftClip
    {
        static float process (float x)
        {
            const float t = 0.9f, a = std::abs (x);
            if (a <= t) return x;
            const float y = t + (1.0f - t) * fastTanh ((a - t) / (1.0f - t));
            return x < 0.0f ? -y : y;
        }
    };

    float sampleRate = 44100.0f;
    double bpm = 145.0, internalPpq = 0.0;
    sd::Phaser phaserL, phaserR;
    bool playing = false;
    FxParams params;
    TubeDistortion tube;
    FormantFilter formant;
    Bitcrusher crusher;
    TranceGate gate;
    PingPongDelay delay;
    FdnReverb reverb;
};
} // namespace rsq
