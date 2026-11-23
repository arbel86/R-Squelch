#pragma once
// R-Squelch — small DSP helpers (no JUCE dependency so they can be unit-tested standalone)

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace rsq
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }

inline float midiToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// Pade-style tanh approximation, accurate enough for saturation, cheap and smooth.
inline float fastTanh (float x)
{
    x = clampf (x, -3.0f, 3.0f);   // approximation peaks at exactly +-1 at |x| = 3
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Flush tiny values to zero to avoid denormal CPU spikes in feedback paths.
inline float undenormalise (float x) { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

// One-pole smoother (used for parameter smoothing — no zipper noise).
struct OnePole
{
    float value = 0.0f, coeff = 0.0f;

    void setTime (float seconds, float sampleRate)
    {
        coeff = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (seconds * sampleRate));
    }
    void reset (float v) { value = v; }
    float process (float target)
    {
        value = target + coeff * (value - target);
        return value;
    }
};

// Simple DC blocker: y[n] = x[n] - x[n-1] + R*y[n-1]
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;

    void prepare (float sampleRate) { r = std::exp (-kTwoPi * 10.0f / sampleRate); x1 = y1 = 0.0f; }
    float process (float x)
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = undenormalise (y);
        return y1;
    }
};

// Tiny deterministic RNG (real-time safe).
struct Rng
{
    uint32_t state = 0x12345678u;
    float next01()
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (state & 0xFFFFFF) / (float) 0x1000000;
    }
};
} // namespace rsq
