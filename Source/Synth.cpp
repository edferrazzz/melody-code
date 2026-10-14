#include "Synth.h"
#include <cmath>

namespace {
constexpr double twoPi = juce::MathConstants<double>::twoPi;
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
}

const char* Synth::presetName(int p) {
    static const char* n[] { "KEYS", "BELLS", "PAD", "PLUCK", "SYNTH", "MALLET" };
    return n[juce::jlimit(0, presetCount - 1, p)];
}

void Synth::prepare(double rate) {
    sr = rate;
    delayBuffer.setSize(2, (int) (sr * 2.0));
    delayBuffer.clear();
    delayWrite = 0;
    reverbParams.roomSize = 0.6f; reverbParams.damping = 0.45f; reverbParams.wetLevel = 0.24f; reverbParams.dryLevel = 1.0f; reverbParams.width = 1.0f;
    reverb.setParameters(reverbParams);
    reverb.setSampleRate(sr);
    character = target;
    allNotesOff();
}

void Synth::setDelayTime(double seconds) {
    delaySamples = juce::jlimit(1, delayBuffer.getNumSamples() - 1, (int) (seconds * sr));
}

void Synth::setCharacter(const Character& c) {
    target = c;
    if (std::abs(c.reverbSize - reverbParams.roomSize) > 0.001f || std::abs(c.reverbWet - reverbParams.wetLevel) > 0.001f) {
        reverbParams.roomSize = c.reverbSize;
        reverbParams.wetLevel = c.reverbWet;
        reverbParams.damping = 0.6f - 0.35f * juce::jlimit(0.0f, 1.5f, c.bright) / 1.5f;
        reverb.setParameters(reverbParams);
    }
}

void Synth::allNotesOff() {
    for (auto& v : voices) v.active = false;
    sub.active = false;
}

void Synth::noteOn(int pitch, int velocity, int lengthSamples, bool, bool isSub) {
    if (isSub) {
        sub.pitch = pitch; sub.freq = hz(pitch);
        if (!sub.active) { sub.env = 0.0f; sub.phase[0] = 0.0; }
        sub.velocity = (float) velocity / 127.0f;
        sub.length = lengthSamples; sub.age = 0; sub.active = true;
        return;
    }
    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
    *v = Voice {};
    v->phase[1] = 0.37; v->phase[2] = 0.71; v->phase[3] = 0.13;
    v->active = true; v->length = lengthSamples;
    v->pitch = pitch; v->preset = preset; v->velocity = 0.55f + 0.45f * (float) velocity / 127.0f;
    v->freq = hz(pitch);
    v->filterEnv = 1.0f;
    v->pan = (panFlip++ % 2 ? 0.18f : -0.18f);   // alternate notes slightly left / right
}

float Synth::renderSub() {
    if (!sub.active) return 0.0f;
    const float tgt = sub.age < sub.length ? sub.velocity : 0.0f;
    const float rate = tgt > sub.env ? (float) (1.0 / (0.004 * sr)) : (float) (1.0 / (0.04 * sr));
    sub.env += juce::jlimit(-rate, rate, tgt - sub.env);
    if (sub.age >= sub.length && sub.env <= 0.0001f) { sub.active = false; return 0.0f; }
    ++sub.age;
    advance(sub.phase[0], sub.freq / sr);
    const double ph = sub.phase[0] * twoPi;
    // sine + a little 2nd/3rd harmonic so it is heard on small speakers, then soft clip
    float s = (float) (std::sin(ph) + 0.22 * std::sin(2.0 * ph) + 0.08 * std::sin(3.0 * ph));
    return std::tanh(1.4f * s) * sub.env * 0.55f;
}

