#pragma once
// R-Squelch — 3D hardware knobs: LED value ring, knurled skirt, brushed-aluminium cap, glowing pointer.

#include "Theme.h"

namespace rsq
{
class RSquelchLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RSquelchLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, theme::background);
        setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xff1b1b21));
        setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff0b0b0e));
        setColour (MidiKeyboardComponent::keySeparatorLineColourId, theme::background);
        setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme::violet.withAlpha (0.25f));
        setColour (MidiKeyboardComponent::keyDownOverlayColourId, theme::magenta.withAlpha (0.75f));
        setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
        setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, theme::panel);
        setColour (MidiKeyboardComponent::upDownButtonArrowColourId, theme::textDim);
        setColour (MidiKeyboardComponent::textLabelColourId, theme::textDim);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float stroke = jmax (2.5f, radius * 0.1f);
        const float arcR = radius - stroke * 0.5f;

        const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
        const bool hot = slider.isMouseOverOrDragging();
        const float valueAngle = startAngle + pos * (endAngle - startAngle);
        const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        // LED arc groove (cut into the panel)
        Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colour (0xff0b0c0e));
        g.strokePath (track, PathStrokeType (stroke + 1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xff202227));
        g.strokePath (track, PathStrokeType (stroke - 0.5f, PathStrokeType::curved, PathStrokeType::rounded));

        // value arc: glowing gold light ring
        if (std::abs (valueAngle - fromAngle) > 0.001f)
        {
            Path arc;
            arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (fromAngle, valueAngle), jmax (fromAngle, valueAngle), true);
            auto area = bounds.expanded (2.0f);
            g.setColour (theme::gold.withAlpha (hot ? 0.35f : 0.18f));                 // warm glow
            g.strokePath (arc, PathStrokeType (stroke * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setGradientFill (theme::goldGradient (area));
            g.strokePath (arc, PathStrokeType (stroke - 0.5f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // ---- 3D metal knob ----
        const float kr = arcR - stroke * 1.9f;                       // skirt radius
        const auto knobArea = [&] (float r) { return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };

        // drop shadow (light comes from the top-left)
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (knobArea (kr).translated (kr * 0.06f, kr * 0.16f));
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (knobArea (kr * 1.06f).translated (kr * 0.10f, kr * 0.24f));

        // dark knurled skirt
        g.setGradientFill (ColourGradient (Colour (0xff36332e), centre.x, centre.y - kr, Colour (0xff070707), centre.x, centre.y + kr, false));
        g.fillEllipse (knobArea (kr));
        g.setColour (theme::gold.withAlpha (0.10f));
        for (int i = 0; i < 24; ++i)                                  // grip ridges
        {
            const float a = (float) i * MathConstants<float>::twoPi / 24.0f;
            const Point<float> d (std::sin (a), -std::cos (a));
            g.drawLine (Line<float> (centre + d * (kr * 0.86f), centre + d * (kr * 0.99f)), 1.0f);
        }

        // polished gold cap
        const float cr = kr * 0.78f;
        g.setGradientFill (ColourGradient (theme::goldHi, centre.x - cr * 0.7f, centre.y - cr * 0.9f,
                                           theme::goldLo, centre.x + cr * 0.6f, centre.y + cr, false));
        g.fillEllipse (knobArea (cr));
        g.setColour (Colours::black.withAlpha (0.05f));               // concentric machining rings
        for (float f = 0.25f; f < 1.0f; f += 0.15f)
            g.drawEllipse (knobArea (cr * f), 0.7f);
        // specular highlight
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), centre.x - cr * 0.35f, centre.y - cr * 0.45f,
                                           Colours::white.withAlpha (0.0f), centre.x - cr * 0.35f + cr * 0.6f, centre.y - cr * 0.45f, true));
        g.fillEllipse (knobArea (cr));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawEllipse (knobArea (cr), 0.9f);

        // pointer: engraved line on the cap + glowing dot on the skirt
        const auto dir = Point<float> (std::sin (valueAngle), -std::cos (valueAngle));
        g.setColour (theme::goldHi.withAlpha (0.7f));
        g.drawLine (Line<float> (centre + dir * (cr * 0.25f) + Point<float> (0.6f, 0.8f), centre + dir * (cr * 0.92f) + Point<float> (0.6f, 0.8f)), 1.4f);
        g.setColour (Colour (0xff2a1f0b));
        g.drawLine (Line<float> (centre + dir * (cr * 0.25f), centre + dir * (cr * 0.92f)), jmax (1.8f, stroke * 0.65f));
        const auto tip = centre + dir * (kr * 0.92f);
        g.setColour (theme::gold.withAlpha (0.45f));
        g.fillEllipse (Rectangle<float> (stroke * 1.9f, stroke * 1.9f).withCentre (tip));
        g.setColour (theme::ivory);
        g.fillEllipse (Rectangle<float> (stroke * 0.9f, stroke * 0.9f).withCentre (tip));
    }
};
} // namespace rsq
