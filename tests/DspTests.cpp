// R-Squelch — standalone DSP unit tests (no JUCE needed).
// Build target: RSquelchDspTests   Run: ctest  (or run the executable directly)

#include "../Source/DSP/SquelchEngine.h"
#include "../Source/DSP/Effects.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace rsq;

// pi as a plain constant: MSVC does not define M_PI by default
constexpr double kPiD = 3.14159265358979323846;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

static float rms (const float* x, int n)
{
    double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return (float) std::sqrt (s / std::max (1, n));
}

static void testOscillator()
{
    Oscillator o; o.reset(); o.setDriftEnabled (false);
    const float sr = 48000.0f;
    double sum = 0; float peak = 0;
    for (float morph : { 0.0f, 0.5f, 1.0f })
        for (int i = 0; i < 48000; ++i)
        {
            float m, s; o.process (110.0f, sr, morph, 0.3f, 1.0f, m, s);
            sum += m; peak = std::max (peak, std::abs (m));
        }
    CHECK (peak < 1.6f, "oscillator peak stays bounded");
    CHECK (std::abs (sum / (3 * 48000)) < 0.02, "oscillator has (almost) no DC offset");
}

static void testFilterStability()
{
    const float sr = 48000.0f;
    Rng rng;
    for (int slope = 0; slope < 4; ++slope)
    {
        BandPassFilter f; f.prepare (sr);
        bool ok = true; float peak = 0;
        for (int i = 0; i < (int) sr * 5; ++i)
        {
            const float sweep = 30.0f * std::exp2 (9.0f * (0.5f + 0.5f * std::sin (i * 0.0003f)));
            const float y = f.process (rng.next01() * 2 - 1, sweep, 1.0f, 0.0f, 1.0f, (BandPassFilter::Slope) slope);
            if (! std::isfinite (y)) ok = false;
            peak = std::max (peak, std::abs (y));
        }
        const char* names[] = { "12dB", "24dB", "SH-5", "303" };
        std::printf ("      [%s] ", names[slope]);
        CHECK (ok, "filter stays finite at max resonance + drive");
        std::printf ("      [%s] ", names[slope]);
        CHECK (peak < 20.0f, "filter output bounded");
    }
}

static float sineThroughFilter (float freq, float cutoff, BandPassFilter::Slope slope)
{
    const float sr = 48000.0f;
    BandPassFilter f; f.prepare (sr);
    std::vector<float> out (24000);
    for (int i = 0; i < 24000; ++i)
        out[(size_t) i] = f.process (0.1f * std::sin (kTwoPi * freq * i / sr), cutoff, 0.5f, 0.3f, 0.0f, slope);
    return rms (out.data() + 12000, 12000);
}

static void testBandPassShape()
{
    for (int slope = 0; slope < 3; ++slope)
    {
        const auto s = (BandPassFilter::Slope) slope;
        const float centre = sineThroughFilter (1000.0f, 1000.0f, s);
        const float low = sineThroughFilter (125.0f, 1000.0f, s);
        const float high = sineThroughFilter (8000.0f, 1000.0f, s);
        const char* names[] = { "12dB", "24dB", "SH-5" };
        std::printf ("      [%s] ", names[slope]);
        CHECK (centre > 3.0f * low && centre > 3.0f * high, "is a band-pass (centre > lows/highs)");
    }
}

static void testEnvelope()
{
    AdsrEnvelope e; e.prepare (1000.0f);
    e.setParameters (0.01f, 0.05f, 0.5f, 0.05f, 0.5f);
    e.noteOn();
    float peak = 0;
    for (int i = 0; i < 12; ++i) peak = std::max (peak, e.process());
    CHECK (peak > 0.99f, "envelope attack reaches 1");
    for (int i = 0; i < 100; ++i) e.process();
    CHECK (std::abs (e.getLevel() - 0.5f) < 1e-3f, "envelope settles on sustain");
    e.noteOff();
    for (int i = 0; i < 100; ++i) e.process();
    CHECK (! e.isActive() && e.getLevel() == 0.0f, "envelope releases to idle");
}

static void testEngine()
{
    SquelchEngine eng; eng.prepare (48000.0f);
    EngineParams p; eng.setParams (p); eng.snapSmoothers();
    std::vector<float> l (48000), r (48000);

    eng.noteOn (45, 1.0f);
    eng.render (l.data(), r.data(), 4800);
    CHECK (rms (l.data(), 4800) > 0.01f, "note-on produces sound");
    eng.noteOff (45);
    eng.render (l.data(), r.data(), 48000);
    CHECK (eng.getNumActiveVoices() == 0, "voice frees after release");
    CHECK (rms (l.data() + 40000, 8000) < 1e-4f, "silence after release");

    // poly + unison + 24 dB + extreme settings: no NaN, bounded output
    p.voiceMode = 1; p.unison = 4; p.slope = 1; p.resonance = 1.0f; p.filterDrive = 1.0f; p.drive = 1.0f;
    p.pEnvAmount = 48.0f; p.fEnvAmount = 1.0f;
    eng.setParams (p);
    for (int n = 0; n < 12; ++n) eng.noteOn (30 + n * 7, 1.0f);
    eng.render (l.data(), r.data(), 48000);
    CHECK (allFinite (l) && allFinite (r), "extreme settings stay finite");
    float pk = 0; for (float x : l) pk = std::max (pk, std::abs (x));
    CHECK (pk <= 1.0f, "output stage keeps peaks within +-1");
}

static void testMotion()
{
    // filter goes right by filterRange octaves at half cycle, and is back at the start after a full cycle
    SquelchEngine eng; eng.prepare (48000.0f);
    EngineParams p; p.motionRate = 0.5f; p.filterRange = 4.0f; p.fEnvAmount = 0.0f; p.keyTrack = 0.0f;
    eng.setParams (p); eng.snapSmoothers();
    std::vector<float> l (48000), r (48000);
    eng.noteOn (52, 1.0f);
    eng.render (l.data(), r.data(), 48);
    const float startCut = eng.uiCutoffHz.load();
    eng.render (l.data(), r.data(), 48000 - 48);         // t = 1 s  -> half cycle (far right)
    const float midCut = eng.uiCutoffHz.load();
    eng.render (l.data(), r.data(), 48000);              // t = 2 s  -> full cycle (back left)
    const float endCut = eng.uiCutoffHz.load();
    CHECK (std::abs (startCut / midCut - 16.0f) < 1.5f, "filter starts at the top of the range (RANGE above the cutoff)");
    CHECK (std::abs (endCut / startCut - 1.0f) < 0.1f, "filter is back at the top after a full cycle");
    CHECK (rms (l.data(), 48000) > 0.01f, "held note keeps sounding (long note)");
}

