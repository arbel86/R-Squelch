#pragma once
// R-Squelch — visual displays: interactive band-pass filter (Serum/Vital style), oscillator wave,
// pitch envelope and amp envelope.

#include <juce_audio_processors/juce_audio_processors.h>
#include <complex>
#include "Theme.h"
#include "Hardware.h"
#include "BinaryData.h"
#include "../Parameters.h"
#include "../DSP/BandPassFilter.h"
#include "../DSP/SquelchEngine.h"

namespace rsq
{
// Base: rounded dark screen that repaints only when one of its parameters changes.
class DisplayBase : public juce::Component, protected juce::Timer
{
public:
    DisplayBase (juce::AudioProcessorValueTreeState& s, std::initializer_list<const char*> watchIds) : state (s)
    {
        for (auto* id : watchIds) watched.push_back (s.getRawParameterValue (id));
        cache.resize (watched.size(), -1.0e9f);
        startTimerHz (30);
    }

protected:
    float get (const char* id) const { return state.getRawParameterValue (id)->load(); }

    void timerCallback() override
    {
        bool changed = false;
        for (size_t i = 0; i < watched.size(); ++i)
        {
            const float v = watched[i]->load();
            if (std::abs (v - cache[i]) > 1.0e-7f) { cache[i] = v; changed = true; }
        }
        if (changed || animating()) repaint();
    }

    virtual bool animating() { return false; }

    // glass screen in a metal bezel (the glass reflection is added on top in paintOverChildren)
    void paintScreen (juce::Graphics& g, juce::Rectangle<float> r) { hw::screen (g, r); }

public:
    void paintOverChildren (juce::Graphics& g) override { hw::glass (g, getLocalBounds().toFloat().reduced (3.0f)); }

protected:

    void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> area, float width, float alpha = 1.0f, float hue = 0.0f)
    {
        g.setGradientFill (theme::iridescent (area.getTopLeft(), area.getTopRight(), alpha * 0.18f, hue));
        g.strokePath (p, juce::PathStrokeType (width * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (theme::iridescent (area.getTopLeft(), area.getTopRight(), alpha, hue));
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::AudioProcessorValueTreeState& state;

private:
    std::vector<std::atomic<float>*> watched;
    std::vector<float> cache;
};

//==============================================================================
class FilterDisplay final : public DisplayBase
{
public:
    FilterDisplay (juce::AudioProcessorValueTreeState& s, const SquelchEngine& e)
        : DisplayBase (s, { ids::cutoff, ids::resonance, ids::width, ids::slope, ids::filterDrive }), engine (e)
    {
        cutoffParam = s.getParameter (ids::cutoff);
        resoParam = s.getParameter (ids::resonance);
        widthParam = s.getParameter (ids::width);
        stopTimer();
        startTimerHz (30);
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        plot = r.reduced (14.0f, 16.0f);

        g.saveState();
        {
            Path clip;   // keep the moon's halo inside the rounded screen
            clip.addRoundedRectangle (r.reduced (1.0f), 8.0f);
            g.reduceClipRegion (clip);
        }

        // moon in the background: faint, with a soft halo that glows brighter while a note plays
        {
            const juce::Image moon = juce::ImageCache::getFromMemory (BinaryData::moon_png, BinaryData::moon_pngSize);   // not static (host freeze on unload)
            if (moon.isValid())
            {
                const float d = r.getHeight() * 0.78f;
                const auto c = juce::Point<float> (r.getRight() - d * 0.62f, r.getY() + d * 0.56f);
                const float glow = 0.10f + 0.22f * juce::jmin (1.0f, liveAmp);
                juce::ColourGradient halo (theme::shifted (theme::violet).withAlpha (glow), c.x, c.y,
                                           theme::shifted (theme::violet).withAlpha (0.0f), c.x + d * 0.85f, c.y, true);
                halo.addColour (0.55, theme::shifted (theme::magenta).withAlpha (glow * 0.35f));
                g.setGradientFill (halo);
                g.fillEllipse (juce::Rectangle<float> (d * 1.7f, d * 1.7f).withCentre (c));
                g.setOpacity (0.30f + 0.08f * juce::jmin (1.0f, liveAmp));
                g.drawImage (moon, juce::Rectangle<float> (d, d).withCentre (c), juce::RectanglePlacement::centred);
                g.setOpacity (1.0f);
            }
        }

        // grid
        g.setFont (theme::font (10.0f));
        for (int fi : { 50, 100, 200, 500, 1000, 2000, 5000, 10000 })
        {
            const float f = (float) fi;
            const float x = freqToX (f);
            g.setColour (theme::grid);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            if (fi == 100 || fi == 1000 || fi == 10000)
            {
                g.setColour (theme::textDim.withAlpha (0.6f));
                g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), (int) x + 4, (int) plot.getBottom() - 14, 30, 12, Justification::left);
            }
        }
        g.setColour (theme::grid.brighter (0.15f));
        g.drawHorizontalLine ((int) dbToY (0.0f), plot.getX(), plot.getRight());

        const float cutoff = get (ids::cutoff), reso = get (ids::resonance), width = get (ids::width);
        const int slope = (int) get (ids::slope);

        // trails: afterimages of the moving filter — the psychedelic touch
        for (int i = 0; i < kTrail; ++i)
        {
            const int idx = (trailHead - 1 - i + kTrail) % kTrail;
            if (trail[idx].amp <= 0.002f) continue;
            const float a = trail[idx].amp * (1.0f - (float) i / kTrail) * 0.12f;   // faint, so they never read as extra peaks
            strokeGlow (g, responsePath (trail[idx].cutoff, reso, width, slope, false), plot, 1.0f, a, 0.04f * (float) i);
        }

        // ONE solid curve: while a note plays it is the live (moving) filter; otherwise your setting.
        // Your setting stays visible only as a faint thin outline while playing, so it never reads as a second peak.
        const bool live = liveAmp > 0.01f && liveCutoff > 0.0f;
        const float mainCutoff = live ? liveCutoff : cutoff;
        if (live)
        {
            g.setColour (theme::textDim.withAlpha (0.35f));
            g.strokePath (responsePath (cutoff, reso, width, slope, false), PathStrokeType (1.0f));
        }
        auto fill = responsePath (mainCutoff, reso, width, slope, true);
        g.setGradientFill (ColourGradient (theme::shifted (theme::violet).withAlpha (0.28f), 0.0f, plot.getY(),
                                           theme::shifted (theme::cyan).withAlpha (0.0f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        strokeGlow (g, responsePath (mainCutoff, reso, width, slope, false), plot, 2.2f, 1.0f);

        // node: x = cutoff, y = resonance
        const auto node = nodePosition (cutoff, reso);
        const bool hot = dragging || hover;
        g.setColour (theme::text.withAlpha (hot ? 0.18f : 0.08f));
        g.fillEllipse (Rectangle<float> (26.0f, 26.0f).withCentre (node));
        g.setColour (theme::background);
        g.fillEllipse (Rectangle<float> (13.0f, 13.0f).withCentre (node));
        g.setColour (theme::text);
        g.drawEllipse (Rectangle<float> (13.0f, 13.0f).withCentre (node), 2.0f);

        // read-out
        g.setFont (theme::labelFont (10.5f));
        g.setColour (hot ? theme::text : theme::textDim);
        const String readout = (cutoff < 1000 ? String (cutoff, 0) + " HZ" : String (cutoff / 1000.0f, 2) + " KHZ")
                             + "   RES " + String (roundToInt (reso * 100)) + "%   WIDTH " + String (roundToInt (width * 100)) + "%";
        g.drawText (readout, r.reduced (14.0f, 10.0f).removeFromTop (14.0f), Justification::left, false);
        g.setColour (theme::textDim.withAlpha (0.5f));
        g.drawText ("DRAG  /  WHEEL = WIDTH", r.reduced (14.0f, 10.0f).removeFromTop (14.0f), Justification::right, false);

        g.restoreState();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool h = e.position.getDistanceFrom (nodePosition (get (ids::cutoff), get (ids::resonance))) < 18.0f;
        if (h != hover) { hover = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        cutoffParam->beginChangeGesture();
        resoParam->beginChangeGesture();
        setFromMouse (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e.position); }
    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        cutoffParam->endChangeGesture();
        resoParam->endChangeGesture();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (auto* p : { cutoffParam, resoParam, widthParam })
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        widthParam->beginChangeGesture();
        widthParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, widthParam->getValue() + w.deltaY * 0.25f));
        widthParam->endChangeGesture();
    }

private:
    static constexpr int kTrail = 14;
    struct TrailPoint { float cutoff = 0.0f, amp = 0.0f; };

    bool animating() override
    {
        liveCutoff = engine.uiCutoffHz.load (std::memory_order_relaxed);
        liveAmp = engine.uiAmp.load (std::memory_order_relaxed);
        trail[trailHead] = { liveCutoff, liveAmp };
        trailHead = (trailHead + 1) % kTrail;
        bool any = liveAmp > 0.002f;
        for (auto& t : trail) any = any || t.amp > 0.002f;
        if (! any) return false;
        for (auto& t : trail) t.amp *= 0.93f;
        return true;
    }

    void setFromMouse (juce::Point<float> p)
    {
        const float x = juce::jlimit (plot.getX(), plot.getRight(), p.x);
        const float y = juce::jlimit (plot.getY(), plot.getBottom(), p.y);
        const float freq = 20.0f * std::pow (1000.0f, (x - plot.getX()) / plot.getWidth());
        const float reso = 1.0f - (y - plot.getY()) / plot.getHeight();
        cutoffParam->setValueNotifyingHost (cutoffParam->convertTo0to1 (freq));
        resoParam->setValueNotifyingHost (resoParam->convertTo0to1 (reso));
    }

    juce::Point<float> nodePosition (float cutoff, float reso) const
    {
        return { freqToX (cutoff), plot.getY() + (1.0f - reso) * plot.getHeight() };
    }

    float freqToX (float f) const { return plot.getX() + plot.getWidth() * std::log (juce::jmax (20.0f, f) / 20.0f) / std::log (1000.0f); }
    float dbToY (float db) const
    {
        constexpr float top = 30.0f, bottom = -48.0f;
        return plot.getY() + plot.getHeight() * (top - juce::jlimit (bottom, top, db)) / (top - bottom);
    }

    // Analog prototype of the DSP filter (same k / width mapping as BandPassFilter).
    static float magnitudeDb (float f, float cutoff, float reso, float width, int slope)
    {
        using C = std::complex<double>;
        const double k = BandPassFilter::resonanceToK (reso);
        double mag;
        if (slope == 0)
        {
            const C s (0.0, f / cutoff);
            const double kk = k * (0.25 + width * 1.75);
            mag = std::abs (s / (s * s + kk * s + 1.0)) * juce::jlimit (0.35, 1.0, 0.35 + kk);
        }
        else if (slope == 3)
        {
            // 303: 4-pole ladder low-pass (partly compensated) under a high-pass set by WIDTH — same as the DSP
            const double k4 = 4.0 * BandPassFilter::acidK (reso);
            const C s (0.0, f / cutoff), one (1.0, 0.0);
            const C g = one / (one + s);
            const C g4 = g * g * g * g;
            const C sh (0.0, f / (20.0 * std::exp2 (width * 4.0)));
            mag = std::abs ((1.0 + 0.2 * k4) * g4 / (one + k4 * g4) * (sh / (one + sh))) * BandPassFilter::kAcidGain;
        }
        else if (slope == 2)
        {
            // SH-5 style: HP -> 4-pole ladder LP (resonant) -> band-pass, same structure as the DSP
            const double sp = BandPassFilter::widthToSpread (width) * 1.5;
            const double kl = BandPassFilter::sh5LadderK (reso);
            const double kb = 2.0 - 1.3 * juce::jlimit (0.0f, 1.0f, reso);
            const C s1 (0.0, f / (cutoff / sp)), s2 (0.0, f / cutoff), s3 (0.0, f / cutoff);
            const C hp = (s1 * s1) / (s1 * s1 + 1.3 * s1 + 1.0);
            const C one (1.0, 0.0);
            const C lp = 1.0 / ((one + s2) * (one + s2) * (one + s2) * (one + s2) + kl);
            const C bp = kb * s3 / (s3 * s3 + kb * s3 + 1.0);
            mag = std::abs (hp * lp * bp) * BandPassFilter::compSh5 ((float) kl);
        }
        else
        {
            // 24 dB: two identical band-passes in series, centred on the cutoff
            const double kk = BandPassFilter::k24 (reso, width);
            const C s (0.0, f / cutoff);
            const C h = kk * s / (s * s + kk * s + 1.0);
            mag = std::abs (h * h) * 1.6 * BandPassFilter::comp24 ((float) kk);
        }
        return (float) (20.0 * std::log10 (juce::jmax (1.0e-6, mag)));
    }

    juce::Path responsePath (float cutoff, float reso, float width, int slope, bool closed) const
    {
        juce::Path p;
        const int n = juce::jmax (64, (int) plot.getWidth() / 2);
        for (int i = 0; i <= n; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) n;
            const float f = 20.0f * std::pow (1000.0f, (float) i / (float) n);
            const float y = dbToY (magnitudeDb (f, cutoff, reso, width, slope));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed)
        {
            p.lineTo (plot.getRight(), plot.getBottom() + 20.0f);
            p.lineTo (plot.getX(), plot.getBottom() + 20.0f);
            p.closeSubPath();
        }
        return p;
    }

    const SquelchEngine& engine;
    juce::RangedAudioParameter *cutoffParam, *resoParam, *widthParam;
    juce::Rectangle<float> plot;
    TrailPoint trail[kTrail];
    int trailHead = 0;
    float liveCutoff = 0.0f, liveAmp = 0.0f;
    bool dragging = false, hover = false;
};

//==============================================================================
class OscDisplay final : public DisplayBase
{
public:
    explicit OscDisplay (juce::AudioProcessorValueTreeState& s)
        : DisplayBase (s, { ids::morph, ids::waveform, ids::subLevel, ids::drive, ids::pulseWidth, ids::sync, ids::fm, ids::fmRatio }) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        auto plot = r.reduced (12.0f, 12.0f);
        const int wf = (int) get (ids::waveform);
        const bool acid = wf == 2;
        const float morph = wf == 1 ? 1.0f : (wf == 0 ? get (ids::morph) : 0.0f);
        const float sub = get (ids::subLevel), drive = get (ids::drive), pw = get (ids::pulseWidth);
        const float ratio = 1.0f + 7.0f * get (ids::sync);
        const float fmAmt = get (ids::fm), fmRatio = 1.0f + std::round (get (ids::fmRatio));
        const float dg = 1.0f + drive * 5.0f, norm = 1.0f / fastTanh (dg);

