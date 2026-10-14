#pragma once
#include "Music.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include <deque>
#include <random>
#include <vector>

// Melody Code engine: a chord progression (Chord Code's HarmonyEngine) drives a sub-bass line,
// and the melody is written on top of both.
namespace mc {
constexpr int ppq = 960;
constexpr int step = ppq / 4;          // one 16th note in ticks
constexpr int stepsPerBar = 16;
constexpr int barTicks = stepsPerBar * step;
constexpr int maxBars = 8;

// "automatic" lets the engine pick the style / vibe it thinks fits; the choice is stored in the Pattern.
enum class Style { automatic, funkBR, pop, trap, phonk, drill, bossaNova, jazz, rnb,
                   reggaeton, house, loFi, count };
enum class Vibe { automatic, dark, happy, sad, hypnotic, epic, chill, romantic, count };
enum class Role { lead, hook, counter, count };
enum class Density { low, mid, high, count };
enum class Range { low, mid, high, count };

struct Note {
    int start {};      // ticks from pattern start
    int length {};     // ticks
    int pitch {};      // MIDI note
    int velocity { 100 };
    bool glide {};     // slide in from the previous note (808 / lead portamento)
};

struct Settings {
    int key { 9 };
    ccp::ScaleType scale { ccp::ScaleType::naturalMinor };
    Style style { Style::automatic };
    Vibe vibe { Vibe::automatic };
    Role role { Role::lead };
    Density density { Density::mid };
    Range range { Range::mid };
    int bars { 4 };
};

struct Pattern {
    Settings settings;
    Style style { Style::pop };        // resolved style (never automatic)
    Vibe vibe { Vibe::chill };         // resolved vibe (never automatic)
    ccp::Progression progression;      // one chord per bar (a held chord repeats in the next bar)
    // Harmonic rhythm: where each bar's chord really starts, in 16ths from its bar line.
    // Negative = arrives early (anticipation / 6+2 feel), positive = the previous chord rings longer.
    std::array<int, maxBars> chordShift {};
    std::vector<Note> bass, melody;
    int bars() const { return progression.chordCount; }
    int chordIndexAt(int tick) const {
        const int n = bars();
        if (n <= 0) return 0;
        int b = std::clamp(tick / barTicks, 0, n - 1);
        if (b + 1 < n && tick >= (b + 1) * barTicks + chordShift[(size_t) (b + 1)] * step) return b + 1;
        if (b > 0 && tick < b * barTicks + chordShift[(size_t) b] * step) return b - 1;
        return b;
    }
    const ccp::Chord& chordAt(int tick) const { return progression.chords[(size_t) chordIndexAt(tick)]; }
    int lengthTicks() const { return bars() * barTicks; }
};

ccp::GenerationSettings harmonySettings(Style resolvedStyle, Vibe resolvedVibe, const Settings&);

class Engine {
public:
    explicit Engine(uint32_t seed = std::random_device {}());

    // New progression + sub + melody. Locked bars keep their chord, sub and melody.
    Pattern generate(const Settings&, const Pattern* current = nullptr, const std::array<bool, maxBars>& locked = {});
    // Keeps chords and sub, writes a new melody (settings may change role / density / range).
    Pattern newMelody(const Pattern&, const Settings&, const std::array<bool, maxBars>& locked = {});
    // Keeps chords and melody, writes a new sub line.
    Pattern newBass(const Pattern&);
    // Small edits to the existing melody: neighbour tones on weak beats.
    Pattern variation(const Pattern&, const std::array<bool, maxBars>& locked = {});
    // Swap one bar's chord, then rewrite that bar's sub and melody.
    Pattern setChord(const Pattern&, int bar, const ccp::Chord&);
    // Same notes, another vibe (the vibe only changes the sound).
    Pattern withVibe(const Pattern&, Vibe);

    static std::vector<int> chordPitchClasses(const ccp::Chord&);
    static int subPitch(const ccp::Chord&);
    static double score(const Pattern&, const std::vector<Note>&);

private:
    Style resolveStyle(Style);
    Vibe resolveVibe(Vibe, Style);
    std::vector<Note> bassFor(const Pattern&);
    void shapeHarmony(Pattern&, const std::array<bool, maxBars>& locked, const Pattern* current);
    std::vector<Note> melodyFor(const Pattern&, const std::vector<Note>& keep, const std::array<bool, maxBars>& locked);
    std::vector<Note> melodyAttempt(const Pattern&, const std::vector<Note>& keep, const std::array<bool, maxBars>& locked);
    uint64_t signature(const std::vector<Note>&) const;
    std::mt19937 rng;
    ccp::ProgressionGenerator harmony;
    std::deque<uint64_t> recent;
};

std::vector<uint8_t> midiBytes(const std::vector<Note>&, int bpm, int channel);
const char* styleName(Style);
const char* vibeName(Vibe);
}
