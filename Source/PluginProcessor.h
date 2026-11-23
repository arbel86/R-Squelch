#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSP/SquelchEngine.h"
#include "DSP/Effects.h"
#include "Presets.h"
#include <unordered_map>
#include <string>

class RSquelchProcessor final : public juce::AudioProcessor
{
public:
    RSquelchProcessor();
    ~RSquelchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "R-Squelch"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }   // delay + reverb tails

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rsq::PresetManager presets { apvts };
    juce::MidiKeyboardState keyboardState;          // on-screen keyboard in the editor
    const rsq::SquelchEngine& getEngine() const { return engine; }
    const rsq::FxChain& getFx() const { return fx; }

private:
    rsq::EngineParams readParams() const;
    rsq::FxParams readFxParams() const;
    void handleMidi (const juce::MidiMessage& m);
    void renderWithMidi (float* l, float* r, int numSamples, const juce::MidiBuffer& midi, int factor);
    void setOversamplingIndex (int index);

    rsq::SquelchEngine engine;
    rsq::FxChain fx;
    std::array<std::atomic<float>*, 16> gateStepParams {};
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2];   // [0] = 2x, [1] = 4x
    int currentOsIndex = -1;
    double hostSampleRate = 44100.0;
    double currentBpm = 145.0;

    // Raw parameter pointers cached once (looking them up by name in the audio thread would allocate).
    std::unordered_map<std::string, std::atomic<float>*> paramCache;
    std::atomic<float>* raw (const char* id) const { return paramCache.at (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RSquelchProcessor)
};
