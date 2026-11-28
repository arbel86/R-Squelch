#pragma once
// R-Squelch — the alien step pyramid: deep-space backdrop, a golden capstone with a glowing eye,
// stepped basalt tiers with gold inlay, and the ancient alien lettering of the title.
// Everything here is vector drawing (plus one stone texture), so it stays sharp at any zoom.

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "BinaryData.h"

namespace rsq::pyramid
{
//==============================================================================
// Geometry (design units). The pyramid is centred; each tier is a slightly sloped trapezoid
// whose top band is the lit ledge the tier above stands on.
struct Tier { float top, bottom, halfTop, halfBottom, ledge; };

constexpr float kWidth = 1300.0f, kHeight = 1056.0f, kCx = kWidth * 0.5f;
constexpr float kApexY = 24.0f, kCapBase = 104.0f, kCapHalf = 100.0f;

constexpr Tier kTiers[] = {
    { 104.0f,  186.0f, 250.0f, 262.0f, 12.0f },   // T1  title
    { 186.0f,  600.0f, 400.0f, 420.0f, 14.0f },   // T2  OSC | FILTER
    { 600.0f,  876.0f, 530.0f, 548.0f, 14.0f },   // T3  AMP | FILTER MOTION | PITCH MOTION | MASTER
    { 876.0f, 1056.0f, 610.0f, 630.0f, 12.0f },   // T4  effects — the base of the pyramid
};

inline juce::Path tierPath (const Tier& t)
{
    juce::Path p;
    p.startNewSubPath (kCx - t.halfTop, t.top);
    p.lineTo (kCx + t.halfTop, t.top);
    p.lineTo (kCx + t.halfBottom, t.bottom);
    p.lineTo (kCx - t.halfBottom, t.bottom);
    p.closeSubPath();
    return p;
}

inline float halfAt (const Tier& t, float y)
{
    return juce::jmap (y, t.top, t.bottom, t.halfTop, t.halfBottom);
}

// (not static — see brushedMetal(): a static Image can freeze some hosts when the plug-in unloads)
inline juce::Image stone()
{
    return juce::ImageCache::getFromMemory (BinaryData::stone_jpg, BinaryData::stone_jpgSize);
}

//==============================================================================
// Alien glyph alphabet: angular polylines on a 6 x 10 grid (y down). Each glyph is a list of strokes,
// plus optional diamond "seals" — the dots that make the letters read as an ancient script.
struct Glyph
{
    float width = 6.0f;
    std::vector<std::vector<juce::Point<float>>> strokes;
    std::vector<juce::Point<float>> seals;
};

inline Glyph glyphFor (juce::juce_wchar c)
{
    using P = juce::Point<float>;
    Glyph gl;
    switch (c)
    {
        case 'R': gl.strokes = { { P (0, 10), P (0, 0), P (4.4f, 0), P (6, 2.5f), P (4.4f, 5), P (0, 5) },
                                 { P (2.6f, 5), P (6, 10) },
                                 { P (-0.9f, 10), P (0.9f, 10) } };
                  gl.seals = { P (2.6f, 2.5f) }; break;
        case 'S': gl.strokes = { { P (6, 1.2f), P (4.8f, 0), P (1.2f, 0), P (0, 1.4f), P (0, 3.6f), P (6, 6.4f), P (6, 8.6f), P (4.8f, 10), P (1.2f, 10), P (0, 8.8f) } };
                  gl.seals = { P (3, 5) }; break;
        case 'Q': gl.strokes = { { P (1.5f, 0), P (4.5f, 0), P (6, 2), P (6, 8), P (4.5f, 10), P (1.5f, 10), P (0, 8), P (0, 2), P (1.5f, 0) },
                                 { P (3.6f, 7.2f), P (6.8f, 11.4f) } };
                  gl.seals = { P (3, 4.4f) }; break;
        case 'U': gl.strokes = { { P (0, 0), P (0, 8), P (1.8f, 10), P (4.2f, 10), P (6, 8), P (6, 0) },
                                 { P (-0.9f, 0), P (0.9f, 0) }, { P (5.1f, 0), P (6.9f, 0) } };
                  gl.seals = { P (3, 6.6f) }; break;
        case 'E': gl.strokes = { { P (6, 0), P (0, 0), P (0, 10), P (6, 10) }, { P (0, 5), P (3.6f, 5) } };
                  gl.seals = { P (5.3f, 5) }; break;
        case 'L': gl.strokes = { { P (0, 0), P (0, 10), P (6, 10), P (6, 8.2f) }, { P (-0.9f, 0), P (0.9f, 0) } };
                  gl.seals = { P (3.2f, 6.6f) }; break;
        case 'C': gl.strokes = { { P (6, 1.6f), P (4.5f, 0), P (1.5f, 0), P (0, 2), P (0, 8), P (1.5f, 10), P (4.5f, 10), P (6, 8.4f) } };
                  gl.seals = { P (3.4f, 5) }; break;
        case 'H': gl.strokes = { { P (0, 0), P (0, 10) }, { P (6, 0), P (6, 10) }, { P (0, 6), P (6, 4) } };
                  gl.seals = { P (3, 1.8f), P (3, 8.2f) }; break;
        case '-': gl.width = 4.0f;
                  gl.strokes = { { P (0, 5), P (4, 5) } };
                  gl.seals = {}; break;
        default:  gl.width = 3.0f; break;
    }
    return gl;
}

inline void addDiamond (juce::Path& p, juce::Point<float> c, float r)
{
    p.startNewSubPath (c.x, c.y - r);
    p.lineTo (c.x + r, c.y);
    p.lineTo (c.x, c.y + r);
    p.lineTo (c.x - r, c.y);
    p.closeSubPath();
}

// Builds the title as outline paths (strokes) and filled seals, in design coordinates.
inline float layoutWord (const juce::String& text, juce::Point<float> origin, float unit, float tracking,
                         juce::Path& strokes, juce::Path& seals)
{
    float x = origin.x;
    for (auto c : text)
    {
        const auto gl = glyphFor (c);
        for (auto& s : gl.strokes)
        {
            strokes.startNewSubPath (x + s[0].x * unit, origin.y + s[0].y * unit);
            for (size_t i = 1; i < s.size(); ++i)
                strokes.lineTo (x + s[i].x * unit, origin.y + s[i].y * unit);
        }
        for (auto& d : gl.seals)
            addDiamond (seals, { x + d.x * unit, origin.y + d.y * unit }, unit * 0.62f);
        if (c == '-')
            addDiamond (seals, { x + 2.0f * unit, origin.y + 5.0f * unit }, unit * 1.05f);
        x += gl.width * unit + tracking;
    }
    return x - tracking - origin.x;
}

inline float wordWidth (const juce::String& text, float unit, float tracking)
{
    float w = 0.0f;
    for (auto c : text) w += glyphFor (c).width * unit + tracking;
    return w - tracking;
}

// Gold-inlaid alien title, centred in the given area.
inline void title (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area)
{
    using namespace juce;
    const float unit = area.getHeight() / 11.0f, tracking = unit * 2.4f;
    const float w = wordWidth (text, unit, tracking);
    const Point<float> origin (area.getCentreX() - w * 0.5f, area.getY() + unit * 0.4f);

    Path strokes, seals;
    layoutWord (text, origin, unit, tracking, strokes, seals);
    const PathStrokeType thick (unit * 0.95f, PathStrokeType::mitered, PathStrokeType::square);

    // the carved channel, shadow, glow, gold inlay, then a dark hairline groove down the middle
    Path carved; thick.createStrokedPath (carved, strokes);
    carved.addPath (seals);

    for (int i = 3; i >= 1; --i)
    {
        g.setColour (theme::gold.withAlpha (0.05f * (float) (4 - i)));
        g.strokePath (carved, PathStrokeType (unit * 0.9f * (float) i, PathStrokeType::mitered));
    }
    g.setColour (Colours::black.withAlpha (0.85f));
    g.fillPath (carved, AffineTransform::translation (0.0f, unit * 0.35f));
    g.setGradientFill (theme::goldGradient (area));
    g.fillPath (carved);
    g.setColour (theme::goldHi.withAlpha (0.55f));
    g.strokePath (carved, PathStrokeType (0.6f));
    g.setColour (Colour (0xff3b2a0c).withAlpha (0.75f));
    g.strokePath (strokes, PathStrokeType (juce::jmax (0.8f, unit * 0.18f), PathStrokeType::mitered, PathStrokeType::butt));

    // cartouche rules above and below, ending in small triangles
    auto rule = [&] (float y, bool up)
    {
        const float x0 = origin.x - unit * 3.0f, x1 = origin.x + w + unit * 3.0f;
        ColourGradient lg (theme::gold.withAlpha (0.0f), x0 - unit * 6.0f, y, theme::gold.withAlpha (0.0f), x1 + unit * 6.0f, y, false);
        lg.addColour (0.15, theme::gold.withAlpha (0.8f));
        lg.addColour (0.85, theme::gold.withAlpha (0.8f));
        g.setGradientFill (lg);
        g.fillRect (x0 - unit * 6.0f, y - 0.6f, (x1 - x0) + unit * 12.0f, 1.2f);
        g.setColour (theme::gold);
        for (float x : { x0, x1, area.getCentreX() })
        {
            Path t;
            const float s = unit * 0.9f * (up ? -1.0f : 1.0f);
            t.addTriangle (x - unit * 0.9f, y, x + unit * 0.9f, y, x, y + s);
            g.fillPath (t);
        }
    };
    rule (area.getY() - unit * 0.9f, false);
    rule (area.getBottom() + unit * 0.4f, true);
}

//==============================================================================
// Small decorative runes for the ledges (deterministic, so they never flicker).
inline void runeBand (juce::Graphics& g, float x0, float x1, float yMid, float h, juce::Colour colour, int seed)
{
    using namespace juce;
    Random rnd (seed);
    Path p;
    const float u = h / 4.0f;
    for (float x = x0; x + 3.0f * u < x1; x += 4.6f * u)
    {
        const float y0 = yMid - 2.0f * u;
        const int kind = rnd.nextInt (8);
        auto L = [&] (float ax, float ay, float bx, float by) { p.startNewSubPath (x + ax * u, y0 + ay * u); p.lineTo (x + bx * u, y0 + by * u); };
        switch (kind)
        {
            case 0: L (0, 0, 0, 4); L (0, 0, 3, 2); L (3, 2, 0, 4); break;
            case 1: L (1.5f, 0, 1.5f, 4); L (0, 1, 3, 1); L (0, 3, 3, 3); break;
            case 2: L (0, 4, 1.5f, 0); L (1.5f, 0, 3, 4); L (0.7f, 2.2f, 2.3f, 2.2f); break;
            case 3: L (0, 0, 3, 4); L (3, 0, 0, 4); break;
            case 4: L (0, 0, 0, 4); L (3, 0, 3, 4); L (0, 2, 3, 2); break;
            case 5: L (0, 2, 1.5f, 0); L (1.5f, 0, 3, 2); L (3, 2, 1.5f, 4); L (1.5f, 4, 0, 2); break;
            case 6: L (1.5f, 0, 1.5f, 4); L (1.5f, 1, 3, 0); L (1.5f, 1, 0, 0); break;
            default: L (0, 4, 0, 0); L (0, 0, 3, 0); L (3, 0, 3, 2); L (3, 2, 1, 2); break;
        }
    }
    g.setColour (Colours::black.withAlpha (0.6f));
    g.strokePath (p, PathStrokeType (1.2f), AffineTransform::translation (0.0f, 0.8f));
    g.setColour (colour);
    g.strokePath (p, PathStrokeType (1.0f));
}

//==============================================================================
// Deep space: gradient sky, faint nebulae, stars.
inline void space (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Colour (0xff0b0716), kCx, 0.0f, Colour (0xff020205), kCx, r.getBottom(), false));
    g.fillRect (r);

    auto nebula = [&] (float x, float y, float rad, Colour c)
    {
        g.setGradientFill (ColourGradient (c, x, y, c.withAlpha (0.0f), x + rad, y, true));
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
    };
    nebula (150.0f, 260.0f, 320.0f, Colour (0x2a6b2bd9));
    nebula (1170.0f, 360.0f, 300.0f, Colour (0x2212a3b8));
    nebula (1050.0f, 120.0f, 220.0f, Colour (0x1ad94896));
    nebula (220.0f, 620.0f, 260.0f, Colour (0x1412b88a));

    Random rnd (1977);
    for (int i = 0; i < 420; ++i)
    {
        const float x = rnd.nextFloat() * r.getWidth(), y = rnd.nextFloat() * r.getHeight() * 0.8f;
        const float s = 0.5f + rnd.nextFloat() * rnd.nextFloat() * 1.8f;
        const float a = 0.25f + rnd.nextFloat() * 0.6f;
        const Colour c = rnd.nextFloat() < 0.15f ? theme::goldHi : (rnd.nextFloat() < 0.2f ? Colour (0xffbfe9ff) : Colours::white);
        g.setColour (c.withAlpha (a));
        g.fillEllipse (x - s * 0.5f, y - s * 0.5f, s, s);
        if (s > 1.7f)   // the brightest ones twinkle with a cross
        {
            g.setColour (c.withAlpha (a * 0.35f));
            g.fillRect (x - s * 2.5f, y - 0.3f, s * 5.0f, 0.6f);
            g.fillRect (x - 0.3f, y - s * 2.5f, 0.6f, s * 5.0f);
        }
    }
}