void Synth::renderVoice(Voice& v, float& outL, float& outR, double wobbleRatio) {
    const auto& c = character;
    const double f = v.freq * wobbleRatio;
    const double dt = f / sr;
    const float t = (float) v.age / (float) sr;
    const bool held = v.age < v.length;
    float l = 0.0f, r = 0.0f, amp = 0.0f, cutoff = 0.0f;
    auto envTo = [&](float tgt, float attack, float release) {
        const float k = tgt > v.env ? (float) (1.0 - std::exp(-1.0 / (attack * sr))) : (float) (1.0 - std::exp(-1.0 / (release * sr)));
        v.env += (tgt - v.env) * k;
    };
    const double spread = c.detune;
    switch (v.preset) {
        case keys: {   // electric piano: FM tine with a soft bark on the attack, slow stereo tremolo
            envTo(held ? 0.25f + 0.75f * std::exp(-t * 1.1f) : 0.0f, 0.003f, 0.28f);
            const float index = 0.35f + 1.5f * std::exp(-t * 5.0f);
            advance(v.modPhase, dt);
            advance(v.modPhase2, dt * 14.0);
            const float tine = sine(v.phase[0] + index * sine(v.modPhase) / twoPi * 2.0 + 0.04 * std::exp(-t * 40.0f) * sine(v.modPhase2));
            const float body = sine(v.phase[1]) * 0.35f;
            advance(v.phase[0], dt); advance(v.phase[1], dt * cents(3.0 * spread));
            const float trem = 0.12f * (float) std::sin(twoPi * 4.2 * t);
            const float m = tine + body;
            l = m * (1.0f + trem); r = m * (1.0f - trem);
            cutoff = 5200.0f; amp = 0.2f; break;
        }
        case bells: {  // glassy FM bell with a second inharmonic partial, long ring
            envTo(held || t < 1.6f ? std::exp(-t * 1.4f) : 0.0f, 0.002f, 0.9f);
            const float index = 0.6f + 2.4f * std::exp(-t * 3.0f);
            advance(v.modPhase, dt * 3.5);
            const float a = sine(v.phase[0] + index * sine(v.modPhase) / twoPi * 2.0);
            const float b = sine(v.phase[1]) * 0.3f * std::exp(-t * 2.4f);
            advance(v.phase[0], dt); advance(v.phase[1], dt * 2.0 * cents(4.0 * spread));
            l = a + b; r = a - b * 0.5f;
            cutoff = 9000.0f; amp = 0.19f; break;
        }
        case pad: {    // three detuned saws spread in stereo, slow attack, warm filter that breathes
            envTo(held ? 0.85f : 0.0f, 0.35f, 0.9f);
            const double d = 8.0 * spread;
            const float s0 = saw(v.phase[0], dt), s1 = saw(v.phase[1], dt * cents(d)), s2 = saw(v.phase[2], dt * cents(-d));
            advance(v.phase[0], dt); advance(v.phase[1], dt * cents(d)); advance(v.phase[2], dt * cents(-d));
            l = 0.45f * s0 + 0.55f * s1 + 0.2f * s2;
            r = 0.45f * s0 + 0.2f * s1 + 0.55f * s2;
            cutoff = 1500.0f + 500.0f * (float) std::sin(twoPi * 0.25 * t); amp = 0.16f; break;
        }
        case pluck: {  // detuned saw pluck with a snappy filter envelope
            envTo(held ? std::exp(-t * 3.6f) : 0.0f, 0.002f, 0.12f);
            v.filterEnv *= (float) std::exp(-1.0 / (0.14 * sr));
            const double d = 6.0 * spread;
            const float s0 = saw(v.phase[0], dt * cents(d)), s1 = saw(v.phase[1], dt * cents(-d));
            advance(v.phase[0], dt * cents(d)); advance(v.phase[1], dt * cents(-d));
            l = 0.7f * s0 + 0.3f * s1; r = 0.3f * s0 + 0.7f * s1;
            cutoff = 450.0f + 5200.0f * v.filterEnv; amp = 0.26f; break;
        }
        case synth: {  // smooth lead: saw + square, gentle delayed vibrato
            envTo(held ? 0.8f : 0.0f, 0.012f, 0.15f);
            const double vib = cents(9.0 * std::sin(twoPi * 5.2 * t) * juce::jlimit(0.0f, 1.0f, (t - 0.25f) * 2.0f));
            const double d = 5.0 * spread;
            const float s0 = saw(v.phase[0], dt * vib * cents(d)), s1 = square(v.phase[1], dt * vib * cents(-d)) * 0.6f;
            advance(v.phase[0], dt * vib * cents(d)); advance(v.phase[1], dt * vib * cents(-d));
            l = s0 + 0.5f * s1; r = 0.5f * s0 + s1;
            cutoff = 2400.0f; amp = 0.17f; break;
        }
        case mallet: default: {   // kalimba / marimba-like: sine body with fast-decaying bright partials
            envTo(std::exp(-t * 4.2f), 0.001f, 0.08f);
            const float m = sine(v.phase[0]) + 0.4f * std::exp(-t * 18.0f) * sine(v.phase[1]) + 0.15f * std::exp(-t * 30.0f) * sine(v.phase[2]);
            advance(v.phase[0], dt); advance(v.phase[1], dt * 2.76); advance(v.phase[2], dt * 5.4);
            l = r = m;
            cutoff = 7000.0f; amp = 0.28f;
            if (t > 1.5f) v.active = false;
            break;
        }
    }
    if (c.octaveUp > 0.0f) {   // shimmer layer
        const float o = sine(v.phase[3]) * c.octaveUp * 0.35f * std::exp(-t * 1.5f);
        advance(v.phase[3], dt * 2.0);
        l += o; r += o;
    }
    // two one-pole low-passes per side (12 dB)
    cutoff *= c.bright * (1.0f - 0.35f * c.wobble);
    const float a = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * juce::jlimit(80.0f, (float) sr * 0.45f, cutoff) / (float) sr);
    v.lp[0][0] += a * (l - v.lp[0][0]); v.lp[0][1] += a * (v.lp[0][0] - v.lp[0][1]);
    v.lp[1][0] += a * (r - v.lp[1][0]); v.lp[1][1] += a * (v.lp[1][0] - v.lp[1][1]);
    ++v.age;
    if (!held && v.env < 0.0005f) v.active = false;
    const float g = v.env * v.velocity * amp;
    outL += v.lp[0][1] * g * (1.0f - v.pan);
    outR += v.lp[1][1] * g * (1.0f + v.pan);
}

