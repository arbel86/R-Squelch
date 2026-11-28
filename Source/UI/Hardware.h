#pragma once
// R-Squelch — black & gold hardware drawing: black anodised faceplate, gold trims and screws,
// recessed black modules with gold hairlines, glass screens in polished gold bezels.
// The psychedelic colours live only inside the screens (curves, moon, lightning).

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "BinaryData.h"

namespace rsq::hw
{
// text printed on the faceplate: gold inlay with a dark shadow edge
const juce::Colour ink     = theme::goldText;
const juce::Colour inkSoft = theme::goldDim;
const juce::Colour etch    { 0xcc000000 };

// No static juce::Image anywhere: JUCE 8 images can hold Direct2D (GPU) resources, and freeing them
// during plug-in unload can freeze Cubase / FL Studio. ImageCache keeps one shared copy anyway.
inline juce::Image brushedMetal()
{
    return juce::ImageCache::getFromMemory (BinaryData::brushed_black_jpg, BinaryData::brushed_black_jpgSize);
}

inline void engravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                          juce::Justification just, juce::Colour colour = ink)
{
    g.setColour (etch);
    g.drawText (text, area.translated (0.0f, 1.0f), just, false);
    g.setColour (colour);
    g.drawText (text, area, just, false);
}

inline void screw (juce::Graphics& g, juce::Point<float> c, float r)
{
    using namespace juce;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.0f + 2.0f, r * 2.0f + 2.0f).withCentre (c.translated (0.0f, 0.6f)));
    g.setGradientFill (ColourGradient (theme::goldHi, c.x - r, c.y - r, theme::goldLo, c.x + r, c.y + r, false));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colour (0xff3a2c10));
    g.drawLine (c.x - r * 0.6f, c.y + r * 0.6f, c.x + r * 0.6f, c.y - r * 0.6f, 1.3f);   // single slot, angled
}

constexpr float kChamfer = 18.0f;

inline juce::Path faceplateShape (juce::Rectangle<float> r, float c = kChamfer)
{
    juce::Path p;
    p.addRoundedRectangle (r, c * 0.55f);
    return p;
}

// Black anodised faceplate with a polished gold edge and four gold screws.
inline void chassis (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    const auto shape = faceplateShape (r);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        const auto& metal = brushedMetal();
        if (metal.isValid()) g.drawImage (metal, r, RectanglePlacement::stretchToFit);
        else g.fillAll (Colour (0xff141518));
    }
    // gold edge trim: bright on top, deeper towards the bottom
    g.setGradientFill (theme::goldGradient (r));
    g.strokePath (faceplateShape (r.reduced (1.5f), kChamfer * 0.55f - 1.0f), PathStrokeType (2.2f));
    g.setColour (Colours::black);
    g.strokePath (shape, PathStrokeType (1.0f));
    // inner hairline
    g.setColour (theme::gold.withAlpha (0.25f));
    g.strokePath (faceplateShape (r.reduced (6.0f), kChamfer * 0.55f - 4.0f), PathStrokeType (0.8f));

    for (auto c : { r.getTopLeft() + Point<float> (22, 20), r.getTopRight() + Point<float> (-22, 20),
                    r.getBottomLeft() + Point<float> (22, -20), r.getBottomRight() + Point<float> (-22, -20) })
        screw (g, c, 4.5f);
}

constexpr float kTitleStrip = 30.0f;

