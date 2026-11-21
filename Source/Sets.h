#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Synthesized instruments, grouped in sound sets ("conjuntos"). A set gives each layer (melody, chords, bass)
// its own patch. A patch is plain data (Sets.cpp): oscillators (analog shapes, supersaw, FM, additive partials,
// plucked string), a filter with its own envelope, amp and pitch envelopes, drive, glide and vibrato.
// Everything is generated here, no samples.
class Sets {
public:
    struct Kit {
        const char* name; const char* description;
        const char* melody; const char* chords; const char* bass;   // patch names (Sets.cpp)
        bool sustainChords { false };   // pad-like chord sounds: one held chord per change instead of the style's rhythm
        bool rootChords { false };      // stabs that already sound like a chord: play only each chord's root
    };
    static constexpr int kitCount = 12;
    static const Kit& kit(int);

    void prepare(double sampleRate);
    void setKit(int k) { kitIndex = k < 0 || k >= kitCount ? 0 : k; }
    void setLevels(float bass, float melody, float chords) { bassLevel = bass; melodyLevel = melody; chordLevel = chords; }
    // Median pitch of each layer of the playing pattern (melody, bass, chords): every line is moved by whole
    // octaves so it sits where its patch sounds best. 0 = unknown, no move.
    void setCenters(int melody, int bass, int chords) { centers = { melody, bass, chords }; }
    // layer: 0 melody, 1 bass, 2 chords
    void noteOn(int pitch, int velocity, int lengthSamples, int layer);
    void allNotesOff();
    // adds one sample: melody and chords to l/r (they go through the Vibe's effects), the bass to the dry pair
    void render(float& l, float& r, float& dryL, float& dryR);
    int activeVoices() const;

    struct Patch;

private:
    static constexpr int maxUnison = 7;
    struct Voice {
        bool active {}, releasing {};
        const Patch* p {};
        int layer {}, pitch {}, left {}, age {};
        float vel {}, gain {}, panL {}, panR {};
        double freq {}, freqTarget {}, glideK { 1.0 };
        double ph[2][maxUnison] {}, mod[2] {}, part[8] {}, sub {};
        float env {}, atkK {}, decK {}, relK {}, kill {};
        float svf[2][2][2] {};          // side, stage, state
        float g {}, k {};               // filter coefficients, refreshed every few samples
        float noiseLp {}, fenv {}, fK {};
        uint32_t rng { 0x9e3779b9 };
        std::vector<float> ks; int ksPos {}; double ksDelay {}; float ksLoss {}, ksLast {};
    };
    void start(Voice&, const Patch&, int pitch, float vel, int length, int layer, const Voice* legatoFrom);
    void renderVoice(Voice&, float& l, float& r);
    std::array<Voice, 64> voices {};
    double sr { 44100.0 };
    int kitIndex {};
    float bassLevel { 0.8f }, melodyLevel { 0.8f }, chordLevel { 0.8f };
    std::array<int, 3> centers {};
    int panFlip {};
};