//==============================================================================
// One stepped tier: basalt face with masonry joints, lit ledge on top, bevelled sloped sides, gold inlay.
inline void tier (juce::Graphics& g, const Tier& t, int index)
{
    using namespace juce;
    const auto shape = tierPath (t);
    const auto bounds = shape.getBounds();

    // shadow cast on the space behind
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (shape, AffineTransform::translation (0.0f, 6.0f));

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        g.fillAll (Colour (0xff141210));   // solid base, so the stone is fully opaque even in the see-through window
        const auto stoneImg = stone();
        if (stoneImg.isValid())
            g.drawImage (stoneImg, Rectangle<float> (0.0f, 0.0f, kWidth, kHeight), RectanglePlacement::stretchToFit);
        else
            g.fillAll (Colour (0xff141210));

        // light from the upper left; deeper towards the bottom of the face
        ColourGradient side (Colours::white.withAlpha (0.05f), bounds.getX(), 0.0f, Colours::black.withAlpha (0.35f), bounds.getRight(), 0.0f, false);
        side.addColour (0.5, Colours::transparentBlack);
        g.setGradientFill (side);
        g.fillRect (bounds);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 0.0f, t.top + t.ledge, Colours::transparentBlack, 0.0f, t.top + t.ledge + 26.0f, false));
        g.fillRect (bounds.withTop (t.top + t.ledge).withHeight (26.0f));

        // masonry: courses every ~34 units, staggered joints
        const float course = 34.0f;
        int row = 0;
        for (float y = t.top + t.ledge + course; y < t.bottom - 4.0f; y += course, ++row)
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillRect (bounds.getX(), y, bounds.getWidth(), 1.0f);
            g.setColour (Colours::white.withAlpha (0.035f));
            g.fillRect (bounds.getX(), y + 1.0f, bounds.getWidth(), 1.0f);
        }
        row = 0;
        for (float y = t.top + t.ledge; y < t.bottom; y += course, ++row)
        {
            const float off = (row % 2 == 0 ? 0.0f : 46.0f) + (float) index * 23.0f;
            for (float x = bounds.getX() + off; x < bounds.getRight(); x += 92.0f)
            {
                g.setColour (Colours::black.withAlpha (0.4f));
                g.fillRect (x, y, 1.0f, juce::jmin (course, t.bottom - y));
            }
        }

        // the ledge: the stone top surface, catching the light
        Path ledge;
        const float lh = halfAt (t, t.top + t.ledge);
        ledge.startNewSubPath (kCx - t.halfTop, t.top);
        ledge.lineTo (kCx + t.halfTop, t.top);
        ledge.lineTo (kCx + lh, t.top + t.ledge);
        ledge.lineTo (kCx - lh, t.top + t.ledge);
        ledge.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xff3a342a), 0.0f, t.top, Colour (0xff1c1915), 0.0f, t.top + t.ledge, false));
        g.fillPath (ledge);
        ColourGradient lshade (Colours::white.withAlpha (0.06f), kCx - t.halfTop, 0.0f, Colours::black.withAlpha (0.45f), kCx + t.halfTop, 0.0f, false);
        g.setGradientFill (lshade);
        g.fillPath (ledge);
    }

    // bevel along the sloped sides: lit left, shaded right
    {
        const float bw = 7.0f;
        Path left, right;
        left.startNewSubPath (kCx - t.halfTop, t.top);
        left.lineTo (kCx - t.halfTop + bw, t.top + t.ledge);
        left.lineTo (kCx - t.halfBottom + bw, t.bottom);
        left.lineTo (kCx - t.halfBottom, t.bottom);
        left.closeSubPath();
        right.startNewSubPath (kCx + t.halfTop, t.top);
        right.lineTo (kCx + t.halfTop - bw, t.top + t.ledge);
        right.lineTo (kCx + t.halfBottom - bw, t.bottom);
        right.lineTo (kCx + t.halfBottom, t.bottom);
        right.closeSubPath();
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillPath (left);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillPath (right);
    }

    // gold inlay: the front edge of the ledge and the outline of the tier
    {
        const float y = t.top + t.ledge;
        const float lh = halfAt (t, y);
        ColourGradient lg (theme::goldLo, kCx - lh, y, theme::goldLo.darker (0.6f), kCx + lh, y, false);
        lg.addColour (0.3, theme::goldHi);
        lg.addColour (0.6, theme::gold);
        g.setGradientFill (lg);
        g.fillRect (kCx - lh, y - 0.5f, lh * 2.0f, 1.6f);
        g.setColour (theme::goldHi.withAlpha (0.55f));
        g.fillRect (kCx - t.halfTop, t.top, t.halfTop * 2.0f, 1.0f);

        g.setColour (theme::gold.withAlpha (0.55f));
        g.strokePath (shape, PathStrokeType (1.2f));
    }

    // runes engraved along the ledge
    runeBand (g, kCx - t.halfTop + 26.0f, kCx + t.halfTop - 26.0f, t.top + t.ledge * 0.5f,
              juce::jmin (8.0f, t.ledge - 4.0f), theme::gold.withAlpha (0.42f), 101 + index * 17);
}

