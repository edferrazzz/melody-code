#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Preview synth: a mono sub voice plus a small polyphonic melody synth with a few presets.
class Synth {
public:
    enum Preset { pluck, lead, bell, marimba, flute, presetCount };
    static const char* presetName(int);

    void prepare(double sampleRate);
    void setPreset(int p) { preset = juce::jlimit(0, presetCount - 1, p); }
    void setLevels(float sub, float melody) { subLevel = sub; melodyLevel = melody; }
    void setDelayTime(double seconds);
    void noteOn(int pitch, int velocity, int lengthSamples, bool glide, bool isSub);
    void allNotesOff();
    void render(float* left, float* right, int numSamples);

private:
    struct Voice {
        bool active {};
        int pitch {}, age {}, length {}, preset {};
        float velocity {}, freq {}, targetFreq {}, glideCoef { 1.0f };
        double phase[3] {}, modPhase {};
        float env {}, filterEnv {}, lp1 {}, lp2 {}, noise {};
        bool released {};
    };
    float renderVoice(Voice&);
    float renderSub();
    double sr { 44100.0 };
    int preset { pluck };
    std::array<Voice, 16> voices {};
    Voice sub {};
    float subLevel { 0.8f }, melodyLevel { 0.8f };
    juce::AudioBuffer<float> delayBuffer;
    int delayWrite {}, delaySamples { 1 };
    float delayLp {};
    juce::Reverb reverb;
    juce::Random random;
};
