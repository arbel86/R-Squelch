// synth_dsp.h — compact, framework-agnostic synth building blocks (C++17, header-only)
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace sd {
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

inline float mtof(float note) { return 440.f * std::exp2((note - 69.f) / 12.f); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float fastTanh(float x) { x = std::clamp(x, -3.f, 3.f); return x * (27.f + x * x) / (27.f + 9.f * x * x); }

// ---------- parameter smoothing (one-pole, ~tau seconds) ----------
struct Smoother {
    float y = 0, target = 0, a = 0;
    void setup(float fs, float tauSec) { a = 1.f - std::exp(-1.f / (tauSec * fs)); }
    void set(float t) { target = t; }
    void reset(float v) { y = target = v; }
    float next() { y += (target - y) * a; return y; }
};

// ---------- PolyBLEP oscillator (saw / pulse / tri / sine) ----------
inline float polyblep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
struct Osc {
    enum Wave { Sine, Saw, Pulse, Tri } wave = Saw;
    float phase = 0, inc = 0, pw = 0.5f, triState = -1.f;
    bool wrapped = false;                       // true on the sample the phase wrapped (sync master)
    void setFreq(float hz, float fs) { inc = std::min(hz / fs, 0.49f); }
    void reset(float p = 0) { phase = p; triState = -1.f; }
    // hard sync: call master.next() first, then slave.syncTo(master) before slave.next().
    // Sub-sample-accurate reset; the jump itself is not BLEP-corrected -> run sync voices oversampled (2-4x).
    void syncTo(const Osc& m) { if (m.wrapped && m.inc > 0) phase = std::fmod(m.phase * (inc / m.inc), 1.f); }
    float next() {
        const float t = phase, dt = inc;
        float out = 0;
        switch (wave) {
            case Sine: out = std::sin(kTwoPi * t); break;
            case Saw:  out = 2.f * t - 1.f - polyblep(t, dt); break;
            case Pulse: case Tri: {
                const float w = wave == Tri ? 0.5f : pw;
                float sq = t < w ? 1.f : -1.f;
                sq += polyblep(t, dt);
                sq -= polyblep(std::fmod(t + 1.f - w, 1.f), dt);
                if (wave == Pulse) { out = sq; break; }
                triState = 4.f * dt * sq + (1.f - 0.01f * dt) * triState;   // leaky-integrated BLEP square
                out = triState; break;
            }
        }
        phase += inc; wrapped = phase >= 1.f;
        if (wrapped) phase -= 1.f;
        return out;
    }
};

// ---------- noise: white / pink (Kellet) / brown ----------
struct Noise {
    uint32_t s = 22222; float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0, br=0;
    float white() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s * 2.3283064e-10f) * 2.f - 1.f; }
    float pink() { float w = white();
        b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f; b2=0.96900f*b2+w*0.1538520f;
        b3=0.86650f*b3+w*0.3104856f; b4=0.55000f*b4+w*0.5329522f; b5=-0.7616f*b5-w*0.0168980f;
        float p=b0+b1+b2+b3+b4+b5+b6+w*0.5362f; b6=w*0.115926f; return p*0.11f; }
    float brown() { br = (br + 0.02f * white()) / 1.02f; return br * 3.5f; }
};

