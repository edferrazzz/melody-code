#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include "Tone.h"
#include "Sampler.h"

// Preview synth. One polyphonic instrument plays both the melody and the low "bass" notes, so the
// bass is the same timbre an octave or two down. Each preset (Tone.h) picks a family voice and its
// macros; per-preset polish (low cut, width, ambience amount) feeds a shared effects bus (chorus,
// tone, drive, tape, delay, reverb, limiter) shaped by the Vibe. The Vibe never changes the notes.
class Synth {
public:
    // The first six ids are the original sounds; the Tone Design Engine adds variants after them (Tone.h).
    enum Preset { keys, bells, pad, pluck, lead, mallet };
    static constexpr int presetCount = 25;
    // The sounds offered in SOUND for now: KEYS (electronic piano), PLUCK (square wave), RHODES.
    static constexpr int shownPresets[] { 0, 3, 7 };
    static int shownPreset(int id) { for (int x : shownPresets) if (x == id) return id; return 0; }
    static const char* presetName(int);

    // Effect settings for one Vibe.
    struct Fx {
        float lowpass { 18000.0f }, highpass { 30.0f };   // bus tone, Hz
        float chorus {};                                   // 0..1 depth and mix
        float drive {};                                    // soft saturation, 0..1
        float tape {};                                     // wow/flutter + bit reduction, 0..1
        float octaveUp {};                                 // shimmer layer, 0..1
        float delayMix {}, delayFeedback { 0.35f }, delayBeats { 0.75f };
        float reverbMix { 0.15f }, reverbTime { 1.5f }, reverbDamp { 0.4f }, preDelayMs { 10.0f };
    };

    void prepare(double sampleRate);
    void setPreset(int p) { preset = juce::jlimit(0, presetCount - 1, p); }
    // Optional macro override for the current preset (tone::vary); nullptr returns to the preset's own macros.
    void setMacros(const tone::Macros* m) { if (m) { macroOverride = *m; useOverride = true; } else useOverride = false; }
    void setLevels(float bass, float melody) { bassLevel = bass; melodyLevel = melody; sampler.setLevels(bass, melody); }
    // A sample kit (Sampler.h) replaces the synth voices; -1 goes back to the synth presets.
    // A synthesized kit (Kit::synth) plays a Tone preset on the synth voices instead.
    void setKit(int k) {
        if (k >= 0 && Sampler::kit(k).synth >= 0) { kit = -1; setPreset(Sampler::kit(k).synth); return; }
        kit = k; if (k >= 0) sampler.setKit(k);
    }
    void setTempo(double bpm) { beatSeconds = 60.0 / juce::jmax(30.0, bpm); }
    void setFx(const Fx& f) { fxTarget = f; }
    // layer: 0 melody, 1 bass, 2 chords (only the sample kits tell the layers apart)
    void noteOn(int pitch, int velocity, int lengthSamples, bool isBass, int layer = -1);
    void allNotesOff();             // short fade, no click
    void render(float* left, float* right, int numSamples);
    int activeVoices() const;

private:
    enum Stage { attack, decay, release, kill };
    struct Voice {
        bool active {}, bass {};
        int pitch {}, age {}, length {}, preset {};
        const tone::Preset* tp {};
        tone::Macros m {};
        Stage stage { attack };
        float velocity {}, velAmp {}, bright {}, freq {}, freqNow {}, glideK {}, env {}, pan {}, gain { 1.0f };
        float atkK {}, decK {}, relK {}, sustain {}, decaySec {};
        double phase[8] {};
        float svf[2][2] {};                 // per-side state-variable filter state
        std::vector<float> ks;              // Karplus-Strong string
        int ksPos {}; float ksLast {}; double ksDelay {}; float ksLoss {}, ksDamp {};
        float noise {}, noise2 {};
    };
    void startVoice(Voice&, int pitch, int velocity, int lengthSamples, bool isBass);
    void renderVoice(Voice&, float& l, float& r, double pitchRatio);
    float lowpass(Voice&, int side, float x, float cutoff, float q);
    float white() { return random.nextFloat() * 2.0f - 1.0f; }

    // effects
    void processFx(float& l, float& r);
    double sr { 44100.0 }, beatSeconds { 0.5 };
    int preset { keys };
    int kit { -1 };
    Sampler sampler;
    Fx fx, fxTarget;
    std::array<Voice, 32> voices {};
    float bassLevel { 0.8f }, melodyLevel { 0.8f };
    int panFlip {};
    tone::Macros macroOverride {}; bool useOverride {};
    float lastMelodyFreq {}; int sinceMelody { 1 << 30 };
    // per-preset polish on the dry signal, smoothed so preset changes do not click
    float cutNow { 40.0f }, widthNow { 1.0f }, hpState[2] {}, hp2State[2] {};
    Fx effective(const Fx& vibe) const;
    juce::Random random;

    std::vector<float> chorusBuf[2]; int chorusPos {}; double chorusPhase {};
    std::vector<float> delayBuf[2]; int delayPos {}; float delayTone[2] {};
    std::vector<float> preBuf; int prePos {};
    struct FdnState {
        static constexpr int lines = 8;
        std::array<std::vector<float>, lines> buf;
        std::array<int, lines> len {}, pos {};
        std::array<float, lines> damp {};
        std::array<std::vector<float>, 4> ap; std::array<int, 4> apPos {};
        double modPhase {};
    } fdn;
    float toneL[2] {}, toneH[2] {}, hpPrev[2] {};
    double wowPhase {}, flutterPhase {};
    float crushHold[2] {}; int crushCount {};
    float limiterEnv {};
    double delaySamplesNow { 1000.0 };
    void updateReverb();
};
