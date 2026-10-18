#pragma once
#include <array>
#include <vector>

// Sampled instruments. A kit gives each layer (melody, chords, bass) its own recorded instrument; the
// samples are free libraries prepared by tools/build_samples.py and embedded in the binary.
class Sampler {
public:
    enum Inst { piano, nylon, guitar, ebass, harp, violin, cello, instCount };
    struct Kit { const char* name; const char* description; Inst melody, chords, bass; bool dark; };
    static constexpr int kitCount = 7;
    static const Kit& kit(int);

    void prepare(double sampleRate);
    void setKit(int k) { kitIndex = k < 0 || k >= kitCount ? 0 : k; }
    void setLevels(float bass, float melody) { bassLevel = bass; melodyLevel = melody; }
    // layer: 0 melody, 1 bass, 2 chords
    void noteOn(int pitch, int velocity, int lengthSamples, int layer);
    void allNotesOff();
    void render(float& l, float& r);   // adds one sample
    int activeVoices() const;

    struct Zone;

private:
    struct Voice {
        bool active {}, releasing {};
        const Zone* zone {};
        double pos {}, inc {};
        float gain {}, env {}, atk {}, rel {}, panL {}, panR {}, lp {}, lpK { 1.0f };
        int left {}, layer {};
    };
    std::array<Voice, 48> voices {};
    double sr { 44100.0 };
    int kitIndex {};
    float bassLevel { 0.8f }, melodyLevel { 0.8f };
    int panFlip {};
};