static void testPitchMotion()
{
    // count zero crossings in 0.25 s windows: with pitch motion the rate of crossings changes over time
    auto crossings = [] (float range, std::vector<int>& out)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.pitchRange = range; p.motionRate = 1.0f; p.filterRange = 0.0f; p.analogDrift = false;
        p.resonance = 0.0f; p.cutoffHz = 2000.0f; p.fEnvAmount = 0.0f; p.slope = 0;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (57, 1.0f);
        std::vector<float> l (12000), r (12000);
        eng.render (l.data(), r.data(), 12000);                  // skip the note's first window (settling)
        for (int w = 0; w < 4; ++w)
        {
            eng.render (l.data(), r.data(), 12000);
            int c = 0; for (int i = 1; i < 12000; ++i) c += (l[(size_t) i - 1] < 0.0f) != (l[(size_t) i] < 0.0f);
            out.push_back (c);
        }
    };
    std::vector<int> flat, moving;
    crossings (0.0f, flat);
    crossings (12.0f, moving);
    const int flatSpread = *std::max_element (flat.begin(), flat.end()) - *std::min_element (flat.begin(), flat.end());
    const int movSpread = *std::max_element (moving.begin(), moving.end()) - *std::min_element (moving.begin(), moving.end());
    CHECK (flatSpread <= 2 && movSpread >= 20, "pitch motion moves the pitch up and down in cycles");
}

static void testEffects()
{
    const float sr = 48000.0f;
    auto impulseRun = [&] (FxParams p, int n, std::vector<float>& outL, std::vector<float>& outR)
    {
        FxChain fx; fx.prepare (sr); fx.setParams (p); fx.setTransport (120.0, false, 0.0);
        outL.assign ((size_t) n, 0.0f); outR.assign ((size_t) n, 0.0f);
        outL[0] = outR[0] = 0.8f;
        fx.process (outL.data(), outR.data(), n);
    };
    std::vector<float> l, r;

    // Delay: 1/8 at 120 BPM = 0.25 s -> first echo on the left at 12000 samples, ping-pong to the right at 24000
    FxParams d; d.reverbOn = false; d.delayOn = true; d.delayTime = 1; d.delayMix = 1.0f; d.delayFeedback = 0.5f;
    impulseRun (d, 30000, l, r);
    auto peakAround = [] (const std::vector<float>& v, int c) { float pk = 0; for (int i = c - 50; i < c + 50; ++i) pk = std::max (pk, std::abs (v[(size_t) i])); return pk; };
    CHECK (peakAround (l, 12000) > 0.1f && peakAround (r, 12000) < 0.01f, "delay: first echo on the left at the synced time (1/8)");
    CHECK (peakAround (r, 24000) > 0.05f, "delay: second echo ping-pongs to the right");

    // Reverb: produces a tail that decays and stays finite
    FxParams rv; rv.delayOn = false; rv.reverbOn = true; rv.reverbMix = 1.0f; rv.reverbSize = 0.5f;
    impulseRun (rv, 96000 * 2, l, r);
    CHECK (allFinite (l) && allFinite (r), "reverb: output stays finite");
    CHECK (rms (l.data() + 4800, 9600) > 1e-4f, "reverb: tail present after the impulse");
    CHECK (rms (l.data() + 180000, 9600) < 0.25f * rms (l.data() + 4800, 9600), "reverb: tail decays");

    // Gate: closed steps are silent (depth 100 %), open steps pass
    FxParams g; g.delayOn = false; g.reverbOn = false; g.gateOn = true; g.gateRate = 1; g.gateLength = 1.0f; g.gateDepth = 1.0f;
    g.gateSteps.fill (false); g.gateSteps[0] = true;
    {
        FxChain fx; fx.prepare (sr); fx.setParams (g); fx.setTransport (120.0, false, 0.0);
        std::vector<float> a (24000, 0.5f), b (24000, 0.5f);   // 1/16 at 120 BPM = 6000 samples per step
        fx.process (a.data(), b.data(), 24000);
        CHECK (a[3000] > 0.45f && std::abs (a[9000]) < 0.01f && std::abs (a[15000]) < 0.01f, "gate: step 1 open, steps 2-3 closed");
    }

    // Bitcrusher: 2 bits -> only a few distinct levels
    FxParams c; c.delayOn = false; c.reverbOn = false; c.crushOn = true; c.bits = 2.0f; c.crushMix = 1.0f;
    {
        FxChain fx; fx.prepare (sr); fx.setParams (c);
        std::vector<float> a (4800), b (4800);
        for (int i = 0; i < 4800; ++i) a[(size_t) i] = b[(size_t) i] = 0.8f * std::sin (i * 0.01f);
        fx.process (a.data(), b.data(), 4800);
        std::vector<float> levels;
        for (float v : a) { bool seen = false; for (float x : levels) seen = seen || std::abs (x - v) < 1e-4f; if (! seen) levels.push_back (v); }
        CHECK (levels.size() <= 5, "bitcrusher: 2 bits gives only a handful of levels");
    }

    // Formant: finite and keeps a sensible level on a saw
    FxParams f; f.delayOn = false; f.reverbOn = false; f.formantOn = true; f.formantMix = 1.0f;
    {
        FxChain fx; fx.prepare (sr); fx.setParams (f);
        std::vector<float> a (48000), b (48000);
        for (int i = 0; i < 48000; ++i) { float ph = std::fmod (i * 110.0f / sr, 1.0f); a[(size_t) i] = b[(size_t) i] = 0.5f * (2 * ph - 1); }
        const float inRms = rms (a.data(), 48000);
        fx.process (a.data(), b.data(), 48000);
        const float outRms = rms (a.data() + 4800, 43200);
        CHECK (allFinite (a) && outRms > inRms * 0.25f && outRms < inRms * 2.5f, "formant: finite, level within range");
    }
}

