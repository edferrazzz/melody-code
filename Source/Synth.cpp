#include "Synth.h"
#include <cmath>

namespace {
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr float pi = juce::MathConstants<float>::pi;
float hz(int midi) { return 440.0f * std::pow(2.0f, (float) (midi - 69) / 12.0f); }
// band-limited saw / square (polyBLEP), phase in [0,1)
float blep(double t, double dt) {
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
float saw(double ph, double dt) { return (float) (2.0 * ph - 1.0) - blep(ph, dt); }
float square(double ph, double dt) {
    double ph2 = ph + 0.5; if (ph2 >= 1.0) ph2 -= 1.0;
    return (ph < 0.5 ? 1.0f : -1.0f) + blep(ph, dt) - blep(ph2, dt);
}
float sine(double ph) { return (float) std::sin(twoPi * ph); }
void advance(double& ph, double inc) { ph += inc; ph -= std::floor(ph); }
double cents(double c) { return std::pow(2.0, c / 1200.0); }
float onePole(float cutoff, double sr) { return 1.0f - std::exp(-2.0f * pi * cutoff / (float) sr); }
float readFrac(const std::vector<float>& buf, int writePos, double delay) {
    const int size = (int) buf.size();
    double rp = writePos - delay;
    while (rp < 0) rp += size;
    const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
    const float f = (float) (rp - std::floor(rp));
    return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
}
}

const char* Synth::presetName(int p) { return tone::get(p).name; }
static_assert(Synth::presetCount == 25, "keep in step with the table in Tone.cpp");

void Synth::prepare(double rate) {
    sr = rate;
    sampler.prepare(rate);
    random.setSeed(0x4d43);   // repeatable renders
    const int ksSize = (int) (sr / 25.0) + 4;
    for (auto& v : voices) { v.ks.assign((size_t) ksSize, 0.0f); v.active = false; }
    for (auto& b : chorusBuf) b.assign((size_t) (sr * 0.05) + 4, 0.0f);
    for (auto& b : delayBuf) b.assign((size_t) (sr * 2.5) + 4, 0.0f);
    preBuf.assign((size_t) (sr * 0.1) + 4, 0.0f);
    static const int lens[] { 1427, 1637, 1871, 2053, 2311, 2539, 2777, 3011 };
    static const int apLens[] { 142, 107, 379, 277 };
    for (int i = 0; i < FdnState::lines; ++i) {
        fdn.len[(size_t) i] = (int) (lens[i] * sr / 44100.0);
        fdn.buf[(size_t) i].assign((size_t) fdn.len[(size_t) i] + 2, 0.0f);
        fdn.pos[(size_t) i] = 0; fdn.damp[(size_t) i] = 0.0f;
    }
    for (int i = 0; i < 4; ++i) { fdn.ap[(size_t) i].assign((size_t) (apLens[i] * sr / 44100.0) + 1, 0.0f); fdn.apPos[(size_t) i] = 0; }
    fx = fxTarget;
    limiterEnv = 0.0f;
    delaySamplesNow = fx.delayBeats * beatSeconds * sr;
}

void Synth::allNotesOff() {
    for (auto& v : voices) if (v.active) v.stage = kill;
    sampler.allNotesOff();
}

int Synth::activeVoices() const {
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n + sampler.activeVoices();
}

void Synth::noteOn(int pitch, int velocity, int lengthSamples, bool isBass, int layer) {
    if (kit >= 0) { sampler.noteOn(pitch, velocity, lengthSamples, layer < 0 ? (isBass ? 1 : 0) : layer); return; }
    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) {   // steal the quietest released voice, else the oldest
        for (auto& x : voices) if (x.stage >= release && (!v || x.env < v->env)) v = &x;
        if (!v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
    }
    startVoice(*v, pitch, velocity, lengthSamples, isBass);
}

namespace {
float timeScale(float macro, float range) { return std::pow(2.0f, (macro - 0.5f) * range); }   // 0.5 = as tuned
float coef(float seconds, double sr) { return std::exp(-6.9f / (juce::jmax(0.0005f, seconds) * (float) sr)); }  // -60 dB in `seconds`
}

void Synth::startVoice(Voice& v, int pitch, int velocity, int lengthSamples, bool isBass) {
    const auto& P = tone::get(preset);
    v.tp = &P; v.m = useOverride ? macroOverride : P.macros;
    v.active = true; v.bass = isBass;
    v.pitch = pitch; v.age = 0; v.length = juce::jmax(1, lengthSamples); v.preset = preset;
    v.velocity = juce::jlimit(0.0f, 1.0f, (float) velocity / 127.0f);
    // velocity: loudness through a per-family curve (soft notes stay audible), brightness separately
    v.velAmp = 0.22f + 0.78f * std::pow(v.velocity, P.perf.velCurve);
    v.bright = (1.0f - P.perf.velBright) * 0.65f + P.perf.velBright * v.velocity;
    v.freq = hz(pitch);
    v.freqNow = v.freq; v.glideK = 1.0f;
    if (!isBass && P.perf.glideMs > 0 && lastMelodyFreq > 0 && sinceMelody < (int) sr) {   // slide from the previous note
        v.freqNow = lastMelodyFreq;
        v.glideK = 1.0f - std::exp(-1.0f / (P.perf.glideMs * 0.001f * (float) sr));
    }
    if (!isBass) { lastMelodyFreq = v.freq; sinceMelody = 0; }
    v.env = 0.0f; v.stage = attack;
    v.gain = isBass ? 0.9f : 1.0f;
    v.pan = isBass ? 0.0f : (panFlip++ % 2 ? 0.15f : -0.15f);
    // layers at the same pitch start in phase (random phases would make every note a different loudness);
    // only detuned stacks start free, so they do not all line up into one click
    for (auto& p : v.phase) p = 0.0;
    if (P.family == tone::Family::pad || (P.family == tone::Family::lead && (P.algo == 0 || P.algo == 3)))
        for (int i = 1; i < 5; ++i) v.phase[i] = random.nextDouble();
    for (auto& s : v.svf) s[0] = s[1] = 0.0f;
    v.noise = v.noise2 = 0.0f;

    // envelope: the preset's times, scaled by the attack / release macros, decay key-tracked
    const auto& m = v.m;
    const float keyTrack = juce::jlimit(0.4f, 2.2f, std::pow(2.0f, -(float) (pitch - 60) / 24.0f));
    const float atk = P.env.attack * timeScale(1.0f - m.attack, 2.0f);
    v.decaySec = P.env.decay * (P.env.sustain > 0 ? 1.0f : keyTrack * timeScale(m.release, 1.4f));
    v.atkK = 1.0f - std::exp(-1.0f / (atk * (float) sr));
    v.decK = coef(v.decaySec, sr);
    v.relK = coef(P.env.release * timeScale(m.release, 2.0f), sr);
    v.sustain = P.env.sustain;

    const bool ks = P.family == tone::Family::pluck && (P.algo == 0 || P.algo == 2);
    if (ks) {   // Karplus-Strong: a burst of filtered noise, shaped by where the string is picked
        std::fill(v.ks.begin(), v.ks.end(), 0.0f);
        v.ksDelay = juce::jlimit(2.0, (double) v.ks.size() - 3.0, sr / v.freq - 0.5);
        const float soft = P.algo == 2 ? 0.45f : 1.0f;
        const float bright = soft * (0.2f + 0.6f * (0.5f * m.attack + 0.5f * v.bright));
        float lp = 0.0f;
        const int n = (int) std::ceil(v.ksDelay) + 2;
        v.ksPos = n;
        for (int i = 0; i < n; ++i) { lp += bright * (white() - lp); v.ks[(size_t) i] = lp; }
        const int pick = juce::jmax(1, (int) (v.ksDelay * (0.06 + 0.3 * (1.0 - m.character))));   // pick position: comb on the burst
        for (int i = n - 1; i >= pick; --i) v.ks[(size_t) i] -= 0.85f * v.ks[(size_t) (i - pick)];
        v.ksLast = 0.0f;
        const float t60 = juce::jlimit(0.4f, 5.0f, v.decaySec * keyTrack * (0.6f + 0.8f * m.body));
        v.ksLoss = std::pow(0.001f, 1.0f / (t60 * v.freq));
        v.ksDamp = P.algo == 2 ? 0.65f : 0.5f;   // felt: darker loop filter
        v.sustain = 1.0f;                         // the string decays by itself, the envelope only gates
    }
}

float Synth::lowpass(Voice& v, int side, float x, float cutoff, float q) {
    // TPT state-variable low-pass (stable at any cutoff)
    const float g = std::tan(pi * juce::jlimit(20.0f, (float) sr * 0.45f, cutoff) / (float) sr);
    const float k = 1.0f / q;
    auto& s = v.svf[side];
    const float v1 = (s[0] + g * (x - s[1])) / (1.0f + g * (g + k));
    const float v2 = s[1] + g * v1;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    return v2;
}

void Synth::renderVoice(Voice& v, float& outL, float& outR, double pitchRatio) {
    const auto& P = *v.tp;
    const auto& m = v.m;
    if (v.glideK < 1.0f) v.freqNow += (v.freq - v.freqNow) * v.glideK;
    const float t = (float) v.age / (float) sr;
    double f = v.freqNow * pitchRatio;
    if (P.perf.vibratoCents > 0 && !v.bass)   // delayed, gentle vibrato: only presets that ask for it
        f *= cents(P.perf.vibratoCents * (0.5 + m.movement) * std::sin(twoPi * P.perf.vibratoRate * t) * juce::jlimit(0.0f, 1.0f, (t - 0.25f) * 2.5f));
    const double dt = f / sr;
    const float br = v.bright;
    const float c = m.character, b = m.body, a = m.attack, mo = m.movement, tx = m.texture;
    float l = 0.0f, r = 0.0f, amp = 0.2f;

    // amplitude envelope
    if (v.stage < release && v.age >= v.length) v.stage = release;
    switch (v.stage) {
        case attack: v.env += (1.25f - v.env) * v.atkK; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = decay; } break;
        case decay: v.env = v.sustain + (v.env - v.sustain) * v.decK; break;
        case release: v.env *= v.relK; break;
        case kill: v.env *= 0.985f; break;
    }

