#pragma once
// R-Squelch — the FLANGE filter (Serum-style "Flange +/-" filter types), after the main filter.
// A comb whose frequency FOLLOWS THE MAIN FILTER CUTOFF — every squelch move, envelope, S&H jump and even the
// GROWL audio-rate wobble drags it along — so at high RES it screams laser sweeps right on top of the squelch.
//
//   FLANGE +    positive comb: resonant peaks on every harmonic of the flange frequency (bright, metallic)
//   FLANGE -    negative comb: the delayed copy is subtracted, peaks fall halfway between (hollow, sucking, jet)
//   FLANGE +/-  left is +, right is - : two different combs screaming against each other in stereo
//
//   FREQ        where it sits relative to the filter cutoff (-36..+36 semitones)
//   RES         feedback 0..99 % — up at the top it rings and whistles like a laser
//   RATE/DEPTH  its own tempo-synced LFO on top of the cutoff tracking (+-2 octaves)
//   DESTROY     distortion inside the feedback loop (square-ish, self-sustaining screams, always bounded)
//   SPREAD      left and right detune / LFO offset (stereo swirl)
//   DAMP, MIX
// Level-compensated across RES / DESTROY so it stays loud but never explodes.

#include "DspUtils.h"
#include "synth_dsp.h"

namespace rsq
{
class CombFilter
{
public:
    enum Mode { flangePlus = 0, flangeMinus = 1, flangeBoth = 2 };
    // backwards names (older code / presets)
    static constexpr int tuned = flangePlus, negFlange = flangeMinus;

    void prepare (float sr)
    {
        sampleRate = sr;
        line.init ((int) (sr / kMinHz) + 8);   // longest delay = lowest comb pitch (~62 ms)
        lp = 0.0f;
    }

    void reset() { std::fill (line.buf.begin(), line.buf.end(), 0.0f); line.w = 0; lp = 0.0f; }

    float maxDelaySamples() const { return (float) line.buf.size() - 6.0f; }

    // delaySamples: comb period; feedback -0.99..0.99; damp, mix, drive 0..1
    inline float process (float x, float delaySamples, float feedback, float damp, float mix, float drive, int mode)
    {
        if (line.buf.empty()) return x;
        const float delay = clampf (delaySamples, 1.0f, maxDelaySamples());
        const float delayed = line.readCubic (delay - 1.0f);              // y[n - delay]

        // damping: a one-pole low-pass inside the loop (20 kHz .. ~300 Hz)
        const float dampHz = 20000.0f * std::exp2 (-6.0f * clampf (damp, 0.0f, 1.0f));
        const float a = 1.0f - std::exp (-kTwoPi * std::min (dampHz, sampleRate * 0.45f) / sampleRate);
        lp += a * (delayed - lp);
        lp = undenormalise (lp);

        // DESTROY: the echoes go through a saturator whose small-signal gain rises with drive (up to 4x),
        // so at high drive + res the loop screams and sustains on its own — its ceiling drops as it does,
        // so it can never run away (it turns into a distorted, square-ish ring instead).
        const float g = 1.0f + 15.0f * drive * drive;
        const float sat = fastTanh (g * lp) / std::sqrt (g);
        const bool minus = mode == flangeMinus;
        const float res = clampf (std::abs (feedback), 0.0f, 0.99f) * (minus ? -1.0f : 1.0f);
        const float y = x + res * sat;
        line.push (fastTanh (y * 0.5f) * 2.0f);                           // soft ceiling on what re-enters the loop

        // FLANGE +: add the delayed copy, FLANGE -: subtract it; level-compensated against the resonance
        float wet = 0.7f * (minus ? (y - sat) : (y + sat)) * std::pow (1.0f - std::abs (res), minus ? 0.1f : 0.2f);
        if (drive > 0.001f)   // DESTROY also crushes the output: hard, buzzing saturation
        {
            const float k = 1.0f + 10.0f * drive;
            wet = fastTanh (wet * k) / std::pow (k, 0.42f);
        }
        return x + clampf (mix, 0.0f, 1.0f) * (wet - x);
    }

private:
    static constexpr float kMinHz = 16.0f;
    float sampleRate = 44100.0f, lp = 0.0f;
    sd::DelayLine line;
};
} // namespace rsq
