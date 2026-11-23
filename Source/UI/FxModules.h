#pragma once
// R-Squelch — the effects row: one compact module per effect (title, on/off pill, 2–3 knobs),
// plus the clickable 16-step pattern for the trance gate.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Controls.h"
#include "Hardware.h"
#include "../Parameters.h"
#include "../DSP/Effects.h"

namespace rsq
{
// Small on/off pill bound to a bool parameter.
class PowerToggle final : public juce::Component
{
public:
    PowerToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, std::function<void (bool)> onChange)
        : changed (std::move (onChange)),
          attachment (*state.getParameter (paramId), [this] (float v) { on = v > 0.5f; if (changed) changed (on); repaint(); }, state.undoManager)
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // Hardware toggle: a small metal button with an LED that lights up when the effect is on.
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        // recessed slot + raised button
        g.setColour (Colour (0xff0b0c0e));
        g.fillRoundedRectangle (r, 4.0f);
        auto b = r.reduced (1.5f);
        g.setGradientFill (ColourGradient (Colour (0xff2c2a27), 0.0f, b.getY(), Colour (0xff0e0e0f), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (Colours::white.withAlpha (0.16f));
        g.drawLine (b.getX() + 3.0f, b.getY() + 0.8f, b.getRight() - 3.0f, b.getY() + 0.8f, 1.0f);
        // LED
        auto led = Rectangle<float> (b.getHeight() * 0.55f, b.getHeight() * 0.55f).withCentre (b.getCentre());
        if (on)
        {
            g.setColour (theme::gold.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
            g.setGradientFill (theme::goldGradient (led));
            g.fillEllipse (led);
            g.setColour (Colours::white.withAlpha (0.8f));
            g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-0.5f, -0.5f));
        }
        else
        {
            g.setColour (Colour (0xff15161a));
            g.fillEllipse (led);
            g.setColour (Colours::white.withAlpha (0.08f));
            g.drawEllipse (led, 0.8f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

private:
    std::function<void (bool)> changed;
    bool on = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// 16 clickable steps. Click toggles a step; drag paints. The playing step lights up.
class GateGrid final : public juce::Component, private juce::Timer
{
public:
    GateGrid (juce::AudioProcessorValueTreeState& s, const FxChain& fxChain) : state (s), fx (fxChain)
    {
        for (int i = 0; i < 16; ++i)
            steps[(size_t) i] = s.getParameter (ids::gateStep (i));
        startTimerHz (30);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const int playing = fx.uiGateStep.load (std::memory_order_relaxed);
        for (int i = 0; i < 16; ++i)
        {
            // rubber pads with a light underneath, like a hardware step sequencer
            auto cell = cellBounds (i);
            const bool on = steps[(size_t) i]->getValue() > 0.5f;
            g.setColour (juce::Colour (0xff0b0c0e));
            g.fillRoundedRectangle (cell, 2.5f);
            auto pad = cell.reduced (1.0f);
            if (on)
            {
                const float a = i == playing ? 1.0f : 0.8f;
                g.setGradientFill (theme::goldGradient (pad, a));
                g.fillRoundedRectangle (pad, 2.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.4f), 0.0f, pad.getY(),
                                                         juce::Colours::white.withAlpha (0.0f), 0.0f, pad.getCentreY(), false));
                g.fillRoundedRectangle (pad.withHeight (pad.getHeight() * 0.5f), 2.0f);
            }
            else
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2826), 0.0f, pad.getY(), juce::Colour (0xff111112), 0.0f, pad.getBottom(), false));
                g.fillRoundedRectangle (pad, 2.0f);
            }
            if (i == playing)
            {
                g.setColour (juce::Colours::white);
                g.drawRoundedRectangle (cell.expanded (0.5f), 3.0f, 1.3f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position);
        if (i < 0) return;
        paintValue = steps[(size_t) i]->getValue() > 0.5f ? 0.0f : 1.0f;
        setStep (i);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position);
        if (i >= 0) setStep (i);
    }

