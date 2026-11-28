#pragma once
// R-Squelch — parameter IDs and layout (all automatable through AudioProcessorValueTreeState).

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/EngineParams.h"
#include "DSP/Effects.h"

namespace rsq::ids
{
#define RSQ_ID(name) inline constexpr const char* name = #name;
RSQ_ID (voiceMode) RSQ_ID (legato) RSQ_ID (glideTime) RSQ_ID (glideMode) RSQ_ID (bendRange)
RSQ_ID (velToFilter) RSQ_ID (velToAmp) RSQ_ID (accentThreshold) RSQ_ID (accentAmount)
RSQ_ID (morph) RSQ_ID (pulseWidth) RSQ_ID (octave) RSQ_ID (semitone) RSQ_ID (fine) RSQ_ID (subLevel)
RSQ_ID (unison) RSQ_ID (detune) RSQ_ID (spread) RSQ_ID (drive) RSQ_ID (sync) RSQ_ID (oscCharacter) RSQ_ID (fm) RSQ_ID (fmRatio)
RSQ_ID (cutoff) RSQ_ID (resonance) RSQ_ID (width) RSQ_ID (filterDrive) RSQ_ID (slope) RSQ_ID (keyTrack)
RSQ_ID (fAttack) RSQ_ID (fDecay) RSQ_ID (fSustain) RSQ_ID (fRelease) RSQ_ID (fEnvAmount) RSQ_ID (fEnvCurve)
RSQ_ID (aAttack) RSQ_ID (aDecay) RSQ_ID (aSustain) RSQ_ID (aRelease)
RSQ_ID (pEnvAmount) RSQ_ID (pAttack) RSQ_ID (pDecay) RSQ_ID (pEnvCurve) RSQ_ID (fEnvToPitch)
RSQ_ID (motionRate) RSQ_ID (filterRange) RSQ_ID (pitchRange) RSQ_ID (pitchLink) RSQ_ID (pitchRate) RSQ_ID (pitchShape) RSQ_ID (waveform) RSQ_ID (filterShape)
RSQ_ID (motionSync) RSQ_ID (motionDiv) RSQ_ID (pitchDiv) RSQ_ID (growl) RSQ_ID (fold) RSQ_ID (randomSeed) RSQ_ID (motionMode)
RSQ_ID (phaserOn) RSQ_ID (phaserRate) RSQ_ID (phaserDepth) RSQ_ID (phaserFeedback) RSQ_ID (phaserMix)
RSQ_ID (combOn) RSQ_ID (combPitch) RSQ_ID (combFeedback) RSQ_ID (combMotion) RSQ_ID (combDamp) RSQ_ID (combMix)
RSQ_ID (combMode) RSQ_ID (combRate) RSQ_ID (combDepth) RSQ_ID (combDrive) RSQ_ID (combSpread) RSQ_ID (delayFeel)
RSQ_ID (oversampling) RSQ_ID (outputGain)
RSQ_ID (tubeDrive) RSQ_ID (tubeType)
RSQ_ID (formantOn) RSQ_ID (vowel) RSQ_ID (formantMix) RSQ_ID (formantLfo) RSQ_ID (formantLfoRate)
RSQ_ID (crushOn) RSQ_ID (bits) RSQ_ID (downsample) RSQ_ID (crushMix)
RSQ_ID (gateOn) RSQ_ID (gateRate) RSQ_ID (gateLength) RSQ_ID (gateDepth)
RSQ_ID (delayOn) RSQ_ID (delayTime) RSQ_ID (delayFeedback) RSQ_ID (delayMix)
RSQ_ID (reverbOn) RSQ_ID (reverbSize) RSQ_ID (reverbTone) RSQ_ID (reverbMix)
#undef RSQ_ID
// tempo-synced motion: cycle lengths, in beats (quarter notes)
inline const juce::StringArray& motionDivNames()
{
    static const juce::StringArray n { "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar", "2 bars", "4 bars", "8 bars" };
    return n;
}
inline float motionDivBeats (int i)
{
    static const float b[] { 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
    return b[juce::jlimit (0, 9, i)];
}

// gate steps: "gateStep1" .. "gateStep16"
inline juce::String gateStep (int index0) { return "gateStep" + juce::String (index0 + 1); }
} // namespace rsq::ids

namespace rsq
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using APF = AudioParameterFloat;
    using APC = AudioParameterChoice;
    using APB = AudioParameterBool;
    using API = AudioParameterInt;
    const EngineParams d;   // defaults