static void testTube()
{
    const float sr = 48000.0f;
    auto run = [&] (float drive, std::vector<float>& out, int type = 0)
    {
        FxParams p; p.delayOn = false; p.reverbOn = false; p.tubeDrive = drive; p.tubeType = type;
        FxChain fx; fx.prepare (sr); fx.setParams (p);
        out.assign (48000, 0.0f); std::vector<float> r (48000);
        for (int i = 0; i < 48000; ++i) out[(size_t) i] = r[(size_t) i] = 0.4f * std::sin (kTwoPi * 440.0f * i / sr);
        fx.process (out.data(), r.data(), 48000);
    };
    auto harmonic = [&] (const std::vector<float>& y, float f)
    {
        double re = 0, im = 0;
        for (int i = 24000; i < 48000; ++i) { re += y[(size_t) i] * std::cos (kTwoPi * f * i / sr); im -= y[(size_t) i] * std::sin (kTwoPi * f * i / sr); }
        return std::sqrt (re * re + im * im) / 24000.0;
    };
    std::vector<float> clean, warm, hot;
    run (0.0f, clean); run (0.4f, warm); run (1.0f, hot);
    CHECK (harmonic (clean, 880.0f) < 1e-4, "tube at 0 = off (no added harmonics)");
    CHECK (harmonic (warm, 880.0f) > 0.005 && harmonic (warm, 1320.0f) > 0.002, "tube adds even (2nd) and odd (3rd) harmonics");
    CHECK (allFinite (hot), "tube at full drive stays finite");
    const float lc = rms (clean.data() + 24000, 24000), lh = rms (hot.data() + 24000, 24000);
    CHECK (lh > lc * 0.5f && lh < lc * 2.5f, "tube keeps the level in a sensible range");

    // the other distortion types
    std::vector<float> fz, sq, sqLow;
    run (0.8f, fz, 1); run (1.0f, sq, 2); run (0.3f, sqLow, 2);
    CHECK (allFinite (fz) && allFinite (sq) && allFinite (sqLow), "fuzz / square stay finite");
    CHECK (harmonic (fz, 880.0f) > 0.2 * harmonic (fz, 440.0f), "fuzz: strong octave-up (2nd harmonic)");
    const double h1 = harmonic (sq, 440.0f), h3 = harmonic (sq, 1320.0f), h5 = harmonic (sq, 2200.0f);
    CHECK (h3 > 0.25 * h1 && h5 > 0.13 * h1, "square: square-wave spectrum (3rd ~1/3, 5th ~1/5)");
    CHECK (harmonic (sq, 880.0f) < 0.02 * h1, "square: symmetric (no even harmonics)");
    double pk = 0; for (int i = 24000; i < 48000; ++i) pk = std::max (pk, (double) std::abs (sq[(size_t) i]));
    CHECK (pk / rms (sq.data() + 24000, 24000) < 1.25, "square: crest factor ~1 (flat-topped, fully squashed)");
    for (auto* v : { &fz, &sq, &sqLow })
    {
        const float l = rms (v->data() + 24000, 24000);
        CHECK (l > lc * 0.5f && l < lc * 2.5f, "fuzz / square keep the level in a sensible range");
    }
    {   // switching type while playing: no blow-up, no huge jumps beyond a square's own edge
        TubeDistortion t; t.prepare (sr);
        bool ok = true;
        for (int i = 0; i < 48000; ++i)
        {
            float l = 0.4f * std::sin (kTwoPi * 110.0f * i / sr), r = l;
            t.process (l, r, 0.7f, (i / 4000) % 3);
            ok = ok && std::isfinite (l) && std::abs (l) < 3.0f;
        }
        CHECK (ok, "distortion type switching stays finite and bounded");
    }
}

// spectrum helper: energy at harmonics of f0 vs everything else (aliasing / inharmonic junk)
static void harmonicSplit (const std::vector<float>& y, float f0, float sr, double& harm, double& other)
{
    const int n = (int) y.size();
    harm = other = 0.0;
    for (int bin = 1; bin < n / 2; bin += 1)
    {
        const double f = bin * sr / n;
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPiD * i / n); re += w * y[(size_t) i] * std::cos (2 * kPiD * bin * i / n); im -= w * y[(size_t) i] * std::sin (2 * kPiD * bin * i / n); }
        const double e = re * re + im * im;
        const double k = f / f0;
        (std::abs (k - std::round (k)) * f0 < 3.0 * sr / n ? harm : other) += e;
    }
}

static void testSync()
{
    const float sr = 48000.0f;
    auto render = [&] (float freq, float ratio, float morph, int n)
    {
        Oscillator o; o.reset(); o.setSeed (1); o.setDriftEnabled (false);
        std::vector<float> y ((size_t) n);
        float m, s;
        for (int i = 0; i < 96000; ++i) o.process (freq, sr, morph, 0.5f, ratio, m, s);     // settle (DC blocker)
        for (int i = 0; i < n; ++i) { o.process (freq, sr, morph, 0.5f, ratio, m, s); y[(size_t) i] = m; }
        return y;
    };
    // pitch stays at the note: with sync the waveform still repeats at the master frequency (harmonics of f0 only)
    double h, o;
    // frequencies sit exactly on an FFT bin but do NOT divide 48 kHz evenly, so any aliasing lands between harmonics
    const float fLow = 9.0f * sr / 2048.0f, fHigh = 65.0f * sr / 2048.0f;    // ~211 Hz and ~1523 Hz
    auto y = render (fLow, 3.4f, 0.0f, 2048);
    harmonicSplit (y, fLow, sr, h, o);
    std::printf ("      sync 3.4 @211 Hz: inharmonic %.1f dB\n", 10 * std::log10 (o / h));
    CHECK (o < h * 1e-3, "sync keeps the pitch (non-harmonic energy below -30 dB)");

    // sync adds upper harmonics (brighter): compare the energy of the signal's slope (weights highs) to its total energy
    auto brightness = [&] (const std::vector<float>& v)
    {
        double hi = 0, tot = 0;
        for (size_t i = 1; i < v.size(); ++i) { const double d = v[i] - v[i - 1]; hi += d * d; tot += (double) v[i] * v[i]; }
        return hi / tot;
    };
    const double b1 = brightness (render (fLow, 1.0f, 0.0f, 8192)), b5 = brightness (render (fLow, 5.0f, 0.0f, 8192));
    CHECK (b5 > b1 * 2.0, "sync adds upper harmonics (brighter)");

    // aliasing stays low with heavy sync in the squelch range (1x quality; 4x brings it to about -90 dB)
    y = render (fLow, 5.3f, 0.0f, 2048);
    harmonicSplit (y, fLow, sr, h, o);
    std::printf ("      sync 5.3 @211 Hz: aliasing %.1f dB\n", 10 * std::log10 (o / h));
    CHECK (o < h * 1e-3, "heavy sync: aliasing below -30 dB at 1x quality");
    y = render (fHigh, 1.0f, 0.0f, 2048);
    harmonicSplit (y, fHigh, sr, h, o);
    std::printf ("      no sync @1.5 kHz: aliasing %.1f dB\n", 10 * std::log10 (o / h));
    CHECK (o < h * 0.003, "plain saw on a high note: aliasing below -25 dB at 1x quality");
}