//==============================================================================
// Golden capstone with the alien eye; `glow` 0..1 pulses the iris.
// blink 0 = open, 1 = closed
inline void capstone (juce::Graphics& g, float glow, bool rays = true, float blink = 0.0f)
{
    using namespace juce;
    const Point<float> apex (kCx, kApexY);
    const float baseY = kCapBase;

    // halo and rays behind the apex (not in the see-through window, where they would hang in the air over the desktop)
    if (rays)
    {
        g.setGradientFill (ColourGradient (Colour (0x55f7e6ae), apex.x, apex.y + 30.0f, Colour (0x00f7e6ae), apex.x + 150.0f, apex.y + 30.0f, true));
        g.fillEllipse (apex.x - 150.0f, apex.y + 30.0f - 150.0f, 300.0f, 300.0f);
        for (int i = 0; i < 18; ++i)
        {
            const float a = MathConstants<float>::pi * (1.05f + 0.9f * (float) i / 17.0f);
            const float len = (i % 2 == 0 ? 170.0f : 115.0f);
            Path ray;
            const Point<float> c (apex.x, apex.y + 30.0f);
            const Point<float> tip (c.x + std::cos (a) * len, c.y + std::sin (a) * len * 0.55f);
            const Point<float> n (-std::sin (a) * 5.0f, std::cos (a) * 5.0f);
            ray.addTriangle (c.x + n.x, c.y + n.y, c.x - n.x, c.y - n.y, tip.x, tip.y);
            g.setGradientFill (ColourGradient (theme::goldHi.withAlpha (0.16f), c, theme::goldHi.withAlpha (0.0f), tip, false));
            g.fillPath (ray);
        }
    }

    // two faces: lit left, shaded right
    Path left, right;
    left.addTriangle (apex.x, apex.y, kCx - kCapHalf, baseY, kCx, baseY);
    right.addTriangle (apex.x, apex.y, kCx, baseY, kCx + kCapHalf, baseY);
    g.setGradientFill (ColourGradient (theme::goldHi, apex.x - 40.0f, apex.y, theme::goldLo, kCx - kCapHalf, baseY, false));
    g.fillPath (left);
    g.setGradientFill (ColourGradient (theme::gold, apex.x, apex.y, Colour (0xff4a3712), kCx + kCapHalf, baseY, false));
    g.fillPath (right);
    g.setColour (Colours::white.withAlpha (0.7f));          // the ridge catches the light
    g.drawLine (apex.x, apex.y, kCx, baseY, 1.0f);
    g.setColour (theme::goldLo.darker (0.5f));
    Path whole; whole.addTriangle (apex.x, apex.y, kCx - kCapHalf, baseY, kCx + kCapHalf, baseY);
    g.strokePath (whole, PathStrokeType (1.0f));

    // the eye
    const Point<float> eye (kCx, 74.0f);
    const float ew = 32.0f, eh = 12.0f;
    Path almond;
    almond.startNewSubPath (eye.x - ew, eye.y);
    almond.quadraticTo (eye.x, eye.y - eh * 1.9f, eye.x + ew, eye.y);
    almond.quadraticTo (eye.x, eye.y + eh * 1.9f, eye.x - ew, eye.y);
    almond.closeSubPath();

    g.setColour (Colour (0xff1a1206));
    g.strokePath (almond, PathStrokeType (3.2f));
    g.setColour (Colour (0xff050806));
    g.fillPath (almond);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (almond);
        const Colour iris = Colour (0xff59ffb5).interpolatedWith (Colour (0xff3fd8ff), 0.5f + 0.5f * std::sin (theme::huePhase() * 0.7f));
        g.setGradientFill (ColourGradient (iris.withAlpha (0.55f + 0.45f * glow), eye.x, eye.y, iris.withAlpha (0.0f), eye.x + 22.0f, eye.y, true));
        g.fillEllipse (eye.x - 22.0f, eye.y - 22.0f, 44.0f, 44.0f);
        g.setColour (Colours::black);
        g.fillEllipse (Rectangle<float> (4.0f, 15.0f).withCentre (eye));      // slit pupil
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillEllipse (eye.x - 9.0f, eye.y - 6.0f, 3.0f, 3.0f);
    }
    g.setColour (theme::goldHi);
    g.strokePath (almond, PathStrokeType (1.1f));

    // eyelids: gold lids slide over the eye (the upper one travels further, like a real blink)
    if (blink > 0.001f)
    {
        const float b = jlimit (0.0f, 1.0f, blink);
        const float meet = eye.y + eh * 0.35f;                       // where the lids close
        const float upperEdge = jmap (b, eye.y - eh * 1.9f, meet);   // control point of the lid's edge
        const float lowerEdge = jmap (b, eye.y + eh * 1.9f, meet);
        Path upper, lower;
        upper.startNewSubPath (eye.x - ew, eye.y);
        upper.quadraticTo (eye.x, eye.y - eh * 1.9f, eye.x + ew, eye.y);
        upper.quadraticTo (eye.x, upperEdge, eye.x - ew, eye.y);
        upper.closeSubPath();
        lower.startNewSubPath (eye.x - ew, eye.y);
        lower.quadraticTo (eye.x, eye.y + eh * 1.9f, eye.x + ew, eye.y);
        lower.quadraticTo (eye.x, lowerEdge, eye.x - ew, eye.y);
        lower.closeSubPath();
        g.setGradientFill (ColourGradient (theme::goldHi, eye.x - ew, eye.y - eh * 2.0f, theme::goldLo, eye.x + ew, eye.y + eh * 2.0f, false));
        g.fillPath (upper);
        g.fillPath (lower);
        Path lidLine;
        lidLine.startNewSubPath (eye.x - ew, eye.y);
        lidLine.quadraticTo (eye.x, upperEdge, eye.x + ew, eye.y);
        g.setColour (Colour (0xff2a1d08));
        g.strokePath (lidLine, PathStrokeType (1.6f));
        g.setColour (theme::goldHi);
        g.strokePath (almond, PathStrokeType (1.1f));
    }
    // outer glow of the iris, spilling onto the gold
    Graphics::ScopedSaveState glowClip (g);
    g.reduceClipRegion (whole);
    g.setGradientFill (ColourGradient (Colour (0xff59ffb5).withAlpha (0.18f * glow), eye.x, eye.y, Colour (0x0059ffb5), eye.x + 60.0f, eye.y, true));
    g.fillEllipse (eye.x - 60.0f, eye.y - 60.0f, 120.0f, 120.0f);
}

