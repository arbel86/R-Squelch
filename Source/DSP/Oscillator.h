#pragma once
// R-Squelch — Moog-style oscillator with hard SYNC.
//
// SYNC (like Serum's "Sync" warp): a hidden master runs at the note's pitch; the audible (slave)
// oscillator runs faster (ratio 1..8) and is restarted every master cycle. The pitch you hear stays
// the same, but the harmonics shift and sweep — the classic sync scream.
// All waveform jumps (natural edges AND sync resets) are smoothed with polyBLEP, using a one-sample
// look-back so both sides of every jump are corrected (keeps aliasing low even with heavy sync).
//
// Moog-style character (inspired by the Minimoog VCOs, not a circuit model):
//   - free-running phase (no reset on each note), like analog oscillators
//   - slow random pitch drift of a few cents, different per oscillator
//   - slightly softened edges (analog bandwidth) and a square that "droops" (AC-coupled tilt)
//   - a touch of asymmetric warmth

#include "DspUtils.h"

namespace rsq
{
inline float polyBlep (float t, float dt)
{
    if (t < dt)       { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt){ t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

class Oscillator
{
public:
    void reset (float startPhase = 0.0f)
    {
        master = slave = startPhase;
        fmPhase = fmCarrier = startPhase;
        subPhase = startPhase * 0.5f;
        pending = 0.0f;
        lp = droopIn = droopOut = 0.0f;
    }

    void setSeed (uint32_t seed) { rng.state = seed | 1u; }
    void setDriftEnabled (bool on) { driftOn = on; }   // tests switch the analog drift off to measure exactly
    // BRIGHT = sharp digital psy saw/square (no softening at all); WARM = the Moog-style colour below
    void setBright (bool on) { bright = on; }
    // FM amount 0..1 (index up to 2.5) and modulator ratio (x note pitch)
    // ACID: the TB-303 saw. Its ramp is not straight: the VCO's capacitor charges exponentially, so the wave
    // rises fast and bends over towards the top before the sharp reset — rounder lows, a vocal, rubbery edge.
    void setAcid (bool on) { acid = on; }
    void setFm (float amount01, float ratio) { fmAmount = clampf (amount01, 0.0f, 1.0f); fmRatio = clampf (ratio, 0.25f, 8.0f); }

    // freqHz: pitch; morph 0 = saw, 1 = square; pw 0.05..0.95; syncRatio 1..8 (1 = no sync)
    inline void process (float freqHz, float sampleRate, float morph, float pw, float syncRatio, float& main, float& sub)
    {
        // ---- analog drift: smoothed random walk, about +-4 cents ----
        driftTarget += (rng.next01() - 0.5f) * 0.002f;
        driftTarget *= 0.9995f;
        drift += 0.0005f * (driftTarget - drift);
        const float f = freqHz * (driftOn ? 1.0f + clampf (drift, -0.0025f, 0.0025f) : 1.0f);

        const float dtNote = clampf (f / sampleRate, 1.0e-6f, 0.45f);    // the pitch you hear
        const float ratio = clampf (syncRatio, 1.0f, 8.0f);
        const bool synced = ratio > 1.001f;

        const float dtM = dtNote;
        const float dtS = std::min (dtM * ratio, 0.45f);

        // ---- FM: a classic 2-operator FM tone (sine carrier phase-modulated by a sine) ----
        // The FM knob blends it in place of the saw/square and drives the modulation harder as it goes up:
        // 0 = clean wave, middle = growl, top = bright metallic. Modulator at (note x ratio) with an integer
        // ratio keeps every sideband on the note's harmonics, so it stays in tune.
        float fmTone = 0.0f;
        if (fmAmount > 1.0e-4f)
        {
            fmCarrier += dtNote;
            fmCarrier -= std::floor (fmCarrier);
            fmPhase += dtNote * fmRatio;
            fmPhase -= std::floor (fmPhase);
            const float index = 1.0f + 7.0f * fmAmount;                       // modulation index 1..8
            fmTone = std::sin (kTwoPi * fmCarrier + index * std::sin (kTwoPi * fmPhase));
        }

        float hNow = 0.0f, hPrev = 0.0f;   // BLEP corrections for this sample and the previous one

        // a jump of height h that happened 'since' samples ago (0..1) inside this sample period
        auto addStep = [&] (float h, float since)
        {
            since = clampf (since, 0.0f, 1.0f);
            hNow  += 0.5f * h * (2.0f * since - since * since - 1.0f);
            hPrev += 0.5f * h * (since * since);
        };

        // jumps of the morphing waveform (square = +1 below pw, -1 above):
        //   at pw:   saw is continuous, square falls by 2
        //   at wrap: saw falls by 2, square rises by 2
        const float pwJump = morph * -2.0f;
        const float wrapJump = -2.0f + morph * 4.0f;

        // advance the slave across a stretch of 'len' samples, starting 'startsAgo' samples before now
        auto runSlave = [&] (float len, float startsAgo)
        {
            const float p0 = slave;
            float p1 = p0 + dtS * len;
            if (p0 < pw && p1 >= pw)                                   // square falling edge
                addStep (pwJump, startsAgo - (pw - p0) / dtS);
            if (p1 >= 1.0f)                                            // natural wrap
            {
                addStep (wrapJump, startsAgo - (1.0f - p0) / dtS);
                p1 -= 1.0f;
                if (p1 >= pw) addStep (pwJump, startsAgo - (1.0f + pw - p0) / dtS);
            }
            slave = p1;
        };

        master += dtM;
        if (synced && master >= 1.0f)
        {
            master -= 1.0f;
            const float since = master / dtM;                          // samples since the master wrapped
            runSlave (1.0f - since, 1.0f);
            // sync reset: slave jumps from its current phase back to 0
            const float sawBefore = acid ? acidSaw (slave) : 2.0f * slave - 1.0f, sqBefore = slave < pw ? 1.0f : -1.0f;
            const float jump = (-1.0f - sawBefore) + morph * ((1.0f - sqBefore) - (-1.0f - sawBefore));
            addStep (jump, since);
            slave = 0.0f;
            runSlave (since, since);
        }
        else
        {
            if (master >= 1.0f) master -= 1.0f;
            runSlave (1.0f, 1.0f);
            if (! synced) slave = master;                              // no sync: slave and master are the same oscillator
        }

        // naive value at the new phase
        const float saw = acid ? acidSaw (slave) : 2.0f * slave - 1.0f;
        float sq = slave < pw ? 1.0f : -1.0f;
        float cur = saw + morph * (sq - saw) + hNow;
        // crossfade towards the FM tone (equal-power-ish so the level stays similar)
        cur = cur * (1.0f - fmAmount) + fmTone * fmAmount * 1.3f;
        pending += hPrev * (1.0f - fmAmount);   // the saw/square part is faded by the FM blend, so are its corrections

        // remove the DC of an asymmetric pulse so the level stays centred
        float out = pending - morph * (2.0f * pw - 1.0f) - (acid ? kAcidDc * (1.0f - morph) * (1.0f - fmAmount) : 0.0f);
        pending = cur;

        if (bright)
        {
            // BRIGHT: the raw, band-limited wave — every harmonic at full strength, straight into the filter
            main = out;
        }
        else
        {
        // ---- Moog-style colour (WARM) ----
        // square droop: a gentle AC-coupling high-pass, only on the square part
        if (std::abs (sampleRate - cachedSr) > 0.5f)
        {
            cachedSr = sampleRate;
            droopCoeff = std::exp (-kTwoPi * 9.0f / sampleRate);
            lpCoeff = std::exp (-kTwoPi * 15000.0f / sampleRate);
        }
        droopOut = droopCoeff * (droopOut + out - droopIn);
        droopIn = out;
        out += morph * 0.35f * (droopOut - out);
        // analog bandwidth: soften the very top a little
        lp = out + lpCoeff * (lp - out);
        out = lp;
        // a touch of asymmetric warmth
        constexpr float bias = 0.08f;
        out = (fastTanh (out * 0.9f + bias) - fastTanh (bias)) * 1.15f;
        // the asymmetry creates a little DC: block it
        const float dc = out - dcX + 0.9995f * dcY;
        dcX = out;
        dcY = undenormalise (dc);
        main = dc;
        }

        // ---- sub oscillator: square one octave below the master ----
        const float sdt = dtNote * 0.5f;   // the sub stays clean (no FM) to keep the low end solid
        float s = subPhase < 0.5f ? 1.0f : -1.0f;
        s += polyBlep (subPhase, sdt);
        float st = subPhase + 0.5f;
        st -= std::floor (st);
        s -= polyBlep (st, sdt);
        sub = s;
        subPhase += sdt;
        if (subPhase >= 1.0f) subPhase -= 1.0f;
    }

    // 303-style curved ramp: -1 .. +1 over the cycle (same end points as the plain saw, so the same BLEP fits)
    static constexpr float kAcidK = 2.2f;
    static float acidSaw (float ph)
    {
        static const float norm = 1.0f / (1.0f - std::exp (-kAcidK));
        return 2.0f * (1.0f - std::exp (-kAcidK * ph)) * norm - 1.0f;
    }
    // the curve's average (removed so the wave stays centred): 2 (1 - (1 - e^-k) / k) / (1 - e^-k) - 1
    static constexpr float kAcidDc = 0.34011f;

private:
    bool acid = false;
    float master = 0.0f, slave = 0.0f, subPhase = 0.0f, pending = 0.0f;
    float fmPhase = 0.0f, fmCarrier = 0.0f, fmAmount = 0.0f, fmRatio = 1.0f;
    float lp = 0.0f, droopIn = 0.0f, droopOut = 0.0f;
    float drift = 0.0f, driftTarget = 0.0f;
    float cachedSr = 0.0f, droopCoeff = 0.0f, lpCoeff = 0.0f, dcX = 0.0f, dcY = 0.0f;
    bool driftOn = true, bright = true;
    Rng rng;
};
} // namespace rsq
