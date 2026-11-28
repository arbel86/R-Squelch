#pragma once
// R-Squelch — plain parameter snapshot passed from the plugin to the DSP engine each block.
// Defaults here are the "sounds great on load" squelch.

namespace rsq
{
struct EngineParams
{
    // Voice
    int   voiceMode       = 0;      // 0 = Mono, 1 = Poly
    bool  legato          = true;
    float glideTime       = 0.0f;   // seconds
    int   glideMode       = 1;      // 0 = Always, 1 = Legato only
    float bendRange       = 2.0f;   // semitones

    // Velocity / accent
    float velToFilter     = 0.5f;   // 0..1
    float velToAmp        = 0.3f;   // 0..1
    float accentThreshold = 0.85f;  // 0..1 (velocity / 127)
    float accentAmount    = 0.5f;   // 0..1

    // Oscillator
    float morph           = 0.0f;   // 0 = saw, 1 = square
    bool  acid            = false;  // ACID: TB-303 style curved saw (replaces the straight saw)
    float pulseWidth      = 0.5f;
    int   octave          = 0;
    int   semitone        = 0;
    float fineCents       = 0.0f;
    float subLevel        = 0.0f;
    int   unison          = 1;      // 1..7 oscillators (detuned, spread across the stereo field)
    float detune          = 0.15f;  // 0..1  (max +-50 cents)
    float spread          = 0.6f;   // 0..1
    float drive           = 0.25f;  // pre-filter drive 0..1
    int   oscCharacter    = 1;      // 0 = WARM (Moog-style), 1 = BRIGHT (sharp digital psy saw) — default
    bool  analogDrift     = true;   // Moog-style slow pitch drift, WARM only (tests switch it off)
    float fm              = 0.0f;   // 0..1 FM amount (0 = off)
    float fmRatio         = 1.0f;   // modulator pitch x note (1 = same pitch, 2 = octave up)
    float sync            = 0.0f;   // 0..1 -> hard-sync ratio 1..8 (adds harmonics, like Serum's Sync warp)

    // Filter
    float cutoffHz        = 220.0f;   // with the 5-octave motion the peak climbs to ~7 kHz: bright psy squelch
    float resonance       = 0.86f;
    float width           = 0.2f; 
    float filterDrive     = 0.2f; 
    int   slope           = 2;      // 0 = 12 dB, 1 = 24 dB, 2 = SH-5 style (default), 3 = TB-303 diode-ladder style low-pass
    float growl           = 0.0f;   // 0..1 audio-rate filter FM: the oscillator wobbles the cutoff (vocal growl)
    float fold            = 0.0f;   // 0..1 sine wavefolder after the filter (metallic harmonics)

    // COMB: tuned feedback comb after the filter (see CombFilter.h)
    bool  combOn          = false;
    float combPitch       = 0.0f;   // FLANGE FREQ: semitones relative to the main filter cutoff, -36..+36
    float combFeedback    = 0.85f;  // FLANGE RES 0..0.99 (the mode sets the polarity)
    float combMotion      = 0.0f;   // -1..1: the squelch movement sweeps the comb by up to +-24 semitones
    float combDamp        = 0.2f;   // 0..1
    float combMix         = 0.6f;   // 0..1
    int   combMode        = 0;      // 0 = FLANGE +, 1 = FLANGE -, 2 = FLANGE +/- (left +, right -)
    float combLfoHz       = 0.5f;   // its own sweep (the processor converts the synced RATE to Hz)
    float combDepth       = 0.0f;   // 0..1: LFO sweep, up to +-12 semitones (tuned) / +-3 octaves of delay (flange)
    float combDrive       = 0.0f;   // 0..1: DESTROY — distortion inside the feedback loop
    float combSpread      = 0.3f;   // 0..1: left / right sweep apart (stereo swirl)
    float keyTrack        = 0.5f;

    // Filter envelope
    float fA = 0.001f, fD = 0.16f, fS = 0.0f, fR = 0.12f;
    float fEnvAmount      = 0.12f;  // -1..1  (x 6 octaves)
    float fEnvCurve       = 0.6f;

    // Amp envelope
    float aA = 0.004f, aD = 0.3f, aS = 1.0f, aR = 0.25f;

    // Pitch modulation
    float pEnvAmount      = 0.0f;   // semitones, -48..48 (short 'zap' at note start)
    float pA              = 0.0f;
    float pD              = 0.035f;
    float pEnvCurve       = 0.7f;
    float filterEnvToPitch = 0.0f;  // semitones, -24..24

    // MOTION — the rolling psy / Zenon squelch.
    // Triangle movement that repeats while the note is held: it starts at the cutoff,
    // moves right by filterRange octaves, comes back left, and so on.
    // The pitch does the same movement (linked to the filter by default, or at its own rate).
    float motionRate      = 0.25f;  // Hz — full left-right-left cycles per second (0.25 = 4 s)
    float filterRange     = 5.0f;   // octaves, -8..8 (negative = moves left)
    float pitchRange      = 3.0f;   // semitones, -12..12 (negative = pitch goes down first)
    bool  pitchLinked     = true;   // pitch follows the filter movement exactly
    float pitchRate       = 0.5f;   // Hz, used when not linked
    int   filterShape     = 0;      // LFO preset for the filter: see motion::Shape (6 = S&H random)
    int   pitchShape      = 0;
    // how the motion LFOs run (filter and pitch together):
    //   0 RETRIG    restart at every note (the filter starts at the top)
    //   1 FREE      keep running between notes, like a free LFO on an analog synth
    //   2 BAR SYNC  locked to the song position: always in the same place on the bar (falls back to FREE when stopped)
    //   3 ONE SHOT  one cycle from the note, then hold the end position (a sweep, like an envelope)
    int   motionMode      = 0;
    int   randomSeed      = 1977;   // picks the RANDOM / S&H pattern (the dice button rolls a new one)      // LFO preset for the pitch: see motion::Shape (0 = triangle)

    // Output
    float outputGain      = 0.5f;   // linear (-6 dB: long held notes are loud)
};
} // namespace rsq