        g.setColour (theme::grid);
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

        juce::Path p;
        const int n = (int) plot.getWidth();
        for (int i = 0; i <= n; ++i)
        {
            const float t = 2.0f * (float) i / (float) n;            // two cycles
            const float mph = t - std::floor (t);                      // master phase
            float ph = mph * ratio;                                    // synced slave: restarts every master cycle
            ph -= std::floor (ph);
            const float saw = acid ? 2.0f * (1.0f - std::exp (-2.2f * ph)) / (1.0f - std::exp (-2.2f)) - 1.0f - 0.34f
                                   : 2.0f * ph - 1.0f;
            const float sq = (ph < pw ? 1.0f : -1.0f) - (2.0f * pw - 1.0f);
            const float subW = (std::fmod (t * 0.5f, 1.0f) < 0.5f ? 1.0f : -1.0f);
            const float fmTone = std::sin (kTwoPi * mph + (1.0f + 7.0f * fmAmt) * std::sin (kTwoPi * mph * fmRatio));
            float v = (saw + morph * (sq - saw)) * (1.0f - fmAmt) + fmTone * fmAmt * 1.3f + sub * subW;
            v = fastTanh (v * dg) * norm / (1.0f + sub * 0.6f);
            const float y = plot.getCentreY() - v * plot.getHeight() * 0.42f;
            if (i == 0) p.startNewSubPath (plot.getX(), y); else p.lineTo (plot.getX() + (float) i, y);
        }
        strokeGlow (g, p, plot, 1.8f);
    }
};

