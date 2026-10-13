#include "Synth.h"
#include <cmath>

namespace {
constexpr double twoPi = juce::MathConstants<double>::twoPi;
float hz(int midi) { return 440.0f * std::pow(2.0f, (float) (midi - 69) / 12.0f); }
// band-limited saw (polyBLEP), phase in [0,1)
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
void advance(double& ph, double inc) { ph += inc; if (ph >= 1.0) ph -= 1.0; }
}

const char* Synth::presetName(int p) {
    static const char* n[] { "PLUCK", "SYNTH LEAD", "FM BELL", "MARIMBA", "FLUTE" };
    return n[juce::jlimit(0, presetCount - 1, p)];
}

void Synth::prepare(double rate) {
    sr = rate;
    delayBuffer.setSize(2, (int) (sr * 2.0));
    delayBuffer.clear();
    delayWrite = 0;
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f; rp.damping = 0.5f; rp.wetLevel = 0.22f; rp.dryLevel = 1.0f; rp.width = 1.0f;
    reverb.setParameters(rp);
    reverb.setSampleRate(sr);
    allNotesOff();
}

void Synth::setDelayTime(double seconds) {
    delaySamples = juce::jlimit(1, delayBuffer.getNumSamples() - 1, (int) (seconds * sr));
}

void Synth::allNotesOff() {
    for (auto& v : voices) v.active = false;
    sub.active = false;
}

