#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Preview synth: a mono sub voice plus a polyphonic melody synth whose presets aim to sound like a finished
// sample (keys, bells, pads, plucks...). The Vibe does not change the notes; it changes the sound's character.
class Synth {
public:
    enum Preset { keys, bells, pad, pluck, synth, mallet, presetCount };
    static const char* presetName(int);

    // Sound character, set from the Vibe.
    struct Character {
        float bright { 1.0f };      // filter cutoff multiplier
        float detune { 1.0f };      // oscillator spread multiplier
        float reverbSize { 0.6f }, reverbWet { 0.24f };
        float delayWet { 0.2f }, delayFeedback { 0.32f };
        float drive {};             // warm saturation, 0..1
        float octaveUp {};          // shimmer layer an octave above, 0..1
        float wobble {};            // tape pitch wobble and gentle tone loss, 0..1
    };

    void prepare(double sampleRate);
    void setPreset(int p) { preset = juce::jlimit(0, presetCount - 1, p); }
    void setLevels(float sub, float melody) { subLevel = sub; melodyLevel = melody; }
    void setDelayTime(double seconds);
    void setCharacter(const Character&);
    void noteOn(int pitch, int velocity, int lengthSamples, bool glide, bool isSub);
    void allNotesOff();
    void render(float* left, float* right, int numSamples);

private:
    struct Voice {
        bool active {};
        int pitch {}, age {}, length {}, preset {};
        float velocity {}, freq {};
        double phase[4] {}, modPhase {}, modPhase2 {};
        float env {}, filterEnv {}, lp[2][2] {};
        float pan {};
    };
    void renderVoice(Voice&, float& l, float& r, double wobbleRatio);
    float renderSub();
    double sr { 44100.0 };
    int preset { keys };
    Character character, target;
    std::array<Voice, 24> voices {};
    Voice sub {};
    float subLevel { 0.8f }, melodyLevel { 0.8f };
    juce::AudioBuffer<float> delayBuffer;
    int delayWrite {}, delaySamples { 1 };
    float delayLp {}, toneL {}, toneR {};
    double wobblePhase {}, wobblePhase2 {};
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    int panFlip {};
};