static void testFm()
{
    const float sr = 48000.0f;
    const float f0 = 9.0f * sr / 2048.0f;   // ~211 Hz, on an FFT bin, does not divide 48 kHz
    auto render = [&] (float fm, float ratio, float sync, int n)
    {
        Oscillator o; o.reset(); o.setDriftEnabled (false); o.setFm (fm, ratio);
        std::vector<float> y ((size_t) n); float m, s;
        for (int i = 0; i < 96000; ++i) o.process (f0, sr, 0.0f, 0.5f, sync, m, s);
        for (int i = 0; i < n; ++i) { o.process (f0, sr, 0.0f, 0.5f, sync, m, s); y[(size_t) i] = m; }
        return y;
    };
    for (float ratio : { 1.0f, 2.0f })
    {
        double h, o;
        harmonicSplit (render (0.5f, ratio, 1.0f, 2048), f0, sr, h, o);
        std::printf ("      FM 50%% ratio %.0f: non-harmonic %.1f dB\n", ratio, 10 * std::log10 (o / h));
        CHECK (o < h * 1e-3, "FM stays in tune (ratio 1 and 2: energy on the note's harmonics)");
    }
    // FM reshapes the spectrum: compare harmonic levels 1..30 with and without FM
    auto harmLevels = [&] (const std::vector<float>& y)
    {
        std::vector<double> H;
        for (int h = 1; h <= 30; ++h)
        {
            const int bin = 9 * h; double re = 0, im = 0;
            for (int i = 0; i < 2048; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPiD * i / 2048); re += w * y[(size_t) i] * std::cos (2 * kPiD * bin * i / 2048); im -= w * y[(size_t) i] * std::sin (2 * kPiD * bin * i / 2048); }
            H.push_back (10 * std::log10 (re * re + im * im + 1e-12));
        }
        return H;
    };
    const auto clean = harmLevels (render (0.0f, 1.0f, 1.0f, 2048)), fmd = harmLevels (render (0.6f, 1.0f, 1.0f, 2048));
    double change = 0; for (size_t i = 0; i < clean.size(); ++i) change += std::abs (clean[i] - fmd[i]);
    std::printf ("      FM 60%%: average harmonic change %.1f dB\n", change / clean.size());
    CHECK (change / clean.size() > 3.0, "FM clearly reshapes the harmonics");
    auto y = render (1.0f, 2.0f, 8.0f, 48000);
    float pk = 0; for (float v : y) pk = std::max (pk, std::abs (v));
    std::printf ("      full FM + SYNC peak %.2f\n", pk);
    CHECK (allFinite (y) && pk < 2.0f, "full FM + full SYNC stays finite and bounded");
}

static void testFormantLevel()
{
    // the formant must change the colour, not the volume (measured on the real squelch)
    auto level = [] (bool on, float vowel, float mix)
    {
        const int sr = 48000;
        SquelchEngine eng; eng.prepare ((float) sr); EngineParams p; eng.setParams (p); eng.snapSmoothers();
        FxChain fx; fx.prepare ((float) sr);
        FxParams f; f.delayOn = false; f.reverbOn = false; f.formantOn = on; f.vowel = vowel; f.formantMix = mix; fx.setParams (f);
        std::vector<float> L ((size_t) (4 * sr)), R ((size_t) (4 * sr)); eng.noteOn (40, 0.9f);
        for (int i = 0; i < 4 * sr; i += 256) { eng.render (&L[(size_t) i], &R[(size_t) i], 256); fx.process (&L[(size_t) i], &R[(size_t) i], 256); }
        double ss = 0; for (int i = sr; i < 4 * sr; ++i) ss += (double) L[(size_t) i] * L[(size_t) i];
        return 10.0 * std::log10 (ss / (3.0 * sr));
    };
    const double off = level (false, 0.0f, 0.0f);
    double worst = 0;
    for (float mix : { 0.3f, 0.6f, 1.0f })
        for (float v : { 0.0f, 2.0f, 4.0f })
            worst = std::max (worst, std::abs (level (true, v, mix) - off));
    std::printf ("      formant: largest level change %.1f dB\n", worst);
    CHECK (worst < 1.5, "formant keeps the volume (within 1.5 dB at every vowel and mix)");
}

static void testFormantLfo()
{
    // with the LFO on, the vowel moves over time (and stays inside A..U); synced: 1 bar at 120 BPM = 2 s per cycle
    FxChain fx; fx.prepare (48000.0f); fx.setTransport (120.0, false, 0.0);
    FxParams f; f.delayOn = false; f.reverbOn = false; f.formantOn = true; f.vowel = 2.0f; f.formantLfoDepth = 1.0f; f.formantLfoRate = 4;
    fx.setParams (f);
    std::vector<float> l (2400, 0.1f), r (2400, 0.1f);
    float lo = 9, hi = -9;
    for (int b = 0; b < 40; ++b) { fx.process (l.data(), r.data(), 2400); const float v = fx.uiFormantVowel.load(); lo = std::min (lo, v); hi = std::max (hi, v); }
    std::printf ("      formant LFO: vowel moved between %.2f and %.2f\n", lo, hi);
    CHECK (hi - lo > 3.0f && lo >= 0.0f && hi <= 4.0f, "formant LFO sweeps the vowel (inside A..U)");
}