private:
    void setStep (int i)
    {
        auto* p = steps[(size_t) i];
        if (std::abs (p->getValue() - paintValue) < 0.5f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (paintValue);
        p->endChangeGesture();
        repaint();
    }

    juce::Rectangle<float> cellBounds (int i) const
    {
        const float groupGap = 5.0f, gap = 2.0f;
        const float w = ((float) getWidth() - 3.0f * groupGap - 12.0f * gap) / 16.0f;
        const float x = (float) i * (w + gap) + (float) (i / 4) * (groupGap - gap);
        return { x, 0.0f, w, (float) getHeight() };
    }

    int cellAt (juce::Point<float> p) const
    {
        for (int i = 0; i < 16; ++i)
            if (cellBounds (i).expanded (1.5f, 4.0f).contains (p)) return i;
        return -1;
    }

    void timerCallback() override
    {
        const int s = fx.uiGateStep.load (std::memory_order_relaxed);
        bool changed = s != lastStep;
        for (int i = 0; i < 16; ++i)
        {
            const bool on = steps[(size_t) i]->getValue() > 0.5f;
            if (on != cache[(size_t) i]) { cache[(size_t) i] = on; changed = true; }
        }
        lastStep = s;
        if (changed) repaint();
    }

    juce::AudioProcessorValueTreeState& state;
    const FxChain& fx;
    std::array<juce::RangedAudioParameter*, 16> steps {};
    std::array<bool, 16> cache {};
    float paintValue = 1.0f;
    int lastStep = -2;
};

//==============================================================================
// Gate pattern presets: "<  NAME  >" in the gate's title strip. Arrows step, click opens the list
// (with RANDOM to roll a fresh pattern). Shows CUSTOM once you edit the steps by hand.
class GatePatternPicker final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    struct Pattern { const char* name; const char* steps; };   // 16 chars, x = open
    static const std::vector<Pattern>& patterns()
    {
        static const std::vector<Pattern> p {
            { "Gallop",   "x.xxx.xxx.xxx.x." },   // the default psy gallop
            { "Full",     "xxxxxxxxxxxxxxxx" },
            { "Trance",   "x.x.x.x.x.x.x.x." },
            { "Offbeat",  ".x.x.x.x.x.x.x.x" },
            { "Rolling",  "xxx.xxx.xxx.xxx." },
            { "Psy Gate", "x.xxx.xx.xxx.xxx" },
            { "Stutter",  "xxxx..x.xxxx..x." },
            { "Broken",   "x..x..x.x..x..x." },
            { "Morse",    "xx.x.xx..x.xx.x." },
            { "Half",     "xxxxxxxx........" },
            { "Build",    "x...x...x.x.xxxx" },
        };
        return p;
    }

    explicit GatePatternPicker (juce::AudioProcessorValueTreeState& s)
    {
        for (int i = 0; i < 16; ++i) steps[(size_t) i] = s.getParameter (ids::gateStep (i));
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Gate patterns: click for the list (and RANDOM), arrows to step through them");
        startTimerHz (5);
        update();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff0b0c0e));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::gold.withAlpha (0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 0.8f);
        auto inner = r.reduced (3.0f, 1.0f);
        auto left = inner.removeFromLeft (11.0f), right = inner.removeFromRight (11.0f);
        g.setColour (theme::gold);
        for (auto [a, l] : { std::pair (left, true), std::pair (right, false) })
        {
            Path p; const auto c = a.getCentre();
            if (l) p.addTriangle (c.x + 2.5f, c.y - 3.5f, c.x + 2.5f, c.y + 3.5f, c.x - 2.5f, c.y);
            else   p.addTriangle (c.x - 2.5f, c.y - 3.5f, c.x - 2.5f, c.y + 3.5f, c.x + 2.5f, c.y);
            g.fillPath (p);
        }
        g.setFont (theme::labelFont (9.0f, true));
        g.setColour (current >= 0 ? theme::goldHi : theme::goldDim);
        g.drawText (current >= 0 ? String (patterns()[(size_t) current].name).toUpperCase() : String ("CUSTOM"), inner, Justification::centred, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = (int) patterns().size();
        if (e.x < 14)               { apply ((juce::jmax (0, current) + n - 1) % n); return; }
        if (e.x > getWidth() - 14)  { apply ((current + 1) % n); return; }
        juce::PopupMenu menu;
        for (int i = 0; i < n; ++i) menu.addItem (i + 1, patterns()[(size_t) i].name, true, i == current);
        menu.addSeparator();
        menu.addItem (1000, "Random (roll a new pattern)");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [safe = juce::Component::SafePointer<GatePatternPicker> (this)] (int id)
            {
                if (safe == nullptr || id == 0) return;
                if (id == 1000) safe->randomise(); else safe->apply (id - 1);
            });
    }

