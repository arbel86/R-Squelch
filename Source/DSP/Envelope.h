#pragma once
// R-Squelch — envelopes with an adjustable curve (linear <-> exponential).

#include "DspUtils.h"

namespace rsq
{
// curve 0..1 -> shape exponent 1 (linear) .. 6 (snappy exponential-like)
inline float curveToExponent (float curve) { return 1.0f + clampf (curve, 0.0f, 1.0f) * 5.0f; }

class AdsrEnvelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (float sr) { sampleRate = sr; reset(); }
    void reset() { stage = Stage::Idle; level = 0.0f; t = 0.0f; }

    void setParameters (float attackSec, float decaySec, float sustainLevel, float releaseSec, float curve)
    {
        attack = std::max (0.0005f, attackSec);
        decay = std::max (0.001f, decaySec);
        sustain = clampf (sustainLevel, 0.0f, 1.0f);
        release = std::max (0.001f, releaseSec);
        exponent = curveToExponent (curve);
    }

    // Retrigger starts from the current level, so there is no click.
    void noteOn() { startLevel = level; t = 0.0f; stage = Stage::Attack; }

    void noteOff()
    {
        if (stage != Stage::Idle) { startLevel = level; t = 0.0f; stage = Stage::Release; }
    }

    bool isActive() const { return stage != Stage::Idle; }
    Stage getStage() const { return stage; }
    float getLevel() const { return level; }

    inline float process()
    {
        const float dt = 1.0f / sampleRate;
        switch (stage)
        {
            case Stage::Idle: level = 0.0f; break;

            case Stage::Attack:
                t += dt / attack;
                if (t >= 1.0f) { level = 1.0f; t = 0.0f; stage = Stage::Decay; }
                else level = startLevel + (1.0f - startLevel) * (1.0f - std::pow (1.0f - t, exponent * 0.5f + 0.5f));
                break;

            case Stage::Decay:
                t += dt / decay;
                if (t >= 1.0f) { level = sustain; stage = Stage::Sustain; }
                else level = sustain + (1.0f - sustain) * std::pow (1.0f - t, exponent);
                break;

            case Stage::Sustain: level = sustain; break;

            case Stage::Release:
                t += dt / release;
                if (t >= 1.0f) { level = 0.0f; stage = Stage::Idle; }
                else level = startLevel * std::pow (1.0f - t, exponent);
                break;
        }
        return level;
    }

private:
    float sampleRate = 44100.0f;
    float attack = 0.001f, decay = 0.2f, sustain = 0.0f, release = 0.1f, exponent = 3.0f;
    float level = 0.0f, startLevel = 0.0f, t = 0.0f;
    Stage stage = Stage::Idle;
};

// Attack/decay envelope for pitch and FM "zaps". Output 0 -> 1 -> 0.
class AdEnvelope
{
public:
    void prepare (float sr) { sampleRate = sr; reset(); }
    void reset() { stage = 0; level = 0.0f; t = 0.0f; }

    void setParameters (float attackSec, float decaySec, float curve)
    {
        attack = std::max (0.0f, attackSec);
        decay = std::max (0.001f, decaySec);
        exponent = curveToExponent (curve);
    }

    void trigger()
    {
        t = 0.0f;
        stage = attack > 0.0f ? 1 : 2;
        if (stage == 2) level = 1.0f;
    }

    inline float process()
    {
        const float dt = 1.0f / sampleRate;
        if (stage == 1)
        {
            t += dt / attack;
            if (t >= 1.0f) { t = 0.0f; stage = 2; level = 1.0f; }
            else level = t;
        }
        else if (stage == 2)
        {
            t += dt / decay;
            if (t >= 1.0f) { stage = 0; level = 0.0f; }
            else level = std::pow (1.0f - t, exponent);
        }
        return level;
    }

private:
    float sampleRate = 44100.0f, attack = 0.0f, decay = 0.05f, exponent = 3.0f;
    float level = 0.0f, t = 0.0f;
    int stage = 0;
};
} // namespace rsq
