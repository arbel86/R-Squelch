#pragma once
// R-Squelch — slim on-screen keyboard in the plugin's style (mouse + computer keyboard: A W S E D ...).

#include <juce_audio_utils/juce_audio_utils.h>
#include "Theme.h"

namespace rsq
{
class Keyboard final : public juce::MidiKeyboardComponent
{
public:
    using MidiKeyboardComponent::MidiKeyboardComponent;

    // ivory key: light gradient, shaded front lip, separation shadow; pressed keys sink and glow
    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour, juce::Colour) override
    {
        using namespace juce;
        auto key = area.reduced (0.6f, 0.0f);
        const Colour top = isDown ? Colour (0xffcfd2d6) : (isOver ? Colour (0xfffbfbf9) : Colour (0xfff3f3f0));
        const Colour bottom = isDown ? Colour (0xffb7bac0) : Colour (0xffd7d8d4);
        g.setGradientFill (ColourGradient (top, 0.0f, key.getY(), bottom, 0.0f, key.getBottom(), false));
        g.fillRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f);
        if (isDown)
        {
            g.setColour (theme::gold.withAlpha (0.45f));
            g.fillRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f);
        }
        // front lip (gets shorter when pressed: the key sinks)
        const float lip = isDown ? 3.0f : 6.0f;
        g.setGradientFill (ColourGradient (Colour (0xffc4c6c4), 0.0f, key.getBottom() - lip, Colour (0xff9c9fa3), 0.0f, key.getBottom(), false));
        g.fillRoundedRectangle (key.removeFromBottom (lip), 2.0f);
        // separation shadow on the right edge
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillRect (area.getRight() - 1.0f, area.getY(), 1.0f, area.getHeight());

        if (note % 12 == 0)
        {
            g.setColour (Colour (0xff6a6f78));
            g.setFont (theme::labelFont (9.0f, true));
            g.drawText (getWhiteNoteText (note), area.withTrimmedBottom (8.0f).removeFromBottom (14.0f), Justification::centred, false);
        }
    }

    // glossy black key with a highlight on top and a darker front
    void drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override
    {
        using namespace juce;
        auto key = area.reduced (1.2f, 0.0f);
        g.setColour (Colours::black.withAlpha (0.45f));                         // shadow on the white keys
        g.fillRoundedRectangle (key.translated (1.5f, 2.0f).withTrimmedTop (-4.0f), 2.5f);
        g.setGradientFill (ColourGradient (Colour (isDown ? 0xff15161a : 0xff3a3d43), 0.0f, key.getY(),
                                           Colour (0xff08090b), 0.0f, key.getBottom(), false));
        g.fillRoundedRectangle (key.withTrimmedTop (-4.0f), 2.5f);
        if (isDown || isOver)
        {
            g.setColour (theme::gold.withAlpha (isDown ? 0.75f : 0.2f));
            g.fillRoundedRectangle (key.reduced (1.5f).withTrimmedTop (-4.0f), 2.0f);
        }
        // top highlight + front bevel
        auto inner = key.reduced (2.0f, 0.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), 0.0f, inner.getY(),
                                           Colours::white.withAlpha (0.0f), 0.0f, inner.getY() + inner.getHeight() * 0.7f, false));
        g.fillRoundedRectangle (inner.withHeight (inner.getHeight() * 0.75f), 1.5f);
        g.setColour (Colour (0xff1c1d21));
        g.fillRoundedRectangle (key.removeFromBottom (isDown ? 3.0f : 5.0f), 1.5f);
    }

    void drawUpDownButton (juce::Graphics&, int, int, bool, bool, bool) override {}
};
} // namespace rsq
