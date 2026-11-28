#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace rsq;

RSquelchProcessor::RSquelchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "R-Squelch", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            paramCache[withId->paramID.toStdString()] = apvts.getRawParameterValue (withId->paramID);
    for (int i = 0; i < 16; ++i)
        gateStepParams[(size_t) i] = apvts.getRawParameterValue (ids::gateStep (i));
}

bool RSquelchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RSquelchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    using OS = juce::dsp::Oversampling<float>;
    for (int i = 0; i < 2; ++i)
    {
        oversamplers[i] = std::make_unique<OS> (2, (size_t) (i + 1), OS::filterHalfBandPolyphaseIIR, true, false);
        oversamplers[i]->initProcessing ((size_t) samplesPerBlock);
    }
    currentOsIndex = -1;
    setOversamplingIndex ((int) raw (ids::oversampling)->load());
    engine.setParams (readParams());
    engine.snapSmoothers();
    fx.prepare ((float) sampleRate);
}

void RSquelchProcessor::setOversamplingIndex (int index)
{
    index = juce::jlimit (0, 2, index);
    if (index == currentOsIndex) return;
    currentOsIndex = index;
    const int factor = 1 << index;
    engine.prepare ((float) (hostSampleRate * factor));
    engine.setParams (readParams());
    engine.snapSmoothers();
    if (index > 0) oversamplers[index - 1]->reset();
    setLatencySamples (index > 0 ? (int) std::round (oversamplers[index - 1]->getLatencyInSamples()) : 0);
}

EngineParams RSquelchProcessor::readParams() const
{
    EngineParams p;
    auto f = [this] (const char* id) { return raw (id)->load(); };
    auto i = [this] (const char* id) { return (int) std::round (raw (id)->load()); };

    p.voiceMode = 0;   // R-Squelch is always mono (classic psy squelch line); Poly is kept in the engine for later
    p.legato = f (ids::legato) > 0.5f;
    p.glideTime = f (ids::glideTime);
    p.glideMode = i (ids::glideMode);
    p.bendRange = f (ids::bendRange);
    p.velToFilter = f (ids::velToFilter);
    p.velToAmp = f (ids::velToAmp);
    p.accentThreshold = f (ids::accentThreshold);
    p.accentAmount = f (ids::accentAmount);

    // waveform: SAW (the old morph knob still works here, for automation), SQUARE, or ACID (303 saw)
    const int wf = i (ids::waveform);
    p.morph = wf == 1 ? 1.0f : (wf == 0 ? f (ids::morph) : 0.0f);
    p.acid = wf == 2;
    p.pulseWidth = f (ids::pulseWidth);
    p.octave = i (ids::octave);
    p.semitone = i (ids::semitone);
    p.fineCents = f (ids::fine);
    p.subLevel = f (ids::subLevel);
    p.unison = i (ids::unison);
    p.detune = f (ids::detune);
    p.spread = f (ids::spread);
    p.drive = f (ids::drive);
    p.sync = f (ids::sync);
    p.oscCharacter = i (ids::oscCharacter);
    p.fm = f (ids::fm);
    p.fmRatio = (float) (1 + i (ids::fmRatio));

    p.cutoffHz = f (ids::cutoff);
    p.resonance = f (ids::resonance);
    p.width = f (ids::width);
    p.filterDrive = f (ids::filterDrive);
    p.slope = i (ids::slope);
    p.growl = f (ids::growl);
    p.combOn = f (ids::combOn) > 0.5f;
    p.combPitch = f (ids::combPitch);
    p.combFeedback = f (ids::combFeedback);
    p.combMotion = f (ids::combMotion);
    p.combDamp = f (ids::combDamp);
    p.combMix = f (ids::combMix);
    p.combMode = i (ids::combMode);
    p.combLfoHz = (float) currentBpm / 60.0f / ids::motionDivBeats (i (ids::combRate));
    p.combDepth = f (ids::combDepth);
    p.combDrive = f (ids::combDrive);
    p.combSpread = f (ids::combSpread);
    p.fold = f (ids::fold);
    p.keyTrack = f (ids::keyTrack);

    p.fA = f (ids::fAttack); p.fD = f (ids::fDecay); p.fS = f (ids::fSustain); p.fR = f (ids::fRelease);
    p.fEnvAmount = f (ids::fEnvAmount);
    p.fEnvCurve = f (ids::fEnvCurve);
    p.aA = f (ids::aAttack); p.aD = f (ids::aDecay); p.aS = f (ids::aSustain); p.aR = f (ids::aRelease);

    p.pEnvAmount = f (ids::pEnvAmount);
    p.pA = f (ids::pAttack);
    p.pD = f (ids::pDecay);
    p.pEnvCurve = f (ids::pEnvCurve);
    p.filterEnvToPitch = f (ids::fEnvToPitch);
    p.motionRate = f (ids::motionRate);
    p.filterRange = f (ids::filterRange);
    p.pitchRange = f (ids::pitchRange);
    p.pitchLinked = i (ids::pitchLink) == 0;
    p.pitchRate = f (ids::pitchRate);
    p.pitchShape = i (ids::pitchShape);
    // tempo sync: rates follow the host BPM (cycle = note division); Hz mode keeps the free knobs
    if (i (ids::motionSync) == 1)
    {
        const float bps = (float) currentBpm / 60.0f;
        p.motionRate = bps / ids::motionDivBeats (i (ids::motionDiv));
        p.pitchRate  = bps / ids::motionDivBeats (i (ids::pitchDiv));
    }
    p.filterShape = i (ids::filterShape);
    p.randomSeed = i (ids::randomSeed);
    p.motionMode = i (ids::motionMode);

    p.outputGain = juce::Decibels::decibelsToGain (f (ids::outputGain), -36.0f);
    return p;
}

void RSquelchProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

// Sample-accurate MIDI: render up to each event, then apply it.
void RSquelchProcessor::renderWithMidi (float* l, float* r, int numSamples, const juce::MidiBuffer& midi, int factor)
{
    int pos = 0;
    for (const auto meta : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, meta.samplePosition * factor);
        if (eventPos > pos)
        {
            engine.render (l + pos, r + pos, eventPos - pos);
            pos = eventPos;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < numSamples)
        engine.render (l + pos, r + pos, numSamples - pos);
}

void RSquelchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // ---- tempo for the motion sync, gate and delay (falls back to 145 BPM when the host gives none) ----
    double bpm = 145.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    currentBpm = juce::jlimit (20.0, 400.0, bpm);

    setOversamplingIndex ((int) raw (ids::oversampling)->load());
    engine.setParams (readParams());

    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);   // merge MIDI from the standalone's virtual keyboard input
    fx.setTransport (bpm, playing, ppq);
    engine.setTransport (bpm, playing, ppq);   // BAR SYNC motion
    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn()) { fx.restartClock(); break; }   // gate step 1 lands on the note
    fx.setParams (readFxParams());

    // ---- synth (optionally oversampled) ----
    if (currentOsIndex == 0)
    {
        renderWithMidi (buffer.getWritePointer (0), buffer.getWritePointer (1), n, midi, 1);
    }
    else
    {
        auto& os = *oversamplers[currentOsIndex - 1];
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = os.processSamplesUp (block);       // gives us a block at the higher rate
        const int factor = 1 << currentOsIndex;
        renderWithMidi (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), midi, factor);
        os.processSamplesDown (block);
    }

    // ---- effects at the host rate ----
    fx.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

FxParams RSquelchProcessor::readFxParams() const
{
    FxParams p;
    auto f = [this] (const char* id) { return raw (id)->load(); };
    auto on = [this] (const char* id) { return raw (id)->load() > 0.5f; };
    auto i = [this] (const char* id) { return (int) std::round (raw (id)->load()); };
    p.tubeDrive = f (ids::tubeDrive);
    p.tubeType = i (ids::tubeType);
    p.formantLfoDepth = f (ids::formantLfo);
    p.formantLfoRate = i (ids::formantLfoRate);
    p.formantOn = on (ids::formantOn);  p.vowel = f (ids::vowel);  p.formantMix = f (ids::formantMix);
    p.crushOn = on (ids::crushOn);      p.bits = f (ids::bits);    p.downsample = f (ids::downsample);  p.crushMix = f (ids::crushMix);
    p.gateOn = on (ids::gateOn);        p.gateRate = i (ids::gateRate);
    p.gateLength = f (ids::gateLength); p.gateDepth = f (ids::gateDepth);
    for (size_t k = 0; k < 16; ++k) p.gateSteps[k] = gateStepParams[k]->load() > 0.5f;
    p.delayOn = on (ids::delayOn);      p.delayTime = i (ids::delayTime); p.delayFeel = i (ids::delayFeel);
    p.delayFeedback = f (ids::delayFeedback); p.delayMix = f (ids::delayMix);
    p.reverbOn = on (ids::reverbOn);    p.reverbSize = f (ids::reverbSize);
    p.reverbTone = f (ids::reverbTone); p.reverbMix = f (ids::reverbMix);
    p.phaserOn = on (ids::phaserOn); p.phaserRate = i (ids::phaserRate); p.phaserDepth = f (ids::phaserDepth);
    p.phaserFeedback = f (ids::phaserFeedback); p.phaserMix = f (ids::phaserMix);
    return p;
}

juce::AudioProcessorEditor* RSquelchProcessor::createEditor() { return new RSquelchEditor (*this); }

void RSquelchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RSquelchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RSquelchProcessor(); }
