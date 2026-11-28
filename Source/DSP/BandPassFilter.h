#pragma once
// R-Squelch — the heart of the squelch: a zero-delay-feedback (TPT) state-variable filter
// with saturation inside the resonance loop, used as a 12 dB or 24 dB band-pass.

#include "DspUtils.h"
#include "synth_dsp.h"

namespace rsq
{
// One TPT SVF (Andrew Simper / Cytomic topology) with a tanh-limited band state.
// The saturated state keeps the filter stable even at self-oscillation and gives
// the "rubbery" squelch character when driven.
struct Svf
{
    float ic1eq = 0.0f, ic2eq = 0.0f;

    void reset() { ic1eq = ic2eq = 0.0f; }

    // g = tan(pi * fc / fs), k = damping (2 = no resonance, ->0 = self-oscillation)
    inline void process (float v0, float g, float k, float& low, float& band, float& high)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = undenormalise (fastTanh (2.0f * v1 - ic1eq));   // saturation in the loop
        ic2eq = undenormalise (2.0f * v2 - ic2eq);
        low = v2;
        band = v1;
        high = v0 - k * v1 - v2;
    }
};

// 4-pole ladder low-pass (zero-delay feedback, TPT one-poles) with saturation on the feedback sum.
// Uncompensated like the analog original: as resonance rises the low end thins out and the peak sings.
struct Ladder
{
    float s[4] {};
    void reset() { for (auto& v : s) v = 0.0f; }

    inline float process (float x, float g, float k)
    {
        const float G = g / (1.0f + g);
        // contribution of the stored states to the 4th-stage output (zero-delay feedback solution)
        const float S1 = s[0] / (1.0f + g), S2 = s[1] / (1.0f + g), S3 = s[2] / (1.0f + g), S4 = s[3] / (1.0f + g);
        const float sigma = G * G * G * S1 + G * G * S2 + G * S3 + S4;
        const float G4 = G * G * G * G;
        float u = (x - k * sigma) / (1.0f + k * G4);
        u = fastTanh (u);                                   // the "overdriven" resonance character
        float y = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - s[i]) * G;
            y = v + s[i];
            s[i] = undenormalise (y + v);
        }
        return y;
    }
};

class BandPassFilter
{
public:
    enum class Slope { dB12 = 0, dB24 = 1, sh5 = 2, tb303 = 3 };

    void prepare (float sr) { sampleRate = sr; reset(); }
    void reset()
    {
        a.reset(); b.reset(); c.reset(); ladder.reset();
        acid = {}; acidHp = {}; acidDc = {};
    }

    // cutoffHz: band centre; resonance 0..1; width 0..1 (band width); drive 0..1 (in-loop saturation)
    inline float process (float x, float cutoffHz, float resonance, float width, float drive, Slope slope)
    {
        if (slope == Slope::tb303)
            return process303 (x, cutoffHz, resonance, width, drive);

        const float nyq = sampleRate * 0.49f;
        const float k = resonanceToK (resonance);
        // The signal runs at a low level inside the filter (kHeadroom), so the tanh in the loop only
        // touches the resonant peak and the resonance stays strong. Drive pushes it harder for grit.
        constexpr float kHeadroom = 0.12f;
        const float inGain = 1.0f + drive * 7.0f;
        const float outComp = 1.0f / (kHeadroom * std::sqrt (inGain));
        x *= inGain * kHeadroom;

        float lo, bp, hi;
        if (slope == Slope::dB12)
        {
            // width scales the bandwidth (damping) on top of the resonance setting
            const float widthMul = 0.25f + width * 1.75f;
            const float fc = clampf (cutoffHz, 20.0f, nyq);
            const float g = std::tan (kPi * fc / sampleRate);
            const float kEff = k * widthMul;
            a.process (x, g, kEff, lo, bp, hi);
            return bp * outComp * clampf (0.35f + kEff, 0.35f, 1.0f);   // level stays even as resonance rises
        }

        if (slope == Slope::sh5)
        {
            // Modelled on the Roland SH-5 dual filter: VCF-1 = high-pass into a resonant 4-pole ladder
            // low-pass (band-pass setting), followed in series by VCF-2, a dedicated band-pass.
            // The resonant ladder corner sits exactly on the cutoff, so there is always ONE peak
            // where you drag it; WIDTH opens the high-pass below it (more body under the peak).
            const float s = widthToSpread (width) * 1.5f;
            const float fHp = clampf (cutoffHz / s, 20.0f, nyq);
            const float fLp = clampf (cutoffHz, 20.0f, nyq);
            const float kl = sh5LadderK (resonance);
            a.process (x, std::tan (kPi * fHp / sampleRate), 1.3f, lo, bp, hi);      // VCF-1 high-pass (no resonance)
            const float ladOut = ladder.process (hi * 2.5f, std::tan (kPi * fLp / sampleRate), kl) * 0.4f;
            const float kb = 2.0f - 1.3f * clampf (resonance, 0.0f, 1.0f);           // VCF-2: gentle band-pass focus
            const float gc = std::tan (kPi * clampf (cutoffHz, 20.0f, nyq) / sampleRate);
            c.process (ladOut, gc, kb, lo, bp, hi);
            return bp * kb * outComp * compSh5 (kl);                                  // even level from low to full resonance
        }

        // 24 dB: two identical resonant band-passes in series, both centred on the cutoff,
        // so there is always ONE peak. Width widens both bands (more damping).
        const float kk = k24 (resonance, width);
        const float g = std::tan (kPi * clampf (cutoffHz, 20.0f, nyq) / sampleRate);
        a.process (x, g, kk, lo, bp, hi);
        const float stage1 = bp * kk;                          // unity gain at the peak
        b.process (stage1, g, kk, lo, bp, hi);
        return bp * kk * outComp * 1.6f * comp24 (kk);
    }