//==============================================================================
// MOTION: the repeating triangle movement (filter: left -> right -> left, pitch: up -> down).
// Two cycles are drawn; a white dot follows the live position while a note plays.
class MotionDisplay final : public DisplayBase
{
public:
    enum class Target { filter, pitch };

    MotionDisplay (juce::AudioProcessorValueTreeState& s, const SquelchEngine& e, Target t)
        : DisplayBase (s, { ids::motionRate, ids::filterRange, ids::pitchRange, ids::pitchLink, ids::pitchRate, ids::pitchShape, ids::filterShape,
                            ids::motionSync, ids::motionDiv, ids::pitchDiv, ids::randomSeed }),
          engine (e), target (t)
    {
        stopTimer();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        auto plot = r.reduced (12.0f, 10.0f);
        plot.removeFromTop (14.0f);

        const bool isFilter = target == Target::filter;
        const bool linked = (int) get (ids::pitchLink) == 0;
        const float rate = (isFilter || linked) ? get (ids::motionRate) : get (ids::pitchRate);
        const float range = isFilter ? get (ids::filterRange) : get (ids::pitchRange);
        const float norm = jlimit (0.0f, 1.0f, std::abs (range) / (isFilter ? 8.0f : 12.0f));
        const int shape = (int) get (isFilter ? ids::filterShape : ids::pitchShape);
        // FALL dives below the note: flip the drawing so it always fits the screen
        const bool down = (range < 0.0f) != (! isFilter && shape == motion::fall);
        const float baseY = down ? plot.getY() : plot.getBottom();
        const float dir = down ? 1.0f : -1.0f;

        g.setColour (theme::grid);
        g.drawHorizontalLine ((int) baseY, plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());

        auto valueAt = [&] (float cycles)
        {
            const float c = jmax (0.0f, cycles);
            const int seed = (int) get (ids::randomSeed);   // RANDOM / S&H: draw the real pattern of this seed
            if (isFilter) return motion::filterShapeValue (shape, c, (uint32_t) c, motion::filterSeedFor (seed));   // most start at the top
            return std::abs (motion::shapeValue (shape, c, (uint32_t) c, motion::pitchSeedFor (seed)));
        };
        auto pointAt = [&] (float cycles)
        {
            return Point<float> (plot.getX() + plot.getWidth() * cycles / 2.0f,
                                 baseY + dir * valueAt (cycles) * jmax (0.04f, norm) * plot.getHeight());
        };
        Path p;
        p.startNewSubPath (pointAt (0.0f));
        for (int i = 1; i <= 400; ++i) p.lineTo (pointAt ((float) i / 200.0f));
        strokeGlow (g, p, plot, 1.8f, norm > 0.001f ? 1.0f : 0.35f, isFilter ? 0.0f : 0.25f);

        if (livePhase >= 0.0f && liveAmp > 0.01f)
            for (int k = 0; k < 2; ++k)
            {
                g.setColour (theme::text.withAlpha (k == 0 ? 1.0f : 0.3f));
                g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (pointAt (livePhase + (float) k)));
            }

