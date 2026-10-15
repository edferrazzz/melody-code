#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// Preview synth. One polyphonic instrument plays both the melody and the low "bass" notes, so the
// bass is the same timbre an octave or two down. A clean effects bus (chorus, tone, drive, tape,
// delay, reverb, limiter) is shaped by the Vibe; the Vibe never changes the notes.
class Synth {
public:
    enum Preset { keys, bells, pad, pluck, lead, mallet, presetCount };
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
    void setLevels(float bass, float melody) { bassLevel = bass; melodyLevel = melody; }
    void setTempo(double bpm) { beatSeconds = 60.0 / juce::jmax(30.0, bpm); }
    void setFx(const Fx& f) { fxTarget = f; }
    void noteOn(int pitch, int velocity, int lengthSamples, bool isBass);
    void allNotesOff();
    void render(float* left, float* right, int numSamples);

private:
    struct Voice {
        bool active {}, bass {};
        int pitch {}, age {}, length {}, preset {};
        float velocity {}, freq {}, env {}, pan {}, gain { 1.0f };
        double phase[8] {};
        float svf[2][2] {};                 // per-side state-variable filter state
        std::vector<float> ks;              // Karplus-Strong string
        int ksPos {}; float ksLast {}; double ksDelay {}; float ksLoss {};
        float noise {};
    };
    void startVoice(Voice&, int pitch, int velocity, int lengthSamples, bool isBass);
    void renderVoice(Voice&, float& l, float& r, double pitchRatio);
    float lowpass(Voice&, int side, float x, float cutoff, float q);

    // effects
    void processFx(float& l, float& r);
    double sr { 44100.0 }, beatSeconds { 0.5 };
    int preset { keys };
    Fx fx, fxTarget;
    std::array<Voice, 32> voices {};
    float bassLevel { 0.8f }, melodyLevel { 0.8f };
    int panFlip {};
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