    // ---- TB-303 style: the low-pass ACID filter ----
    // A zero-delay-feedback 4-pole ladder (sd::Ladder from synth_dsp.h) voiced like the 303's diode ladder:
    //  - resonance stops just short of self-oscillation and the low end thins out as it rises
    //    (only part of the usual pass-band compensation), giving the hollow, squelchy "acid" peak
    //  - WIDTH sets a high-pass under the filter (20..320 Hz): turn it up for the thin, rubbery 303 bass
    //  - DRIVE pushes the tanh in the ladder for the screaming, overdriven acid sound
    inline float process303 (float x, float cutoffHz, float resonance, float width, float drive)
    {
        const float r = acidK (resonance);                    // 0..~0.93 of self-oscillation
        acid.set (clampf (cutoffHz, 20.0f, sampleRate * 0.45f), r, sampleRate);
        acid.drive = 1.0f + drive * 3.0f;
        const float k = 4.0f * r;
        x *= (1.0f + 0.2f * k) / (1.0f + 0.5f * k);           // undo most of sd::Ladder's bass compensation
        const float y = acid.process (x);
        acidHp.set (20.0f * std::exp2 (width * 4.0f), sampleRate);
        acidDc.set (8.0f, sampleRate);
        const float out = acidHp.hp (y) * kAcidGain / std::sqrt (acid.drive);
        return acidDc.hp (out);
    }
    static float acidK (float resonance) { return 0.93f * std::pow (clampf (resonance, 0.0f, 1.0f), 0.8f); }
    static constexpr float kAcidGain = 1.7f;

    // high-pass spread below the peak (SH-5 mode): gentle near 0, wide at the top of the range
    static float widthToSpread (float width) { return std::exp2 (width * width * 2.0f); }

    // 24 dB damping: resonance sharpens the band, width widens it
    static float k24 (float resonance, float width) { return (0.15f + resonanceToK (resonance) * 0.85f) * (0.5f + width * 1.5f); }
    // narrower band = less energy through it: make it up so the level stays even
    static float comp24 (float kk) { return clampf (std::pow (0.6f / kk, 0.9f), 1.0f, 6.0f); }
    // SH-5 level make-up across the resonance range (steeper near self-oscillation)
    static float compSh5 (float kl) { return kl < 3.37f ? 6.5f - 1.15f * kl : 2.62f - 2.36f * (kl - 3.37f); }

    // ladder feedback: self-oscillates just below 4 (the tanh keeps it bounded)
    static float sh5LadderK (float r) { r = clampf (r, 0.0f, 1.0f); return 4.1f * std::pow (r, 1.3f); }

    static float resonanceToK (float r)
    {
        r = clampf (r, 0.0f, 1.0f);
        const float inv = 1.0f - r;
        return 0.004f + 1.996f * inv * inv;                  // 2.0 (flat) -> ~0 (self-oscillation)
    }

private:
    float sampleRate = 44100.0f;
    Svf a, b, c;
    Ladder ladder;
    sd::Ladder acid;
    sd::OnePole acidHp, acidDc;
};
} // namespace rsq
