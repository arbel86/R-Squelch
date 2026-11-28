#pragma once
// R-Squelch — Knob (name + dial + value) and Segmented switch, both bound to APVTS parameters.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Hardware.h"

namespace rsq
{
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& displayName,
          bool bipolar = false, bool compactLayout = false)
        : name (displayName.toUpperCase()), compact (compactLayout)
    {
        param = state.getParameter (paramId);
        jassert (param != nullptr);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setMouseDragSensitivity (220);
        slider.getProperties().set ("bipolar", bipolar);
        slider.onValueChange = [this] { repaint(); };
        slider.addMouseListener (this, false);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        setTooltipText();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (compact)   // dial on the left, name + value on the right (used in the header)
        {
            // printed straight on the brushed metal: engraved dark text
            r.removeFromLeft ((float) getHeight() + 6.0f);
            g.setFont (theme::labelFont (9.5f, true));
            hw::engravedText (g, name, r.removeFromTop (r.getHeight() * 0.5f), juce::Justification::bottomLeft, hw::inkSoft);
            g.setFont (theme::font (11.5f, true));
            hw::engravedText (g, param->getCurrentValueAsText(), r, juce::Justification::topLeft, hw::ink);
            return;
        }
        g.setColour (theme::goldDim);
        g.setFont (theme::labelFont (10.5f, true));
        g.drawText (name, r.removeFromTop (16.0f), juce::Justification::centred, false);

        const bool hot = slider.isMouseOverOrDragging();
        g.setColour (hot ? theme::goldHi : theme::ivory.withAlpha (0.8f));
        g.setFont (theme::font (11.5f));
        g.drawText (param->getCurrentValueAsText(), r.removeFromBottom (16.0f), juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (compact) { slider.setBounds (r.removeFromLeft (getHeight())); return; }
        r.removeFromTop (16);
        r.removeFromBottom (16);
        const int s = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (s, s));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void setTooltipText() { slider.setTooltip (name + "  (double-click to reset)"); }

    juce::String name;
    bool compact = false;
    juce::RangedAudioParameter* param = nullptr;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Pill-shaped segmented switch for choice / bool parameters.
class Segmented final : public juce::Component
{
public:
    Segmented (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::StringArray segmentLabels)
        : labels (std::move (segmentLabels)),
          attachment (*state.getParameter (paramId), [this] (float v) { index = juce::roundToInt (v); repaint(); }, state.undoManager)
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // Hardware push-buttons sitting in a recessed slot; the selected one is pressed in and lit.
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 5.0f);
        g.setColour (Colour (0xff0b0c0e));
        g.fillRoundedRectangle (r, 5.0f);

        const float segW = r.getWidth() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto seg = Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight()).reduced (2.0f, 2.0f);
            const bool on = i == index;
            if (on)
            {
                // lit button: coloured light shining through, glow around it
                g.setColour (theme::gold.withAlpha (0.25f));
                g.fillRoundedRectangle (seg.expanded (1.5f), 4.0f);
                g.setGradientFill (theme::goldGradient (seg));
                g.fillRoundedRectangle (seg, 3.0f);
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), 0.0f, seg.getY(),
                                                   Colours::white.withAlpha (0.0f), 0.0f, seg.getCentreY(), false));
                g.fillRoundedRectangle (seg.withHeight (seg.getHeight() * 0.5f), 3.0f);
            }
            else
            {
                // raised dark button
                g.setGradientFill (ColourGradient (Colour (0xff2c2a27), 0.0f, seg.getY(), Colour (0xff0e0e0f), 0.0f, seg.getBottom(), false));
                g.fillRoundedRectangle (seg, 3.0f);
                g.setColour (Colours::white.withAlpha (0.18f));
                g.drawLine (seg.getX() + 3.0f, seg.getY() + 0.8f, seg.getRight() - 3.0f, seg.getY() + 0.8f, 1.0f);
            }
            g.setColour (on ? Colour (0xff1d1508) : theme::goldDim);
            g.setFont (theme::labelFont (10.5f, true));
            g.drawText (labels[i], seg, Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, labels.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) labels.size()));
        attachment.setValueAsCompleteGesture ((float) i);
    }

private:
    juce::StringArray labels;
    int index = 0;
    juce::ParameterAttachment attachment;
};