    switch (P.family) {
        case tone::Family::keys: {   // electric piano family: FM tine / bark, body in the low mids
            const float bark = (0.15f + (0.4f + 2.0f * c) * br) * std::exp(-t * (2.5f + 3.0f * a)) + 0.1f + 0.3f * c * br;
            const float tine = (0.1f + 0.9f * a) * br * std::exp(-t * 24.0f);
            float s;
            if (P.algo == 3) {   // modern: two-operator stack plus a detuned twin carrier
                const float mod = std::sin(twoPi * v.phase[1] + 0.9f * c * br * std::exp(-t * 6.0f) * std::sin(twoPi * v.phase[2]));
                s = 0.6f * (float) std::sin(twoPi * v.phase[0] + 1.2f * bark * mod) + 0.4f * (float) std::sin(twoPi * v.phase[3] + bark * mod);
                advance(v.phase[1], dt); advance(v.phase[2], dt * 3.0); advance(v.phase[3], dt * cents(6.0));
            } else {
                const double tineRatio = P.algo == 1 ? 7.0 : 14.0;
                const float k = P.algo == 1 ? 0.45f : 1.0f;
                s = (float) std::sin(twoPi * v.phase[0] + k * bark * std::sin(twoPi * v.phase[1]) + k * tine * std::sin(twoPi * v.phase[2]));
                if (P.algo == 1) s += 0.12f * sine(v.phase[3]);
                advance(v.phase[1], dt); advance(v.phase[2], dt * tineRatio); advance(v.phase[3], dt * 2.0);
            }
            const float keyFade = juce::jlimit(0.3f, 1.0f, (float) (v.pitch - 36) / 24.0f);   // less body low down: no mud
            s += sine(v.phase[4]) * (0.1f + 0.35f * b) * keyFade;
            advance(v.phase[0], dt); advance(v.phase[4], dt * cents(3.0 * mo));
            if (tx > 0.01f) { const float d = 1.0f + 4.0f * tx; s = std::tanh(s * d) / std::tanh(d); }
            l = r = s;
            if (P.algo == 2 || mo > 0.4f) {   // stereo tremolo (suitcase), depth from movement
                const float lfo = (float) std::sin(twoPi * 4.8 * t);
                const float depth = 0.45f * mo;
                l *= 1.0f - depth * (0.5f + 0.5f * lfo); r *= 1.0f - depth * (0.5f - 0.5f * lfo);
            }
            amp = 0.2f; break;
        }
        case tone::Family::bells: {   // FM / additive bells; upper partials always die faster than the fundamental
            const float index = (0.2f + 1.8f * c) * (0.35f + 0.65f * br) * std::exp(-t * (2.2f + 1.5f * (1.0f - m.release)));
            float s = 0.0f;
            switch (P.algo) {
                case 1:   // soft: harmonic, music box
                    s = (float) std::sin(twoPi * v.phase[0] + 0.5f * index * std::sin(twoPi * v.phase[1]))
                      + 0.3f * sine(v.phase[2]) * std::exp(-t * 3.0f) + 0.08f * c * sine(v.phase[3]) * std::exp(-t * 5.0f);
                    advance(v.phase[1], dt); advance(v.phase[2], dt * 2.0); advance(v.phase[3], dt * 3.0); break;
                case 2:   // metal: dense inharmonic
                    s = (float) std::sin(twoPi * v.phase[0] + 1.3f * index * std::sin(twoPi * v.phase[1]))
                      + 0.35f * sine(v.phase[2]) * std::exp(-t * 2.2f) + 0.2f * tx * sine(v.phase[3]) * std::exp(-t * 5.0f);
                    advance(v.phase[1], dt * 1.414); advance(v.phase[2], dt * 2.76); advance(v.phase[3], dt * 5.4); break;
                case 3: { // glass / bowl: additive partials
                    s = sine(v.phase[0]) + 0.5f * (0.5f + c) * sine(v.phase[1]) * std::exp(-t * 1.8f)
                      + (0.05f + 0.25f * c) * sine(v.phase[2]) * std::exp(-t * 3.2f) + 0.12f * tx * sine(v.phase[3]) * std::exp(-t * 5.5f);
                    advance(v.phase[1], dt * 2.32); advance(v.phase[2], dt * 4.25); advance(v.phase[3], dt * 6.63); break;
                }
                default:  // crystal (0) and atmos (4)
                    s = (float) std::sin(twoPi * v.phase[0] + (P.algo == 4 ? 0.7f : 1.0f) * index * std::sin(twoPi * v.phase[1]))
                      + 0.25f * (0.5f + b) * sine(v.phase[2]) * std::exp(-t * 2.8f) + 0.18f * tx * sine(v.phase[3]) * std::exp(-t * 7.0f);
                    advance(v.phase[1], dt * 3.5); advance(v.phase[2], dt * 2.0); advance(v.phase[3], dt * 5.43); break;
            }
            advance(v.phase[0], dt);
            const float body = 0.3f * b * sine(v.phase[4]);
            advance(v.phase[4], dt);
            const float twin = 0.35f * mo * sine(v.phase[5]);   // beating partner: slow shimmer
            advance(v.phase[5], dt * cents(2.0 + 5.0 * mo));
            v.noise += 0.35f * (white() - v.noise);
            const float strike = v.noise * 0.25f * a * br * std::exp(-t * 600.0f);
            const float cut = juce::jmin(14000.0f, (float) f * 16.0f + 2000.0f);   // tame the top
            l = lowpass(v, 0, s + body + twin + strike, cut, 0.7f);
            r = lowpass(v, 1, s + body - twin + strike, cut, 0.7f);
            amp = 0.17f; break;
        }
        case tone::Family::pad: {   // detuned stacks (or FM glass) through a slowly opening, breathing filter
            const double spread = 5.0 + 14.0 * mo;
            static const double det[] { 0.0, 1.0, -1.0, 2.0, -2.0 };
            float sl = 0.0f, sr2 = 0.0f;
            for (int i = 0; i < 5; ++i) {
                const double d = dt * cents(det[i] * spread * 0.5);
                float s;
                if (P.algo == 2) s = (float) std::sin(twoPi * v.phase[i] + (0.4f + 0.8f * c) * (0.5f + 0.5f * (float) std::sin(twoPi * 0.13 * t + i)) * std::sin(twoPi * v.phase[i] * 2.0));
                else if (P.algo == 3) s = square(v.phase[i], d) * 0.7f;
                else s = saw(v.phase[i], d);
                advance(v.phase[i], d);
                const float w = i == 0 ? 0.7f : 1.0f;
                sl += s * w * (i % 2 ? 1.0f : 0.55f);
                sr2 += s * w * (i % 2 ? 0.55f : 1.0f);
            }
            const float sub = v.bass || v.pitch < 55 ? 0.0f : sine(v.phase[5]) * 0.3f * b;   // body an octave down, never on bass notes
            advance(v.phase[5], dt * 0.5);
            v.noise += 0.5f * (white() - v.noise);
            const float air = (v.noise - v.noise2) * tx * (P.algo == 1 ? 0.5f : 0.2f); v.noise2 = v.noise;   // breath / air layer
            const float base = P.algo == 1 ? 2500.0f + 5000.0f * c : P.algo == 3 ? 400.0f + 1600.0f * c : P.algo == 2 ? 6000.0f : 900.0f + 2600.0f * c;
            const float swell = 0.6f + 0.4f * juce::jmin(1.0f, t / (P.env.attack * 2.0f + 0.01f));
            const float lfo = 1.0f + 0.3f * mo * (float) std::sin(twoPi * (0.12 + 0.2 * mo) * t + v.pitch);
            const float key = juce::jlimit(0.6f, 1.8f, std::pow(2.0f, (float) (v.pitch - 60) / 24.0f));
            const float cutoff = base * swell * lfo * key * (0.8f + 0.4f * br);
            l = lowpass(v, 0, sl * 0.25f + sub + air, cutoff, 0.65f);
            r = lowpass(v, 1, sr2 * 0.25f + sub - air, cutoff, 0.65f);
            if (tx > 0.2f) { const float d = 1.0f + 2.0f * tx; l = std::tanh(l * d) / d * 1.3f; r = std::tanh(r * d) / d * 1.3f; }
            amp = P.algo == 2 ? 0.3f : 0.45f; break;
        }
        case tone::Family::pluck: {
            float s;
            if (P.algo == 0 || P.algo == 2) {   // string: Karplus-Strong, damped harder once the note is released
                const float y = readFrac(v.ks, v.ksPos, v.ksDelay);
                const float loss = v.stage >= release ? v.ksLoss * 0.995f : v.ksLoss;
                const float next = loss * ((1.0f - v.ksDamp) * y + v.ksDamp * v.ksLast);
                v.ksLast = y;
                v.ks[(size_t) v.ksPos] = next;
                if (++v.ksPos >= (int) v.ks.size()) v.ksPos = 0;
                s = y + sine(v.phase[0]) * (0.08f + 0.3f * b) * std::exp(-t * 3.0f);
                v.noise += 0.5f * (white() - v.noise);
                s += v.noise * 0.12f * a * std::exp(-t * 800.0f);   // pick click
                amp = P.algo == 2 ? 0.75f : 0.6f;
                if (t > 6.0f) v.stage = kill;
            } else if (P.algo == 1) {   // synth pluck: saw + square into a resonant filter that snaps shut
                const double d2 = dt * cents(6.0 * mo);
                const float osc = saw(v.phase[1], dt) + 0.5f * c * square(v.phase[2], d2);
                advance(v.phase[1], dt); advance(v.phase[2], d2);
                const float fenv = std::exp(-t / (0.05f + 0.25f * (1.0f - a)));
                const float cutoff = (float) f * 1.5f + 8000.0f * br * (0.4f + 0.6f * c) * fenv;
                s = lowpass(v, 0, osc * 0.5f, cutoff, 0.7f + 1.5f * c);
                s += sine(v.phase[0]) * 0.25f * b;
                amp = 0.62f;
            } else if (P.algo == 4) {   // square wave pluck: a slightly detuned pulse pair through a snapping low-pass
                const double d2 = dt * cents(4.0 + 6.0 * mo);
                const float osc = square(v.phase[1], dt) + 0.35f * square(v.phase[2], d2);
                advance(v.phase[1], dt); advance(v.phase[2], d2);
                const float fenv = std::exp(-t / (0.04f + 0.2f * (1.0f - a)));
                const float cutoff = (float) f * 2.0f + 6000.0f * br * (0.35f + 0.65f * c) * fenv;
                s = lowpass(v, 0, osc * 0.4f, cutoff, 0.8f + 0.8f * c);
                s += sine(v.phase[0]) * 0.2f * b;
                amp = 0.5f;
            } else {   // digital: FM with a fast index drop
                const float idx = (1.0f + 3.0f * c * br) * std::exp(-t * (12.0f + 12.0f * a));
                s = (float) std::sin(twoPi * v.phase[0] + idx * std::sin(twoPi * v.phase[1])) + 0.2f * b * sine(v.phase[3]);
                advance(v.phase[1], dt * 2.0); advance(v.phase[3], dt * 0.5);
                amp = 0.3f;
            }
            advance(v.phase[0], dt);
            if (tx > 0.05f) { const float d = 1.0f + 3.0f * tx; s = std::tanh(s * d) / std::tanh(d); }
            l = r = s; break;
        }
        case tone::Family::lead: {
            const float fenv = std::exp(-t * (3.0f + 4.0f * a));
            if (P.algo == 1) {   // pulse with slow width movement
                const float pw = juce::jlimit(0.08f, 0.5f, 0.5f - 0.3f * c + 0.15f * mo * (float) std::sin(twoPi * 0.7 * t));
                double ph2 = v.phase[0] + pw; ph2 -= std::floor(ph2);
                const float pulse = saw(v.phase[0], dt) - saw(ph2, dt);
                const float cutoff = 1200.0f + 3000.0f * br * (0.5f + c) * (0.5f + 0.5f * fenv);
                l = r = lowpass(v, 0, pulse * 0.5f, cutoff, 0.8f);
                amp = 0.33f;
            } else if (P.algo == 2) {   // breathy flute-like: sines plus band-limited breath, stronger at the onset
                const float tone_ = sine(v.phase[0]) + 0.25f * c * sine(v.phase[1]) + 0.08f * sine(v.phase[2]);
                advance(v.phase[1], dt * 2.0); advance(v.phase[2], dt * 3.0);
                const float breath = lowpass(v, 0, white(), (float) f * 3.0f, 0.9f) * (0.12f + 0.3f * tx) * (1.0f + 2.0f * std::exp(-t * 20.0f));
                l = r = tone_ + breath;
                amp = 0.3f;
            } else if (P.algo == 3) {   // focused saw stack
                const double sp = 4.0 + 10.0 * mo;
                const float s = saw(v.phase[0], dt) + 0.7f * saw(v.phase[1], dt * cents(sp)) + 0.7f * saw(v.phase[2], dt * cents(-sp));
                advance(v.phase[1], dt * cents(sp)); advance(v.phase[2], dt * cents(-sp));
                const float cutoff = 1800.0f + 5000.0f * br * (0.5f + c) * (0.4f + 0.6f * fenv);
                float y = lowpass(v, 0, s * 0.3f, cutoff, 0.75f);
                if (tx > 0.05f) { const float d = 1.0f + 3.0f * tx; y = std::tanh(y * d) / std::tanh(d) * 0.8f; }
                l = r = y;
                amp = 0.32f;
            } else {   // soft analog: two saws and a square through an enveloped low-pass
                const float x = saw(v.phase[1], dt * cents(5)), y = saw(v.phase[2], dt * cents(-5)), z = square(v.phase[3], dt * (v.bass ? 1.0 : 0.5));
                advance(v.phase[1], dt * cents(5)); advance(v.phase[2], dt * cents(-5)); advance(v.phase[3], dt * (v.bass ? 1.0 : 0.5));
                const float cutoff = 1800.0f + (1500.0f + 2500.0f * br) * (0.5f + c) * fenv;
                l = lowpass(v, 0, 0.5f * x + 0.3f * y + 0.25f * z, cutoff, 0.8f);
                r = lowpass(v, 1, 0.3f * x + 0.5f * y + 0.25f * z, cutoff, 0.8f);
                amp = 0.3f;
            }
            advance(v.phase[0], dt);
            break;
        }
        case tone::Family::mallet: default: {   // struck bars: modal partials with their own decays and a mallet click
            static const double ratios[3][3] { { 1.0, 3.99, 10.65 }, { 1.0, 3.92, 9.9 }, { 1.0, 6.27, 17.6 } };
            const auto& R = ratios[juce::jlimit(0, 2, P.algo)];
            float s = sine(v.phase[0]) * (0.8f + 0.3f * b)
                    + (0.15f + 0.35f * c) * std::exp(-t * 11.0f) * sine(v.phase[1])
                    + 0.1f * br * std::exp(-t * 24.0f) * sine(v.phase[2]);
            advance(v.phase[0], dt); advance(v.phase[1], dt * R[1]); advance(v.phase[2], dt * R[2]);
            v.noise += 0.3f * (white() - v.noise);
            s += v.noise * (0.1f + 0.35f * a) * br * std::exp(-t * 400.0f);
            if (P.algo == 1) s *= 1.0f - 0.5f * mo * (0.5f + 0.5f * (float) std::sin(twoPi * 5.5 * t));   // vibraphone motor
            if (P.algo == 2 && tx > 0.05f) { const float d = 1.0f + 3.0f * tx * std::exp(-t * 6.0f); s = std::tanh(s * d) / std::tanh(d); }   // tine buzz
            l = r = s;
            amp = 0.3f; break;
        }
    }
    if (fx.octaveUp > 0.001f && !v.bass) {   // shimmer layer an octave up (Vibe)
        const float o = sine(v.phase[7]) * fx.octaveUp * 0.25f * std::exp(-t * 1.2f);
        advance(v.phase[7], dt * 2.0);
        l += o; r += o;
    }
    ++v.age;
    if (v.stage >= decay && v.env < 0.0002f && (v.stage != decay || v.sustain == 0.0f)) v.active = false;
    // 0.7: headroom for melody + chords + bass together, so the limiter only catches peaks
    const float g = 0.7f * v.env * v.velAmp * amp * P.level * v.gain * (v.bass ? bassLevel : melodyLevel);
    outL += l * g * (1.0f - v.pan);
    outR += r * g * (1.0f + v.pan);
}