// A recessed black module with a thin gold outline and a title strip.
inline void module (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool active = true)
{
    using namespace juce;
    const float corner = 8.0f;

    // cut into the faceplate: deep shadow above, a faint gold catch-light below
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (r.translated (0.0f, -1.0f).expanded (1.5f), corner + 1.5f);
    g.setColour (theme::gold.withAlpha (0.18f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f).expanded (0.5f), corner + 0.5f);

    g.setGradientFill (ColourGradient (Colour (0xff111114), 0.0f, r.getY(), Colour (0xff09090b), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);

    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.7f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false));
        g.fillRect (r.withHeight (10.0f));

        auto strip = r.withHeight (kTitleStrip);
        g.setGradientFill (ColourGradient (Colour (0xff070708), 0.0f, strip.getY(), Colour (0xff0b0b0d), 0.0f, strip.getBottom(), false));
        g.fillRect (strip);
        // gold hairline under the title
        ColourGradient line (theme::gold.withAlpha (0.0f), strip.getX(), 0.0f, theme::gold.withAlpha (0.0f), strip.getRight(), 0.0f, false);
        line.addColour (0.15, theme::gold.withAlpha (0.55f));
        line.addColour (0.85, theme::gold.withAlpha (0.55f));
        g.setGradientFill (line);
        g.fillRect (strip.getX(), strip.getBottom() - 1.0f, strip.getWidth(), 1.0f);
    }
    // gold outline
    g.setColour (theme::gold.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);

    // jewel LED + title
    auto led = Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 16.0f, r.getY() + kTitleStrip * 0.5f });
    if (active)
    {
        g.setColour (theme::gold.withAlpha (0.25f));
        g.fillEllipse (led.expanded (4.0f));
        g.setGradientFill (theme::goldGradient (led));
        g.fillEllipse (led);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillEllipse (led.reduced (2.3f).translated (-0.6f, -0.6f));
    }
    else
    {
        g.setColour (Colour (0xff2a2722));
        g.fillEllipse (led);
    }
    g.setColour (active ? theme::goldText : theme::goldDim.withAlpha (0.6f));
    g.setFont (theme::labelFont (11.5f, true));
    g.drawText (title, Rectangle<float> (r.getX() + 28.0f, r.getY(), r.getWidth() - 40.0f, kTitleStrip), Justification::centredLeft, false);
}

// Glass screen in a polished gold bezel. Returns the screen area.
inline juce::Rectangle<float> screen (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    g.setGradientFill (theme::goldGradient (r));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (2.5f), 5.5f, 1.0f);
    auto s = r.reduced (3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff0c0d12), 0.0f, s.getY(), Colour (0xff040406), 0.0f, s.getBottom(), false));
    g.fillRoundedRectangle (s, 5.0f);
    return s;
}

inline void glass (juce::Graphics& g, juce::Rectangle<float> s)
{
    using namespace juce;
    Graphics::ScopedSaveState ss (g);
    Path clip; clip.addRoundedRectangle (s, 5.0f);
    g.reduceClipRegion (clip);
    Path sheen;
    sheen.startNewSubPath (s.getX(), s.getY());
    sheen.lineTo (s.getX() + s.getWidth() * 0.62f, s.getY());
    sheen.lineTo (s.getX() + s.getWidth() * 0.32f, s.getBottom());
    sheen.lineTo (s.getX(), s.getBottom());
    sheen.closeSubPath();
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), s.getX(), s.getY(),
                                       Colours::white.withAlpha (0.0f), s.getX() + s.getWidth() * 0.45f, s.getBottom(), false));
    g.fillPath (sheen);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 0.0f, s.getY(), Colours::transparentBlack, 0.0f, s.getY() + 8.0f, false));
    g.fillRect (s.withHeight (8.0f));
}

// Around the faceplate: a deep black velvet backdrop and a soft shadow — the unit floats, framed in gold.
inline void synthBody (juce::Graphics& g, juce::Rectangle<float> stage, juce::Rectangle<float> face)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Colour (0xff1b1a19), stage.getCentreX(), stage.getCentreY(),
                                       Colour (0xff030303), stage.getX(), stage.getY(), true));
    g.fillRect (stage);
    for (int i = 0; i < 8; ++i)   // soft drop shadow
    {
        const float grow = 2.0f + 3.0f * (float) i;
        g.setColour (Colours::black.withAlpha (0.11f));
        g.fillRoundedRectangle (face.expanded (grow).translated (0.0f, 6.0f + (float) i), kChamfer * 0.55f + grow);
    }
}

inline void synthBodyOverlay (juce::Graphics&, juce::Rectangle<float>) {}
} // namespace rsq::hw