static void testAcid()
{
    const float sr = 48000.0f;
    auto render = [&] (float freq, bool acid, int n)
    {
        Oscillator o; o.reset(); o.setSeed (1); o.setDriftEnabled (false); o.setAcid (acid);
        std::vector<float> y ((size_t) n);
        float m, s;
        for (int i = 0; i < 4800; ++i) o.process (freq, sr, 0.0f, 0.5f, 1.0f, m, s);
        for (int i = 0; i < n; ++i) { o.process (freq, sr, 0.0f, 0.5f, 1.0f, m, s); y[(size_t) i] = m; }
        return y;
    };
    const float fLow = 9.0f * sr / 2048.0f, fHigh = 65.0f * sr / 2048.0f;
    auto y = render (fLow, true, 2048);
    double h, o;
    harmonicSplit (y, fLow, sr, h, o);
    std::printf ("      acid @211 Hz: inharmonic %.1f dB\n", 10 * std::log10 (o / h));
    CHECK (o < h * 1e-3, "ACID saw is in tune (energy on the note's harmonics)");
    double dc = 0; float pk = 0;
    for (auto v : y) { dc += v; pk = std::max (pk, std::abs (v)); }
    CHECK (std::abs (dc / (double) y.size()) < 0.02 && pk < 1.4f, "ACID saw is centred (no DC) and bounded");
    y = render (fHigh, true, 2048);
    harmonicSplit (y, fHigh, sr, h, o);
    std::printf ("      acid @1.5 kHz: aliasing %.1f dB\n", 10 * std::log10 (o / h));
    CHECK (o < h * 0.003, "ACID saw on a high note: aliasing below -25 dB at 1x quality");
    // the shape really differs from the straight saw: compare the two over one cycle
    auto a = render (fLow, true, 228), b = render (fLow, false, 228);
    double diff = 0; for (size_t i = 0; i < a.size(); ++i) diff = std::max (diff, (double) std::abs (a[i] - b[i]));
    CHECK (diff > 0.2, "ACID bends the ramp (clearly different from the plain saw)");
}

static void testPitchShapes()
{
    using namespace motion;
    bool startsOnNote = true;
    for (int s = 0; s < numShapes; ++s) startsOnNote = startsOnNote && std::abs (shapeValue (s, 0.0f)) < 1e-6f;
    CHECK (startsOnNote, "every pitch LFO preset starts on the played note");
    CHECK (std::abs (shapeValue (rise, 0.99f) - 0.99f) < 1e-4f && std::abs (shapeValue (fall, 0.5f) + 0.5f) < 1e-4f, "RISE climbs, FALL dives");
    CHECK (shapeValue (jump, 0.25f) == 0.0f && shapeValue (jump, 0.75f) == 1.0f, "JUMP switches between the note and note + RANGE");
    CHECK (std::abs (shapeValue (steps, 0.6f) - 2.0f / 3.0f) < 1e-5f, "STEPS climbs a 4-step staircase");
    float lo = 1, hi = 0;
    for (uint32_t c = 1; c < 50; ++c) { const float v = shapeValue (motion::random, 0.3f, 1, pitchSeedFor ((int) c)); lo = std::min (lo, v); hi = std::max (hi, v); }
    CHECK (lo >= 0.0f && hi <= 1.0f && hi - lo > 0.5f, "RANDOM gives varied steps within the range");

    // every preset runs in the engine and moves the pitch
    bool ok = true;
    for (int s = 0; s < numShapes; ++s)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.pitchRange = 12.0f; p.motionRate = 2.0f; p.pitchShape = s; p.analogDrift = false;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (57, 1.0f);
        std::vector<float> l (48000), r (48000);
        eng.render (l.data(), r.data(), 48000);
        ok = ok && allFinite (l) && rms (l.data(), 48000) > 0.01f;
    }
    CHECK (ok, "all pitch LFO presets play and stay finite");
}

static void testFilterShapes()
{
    using namespace motion;
    CHECK (filterShapeValue (triangle, 0.0f) == 1.0f && filterShapeValue (triangle, 0.5f) == 0.0f, "filter TRIANGLE starts at the top, reaches the cutoff mid-cycle");
    bool tops = true;
    for (int s : { (int) sine, (int) fall, (int) jump, (int) steps, (int) motion::random }) tops = tops && std::abs (filterShapeValue (s, 0.0f) - 1.0f) < 1e-6f;
    CHECK (tops && filterShapeValue (rise, 0.0f) == 0.0f, "filter presets start at the top of the range (RISE starts at the cutoff)");

    // S&H RANDOM in the engine: the cutoff holds still inside each step and jumps to new places between steps
    SquelchEngine eng; eng.prepare (48000.0f);
    EngineParams p; p.filterShape = motion::random; p.motionRate = 1.0f; p.filterRange = 5.0f; p.fEnvAmount = 0.0f; p.keyTrack = 0.0f;
    eng.setParams (p); eng.snapSmoothers();
    eng.noteOn (45, 1.0f);
    std::vector<float> l (512), r (512), cut;
    for (int b = 0; b < 375; ++b)   // 4 s
    {
        eng.render (l.data(), r.data(), 512);
        if (b % 23 == 11) cut.push_back (eng.uiCutoffHz.load());   // samples land in different quarter-cycle steps
    }
    std::sort (cut.begin(), cut.end());
    int distinct = 1;
    for (size_t i = 1; i < cut.size(); ++i) distinct += cut[i] > cut[i - 1] * 1.05f;
    CHECK (distinct >= 6 && cut.back() / cut.front() > 4.0f, "S&H RANDOM: the filter jumps between many random positions");
}

