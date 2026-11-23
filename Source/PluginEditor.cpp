#include "PluginEditor.h"
#include "BinaryData.h"

using namespace rsq;

namespace
{
juce::Image makeLogo (juce::Colour colour)
{
    // The RBell logo (black on transparent), recoloured — dark for the engraving, light for its edge.
    auto img = juce::ImageCache::getFromMemory (BinaryData::rbell_logo_png, BinaryData::rbell_logo_pngSize);
    if (! img.isValid()) return {};
    const int h = 120;   // pre-scale once (source is ~3400 px wide); headroom for HiDPI / 200% zoom
    img = img.convertedToFormat (juce::Image::ARGB)
             .rescaled (juce::roundToInt ((float) img.getWidth() * h / (float) img.getHeight()), h, juce::Graphics::highResamplingQuality);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
        for (int x = 0; x < bd.width; ++x)
            bd.setPixelColour (x, y, colour.withAlpha (bd.getPixelColour (x, y).getAlpha() / 255.0f * colour.getFloatAlpha()));
    return img;
}

struct PanelInfo { juce::Rectangle<int> bounds; juce::String title; };

} // namespace

//==============================================================================
class RSquelchEditor::Content final : public juce::Component
{
public:
    explicit Content (RSquelchProcessor& p)
        : apvts (p.apvts), engine (p.getEngine()),
          filterDisplay (apvts, p.getEngine()), oscDisplay (apvts), filterMotionDisplay (apvts, p.getEngine(), MotionDisplay::Target::filter),
          pitchMotionDisplay (apvts, p.getEngine(), MotionDisplay::Target::pitch), ampDisplay (apvts),
          // OSC
          wave (apvts, ids::waveform, { "Saw", "Square", "Acid" }), character (apvts, ids::oscCharacter, { "Warm", "Bright" }), sync (apvts, ids::sync, "Sync"), fm (apvts, ids::fm, "FM"), sub (apvts, ids::subLevel, "Sub"),
          drive (apvts, ids::drive, "Drive"), octave (apvts, ids::octave, "Oct", true),
          unison (apvts, ids::unison, "Unison"), detune (apvts, ids::detune, "Detune"),
          // FILTER
          cutoff (apvts, ids::cutoff, "Cutoff"), reso (apvts, ids::resonance, "Reso"),
          width (apvts, ids::width, "Width"), envAmount (apvts, ids::fEnvAmount, "Env Amt", true),
          envDecay (apvts, ids::fDecay, "Decay"), growl (apvts, ids::growl, "Growl"), fold (apvts, ids::fold, "Fold"),
          slope (apvts, ids::slope, { "12 dB", "24 dB", "SH-5", "303" }),
          glide (apvts, ids::glideTime, "Glide"),
          // MOTION
          filterRate (apvts, ids::motionRate, "Rate"), filterDiv (apvts, ids::motionDiv, "Rate"),
          motionSync (apvts, ids::motionSync, { "Hz", "Sync" }), filterRange (apvts, ids::filterRange, "Range", true),
          motionMode (apvts, ids::motionMode, "Mode"),
          pitchLink (apvts, ids::pitchLink, { "Linked", "Free" }),
          pitchShape (apvts, ids::pitchShape, "LFO"),
          filterShape (apvts, ids::filterShape, "LFO"),
          filterDice (apvts, ids::randomSeed), pitchDice (apvts, ids::randomSeed),
          pitchRate (apvts, ids::pitchRate, "Rate"), pitchDiv (apvts, ids::pitchDiv, "Rate"), pitchRange (apvts, ids::pitchRange, "Range", true),
          // AMP
          ampDecay (apvts, ids::aDecay, "Decay"), ampSustain (apvts, ids::aSustain, "Sustain"),
          ampRelease (apvts, ids::aRelease, "Release"),
          // HEADER
          quality (apvts, ids::oversampling, { "1x", "2x", "4x" }),
          output (apvts, ids::outputGain, "Output"),
          tube (apvts, ids::tubeDrive, "Drive"),
          distType (apvts, ids::tubeType, { "Tube", "Fuzz", "Square" }),
          presetBar (p.presets)
    {
        logo = makeLogo (theme::goldText);
        // the RBell logo is a link to RBell on SoundCloud
        logoLink.setTooltip ("RBell on SoundCloud");
        logoLink.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        logoLink.onClick = [] { juce::URL ("https://soundcloud.com/rbellmusic").launchInDefaultBrowser(); };
        addAndMakeVisible (logoLink);
        logoEtch = makeLogo (juce::Colours::black.withAlpha (0.8f));

        for (auto* c : std::initializer_list<juce::Component*> {
                 &filterDisplay, &oscDisplay, &filterMotionDisplay, &pitchMotionDisplay, &ampDisplay,
                 &filterRate, &filterRange, &pitchLink, &pitchShape, &filterShape, &filterDice, &pitchDice, &pitchRate, &pitchRange,
                 &wave, &character, &sync, &fm, &sub, &drive, &octave, &unison, &detune,
                 &cutoff, &reso, &width, &envAmount, &envDecay, &growl, &fold, &slope,
                 &filterDiv, &pitchDiv, &motionSync, &motionMode,
                 &glide,
                 &ampDecay, &ampSustain, &ampRelease,
                 &quality, &output, &tube, &distType, &presetBar })
            addAndMakeVisible (c);

        // ---- effects row ----
        auto knob = [this] (const char* id, const char* name) { return std::make_unique<Knob> (apvts, id, name); };
        auto knobs = [] (auto... k) { std::vector<std::unique_ptr<Knob>> v; (v.push_back (std::move (k)), ...); return v; };
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Formant", ids::formantOn,
            knobs (knob (ids::vowel, "Vowel"), knob (ids::formantLfo, "LFO"), knob (ids::formantLfoRate, "Rate"), knob (ids::formantMix, "Mix"))));
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Crush", ids::crushOn,
            knobs (knob (ids::bits, "Bits"), knob (ids::downsample, "Rate"), knob (ids::crushMix, "Mix"))));
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Gate", ids::gateOn,
            knobs (knob (ids::gateRate, "Rate"), knob (ids::gateLength, "Length"), knob (ids::gateDepth, "Depth")),
            std::make_unique<GateGrid> (apvts, p.getFx())));
        fxModules.back()->setTitleWidget (std::make_unique<GatePatternPicker> (apvts));
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Delay", ids::delayOn,
            knobs (knob (ids::delayTime, "Time"), knob (ids::delayFeedback, "Repeats"), knob (ids::delayMix, "Mix"))));
        fxModules.back()->setTitleWidget (std::make_unique<Segmented> (apvts, ids::delayFeel, juce::StringArray { "Norm", "Dot", "Trip" }));
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Reverb", ids::reverbOn,
            knobs (knob (ids::reverbSize, "Size"), knob (ids::reverbTone, "Tone"), knob (ids::reverbMix, "Mix"))));
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Phaser", ids::phaserOn,
            knobs (knob (ids::phaserRate, "Rate"), knob (ids::phaserDepth, "Depth"),
                   std::make_unique<Knob> (apvts, ids::phaserFeedback, "Feedback", true), knob (ids::phaserMix, "Mix"))));
        fxModules.push_back (std::make_unique<FxModule> (apvts, "Flange", ids::combOn,
            knobs (std::make_unique<Knob> (apvts, ids::combPitch, "Freq", true), knob (ids::combFeedback, "Res"),
                   knob (ids::combRate, "Rate"), knob (ids::combDepth, "Depth"), knob (ids::combDrive, "Destroy"),
                   knob (ids::combSpread, "Spread"), knob (ids::combDamp, "Damp"), knob (ids::combMix, "Mix"))));
        fxModules.back()->setTitleWidget (std::make_unique<Segmented> (apvts, ids::combMode, juce::StringArray { "+", "-", "+/-" }));
        for (auto& m : fxModules) addAndMakeVisible (*m);

        // psychedelic lightning overlay (on top of everything, ignores the mouse)
        lightning = std::make_unique<LightningOverlay> (apvts, p.getEngine());


        addAndMakeVisible (*lightning);   // added last, so it draws above all panels
        setOpaque (true);
        setSize (designWidth, designHeight);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        // ---------- black anodised faceplate with a gold edge ----------
        g.fillAll (Colours::black);
        hw::chassis (g, getLocalBounds().toFloat());

        // ---------- header: RBell logo | R-SQUELCH (alien glyphs) ...... presets ...... the cat ----------
        if (logo.isValid())
        {
            const auto lr = logoArea.toFloat();
            g.drawImage (logoEtch, lr.translated (0.0f, 1.0f), RectanglePlacement::centred);
            g.drawImage (logo, lr, RectanglePlacement::centred);
            g.setColour (theme::gold.withAlpha (0.45f));
            g.fillRect (lr.getRight() + 16.0f, lr.getY() + 2.0f, 1.0f, lr.getHeight() - 4.0f);
        }
        pyramid::title (g, "R-SQUELCH", titleArea.toFloat());

        // gold hairline under the header, fading at the ends
        {
            const float y = (float) kHeader;
            ColourGradient line (theme::gold.withAlpha (0.0f), 14.0f, y, theme::gold.withAlpha (0.0f), (float) getWidth() - 14.0f, y, false);
            line.addColour (0.15, theme::gold.withAlpha (0.55f));
            line.addColour (0.85, theme::gold.withAlpha (0.55f));
            g.setGradientFill (line);
            g.fillRect (14.0f, y, (float) getWidth() - 28.0f, 1.0f);
        }

        // ---------- recessed modules ----------
        for (auto& panel : panels)
            hw::module (g, panel.bounds.toFloat(), panel.title);

        // MASTER: label for the quality switch
        g.setFont (theme::labelFont (9.5f, true));
        hw::engravedText (g, "QUALITY", quality.getBounds().translated (0, -18).withHeight (14).toFloat(), Justification::centred, hw::inkSoft);
        hw::engravedText (g, "DISTORTION", distType.getBounds().translated (0, -18).withHeight (14).toFloat(), Justification::centred, hw::inkSoft);

        // the Sphynx cat, sitting on the header line (it watches the filter)
        pyramid::sphynxCat (g, (float) (Time::getMillisecondCounterHiRes() * 0.001), catBlink, catMood, catGround(), kCatScale);
    }

    void resized() override
    {
        using R = juce::Rectangle<int>;
        panels.clear();
        const int W = getWidth();

        // header
        logoArea = R (44, 16, 100, 30);
        logoLink.setBounds (logoArea.expanded (4));
        titleArea = R (176, 18, 270, 27);
        presetBar.setBounds (R (0, 18, 320, 26).withCentre ({ W / 2, 31 }));

        // rows: OSC | FILTER   /   AMP | FILTER MOTION | PITCH MOTION | MASTER   /   five effects
        const int x0 = kMargin, x1 = W - kMargin, cw = (x1 - x0 - 3 * kGap) / 4;
        const int y2 = kHeader + kGap, h2 = 340;
        const int y3 = y2 + h2 + kGap, h3 = 244;
        const int y4 = y3 + h3 + kGap, h4 = (designHeight - kMargin - y4 - kGap) / 2;
        const int y5 = y4 + h4 + kGap, h5 = designHeight - kMargin - y5;

        R osc (x0, y2, cw, h2);
        R filter (x0 + cw + kGap, y2, x1 - (x0 + cw + kGap), h2);
        R amp (x0, y3, cw, h3);
        R fMot (x0 + (cw + kGap), y3, cw, h3);
        R pMot (x0 + 2 * (cw + kGap), y3, cw, h3);
        R master (x0 + 3 * (cw + kGap), y3, x1 - (x0 + 3 * (cw + kGap)), h3);

        panels = { { osc, "OSC" }, { filter, "FILTER" }, { amp, "AMP" }, { fMot, "FILTER MOTION" }, { pMot, "PITCH MOTION" }, { master, "MASTER" } };

        auto knobRow = [] (R row, std::initializer_list<juce::Component*> knobs, int maxW)
        {
            const int w = row.getWidth() / (int) knobs.size();
            for (auto* k : knobs) k->setBounds (row.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w, maxW), row.getHeight()));
        };

        // OSC: wave view on top, two rows of knobs
        {
            auto r = osc.reduced (12);
            r.removeFromTop (26);
            auto row2 = r.removeFromBottom (kKnobH);
            r.removeFromBottom (4);
            auto row1 = r.removeFromBottom (kKnobH);
            r.removeFromBottom (8);
            oscDisplay.setBounds (r);
            knobRow (row1, { &sync, &fm, &sub }, 80);
            knobRow (row2, { &drive, &octave, &unison, &detune }, 80);
            character.setBounds (oscDisplay.getBounds().reduced (8).removeFromTop (18).removeFromLeft (112));   // sits on top of the wave view
            wave.setBounds (titleSlot (osc, 156));
        }

        // FILTER: big display + the squelch knobs
        {
            auto r = filter.reduced (12);
            r.removeFromTop (26);
            slope.setBounds (titleSlot (filter, 236));
            r.removeFromTop (4);
            auto kr = r.removeFromBottom (kKnobH + 6);
            r.removeFromBottom (10);
            filterDisplay.setBounds (r);
            knobRow (kr, { &cutoff, &reso, &width, &envAmount, &envDecay, &growl, &fold }, 92);
        }

        // AMP
        {
            auto r = amp.reduced (12);
            r.removeFromTop (26);
            auto kr = r.removeFromBottom (kKnobH);
            r.removeFromBottom (8);
            ampDisplay.setBounds (r);
            knobRow (kr, { &ampDecay, &ampSustain, &ampRelease }, 78);
        }

        // FILTER / PITCH MOTION: LFO picker, display, knobs
        auto layoutWithPicker = [&] (R panel, PresetSelector& picker, juce::Component& display, std::initializer_list<juce::Component*> knobs)
        {
            auto r = panel.reduced (12);
            r.removeFromTop (26);
            picker.setBounds (r.removeFromTop (22));
            r.removeFromTop (6);
            auto kr = r.removeFromBottom (kKnobH);
            r.removeFromBottom (8);
            display.setBounds (r);
            knobRow (kr, knobs, 78);
        };
        layoutWithPicker (fMot, filterShape, filterMotionDisplay, { &filterRate, &filterRange, &motionMode });
        layoutWithPicker (pMot, pitchShape, pitchMotionDisplay, { &pitchRate, &pitchRange, &glide });
        filterPickerFull = filterShape.getBounds();
        pitchPickerFull = pitchShape.getBounds();
        diceShown = -1;   // re-place the pickers / dice on the next animation tick
        filterDiv.setBounds (filterRate.getBounds());   // synced rate sits in the same place; one of the two is shown
        pitchDiv.setBounds (pitchRate.getBounds());
        motionSync.setBounds (titleSlot (fMot, 96));
        pitchLink.setBounds (titleSlot (pMot, 112));

        // MASTER: quality switch, tube + output knobs
        {
            auto r = master.reduced (12);
            r.removeFromTop (26);
            knobRow (r.removeFromBottom (kKnobH), { &tube, &output }, 92);
            // two labelled switches stacked above the knobs: QUALITY, then the DISTORTION type
            auto top = r.removeFromTop (r.getHeight() / 2);
            quality.setBounds (top.withTrimmedTop (16).withSizeKeepingCentre (160, 22));
            distType.setBounds (r.withTrimmedTop (16).withSizeKeepingCentre (196, 22));
        }

        // effects, two rows on the same 4-column grid:
        //   FORMANT | CRUSH | GATE | DELAY
        //   REVERB  | PHASER | COMB (double width: the extreme one)
        {
            auto col = [&] (int c) { return x0 + c * (cw + kGap); };
            auto colW = [&] (int c, int span) { return (c + span >= 4 ? x1 : col (c + span) - kGap) - col (c); };
            for (int i = 0; i < 4; ++i)
                fxModules[(size_t) i]->setBounds (col (i), y4, colW (i, 1), h4);
            fxModules[4]->setBounds (col (0), y5, colW (0, 1), h5);
            fxModules[5]->setBounds (col (1), y5, colW (1, 1), h5);
            fxModules[6]->setBounds (col (2), y5, colW (2, 2), h5);
        }

        if (lightning) lightning->setBounds (getLocalBounds());
    }

    void animate()
    {
        // the cat blinks slowly now and then
        const double now = juce::Time::getMillisecondCounterHiRes();
        auto blinkCurve = [] (double ms, double closeMs, double holdMs, double openMs)
        {
            if (ms < 0.0) return 0.0f;
            if (ms < closeMs) return (float) (ms / closeMs);
            if (ms < closeMs + holdMs) return 1.0f;
            if (ms < closeMs + holdMs + openMs) return 1.0f - (float) ((ms - closeMs - holdMs) / openMs);
            return 0.0f;
        };
        if (now >= nextCatBlink)
        {
            catBlinkStart = now;
            nextCatBlink = now + 6000.0 + rnd.nextFloat() * 7000.0;
        }
        catBlink = blinkCurve (now - catBlinkStart, 350.0, 500.0, 450.0);   // slow, sleepy "cat kiss"
        // the cat watches the filter like a laser dot in every preset: its head and eyes follow the filter
        // position, and its ears and tail flick when the filter jumps. In S&H RANDOM it gets excited:
        // wide glowing pupils, and the eye colour changes with every random step.
        {
            const bool sh = (int) apvts.getRawParameterValue (ids::filterShape)->load() == motion::random;
            catMood.alert += ((sh ? 1.0f : 0.0f) - catMood.alert) * 0.08f;
            const float mv = engine.uiFilterMove.load (std::memory_order_relaxed);
            const bool playing = mv >= 0.0f && engine.uiAmp.load (std::memory_order_relaxed) > 0.005f;
            if (playing && lastFilterMove >= 0.0f && std::abs (mv - lastFilterMove) > 0.06f)
            {
                stepTime = now;     // a jump (S&H, JUMP, STEPS, the snap-back of RISE / FALL)
                catMood.hue = mv;
            }
            lastFilterMove = playing ? mv : -1.0f;
            targetTilt = playing ? (mv - 0.5f) * 0.55f : 0.0f;
            catMood.tilt += (targetTilt - catMood.tilt) * 0.3f;
            catMood.look += ((playing ? (mv - 0.5f) * 2.0f : 0.0f) - catMood.look) * 0.3f;
            catMood.flick = (float) std::exp (-(now - stepTime) / 160.0);
        }
        repaint (pyramid::catBoundsAt (catGround(), kCatScale));

        // RANDOM / S&H selected: the dice button appears next to the LFO picker
        {
            const bool fRnd = (int) apvts.getRawParameterValue (ids::filterShape)->load() == motion::random;
            const bool pRnd = (int) apvts.getRawParameterValue (ids::pitchShape)->load() == motion::random;
            const int state = (fRnd ? 1 : 0) | (pRnd ? 2 : 0);
            if (state != diceShown)
            {
                diceShown = state;
                auto place = [] (PresetSelector& picker, DiceButton& dice, juce::Rectangle<int> full, bool show)
                {
                    dice.setVisible (show);
                    picker.setBounds (show ? full.withTrimmedRight (28) : full);
                    dice.setBounds (full.removeFromRight (24).withSizeKeepingCentre (22, 22));
                };
                place (filterShape, filterDice, filterPickerFull, fRnd);
                place (pitchShape, pitchDice, pitchPickerFull, pRnd);
            }
        }

        // Hz / Sync: show the free rate knob or the note-division knob
        const bool synced = (int) apvts.getRawParameterValue (ids::motionSync)->load() == 1;
        if (syncShown != (int) synced)
        {
            syncShown = (int) synced;
            filterDiv.setVisible (synced);  filterRate.setVisible (! synced);
            pitchDiv.setVisible (synced);   pitchRate.setVisible (! synced);
        }
        // pitch RATE only matters in FREE mode — dim it while LINKED
        const bool free = apvts.getRawParameterValue (ids::pitchLink)->load() > 0.5f;
        if (pitchRate.isEnabled() != free || pitchDiv.isEnabled() != free)
        {
            for (auto* k : { &pitchRate, &pitchDiv })
            {
                k->setEnabled (free);
                k->setAlpha (free ? 1.0f : 0.3f);
            }
        }
    }