// The Vibe's bus, adjusted by the preset: how much ambience a family takes (space macro), extra chorus,
// and pre-delay for families whose attack must stay defined.
Synth::Fx Synth::effective(const Fx& vibe) const {
    const auto& P = tone::get(preset);
    const float s = (useOverride ? macroOverride : P.macros).space;
    Fx e = vibe;
    float rev = 1.0f, del = 1.0f, pre = 0.0f, damp = 0.0f;
    switch (P.family) {
        case tone::Family::keys:   rev = 0.35f + 1.0f * s; del = 0.4f + 0.8f * s; pre = 25.0f * s; break;
        case tone::Family::bells:  rev = 0.5f + 1.1f * s;  del = 0.5f + 0.8f * s; damp = 0.1f; break;
        case tone::Family::pad:    rev = 0.6f + 1.0f * s;  del = 0.3f + 0.6f * s; break;
        case tone::Family::pluck:  rev = 0.35f + 0.8f * s; del = 0.6f + 1.0f * s; pre = 10.0f * s; break;
        case tone::Family::lead:   rev = 0.3f + 0.8f * s;  del = 0.5f + 0.8f * s; pre = 30.0f * s; break;
        case tone::Family::mallet: rev = 0.4f + 0.9f * s;  del = 0.4f + 0.8f * s; pre = 15.0f * s; break;
    }
    e.reverbMix = juce::jlimit(0.0f, 0.7f, vibe.reverbMix * rev);
    e.delayMix = juce::jlimit(0.0f, 0.5f, vibe.delayMix * del);
    e.preDelayMs = juce::jlimit(0.0f, 90.0f, vibe.preDelayMs + pre);
    e.reverbDamp = juce::jlimit(0.0f, 0.9f, vibe.reverbDamp + damp);
    e.chorus = juce::jlimit(0.0f, 1.0f, vibe.chorus + P.polish.chorus);
    return e;
}