inline juce::Rectangle<int> capstoneBounds() { return { (int) (kCx - 70.0f), 40, 140, 70 }; }

// The pyramid's silhouette (capstone + every tier) — used for clicks and to clip effects.
inline juce::Path outline()
{
    juce::Path p;
    p.addTriangle (kCx, kApexY, kCx - kCapHalf, kCapBase + 1.0f, kCx + kCapHalf, kCapBase + 1.0f);
    for (auto& t : kTiers)
        p.addPath (tierPath (t));
    return p;
}

// What fills the window around the pyramid:
//   space   — stars and nebulae (preview / screenshots)
//   black   — plain black, blends with the DAW's plugin frame (VST3)
//   none    — fully transparent: the standalone app is a borderless, pyramid-shaped window
enum class Background { space, black, none };

// The whole scene: backdrop, capstone and every tier.
//==============================================================================
// A Sphynx cat sitting upright on the top ledge beside the capstone: slender, long wrinkled neck,
// huge ears, pale blue eyes, pale pink skin, a cream tail curled around its side.
// t = time in seconds (tail tip sways, an ear twitches), catBlink 0..1.
constexpr float kCatX = 800.0f, kCatY = kCapBase;   // ground point under the front paws
constexpr float kCatScale = 0.8f;

// area the cat (with its head tilt, ears and tail) can paint, for a given ground point and scale
inline juce::Rectangle<int> catBoundsAt (juce::Point<float> ground, float scale)
{
    return juce::Rectangle<float> (ground.x - 62.0f * scale, ground.y - 132.0f * scale, 134.0f * scale, 142.0f * scale).getSmallestIntegerContainer();
}
inline juce::Rectangle<int> catBounds() { return catBoundsAt ({ kCatX, kCatY }, kCatScale); }