private:
    void apply (int index)
    {
        const char* pat = patterns()[(size_t) index].steps;
        for (int i = 0; i < 16; ++i) set (i, pat[i] == 'x');
        update();
    }

    void randomise()
    {
        auto& rnd = juce::Random::getSystemRandom();
        for (int i = 0; i < 16; ++i)
            set (i, i == 0 || rnd.nextFloat() < (i % 4 == 0 ? 0.85f : 0.55f));   // downbeats more likely, step 1 always on
        update();
    }

    void set (int i, bool on)
    {
        auto* p = steps[(size_t) i];
        p->beginChangeGesture();
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }

    void update()
    {
        int found = -1;
        for (int k = 0; k < (int) patterns().size() && found < 0; ++k)
        {
            bool same = true;
            for (int i = 0; i < 16 && same; ++i) same = (steps[(size_t) i]->getValue() > 0.5f) == (patterns()[(size_t) k].steps[i] == 'x');
            if (same) found = k;
        }
        if (found != current) { current = found; repaint(); }
    }
    void timerCallback() override { update(); }

    std::array<juce::RangedAudioParameter*, 16> steps {};
    int current = -2;
};

//==============================================================================
// One effect: panel with title, on/off pill, optional extra row (gate grid), and a row of knobs.
class FxModule final : public juce::Component
{
public:
    FxModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleTitle, const char* onParamId,
              std::vector<std::unique_ptr<Knob>> knobList, std::unique_ptr<juce::Component> extraRow = {})
        : title (moduleTitle.toUpperCase()), knobs (std::move (knobList)), extra (std::move (extraRow)),
          power (state, onParamId, [this] (bool on) { setActiveLook (on); })
    {
        addAndMakeVisible (power);
        for (auto& k : knobs) addAndMakeVisible (*k);
        if (extra) addAndMakeVisible (*extra);
    }

    void paint (juce::Graphics& g) override
    {
        // recessed module (the bevel needs 2 px of room inside this component)
        hw::module (g, getLocalBounds().toFloat().reduced (2.0f), title, active);
    }

    // an extra control in the title strip, between the title and the power switch (gate patterns)
    void setTitleWidget (std::unique_ptr<juce::Component> w)
    {
        titleWidget = std::move (w);
        addAndMakeVisible (*titleWidget);
        resized();
    }

    void resized() override
    {
        power.setBounds (getWidth() - 12 - 30, 2 + 6, 30, 18);           // in the title strip
        if (titleWidget)
        {
            // start right after the title text, so long titles never run under the widget
            const int x = juce::jmax (78, 28 + juce::roundToInt (juce::GlyphArrangement::getStringWidth (theme::labelFont (11.5f, true), title)) + 14);
            titleWidget->setBounds (x, 2 + 6, juce::jmin (getWidth() - x - 48, 210), 18);
        }
        auto r = getLocalBounds().reduced (12, 10);
        r.removeFromTop ((int) hw::kTitleStrip - 6);
        if (extra)
        {
            r.removeFromTop (6);
            extra->setBounds (r.removeFromTop (18).reduced (2, 0));
        }
        auto row = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 86));
        const int w = row.getWidth() / juce::jmax (1, (int) knobs.size());
        for (auto& k : knobs)
            k->setBounds (row.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w, 70), row.getHeight()));
    }

private:
    void setActiveLook (bool on)
    {
        active = on;
        for (auto& k : knobs) k->setAlpha (on ? 1.0f : 0.4f);
        if (extra) extra->setAlpha (on ? 1.0f : 0.4f);
        repaint();
    }

    juce::String title;
    bool active = true;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<juce::Component> extra, titleWidget;
    PowerToggle power;
};
} // namespace rsq
