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

const char* Synth::presetName(int p) {
    static const char* n[] { "KEYS", "BELLS", "PAD", "PLUCK", "LEAD", "MALLET" };
    return n[juce::jlimit(0, presetCount - 1, p)];
}

void Synth::prepare(double rate) {
    sr = rate;
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
    for (auto& v : voices) v.active = false;
}

void Synth::noteOn(int pitch, int velocity, int lengthSamples, bool isBass) {
    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
    startVoice(*v, pitch, velocity, lengthSamples, isBass);
}

void Synth::startVoice(Voice& v, int pitch, int velocity, int lengthSamples, bool isBass) {
    v.active = true; v.bass = isBass;
    v.pitch = pitch; v.age = 0; v.length = juce::jmax(1, lengthSamples); v.preset = preset;
    v.velocity = (float) velocity / 127.0f;
    v.freq = hz(pitch);
    v.env = 0.0f;
    v.gain = isBass ? 0.9f : 1.0f;
    v.pan = isBass ? 0.0f : (panFlip++ % 2 ? 0.15f : -0.15f);
    for (auto& p : v.phase) p = random.nextDouble();
    v.phase[0] = 0.0;
    for (auto& s : v.svf) s[0] = s[1] = 0.0f;
    v.noise = 0.0f;
    if (preset == pluck) {   // Karplus-Strong: fill the string with a burst of filtered noise
        std::fill(v.ks.begin(), v.ks.end(), 0.0f);
        v.ksDelay = juce::jlimit(2.0, (double) v.ks.size() - 3.0, sr / v.freq - 0.5);
        const float bright = 0.35f + 0.5f * v.velocity;
        float lp = 0.0f;
        const int n = (int) std::ceil(v.ksDelay) + 2;
        v.ksPos = n;
        for (int i = 0; i < n; ++i) { lp += bright * ((random.nextFloat() * 2.0f - 1.0f) - lp); v.ks[(size_t) i] = lp; }
        v.ksLast = 0.0f;
        const float t60 = juce::jlimit(0.8f, 4.0f, 3.2f - (float) (pitch - 48) * 0.04f);
        v.ksLoss = std::pow(0.001f, 1.0f / (t60 * v.freq));
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
    const double f = v.freq * pitchRatio;
    const double dt = f / sr;
    const float t = (float) v.age / (float) sr;
    const bool held = v.age < v.length;
    const float vel = v.velocity;
    float l = 0.0f, r = 0.0f, amp = 0.0f;
    auto env = [&](float tgt, float attack, float release) {
        const float k = tgt > v.env ? 1.0f - std::exp(-1.0f / (attack * (float) sr)) : 1.0f - std::exp(-1.0f / (release * (float) sr));
        v.env += (tgt - v.env) * k;
    };
    switch (v.preset) {
        case keys: {   // electric piano: FM with a velocity "bark", a short tine and a slow decay
            const float decay = juce::jlimit(0.35f, 2.0f, 0.8f + (float) (v.pitch - 60) * 0.03f);
            env(held ? std::exp(-t * decay) : 0.0f, 0.002f, 0.32f);
            const float bark = (0.25f + 1.6f * vel) * std::exp(-t * 3.5f) + 0.2f;
            const float tine = 0.6f * vel * std::exp(-t * 22.0f);
            const double ph = v.phase[0];
            const float s = (float) std::sin(twoPi * ph + bark * std::sin(twoPi * v.phase[1]) + tine * std::sin(twoPi * v.phase[2]));
            const float s2 = sine(v.phase[3]) * 0.22f;
            advance(v.phase[0], dt); advance(v.phase[1], dt); advance(v.phase[2], dt * 14.0); advance(v.phase[3], dt * cents(4.0));
            l = s + s2; r = s - s2 * 0.5f;
            amp = 0.20f; break;
        }
        case bells: {  // celesta / glass bell: FM partial plus two clean inharmonic partials
            env(held || t < 0.25f ? std::exp(-t * 1.3f) : 0.0f, 0.0015f, 0.9f);
            const float index = 0.15f + 1.3f * vel * std::exp(-t * 2.5f);
            const float a = (float) std::sin(twoPi * v.phase[0] + index * std::sin(twoPi * v.phase[1]));
            const float b = sine(v.phase[2]) * 0.3f * std::exp(-t * 2.8f);
            const float c = sine(v.phase[3]) * 0.12f * std::exp(-t * 6.0f);
            advance(v.phase[0], dt); advance(v.phase[1], dt * 3.5); advance(v.phase[2], dt * 2.0); advance(v.phase[3], dt * 4.2);
            l = a + b + c; r = a + b * 0.6f + c * 1.4f;
            amp = 0.16f; break;
        }
        case pad: {    // five detuned saws spread left/right, a sine an octave down, warm breathing filter
            env(held ? 1.0f : 0.0f, 0.45f, 1.2f);
            static const double det[] { 0.0, 7.0, -7.0, 14.0, -14.0 };
            float sl = 0.0f, sr2 = 0.0f;
            for (int i = 0; i < 5; ++i) {
                const double d = dt * cents(det[i]);
                const float s = saw(v.phase[i], d);
                advance(v.phase[i], d);
                const float w = i == 0 ? 0.7f : 1.0f;
                sl += s * w * (i % 2 ? 1.0f : 0.55f);
                sr2 += s * w * (i % 2 ? 0.55f : 1.0f);
            }
            const float sub = v.bass ? 0.0f : sine(v.phase[5]) * 0.3f;   // body an octave down, never on the bass notes
            advance(v.phase[5], dt * 0.5);
            const float cutoff = 1700.0f + 500.0f * (float) std::sin(twoPi * 0.18 * t + v.pitch);
            l = lowpass(v, 0, sl * 0.25f + sub, cutoff, 0.6f);
            r = lowpass(v, 1, sr2 * 0.25f + sub, cutoff, 0.6f);
            amp = 0.45f; break;
        }
        case pluck: {  // plucked string (Karplus-Strong) with a little sine body for warmth
            env(held ? 1.0f : 0.0f, 0.001f, 0.18f);
            const float y = readFrac(v.ks, v.ksPos, v.ksDelay);
            const float next = v.ksLoss * 0.5f * (y + v.ksLast);
            v.ksLast = y;
            v.ks[(size_t) v.ksPos] = next;
            if (++v.ksPos >= (int) v.ks.size()) v.ksPos = 0;
            const float body = sine(v.phase[0]) * 0.18f * std::exp(-t * 2.5f);
            advance(v.phase[0], dt);
            l = r = y + body;
            amp = 0.6f;
            if (t > 5.0f) v.active = false;
            break;
        }
        case lead: {   // soft analog lead: two saws and a square through an enveloped low-pass, delayed vibrato
            env(held ? 0.9f : 0.0f, 0.01f, 0.2f);
            const double vib = cents(8.0 * std::sin(twoPi * 5.0 * t) * juce::jlimit(0.0f, 1.0f, (t - 0.3f) * 2.0f));
            const double d = dt * vib;
            const float a = saw(v.phase[0], d * cents(5)), b = saw(v.phase[1], d * cents(-5)), c = square(v.phase[2], d * (v.bass ? 1.0 : 0.5));
            advance(v.phase[0], d * cents(5)); advance(v.phase[1], d * cents(-5)); advance(v.phase[2], d * (v.bass ? 1.0 : 0.5));
            const float cutoff = 900.0f + (1500.0f + 2500.0f * vel) * std::exp(-t * 3.0f) + 900.0f;
            l = lowpass(v, 0, 0.5f * a + 0.3f * b + 0.25f * c, cutoff, 0.8f);
            r = lowpass(v, 1, 0.3f * a + 0.5f * b + 0.25f * c, cutoff, 0.8f);
            amp = 0.30f; break;
        }
        case mallet: default: {   // marimba bar: three modes with the bar's ratios, soft mallet click
            env(std::exp(-t * 3.2f), 0.0008f, 0.06f);
            const float m = sine(v.phase[0]) + 0.3f * std::exp(-t * 11.0f) * sine(v.phase[1]) + 0.1f * std::exp(-t * 24.0f) * sine(v.phase[2]);
            advance(v.phase[0], dt); advance(v.phase[1], dt * 3.99); advance(v.phase[2], dt * 10.65);
            v.noise += 0.3f * ((random.nextFloat() * 2.0f - 1.0f) - v.noise);
            const float click = v.noise * 0.25f * std::exp(-t * 400.0f);
            l = r = m + click;
            amp = 0.30f;
            if (t > 2.0f) v.active = false;
            break;
        }
    }
    if (fx.octaveUp > 0.001f && !v.bass) {   // shimmer layer an octave up
        const float o = sine(v.phase[7]) * fx.octaveUp * 0.25f * std::exp(-t * 1.2f);
        advance(v.phase[7], dt * 2.0);
        l += o; r += o;
    }
    ++v.age;
    if (!held && v.env < 0.0003f) v.active = false;
    const float g = v.env * (0.35f + 0.65f * vel) * amp * v.gain * (v.bass ? bassLevel : melodyLevel);
    outL += l * g * (1.0f - v.pan);
    outR += r * g * (1.0f + v.pan);
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
    // move the effect settings toward the vibe's target (no clicks when the vibe changes)
    const float k = 1.0f - std::exp(-(float) n / (0.08f * (float) sr));
    auto glide = [k](float& a, float b) { a += (b - a) * k; };
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
        processFx(l, r);
        // peak limiter: instant attack, smooth release, ceiling -1 dBFS
        const float peak = std::max(std::abs(l), std::abs(r));
        limiterEnv = peak > limiterEnv ? peak : limiterEnv * rel + peak * (1.0f - rel);
        const float gain = limiterEnv > 0.89f ? 0.89f / limiterEnv : 1.0f;
        left[i] = l * gain;
        right[i] = r * gain;
    }
}
