#pragma once
// R-Squelch — main editor: a clean black & gold panel.
//   header:  RBell logo | R-SQUELCH ........ presets ........ the Sphynx cat
//   row 1:   OSC | FILTER (big interactive display)
//   row 2:   AMP | FILTER MOTION | PITCH MOTION | MASTER
//   row 3:   FORMANT | CRUSH | GATE | DELAY
//   row 4:   REVERB | PHASER | COMB
// Everything is drawn at a fixed design size and scaled as a whole, so it stays sharp.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI/RSquelchLookAndFeel.h"
#include "UI/Controls.h"
#include "UI/Displays.h"
#include "UI/FxModules.h"
#include "UI/Lightning.h"
#include "UI/Hardware.h"
#include "UI/Pyramid.h"
#include "UI/PresetBar.h"

class RSquelchEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RSquelchEditor (RSquelchProcessor&);
    ~RSquelchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1240, designHeight = 1028;          // the whole panel

private:
    class Content;
    void timerCallback() override;

    rsq::RSquelchLookAndFeel lnf;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RSquelchEditor)
};
