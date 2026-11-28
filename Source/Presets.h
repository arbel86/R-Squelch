#pragma once
// R-Squelch — sound presets: built-in factory sounds + your own presets saved as files.
// User presets live in  Documents/RBell/R-Squelch/Presets/*.rsqpreset  (the full plugin state as XML),
// so they can be copied between computers and shared.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

namespace rsq
{
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values;   // parameter id -> real value (Hz, seconds, index...)
    const char* gate = nullptr;                           // optional 16-step gate pattern, 'x' = open
};

// gate patterns used by the presets (same as the gate's own pattern picker)
namespace gatepat
{
    inline constexpr const char* gallop  = "x.xxx.xxx.xxx.x.";
    inline constexpr const char* trance  = "x.x.x.x.x.x.x.x.";
    inline constexpr const char* offbeat = ".x.x.x.x.x.x.x.x";
    inline constexpr const char* rolling = "xxx.xxx.xxx.xxx.";
    inline constexpr const char* stutter = "xxxx..x.xxxx..x.";
    inline constexpr const char* psygate = "x.xxx.xx.xxx.xxx";
}

// Every preset starts from the default (Init) sound and changes only what is listed.
// Indices: waveform 0 Saw 1 Square 2 Acid | slope 0 12dB 1 24dB 2 SH-5 3 303 | oscCharacter 0 Warm 1 Bright
//          motionDiv / pitchDiv 0 1/16 1 1/8T 2 1/8 3 1/4T 4 1/4 5 1/2 6 1bar 7 2bars 8 4bars 9 8bars
//          filterShape / pitchShape 0 Tri 1 Sine 2 Rise 3 Fall 4 Jump 5 Steps 6 Random (S&H)
//          delayTime 0 1/16 1 1/8 2 3/16 3 1/4 4 3/8 5 1/2 | gateRate 0 1/8 1 1/16 2 1/32 | formantLfoRate 0 1/16 .. 6 4 bars
inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = []
    {
    std::vector<FactoryPreset> l {
        // ---------------- SQUELCH ----------------
        { "Zenon Roller", "Squelch", {
            { ids::slope, 2 }, { ids::cutoff, 220 }, { ids::resonance, 0.86f }, { ids::filterRange, 5 },
            { ids::motionSync, 1 }, { ids::motionDiv, 7 }, { ids::pitchRange, 3 },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayFeedback, 0.3f }, { ids::delayMix, 0.16f } } },
        { "Dark Forest", "Squelch", {
            { ids::slope, 1 }, { ids::oscCharacter, 0 }, { ids::cutoff, 140 }, { ids::resonance, 0.92f }, { ids::width, 0.15f },
            { ids::filterRange, 4 }, { ids::motionSync, 1 }, { ids::motionDiv, 8 }, { ids::filterShape, 1 }, { ids::pitchRange, -2 },
            { ids::formantOn, 1 }, { ids::vowel, 2.0f }, { ids::formantMix, 0.4f }, { ids::formantLfo, 0.3f }, { ids::formantLfoRate, 5 },
            { ids::tubeDrive, 0.2f }, { ids::reverbOn, 1 }, { ids::reverbMix, 0.2f } } },
        { "Rubber Worm", "Squelch", {
            { ids::slope, 0 }, { ids::waveform, 1 }, { ids::cutoff, 300 }, { ids::resonance, 0.9f }, { ids::width, 0.3f },
            { ids::filterRange, 3.5f }, { ids::motionSync, 1 }, { ids::motionDiv, 6 }, { ids::pitchRange, 0 }, { ids::growl, 0.25f },
            { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.6f } } },
        { "Trance Gate Squelch", "Squelch", {
            { ids::slope, 2 }, { ids::cutoff, 260 }, { ids::resonance, 0.88f }, { ids::filterRange, 4.5f },
            { ids::motionSync, 1 }, { ids::motionDiv, 8 }, { ids::pitchRange, 2 },
            { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.55f }, { ids::gateDepth, 1.0f },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayMix, 0.2f }, { ids::reverbOn, 1 }, { ids::reverbMix, 0.15f } } },
        { "Psy Bubble Bass", "Squelch", {
            { ids::slope, 2 }, { ids::octave, -1 }, { ids::subLevel, 0.45f }, { ids::cutoff, 120 }, { ids::resonance, 0.8f },
            { ids::filterRange, 3 }, { ids::motionSync, 1 }, { ids::motionDiv, 6 }, { ids::pitchRange, 0 },
            { ids::aDecay, 0.25f }, { ids::aSustain, 0.6f }, { ids::tubeDrive, 0.25f } } },

        // ---------------- S&H / RANDOM ----------------
        { "Alien Talk", "Random", {
            { ids::outputGain, -8.0f },
            { ids::slope, 2 }, { ids::cutoff, 240 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 },
            { ids::motionSync, 1 }, { ids::motionDiv, 6 }, { ids::filterShape, 6 }, { ids::pitchRange, 0 },
            { ids::formantOn, 1 }, { ids::vowel, 1.0f }, { ids::formantMix, 0.8f }, { ids::formantLfo, 0.7f }, { ids::formantLfoRate, 2 },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayMix, 0.2f } } },
        { "S&H Bubbles", "Random", {
            { ids::slope, 0 }, { ids::waveform, 1 }, { ids::cutoff, 250 }, { ids::resonance, 0.93f }, { ids::width, 0.2f },
            { ids::filterRange, 5 }, { ids::motionSync, 1 }, { ids::motionDiv, 5 }, { ids::filterShape, 6 }, { ids::pitchRange, 0 },
            { ids::delayOn, 1 }, { ids::delayTime, 1 }, { ids::delayFeedback, 0.45f }, { ids::delayMix, 0.3f } } },
        { "Crushed Robot", "Random", {
            { ids::slope, 0 }, { ids::waveform, 1 }, { ids::cutoff, 350 }, { ids::resonance, 0.85f }, { ids::filterRange, 4 },
            { ids::motionSync, 1 }, { ids::motionDiv, 5 }, { ids::filterShape, 6 }, { ids::pitchRange, 0 },
            { ids::crushOn, 1 }, { ids::bits, 6.0f }, { ids::downsample, 8.0f }, { ids::crushMix, 0.6f } } },
        { "Deep Space", "Random", {
            { ids::slope, 1 }, { ids::oscCharacter, 0 }, { ids::cutoff, 180 }, { ids::resonance, 0.9f }, { ids::filterRange, 5 },
            { ids::motionSync, 1 }, { ids::motionDiv, 9 }, { ids::filterShape, 6 }, { ids::pitchLink, 1 }, { ids::pitchDiv, 8 },
            { ids::pitchShape, 1 }, { ids::pitchRange, 5 },
            { ids::formantOn, 1 }, { ids::formantLfo, 0.5f }, { ids::formantLfoRate, 6 }, { ids::formantMix, 0.5f },
            { ids::delayOn, 1 }, { ids::delayTime, 4 }, { ids::delayFeedback, 0.6f }, { ids::delayMix, 0.35f },
            { ids::reverbOn, 1 }, { ids::reverbSize, 0.9f }, { ids::reverbMix, 0.45f } } },

        // ---------------- ACID (303) ----------------
        { "303 Classic", "Acid", {
            { ids::waveform, 2 }, { ids::slope, 3 }, { ids::cutoff, 300 }, { ids::resonance, 0.82f }, { ids::width, 0.4f },
            { ids::filterDrive, 0.35f }, { ids::fEnvAmount, 0.45f }, { ids::fDecay, 0.25f },
            { ids::filterRange, 2.5f }, { ids::motionSync, 1 }, { ids::motionDiv, 7 }, { ids::pitchRange, 0 },
            { ids::glideTime, 0.06f }, { ids::tubeDrive, 0.25f } } },
        { "Acid Screamer", "Acid", {
            { ids::waveform, 2 }, { ids::slope, 3 }, { ids::cutoff, 500 }, { ids::resonance, 0.95f }, { ids::width, 0.3f },
            { ids::filterDrive, 0.9f }, { ids::drive, 0.6f }, { ids::filterRange, 3 }, { ids::motionSync, 1 }, { ids::motionDiv, 6 },
            { ids::filterShape, 1 }, { ids::pitchRange, 0 }, { ids::tubeDrive, 0.55f },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayMix, 0.2f } } },
        { "Acid Steps", "Acid", {
            { ids::waveform, 1 }, { ids::slope, 3 }, { ids::cutoff, 260 }, { ids::resonance, 0.9f }, { ids::width, 0.5f },
            { ids::filterDrive, 0.5f }, { ids::filterRange, 4 }, { ids::motionSync, 1 }, { ids::motionDiv, 6 }, { ids::filterShape, 5 },
            { ids::pitchRange, 0 }, { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.7f } } },

        // ---------------- GROWL / METAL ----------------
        { "Growl Monster", "Growl", {
            { ids::slope, 2 }, { ids::cutoff, 200 }, { ids::resonance, 0.8f }, { ids::growl, 0.6f }, { ids::fold, 0.2f },
            { ids::filterRange, 4 }, { ids::motionSync, 1 }, { ids::motionDiv, 7 }, { ids::pitchRange, -5 }, { ids::pitchShape, 1 },
            { ids::tubeDrive, 0.3f } } },
        { "Metal Fold", "Growl", {
            { ids::slope, 1 }, { ids::fm, 0.3f }, { ids::fmRatio, 1 }, { ids::fold, 0.7f }, { ids::cutoff, 300 }, { ids::resonance, 0.85f },
            { ids::filterRange, 4 }, { ids::motionSync, 1 }, { ids::motionDiv, 7 }, { ids::pitchRange, 0 },
            { ids::crushOn, 1 }, { ids::bits, 10.0f }, { ids::crushMix, 0.3f } } },
        { "Sync Scream", "Growl", {
            { ids::slope, 2 }, { ids::sync, 0.55f }, { ids::cutoff, 350 }, { ids::resonance, 0.9f }, { ids::filterRange, 4 },
            { ids::motionSync, 1 }, { ids::motionDiv, 7 }, { ids::filterShape, 2 },
            { ids::pitchLink, 1 }, { ids::pitchShape, 4 }, { ids::pitchRange, 12 }, { ids::pitchDiv, 5 }, { ids::tubeDrive, 0.35f } } },
        { "Square Destroyer", "Growl", {
            { ids::outputGain, -3.0f },
            { ids::slope, 2 }, { ids::cutoff, 260 }, { ids::resonance, 0.92f }, { ids::width, 0.3f }, { ids::filterRange, 4.5f },
            { ids::motionSync, 1 }, { ids::motionDiv, 6 }, { ids::filterShape, 1 }, { ids::pitchRange, 0 },
            { ids::tubeType, 2 }, { ids::tubeDrive, 0.8f },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayFeel, 1 }, { ids::delayMix, 0.15f } } },
        { "Octave Fuzz", "Growl", {
            { ids::outputGain, -2.0f },
            { ids::waveform, 1 }, { ids::slope, 1 }, { ids::cutoff, 220 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 },
            { ids::motionSync, 1 }, { ids::motionDiv, 7 }, { ids::filterShape, 2 }, { ids::pitchRange, 0 },
            { ids::tubeType, 1 }, { ids::tubeDrive, 0.65f } } },

        // ================= more sounds =================
        // ---- Squelch ----
        { "Goa Morning", "Squelch", {
            { ids::slope, 2 }, { ids::oscCharacter, 0 }, { ids::cutoff, 260 }, { ids::resonance, 0.88f }, { ids::filterRange, 4.5f },
            { ids::motionDiv, 8 }, { ids::filterShape, 1 }, { ids::pitchRange, 5 },
            { ids::formantOn, 1 }, { ids::vowel, 3.0f }, { ids::formantMix, 0.3f },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayMix, 0.2f }, { ids::reverbOn, 1 }, { ids::reverbMix, 0.2f } } },
        { "Night Crawler", "Squelch", {
            { ids::slope, 1 }, { ids::waveform, 1 }, { ids::cutoff, 160 }, { ids::resonance, 0.9f }, { ids::width, 0.25f },
            { ids::filterRange, 3.5f }, { ids::motionDiv, 6 }, { ids::pitchRange, -3 }, { ids::tubeDrive, 0.35f } } },
        { "Zenon Twister", "Squelch", {
            { ids::slope, 2 }, { ids::sync, 0.35f }, { ids::cutoff, 230 }, { ids::resonance, 0.9f }, { ids::filterRange, 5 },
            { ids::motionDiv, 7 }, { ids::pitchLink, 1 }, { ids::pitchShape, 1 }, { ids::pitchDiv, 6 }, { ids::pitchRange, 4 },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayMix, 0.18f } } },
        { "Liquid Squelch", "Squelch", {
            { ids::slope, 0 }, { ids::cutoff, 300 }, { ids::resonance, 0.92f }, { ids::width, 0.15f }, { ids::filterRange, 4 },
            { ids::motionDiv, 7 }, { ids::filterShape, 1 }, { ids::pitchRange, 2 },
            { ids::formantOn, 1 }, { ids::formantLfo, 0.4f }, { ids::formantLfoRate, 5 }, { ids::formantMix, 0.5f },
            { ids::reverbOn, 1 }, { ids::reverbMix, 0.25f } } },
        { "Wide Unison Squelch", "Squelch", {
            { ids::slope, 2 }, { ids::unison, 3 }, { ids::detune, 0.25f }, { ids::spread, 0.8f },
            { ids::cutoff, 240 }, { ids::resonance, 0.85f }, { ids::filterRange, 4.5f }, { ids::motionDiv, 7 }, { ids::pitchRange, 2 } } },

        // ---- Acid ----
        { "Acid Rain", "Acid", {
            { ids::waveform, 2 }, { ids::slope, 3 }, { ids::cutoff, 400 }, { ids::resonance, 0.9f }, { ids::width, 0.5f },
            { ids::fEnvAmount, 0.5f }, { ids::fDecay, 0.2f }, { ids::filterRange, 2 }, { ids::motionDiv, 4 }, { ids::filterShape, 6 },
            { ids::pitchRange, 0 }, { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayFeedback, 0.4f }, { ids::delayMix, 0.25f } } },
        { "Rubber 303", "Acid", {
            { ids::waveform, 2 }, { ids::slope, 3 }, { ids::cutoff, 220 }, { ids::resonance, 0.88f }, { ids::width, 0.7f },
            { ids::filterDrive, 0.5f }, { ids::filterRange, 3 }, { ids::motionDiv, 6 }, { ids::pitchRange, 0 }, { ids::glideTime, 0.08f } } },
        { "Acid Jump", "Acid", {
            { ids::waveform, 1 }, { ids::slope, 3 }, { ids::cutoff, 320 }, { ids::resonance, 0.9f }, { ids::width, 0.4f },
            { ids::filterRange, 3 }, { ids::motionDiv, 5 }, { ids::filterShape, 4 },
            { ids::pitchLink, 1 }, { ids::pitchShape, 4 }, { ids::pitchDiv, 5 }, { ids::pitchRange, 12 } } },
        { "Tube Acid", "Acid", {
            { ids::waveform, 2 }, { ids::slope, 3 }, { ids::cutoff, 280 }, { ids::resonance, 0.93f }, { ids::width, 0.35f },
            { ids::filterDrive, 0.7f }, { ids::drive, 0.5f }, { ids::tubeDrive, 0.6f },
            { ids::filterRange, 3 }, { ids::motionDiv, 7 }, { ids::filterShape, 2 }, { ids::pitchRange, 0 } } },

        // ---- Random ----
        { "Laser Cat", "Random", {
            { ids::slope, 2 }, { ids::fm, 0.2f }, { ids::cutoff, 300 }, { ids::resonance, 0.9f }, { ids::filterRange, 5 },
            { ids::motionDiv, 2 }, { ids::filterShape, 6 }, { ids::pitchRange, 0 }, { ids::randomSeed, 77 },
            { ids::delayOn, 1 }, { ids::delayTime, 0 }, { ids::delayFeedback, 0.3f }, { ids::delayMix, 0.25f } } },
        { "Chaos Dice", "Random", {
            { ids::slope, 0 }, { ids::waveform, 1 }, { ids::cutoff, 280 }, { ids::resonance, 0.9f }, { ids::filterRange, 4 },
            { ids::motionDiv, 4 }, { ids::filterShape, 6 }, { ids::pitchLink, 1 }, { ids::pitchShape, 6 }, { ids::pitchDiv, 4 },
            { ids::pitchRange, 7 }, { ids::randomSeed, 4242 }, { ids::crushOn, 1 }, { ids::bits, 8.0f }, { ids::crushMix, 0.3f } } },
        { "Random Vowels", "Random", {
            { ids::outputGain, -11.0f },
            { ids::slope, 2 }, { ids::cutoff, 260 }, { ids::filterRange, 4 }, { ids::motionDiv, 6 },
            { ids::filterShape, 6 }, { ids::pitchRange, 0 }, { ids::randomSeed, 303 },
            { ids::formantOn, 1 }, { ids::formantLfo, 0.7f }, { ids::formantLfoRate, 2 }, { ids::formantMix, 0.6f }, { ids::resonance, 0.8f } } },

        // ---- Growl ----
        { "Bear Growl", "Growl", {
            { ids::slope, 2 }, { ids::growl, 0.8f }, { ids::fold, 0.1f }, { ids::cutoff, 150 }, { ids::resonance, 0.75f },
            { ids::filterRange, 3 }, { ids::motionDiv, 6 }, { ids::filterShape, 1 }, { ids::pitchRange, 0 }, { ids::tubeDrive, 0.4f } } },
        { "Fold Bells", "Growl", {
            { ids::slope, 1 }, { ids::fold, 0.9f }, { ids::fm, 0.4f }, { ids::fmRatio, 2 }, { ids::cutoff, 500 }, { ids::resonance, 0.8f },
            { ids::filterRange, 3 }, { ids::motionDiv, 7 }, { ids::pitchRange, 0 },
            { ids::delayOn, 1 }, { ids::delayTime, 4 }, { ids::delayFeedback, 0.5f }, { ids::delayMix, 0.3f },
            { ids::reverbOn, 1 }, { ids::reverbMix, 0.3f } } },
        { "Sync Growl", "Growl", {
            { ids::outputGain, -8.0f },
            { ids::slope, 2 }, { ids::sync, 0.6f }, { ids::growl, 0.5f }, { ids::cutoff, 260 }, { ids::resonance, 0.85f },
            { ids::filterRange, 4 }, { ids::motionDiv, 7 }, { ids::pitchRange, 0 } } },

        // ---- Rhythm (gate patterns) ----
        { "Gated Rolling", "Rhythm", {
            { ids::slope, 2 }, { ids::cutoff, 240 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 }, { ids::motionDiv, 8 },
            { ids::pitchRange, 2 }, { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.5f } }, gatepat::rolling },
        { "Offbeat Pump", "Rhythm", {
            { ids::slope, 2 }, { ids::cutoff, 250 }, { ids::resonance, 0.85f }, { ids::filterRange, 3 }, { ids::motionDiv, 7 },
            { ids::pitchRange, 0 }, { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.8f } }, gatepat::offbeat },
        { "Stutter Machine", "Rhythm", {
            { ids::outputGain, -4.0f },
            { ids::slope, 0 }, { ids::waveform, 1 }, { ids::cutoff, 300 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 },
            { ids::motionDiv, 5 }, { ids::filterShape, 6 }, { ids::pitchRange, 0 },
            { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.6f },
            { ids::crushOn, 1 }, { ids::bits, 10.0f }, { ids::crushMix, 0.25f } }, gatepat::stutter },
        { "Hypnotic Steps", "Rhythm", {
            { ids::slope, 2 }, { ids::cutoff, 230 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 }, { ids::motionDiv, 6 },
            { ids::filterShape, 5 }, { ids::pitchShape, 5 }, { ids::pitchRange, 12 },
            { ids::gateOn, 1 }, { ids::gateRate, 1 }, { ids::gateLength, 0.7f } }, gatepat::trance },

        // ---- Comb / Phaser / Unison ----
        { "Flange Madness", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 260 }, { ids::resonance, 0.85f }, { ids::filterRange, 4 }, { ids::motionDiv, 7 },
            { ids::pitchRange, 0 }, { ids::combOn, 1 }, { ids::combPitch, 0.0f }, { ids::combFeedback, 0.93f },
            { ids::combDepth, 0.3f }, { ids::combRate, 6 }, { ids::combDamp, 0.15f }, { ids::combMix, 0.8f } } },
        { "Hollow Tube Flange", "Flange & Phase", {
            { ids::slope, 1 }, { ids::waveform, 1 }, { ids::cutoff, 300 }, { ids::resonance, 0.8f }, { ids::filterRange, 3 },
            { ids::motionDiv, 6 }, { ids::filterShape, 1 }, { ids::pitchRange, 0 },
            { ids::combOn, 1 }, { ids::combMode, 1 }, { ids::combPitch, 7.0f }, { ids::combFeedback, 0.9f }, { ids::combMotion, -0.4f },
            { ids::combDamp, 0.35f }, { ids::combMix, 0.7f }, { ids::tubeDrive, 0.3f } } },
        { "Metallic S&H Flange", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 280 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 }, { ids::motionDiv, 4 },
            { ids::filterShape, 6 }, { ids::pitchRange, 0 }, { ids::randomSeed, 909 },
            { ids::combOn, 1 }, { ids::combPitch, 24.0f }, { ids::combFeedback, 0.97f }, { ids::combMotion, 1.0f },
            { ids::combDamp, 0.0f }, { ids::combMix, 0.6f }, { ids::combDrive, 0.4f }, { ids::outputGain, -3.0f } } },
        { "Neg Flange Jet", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 240 }, { ids::resonance, 0.86f }, { ids::filterRange, 4.5f }, { ids::motionDiv, 7 },
            { ids::pitchRange, 0 }, { ids::combOn, 1 }, { ids::combMode, 1 }, { ids::combPitch, 0.0f }, { ids::combFeedback, 0.85f },
            { ids::combMotion, 0.5f }, { ids::combRate, 7 }, { ids::combDepth, 0.7f }, { ids::combSpread, 0.8f },
            { ids::combDamp, 0.1f }, { ids::combMix, 0.9f } } },
        { "Destroyer", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 260 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 }, { ids::motionDiv, 5 },
            { ids::filterShape, 6 }, { ids::pitchRange, 0 }, { ids::randomSeed, 666 },
            { ids::combOn, 1 }, { ids::combMode, 2 }, { ids::combPitch, 12.0f }, { ids::combFeedback, 0.95f }, { ids::combMotion, 0.8f },
            { ids::combRate, 4 }, { ids::combDepth, 0.4f }, { ids::combDrive, 0.85f }, { ids::combSpread, 1.0f }, { ids::combMix, 0.8f } } },
        { "Psychedelic Swirl", "Flange & Phase", {
            { ids::outputGain, -5.0f },
            { ids::slope, 1 }, { ids::oscCharacter, 0 }, { ids::cutoff, 280 }, { ids::resonance, 0.85f }, { ids::filterRange, 4 },
            { ids::motionDiv, 8 }, { ids::filterShape, 1 }, { ids::pitchRange, 0 },
            { ids::combOn, 1 }, { ids::combMode, 1 }, { ids::combPitch, -5.0f }, { ids::combFeedback, 0.7f }, { ids::combRate, 8 },
            { ids::combDepth, 1.0f }, { ids::combSpread, 1.0f }, { ids::combMix, 0.8f },
            { ids::phaserOn, 1 }, { ids::phaserRate, 6 }, { ids::phaserMix, 0.5f }, { ids::reverbOn, 1 }, { ids::reverbMix, 0.25f } } },
        { "Laser Flange", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 220 }, { ids::resonance, 0.88f }, { ids::filterRange, 5 }, { ids::motionDiv, 6 },
            { ids::pitchRange, 0 }, { ids::combOn, 1 }, { ids::combMode, 0 }, { ids::combPitch, 0.0f }, { ids::combFeedback, 0.97f },
            { ids::combSpread, 0.6f }, { ids::combDamp, 0.05f }, { ids::combMix, 0.85f },
            { ids::delayOn, 1 }, { ids::delayTime, 1 }, { ids::delayFeel, 1 }, { ids::delayMix, 0.22f } } },
        { "Psycho Flange +/-", "Flange & Phase", {
            { ids::slope, 2 }, { ids::growl, 0.4f }, { ids::cutoff, 260 }, { ids::resonance, 0.85f }, { ids::filterRange, 4.5f },
            { ids::motionDiv, 5 }, { ids::filterShape, 6 }, { ids::pitchRange, 0 }, { ids::randomSeed, 2026 },
            { ids::combOn, 1 }, { ids::combMode, 2 }, { ids::combPitch, -12.0f }, { ids::combFeedback, 0.96f }, { ids::combRate, 4 },
            { ids::combDepth, 0.5f }, { ids::combDrive, 0.5f }, { ids::combSpread, 1.0f }, { ids::combMix, 0.9f } } },
        { "Phase Squelch", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 240 }, { ids::resonance, 0.86f }, { ids::filterRange, 4.5f }, { ids::motionDiv, 7 },
            { ids::pitchRange, 2 }, { ids::phaserOn, 1 }, { ids::phaserRate, 5 }, { ids::phaserDepth, 0.8f },
            { ids::phaserFeedback, 0.7f }, { ids::phaserMix, 0.8f } } },
        { "Supersaw Squelch", "Flange & Phase", {
            { ids::slope, 0 }, { ids::unison, 7 }, { ids::detune, 0.45f }, { ids::spread, 1.0f }, { ids::cutoff, 320 },
            { ids::resonance, 0.82f }, { ids::width, 0.35f }, { ids::filterRange, 4 }, { ids::motionDiv, 7 }, { ids::pitchRange, 0 },
            { ids::phaserOn, 1 }, { ids::phaserRate, 6 }, { ids::phaserDepth, 0.5f }, { ids::phaserMix, 0.5f },
            { ids::reverbOn, 1 }, { ids::reverbMix, 0.2f } } },
        { "Free Roller", "Flange & Phase", {
            { ids::slope, 2 }, { ids::cutoff, 230 }, { ids::resonance, 0.88f }, { ids::filterRange, 4.5f }, { ids::motionDiv, 6 },
            { ids::motionMode, 1 }, { ids::pitchRange, 3 }, { ids::unison, 3 }, { ids::detune, 0.2f },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayMix, 0.2f } } },

        // ---- Lead & FX ----
        { "Psy Lead", "Lead & FX", {
            { ids::octave, 1 }, { ids::unison, 3 }, { ids::detune, 0.3f }, { ids::spread, 0.9f },
            { ids::slope, 0 }, { ids::cutoff, 900 }, { ids::resonance, 0.6f }, { ids::width, 0.6f }, { ids::filterRange, 2 },
            { ids::motionDiv, 7 }, { ids::filterShape, 1 }, { ids::pitchRange, 0 }, { ids::aRelease, 0.4f },
            { ids::delayOn, 1 }, { ids::delayTime, 2 }, { ids::delayFeedback, 0.4f }, { ids::delayMix, 0.3f },
            { ids::reverbOn, 1 }, { ids::reverbMix, 0.3f } } },
        { "Alien Choir", "Lead & FX", {
            { ids::oscCharacter, 0 }, { ids::slope, 1 }, { ids::cutoff, 600 }, { ids::resonance, 0.7f }, { ids::width, 0.5f },
            { ids::filterRange, 2 }, { ids::motionDiv, 8 }, { ids::filterShape, 1 }, { ids::pitchRange, 0 },
            { ids::formantOn, 1 }, { ids::vowel, 0.0f }, { ids::formantLfo, 0.6f }, { ids::formantLfoRate, 5 }, { ids::formantMix, 1.0f },
            { ids::reverbOn, 1 }, { ids::reverbSize, 0.9f }, { ids::reverbMix, 0.5f } } },
        { "Space Siren", "Lead & FX", {
            { ids::slope, 2 }, { ids::cutoff, 800 }, { ids::resonance, 0.7f }, { ids::filterRange, 2 }, { ids::motionDiv, 7 },
            { ids::pitchLink, 1 }, { ids::pitchShape, 1 }, { ids::pitchDiv, 6 }, { ids::pitchRange, 12 },
            { ids::delayOn, 1 }, { ids::delayTime, 4 }, { ids::delayFeedback, 0.7f }, { ids::delayMix, 0.35f },
            { ids::reverbOn, 1 }, { ids::reverbMix, 0.35f } } },
        { "Zap Bass", "Lead & FX", {
            { ids::outputGain, -2.0f },
            { ids::octave, -1 }, { ids::subLevel, 0.5f }, { ids::pEnvAmount, 24.0f }, { ids::pDecay, 0.04f },
            { ids::slope, 2 }, { ids::cutoff, 180 }, { ids::resonance, 0.8f }, { ids::filterRange, 2 }, { ids::motionDiv, 6 },
            { ids::pitchRange, 0 }, { ids::aDecay, 0.2f }, { ids::aSustain, 0.5f } } },
        { "Deep Dive", "Lead & FX", {
            { ids::outputGain, -1.0f },
            { ids::octave, -1 }, { ids::slope, 1 }, { ids::cutoff, 300 }, { ids::resonance, 0.88f }, { ids::filterRange, 4 },
            { ids::motionDiv, 7 }, { ids::filterShape, 3 }, { ids::pitchLink, 1 }, { ids::pitchShape, 3 }, { ids::pitchDiv, 7 },
            { ids::pitchRange, 12 }, { ids::reverbOn, 1 }, { ids::reverbMix, 0.3f } } },
        { "Wormhole", "Lead & FX", {
            { ids::slope, 2 }, { ids::cutoff, 200 }, { ids::resonance, 0.9f }, { ids::filterRange, 5 }, { ids::motionDiv, 7 },
            { ids::filterShape, 2 }, { ids::pitchShape, 2 }, { ids::pitchRange, 7 },
            { ids::delayOn, 1 }, { ids::delayTime, 5 }, { ids::delayFeedback, 0.6f }, { ids::delayMix, 0.3f },
            { ids::reverbOn, 1 }, { ids::reverbSize, 0.95f }, { ids::reverbMix, 0.4f } } },
    };
    // keep each category together (the arrows step through them in this order)
    static const char* order[] = { "Squelch", "Acid", "Random", "Growl", "Rhythm", "Flange & Phase", "Lead & FX" };
    auto rank = [] (const char* c) { for (int i = 0; i < 7; ++i) if (juce::String (c) == order[i]) return i; return 7; };
    std::stable_sort (l.begin(), l.end(), [&] (const FactoryPreset& a, const FactoryPreset& b) { return rank (a.category) < rank (b.category); });
    return l;
    }();
    return list;
}

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

    static juce::File userFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("RBell").getChildFile ("R-Squelch").getChildFile ("Presets");
    }
    static constexpr const char* extension = ".rsqpreset";

    juce::StringArray userPresetNames() const
    {
        juce::StringArray names;
        for (auto& f : userFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
            names.add (f.getFileNameWithoutExtension());
        names.sort (true);
        return names;
    }

    // every preset in browse order: Init, the factory sounds, then yours
    juce::StringArray allNames() const
    {
        juce::StringArray all { "Init" };
        for (auto& p : factoryPresets()) all.add (p.name);
        all.addArray (userPresetNames());
        return all;
    }

    juce::String currentName() const { return state.state.getProperty ("presetName", "Init").toString(); }

    void loadInit()
    {
        resetToDefaults();
        setName ("Init");
    }

    void loadFactory (int index)
    {
        const auto& list = factoryPresets();
        if (index < 0 || index >= (int) list.size()) return;
        resetToDefaults();
        for (auto& [id, value] : list[(size_t) index].values)
            if (auto* p = state.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        if (const char* pat = list[(size_t) index].gate)
            for (int i = 0; i < 16; ++i)
                if (auto* p = state.getParameter (ids::gateStep (i)))
                    p->setValueNotifyingHost (pat[i] == 'x' ? 1.0f : 0.0f);
        setName (list[(size_t) index].name);
    }

    bool loadUser (const juce::String& name)
    {
        auto xml = juce::XmlDocument::parse (userFolder().getChildFile (name + extension));
        if (xml == nullptr || ! xml->hasTagName (state.state.getType())) return false;
        resetToDefaults();   // presets saved by older versions still get sensible values for newer knobs
        state.replaceState (juce::ValueTree::fromXml (*xml));
        setName (name);
        return true;
    }

    bool saveUser (const juce::String& rawName)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return false;
        if (! userFolder().createDirectory()) return false;
        setName (name);
        auto xml = state.copyState().createXml();
        return xml != nullptr && xml->writeTo (userFolder().getChildFile (name + extension));
    }

    void loadByName (const juce::String& name)
    {
        if (name == "Init") { loadInit(); return; }
        const auto& list = factoryPresets();
        for (int i = 0; i < (int) list.size(); ++i)
            if (name == list[(size_t) i].name) { loadFactory (i); return; }
        loadUser (name);
    }

    void step (int delta)
    {
        const auto all = allNames();
        const int i = juce::jmax (0, all.indexOf (currentName()));
        loadByName (all[(i + delta + all.size()) % all.size()]);
    }

private:
    void resetToDefaults()
    {
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    }

    void setName (const juce::String& n) { state.state.setProperty ("presetName", n, nullptr); }

    juce::AudioProcessorValueTreeState& state;
};
} // namespace rsq