void Synth::noteOn(int pitch, int velocity, int lengthSamples, bool glide, bool isSub) {
    if (isSub) {
        const bool slide = glide && sub.active;
        sub.pitch = pitch; sub.targetFreq = hz(pitch);
        if (!slide) { sub.freq = sub.targetFreq; sub.env = sub.active ? sub.env : 0.0f; sub.phase[0] = sub.active ? sub.phase[0] : 0.0; }
        sub.glideCoef = slide ? (float) (1.0 - std::exp(-1.0 / (0.07 * sr))) : 1.0f;
        sub.velocity = (float) velocity / 127.0f;
        sub.length = lengthSamples; sub.age = 0; sub.released = false; sub.active = true;
        return;
    }
    // glide: reuse the most recent voice so the pitch slides
    Voice* v = nullptr;
    float from = 0.0f;
    if (glide) {
        Voice* newest = nullptr;
        for (auto& x : voices) if (x.active && (!newest || x.age < newest->age)) newest = &x;
        if (newest) { from = newest->freq; v = newest; }
    }
    if (!v) {
        for (auto& x : voices) if (!x.active) { v = &x; break; }
        if (!v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
        *v = Voice {};
        v->phase[1] = 0.37; v->phase[2] = 0.71;
    }
    v->active = true; v->released = false; v->age = 0; v->length = lengthSamples;
    v->pitch = pitch; v->preset = preset; v->velocity = (float) velocity / 127.0f;
    v->targetFreq = hz(pitch);
    v->freq = from > 0.0f ? from : v->targetFreq;
    v->glideCoef = from > 0.0f ? (float) (1.0 - std::exp(-1.0 / (0.05 * sr))) : 1.0f;
    v->filterEnv = 1.0f;
    if (from <= 0.0f) v->env = 0.0f;
}

float Synth::renderSub() {
    if (!sub.active) return 0.0f;
    sub.freq += (sub.targetFreq - sub.freq) * sub.glideCoef;
    const float target = sub.age < sub.length ? sub.velocity : 0.0f;
    const float rate = target > sub.env ? (float) (1.0 / (0.004 * sr)) : (float) (1.0 / (0.04 * sr));
    sub.env += juce::jlimit(-rate, rate, target - sub.env);
    if (sub.age >= sub.length && sub.env <= 0.0001f) { sub.active = false; return 0.0f; }
    ++sub.age;
    advance(sub.phase[0], sub.freq / sr);
    const double ph = sub.phase[0] * twoPi;
    // sine + a little 2nd/3rd harmonic so it is heard on small speakers, then soft clip
    float s = (float) (std::sin(ph) + 0.22 * std::sin(2.0 * ph) + 0.08 * std::sin(3.0 * ph));
    return std::tanh(1.4f * s) * sub.env * 0.55f;
}

float Synth::renderVoice(Voice& v) {
    v.freq += (v.targetFreq - v.freq) * v.glideCoef;
    const double dt = v.freq / sr;
    const float t = (float) v.age / (float) sr;
    const bool held = v.age < v.length;
    float out = 0.0f, amp = 0.0f, cutoff = 0.0f;
    auto envTo = [&](float target, float attack, float release) {
        const float k = target > v.env ? (float) (1.0 - std::exp(-1.0 / (attack * sr))) : (float) (1.0 - std::exp(-1.0 / (release * sr)));
        v.env += (target - v.env) * k;
    };
    switch (v.preset) {
        case pluck: {
            envTo(held ? std::exp(-t * 3.2f) : 0.0f, 0.002f, 0.06f);
            v.filterEnv *= (float) std::exp(-1.0 / (0.12 * sr));
            out = 0.5f * (saw(v.phase[0], dt) + saw(v.phase[1], dt * 1.004));
            advance(v.phase[0], dt); advance(v.phase[1], dt * 1.004);
            cutoff = 400.0f + 5200.0f * v.filterEnv; amp = 0.30f; break;
        }
        case lead: {
            envTo(held ? 0.85f : 0.0f, 0.012f, 0.07f);
            const double vib = 1.0 + 0.004 * std::sin(twoPi * 5.5 * t) * juce::jmin(1.0f, t * 3.0f);
            out = 0.55f * saw(v.phase[0], dt * vib) + 0.35f * square(v.phase[1], dt * vib * 1.003);
            advance(v.phase[0], dt * vib); advance(v.phase[1], dt * vib * 1.003);
            cutoff = 3200.0f; amp = 0.20f; break;
        }
        case bell: {
            envTo(held || t < 1.2f ? std::exp(-t * 2.2f) : 0.0f, 0.002f, 0.3f);
            const float index = 2.2f * std::exp(-t * 2.5f);
            advance(v.modPhase, dt * 3.5);
            out = (float) std::sin(twoPi * v.phase[0] + index * std::sin(twoPi * v.modPhase));
            advance(v.phase[0], dt);
            cutoff = 9000.0f; amp = 0.32f; break;
        }
        case marimba: {
            envTo(std::exp(-t * 7.0f), 0.001f, 0.05f);
            out = (float) (std::sin(twoPi * v.phase[0]) + 0.35 * std::exp(-t * 30.0f) * std::sin(twoPi * v.phase[1]));
            advance(v.phase[0], dt); advance(v.phase[1], dt * 4.0);
            cutoff = 8000.0f; amp = 0.45f;
            if (t > 1.0f) v.active = false;
            break;
        }
        case flute: default: {
            envTo(held ? 0.8f : 0.0f, 0.05f, 0.08f);
            const double vib = 1.0 + 0.003 * std::sin(twoPi * 5.0 * t);
            const float tri = (float) (4.0 * std::abs(v.phase[0] - 0.5) - 1.0);
            advance(v.phase[0], dt * vib);
            const float breath = (random.nextFloat() * 2.0f - 1.0f) * 0.25f * std::exp(-t * 12.0f);
            out = tri + breath;
            cutoff = 4200.0f; amp = 0.34f; break;
        }
    }
    // two one-pole low-passes (12 dB)
    const float a = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * juce::jmin(cutoff, (float) sr * 0.45f) / (float) sr);
    v.lp1 += a * (out - v.lp1); v.lp2 += a * (v.lp1 - v.lp2);
    ++v.age;
    if (!held && v.env < 0.0005f) v.active = false;
    return v.lp2 * v.env * v.velocity * amp;
}

void Synth::render(float* left, float* right, int n) {
    auto* dl = delayBuffer.getWritePointer(0);
    auto* dr = delayBuffer.getWritePointer(1);
    const int size = delayBuffer.getNumSamples();
    for (int i = 0; i < n; ++i) {
        float mel = 0.0f;
        for (auto& v : voices) if (v.active) mel += renderVoice(v);
        mel *= melodyLevel;
        // ping-pong delay on the melody only
        int r = delayWrite - delaySamples; if (r < 0) r += size;
        const float echoL = dl[r], echoR = dr[r];
        delayLp += 0.35f * (echoR - delayLp);
        dl[delayWrite] = mel + delayLp * 0.32f;
        dr[delayWrite] = echoL * 0.9f;
        if (++delayWrite >= size) delayWrite = 0;
        left[i] = mel + echoL * 0.22f;
        right[i] = mel + echoR * 0.22f;
    }
    reverb.processStereo(left, right, n);
    for (int i = 0; i < n; ++i) {
        const float s = renderSub() * subLevel;
        left[i] = std::tanh(left[i] + s);
        right[i] = std::tanh(right[i] + s);
    }
}