void Synth::updateReverb() {
    for (int i = 0; i < FdnState::lines; ++i) fdn.damp[(size_t) i] = juce::jlimit(0.0f, 0.95f, fx.reverbDamp);
}

void Synth::processFx(float& l, float& r) {
    float x[2] { l, r };
    // tone: gentle high-pass and low-pass
    const float hpA = onePole(fx.highpass, sr), lpA = onePole(fx.lowpass, sr);
    for (int c = 0; c < 2; ++c) {
        toneH[c] += hpA * (x[c] - toneH[c]);
        x[c] -= toneH[c];
        toneL[c] += lpA * (x[c] - toneL[c]);
        x[c] = toneL[c] + (x[c] - toneL[c]) * 0.0f;
    }
    // warm saturation, level-matched
    if (fx.drive > 0.001f) {
        // unity gain for quiet signals, only the peaks get rounded off (no loudness jump, no clipping)
        const float k = 1.0f + 4.0f * fx.drive, wet = 0.7f * fx.drive / 0.35f;
        for (auto& s : x) s += (std::tanh(s * k) / k - s) * std::min(1.0f, wet);
    }
    // tape: lower resolution and a touch of hiss
    if (fx.tape > 0.001f) {
        const int hold = 1 + (int) (fx.tape * 2.0f);
        const float steps = std::pow(2.0f, 14.0f - 5.0f * fx.tape);
        if (++crushCount >= hold) {
            crushCount = 0;
            for (int c = 0; c < 2; ++c) crushHold[c] = std::round(x[c] * steps) / steps;
        }
        for (int c = 0; c < 2; ++c) x[c] = crushHold[c] + (random.nextFloat() - 0.5f) * 0.0025f * fx.tape;
    }
    // chorus: two modulated taps, one per side
    {
        const int size = (int) chorusBuf[0].size();
        const double base = 0.011 * sr, depth = 0.0025 * sr * fx.chorus;
        advance(chorusPhase, 0.35 / sr);
        float wet[2];
        for (int c = 0; c < 2; ++c) {
            chorusBuf[c][(size_t) chorusPos] = x[c];
            const double d = base + depth * std::sin(twoPi * (chorusPhase + c * 0.25));
            wet[c] = readFrac(chorusBuf[c], chorusPos, d);
        }
        if (++chorusPos >= size) chorusPos = 0;
        const float mix = 0.5f * fx.chorus;
        x[0] = x[0] * (1.0f - mix * 0.5f) + wet[1] * mix;
        x[1] = x[1] * (1.0f - mix * 0.5f) + wet[0] * mix;
    }
    // ping-pong delay, darker on every repeat
    {
        const int size = (int) delayBuf[0].size();
        const double want = juce::jlimit(100.0, (double) size - 4.0, fx.delayBeats * beatSeconds * sr);
        delaySamplesNow += (want - delaySamplesNow) * 0.0005;
        const float eL = readFrac(delayBuf[0], delayPos, delaySamplesNow);
        const float eR = readFrac(delayBuf[1], delayPos, delaySamplesNow);
        const float a = onePole(3200.0f, sr);
        delayTone[0] += a * (eL - delayTone[0]);
        delayTone[1] += a * (eR - delayTone[1]);
        delayBuf[0][(size_t) delayPos] = 0.5f * (x[0] + x[1]) + delayTone[1] * fx.delayFeedback;
        delayBuf[1][(size_t) delayPos] = delayTone[0] * fx.delayFeedback;
        if (++delayPos >= size) delayPos = 0;
        x[0] += eL * fx.delayMix;
        x[1] += eR * fx.delayMix;
    }
    // reverb: pre-delay, input diffusion, 8-line feedback delay network with damping
    {
        const int psize = (int) preBuf.size();
        preBuf[(size_t) prePos] = 0.5f * (x[0] + x[1]);
        const int pd = juce::jlimit(1, psize - 2, (int) (fx.preDelayMs * 0.001 * sr));
        float in = preBuf[(size_t) ((prePos - pd + psize) % psize)];
        if (++prePos >= psize) prePos = 0;
        for (int i = 0; i < 4; ++i) {   // allpass diffusers
            auto& b = fdn.ap[(size_t) i];
            auto& p = fdn.apPos[(size_t) i];
            const float d = b[(size_t) p];
            const float y = -0.6f * in + d;
            b[(size_t) p] = in + 0.6f * y;
            if (++p >= (int) b.size()) p = 0;
            in = y;
        }
        std::array<float, FdnState::lines> o {};
        float sum = 0.0f;
        for (int i = 0; i < FdnState::lines; ++i) {
            auto& b = fdn.buf[(size_t) i];
            const int L = fdn.len[(size_t) i];
            o[(size_t) i] = b[(size_t) ((fdn.pos[(size_t) i] - L + (int) b.size()) % (int) b.size())];
            sum += o[(size_t) i];
        }
        const float house = 2.0f / FdnState::lines * sum;
        float rl = 0.0f, rr = 0.0f;
        for (int i = 0; i < FdnState::lines; ++i) {
            const int L = fdn.len[(size_t) i];
            const float g = std::pow(10.0f, -3.0f * (float) L / (fx.reverbTime * (float) sr));
            float y = (o[(size_t) i] - house) * g;
            auto& dmp = fdn.damp[(size_t) i];
            dmp += (1.0f - fx.reverbDamp) * (y - dmp);
            y = dmp;
            auto& b = fdn.buf[(size_t) i];
            b[(size_t) fdn.pos[(size_t) i]] = y + in * 0.35f;
            if (++fdn.pos[(size_t) i] >= (int) b.size()) fdn.pos[(size_t) i] = 0;
            (i % 2 ? rr : rl) += o[(size_t) i];
        }
        x[0] += rl * 0.3f * fx.reverbMix;
        x[1] += rr * 0.3f * fx.reverbMix;
    }
    l = x[0]; r = x[1];
}

