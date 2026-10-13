#include "MelodyEngine.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <sstream>

namespace mc {
namespace {
struct Cell { double on, len; };                     // in 16ths; thirds of a 16th give triplets
enum BassKind { root, octave, fifth, approach, third };
struct Hit { double on, len; int kind; };
enum class Texture { none, thirds, sixths, shell, octaves, pedal };

/* One profile per style. The fields encode how hooks are written in that genre:
   rep         chance of repeating the previous pitch on a weak beat
   stac        note-length factor (short = punchy, 1 = legato)
   leap/span   largest jump and overall range, in scale steps
   appr        chance of a chromatic approach note into the next target
   arp         chance of outlining the chord instead of stepping
   ext         sevenths/ninths count as chord tones on strong beats
   glide       808 / lead portamento between tied notes
   swing16/8   delay of off-beat 16ths / 8ths (0.33 of an 8th = triplet swing)
   anticipate  chance a downbeat note arrives a 16th early (pop, bossa, reggaeton syncopation)
   grace       chance of a short ornament before a strong note
   overlap     chance a note rings into the next one (legato overlap, only on consonant intervals)
   texture     the second voice this style uses (sertanejo's "segunda voz" in thirds, jazz shells, pedals...)
   voice       how often that second voice appears
   forms       phrase forms the engine picks from (A = motif, B = contrast, C = climax / new idea)
   vibes       vibes that suit the style when the vibe is left on automatic */
struct Profile {
    ccp::Style harmony;
    double rep, stac; int leap, span; double appr, arp; bool ext, glide;
    double swing16, swing8, anticipate, grace, overlap;
    Texture texture; double voice;
    const char* forms;
    std::vector<Vibe> vibes;
    std::vector<std::vector<Cell>> cells;
    std::vector<std::vector<Hit>> bass;
};

const Profile& profile(Style s) {
    using V = Vibe;
    static const std::map<Style, Profile> table {
        { Style::funkBR, { .harmony = ccp::Style::funkBrasileiro, .rep = .4, .stac = .75, .leap = 3, .span = 7, .appr = .05, .arp = .15, .ext = false, .glide = false,
            .swing16 = .12, .swing8 = 0, .anticipate = .1, .grace = .05, .overlap = 0, .texture = Texture::octaves, .voice = .12,
            .forms = "AABA|AAAB|ABAB", .vibes = { V::dark, V::hypnotic, V::happy },
            .cells = { {{0,2},{3,1},{6,2},{10,2},{12,3}}, {{0,1},{3,2},{6,1},{8,2},{11,1},{14,2}},
                       {{0,3},{3,3},{6,2},{10,1},{12,2}}, {{0,1},{2,1},{3,2},{7,1},{10,2},{13,1}} },
            .bass = { {{0,2,root},{3,2,root},{6,2,octave},{10,2,root},{12,3,root}},
                      {{0,3,root},{3,3,root},{7,1,root},{10,2,octave},{14,2,approach}},
                      {{0,2,root},{3,3,root},{8,2,root},{11,2,octave},{14,2,root}} } } },
        { Style::pop, { .harmony = ccp::Style::pop, .rep = .15, .stac = .95, .leap = 4, .span = 9, .appr = .06, .arp = .15, .ext = false, .glide = false,
            .swing16 = 0, .swing8 = 0, .anticipate = .3, .grace = .08, .overlap = .12, .texture = Texture::thirds, .voice = .12,
            .forms = "AABA|ABAC|AABC", .vibes = { V::happy, V::romantic, V::sad, V::epic },
            .cells = { {{0,4},{4,2},{6,2},{8,6}}, {{0,2},{2,2},{4,4},{10,2},{12,4}}, {{2,2},{4,2},{6,4},{12,4}},
                       {{0,2},{2,2},{4,2},{6,2},{8,8}}, {{2,2},{4,2},{6,2},{8,2},{10,6}} },
            .bass = { {{0,6,root},{6,2,root},{8,6,root},{14,2,approach}}, {{0,8,root},{8,4,root},{12,4,fifth}},
                      {{0,4,root},{4,4,root},{8,4,root},{12,4,octave}} } } },
        { Style::trap, { .harmony = ccp::Style::trap, .rep = .35, .stac = .9, .leap = 5, .span = 8, .appr = .08, .arp = .12, .ext = false, .glide = true,
            .swing16 = 0, .swing8 = 0, .anticipate = .1, .grace = .05, .overlap = .15, .texture = Texture::pedal, .voice = .2,
            .forms = "AABA|ABAB|AAAB", .vibes = { V::dark, V::sad, V::hypnotic, V::epic },
            .cells = { {{0,1},{1,1},{2,2},{6,2},{8,1},{9,1},{10,4}}, {{0,3},{4,1},{5,1},{6,2},{12,3}},
                       {{0,2},{3,3},{8,1},{9,1},{10,1},{12,4}}, {{0,4.0/3},{4.0/3,4.0/3},{8.0/3,4.0/3},{4,4},{10,2},{12,4}} },
            .bass = { {{0,7,root},{7,3,root},{10,6,octave}}, {{0,4,root},{6,2,root},{8,8,root}}, {{0,3,root},{3,4,root},{11,5,octave}} } } },
        { Style::phonk, { .harmony = ccp::Style::phonk, .rep = .4, .stac = .7, .leap = 4, .span = 7, .appr = .05, .arp = .2, .ext = false, .glide = true,
            .swing16 = .08, .swing8 = 0, .anticipate = .05, .grace = 0, .overlap = 0, .texture = Texture::octaves, .voice = .3,
            .forms = "AAAB|AABA", .vibes = { V::dark, V::hypnotic },
            .cells = { {{0,2},{2,2},{4,1},{6,2},{8,2},{10,2},{12,4}}, {{0,3},{3,3},{6,2},{8,3},{11,3},{14,2}},
                       {{0,1},{1,1},{2,2},{4,1},{5,1},{6,2},{8,2},{10,2},{12,2},{14,2}} },
            .bass = { {{0,4,root},{6,2,root},{8,4,root},{14,2,octave}}, {{0,3,root},{3,3,root},{6,10,root}} } } },
        { Style::drill, { .harmony = ccp::Style::trap, .rep = .3, .stac = .85, .leap = 5, .span = 8, .appr = .1, .arp = .1, .ext = false, .glide = true,
            .swing16 = 0, .swing8 = 0, .anticipate = .15, .grace = .1, .overlap = .1, .texture = Texture::pedal, .voice = .2,
            .forms = "AABA|ABAB", .vibes = { V::dark, V::sad },
            .cells = { {{0,4.0/3},{4.0/3,4.0/3},{8.0/3,8.0/3},{6,2},{8,1},{9,1},{10,6}}, {{0,3},{3,1},{4,4},{10,2},{12,4}},
                       {{0,2},{3,2},{6,2},{10,4.0/3},{34.0/3,4.0/3},{38.0/3,10.0/3}} },
            .bass = { {{0,3,root},{3,3,root},{6,4,octave},{11,2,root},{13,3,approach}}, {{0,6,root},{6,4,root},{10,6,octave}} } } },
        { Style::piseiro, { .harmony = ccp::Style::latin, .rep = .15, .stac = .8, .leap = 4, .span = 10, .appr = .05, .arp = .45, .ext = false, .glide = false,
            .swing16 = .05, .swing8 = 0, .anticipate = .15, .grace = .15, .overlap = .05, .texture = Texture::thirds, .voice = .55,
            .forms = "AABA|ABAB|AABC", .vibes = { V::happy, V::romantic, V::sad },
            .cells = { {{0,2},{2,2},{4,2},{6,2},{8,2},{10,2},{12,4}}, {{0,1},{2,1},{4,2},{8,1},{10,1},{12,4}},
                       {{0,2},{2,1},{3,1},{4,2},{6,2},{8,4},{12,2},{14,2}} },
            .bass = { {{0,3,root},{3,1,fifth},{4,4,fifth},{8,3,root},{11,1,fifth},{12,4,fifth}},
                      {{0,3,root},{3,5,root},{8,3,octave},{11,5,root}} } } },
        { Style::sertanejo, { .harmony = ccp::Style::pop, .rep = .15, .stac = 1.0, .leap = 4, .span = 10, .appr = .04, .arp = .2, .ext = false, .glide = false,
            .swing16 = 0, .swing8 = 0, .anticipate = .25, .grace = .2, .overlap = .15, .texture = Texture::thirds, .voice = .7,
            .forms = "AABA|ABAC|AABC", .vibes = { V::romantic, V::sad, V::happy },
            .cells = { {{0,2},{2,2},{4,4},{8,2},{10,2},{12,4}}, {{2,1},{3,1},{4,2},{6,2},{8,6},{14,2}},
                       {{0,3},{3,1},{4,4},{10,2},{12,4}}, {{0,2},{2,2},{4,2},{6,6},{12,2},{14,2}} },
            .bass = { {{0,3,root},{3,3,root},{6,2,fifth},{8,3,root},{11,3,root},{14,2,approach}},
                      {{0,4,root},{4,4,fifth},{8,4,root},{12,4,fifth}} } } },
        { Style::pagode, { .harmony = ccp::Style::latin, .rep = .2, .stac = .85, .leap = 4, .span = 9, .appr = .12, .arp = .2, .ext = true, .glide = false,
            .swing16 = .1, .swing8 = 0, .anticipate = .3, .grace = .25, .overlap = .1, .texture = Texture::thirds, .voice = .35,
            .forms = "AABA|ABAB|ABAC", .vibes = { V::happy, V::romantic, V::chill },
            .cells = { {{0,2},{3,1},{4,2},{7,1},{8,3},{11,1},{12,4}}, {{1,2},{3,2},{6,2},{8,2},{10,2},{12,4}},
                       {{0,1},{2,2},{4,1},{6,2},{8,1},{10,2},{12,4}} },
            .bass = { {{0,3,root},{3,1,root},{4,2,fifth},{8,3,root},{11,1,octave},{12,4,approach}} } } },
        { Style::bossaNova, { .harmony = ccp::Style::jazz, .rep = .1, .stac = 1.0, .leap = 5, .span = 10, .appr = .15, .arp = .2, .ext = true, .glide = false,
            .swing16 = 0, .swing8 = 0, .anticipate = .4, .grace = .1, .overlap = .25, .texture = Texture::shell, .voice = .3,
            .forms = "AABA|ABAC", .vibes = { V::chill, V::romantic, V::sad },
            .cells = { {{0,3},{3,3},{6,4},{10,2},{12,4}}, {{2,2},{4,3},{7,3},{10,6}}, {{0,2},{2,4},{6,2},{8,4},{14,2}},
                       {{0,6},{6,2},{8,2},{10,6}} },
            .bass = { {{0,5,root},{6,2,fifth},{8,5,fifth},{14,2,root}}, {{0,6,root},{6,2,root},{8,6,fifth},{14,2,approach}} } } },
        { Style::jazz, { .harmony = ccp::Style::jazz, .rep = .08, .stac = .95, .leap = 5, .span = 11, .appr = .35, .arp = .3, .ext = true, .glide = false,
            .swing16 = 0, .swing8 = .33, .anticipate = .2, .grace = .25, .overlap = .1, .texture = Texture::shell, .voice = .3,
            .forms = "AABA|ABAC", .vibes = { V::chill, V::romantic, V::happy },
            .cells = { {{0,2},{2,2},{4,2},{6,2},{8,4.0/3},{28.0/3,4.0/3},{32.0/3,4.0/3},{12,4}}, {{2,2},{4,2},{6,4},{10,2},{12,2},{14,2}},
                       {{0,6},{6,2},{8,2},{10,2},{12,4}}, {{0,2},{2,2},{4,2},{6,2},{8,2},{10,2},{12,2},{14,2}} },
            .bass = { {{0,4,root},{4,4,third},{8,4,fifth},{12,4,approach}}, {{0,4,root},{4,4,fifth},{8,4,octave},{12,4,approach}} } } },
        { Style::rnb, { .harmony = ccp::Style::rnb, .rep = .1, .stac = 1.0, .leap = 5, .span = 10, .appr = .3, .arp = .2, .ext = true, .glide = true,
            .swing16 = .1, .swing8 = 0, .anticipate = .2, .grace = .3, .overlap = .35, .texture = Texture::sixths, .voice = .2,
            .forms = "AABA|ABAC|AABC", .vibes = { V::romantic, V::chill, V::sad },
            .cells = { {{0,3},{3,1},{4,4},{10,2},{12,4}}, {{1,2},{3,3},{8,3},{12,4}}, {{0,1},{1,1},{2,4},{7,1},{8,6}} },
            .bass = { {{0,10,root},{10,2,fifth},{12,4,approach}}, {{0,6,root},{7,3,root},{12,4,octave}} } } },
        { Style::reggaeton, { .harmony = ccp::Style::latin, .rep = .3, .stac = .8, .leap = 4, .span = 8, .appr = .05, .arp = .2, .ext = false, .glide = false,
            .swing16 = 0, .swing8 = 0, .anticipate = .2, .grace = .1, .overlap = .05, .texture = Texture::thirds, .voice = .2,
            .forms = "AABA|AAAB|ABAB", .vibes = { V::happy, V::dark, V::romantic },
            .cells = { {{0,3},{3,3},{6,2},{8,3},{11,3},{14,2}}, {{0,2},{3,1},{4,4},{8,2},{11,1},{12,4}}, {{3,3},{6,2},{11,3},{14,2}} },
            .bass = { {{0,3,root},{3,1,root},{4,2,root},{6,2,octave},{8,3,root},{11,1,root},{12,2,root},{14,2,octave}} } } },
        { Style::afrobeat, { .harmony = ccp::Style::latin, .rep = .25, .stac = .8, .leap = 4, .span = 8, .appr = .05, .arp = .25, .ext = false, .glide = false,
            .swing16 = .08, .swing8 = 0, .anticipate = .15, .grace = .15, .overlap = .05, .texture = Texture::thirds, .voice = .25,
            .forms = "AAAB|AABA|ABAB", .vibes = { V::happy, V::chill, V::romantic },
            .cells = { {{0,2},{2,1},{3,2},{6,2},{8,2},{10,1},{11,2},{14,2}}, {{1,2},{3,3},{8,1},{9,2},{12,4}}, {{0,3},{3,3},{6,2},{10,2},{12,3}} },
            .bass = { {{0,2,root},{3,2,root},{6,3,fifth},{10,2,root},{12,2,octave},{14,2,approach}} } } },
        { Style::house, { .harmony = ccp::Style::house, .rep = .45, .stac = .75, .leap = 3, .span = 6, .appr = 0, .arp = .2, .ext = true, .glide = false,
            .swing16 = .06, .swing8 = 0, .anticipate = .1, .grace = 0, .overlap = .05, .texture = Texture::octaves, .voice = .25,
            .forms = "AAAB|AABA", .vibes = { V::happy, V::hypnotic, V::epic },
            .cells = { {{2,2},{6,2},{10,2},{14,2}}, {{0,2},{2,2},{6,4},{10,2},{12,2}}, {{2,1},{4,2},{6,1},{10,1},{12,2},{14,1}} },
            .bass = { {{2,2,root},{6,2,root},{10,2,root},{14,2,octave}}, {{0,1,root},{2,2,root},{6,2,octave},{10,2,root},{14,2,root}} } } },
        { Style::loFi, { .harmony = ccp::Style::loFi, .rep = .1, .stac = 1.0, .leap = 4, .span = 8, .appr = .2, .arp = .25, .ext = true, .glide = false,
            .swing16 = .2, .swing8 = 0, .anticipate = .2, .grace = .1, .overlap = .4, .texture = Texture::shell, .voice = .2,
            .forms = "AABA|ABAC", .vibes = { V::chill, V::sad, V::romantic },
            .cells = { {{0,6},{8,2},{10,6}}, {{2,4},{8,4},{13,3}}, {{0,3},{4,2},{6,6}}, {{0,2},{2,2},{4,4},{10,6}} },
            .bass = { {{0,12,root},{12,4,approach}}, {{0,7,root},{8,8,fifth}} } } },
    };
    return table.at(s);
}

struct VibeMod { int lean, spanAdd, leapAdd, contour; double repAdd, overlapAdd, voiceAdd; };
VibeMod vibeMod(Vibe v) {
    switch (v) {
        case Vibe::dark:     return { -1, 0, 0, -1, .05, 0, 0 };
        case Vibe::happy:    return { 1, 1, 0, 1, 0, 0, .05 };
        case Vibe::sad:      return { -1, 0, 0, -1, 0, .1, .05 };
        case Vibe::hypnotic: return { 0, -2, -1, 0, .2, 0, 0 };
        case Vibe::epic:     return { 1, 3, 2, 1, 0, 0, .1 };
        case Vibe::chill:    return { 0, -1, 0, 0, .05, .1, 0 };
        case Vibe::romantic: return { 0, 1, 0, 0, 0, .15, .1 };
        default:             return {};
    }
}

double chance(std::mt19937& r) { return std::uniform_real_distribution<double>(0, 1)(r); }
template <typename T> const T& pick(std::mt19937& r, const std::vector<T>& v) {
    return v[std::uniform_int_distribution<size_t>(0, v.size() - 1)(r)];
}
int wrap12(int n) { n %= 12; return n < 0 ? n + 12 : n; }
bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
int ticks(double sixteenths) { return (int) std::lround(sixteenths * step); }
bool consonant(int a, int b) { const int d = std::abs(a - b) % 12; return d == 3 || d == 4 || d == 5 || d == 7 || d == 8 || d == 9 || d == 0; }
int barOf(int tick) { return std::max(0, tick) / barTicks; }

std::vector<int> triadOf(const ccp::Chord& c, const std::array<int, 7>& scale) {
    std::vector<int> t { c.root };
    if (c.quality >= 0) { for (int i = 1; i < c.noteCount && t.size() < 3; ++i) if (!has(t, wrap12(c.notes[(size_t) i]))) t.push_back(wrap12(c.notes[(size_t) i])); return t; }
    t.push_back(scale[(size_t) ((c.degree + 2) % 7)]);
    t.push_back(scale[(size_t) ((c.degree + 4) % 7)]);
    return t;
}

void applySwing(std::vector<Note>& notes, const Profile& p) {
    for (auto& n : notes) {
        if (p.swing8 > 0 && n.start % ppq == ppq / 2) n.start += (int) (p.swing8 * ppq / 2);
        else if (p.swing16 > 0 && n.start % (2 * step) == step) n.start += (int) (p.swing16 * step);
    }
}
}

ccp::GenerationSettings harmonySettings(Style style, Vibe vibe, const Settings& s) {
    static const std::map<Vibe, ccp::Mood> moods { { Vibe::dark, ccp::Mood::dark }, { Vibe::happy, ccp::Mood::fun },
        { Vibe::sad, ccp::Mood::melancholic }, { Vibe::hypnotic, ccp::Mood::mysterious }, { Vibe::epic, ccp::Mood::epic },
        { Vibe::chill, ccp::Mood::atmospheric }, { Vibe::romantic, ccp::Mood::romantic } };
    ccp::GenerationSettings g;
    g.key = s.key; g.scale = s.scale;
    g.style = style == Style::automatic ? ccp::Style::standard : profile(style).harmony;
    g.mood = vibe == Vibe::automatic ? ccp::Mood::standard : moods.at(vibe);
    g.complexity = ccp::Complexity::automatic;
    return g;
}

Engine::Engine(uint32_t seed) : rng(seed), harmony(seed ^ 0x9e3779b9u) {}

Style Engine::resolveStyle(Style s) {
    if (s != Style::automatic) return s;
    return (Style) std::uniform_int_distribution<int>(1, (int) Style::count - 1)(rng);
}
Vibe Engine::resolveVibe(Vibe v, Style s) {
    if (v != Vibe::automatic) return v;
    return pick(rng, profile(s).vibes);
}

std::vector<int> Engine::chordPitchClasses(const ccp::Chord& c) {
    std::vector<int> pcs;
    for (int i = 0; i < c.noteCount; ++i) if (!has(pcs, wrap12(c.notes[(size_t) i]))) pcs.push_back(wrap12(c.notes[(size_t) i]));
    return pcs;
}

int Engine::subPitch(const ccp::Chord& c) {
    int n = 24 + c.root;        // C1..B1
    if (n < 28) n += 12;        // keep it between E1 and D#2
    return n;
}

std::vector<Note> Engine::bassFor(const Pattern& pat) {
    const auto& prof = profile(pat.style);
    const auto& tpl = pick(rng, prof.bass);
    const auto& p = pat.progression;
    std::vector<Note> out;
    for (int b = 0; b < p.chordCount; ++b) {
        const auto& chord = p.chords[(size_t) b];
        const int sub = subPitch(chord);
        const int next = subPitch(p.chords[(size_t) ((b + 1) % p.chordCount)]);
        int third = sub + 4;
        for (int pc : chordPitchClasses(chord)) { const int iv = wrap12(pc - chord.root); if (iv == 3 || iv == 4) third = sub + iv; }
        int prev = -1;
        for (size_t i = 0; i < tpl.size(); ++i) {
            const auto& h = tpl[i];
            if (pat.settings.density == Density::low && i > 0 && h.kind != root && i % 2 == 1) continue;
            int pitch = sub;
            if (h.kind == octave) pitch = sub + 12;
            else if (h.kind == fifth) pitch = sub + 7 > 45 ? sub - 5 : sub + 7;
            else if (h.kind == third) pitch = third;
            else if (h.kind == approach) pitch = next == sub ? sub + 7 : next + (next > sub ? -1 : 1);
            Note n;
            n.start = b * barTicks + ticks(h.on);
            n.length = std::max(step / 2, ticks(h.len) - step / 4);
            n.pitch = pitch;
            n.velocity = h.on == 0 ? 112 : 96;
            n.glide = prof.glide && prev >= 0 && prev != pitch && h.kind == octave;
            out.push_back(n);
            prev = pitch;
        }
    }
    applySwing(out, prof);
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

/* Phrase engine.
   1. A phrase form is picked for the style (AABA, ABAC, AAAB...). Each letter owns a rhythm cell, so a returning
      letter brings its rhythm back: that repetition is what makes a hook memorable.
   2. The first time a letter appears its melody is composed note by note: chord tones on strong beats, passing and
      neighbour tones on weak beats, a leap is followed by a step back the other way (gap-fill), and the line follows
      the vibe's contour. 'C' sections reach for the climax of the phrase.
   3. When a letter returns, its motif is moved to fit the new chord (a sequence) and its ending may change.
   4. The middle of the phrase rests on a chord tone (half cadence); the last bar resolves to the tonic.
   The engine writes many candidates, scores each one (see score) and keeps the best that has not come out recently.
   Decorations (second voice, overlaps, ornaments, anticipations, swing) are added to the winner. */
std::vector<Note> Engine::melodyAttempt(const Pattern& pat, const std::vector<Note>& keep, const std::array<bool, maxBars>& locked) {
    const auto& s = pat.settings;
    const auto& st = profile(pat.style);
    const auto vm = vibeMod(pat.vibe);
    const auto scale = ccp::HarmonyEngine::scaleNotes(s.key, s.scale);
    const bool counter = s.role == Role::counter;
    const int span = std::max(5, st.span + vm.spanAdd - (counter ? 2 : 0));
    const int maxLeap = std::max(2, st.leap + vm.leapAdd);
    const double rep = std::min(.75, st.rep + vm.repAdd + (s.role == Role::hook ? .1 : 0));

    int base = s.range == Range::low ? 55 : s.range == Range::high ? 69 : 62;
    if (counter) base -= 5;
    std::vector<int> pool;
    for (int m = base - 9; m <= base + 19; ++m) if (has({ scale.begin(), scale.end() }, wrap12(m))) pool.push_back(m);
    const int mid = (int) pool.size() / 2 - 1;
    const int lo = std::max(0, mid - span / 2), hi = std::min((int) pool.size() - 1, lo + span);
    auto clamp = [&](int i) { return std::clamp(i, lo, hi); };
    auto chordIdx = [&](const std::vector<int>& pcs, int from, int spread) {
        std::vector<int> c;
        for (int i = lo; i <= hi; ++i) if (has(pcs, wrap12(pool[(size_t) i]))) c.push_back(i);
        if (c.empty()) return clamp(from);
        std::shuffle(c.begin(), c.end(), rng);
        std::stable_sort(c.begin(), c.end(), [&](int a, int b) { return std::abs(a - from) < std::abs(b - from); });
        return c[(size_t) std::min<int>((int) c.size() - 1, std::uniform_int_distribution<int>(0, std::max(0, spread - 1))(rng))];
    };

    // phrase form, one letter per bar
    const int bars = pat.bars();
    std::vector<std::string> forms;
    { std::stringstream ss(st.forms); std::string f; while (std::getline(ss, f, '|')) forms.push_back(f); }
    std::string form = pick(rng, forms);
    if (s.role == Role::hook) form = chance(rng) < .5 ? "AAAB" : "AABA";
    std::string letters;
    if (bars == 2) letters = std::string(1, form[0]) + (form[1] == form[0] ? std::string("B") : std::string(1, form[1]));
    else if (bars == 4) letters = form;
    else letters = form + form.substr(0, 3) + "C";

    auto thin = [&](std::vector<Cell> c) {
        const Density d = counter ? Density::low : s.density;
        if (d == Density::low) {
            std::vector<Cell> o;
            for (size_t i = 0; i < c.size(); ++i) if (i % 2 == 0 || i + 1 == c.size()) o.push_back(c[i]);
            for (size_t i = 0; i + 1 < o.size(); ++i) o[i].len = o[i + 1].on - o[i].on;
            return o;
        }
        if (d == Density::high) {
            std::vector<Cell> o;
            for (auto x : c) if (x.len >= 2 && chance(rng) < .6) { o.push_back({ x.on, x.len / 2 }); o.push_back({ x.on + x.len / 2, x.len / 2 }); } else o.push_back(x);
            return o;
        }
        return c;
    };
    std::map<char, std::vector<Cell>> cellOf;
    {
        auto pool2 = st.cells;
        std::shuffle(pool2.begin(), pool2.end(), rng);
        size_t k = 0;
        for (char L : letters) if (!cellOf.count(L)) cellOf[L] = thin(pool2[k++ % pool2.size()]);
    }

    struct Ev { int on, len, idx; };
    std::map<char, std::vector<int>> motif;
    std::vector<Note> out;
    int idx = clamp((lo + hi) / 2 + vm.lean), lastLeap = 0;
    for (int b = 0; b < bars; ++b) {
        const auto& chord = pat.progression.chords[(size_t) b];
        auto pcs = chordPitchClasses(chord);
        const auto triad = triadOf(chord, scale);
        if (!st.ext) pcs = triad;
        const char L = letters[(size_t) b];
        const bool last = b == bars - 1, half = bars >= 4 && b == bars / 2 - 1;
        if (locked[(size_t) b]) {
            for (const auto& n : keep) if (barOf(n.start) == b) out.push_back(n);
            for (auto it = keep.rbegin(); it != keep.rend(); ++it)
                if (barOf(it->start) == b) { auto f = std::find(pool.begin(), pool.end(), it->pitch); if (f != pool.end()) idx = (int) (f - pool.begin()); break; }
            continue;
        }
        const auto& cell = cellOf[L];
        std::vector<Ev> bn;
        const double barPos = (double) b / std::max(1, bars - 1);
        if (motif.count(L) && !last) {
            // restatement: sequence the motif onto this chord, sometimes with a new ending
            const auto& m = motif[L];
            const int target = has(pcs, wrap12(pool[(size_t) m[0]])) && chance(rng) < .5 ? m[0] : chordIdx(triad, m[0], 1);
            const int shift = target - m[0];
            const bool newEnding = chance(rng) < .45;
            for (size_t i = 0; i < cell.size(); ++i) {
                int ni = clamp((i < m.size() ? m[i] : m.back()) + shift);
                const bool strong = std::fmod(cell[i].on, 4.0) == 0;
                if (strong && !has(pcs, wrap12(pool[(size_t) ni])) && chance(rng) < .75) ni = chordIdx(pcs, ni, 1);
                if (newEnding && i + 2 >= cell.size() && i > 0) ni = clamp(bn.back().idx + (chance(rng) < .5 ? 1 : -1) * (chance(rng) < .3 ? 2 : 1));
                bn.push_back({ b * barTicks + ticks(cell[i].on), ticks(cell[i].len), ni });
            }
            idx = bn.back().idx;
        } else {
            for (size_t i = 0; i < cell.size(); ++i) {
                const double o = cell[i].on;
                const bool strong = std::fmod(o, 8.0) == 0 || (std::fmod(o, 4.0) == 0 && chance(rng) < .6);
                const double phase = (b + o / 16.0) / bars;
                int dirBias = vm.contour != 0 ? (chance(rng) < .7 ? vm.contour : -vm.contour) : (phase < .55 ? 1 : -1);
                if (L == 'C' && phase < .85) dirBias = 1;                         // climax section climbs
                const bool lastInBar = i + 1 == cell.size();
                int ni;
                if (i == 0) ni = chordIdx(triad, idx + (L == 'B' ? dirBias : 0) + (L == 'C' ? 2 : 0), 2);
                else if (std::abs(lastLeap) >= 3) ni = idx - (lastLeap > 0 ? 1 : -1);    // gap-fill
                else if (!strong && chance(rng) < rep) ni = idx;
                else if (chance(rng) < st.arp) {
                    int found = -1;
                    if (dirBias > 0) { for (int k = idx + 1; k <= hi; ++k) if (has(triad, wrap12(pool[(size_t) k]))) { found = k; break; } }
                    else { for (int k = idx - 1; k >= lo; --k) if (has(triad, wrap12(pool[(size_t) k]))) { found = k; break; } }
                    ni = found >= 0 ? found : chordIdx(triad, idx, 2);
                }
                else if (strong || lastInBar) ni = chordIdx(pcs, idx + dirBias, 2);
                else {
                    const int leap = chance(rng) < .15 + barPos * .05 ? std::min(maxLeap, 2 + (int) (chance(rng) * 3)) : 1;
                    ni = idx + dirBias * leap;
                }
                ni = clamp(ni);
                lastLeap = ni - idx; idx = ni;
                bn.push_back({ b * barTicks + ticks(o), ticks(cell[i].len), ni });
            }
        }
        if (!motif.count(L)) { std::vector<int> m; for (auto& e : bn) m.push_back(e.idx); motif[L] = m; }
        if ((last || half) && !bn.empty()) {
            auto& t = bn.back();
            if (last) {   // nearest tonic, even if it sits just outside the phrase span
                int bestI = t.idx;
                for (int i = 0; i < (int) pool.size(); ++i)
                    if (wrap12(pool[(size_t) i]) == wrap12(s.key) && (wrap12(pool[(size_t) bestI]) != wrap12(s.key) || std::abs(i - t.idx) < std::abs(bestI - t.idx))) bestI = i;
                t.idx = bestI;
            }
            else {   // half cadence: rest on the chord's 3rd or 5th, not the root
                std::vector<int> open(triad.begin() + 1, triad.end());
                t.idx = chordIdx(open, t.idx, 1);
            }
            const int barEnd = (barOf(t.on) + 1) * barTicks;
            t.len = std::max(t.len, std::min(4 * step, barEnd - t.on));
            idx = t.idx;
        }
        for (size_t i = 0; i < bn.size(); ++i) {
            const auto& e = bn[i];
            const bool phraseEnd = (last || half) && i + 1 == bn.size();
            const double factor = phraseEnd ? 1.0 : st.stac;
            const bool onBeat = (e.on % ppq) == 0;
            const int vel = (onBeat ? 100 : 84) + (int) (chance(rng) * 16);
            if (i + 1 < bn.size() && st.appr > 0 && e.len >= 2 * step && (bn[i + 1].on % ppq) == 0 && chance(rng) < st.appr) {
                // chromatic approach: half-step below (or above, in jazz enclosures) the next target, on the last 16th
                const int target = pool[(size_t) bn[i + 1].idx];
                const int app = pat.style == Style::jazz && chance(rng) < .4 ? target + 1 : target - 1;
                out.push_back({ e.on, (int) std::max<double>(step / 2, (e.len - step) * factor), pool[(size_t) e.idx], vel, false });
                out.push_back({ e.on + e.len - step, step - step / 8, app, vel - 14, false });
            } else {
                out.push_back({ e.on, (int) std::max<double>(step / 2, e.len * factor - (factor < 1 ? 0 : step / 8)), pool[(size_t) e.idx], vel, false });
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

double Engine::score(const Pattern& pat, const std::vector<Note>& m) {
    if (m.size() < 3) return -100;
    const auto& s = pat.settings;
    const auto scale = ccp::HarmonyEngine::scaleNotes(s.key, s.scale);
    const int leadingTone = wrap12(s.key + 11);
    const bool repeatsOk = pat.style == Style::funkBR || pat.style == Style::phonk || pat.style == Style::house || pat.vibe == Vibe::hypnotic;
    double sc = 0;
    int strong = 0, strongHits = 0, steps = 0, leaps = 0, bigLeaps = 0, gapFills = 0, runs = 0, run = 1, sameDir = 0, longSameDir = 0;
    int lo = 127, hi = 0;
    std::vector<int> distinct;
    for (size_t i = 0; i < m.size(); ++i) {
        const auto& n = m[i];
        lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch);
        if (!has(distinct, n.pitch)) distinct.push_back(n.pitch);
        const int bar = std::min(pat.bars() - 1, barOf(n.start));
        if (n.start % ppq == 0) {
            ++strong;
            if (has(chordPitchClasses(pat.progression.chords[(size_t) bar]), wrap12(n.pitch))) ++strongHits;
        }
        if (i) {
            const int d = n.pitch - m[i - 1].pitch, ad = std::abs(d);
            if (ad > 0 && ad <= 2) ++steps;
            if (ad > 7) ++leaps;
            if (ad > 12) ++bigLeaps;
            if (i >= 2) {
                const int pd = m[i - 1].pitch - m[i - 2].pitch;
                if (std::abs(pd) >= 5 && ad > 0 && ad <= 2 && (pd > 0) != (d > 0)) ++gapFills;
                if (d != 0 && pd != 0 && (d > 0) == (pd > 0)) { if (++sameDir > 4) ++longSameDir; } else sameDir = 0;
            }
            if (d == 0) { if (++run > 3) ++runs; } else run = 1;
            if (wrap12(m[i - 1].pitch) == leadingTone && has({ scale.begin(), scale.end() }, leadingTone))
                sc += wrap12(n.pitch) == wrap12(s.key) ? 1.0 : -0.5;
        }
    }
    sc += strong ? 12.0 * strongHits / strong : 0;
    sc += 5.0 * steps / (double) m.size();
    sc += 1.5 * gapFills;
    sc -= 2.5 * leaps + 4.0 * bigLeaps + (repeatsOk ? 0.5 : 1.5) * runs + 1.5 * longSameDir;
    const int range = hi - lo;
    sc += range >= 5 && range <= 16 ? 3 : -2;
    sc += distinct.size() >= 4 ? 2 : -3;
    int climaxBars = 0, prevBar = -1;
    for (auto& n : m) if (n.pitch == hi && barOf(n.start) != prevBar) { ++climaxBars; prevBar = barOf(n.start); }
    if (climaxBars == 1) sc += 2;
    if (wrap12(m.back().pitch) == wrap12(s.key)) sc += 4;
    return sc;
}

uint64_t Engine::signature(const std::vector<Note>& m) const {
    uint64_t h = 1469598103934665603ull;
    for (auto& n : m) { h ^= (uint64_t) (n.start * 131 + n.pitch); h *= 1099511628211ull; }
    return h;
}

std::vector<Note> Engine::melodyFor(const Pattern& pat, const std::vector<Note>& keep, const std::array<bool, maxBars>& locked) {
    std::vector<Note> best;
    double bestScore = -1e9;
    std::uniform_real_distribution<double> jitter(-1.5, 1.5);
    for (int attempt = 0; attempt < 32; ++attempt) {
        auto m = melodyAttempt(pat, keep, locked);
        if (std::find(recent.begin(), recent.end(), signature(m)) != recent.end()) continue;
        const double sc = score(pat, m) + jitter(rng);
        if (sc > bestScore) { bestScore = sc; best = std::move(m); }
    }
    if (best.empty()) best = melodyAttempt(pat, keep, locked);
    recent.push_back(signature(best));
    while (recent.size() > 64) recent.pop_front();

    // ---- decorations on the unlocked bars of the winning line
    const auto& st = profile(pat.style);
    const auto vm = vibeMod(pat.vibe);
    const auto& s = pat.settings;
    const auto scale = ccp::HarmonyEngine::scaleNotes(s.key, s.scale);
    std::vector<int> pool;
    for (int m = 30; m <= 100; ++m) if (has({ scale.begin(), scale.end() }, wrap12(m))) pool.push_back(m);
    auto poolIndex = [&](int pitch) { auto f = std::lower_bound(pool.begin(), pool.end(), pitch); return (int) (f - pool.begin()); };
    auto free = [&](int b) { return b >= 0 && b < pat.bars() && !locked[(size_t) b]; };
    std::vector<Note> main, extra;
    for (auto& n : best) (free(barOf(n.start)) ? main : extra).push_back(n);   // extra = locked bars, untouched

    // anticipation: a downbeat note arrives one 16th early (syncopated pop / bossa / reggaeton phrasing)
    for (size_t i = 1; i < main.size(); ++i) {
        auto& n = main[i];
        if (n.start % barTicks == 0 && free(barOf(n.start) - 1) && main[i - 1].start + main[i - 1].length <= n.start - step && chance(rng) < st.anticipate)
            { n.start -= step; n.length += step; }
    }
    // legato overlap: ring into the next note when the interval is consonant
    const double overlap = std::min(.6, st.overlap + vm.overlapAdd);
    for (size_t i = 0; i + 1 < main.size(); ++i) {
        auto& n = main[i]; const auto& nx = main[i + 1];
        if (nx.start - n.start >= step && consonant(n.pitch, nx.pitch) && n.pitch != nx.pitch && chance(rng) < overlap)
            n.length = std::max(n.length, nx.start - n.start + step * (1 + (int) (chance(rng) * 2)));
    }
    // second voice
    const double voice = std::min(.85, st.voice + vm.voiceAdd) * (s.role == Role::counter ? .3 : 1.0);
    std::vector<Note> voices;
    if (st.texture == Texture::pedal) {
        for (int b = 0; b < pat.bars(); ++b) {
            if (!free(b) || chance(rng) > voice) continue;
            int top = 0;
            for (auto& n : main) if (barOf(n.start) == b) top = std::max(top, n.pitch);
            if (!top) continue;
            const auto pcs = chordPitchClasses(pat.progression.chords[(size_t) b]);
            for (int p = top + 1; p <= top + 9; ++p)
                if (has(pcs, wrap12(p)) && has({ scale.begin(), scale.end() }, wrap12(p))) { voices.push_back({ b * barTicks, barTicks - step, p, 68, false }); break; }
        }
    } else if (st.texture != Texture::none) {
        for (auto& n : main) {
            const bool longNote = n.length >= 2 * step;
            if (!longNote || chance(rng) > voice) continue;
            const int pi = poolIndex(n.pitch);
            int v = -1;
            switch (st.texture) {
                case Texture::thirds: if (pi >= 2) v = pool[(size_t) (pi - 2)]; break;
                case Texture::sixths: if (pi >= 5) v = pool[(size_t) (pi - 5)]; break;
                case Texture::octaves: v = n.pitch - 12; break;
                case Texture::shell: {
                    const auto pcs = chordPitchClasses(pat.progression.chords[(size_t) std::min(pat.bars() - 1, barOf(n.start))]);
                    for (int p = n.pitch - 3; p >= n.pitch - 9; --p) if (has(pcs, wrap12(p)) && consonant(p, n.pitch)) { v = p; break; }
                    break;
                }
                default: break;
            }
            if (v > 40) voices.push_back({ n.start, n.length, v, std::max(40, n.velocity - 22), false });
        }
    }
    // grace notes: a quick upper neighbour (or chromatic lower neighbour in jazz / R&B) into a strong note
    std::vector<Note> graces;
    for (size_t i = 1; i < main.size(); ++i) {
        const auto& n = main[i];
        if (n.start % ppq != 0 || !free(barOf(n.start - step / 2)) || chance(rng) > st.grace) continue;
        if (main[i - 1].start + main[i - 1].length > n.start - step / 2) continue;
        const bool chromatic = (pat.style == Style::jazz || pat.style == Style::rnb) && chance(rng) < .5;
        const int g = chromatic ? n.pitch - 1 : pool[(size_t) std::min((int) pool.size() - 1, poolIndex(n.pitch) + 1)];
        graces.push_back({ n.start - step / 2, step / 2 - 10, g, std::max(40, n.velocity - 20), false });
    }
    if (st.glide)
        for (size_t i = 1; i < main.size(); ++i)
            main[i].glide = main[i - 1].start + main[i - 1].length >= main[i].start - step / 2 && chance(rng) < .3;

    std::vector<Note> all = main;
    all.insert(all.end(), voices.begin(), voices.end());
    all.insert(all.end(), graces.begin(), graces.end());
    applySwing(all, st);
    all.insert(all.end(), extra.begin(), extra.end());
    for (auto& n : all) n.length = std::min(n.length, pat.lengthTicks() - n.start);
    all.erase(std::remove_if(all.begin(), all.end(), [](const Note& n) { return n.length <= 0 || n.start < 0; }), all.end());
    std::sort(all.begin(), all.end(), [](const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch > b.pitch; });
    return all;
}

Pattern Engine::generate(const Settings& s, const Pattern* current, const std::array<bool, maxBars>& locked) {
    Pattern out;
    out.settings = s;
    const bool sameShape = current && current->bars() == s.bars;
    bool anyLock = false;
    if (sameShape) for (int i = 0; i < s.bars; ++i) anyLock |= locked[(size_t) i];
    // with locked bars, an automatic style / vibe keeps what it picked so the locked music still fits
    out.style = anyLock && s.style == Style::automatic ? current->style : resolveStyle(s.style);
    out.vibe = anyLock && s.vibe == Vibe::automatic ? current->vibe : resolveVibe(s.vibe, out.style);
    std::array<bool, ccp::maxChords> chordLocks {};
    if (sameShape) for (int i = 0; i < s.bars; ++i) chordLocks[(size_t) i] = locked[(size_t) i];
    out.progression = harmony.generate(harmonySettings(out.style, out.vibe, s), sameShape ? &current->progression : nullptr, chordLocks, s.bars);
    out.bass = bassFor(out);
    if (sameShape)
        for (int b = 0; b < s.bars; ++b) if (locked[(size_t) b]) {
            out.bass.erase(std::remove_if(out.bass.begin(), out.bass.end(), [&](const Note& n) { return barOf(n.start) == b; }), out.bass.end());
            for (auto& n : current->bass) if (barOf(n.start) == b) out.bass.push_back(n);
        }
    std::sort(out.bass.begin(), out.bass.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    std::array<bool, maxBars> none {};
    out.melody = melodyFor(out, sameShape ? current->melody : std::vector<Note> {}, sameShape ? locked : none);
    return out;
}

Pattern Engine::newMelody(const Pattern& p, const Settings& s, const std::array<bool, maxBars>& locked) {
    Pattern out = p;
    out.settings.role = s.role; out.settings.density = s.density; out.settings.range = s.range;
    out.melody = melodyFor(out, p.melody, locked);
    return out;
}

Pattern Engine::newBass(const Pattern& p) {
    Pattern out = p;
    out.bass = bassFor(out);
    return out;
}

Pattern Engine::variation(const Pattern& p, const std::array<bool, maxBars>& locked) {
    Pattern out = p;
    const auto scale = ccp::HarmonyEngine::scaleNotes(p.settings.key, p.settings.scale);
    auto inScale = [&](int m) { return has({ scale.begin(), scale.end() }, wrap12(m)); };
    bool changed = false;
    for (int tries = 0; tries < 6 && !changed; ++tries)
        for (auto& n : out.melody) {
            const int bar = barOf(n.start);
            if (bar < maxBars && locked[(size_t) bar]) continue;
            if (n.start % ppq != 0 && chance(rng) < .4) {
                const int dir = chance(rng) < .5 ? 1 : -1;
                int m = n.pitch + dir;
                while (!inScale(m)) m += dir;
                if (std::abs(m - n.pitch) <= 3) { n.pitch = m; changed = true; }
            }
        }
    return out;
}

Pattern Engine::setChord(const Pattern& p, int bar, const ccp::Chord& chord) {
    Pattern out = p;
    if (bar < 0 || bar >= p.bars()) return out;
    const auto gs = harmonySettings(p.style, p.vibe, p.settings);
    const ccp::Chord* prev = bar ? &out.progression.chords[(size_t) (bar - 1)] : nullptr;
    out.progression.chords[(size_t) bar] = ccp::HarmonyEngine::rebuild(gs, chord, prev, bar);
    for (int i = bar + 1; i < out.bars(); ++i)
        out.progression.chords[(size_t) i] = ccp::HarmonyEngine::rebuild(gs, out.progression.chords[(size_t) i], &out.progression.chords[(size_t) (i - 1)], i);
    // move this bar's sub to the new root (keeping its rhythm), then rewrite only this bar's melody
    const int shift = subPitch(out.progression.chords[(size_t) bar]) - subPitch(p.progression.chords[(size_t) bar]);
    for (auto& n : out.bass) if (barOf(n.start) == bar) n.pitch += shift;
    if (bar > 0) { // the previous bar's last sub note leads into the new root
        Note* lastPrev = nullptr;
        for (auto& n : out.bass) if (barOf(n.start) == bar - 1) lastPrev = &n;
        const int prevRoot = subPitch(out.progression.chords[(size_t) (bar - 1)]);
        if (lastPrev && lastPrev->pitch != prevRoot && lastPrev->pitch != prevRoot + 12 && lastPrev->pitch != prevRoot + 7 && lastPrev->pitch != prevRoot - 5)
            lastPrev->pitch = subPitch(out.progression.chords[(size_t) bar]) - 1;
    }
    std::array<bool, maxBars> locks {};
    for (int i = 0; i < out.bars(); ++i) locks[(size_t) i] = i != bar;
    out.melody = melodyFor(out, out.melody, locks);
    return out;
}

namespace {
void be16(std::vector<uint8_t>& o, uint16_t x) { o.push_back((uint8_t) (x >> 8)); o.push_back((uint8_t) x); }
void be32(std::vector<uint8_t>& o, uint32_t x) { for (int s = 24; s >= 0; s -= 8) o.push_back((uint8_t) (x >> s)); }
void vlq(std::vector<uint8_t>& o, uint32_t v) {
    uint8_t b[5]; int n = 0; b[n++] = v & 127;
    while ((v >>= 7)) b[n++] = (uint8_t) (128 | (v & 127));
    while (n) o.push_back(b[--n]);
}
}

std::vector<uint8_t> midiBytes(const std::vector<Note>& notes, int bpm, int channel) {
    struct Ev { int tick; bool on; int note, vel; };
    std::vector<Ev> ev;
    for (auto& n : notes) { ev.push_back({ n.start, true, n.pitch, n.velocity }); ev.push_back({ n.start + std::max(1, n.length), false, n.pitch, 0 }); }
    std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : (!a.on && b.on); });
    std::vector<uint8_t> tr;
    const uint32_t us = (uint32_t) (60000000 / std::max(1, bpm));
    vlq(tr, 0); tr.insert(tr.end(), { 0xff, 0x51, 0x03, (uint8_t) (us >> 16), (uint8_t) (us >> 8), (uint8_t) us });
    int last = 0;
    const uint8_t ch = (uint8_t) (channel & 15);
    for (auto& e : ev) {
        vlq(tr, (uint32_t) std::max(0, e.tick - last)); last = e.tick;
        tr.push_back((uint8_t) ((e.on ? 0x90 : 0x80) | ch)); tr.push_back((uint8_t) std::clamp(e.note, 0, 127)); tr.push_back((uint8_t) std::clamp(e.vel, 0, 127));
    }
    vlq(tr, 0); tr.insert(tr.end(), { 0xff, 0x2f, 0 });
    std::vector<uint8_t> out { 'M', 'T', 'h', 'd' };
    be32(out, 6); be16(out, 0); be16(out, 1); be16(out, ppq);
    out.insert(out.end(), { 'M', 'T', 'r', 'k' }); be32(out, (uint32_t) tr.size());
    out.insert(out.end(), tr.begin(), tr.end());
    return out;
}

const char* styleName(Style x) {
    static const char* n[] { "PADRÃO", "FUNK BR", "POP", "TRAP", "PHONK", "DRILL", "PISEIRO", "SERTANEJO", "PAGODE",
                             "BOSSA NOVA", "JAZZ", "R&B", "REGGAETON", "AFROBEAT", "HOUSE", "LO-FI" };
    return n[(int) x];
}
const char* vibeName(Vibe x) { static const char* n[] { "PADRÃO", "DARK", "HAPPY", "SAD", "HYPNOTIC", "EPIC", "CHILL", "ROMANTIC" }; return n[(int) x]; }
}