void Synth::render(float* left, float* right, int n) {
    auto* dl = delayBuffer.getWritePointer(0);
    auto* dr = delayBuffer.getWritePointer(1);
    const int size = delayBuffer.getNumSamples();
    for (int i = 0; i < n; ++i) {
        // glide the character toward the target so a vibe change never clicks
        auto glide = [](float& x, float to) { x += (to - x) * 0.0005f; };
        glide(character.bright, target.bright); glide(character.detune, target.detune);
        glide(character.delayWet, target.delayWet); glide(character.delayFeedback, target.delayFeedback);
        glide(character.drive, target.drive); glide(character.octaveUp, target.octaveUp); glide(character.wobble, target.wobble);

        advance(wobblePhase, 0.55 / sr); advance(wobblePhase2, 0.13 / sr);
        const double wob = cents(character.wobble * (11.0 * std::sin(twoPi * wobblePhase) + 6.0 * std::sin(twoPi * wobblePhase2)));
        float ml = 0.0f, mr = 0.0f;
        for (auto& v : voices) if (v.active) renderVoice(v, ml, mr, wob);
        if (character.drive > 0.001f) {
            const float k = 1.0f + 3.0f * character.drive;
            ml = std::tanh(ml * k) / k * (1.0f + character.drive);
            mr = std::tanh(mr * k) / k * (1.0f + character.drive);
        }
        ml *= melodyLevel; mr *= melodyLevel;
        // ping-pong delay on the melody only, darker on every repeat
        int rd = delayWrite - delaySamples; if (rd < 0) rd += size;
        const float echoL = dl[rd], echoR = dr[rd];
        delayLp += 0.35f * (echoR - delayLp);
        dl[delayWrite] = 0.5f * (ml + mr) + delayLp * character.delayFeedback;
        dr[delayWrite] = echoL * 0.9f;
        if (++delayWrite >= size) delayWrite = 0;
        left[i] = ml + echoL * character.delayWet;
        right[i] = mr + echoR * character.delayWet;
    }
    reverb.processStereo(left, right, n);
    for (int i = 0; i < n; ++i) {
        const float s = renderSub() * subLevel;
        left[i] = std::tanh(left[i] + s);
        right[i] = std::tanh(right[i] + s);
    }
}