private:
    static constexpr int kGap = 12, kKnobH = 86, kMargin = 14, kHeader = 62;
    static constexpr float kCatScale = 0.47f;
    juce::Point<float> catGround() const { return { (float) getWidth() - 96.0f, (float) kHeader - 1.0f }; }
    juce::Rectangle<int> titleArea;

    juce::AudioProcessorValueTreeState& apvts;
    const SquelchEngine& engine;
    juce::Image logo, logoEtch;
    juce::Rectangle<int> logoArea;
    float catBlink = 0.0f;
    double nextCatBlink = 4000.0, catBlinkStart = -1.0e9;
    juce::Random rnd;
    pyramid::CatMood catMood;
    float lastFilterMove = -1.0f, targetTilt = 0.0f;
    double stepTime = -1.0e9;
    int syncShown = -1;

    // controls that live in a module's title strip (right-aligned, vertically centred in the strip)
    static juce::Rectangle<int> titleSlot (juce::Rectangle<int> panel, int width)
    {
        return { panel.getRight() - 8 - width, panel.getY() + 4, width, 22 };
    }
    std::vector<PanelInfo> panels;

    FilterDisplay filterDisplay;
    OscDisplay oscDisplay;
    MotionDisplay filterMotionDisplay, pitchMotionDisplay;
    AmpDisplay ampDisplay;

    Segmented wave, character;
    Knob sync, fm, sub, drive, octave, unison, detune;
    Knob cutoff, reso, width, envAmount, envDecay, growl, fold;
    Segmented slope;
    Knob glide;
    Knob filterRate, filterDiv;
    Segmented motionSync;
    Knob filterRange, motionMode;
    Segmented pitchLink;
    PresetSelector pitchShape, filterShape;
    DiceButton filterDice, pitchDice;
    juce::Rectangle<int> filterPickerFull, pitchPickerFull;
    int diceShown = -1;
    Knob pitchRate, pitchDiv, pitchRange;
    Knob ampDecay, ampSustain, ampRelease;
    Segmented quality;
    Knob output, tube;
    Segmented distType;
    PresetBar presetBar;
    // invisible button over the logo (the logo itself is painted by the panel)
    struct LogoLink final : juce::Button
    {
        LogoLink() : juce::Button ("RBell") {}
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            if (over) { g.setColour (theme::gold.withAlpha (0.10f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f); }
        }
    } logoLink;
    std::vector<std::unique_ptr<FxModule>> fxModules;
    std::unique_ptr<LightningOverlay> lightning;
};

//==============================================================================
RSquelchEditor::RSquelchEditor (RSquelchProcessor& p) : AudioProcessorEditor (p)
{
    setLookAndFeel (&lnf);
    content = std::make_unique<Content> (p);
    setOpaque (true);
    addAndMakeVisible (*content);

    // the whole panel scales together; it opens at 75 % so it fits laptop screens
    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth * 3 / 4, designHeight * 3 / 4);
    startTimerHz (30);
}

RSquelchEditor::~RSquelchEditor()
{
    stopTimer();
    content.reset();
    setLookAndFeel (nullptr);
}

void RSquelchEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }

void RSquelchEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, designWidth, designHeight);
}

void RSquelchEditor::timerCallback()
{
    theme::huePhase() += 0.012f;
    content->animate();
}
