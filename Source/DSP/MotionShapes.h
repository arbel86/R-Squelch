#pragma once
// R-Squelch — pitch-motion LFO presets. Every shape starts on the played note (value 0 at phase 0)
// and moves by up to 1 x RANGE. Used by the voice (audio) and by the PITCH MOTION display.

#include <cmath>
#include <cstdint>

namespace rsq::motion
{
enum Shape { triangle = 0, sine, rise, fall, jump, steps, random, numShapes };

inline const char* shapeName (int s)
{
    static const char* names[] = { "Triangle", "Sine", "Rise", "Fall", "Jump", "Steps", "Random" };
    return names[s < 0 || s >= numShapes ? 0 : s];
}

// deterministic value 0..1 for random step k (k = 0 is always 0, so the note starts in tune)
inline float randomStep (uint32_t k, uint32_t seed)
{
    if (k == 0) return 0.0f;
    uint32_t x = k * 0x9E3779B1u ^ seed;
    x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15;
    return (float) (x >> 8) / 16777216.0f;
}

// RANDOM / S&H patterns are a 16-step loop (4 steps per cycle, repeating every 4 cycles), fixed by a seed:
// the same seed always plays the same pattern, so a sound is repeatable; the dice button rolls a new seed.
constexpr uint32_t kRandomLoopCycles = 4;
inline uint32_t filterSeedFor (int userSeed) { return (uint32_t) userSeed * 0x9E3779B1u + 0xF17E5u; }
inline uint32_t pitchSeedFor (int userSeed)  { return (uint32_t) userSeed * 0x85EBCA6Bu + 0x51ED27u; }

inline const char* filterShapeName (int s)
{
    static const char* names[] = { "Triangle", "Sine", "Rise", "Fall", "Jump", "Steps", "S&H Random" };
    return names[s < 0 || s >= numShapes ? 0 : s];
}

// FILTER motion presets. 1 = the top of the range (where RANGE is set), 0 = the cutoff.
// They start at the top (like the original triangle), except RISE, which sweeps up from the cutoff.
// S&H RANDOM: sample & hold — a new random filter position on every quarter cycle.
inline float filterShapeValue (int shape, float phase, uint32_t cycle = 0, uint32_t seed = 0xF17E5u)
{
    const float ph = phase - std::floor (phase);
    switch (shape)
    {
        case sine:   return 0.5f + 0.5f * std::cos (6.2831853f * ph);
        case rise:   return ph;
        case fall:   return 1.0f - ph;
        case jump:   return ph < 0.5f ? 1.0f : 0.0f;
        case steps:  return 1.0f - std::floor (ph * 4.0f) / 3.0f;
        case random:
        {
            const uint32_t k = (cycle % kRandomLoopCycles) * 4u + (uint32_t) (ph * 4.0f);
            return k == 0 ? 1.0f : randomStep (k, seed);
        }
        case triangle:
        default:     return ph < 0.5f ? 1.0f - 2.0f * ph : 2.0f * ph - 1.0f;
    }
}

// phase 0..1 within the cycle; cycle = how many full cycles have passed (for RANDOM)
inline float shapeValue (int shape, float phase, uint32_t cycle = 0, uint32_t seed = 0x51ED27u)
{
    const float ph = phase - std::floor (phase);
    switch (shape)
    {
        case sine:   return 0.5f - 0.5f * std::cos (6.2831853f * ph);          // smooth up and back
        case rise:   return ph;                                                // climbs, then snaps back to the note
        case fall:   return -ph;                                               // dives below the note, then snaps back
        case jump:   return ph < 0.5f ? 0.0f : 1.0f;                           // note / note + RANGE (octave jumps)
        case steps:  return std::floor (ph * 4.0f) / 3.0f;                     // a 4-step staircase up
        case random: return randomStep ((cycle % kRandomLoopCycles) * 4u + (uint32_t) (ph * 4.0f), seed);   // 4 random steps per cycle, 16-step loop
        case triangle:
        default:     return ph < 0.5f ? 2.0f * ph : 2.0f - 2.0f * ph;          // up and back down
    }
}
} // namespace rsq::motion