// The cat watches the filter, in every LFO preset:
//   tilt        — head tilt (radians), follows the filter position like a cat watching a laser dot
//   look -1..1  — the pupils follow it too (left = low, right = high)
//   flick 0..1  — ears and tail flick when the filter jumps, then settle
// and in S&H RANDOM mode it gets excited:
//   alert 0..1  — eyes glow in shifting psychedelic colours, pupils go wide
//   hue 0..1    — eye colour of the current step
struct CatMood { float alert = 0.0f, tilt = 0.0f, hue = 0.0f, flick = 0.0f, look = 0.0f; };

inline void sphynxCat (juce::Graphics& g, float t, float catBlink, const CatMood& mood = {},
                       juce::Point<float> ground = { kCatX, kCatY }, float scale = kCatScale)
{
    using namespace juce;
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::scale (scale).translated (ground.x, ground.y));   // local units: ground at y = 0, ~100 tall

    const Colour skinHi (0xfff3e2e0), skin (0xffdcc0c3), skinLo (0xffa88d95), line (0xff5e4750),
                 pink (0xffe6929f), tailHi (0xfffbf5e6), tailLo (0xffd9ccb0);
    const ColourGradient skinFill (skinHi, -24.0f, -95.0f, skinLo, 34.0f, 0.0f, false);

    // every body part: dark outline first (all parts), then the skin on top, so the joins look seamless
    auto outline = [&] (const Path& p) { g.setColour (line.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded)); };
    auto fill = [&] (const Path& p, float darker = 0.0f)
    {
        g.setGradientFill (skinFill);
        g.fillPath (p);
        if (darker > 0.0f) { g.setColour (skinLo.withAlpha (darker)); g.fillPath (p); }
    };

    // soft contact shadow on the stone
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 6.0f, 0.0f, Colours::transparentBlack, 46.0f, 0.0f, true));
    g.fillEllipse (-34.0f, -5.0f, 80.0f, 9.0f);

    // ---- shapes ----
    Path farLeg;  farLeg.addRoundedRectangle (-15.0f, -34.0f, 7.0f, 33.0f, 3.5f);
    Path nearLeg; nearLeg.addRoundedRectangle (-23.0f, -36.0f, 7.5f, 35.0f, 3.7f);
    Path paws;    paws.addEllipse (-26.0f, -4.5f, 11.0f, 5.5f); paws.addEllipse (-17.0f, -4.0f, 10.0f, 5.0f);

    Path torso;   // chest at the front (left), the back curving down to a big round rump
    torso.startNewSubPath (-19.0f, -66.0f);
    torso.cubicTo (-26.0f, -56.0f, -26.0f, -40.0f, -20.0f, -26.0f);
    torso.lineTo (-6.0f, -6.0f);
    torso.cubicTo (6.0f, -1.0f, 26.0f, -1.0f, 32.0f, -6.0f);
    torso.cubicTo (38.0f, -16.0f, 33.0f, -36.0f, 20.0f, -48.0f);
    torso.cubicTo (12.0f, -55.0f, 4.0f, -62.0f, -2.0f, -72.0f);
    torso.closeSubPath();
    Path haunch; haunch.addEllipse (2.0f, -40.0f, 33.0f, 39.0f);
    Path hindPaw; hindPaw.addEllipse (-7.0f, -5.0f, 15.0f, 5.5f);

    Path neck;    // long and slender, leaning slightly forward
    neck.startNewSubPath (-20.0f, -66.0f);
    neck.cubicTo (-21.0f, -74.0f, -19.0f, -80.0f, -18.0f, -84.0f);
    neck.lineTo (-6.0f, -84.0f);
    neck.cubicTo (-5.0f, -79.0f, -2.0f, -74.0f, 1.0f, -70.0f);
    neck.closeSubPath();

    // head: narrow wedge, high cheekbones, small chin
    const Point<float> hc (-13.0f, -88.0f);
    Path head;
    head.startNewSubPath (hc.x - 10.0f, hc.y - 6.0f);
    head.cubicTo (hc.x - 8.0f, hc.y - 12.0f, hc.x + 8.0f, hc.y - 12.0f, hc.x + 10.0f, hc.y - 6.0f);   // forehead
    head.cubicTo (hc.x + 11.5f, hc.y - 1.0f, hc.x + 9.0f, hc.y + 3.0f, hc.x + 5.0f, hc.y + 7.5f);     // cheek
    head.cubicTo (hc.x + 3.0f, hc.y + 10.5f, hc.x - 3.0f, hc.y + 10.5f, hc.x - 5.0f, hc.y + 7.5f);    // chin
    head.cubicTo (hc.x - 9.0f, hc.y + 3.0f, hc.x - 11.5f, hc.y - 1.0f, hc.x - 10.0f, hc.y - 6.0f);
    head.closeSubPath();

    // ears: huge, wide-set, pink inside; the right one twitches now and then
    const float twitchPhase = std::fmod (t, 7.3f);
    const float flick = (twitchPhase < 0.25f ? std::sin (twitchPhase / 0.25f * MathConstants<float>::pi) * 2.5f : 0.0f) + mood.flick * 3.0f;
    auto earPath = [&] (Point<float> b1, Point<float> tip, Point<float> b2)
    {
        Path e;
        e.startNewSubPath (b1);
        e.quadraticTo ((b1 + tip) * 0.5f + Point<float> (-2.5f, 0.0f), tip);
        e.quadraticTo ((b2 + tip) * 0.5f + Point<float> (2.5f, 0.0f), b2);
        e.closeSubPath();
        return e;
    };
    const Point<float> lb1 (hc.x - 10.5f, hc.y - 3.0f), ltip (hc.x - 17.0f - mood.flick * 3.0f, hc.y - 26.0f), lb2 (hc.x - 2.5f, hc.y - 10.5f);
    const Point<float> rb1 (hc.x + 2.5f, hc.y - 10.5f), rtip (hc.x + 16.0f + flick, hc.y - 26.0f), rb2 (hc.x + 10.5f, hc.y - 3.0f);
    const Path earL = earPath (lb1, ltip, lb2), earR = earPath (rb1, rtip, rb2);
    auto innerEar = [&] (Point<float> b1, Point<float> tip, Point<float> b2)
    {
        const auto c = (b1 + b2) * 0.5f;
        return earPath (b1 + (c - b1) * 0.3f, tip + (c - tip) * 0.18f, b2 + (c - b2) * 0.3f);
    };

    // tail: cream, from the rump, curling forward along the ground and up the cat's side
    const float sway = std::sin (t * 1.3f) * 2.0f + std::sin (t * 0.47f) * 1.2f + mood.flick * 5.0f * std::sin (t * 40.0f);
    Path tail;
    tail.startNewSubPath (31.0f, -7.0f);
    tail.cubicTo (22.0f, 0.0f, 2.0f, 0.5f, -1.0f, -9.0f);
    tail.cubicTo (-3.0f, -17.0f, 4.0f, -26.0f, 13.0f + sway * 0.5f, -28.0f);
    tail.quadraticTo (19.0f + sway, -29.0f, 22.0f + sway, -25.0f);

    // ---- draw: outlines, then fills back to front ----
    for (const Path* p : std::initializer_list<const Path*> { &farLeg, &torso, &haunch, &hindPaw, &nearLeg, &paws, &neck })
        outline (*p);
    fill (farLeg, 0.45f);
    fill (torso);
    fill (haunch, 0.12f);
    fill (hindPaw);
    fill (nearLeg);
    fill (paws);
    fill (neck);

    // wrinkles: the neck folds, chest, shoulder and rump lines
    g.setColour (skinLo.withAlpha (0.6f));
    for (int i = 0; i < 6; ++i)
    {
        Path w;
        const float y = -80.0f + (float) i * 3.2f;
        const float x0 = -19.5f - (float) i * 0.3f;
        w.startNewSubPath (x0 + 1.5f, y);
        w.quadraticTo (-11.0f, y + 2.0f + (float) (i % 2), -4.5f + (float) i * 0.8f - (float) (i % 2) * 2.0f, y - 0.4f);
        g.strokePath (w, PathStrokeType (0.75f));
    }
    for (int i = 0; i < 4; ++i)
    {
        Path w;
        const float y = -58.0f + (float) i * 4.0f;
        w.startNewSubPath (-20.0f, y);
        w.quadraticTo (-15.0f, y + 2.0f, -11.0f, y + 0.5f);
        g.strokePath (w, PathStrokeType (0.7f));
    }
    for (int i = 0; i < 4; ++i)
    {
        Path w;
        const float x = 10.0f + (float) i * 5.5f;
        w.startNewSubPath (x, -38.0f + (float) i * 1.5f);
        w.quadraticTo (x - 3.0f, -24.0f, x + 1.0f, -10.0f);
        g.strokePath (w, PathStrokeType (0.7f));
    }

    // tail on top (it wraps around the near side)
    g.setColour (line.withAlpha (0.8f));
    g.strokePath (tail, PathStrokeType (5.6f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (tailHi, 0.0f, -28.0f, tailLo, 30.0f, -2.0f, false));
    g.strokePath (tail, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // toes
    g.setColour (skinLo);
    for (float x : { -22.5f, -19.5f, -13.5f, -10.5f })
        g.drawLine (x, -3.0f, x, 0.8f, 0.6f);

    // ---- the head: tilts as one piece (ears, face) ----
    Graphics::ScopedSaveState headState (g);
    g.addTransform (AffineTransform::rotation (mood.tilt, hc.x, hc.y + 9.0f));
    // in S&H mode a soft psychedelic aura breathes around the head
    const Colour moodColour = Colour::fromHSV (std::fmod (mood.hue + theme::huePhase() * 0.05f, 1.0f), 0.75f, 1.0f, 1.0f);
    if (mood.alert > 0.01f)
    {
        g.setGradientFill (ColourGradient (moodColour.withAlpha (0.35f * mood.alert), hc.x, hc.y, moodColour.withAlpha (0.0f), hc.x + 26.0f, hc.y, true));
        g.fillEllipse (hc.x - 26.0f, hc.y - 26.0f, 52.0f, 52.0f);
    }
    outline (earL); outline (earR); outline (head);
    fill (earL); fill (earR);
    g.setColour (pink.withAlpha (0.8f));
    g.fillPath (innerEar (lb1, ltip, lb2));
    g.fillPath (innerEar (rb1, rtip, rb2));
    fill (head);

    // face: almond pale-blue eyes, a light nose bridge, small pink nose
    g.setColour (skinHi.withAlpha (0.8f));
    g.fillEllipse (hc.x - 1.6f, hc.y - 3.0f, 3.2f, 9.0f);
    for (float side : { -1.0f, 1.0f })
    {
        const Point<float> ec (hc.x + side * 5.0f, hc.y - 1.5f);
        Path eye;
        eye.startNewSubPath (ec.x - 3.6f * side, ec.y + 1.0f);                 // inner corner, a little lower
        eye.quadraticTo (ec.x, ec.y - 3.8f, ec.x + 3.6f * side, ec.y - 1.0f);   // outer corner, lifted
        eye.quadraticTo (ec.x, ec.y + 3.6f, ec.x - 3.6f * side, ec.y + 1.0f);
        eye.closeSubPath();
        g.setColour (Colour (0xff2a2228));
        g.strokePath (eye, PathStrokeType (1.1f));
        const Colour irisHi = Colour (0xffbfe2ff).interpolatedWith (moodColour.brighter (0.4f), mood.alert);
        const Colour irisLo = Colour (0xff4f8fd0).interpolatedWith (moodColour, mood.alert);
        g.setGradientFill (ColourGradient (irisHi, ec.x, ec.y - 2.5f, irisLo, ec.x, ec.y + 2.5f, false));
        g.fillPath (eye);
        const float pupilW = 1.5f + 1.6f * mood.alert;   // wide pupils: hunting mode
        g.setColour (Colour (0xff10141c));
        g.fillEllipse (ec.x - pupilW * 0.5f + mood.look * 1.3f, ec.y - 2.1f, pupilW, 4.0f);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillEllipse (ec.x - 1.6f, ec.y - 1.9f, 1.0f, 1.0f);
        if (catBlink > 0.001f)
        {
            Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (eye);
            const float lid = jlimit (0.0f, 1.0f, catBlink);
            g.setColour (skin);
            g.fillRect (ec.x - 4.5f, ec.y - 4.5f, 9.0f, 8.5f * lid);
            g.setColour (Colour (0xff2a2228));
            g.fillRect (ec.x - 4.5f, ec.y - 4.5f + 8.5f * lid - 0.5f, 9.0f, 0.9f);
        }
    }
    if (mood.alert > 0.01f)   // the eyes shine
        for (float side : { -1.0f, 1.0f })
        {
            const Point<float> ec (hc.x + side * 5.0f, hc.y - 1.5f);
            g.setGradientFill (ColourGradient (moodColour.withAlpha (0.55f * mood.alert * (1.0f - catBlink)), ec.x, ec.y,
                                               moodColour.withAlpha (0.0f), ec.x + 7.0f, ec.y, true));
            g.fillEllipse (ec.x - 7.0f, ec.y - 7.0f, 14.0f, 14.0f);
        }
    Path nose;
    nose.addTriangle (hc.x - 1.8f, hc.y + 4.2f, hc.x + 1.8f, hc.y + 4.2f, hc.x, hc.y + 6.3f);
    g.setColour (pink.darker (0.1f));
    g.fillPath (nose);
    Path mouth;
    mouth.startNewSubPath (hc.x, hc.y + 6.3f); mouth.lineTo (hc.x, hc.y + 7.4f);
    mouth.startNewSubPath (hc.x - 2.2f, hc.y + 7.4f); mouth.quadraticTo (hc.x - 1.1f, hc.y + 8.6f, hc.x, hc.y + 7.4f);
    mouth.quadraticTo (hc.x + 1.1f, hc.y + 8.6f, hc.x + 2.2f, hc.y + 7.4f);
    g.setColour (line);
    g.strokePath (mouth, PathStrokeType (0.6f));
}

inline void scene (juce::Graphics& g, float glow, Background bg, float blink = 0.0f, float time = 0.0f, float catBlink = 0.0f, const CatMood& mood = {})
{
    if (bg == Background::space)      space (g, { 0.0f, 0.0f, kWidth, kHeight });
    else if (bg == Background::black) g.fillAll (juce::Colours::black);
    capstone (g, glow, bg != Background::none, blink);
    for (int i = 0; i < (int) std::size (kTiers); ++i)
        tier (g, kTiers[i], i);
    sphynxCat (g, time, catBlink, mood);
}
} // namespace rsq::pyramid
