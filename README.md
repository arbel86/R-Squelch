# R-Squelch — by RBell

Psytrance squelch synth (VST3 / Standalone, AU on macOS). JUCE 8 + CMake.

## בנייה ב-Windows

1. צריך Visual Studio Community מותקן, עם הרכיב **Desktop development with C++**.
2. לחיצה כפולה על `build.bat`.
3. בסיום, הקבצים נמצאים בתיקייה `Output`:
   - `R-Squelch.exe`: גרסה עצמאית, נפתחת בלחיצה כפולה.
   - `R-Squelch.vst3`: הפלאגין. מעתיקים אותו ל-`C:\Program Files\Common Files\VST3` ועושים Rescan ב-DAW.

## קובץ התקנה (Installer)

1. מריצים קודם את `build.bat`.
2. מתקינים פעם אחת את Inno Setup 6 (חינמי): https://jrsoftware.org/isdl.php
3. לחיצה כפולה על `make_installer.bat`. קובץ ההתקנה נוצר בתיקייה `Installer`: `R-Squelch-Setup-0.1.0.exe`.
   הוא מתקין את ה-VST3 ל-`C:\Program Files\Common Files\VST3` ואת האפליקציה ל-`Program Files\RBell`, עם קיצור דרך ו-Uninstall.
   הגרסה מוגדרת ב-`installer/R-Squelch.iss` (שורה `#define AppVer`).

## בנייה ל-Mac

יש שתי דרכים:

**דרך GitHub, בלי Mac.** הפרויקט נמצא ב-GitHub (ריפו פרטי `arbel86/R-Squelch`). בכל עדכון לענף `main`, GitHub בונה אוטומטית על מחשב Mac בענן (`.github/workflows/build-mac.yml`):
- VST3, Audio Unit ו-Standalone, כ-Universal (Apple Silicon + Intel), ל-macOS 10.13 ומעלה.
- בדיקות DSP, ‏auval ו-pluginval.
- קובץ התקנה `R-Squelch-0.1.0-Mac.pkg` וקובץ zip.

הקבצים מופיעים בעמוד **Releases** של הריפו. הלוגים של כל בנייה נשמרים בענף `ci-logs`.

**על Mac.** צריך Xcode Command Line Tools ו-CMake (`brew install cmake`). מריצים `./build_mac.sh`, והתוצאה נוצרת בתיקייה `Output-mac`.

**חתימה:** הפלאגין חתום חתימה עצמית (ad-hoc), לא Developer ID של Apple. בפעם הראשונה שפותחים את ה-pkg, macOS מזהיר: "unidentified developer". הפתרון: קליק ימני ← Open ← Open. ההוראות למשתמשים נמצאות בקובץ `installer/mac/README_MAC.txt`. כדי שלא תהיה אזהרה בכלל, צריך Apple Developer Program (‏99$ לשנה) וחתימה ו-notarization.

## Status