    auto pid = [] (const char* id) { return ParameterID { id, 1 }; };

    auto range = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    auto msAttr = AudioParameterFloatAttributes().withLabel ("ms")
        .withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String (v * 1000.0f, 0) + " ms" : String (v, 2) + " s"; });
    auto pctAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto stAttr = AudioParameterFloatAttributes().withLabel ("st")
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " st"; });

    auto timeRange = [&] (float maxSec, float centreSec) { return range (0.0f, maxSec, centreSec); };

    AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Voice ----
    auto voice = std::make_unique<AudioProcessorParameterGroup> ("voice", "VOICE", "|");
    voice->addChild (std::make_unique<APC> (pid (ids::voiceMode), "Voice Mode (unused, always mono)", StringArray { "Mono", "Poly" }, d.voiceMode));
    voice->addChild (std::make_unique<APB> (pid (ids::legato), "Legato", d.legato));
    voice->addChild (std::make_unique<APF> (pid (ids::glideTime), "Glide", timeRange (0.5f, 0.08f), d.glideTime, msAttr));
    voice->addChild (std::make_unique<APC> (pid (ids::glideMode), "Glide Mode", StringArray { "Always", "Legato Only" }, d.glideMode));
    voice->addChild (std::make_unique<API> (pid (ids::bendRange), "Pitch Bend Range", 1, 24, (int) d.bendRange));
    voice->addChild (std::make_unique<APF> (pid (ids::velToFilter), "Velocity > Filter", NormalisableRange<float> (0, 1), d.velToFilter, pctAttr));
    voice->addChild (std::make_unique<APF> (pid (ids::velToAmp), "Velocity > Amp", NormalisableRange<float> (0, 1), d.velToAmp, pctAttr));
    voice->addChild (std::make_unique<APF> (pid (ids::accentThreshold), "Accent Threshold", NormalisableRange<float> (0, 1), d.accentThreshold,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "vel " + String (roundToInt (v * 127.0f)); })));
    voice->addChild (std::make_unique<APF> (pid (ids::accentAmount), "Accent Amount", NormalisableRange<float> (0, 1), d.accentAmount, pctAttr));

    // ---- Oscillator ----
    auto osc = std::make_unique<AudioProcessorParameterGroup> ("osc", "OSCILLATOR", "|");
    osc->addChild (std::make_unique<APC> (pid (ids::waveform), "Waveform", StringArray { "Saw", "Square", "Acid" }, 0));
    osc->addChild (std::make_unique<APF> (pid (ids::morph), "Wave Morph (Saw > Square)", NormalisableRange<float> (0, 1), d.morph, pctAttr));
    osc->addChild (std::make_unique<APF> (pid (ids::pulseWidth), "Pulse Width", NormalisableRange<float> (0.05f, 0.95f), d.pulseWidth, pctAttr));
    osc->addChild (std::make_unique<API> (pid (ids::octave), "Octave", -3, 3, d.octave));
    osc->addChild (std::make_unique<API> (pid (ids::semitone), "Semitone", -12, 12, d.semitone));
    osc->addChild (std::make_unique<APF> (pid (ids::fine), "Fine", NormalisableRange<float> (-100, 100), d.fineCents,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ct"; })));
    osc->addChild (std::make_unique<APF> (pid (ids::subLevel), "Sub Level", NormalisableRange<float> (0, 1), d.subLevel, pctAttr));
    osc->addChild (std::make_unique<API> (pid (ids::unison), "Unison Voices", 1, 7, d.unison));
    osc->addChild (std::make_unique<APF> (pid (ids::detune), "Unison Detune", NormalisableRange<float> (0, 1), d.detune, pctAttr));
    osc->addChild (std::make_unique<APF> (pid (ids::spread), "Stereo Spread", NormalisableRange<float> (0, 1), d.spread, pctAttr));
    osc->addChild (std::make_unique<APF> (pid (ids::drive), "Drive", NormalisableRange<float> (0, 1), d.drive, pctAttr));
    osc->addChild (std::make_unique<APC> (pid (ids::oscCharacter), "Osc Character", StringArray { "Warm", "Bright" }, d.oscCharacter));
    osc->addChild (std::make_unique<APF> (pid (ids::fm), "FM", NormalisableRange<float> (0, 1), d.fm,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : String (roundToInt (v * 100.0f)) + " %"; })));
    osc->addChild (std::make_unique<APC> (pid (ids::fmRatio), "FM Ratio", StringArray { "1:1", "2:1", "3:1" }, 0));
    osc->addChild (std::make_unique<APF> (pid (ids::sync), "Sync", NormalisableRange<float> (0, 1), d.sync,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : "x" + String (1.0f + 7.0f * v, 2); })));
    osc->addChild (std::make_unique<APC> (pid (ids::oversampling), "Oversampling", StringArray { "Off (PolyBLEP)", "2x", "4x" }, 0));

    // ---- Filter ----
    auto flt = std::make_unique<AudioProcessorParameterGroup> ("filter", "BAND-PASS FILTER", "|");
    flt->addChild (std::make_unique<APF> (pid (ids::cutoff), "Cutoff", range (20.0f, 20000.0f, 800.0f), d.cutoffHz,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1000 ? String (v, 0) + " Hz" : String (v / 1000, 2) + " kHz"; })));
    flt->addChild (std::make_unique<APF> (pid (ids::resonance), "Resonance", NormalisableRange<float> (0, 1), d.resonance, pctAttr));
    flt->addChild (std::make_unique<APF> (pid (ids::width), "Width", NormalisableRange<float> (0, 1), d.width, pctAttr));
    flt->addChild (std::make_unique<APF> (pid (ids::filterDrive), "Filter Drive", NormalisableRange<float> (0, 1), d.filterDrive, pctAttr));
    flt->addChild (std::make_unique<APC> (pid (ids::slope), "Filter Type", StringArray { "12 dB", "24 dB", "SH-5", "303" }, d.slope));
    flt->addChild (std::make_unique<APF> (pid (ids::growl), "Growl (Filter FM)", NormalisableRange<float> (0, 1), d.growl,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : String (roundToInt (v * 100.0f)) + " %"; })));
    flt->addChild (std::make_unique<APF> (pid (ids::fold), "Fold (Wavefolder)", NormalisableRange<float> (0, 1), d.fold,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : String (roundToInt (v * 100.0f)) + " %"; })));
    flt->addChild (std::make_unique<APF> (pid (ids::keyTrack), "Key Tracking", NormalisableRange<float> (0, 1), d.keyTrack, pctAttr));

    auto fenv = std::make_unique<AudioProcessorParameterGroup> ("fenv", "FILTER ENVELOPE", "|");
    fenv->addChild (std::make_unique<APF> (pid (ids::fEnvAmount), "Env Amount", NormalisableRange<float> (-1, 1), d.fEnvAmount, pctAttr));
    fenv->addChild (std::make_unique<APF> (pid (ids::fAttack), "Attack", timeRange (2.0f, 0.05f), d.fA, msAttr));
    fenv->addChild (std::make_unique<APF> (pid (ids::fDecay), "Decay", timeRange (4.0f, 0.3f), d.fD, msAttr));
    fenv->addChild (std::make_unique<APF> (pid (ids::fSustain), "Sustain", NormalisableRange<float> (0, 1), d.fS, pctAttr));
    fenv->addChild (std::make_unique<APF> (pid (ids::fRelease), "Release", timeRange (4.0f, 0.3f), d.fR, msAttr));
    fenv->addChild (std::make_unique<APF> (pid (ids::fEnvCurve), "Curve (Lin > Exp)", NormalisableRange<float> (0, 1), d.fEnvCurve, pctAttr));

    auto aenv = std::make_unique<AudioProcessorParameterGroup> ("aenv", "AMP ENVELOPE", "|");
    aenv->addChild (std::make_unique<APF> (pid (ids::aAttack), "Attack", timeRange (2.0f, 0.05f), d.aA, msAttr));
    aenv->addChild (std::make_unique<APF> (pid (ids::aDecay), "Decay", timeRange (4.0f, 0.3f), d.aD, msAttr));
    aenv->addChild (std::make_unique<APF> (pid (ids::aSustain), "Sustain", NormalisableRange<float> (0, 1), d.aS, pctAttr));
    aenv->addChild (std::make_unique<APF> (pid (ids::aRelease), "Release", timeRange (4.0f, 0.3f), d.aR, msAttr));

    auto pitch = std::make_unique<AudioProcessorParameterGroup> ("pitch", "PITCH MODULATION", "|");
    pitch->addChild (std::make_unique<APF> (pid (ids::pEnvAmount), "Pitch Env Amount", NormalisableRange<float> (-48, 48), d.pEnvAmount, stAttr));
    pitch->addChild (std::make_unique<APF> (pid (ids::pAttack), "Pitch Env Attack", timeRange (1.0f, 0.03f), d.pA, msAttr));
    pitch->addChild (std::make_unique<APF> (pid (ids::pDecay), "Pitch Env Decay", timeRange (2.0f, 0.1f), d.pD, msAttr));
    pitch->addChild (std::make_unique<APF> (pid (ids::pEnvCurve), "Pitch Env Curve", NormalisableRange<float> (0, 1), d.pEnvCurve, pctAttr));
    pitch->addChild (std::make_unique<APF> (pid (ids::fEnvToPitch), "Filter Env > Pitch", NormalisableRange<float> (-24, 24), d.filterEnvToPitch, stAttr));

    auto rateAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const float secs = 1.0f / jmax (0.001f, v);   // show the cycle length too — easier to feel
        return String (v, v < 1.0f ? 2 : 1) + " Hz  (" + String (secs, secs < 10.0f ? 1 : 0) + " s)";
    });

    auto motion = std::make_unique<AudioProcessorParameterGroup> ("motion", "MOTION", "|");
    motion->addChild (std::make_unique<APF> (pid (ids::motionRate), "Filter Motion Rate", range (0.03f, 8.0f, 0.5f), d.motionRate, rateAttr));
    motion->addChild (std::make_unique<APF> (pid (ids::filterRange), "Filter Motion Range", NormalisableRange<float> (-8.0f, 8.0f), d.filterRange,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " oct"; })));
    motion->addChild (std::make_unique<APF> (pid (ids::pitchRange), "Pitch Motion Range", NormalisableRange<float> (-12.0f, 12.0f), d.pitchRange,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; })));
    motion->addChild (std::make_unique<APC> (pid (ids::pitchLink), "Pitch Motion Mode", StringArray { "Linked", "Free" }, d.pitchLinked ? 0 : 1));
    motion->addChild (std::make_unique<APF> (pid (ids::pitchRate), "Pitch Motion Rate (Free)", range (0.03f, 8.0f, 0.5f), d.pitchRate, rateAttr));
    motion->addChild (std::make_unique<APC> (pid (ids::motionSync), "Motion Sync", StringArray { "Hz", "Sync" }, 1));
    motion->addChild (std::make_unique<APC> (pid (ids::motionDiv), "Filter Motion Rate (Sync)", ids::motionDivNames(), 7));   // 2 bars
    motion->addChild (std::make_unique<APC> (pid (ids::pitchDiv), "Pitch Motion Rate (Sync)", ids::motionDivNames(), 6));     // 1 bar
    motion->addChild (std::make_unique<APC> (pid (ids::motionMode), "Motion Mode",
        StringArray { "Retrig", "Free", "Bar Sync", "One Shot" }, d.motionMode));
    motion->addChild (std::make_unique<API> (pid (ids::randomSeed), "Random Pattern (S&H seed)", 0, 9999, d.randomSeed));
    motion->addChild (std::make_unique<APC> (pid (ids::filterShape), "Filter Motion Shape",
        StringArray { "Triangle", "Sine", "Rise", "Fall", "Jump", "Steps", "S&H Random" }, d.filterShape));
    motion->addChild (std::make_unique<APC> (pid (ids::pitchShape), "Pitch Motion Shape",
        StringArray { "Triangle", "Sine", "Rise", "Fall", "Jump", "Steps", "Random" }, d.pitchShape));

    // ---- Effects ----
    const FxParams fx;
    auto mix = [&] (const char* id, const char* name, float def)
    { return std::make_unique<APF> (pid (id), name, NormalisableRange<float> (0, 1), def, pctAttr); };

    auto fxGroup = std::make_unique<AudioProcessorParameterGroup> ("fx", "EFFECTS", "|");
    fxGroup->addChild (std::make_unique<APF> (pid (ids::tubeDrive), "Distortion", NormalisableRange<float> (0.0f, 1.0f), fx.tubeDrive,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : String (roundToInt (v * 100.0f)) + " %"; })));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::tubeType), "Distortion Type", StringArray { "Tube", "Fuzz", "Square" }, fx.tubeType));
    fxGroup->addChild (std::make_unique<APB> (pid (ids::formantOn), "Formant On", fx.formantOn));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::vowel), "Formant Vowel", NormalisableRange<float> (0.0f, 4.0f), fx.vowel,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            static const char* names[] = { "A", "E", "I", "O", "U" };
            const int i = jlimit (0, 3, (int) v);
            const float t = v - (float) i;
            if (t < 0.08f) return String (names[i]);
            if (t > 0.92f) return String (names[i + 1]);
            return String (names[i]) + " > " + String (names[i + 1]);
        })));
    fxGroup->addChild (mix (ids::formantMix, "Formant Mix", fx.formantMix));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::formantLfo), "Formant LFO", NormalisableRange<float> (0.0f, 1.0f), fx.formantLfoDepth,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : String (roundToInt (v * 100.0f)) + " %"; })));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::formantLfoRate), "Formant LFO Rate",
        StringArray { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }, fx.formantLfoRate));

    fxGroup->addChild (std::make_unique<APB> (pid (ids::crushOn), "Crush On", fx.crushOn));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::bits), "Crush Bits", NormalisableRange<float> (1.0f, 16.0f), fx.bits,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::downsample), "Crush Rate", range (1.0f, 32.0f, 6.0f), fx.downsample,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1.05f ? String ("off") : "/" + String (v, 1); })));
    fxGroup->addChild (mix (ids::crushMix, "Crush Mix", fx.crushMix));

    fxGroup->addChild (std::make_unique<APB> (pid (ids::gateOn), "Gate On", fx.gateOn));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::gateRate), "Gate Rate", StringArray { "1/8", "1/16", "1/32" }, fx.gateRate));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::gateLength), "Gate Length", NormalisableRange<float> (0.1f, 1.0f), fx.gateLength, pctAttr));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::gateDepth), "Gate Depth", NormalisableRange<float> (0.0f, 1.0f), fx.gateDepth, pctAttr));
    for (int i = 0; i < 16; ++i)
        fxGroup->addChild (std::make_unique<APB> (ParameterID { ids::gateStep (i), 1 }, "Gate Step " + String (i + 1), fx.gateSteps[(size_t) i]));

    fxGroup->addChild (std::make_unique<APB> (pid (ids::phaserOn), "Phaser On", fx.phaserOn));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::phaserRate), "Phaser Rate",
        StringArray { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }, fx.phaserRate));
    fxGroup->addChild (mix (ids::phaserDepth, "Phaser Depth", fx.phaserDepth));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::phaserFeedback), "Phaser Feedback", NormalisableRange<float> (-0.9f, 0.9f), fx.phaserFeedback, pctAttr));
    fxGroup->addChild (mix (ids::phaserMix, "Phaser Mix", fx.phaserMix));

    fxGroup->addChild (std::make_unique<APB> (pid (ids::combOn), "Flange On", d.combOn));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::combPitch), "Flange Freq (vs cutoff)", NormalisableRange<float> (-36.0f, 36.0f), d.combPitch,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; })));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::combFeedback), "Flange Res", NormalisableRange<float> (0.0f, 0.99f), d.combFeedback, pctAttr));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::combMotion), "Flange Motion (unused)", NormalisableRange<float> (-1.0f, 1.0f), d.combMotion,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.005f ? String ("off") : (v > 0 ? "+" : "") + String (v * 24.0f, 1) + " st"; })));
    fxGroup->addChild (mix (ids::combDamp, "Flange Damp", d.combDamp));
    fxGroup->addChild (mix (ids::combMix, "Flange Mix", d.combMix));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::combMode), "Flange Mode", StringArray { "Flange +", "Flange -", "Flange +/-" }, d.combMode));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::combRate), "Flange LFO Rate", ids::motionDivNames(), 6));   // 1 bar
    fxGroup->addChild (mix (ids::combDepth, "Flange LFO Depth", d.combDepth));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::combDrive), "Flange Destroy", NormalisableRange<float> (0, 1), d.combDrive,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? String ("off") : String (roundToInt (v * 100.0f)) + " %"; })));
    fxGroup->addChild (mix (ids::combSpread, "Flange Stereo Spread", d.combSpread));

    fxGroup->addChild (std::make_unique<APB> (pid (ids::delayOn), "Delay On", fx.delayOn));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::delayTime), "Delay Time", StringArray { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, fx.delayTime));
    fxGroup->addChild (std::make_unique<APC> (pid (ids::delayFeel), "Delay Feel", StringArray { "Normal", "Dotted", "Triplet" }, fx.delayFeel));
    fxGroup->addChild (std::make_unique<APF> (pid (ids::delayFeedback), "Delay Feedback", NormalisableRange<float> (0.0f, 0.9f), fx.delayFeedback, pctAttr));
    fxGroup->addChild (mix (ids::delayMix, "Delay Mix", fx.delayMix));

    fxGroup->addChild (std::make_unique<APB> (pid (ids::reverbOn), "Reverb On", fx.reverbOn));
    fxGroup->addChild (mix (ids::reverbSize, "Reverb Size", fx.reverbSize));
    fxGroup->addChild (mix (ids::reverbTone, "Reverb Tone", fx.reverbTone));
    fxGroup->addChild (mix (ids::reverbMix, "Reverb Mix", fx.reverbMix));

    auto out = std::make_unique<AudioProcessorParameterGroup> ("out", "OUTPUT", "|");
    out->addChild (std::make_unique<APF> (pid (ids::outputGain), "Output", NormalisableRange<float> (-36.0f, 6.0f), Decibels::gainToDecibels (d.outputGain),
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    layout.add (std::move (osc), std::move (flt), std::move (motion), std::move (fxGroup), std::move (fenv), std::move (pitch), std::move (aenv), std::move (voice), std::move (out));
    return layout;
}
} // namespace rsq