// Preset picker for a choice parameter: "<  NAME  >" in a recessed slot.
// The arrows step through the presets; clicking the name opens the full list.
class PresetSelector final : public juce::Component, public juce::SettableTooltipClient
{
public:
    PresetSelector (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::String caption)
        : title (std::move (caption)),
          param (dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId))),
          attachment (*state.getParameter (paramId), [this] (float v) { index = juce::roundToInt (v); repaint(); }, state.undoManager)
    {
        jassert (param != nullptr);
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Click to pick a preset, arrows to step through them");
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 5.0f);
        g.setColour (Colour (0xff0b0c0e));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::gold.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 0.8f);

        auto inner = r.reduced (4.0f, 2.0f);
        auto left = inner.removeFromLeft (16.0f), right = inner.removeFromRight (16.0f);
        auto arrow = [&] (Rectangle<float> a, bool pointsLeft)
        {
            Path p;
            const auto c = a.getCentre();
            if (pointsLeft) p.addTriangle (c.x + 3.0f, c.y - 4.5f, c.x + 3.0f, c.y + 4.5f, c.x - 3.5f, c.y);
            else            p.addTriangle (c.x - 3.0f, c.y - 4.5f, c.x - 3.0f, c.y + 4.5f, c.x + 3.5f, c.y);
            g.setGradientFill (theme::goldGradient (a.reduced (0.0f, 4.0f)));
            g.fillPath (p);
        };
        arrow (left, true);
        arrow (right, false);

        g.setFont (theme::labelFont (9.0f, true));
        g.setColour (theme::goldDim);
        const auto cap = title + "  ";
        const float capW = GlyphArrangement::getStringWidth (g.getCurrentFont(), cap);
        g.drawText (cap, inner.removeFromLeft (capW), Justification::centredLeft, false);
        g.setFont (theme::labelFont (10.5f, true));
        g.setColour (theme::goldHi);
        g.drawText (param != nullptr ? param->choices[index].toUpperCase() : String(), inner, Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        const int n = param->choices.size();
        if (e.x < 20)                   { set ((index + n - 1) % n); return; }
        if (e.x > getWidth() - 20)      { set ((index + 1) % n); return; }
        juce::PopupMenu menu;
        for (int i = 0; i < n; ++i) menu.addItem (i + 1, param->choices[i], true, i == index);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<PresetSelector> (this)] (int result)
                            {
                                if (safe != nullptr && result > 0) safe->set (result - 1);
                            });
    }

private:
    void set (int i) { attachment.setValueAsCompleteGesture ((float) i); }

    juce::String title;
    juce::AudioParameterChoice* param = nullptr;
    int index = 0;
    juce::ParameterAttachment attachment;
};
//==============================================================================
// Gold die: rolls a new random value for an int parameter (the S&H / RANDOM pattern seed).
class DiceButton final : public juce::Button
{
public:
    DiceButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
        : juce::Button ("Roll a new random pattern"), param (state.getParameter (paramId))
    {
        setTooltip ("Roll a new random pattern");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        onClick = [this]
        {
            if (param == nullptr) return;
            param->beginChangeGesture();
            param->setValueNotifyingHost (juce::Random::getSystemRandom().nextFloat());
            param->endChangeGesture();
            face = 1 + (face + juce::Random::getSystemRandom().nextInt (5)) % 6;   // show a different face
            repaint();
        };
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        r = r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight()));
        if (down) r = r.reduced (0.8f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 3.5f);
        g.setGradientFill (theme::goldGradient (r, over ? 1.0f : 0.85f));
        g.fillRoundedRectangle (r, 3.5f);
        g.setColour (Colour (0xff3a2a0c));
        g.drawRoundedRectangle (r, 3.5f, 0.8f);
        // pips
        const float u = r.getWidth() / 4.0f, pr = r.getWidth() * 0.09f;
        auto pip = [&] (float cx, float cy) { g.fillEllipse (r.getX() + cx * u - pr, r.getY() + cy * u - pr, pr * 2.0f, pr * 2.0f); };
        g.setColour (Colour (0xff1d1508));
        static const int faces[7][7][2] = { {}, { {2,2} }, { {1,1},{3,3} }, { {1,1},{2,2},{3,3} }, { {1,1},{3,1},{1,3},{3,3} },
                                            { {1,1},{3,1},{2,2},{1,3},{3,3} }, { {1,1},{3,1},{1,2},{3,2},{1,3},{3,3} } };
        for (int i = 0; i < face; ++i) pip ((float) faces[face][i][0], (float) faces[face][i][1]);
    }

private:
    juce::RangedAudioParameter* param = nullptr;
    int face = 5;
};
} // namespace rsq