| Stage | What | Status |
|---|---|---|
| 1 | DSP core: osc, band-pass, envelopes, pitch mod | **done** |
| 1b | Simple custom UI: interactive filter display, ~15 controls, keyboard | **done** |
| 1c | Long held notes with a slow filter and pitch SWEEP, SAW/SQUARE switch, mono only | **done** |
| 1d | Quality and Output in the header | **done** |
| 1e | Repeating triangle MOTION on filter and pitch (linked or free) | **done** |
| 1f | SH-5 style filter mode (HP -> ladder LP -> band-pass), now the default | **done** |
| 1g | Single resonant peak in every filter mode; display shows one solid curve | **done** |
| 3b | Psychedelic lightning in the UI every 6 s while the gate is on | **done** |
| 3c | TUBE distortion knob in the header (0 = off), first in the FX chain | **done** |
| 1h | Moog-style oscillators (free-running, drift, soft edges, droop, warmth) + SYNC knob (anti-aliased hard sync, x1-x8) | **done** |
| 3d | Moon in the filter display background (original, generated image) with a glow while playing | **done** |
| 1i | BRIGHT (digital psy, default) / WARM (Moog) oscillator character; brighter default motion (220 Hz + 5 oct) | **done** |
| 1j | FM knob: 2-operator FM tone (sine PM, index 1-8, ratio 1:1 / 2:1 / 3:1), in tune | **done** |
| 3 | FX row: Formant, Crush, Trance Gate (16 steps, synced), Ping-pong Delay (synced), Reverb | **done** |
| 2 | FM + LFOs | next |
| 4 | 3D hardware UI: brushed-metal faceplate, recessed modules, metal knobs with LED rings, glass screens, lit buttons, piano keys | **done** |
| 4b | Black & gold luxury finish: black anodised faceplate, gold trims, gold knob caps and light rings, gold bezels, gold-lit buttons | **done** |
| 3e | Formant LFO: depth + tempo-synced rate (1/16 to 4 bars), sweeps the vowel | **done** |
| 4c | Alien step pyramid UI: space backdrop, golden capstone with a glowing eye, basalt tiers with gold inlay and runes, R-SQUELCH in alien glyphs, MASTER panel | **done** |
| 4d | Pyramid-shaped window: the standalone app is borderless and see-through around the pyramid (drag by the stone, gold buttons for settings / minimise / close); in a DAW the corners are black | **done** |
| 1k | ACID waveform (TB-303 style curved saw) next to SAW / SQUARE | **done** |
| 1l | Pitch LFO presets: Triangle, Sine, Rise, Fall, Jump, Steps, Random (all start on the played note) | **done** |
| 1m | Filter LFO presets: Triangle, Sine, Rise, Fall, Jump, Steps, S&H Random (sample & hold, 4 steps per cycle, click-free) | **done** |
| 4f | In S&H mode the cat watches the filter: head tilts with every step, ears and tail flick, eyes glow in changing colours | **done** |
| 4e | Blinking eye, a Sphynx cat resting beside the capstone, SETTINGS label on the standalone button | **done** |
| 2a | Tempo sync for FILTER / PITCH MOTION: Hz or Sync (1/16 .. 8 bars, follows the host BPM) | **done** |
| 2b | 303 filter mode: ZDF 4-pole ladder voiced like the TB-303 (WIDTH = high-pass under it) | **done** |
| 2c | GROWL (audio-rate filter FM from the oscillator) and FOLD (sine wavefolder after the filter) | **done** |
| 6a | Sound presets: 15 factory sounds (Squelch / Random / Acid / Growl) + your own, saved in Documents/RBell/R-Squelch/Presets | **done** |
| 2d | S&H / RANDOM: a repeatable 16-step pattern chosen by a seed, with a dice button to roll a new one | **done** |
| 3f | Gate pattern presets (Gallop, Full, Trance, Offbeat, Rolling, Psy Gate, Stutter, Broken, Morse, Half, Build) + Random | **done** |
| 7a | Cubase hang on close: JUCE 8.0.15 (Direct2D message-thread fix), no static juce::Image, 30 Hz displays; passes pluginval strictness 10 | **done** |
| 4g | Back to a clean rectangular black & gold panel (1240×884, opens at 80%); normal standalone window with JUCE's Options menu; the Sphynx cat sits on the header line | **done** |
| 6b | 40 factory presets in 6 categories (Squelch, Acid, Random, Growl, Rhythm, Lead & FX), presets can carry gate patterns, one flat menu with headings | **done** |
| 1n | UNISON up to 7 oscillators + DETUNE knobs in OSC (spread starting phases, stereo) | **done** |
| 3g | PHASER (6 all-pass stages, tempo-synced sweep, stereo) and extreme COMB filter (tuned to the note, bipolar feedback, swept by the squelch MOTION) | **done** |
| 2e | Motion MODE: Retrig, Free (keeps running), Bar Sync (locked to the song position), One Shot | **done** |
| 6c | 6 Comb & Phase presets (46 in total) | **done** |
| 3h | COMB upgrade: NEG FLANGE mode, its own synced LFO (Rate/Depth), DESTROY (distortion in the loop), stereo SPREAD; 3 new presets | **done** |
| 3i | COMB replaced by a FLANGE +/- filter (Serum-style): follows the main filter cutoff, RES up to 99 %, +, -, +/- (stereo) modes, LFO, DESTROY, SPREAD; delay NORMAL / DOTTED / TRIPLET; the RBell logo links to soundcloud.com/rbellmusic | **done** |
| 5 | Mod matrix + drag-to-modulate | |
| 6 | Presets + polish | |

Full spec: [SPEC.md](SPEC.md).

## Layout

- `Source/DSP/`: pure C++ engine with no JUCE dependency, so it can be unit-tested on its own.
  - `Oscillator.h`: Moog-style saw/square with anti-aliased hard SYNC (polyBLEP on every jump, one-sample look-back), drift, droop, warmth; sub.
  - `BandPassFilter.h`: TPT SVF with tanh in the loop. 12 dB (one BP), 24 dB (two identical BPs in series), SH-5 (HP → ladder LP on the cutoff → BP). Always one peak, on the cutoff.
  - `synth_dsp.h`: Arbel's synth building-block library (ladder, one-pole, wavefolders, LFO, ...), used by the 303 filter and FOLD.
  - `CombFilter.h`: the FLANGE +/- filter (a comb that follows the filter cutoff), with DESTROY drive in the loop.
  - `MotionShapes.h`: the pitch LFO presets.
  - `Envelope.h`: ADSR and AD with curve.
  - `Voice.h` / `SquelchEngine.h`: unison, mono/legato/glide/poly, accent, smoothing, output stage.
  - `Effects.h`: tube distortion (ADAA anti-aliased), formant, bitcrusher, 16-step trance gate, ping-pong delay, FDN reverb (tempo from the host, 145 BPM fallback).
- `Source/Parameters.h`: all APVTS parameters.
- `Source/Presets.h` + `Source/UI/PresetBar.h`: factory presets, saving / loading your own, the preset bar in MASTER.
- `Source/PluginProcessor.*`: sample-accurate MIDI and 1x/2x/4x oversampling.
- `Source/PluginEditor.*`: the main window, a clean panel: header (logo, name, presets, cat) / OSC + FILTER / AMP, FILTER MOTION, PITCH MOTION, MASTER / effects (no on-screen keyboard: play it from MIDI). It is drawn at 1240×1028, opens at 75% and scales from 50% to 200%.
- `Source/UI/`: theme, knob look, knobs and switches, displays (interactive filter, wave, pitch, amp), and the keyboard.
  Only about 15 controls are shown. The other parameters stay in the engine at their default values, and the DAW can still automate them.
- `Source/UI/FxModules.h`: the effects row (on/off pills, knobs, clickable gate steps).
- `Source/UI/Hardware.h`: 3D drawing helpers (metal faceplate, screws, recessed modules, glass screens).
- `Source/StandaloneApp.cpp`: no longer used (safe to delete).
- `Source/UI/Pyramid.h`: the pyramid scene (space, capstone, tiers) and the alien glyph alphabet for the title.
- `Source/UI/Lightning.h`: lightning overlay (visual only, mouse passes through).
- `tests/DspTests.cpp`: DSP unit tests. Run them with `ctest -C Release` in `build/`.