// ---------- TPT state-variable filter (Zavalishin/Simper): LP/BP/HP/Notch/Peak ----------
struct SVF {
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set(float fc, float res01, float fs) {          // res01: 0..1 (1 = near self-osc)
        fc = std::clamp(fc, 10.f, fs * 0.49f);
        g = std::tan(kPi * fc / fs); k = 2.f - 1.98f * std::clamp(res01, 0.f, 1.f);
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void process(float v0) {
        float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    float bpNorm() const { return k * bp; }               // unity gain at centre
    float notch() const { return lp + hp; }
};

// ---------- 4-pole ZDF ladder (Moog-style) with tanh drive ----------
struct Ladder {
    float s[4] = {0,0,0,0}, G = 0, beta = 1, k = 0, drive = 1;
    void set(float fc, float res01, float fs) {
        fc = std::clamp(fc, 10.f, fs * 0.45f);
        float g = std::tan(kPi * fc / fs); G = g / (1.f + g); beta = 1.f / (1.f + g);
        k = 4.f * std::clamp(res01, 0.f, 1.05f);           // 4 = self-oscillation
    }
    float process(float x) {
        float G2 = G * G, G4 = G2 * G2;
        float S = beta * (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]);
        float y4 = (G4 * x + S) / (1.f + k * G4);
        float u = fastTanh(drive * ((1.f + 0.5f * k) * x - k * y4));   // passband-loss compensation + saturation
        for (float& st : s) { float v = (u - st) * G; float y = v + st; st = y + v; u = y; }
        return u;
    }
};

// ---------- one-pole (TPT) LP/HP: DC blocking, tone, feedback-loop damping ----------
struct OnePole {
    float s = 0, G = 0;
    void set(float fc, float fs) { float g = std::tan(kPi * std::min(fc, fs * 0.49f) / fs); G = g / (1.f + g); }
    float lp(float x) { float v = (x - s) * G, y = v + s; s = y + v; return y; }
    float hp(float x) { return x - lp(x); }
};

// ---------- ADSR with RC-style exponential segments (EarLevel/Redmon form) ----------
struct ADSR {
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float out = 0, sus = 0.7f, aC=0, aB=0, dC=0, dB=0, rC=0, rB=0;
    float ratioA = 0.3f, ratioDR = 0.0001f;              // larger = more linear
    static float coef(float samples, float ratio) { return samples <= 0 ? 0.f : std::exp(-std::log((1.f + ratio) / ratio) / samples); }
    void set(float a, float d, float s, float r, float fs) {
        sus = s;
        aC = coef(a * fs, ratioA);  aB = (1.f + ratioA) * (1.f - aC);
        dC = coef(d * fs, ratioDR); dB = (sus - ratioDR) * (1.f - dC);
        rC = coef(r * fs, ratioDR); rB = -ratioDR * (1.f - rC);
    }
    void gateOn(bool resetToZero = false) { if (resetToZero) out = 0; stage = Attack; }  // default: retrigger from current level (no click)
    void gateOff() { if (stage != Idle) stage = Release; }
    bool active() const { return stage != Idle; }
    float next() {
        switch (stage) {
            case Attack:  out = aB + out * aC; if (out >= 1.f) { out = 1.f; stage = Decay; } break;
            case Decay:   out = dB + out * dC; if (out <= sus) { out = sus; stage = Sustain; } break;
            case Sustain: out = sus; break;
            case Release: out = rB + out * rC; if (out <= 1e-5f) { out = 0; stage = Idle; } break;
            default: break;
        }
        return out;
    }
};

// ---------- LFO: shapes, S&H, smooth random, delay+fade, free/retrigger ----------
struct LFO {
    enum Shape { Sine, Tri, SawUp, SawDown, Square, SampleHold, SmoothRandom } shape = Sine;
    float phase = 0, inc = 0, held = 0, prevHeld = 0, delaySmp = 0, fadeSmp = 0, age = 0;
    Noise rng;
    void setRate(float hz, float fs) { inc = hz / fs; }
    void setRateSync(float bpm, float beats, float fs) { inc = (bpm / 60.f) / beats / fs; }  // beats: 0.25 = 1/16 note
    void retrigger(float startPhase = 0) { phase = startPhase; age = 0; }
    float next() {                                        // bipolar -1..1
        float v = 0, p = phase;
        switch (shape) {
            case Sine: v = std::sin(kTwoPi * p); break;
            case Tri: v = 4.f * std::fabs(p - 0.5f) - 1.f; break;
            case SawUp: v = 2.f * p - 1.f; break;
            case SawDown: v = 1.f - 2.f * p; break;
            case Square: v = p < 0.5f ? 1.f : -1.f; break;
            case SampleHold: v = held; break;
            case SmoothRandom: v = prevHeld + (held - prevHeld) * (0.5f - 0.5f * std::cos(kPi * p)); break;
        }
        phase += inc;
        if (phase >= 1.f) { phase -= 1.f; prevHeld = held; held = rng.white(); }
        float env = 1.f; age += 1.f;                      // delayed vibrato / fade-in
        if (age < delaySmp) env = 0.f; else if (fadeSmp > 0) env = std::min(1.f, (age - delaySmp) / fadeSmp);
        return v * env;
    }
};

// ---------- FM/PM operator (DX-style phase modulation with averaged self-feedback) ----------
struct Operator {
    float phase = 0, inc = 0, fb = 0, y1 = 0, y2 = 0;
    void setFreq(float hz, float fs) { inc = hz / fs; }
    float next(float pmInput /*radians*/) {
        float self = fb * 0.5f * (y1 + y2);               // averaging tames feedback noise
        float y = std::sin(kTwoPi * phase + pmInput + self);
        y2 = y1; y1 = y;
        phase += inc; phase -= std::floor(phase);
        return y;
    }
};

// ---------- waveshapers (run at 2-8x oversampling) ----------
inline float softClip(float x)  { return fastTanh(x); }
inline float hardClip(float x)  { return std::clamp(x, -1.f, 1.f); }
inline float sineFold(float x)  { return std::sin(x * kPi * 0.5f); }               // drive >1 folds
inline float asymSat(float x, float bias) { return fastTanh(x + bias) - fastTanh(bias); }  // even harmonics
inline float chebyshev(float x, int n) { float t0 = 1, t1 = x; if (n == 0) return 1; for (int i = 1; i < n; ++i) { float t2 = 2*x*t1 - t0; t0 = t1; t1 = t2; } return t1; }

// ---------- bitcrusher / sample-rate reducer ----------
struct Crusher {
    float hold = 0, cnt = 0;
    float process(float x, float bits, float downsample /*>=1*/) {
        cnt += 1.f; if (cnt >= downsample) { cnt -= downsample; float q = std::exp2(bits - 1.f); hold = std::round(x * q) / q; }
        return hold;
    }
};

// ---------- fractional delay line (linear + cubic Hermite read) ----------
struct DelayLine {
    std::vector<float> buf; int w = 0;
    void init(int maxSamples) { buf.assign(maxSamples + 4, 0.f); w = 0; }
    void push(float x) { buf[w] = x; if (++w >= (int)buf.size()) w = 0; }
    float at(int d) const { int i = w - 1 - d; while (i < 0) i += (int)buf.size(); return buf[i]; }
    float readLinear(float d) const { int i = (int)d; float f = d - i; return at(i) + f * (at(i + 1) - at(i)); }
    float readCubic(float d) const { int i = (int)d; float f = d - i;
        float xm1 = at(i - 1 < 0 ? 0 : i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
        float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0; }
};

// ---------- Karplus-Strong plucked string ----------
struct Pluck {
    DelayLine dl; float period = 100, damp = 0.5f, decay = 0.996f, last = 0; Noise n;
    void init(float fs) { dl.init((int)(fs / 20.f)); }
    void trigger(float hz, float fs, float brightness01) {
        period = fs / hz - 0.5f;                          // -0.5: group delay of the averaging filter
        OnePole tone; tone.set(200.f + brightness01 * 12000.f, fs);
        for (int i = 0; i < (int)period + 2; ++i) dl.push(tone.lp(n.white()));
    }
    float next() {
        float y = dl.readLinear(period - 1.f);
        float f = decay * ((1.f - damp) * y + damp * last); last = y;
        dl.push(f); return y;
    }
};

// ---------- first-order allpass + phaser ----------
struct Allpass1 { float x1 = 0, y1 = 0, a = 0;
    void set(float fc, float fs) { float t = std::tan(kPi * fc / fs); a = (t - 1.f) / (t + 1.f); }
    float process(float x) { float y = a * x + x1 - a * y1; x1 = x; y1 = y; return y; } };
struct Phaser {
    Allpass1 ap[8]; int stages = 6; float fbk = 0.5f, last = 0;
    float process(float x, float centreHz, float fs) {   // modulate centreHz with an LFO (exponentially)
        for (int i = 0; i < stages; ++i) ap[i].set(centreHz * (1.f + 0.3f * i), fs);
        float y = x + fbk * last;
        for (int i = 0; i < stages; ++i) y = ap[i].process(y);
        last = y; return 0.5f * (x + y);                  // notches where phase = 180°
    }
};

// ---------- chorus / flanger (modulated delay) ----------
struct ModDelay {
    DelayLine dl; float fb = 0, lastOut = 0;
    void init(float fs) { dl.init((int)(fs * 0.05f)); }
    // chorus: base 7-20 ms, depth 1-5 ms, rate 0.2-1 Hz, fb 0;  flanger: base 0.5-5 ms, fb +-0.5..0.9
    float process(float x, float delaySamples) { dl.push(x + fb * lastOut); lastOut = dl.readCubic(delaySamples); return lastOut; }
};

// ---------- feedback delay with damping (tempo-syncable, ping-pong by cross-feeding two) ----------
struct EchoDelay {
    DelayLine dl; OnePole damp; OnePole lowcut; float fb = 0.45f;
    void init(float fs, float maxSec) { dl.init((int)(fs * maxSec)); damp.set(5000.f, fs); lowcut.set(150.f, fs); }
    float process(float x, float delaySamples) {
        float y = dl.readCubic(delaySamples);
        dl.push(x + fb * lowcut.hp(damp.lp(y)));          // filtering inside loop: repeats darken & thin
        return y;
    }
};

// ---------- small FDN reverb (4 lines, Hadamard mixing, damped) ----------
struct FDNReverb {
    DelayLine d[4]; OnePole lp[4]; float len[4] = {1557, 1617, 1491, 1422}, decay = 0.85f;
    void init(float fs, float size = 1.f, float dampHz = 6000.f) {
        for (int i = 0; i < 4; ++i) { len[i] *= size * fs / 44100.f; d[i].init((int)len[i] + 8); lp[i].set(dampHz, fs); } }
    void process(float inL, float inR, float& outL, float& outR) {
        float o[4]; for (int i = 0; i < 4; ++i) o[i] = lp[i].lp(d[i].at((int)len[i]));
        float h0 = o[0]+o[1]+o[2]+o[3], h1 = o[0]-o[1]+o[2]-o[3], h2 = o[0]+o[1]-o[2]-o[3], h3 = o[0]-o[1]-o[2]+o[3];
        float g = 0.5f * decay;
        d[0].push(inL + g * h0); d[1].push(inR + g * h1); d[2].push(inL + g * h2); d[3].push(inR + g * h3);
        outL = 0.5f * (o[0] + o[2]); outR = 0.5f * (o[1] + o[3]);
    }
};

// ---------- glide in pitch (log) domain: constant-time portamento ----------
struct Glide {
    float note = 60, target = 60, a = 1;
    void setTime(float sec, float fs) { a = sec <= 0 ? 1.f : 1.f - std::exp(-1.f / (sec * fs / 4.6f)); }  // ~99% in 'sec'
    void to(float n, bool jump) { target = n; if (jump) note = n; }
    float next() { note += (target - note) * a; return note; }
};

// ---------- mono note stack with last/low/high priority ----------
struct NoteStack {
    std::vector<int> held; enum Prio { Last, Low, High } prio = Last;
    void on(int n) { off(n); held.push_back(n); }
    void off(int n) { held.erase(std::remove(held.begin(), held.end(), n), held.end()); }
    bool empty() const { return held.empty(); }
    int current() const {
        if (prio == Low)  return *std::min_element(held.begin(), held.end());
        if (prio == High) return *std::max_element(held.begin(), held.end());
        return held.back();
    }
};

inline float flushDenormal(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }
} // namespace sd