void Synth::render(float* left, float* right, int n) {
    // move the effect settings toward the vibe's (and preset's) target: no clicks on changes
    const float k = 1.0f - std::exp(-(float) n / (0.08f * (float) sr));
    auto glide = [k](float& a, float b) { a += (b - a) * k; };
    const Fx fxTarget = effective(this->fxTarget);
    const auto& P = tone::get(preset);
    glide(cutNow, P.polish.lowCut); glide(widthNow, P.polish.width);
    sinceMelody += n;
    const float hpA = onePole(cutNow, sr);
    glide(fx.lowpass, fxTarget.lowpass); glide(fx.highpass, fxTarget.highpass); glide(fx.chorus, fxTarget.chorus);
    glide(fx.drive, fxTarget.drive); glide(fx.tape, fxTarget.tape); glide(fx.octaveUp, fxTarget.octaveUp);
    glide(fx.delayMix, fxTarget.delayMix); glide(fx.delayFeedback, fxTarget.delayFeedback); fx.delayBeats = fxTarget.delayBeats;
    glide(fx.reverbMix, fxTarget.reverbMix); glide(fx.reverbTime, fxTarget.reverbTime); glide(fx.reverbDamp, fxTarget.reverbDamp);
    glide(fx.preDelayMs, fxTarget.preDelayMs);

    const float rel = std::exp(-1.0f / (0.12f * (float) sr));
    for (int i = 0; i < n; ++i) {
        advance(wowPhase, 0.5 / sr); advance(flutterPhase, 5.5 / sr);
        const double ratio = fx.tape > 0.001f ? cents(fx.tape * (9.0 * std::sin(twoPi * wowPhase) + 2.5 * std::sin(twoPi * flutterPhase))) : 1.0;
        float l = 0.0f, r = 0.0f;
        for (auto& v : voices) if (v.active) renderVoice(v, l, r, ratio);
        if (kit >= 0) sampler.render(l, r);
        // preset polish: 12 dB/oct low cut (keeps the low end clean), then stereo width
        float x[2] { l, r };
        for (int c = 0; c < 2; ++c) {
            hpState[c] += hpA * (x[c] - hpState[c]); x[c] -= hpState[c];
            hp2State[c] += hpA * (x[c] - hp2State[c]); x[c] -= hp2State[c];
        }
        const float mid = 0.5f * (x[0] + x[1]), side = 0.5f * (x[0] - x[1]) * widthNow;
        l = mid + side; r = mid - side;
        processFx(l, r);
        // peak limiter: instant attack, smooth release, ceiling -1 dBFS
        const float peak = std::max(std::abs(l), std::abs(r));
        limiterEnv = peak > limiterEnv ? peak : limiterEnv * rel + peak * (1.0f - rel);
        const float gain = limiterEnv > 0.89f ? 0.89f / limiterEnv : 1.0f;
        left[i] = l * gain;
        right[i] = r * gain;
    }
}