static void test303()
{
    using S = BandPassFilter::Slope;
    // a real low-pass: lows pass, highs are cut steeply
    const float low = sineThroughFilter (200.0f, 1000.0f, S::tb303);
    const float high = sineThroughFilter (8000.0f, 1000.0f, S::tb303);
    std::printf ("      303: 8 kHz is %.1f dB below 200 Hz (cutoff 1 kHz)\n", 20 * std::log10 (high / low));
    CHECK (high < low * 0.03f, "303: low-pass, highs 3 octaves above the cutoff drop by more than 30 dB");

    // resonance makes a peak at the cutoff, but stops just short of endless self-oscillation
    auto peakGain = [] (float reso)
    {
        BandPassFilter f; f.prepare (48000.0f);
        std::vector<float> y (24000);
        for (int i = 0; i < 24000; ++i) y[(size_t) i] = f.process (0.05f * std::sin (kTwoPi * 1000.0f * i / 48000.0f), 1000.0f, reso, 0.0f, 0.0f, S::tb303);
        return rms (y.data() + 12000, 12000);
    };
    CHECK (peakGain (0.9f) > peakGain (0.0f) * 3.0f, "303: resonance builds a strong peak at the cutoff");
    {
        BandPassFilter f; f.prepare (48000.0f);
        float tail = 0;
        for (int i = 0; i < 96000; ++i)
        {
            const float y = f.process (i == 0 ? 1.0f : 0.0f, 800.0f, 1.0f, 0.0f, 0.0f, S::tb303);
            if (i > 72000) tail = std::max (tail, std::abs (y));
        }
        CHECK (tail < 1e-3f, "303: at full resonance the ring dies away (no endless whistle)");
    }
    // stable everywhere: every sample rate, cutoff and resonance, full drive, loud input
    bool ok = true; float pk = 0;
    for (float sr : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        BandPassFilter f; f.prepare (sr);
        Rng rng;
        for (int i = 0; i < (int) sr * 2; ++i)
        {
            const float fc = 20.0f * std::exp2 (10.0f * (0.5f + 0.5f * std::sin (i * 0.00031f)));
            const float y = f.process ((rng.next01() * 2 - 1) * 2.0f, fc, 1.0f, 0.5f, 1.0f, S::tb303);
            ok = ok && std::isfinite (y); pk = std::max (pk, std::abs (y));
        }
    }
    CHECK (ok && pk < 10.0f, "303: stable and bounded at every sample rate, sweeping 20 Hz - 20 kHz, full reso + drive");

    // the 303 mode sits at a similar volume to the SH-5 default in the engine
    auto engineRms = [] (int slope, float growl, float fold)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.slope = slope; p.growl = growl; p.fold = fold; p.analogDrift = false;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (45, 1.0f);
        std::vector<float> l (96000), r (96000);
        eng.render (l.data(), r.data(), 96000);
        return std::pair<float, bool> (rms (l.data() + 4800, 91200), allFinite (l) && allFinite (r));
    };
    const auto sh5 = engineRms (2, 0, 0), acid = engineRms (3, 0, 0);
    std::printf ("      303 vs SH-5 level: %.1f dB\n", 20 * std::log10 (acid.first / sh5.first));
    CHECK (acid.second && std::abs (20 * std::log10 (acid.first / sh5.first)) < 6.0f, "303: level within 6 dB of the SH-5 mode");

    // GROWL and FOLD: finite, they change the sound, the level stays sensible
    const auto growl = engineRms (2, 1.0f, 0), fold = engineRms (2, 0, 1.0f);
    std::printf ("      growl: %.1f dB, fold: %.1f dB (vs clean)\n", 20 * std::log10 (growl.first / sh5.first), 20 * std::log10 (fold.first / sh5.first));
    CHECK (growl.second && fold.second, "GROWL and FOLD at full stay finite");
    CHECK (std::abs (20 * std::log10 (growl.first / sh5.first)) < 9.0f && std::abs (20 * std::log10 (fold.first / sh5.first)) < 9.0f, "GROWL and FOLD keep the level within 9 dB");
}

static void testFoldHarmonics()
{
    // fold adds overtones: compare the slope energy of the output with and without
    auto bright = [] (float fold)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.fold = fold; p.analogDrift = false; p.filterRange = 0.0f; p.cutoffHz = 400.0f;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (45, 1.0f);
        std::vector<float> l (48000), r (48000);
        eng.render (l.data(), r.data(), 48000);
        double hi = 0, tot = 0;
        for (size_t i = 4801; i < l.size(); ++i) { const double d = l[i] - l[i - 1]; hi += d * d; tot += (double) l[i] * l[i]; }
        return hi / tot;
    };
    CHECK (bright (1.0f) > bright (0.0f) * 1.5, "FOLD adds bright overtones");
}

static void testRandomSeed()
{
    // S&H: the seed fixes the pattern — same seed = the same pattern on every note; the dice gives a new one
    auto pattern = [] (int seed, int note)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.filterShape = motion::random; p.motionRate = 1.0f; p.filterRange = 5.0f; p.fEnvAmount = 0.0f;
        p.keyTrack = 0.0f; p.randomSeed = seed;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (note, 1.0f);
        std::vector<float> l (512), r (512), cut;
        for (int b = 0; b < 375; ++b) { eng.render (l.data(), r.data(), 512); if (b % 12 == 6) cut.push_back (eng.uiCutoffHz.load()); }
        return cut;
    };
    const auto a = pattern (1977, 45), b = pattern (1977, 45), c = pattern (42, 45);
    double same = 0, diff = 0;
    for (size_t i = 0; i < a.size(); ++i) { same = std::max (same, (double) std::abs (a[i] - b[i])); diff = std::max (diff, (double) std::abs (a[i] - c[i])); }
    CHECK (same < 1.0 && diff > 100.0, "S&H: the same seed repeats the same pattern, a new seed (dice) gives another");
    // the pattern loops every 4 cycles (16 steps)
    using namespace motion;
    bool loops = true;
    for (int k = 0; k < 16; ++k)
        loops = loops && filterShapeValue (motion::random, (k % 4) * 0.25f + 0.1f, (uint32_t) (k / 4), 7u) == filterShapeValue (motion::random, (k % 4) * 0.25f + 0.1f, (uint32_t) (k / 4 + 4), 7u);
    CHECK (loops, "S&H: a 16-step loop (repeats every 4 cycles)");
}