        auto top = r.reduced (12.0f, 8.0f).removeFromTop (12.0f);
        g.setFont (theme::labelFont (9.5f));
        g.setColour (isFilter ? theme::shifted (theme::magenta) : theme::cyan);
        g.drawText ((range > 0 ? "+" : "") + String (range, 1) + (isFilter ? " OCT" : " ST"), top, Justification::left, false);
        g.setColour (theme::textDim);
        const float secs = 1.0f / jmax (0.001f, rate);
        const bool synced = (int) get (ids::motionSync) == 1;
        const String cycle = synced ? ids::motionDivNames()[(int) get ((isFilter || linked) ? ids::motionDiv : ids::pitchDiv)].toUpperCase() + " / CYCLE"
                                    : String (secs, secs < 10.0f ? 1 : 0) + " S / CYCLE";
        g.drawText ((! isFilter && linked ? "LINKED  " : "") + cycle, top, Justification::right, false);
    }

private:
    bool animating() override
    {
        const float ph = (target == Target::filter ? engine.uiFilterPhase : engine.uiPitchPhase).load (std::memory_order_relaxed);
        const float amp = engine.uiAmp.load (std::memory_order_relaxed);
        const bool changed = std::abs (ph - livePhase) > 1.0e-4f || std::abs (amp - liveAmp) > 1.0e-3f;
        livePhase = ph; liveAmp = amp;
        return changed;
    }

    const SquelchEngine& engine;
    Target target;
    float livePhase = -1.0f, liveAmp = 0.0f;
};

//==============================================================================
class AmpDisplay final : public DisplayBase
{
public:
    explicit AmpDisplay (juce::AudioProcessorValueTreeState& s)
        : DisplayBase (s, { ids::aAttack, ids::aDecay, ids::aSustain, ids::aRelease }) {}

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        paintScreen (g, r);
        auto plot = r.reduced (12.0f, 12.0f);
        const float a = get (ids::aAttack), d = get (ids::aDecay), s = get (ids::aSustain), rel = get (ids::aRelease);
        const float hold = 0.25f;
        const float total = jmax (0.05f, a + d + hold + rel);
        auto X = [&] (float t) { return plot.getX() + plot.getWidth() * t / total; };
        auto Y = [&] (float v) { return plot.getBottom() - v * plot.getHeight(); };
        const float expo = curveToExponent (0.45f);

        Path p;
        p.startNewSubPath (X (0), Y (0));
        p.lineTo (X (a), Y (1));
        for (int i = 1; i <= 40; ++i)
        {
            const float u = (float) i / 40.0f;
            p.lineTo (X (a + d * u), Y (s + (1 - s) * std::pow (1 - u, expo)));
        }
        p.lineTo (X (a + d + hold), Y (s));
        for (int i = 1; i <= 40; ++i)
        {
            const float u = (float) i / 40.0f;
            p.lineTo (X (a + d + hold + rel * u), Y (s * std::pow (1 - u, expo)));
        }

        Path fill (p);
        fill.lineTo (X (total), Y (0));
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (theme::shifted (theme::magenta).withAlpha (0.22f), 0.0f, plot.getY(),
                                           theme::shifted (theme::violet).withAlpha (0.0f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        strokeGlow (g, p, plot, 1.8f);
    }
};
} // namespace rsq
