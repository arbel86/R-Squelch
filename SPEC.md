# R-Squelch — full spec (source prompt)

Build a VST3 instrument plugin (plus AU on macOS and a Standalone app) in C++ with JUCE 8 and CMake.

PLUGIN NAME: R-Squelch
DEVELOPER / BRAND: RBell (the logo file is at /assets/rbell_logo.png)
PURPOSE: Psytrance squelches. These are short resonant sawtooth or square stabs swept
through a band-pass filter, with pitch modulation you can adjust. The plugin must sound
great as soon as it loads, with no tweaking needed.

## 1. SIGNAL FLOW
MIDI In → [Osc A + FM Modulator] → Drive → Band-Pass Filter → Amp Env
→ FX Chain (Bitcrusher → Phaser → Delay → Reverb, order can be changed by drag) → Output

## 2. VOICE
- Mono by default (psytrance style), with Legato and Glide (0–500 ms, modes: Always / Legato-only).
- Optional Poly mode (up to 8 voices).
- Velocity → Filter Env Amount and Velocity → Amp (each with its own adjustable amount).
- Accent: velocity above a threshold you can set gives a stronger filter envelope and more resonance (303-style accent).

## 3. OSCILLATOR
- Waveform: Saw / Square, plus a continuous Morph knob (Saw ↔ Square).
- Pulse Width for the square (5%–95%), which can be modulated.
- Anti-aliasing: PolyBLEP, or 2x/4x oversampling (selectable).
- Octave (-3..+3), Semitone (-12..+12), Fine (±100 cents).
- Sub oscillator (-1 octave, square) with its own level.
- Unison: 1–4 voices with Detune and Stereo Spread.

FM SECTION:
- A sine modulator (with an option for Saw/Square) that modulates Osc A.
- Ratio (0.25–16, with steps on whole ratios plus Fine), FM Amount, and Feedback.
- Its own envelope for FM Amount (Decay + Amount). This gives the metallic "bite" at the start of each note.
- Mode: Linear FM / Phase Modulation.

## 4. BAND-PASS FILTER (THE CORE OF THE SQUELCH)
- ZDF / TPT State Variable Filter in Band-Pass mode, with an option for a 4-pole BP (24 dB).
- Cutoff (20 Hz–20 kHz, log scale), Resonance (up to self-oscillation, kept stable without blowing up).
- Width/Bandwidth: a separate control for the width of the band.
- Filter Drive: saturation inside the filter's feedback loop (tanh), for the classic "rubbery" character.
- Key Tracking (0–100%).
- Filter Envelope: Attack / Decay / Sustain / Release + Amount (bipolar ±). Curve knob (exponential ↔ linear).
- Filter LFO: Rate (Hz or tempo-sync from 1/64 up to 4 bars, including triplets and dotted values), Depth,
  Shapes: Sine / Triangle / Saw Up / Saw Down / Square / Sample&Hold / Smooth Random, Retrigger on note, Phase.

## 5. PITCH MODULATION (ADJUSTABLE)
- Pitch Envelope: Amount in semitones (-48..+48), Attack, Decay, Curve.
- Pitch LFO: Rate (Hz/Sync), Depth (in cents/semitones), Shapes as in the filter LFO, Fade-in time, Retrigger.
- Pitch Bend Range (1–24).
- Optional modulation of the pitch from the filter envelope (Filter Env → Pitch, ±).

## 6. MOD MATRIX (SIMPLE)
- 8 slots: Source → Destination → Amount.
- Sources: LFO1, LFO2, Filter Env, Pitch Env, FM Env, Velocity, Mod Wheel, Aftertouch, Random per note.
- Destinations: Cutoff, Resonance, Width, Pitch, PW, Morph, FM Amount, Drive, and any FX parameter.
- Drag & drop: drag from a source onto any knob to create a modulation (like Serum/Vital). Shown as a colored ring around the knob.