static void testUnisonCombPhaser()
{
    auto run = [] (EngineParams p, int note = 45)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        p.analogDrift = false;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (note, 1.0f);
        std::vector<float> l (96000), r (96000);
        eng.render (l.data(), r.data(), 96000);
        return std::pair<std::vector<float>, std::vector<float>> (l, r);
    };
    auto db = [] (float a, float b) { return 20.0f * std::log10 (a / b); };

    // UNISON: 7 voices, finite, level close to a single voice, and a real stereo spread
    EngineParams base;
    const auto one = run (base);
    EngineParams u = base; u.unison = 7; u.detune = 0.4f; u.spread = 0.9f;
    const auto seven = run (u);
    double diff = 0;
    for (size_t i = 4800; i < seven.first.size(); ++i) diff += std::abs (seven.first[i] - seven.second[i]);
    const float lvl = db (rms (seven.first.data() + 4800, 91200), rms (one.first.data() + 4800, 91200));
    std::printf ("      unison 7 vs 1: %.1f dB\n", lvl);
    CHECK (allFinite (seven.first) && allFinite (seven.second) && std::abs (lvl) < 4.0f, "UNISON x7: finite, level within 4 dB of one voice");
    CHECK (diff / 91200.0 > 0.01, "UNISON x7: left and right differ (stereo spread)");

    // COMB: finite and bounded at the extremes, level kept sensible
    bool ok = true; float worst = 0;
    for (int mode : { 0, 1, 2 })
    for (float drive : { 0.0f, 1.0f })
    for (float fb : { -0.99f, -0.5f, 0.0f, 0.5f, 0.85f, 0.99f })
        for (float pitchOff : { -24.0f, 0.0f, 36.0f })
            for (float motion : { -1.0f, 1.0f })
            {
                EngineParams c = base; c.combOn = true; c.combFeedback = fb; c.combPitch = pitchOff; c.combMotion = motion;
                c.combMix = 1.0f; c.combDamp = 0.0f; c.combMode = mode; c.combDrive = drive; c.combDepth = 1.0f; c.combLfoHz = 2.0f;
                c.combSpread = 1.0f;
                const auto o = run (c);
                ok = ok && allFinite (o.first);
                worst = std::max (worst, std::abs (db (rms (o.first.data() + 4800, 91200), rms (one.first.data() + 4800, 91200))));
            }
    std::printf ("      comb: largest level change %.1f dB\n", worst);
    CHECK (ok, "COMB: finite at +-99 % feedback, every pitch, motion, mode and drive");
    CHECK (worst < 6.0f, "FLANGE: level stays within 6 dB at every setting (all modes, full DESTROY)");

    // COMB rings at its pitch: an impulse through the comb (no feedback loss) repeats at 1/f
    {
        CombFilter c; c.prepare (48000.0f);
        std::vector<float> y (4000);
        for (int i = 0; i < 4000; ++i) y[(size_t) i] = c.process (i == 0 ? 1.0f : 0.0f, 100.0f, 0.9f, 0.0f, 1.0f, 0.0f, CombFilter::flangePlus);   // 100-sample period
        int peakAt = 50;
        for (int i = 50; i < 150; ++i) if (std::abs (y[(size_t) i]) > std::abs (y[(size_t) peakAt])) peakAt = i;
        CHECK (std::abs (peakAt - 100) <= 1, "COMB: echoes exactly one period of the comb pitch later");
    }
    // NEG FLANGE cancels the lows (the delayed copy is subtracted); DESTROY adds overtones
    {
        auto runComb = [] (float freq, int mode, float drive, float fb)
        {
            CombFilter c; c.prepare (48000.0f);
            std::vector<float> y (48000);
            for (int i = 0; i < 48000; ++i)
                y[(size_t) i] = c.process (0.5f * std::sin (kTwoPi * freq * i / 48000.0f), 24.0f, fb, 0.0f, 1.0f, drive, mode);   // 0.5 ms
            return y;
        };
        const auto lowF = runComb (60.0f, CombFilter::flangeMinus, 0.0f, 0.0f), highF = runComb (1000.0f, CombFilter::flangeMinus, 0.0f, 0.0f);
        const auto lowP = runComb (60.0f, CombFilter::flangePlus, 0.0f, 0.0f);
        CHECK (rms (lowP.data() + 4800, 40000) > 0.3f, "FLANGE +: adds the delayed copy (lows pass, doubled)");
        CHECK (rms (lowF.data() + 4800, 40000) < 0.25f * rms (highF.data() + 4800, 40000), "FLANGE -: subtracts the delayed copy (lows cancel)");
        auto bright = [] (const std::vector<float>& v) { double hi = 0, tot = 0; for (size_t i = 4801; i < v.size(); ++i) { const double d = v[i] - v[i - 1]; hi += d * d; tot += (double) v[i] * v[i]; } return hi / tot; };
        (void) bright;
        // a pure sine through a linear comb stays a pure sine; DESTROY adds harmonics (energy outside the fundamental)
        auto thd = [] (const std::vector<float>& v, float f0)
        {
            double re = 0, im = 0, tot = 0; const int n0 = 4800, n = 38400;   // 38400 = whole cycles of 200 Hz
            for (int i = n0; i < n0 + n; ++i) { const double w = 2.0 * 3.14159265358979 * f0 * i / 48000.0; re += v[(size_t) i] * std::cos (w); im += v[(size_t) i] * std::sin (w); tot += (double) v[(size_t) i] * v[(size_t) i]; }
            const double fund = 2.0 * (re * re + im * im) / n;
            return (tot - fund) / tot;
        };
        const double clean = thd (runComb (200.0f, CombFilter::flangePlus, 0.0f, 0.95f), 200.0f), dirty = thd (runComb (200.0f, CombFilter::flangePlus, 1.0f, 0.95f), 200.0f);
        std::printf ("      destroy: harmonics %.1f%% -> %.1f%%\n", 100 * clean, 100 * dirty);
        CHECK (dirty > clean + 0.05, "DESTROY: distortion in the loop adds harmonics");
    }

    // FLANGE follows the main filter cutoff: its resonant peak moves with CUTOFF
    {
        auto peakHz = [] (float cutoff)
        {
            SquelchEngine eng; eng.prepare (48000.0f);
            EngineParams c; c.analogDrift = false; c.filterRange = 0.0f; c.fEnvAmount = 0.0f; c.keyTrack = 0.0f; c.cutoffHz = cutoff;
            c.slope = 0; c.resonance = 0.0f; c.width = 1.0f;
            c.combOn = true; c.combFeedback = 0.97f; c.combPitch = 0.0f; c.combMix = 1.0f; c.combDamp = 0.0f; c.combDepth = 0.0f; c.combSpread = 0.0f;
            eng.setParams (c); eng.snapSmoothers();
            eng.noteOn (33, 1.0f);
            std::vector<float> l (48000), r (48000);
            eng.render (l.data(), r.data(), 48000);
            // the comb repeats the sound every 1/f: find the strongest autocorrelation lag between 8 and 300 samples
            // (the note itself, A1, repeats only every ~870 samples)
            double best = -1e9; int bestLag = 0;
            for (int lag = 8; lag < 300; ++lag)
            {
                double ac = 0;
                for (int i = 24000; i < 46000; ++i) ac += (double) l[(size_t) i] * l[(size_t) i - (size_t) lag];
                if (ac > best) { best = ac; bestLag = lag; }
            }
            const float bestHz = 48000.0f / (float) bestLag;
            return bestHz;
        };
        const float a = peakHz (500.0f), b = peakHz (2000.0f);
        std::printf ("      flange peak: cutoff 500 -> %.0f Hz, cutoff 2000 -> %.0f Hz\n", a, b);
        CHECK (b > a * 2.5f, "FLANGE: follows the filter cutoff (the peak moves up with CUTOFF)");
    }

    // PHASER: finite, level steady, sweeps (the sound changes over time)
    {
        FxParams fp; fp.delayOn = false; fp.reverbOn = false; fp.phaserOn = true; fp.phaserRate = 0; fp.phaserFeedback = 0.9f;
        FxChain fx; fx.prepare (48000.0f); fx.setParams (fp); fx.setTransport (120.0, false, 0.0);
        Rng rng;
        std::vector<float> l (96000), r (96000), inL (96000);
        for (int i = 0; i < 96000; ++i) { inL[(size_t) i] = l[(size_t) i] = r[(size_t) i] = (rng.next01() * 2.0f - 1.0f) * 0.3f; }
        fx.process (l.data(), r.data(), 96000);
        const float lv = db (rms (l.data() + 4800, 91200), rms (inL.data() + 4800, 91200));
        std::printf ("      phaser level: %.1f dB\n", lv);
        CHECK (allFinite (l) && allFinite (r) && std::abs (lv) < 4.0f, "PHASER: finite, level within 4 dB");
        // brightness changes over the sweep
        auto bright = [&] (int start) { double hi = 0, tot = 0; for (int i = start + 1; i < start + 2000; ++i) { const double d = l[(size_t) i] - l[(size_t) i - 1]; hi += d * d; tot += (double) l[(size_t) i] * l[(size_t) i]; } return hi / tot; };
        double lo = 1e9, hi = 0;
        for (int s0 = 4800; s0 < 90000; s0 += 2000) { const double b = bright (s0); lo = std::min (lo, b); hi = std::max (hi, b); }
        CHECK (hi > lo * 1.3, "PHASER: the notches sweep (tone changes over time)");
    }
}

