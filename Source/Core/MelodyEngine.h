#pragma once
#include "Music.h"
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

// Melody Code engine: a chord progression (Chord Code's HarmonyEngine) drives a sub-bass line,
// and the melody is written on top of both.
namespace mc {
constexpr int ppq = 960;
constexpr int step = ppq / 4;          // one 16th note in ticks
constexpr int stepsPerBar = 16;
constexpr int maxBars = 8;

enum class Style { funkBR, pop, trap, piseiro, rnb, house, loFi, phonk, count };
enum class Vibe { dark, happy, sad, hypnotic, epic, chill, romantic, count };
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
    Style style { Style::funkBR };
    Vibe vibe { Vibe::dark };
    Role role { Role::lead };
    Density density { Density::mid };
    Range range { Range::mid };
    int bars { 4 };
};

struct Pattern {
    Settings settings;
    ccp::Progression progression;      // one chord per bar
    std::vector<Note> bass, melody;
    int bars() const { return progression.chordCount; }
    int lengthTicks() const { return bars() * stepsPerBar * step; }
};

ccp::GenerationSettings harmonySettings(const Settings&);

class Engine {
public:
    explicit Engine(uint32_t seed = std::random_device {}());

    // New progression + sub + melody. Locked bars keep their chord, sub and melody.
    Pattern generate(const Settings&, const Pattern* current = nullptr, const std::array<bool, maxBars>& locked = {});
    // Keeps chords and sub, writes a new melody.
    Pattern newMelody(const Pattern&, const std::array<bool, maxBars>& locked = {});
    // Keeps chords and melody, writes a new sub line.
    Pattern newBass(const Pattern&);
    // Small edits to the existing melody: neighbour tones on weak beats, rhythm nudges.
    Pattern variation(const Pattern&, const std::array<bool, maxBars>& locked = {});
    // Swap one bar's chord, then rewrite that bar's sub and melody.
    Pattern setChord(const Pattern&, int bar, const ccp::Chord&);

    std::vector<Note> bassFor(const Settings&, const ccp::Progression&);
    std::vector<Note> melodyFor(const Settings&, const ccp::Progression&, const std::vector<Note>& keep,
                                const std::array<bool, maxBars>& locked);

    static std::vector<int> chordPitchClasses(const ccp::Chord&);
    static int subPitch(const ccp::Chord&);
    static double score(const Settings&, const ccp::Progression&, const std::vector<Note>&);

private:
    std::vector<Note> melodyAttempt(const Settings&, const ccp::Progression&, const std::vector<Note>& keep,
                                    const std::array<bool, maxBars>& locked);
    uint64_t signature(const std::vector<Note>&) const;
    std::mt19937 rng;
    ccp::ProgressionGenerator harmony;
    std::deque<uint64_t> recent;
};

std::vector<uint8_t> midiBytes(const std::vector<Note>&, int bpm, int channel);
const char* styleName(Style);
const char* vibeName(Vibe);
const char* roleName(Role);
const char* densityName(Density);
const char* rangeName(Range);
}
