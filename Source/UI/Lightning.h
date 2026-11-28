#pragma once
// R-Squelch — psychedelic lightning: while the trance gate is on and a note is sounding,
// a bolt strikes across the window every 6 seconds (visual only, never touches the audio).
// The overlay is transparent to the mouse, so everything underneath stays playable.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"
#include "../DSP/SquelchEngine.h"

namespace rsq
{
class LightningOverlay final : public juce::Component, private juce::Timer
{
public:
    static constexpr double intervalMs = 6000.0;   // one strike every 6 seconds
    static constexpr double strikeMs = 750.0;      // how long a strike stays visible

    LightningOverlay (juce::AudioProcessorValueTreeState& s, const SquelchEngine& e)
        : gateOn (s.getRawParameterValue (ids::gateOn)), engine (e)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (40);
    }

    // Strikes are drawn only inside this shape (the pyramid), never on what lies around it.
    void setClipShape (juce::Path p) { clipShape = std::move (p); }

    void paint (juce::Graphics& g) override
    {
        if (strikeStart < 0.0) return;
        if (! clipShape.isEmpty()) g.reduceClipRegion (clipShape);
        const float t = (float) ((now() - strikeStart) / strikeMs);   // 0..1
        if (t >= 1.0f) return;

        // flicker like real lightning: bright, dip, second flash, then fade out
        const float flicker = t < 0.10f ? 1.0f
                            : t < 0.18f ? 0.35f
                            : t < 0.30f ? 0.95f
                            : juce::jmax (0.0f, 1.0f - (t - 0.30f) / 0.70f) * 0.8f;

        // faint full-window flash in the accent colour
        g.setColour (theme::shifted (theme::violet, hue).withAlpha (0.07f * flicker));
        g.fillAll();

        auto area = getLocalBounds().toFloat();
        for (size_t i = 0; i < bolts.size(); ++i)
        {
            const auto& b = bolts[i];
            const float a = flicker * b.strength;
            auto grad = theme::iridescent (area.getTopLeft(), area.getBottomRight(), 1.0f, hue + 0.05f * (float) i);
            grad.multiplyOpacity (0.12f * a);
            g.setGradientFill (grad);
            g.strokePath (b.path, juce::PathStrokeType (b.width * 9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            grad = theme::iridescent (area.getTopLeft(), area.getBottomRight(), 1.0f, hue + 0.05f * (float) i);
            grad.multiplyOpacity (0.45f * a);
            g.setGradientFill (grad);
            g.strokePath (b.path, juce::PathStrokeType (b.width * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white.withAlpha (juce::jmin (1.0f, 0.95f * a)));
            g.strokePath (b.path, juce::PathStrokeType (b.width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // exposed for testing / screenshots
    void strikeNow() { triggerStrike(); }

private:
    juce::Path clipShape;
    struct Bolt { juce::Path path; float width = 1.5f, strength = 1.0f; };

    static double now() { return juce::Time::getMillisecondCounterHiRes(); }

    void timerCallback() override
    {
        const bool active = gateOn->load() > 0.5f && engine.uiAmp.load (std::memory_order_relaxed) > 0.01f;
        const double t = now();

        if (active)
        {
            if (! wasActive || t - lastStrike >= intervalMs)   // strike when the gate starts, then every 6 s
                triggerStrike();
        }
        wasActive = active;

        if (strikeStart >= 0.0)
        {
            repaint();
            if (t - strikeStart > strikeMs) { strikeStart = -1.0; bolts.clear(); repaint(); }
        }
    }

    void triggerStrike()
    {
        lastStrike = strikeStart = now();
        hue = rng.nextFloat() * 0.6f - 0.3f;
        bolts.clear();

        const float w = (float) getWidth(), h = (float) getHeight();
        const int mainBolts = rng.nextFloat() < 0.35f ? 2 : 1;
        for (int m = 0; m < mainBolts; ++m)
        {
            const juce::Point<float> start (w * (0.15f + 0.7f * rng.nextFloat()), h * 0.07f);
            const juce::Point<float> end (start.x + w * (rng.nextFloat() - 0.5f) * 0.5f, h * (0.62f + 0.25f * rng.nextFloat()));
            std::vector<juce::Point<float>> pts;
            makeJagged (start, end, 0.22f, 7, pts);
            bolts.push_back ({ toPath (pts), 2.0f, 1.0f });

            // a few branches splitting off the main bolt
            const int branches = 2 + rng.nextInt (3);
            for (int b = 0; b < branches; ++b)
            {
                const auto& from = pts[(size_t) (pts.size() / 5 + (size_t) rng.nextInt ((int) pts.size() * 3 / 5))];
                const float ang = (rng.nextFloat() - 0.5f) * 1.6f + juce::MathConstants<float>::halfPi;
                const float len = h * (0.08f + 0.18f * rng.nextFloat());
                const juce::Point<float> to (from.x + std::cos (ang) * len * (rng.nextBool() ? 1.0f : -1.0f), from.y + std::sin (ang) * len);
                std::vector<juce::Point<float>> bp;
                makeJagged (from, to, 0.28f, 5, bp);
                bolts.push_back ({ toPath (bp), 1.1f, 0.7f });
            }
        }
        repaint();
    }

    // midpoint displacement: recursively split the line and push the midpoint sideways
    void makeJagged (juce::Point<float> a, juce::Point<float> b, float rough, int depth, std::vector<juce::Point<float>>& out)
    {
        out.clear();
        out.push_back (a);
        out.push_back (b);
        float disp = a.getDistanceFrom (b) * rough;
        for (int d = 0; d < depth; ++d)
        {
            std::vector<juce::Point<float>> next;
            next.reserve (out.size() * 2);
            for (size_t i = 0; i + 1 < out.size(); ++i)
            {
                const auto p = out[i], q = out[i + 1];
                const auto dir = q - p;
                const float len = dir.getDistanceFromOrigin();
                const juce::Point<float> normal (len > 0 ? -dir.y / len : 0.0f, len > 0 ? dir.x / len : 0.0f);
                next.push_back (p);
                next.push_back ((p + q) * 0.5f + normal * ((rng.nextFloat() * 2.0f - 1.0f) * disp));
            }
            next.push_back (out.back());
            out.swap (next);
            disp *= 0.55f;
        }
    }

    static juce::Path toPath (const std::vector<juce::Point<float>>& pts)
    {
        juce::Path p;
        p.startNewSubPath (pts.front());
        for (size_t i = 1; i < pts.size(); ++i) p.lineTo (pts[i]);
        return p;
    }

    std::atomic<float>* gateOn;
    const SquelchEngine& engine;
    std::vector<Bolt> bolts;
    juce::Random rng;
    double strikeStart = -1.0, lastStrike = -1.0e12;
    bool wasActive = false;
    float hue = 0.0f;
};
} // namespace rsq