static void testMotionModes()
{
    // the filter position right after a note starts, for a given mode and history
    auto startMove = [] (int mode, bool playing, double ppq, int idleSamples)
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.motionMode = mode; p.motionRate = 1.0f; p.filterShape = motion::triangle; p.analogDrift = false;
        eng.setParams (p); eng.snapSmoothers();
        std::vector<float> l (512), r (512);
        for (int done = 0; done < idleSamples; done += 512) { eng.setTransport (120.0, playing, ppq); eng.render (l.data(), r.data(), 512); }
        eng.setTransport (120.0, playing, ppq);
        eng.noteOn (45, 1.0f);
        eng.render (l.data(), r.data(), 64);
        return eng.uiFilterMove.load();
    };
    // RETRIG: always starts at the top (1)
    CHECK (startMove (0, false, 0.0, 24000) > 0.99f, "MODE RETRIG: every note starts at the top of the range");
    // FREE: after 0.25 s of silence at 1 Hz the LFO has moved on: triangle at phase 0.25 = 0.5
    const float fr = startMove (1, false, 0.0, 12288);
    std::printf ("      free: note starts at %.2f\n", fr);
    CHECK (fr > 0.35f && fr < 0.65f, "MODE FREE: the LFO kept running between notes");
    // BAR SYNC: song at beat 1.0, 120 BPM, 1 Hz = 2 beats per cycle -> half a cycle -> bottom (0)
    const float bs = startMove (2, true, 1.0, 0);
    std::printf ("      bar sync: note starts at %.2f\n", bs);
    CHECK (bs < 0.05f, "MODE BAR SYNC: the note lands where the song position says");
    // ONE SHOT: after one full cycle the motion holds still
    {
        SquelchEngine eng; eng.prepare (48000.0f);
        EngineParams p; p.motionMode = 3; p.motionRate = 2.0f; p.filterShape = motion::rise; p.analogDrift = false;
        eng.setParams (p); eng.snapSmoothers();
        eng.noteOn (45, 1.0f);
        std::vector<float> l (512), r (512);
        float a = 0, b = 0;
        for (int k = 0; k < 120; ++k) { eng.render (l.data(), r.data(), 512); if (k == 80) a = eng.uiFilterMove.load(); }
        b = eng.uiFilterMove.load();
        std::printf ("      one shot: holds at %.2f / %.2f\n", a, b);
        CHECK (std::abs (a - b) < 1e-4f && a > 0.95f, "MODE ONE SHOT: one sweep, then it holds at the end");
    }
}

int main()
{
    testOscillator();
    testFilterStability();
    testBandPassShape();
    testEnvelope();
    testEngine();
    testMotion();
    testPitchMotion();
    testAcid();
    testPitchShapes();
    testFilterShapes();
    testRandomSeed();
    test303();
    testFoldHarmonics();
    testUnisonCombPhaser();
    testMotionModes();
    testEffects();
    testTube();
    testSync();
    testFm();
    testFormantLevel();
    testFormantLfo();
    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
