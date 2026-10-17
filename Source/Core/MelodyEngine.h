#pragma once
#include "Music.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

// Melody Code engine: a chord progression (Chord Code's HarmonyEngine) drives a low bass line and a melody.
// Both are played by the same instrument; the bass is just the low notes.
namespace mc {
constexpr int ppq = 960;
constexpr int step = ppq / 4;          // one 16th note in ticks
constexpr int stepsPerBar = 16;
constexpr int barTicks = stepsPerBar * step;
constexpr int maxBars = 8;
constexpr int autoKey = -1, autoScale = -1;   // "PADRÃO": the engine picks

// "automatic" lets the engine pick the style; the choice is stored in the Pattern.
enum class Style { automatic, funkBR, pop, trap, phonk, drill, bossaNova, jazz, rnb, reggaeton, house, loFi, count };
// The vibe is the sound's aesthetic (effects); it never changes the notes.
enum class Vibe { clean, dreamy, dark, tape, space, count };
enum class Feel { straight, swing, triplet, count };
enum class Role { lead, hook, counter, count };
enum class Density { low, mid, high, count };
enum class Range { low, mid, high, count };

struct Note {
    int start {};      // ticks from pattern start
    int length {};     // ticks
    int pitch {};      // MIDI note
    int velocity { 100 };
};

struct Settings {
    int key { autoKey };            // 0..11 or autoKey
    int scale { autoScale };        // ccp::ScaleType index or autoScale
    Style style { Style::automatic };
    Vibe vibe { Vibe::dreamy };
    Feel feel { Feel::straight };
    Role role { Role::lead };
    Density density { Density::mid };
    Range range { Range::mid };
    int bars { 4 };
};

struct Pattern {
    Settings settings;
    Style style { Style::pop };                         // resolved (never automatic)
    int key { 9 };                                      // resolved
    ccp::ScaleType scale { ccp::ScaleType::naturalMinor };   // resolved
    ccp::Progression progression;      // one chord per bar (a held chord repeats in the next bar)
    // Harmonic rhythm: where each bar's chord really starts, in 16ths from its bar line.
    // Negative = arrives early (anticipation), positive = the previous chord rings longer.
    std::array<int, maxBars> chordShift {};
    std::vector<Note> bass, melody;
    int motif { -1 };                  // seed of the learned engine's idea, -1 = written by the rule engine
    int bars() const { return progression.chordCount; }
    int lengthTicks() const { return bars() * barTicks; }
    int chordIndexAt(int tick) const {
        const int n = bars();
        if (n <= 0) return 0;
        int b = std::clamp(tick / barTicks, 0, n - 1);
        if (b + 1 < n && tick >= (b + 1) * barTicks + chordShift[(size_t) (b + 1)] * step) return b + 1;
        if (b > 0 && tick < b * barTicks + chordShift[(size_t) b] * step) return b - 1;
        return b;
    }
    const ccp::Chord& chordAt(int tick) const { return progression.chords[(size_t) chordIndexAt(tick)]; }
};

ccp::GenerationSettings harmonySettings(Style resolvedStyle, int key, ccp::ScaleType);

class Engine {
public:
    explicit Engine(uint32_t seed = std::random_device {}());

    // New key/scale (when PADRÃO), chords, bass and melody.
    Pattern generate(const Settings&);
    // Keeps chords and bass, writes a new melody (settings may change role / density / range / feel).
    Pattern newMelody(const Pattern&, const Settings&);
    // Small edits to the existing melody: neighbour tones on weak beats.
    Pattern variation(const Pattern&);

    static std::vector<int> chordPitchClasses(const ccp::Chord&);
    static int bassRoot(const ccp::Chord&);
    static double score(const Pattern&, const std::vector<Note>&);

private:
    Style resolveStyle(Style);
    std::vector<Note> bassFor(const Pattern&);
    void shapeHarmony(Pattern&);
    std::vector<Note> melodyFor(const Pattern&);
    std::vector<Note> melodyAttempt(const Pattern&);
    std::vector<Note> motifAttempt(const Pattern&);
    bool fromLibrary(Pattern&, int templateIndex);
    std::vector<Note> libraryMelody(const Pattern&);
    uint64_t signature(const std::vector<Note>&) const;
    std::mt19937 rng;
    ccp::ProgressionGenerator harmony;
    std::deque<uint64_t> recent;
};

std::vector<uint8_t> midiBytes(const std::vector<Note>&, int bpm, int channel);
const char* styleName(Style);
const char* vibeName(Vibe);
const char* feelName(Feel);
}