## 7. FX CHAIN
Each effect has an On/Off button, Mix (Dry/Wet), and its own clear UI module.
- BITCRUSHER: Bit Depth (1–16, continuous), Sample Rate Reduction, Jitter, Pre/Post filter option, Mix.
- PHASER: Stages (2/4/6/8/12), Rate (Hz/Sync), Depth, Feedback (±), Center Freq, Stereo Phase Offset, Mix.
- DELAY: Time (ms or Sync, incl. dotted/triplet), Feedback, Ping-Pong / Stereo / Mono, HP/LP in the feedback loop,
  Ducking, Modulation (light Wow/Flutter), Mix.
- REVERB: Algorithmic (FDN or Dattorro plate), Size, Decay, Pre-Delay, Damping, Width, HP/LP on the tail, Ducking, Freeze, Mix.
- Drag-to-reorder for the FX order.
- Output stage: Soft Clipper, Stereo Width, Output Gain, Level meter.

## 8. UI DESIGN: MODERN, CLEAN, MINIMAL, WITH A PSYCHEDELIC TOUCH
- Clean and minimal: lots of empty space, flat surfaces, no skeuomorphism, no heavy shadows. The psychedelic element
  shows up only as accents and animation, never as clutter.
- Colors: background #0B0B0E, panels #141418, borders #232329, text #EDEDED, secondary #8A8A95.
  Accent: iridescent gradient violet #8B5CF6 → magenta #EC4899 → cyan #22D3EE → acid green #A3E635, slowly hue-shifting.
  Used ONLY on knob arcs, active modulation rings and the main visual display.
- Typography: clean geometric sans (Inter / Manrope via BinaryData), uppercase, wide letter-spacing for section names.
- Logo: /assets/rbell_logo.png via BinaryData, rendered white on the dark background, header top-left. Next to it
  "R-SQUELCH" bold and wide, the "R" in the iridescent gradient. Optional glow animation on load.
- Layout (default 1100×700, resizable 75%–200%, sharp in HiDPI):
  - Header: logo, preset browser (prev/next, categories, save, init), Oversampling, CPU meter, Output knob.
  - Top row: OSC | FM | FILTER (largest, most central).
  - Center: Visual Display — live band-pass curve, output oscilloscope, neon afterimage trails as the filter moves,
    subtle kaleidoscopic/fractal background reacting to amplitude and resonance (max 15% opacity); "Visuals" toggle.
  - Bottom row: ENVELOPES (Filter / Pitch / Amp / FM tabs, draggable graphic editor) | LFO 1 | LFO 2.
  - Right side or tab: FX RACK, vertical modules, drag to reorder.
  - Bottom tab: MOD MATRIX.
- Controls: thin minimal knobs (ring + arc, arc in the gradient). Hover shows value, double-click resets,
  Ctrl/Cmd+drag fine, typed entry. Smooth 60fps animation without repainting the whole window.

## 9. PRESETS
At least 24 factory presets: Classic Squelch, Rubber Squelch, Zap / Laser, FM Bite, Acid Morph, Crushed, Space Squelch.
XML/JSON preset files, A/B compare, Undo/Redo, Randomize with per-section lock.

## 10. TECHNICAL REQUIREMENTS
- All parameters through AudioProcessorValueTreeState with full DAW automation; smoothing everywhere.
- Sample-accurate MIDI; tempo sync from the host PlayHead.
- Real-time safe audio thread; stable at 44.1–192 kHz and any buffer size; denormal protection; DC blocker.
- CPU target: under 3% on one core for a mono voice with all FX on.
- Windows (VST3 + Standalone) and macOS (VST3 + AU + Standalone).
- Structure: Source/DSP, Source/UI, Source/Presets, assets. Unit tests for the DSP.

## 11. WORK ORDER
1. DSP core (Osc + BP Filter + Envs + Pitch mod) with a basic generic UI.
2. FM and LFOs.
3. FX chain.
4. Full UI to the design spec.
5. Mod Matrix + drag-to-modulate.
6. Presets + polish.
After each stage, stop, explain what was built, and how to test it in a DAW.
