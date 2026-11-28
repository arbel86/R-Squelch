#pragma once
// R-Squelch — colours, fonts and the iridescent accent gradient.

#include <juce_gui_basics/juce_gui_basics.h>

namespace rsq::theme
{
const juce::Colour background { 0xff0b0b0e };
const juce::Colour panel      { 0xff141418 };
const juce::Colour panelHi    { 0xff1a1a20 };
const juce::Colour display    { 0xff0e0e12 };
const juce::Colour border     { 0xff232329 };
const juce::Colour grid       { 0xff1c1c23 };
const juce::Colour text       { 0xffededed };
const juce::Colour textDim    { 0xff8a8a95 };
const juce::Colour violet     { 0xff8b5cf6 };
const juce::Colour magenta    { 0xffec4899 };
const juce::Colour cyan       { 0xff22d3ee };
const juce::Colour acid       { 0xffa3e635 };

// Black & gold hardware palette
const juce::Colour goldHi   { 0xfff7e6ae };
const juce::Colour gold     { 0xffd6af5c };
const juce::Colour goldLo   { 0xff7d5f22 };
const juce::Colour goldText { 0xffdcc07a };
const juce::Colour goldDim  { 0xffa48a52 };
const juce::Colour ivory    { 0xffefe7d6 };

// polished gold, lit from above
inline juce::ColourGradient goldGradient (juce::Rectangle<float> r, float alpha = 1.0f)
{
    juce::ColourGradient g (goldHi.withMultipliedAlpha (alpha), 0.0f, r.getY(), goldLo.withMultipliedAlpha (alpha), 0.0f, r.getBottom(), false);
    g.addColour (0.45, gold.withMultipliedAlpha (alpha));
    return g;
}

// Global animation phase, advanced by the editor's timer — drives the slow hue drift.
inline float& huePhase() { static float p = 0.0f; return p; }

inline juce::Colour shifted (juce::Colour c, float extra = 0.0f)
{
    return c.withRotatedHue (std::sin (huePhase()) * 0.05f + extra);
}

// violet -> magenta -> cyan -> acid green, across the given line
inline juce::ColourGradient iridescent (juce::Point<float> from, juce::Point<float> to, float alpha = 1.0f, float hueOffset = 0.0f)
{
    juce::ColourGradient g (shifted (violet, hueOffset).withMultipliedAlpha (alpha), from,
                            shifted (acid, hueOffset).withMultipliedAlpha (alpha), to, false);
    g.addColour (0.35, shifted (magenta, hueOffset).withMultipliedAlpha (alpha));
    g.addColour (0.70, shifted (cyan, hueOffset).withMultipliedAlpha (alpha));
    return g;
}

inline juce::ColourGradient iridescent (juce::Rectangle<float> r, float alpha = 1.0f, float hueOffset = 0.0f)
{
    return iridescent (r.getTopLeft(), r.getBottomRight(), alpha, hueOffset);
}

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

// Uppercase label with wide letter-spacing (section titles, knob names).
inline juce::Font labelFont (float height, bool bold = false)
{
    auto f = font (height, bold);
    f.setExtraKerningFactor (0.14f);
    return f;
}
} // namespace rsq::theme
