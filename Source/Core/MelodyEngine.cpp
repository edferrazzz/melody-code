#include "MelodyEngine.h"
#include "Composer.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace mc {
namespace {
struct Cell { double on, len; };                     // in 16ths; thirds of a 16th give triplets
enum BassKind { root, octave, fifth, approach, third };
struct Hit { double on, len; int kind; };
enum class Texture { none, thirds, sixths, shell, octaves, pedal };
constexpr double T = 4.0 / 3.0;                      // one 8th-note triplet, in 16ths

/* One profile per style: how a producer writes a hook in that genre.
   rep         chance of repeating the previous pitch on a weak beat (hooks repeat)
   stac        note-length factor (short = punchy, 1 = legato)
   leap        largest jump, in scale steps; span = range of the line, in semitones
   pent        how strictly the line stays on the pentatonic (the "catchy" notes) off the strong beats
   appr        chance of a chromatic approach note into the next target (jazz, R&B)
   arp         chance of outlining the chord instead of stepping
   ext         sevenths/ninths count as chord tones on strong beats
   swing8      the swing button swings 8ths (jazz, lo-fi) instead of 16ths
   anticipate  chance a downbeat note arrives a 16th early
   grace       chance of a short ornament before a strong note
   overlap     chance a note rings into the next one (only on consonant intervals)
   texture     the second voice this style uses, and how often (voice)
   scales      scales PADRÃO picks from for this style
   cells       one-bar rhythm cells for the call; the response is derived from the call
   bass        one-bar low-note rhythms (root / octave / fifth / third / approach) */
struct Profile {
    ccp::Style harmony;
    double rep, stac; int leap, span; double pent, appr, arp; bool ext, swing8;
    double anticipate, grace, overlap;
    Texture texture; double voice;
    std::vector<ccp::ScaleType> scales;
    std::vector<std::vector<Cell>> cells;
    std::vector<std::vector<Hit>> bass;
};

using S = ccp::ScaleType;
const Profile& profile(Style s) {
    static const std::map<Style, Profile> table {
        { Style::funkBR, { ccp::Style::funkBrasileiro, .45, .8, 3, 5, .9, .0, .15, false, false, .1, .05, 0, Texture::octaves, .15,
            { S::naturalMinor, S::phrygian, S::harmonicMinor, S::naturalMinor },
            { {{0,2},{3,1},{6,2},{10,2},{12,3}}, {{0,1},{3,2},{6,1},{8,2},{11,1},{14,2}}, {{0,3},{3,3},{6,2},{10,1},{12,2}}, {{0,1},{2,1},{3,2},{7,1},{10,2},{13,1}} },
            { {{0,3,root},{3,3,root},{6,2,octave},{10,2,root},{12,4,root}}, {{0,6,root},{6,2,octave},{8,6,root},{14,2,approach}} } } },
        { Style::pop, { ccp::Style::pop, .3, .92, 3, 7, .8, .04, .15, false, false, .3, .06, .12, Texture::thirds, .12,
            { S::naturalMinor, S::harmonicMinor, S::naturalMinor, S::major },
            { {{0,4},{4,2},{6,2},{8,6}}, {{0,2},{2,2},{4,4},{10,2},{12,4}}, {{2,2},{4,2},{6,4},{12,4}}, {{0,2},{2,2},{4,2},{6,2},{8,8}}, {{2,2},{4,2},{6,2},{8,2},{10,6}} },
            { {{0,8,root},{8,6,root},{14,2,approach}}, {{0,6,root},{6,2,fifth},{8,8,root}}, {{0,4,root},{4,4,root},{8,4,root},{12,4,octave}} } } },
        { Style::trap, { ccp::Style::trap, .35, .9, 3, 6, .85, .06, .12, false, false, .1, .05, .12, Texture::pedal, .2,
            { S::naturalMinor, S::harmonicMinor, S::phrygian },
            { {{0,3},{3,3},{6,2},{8,3},{11,3},{14,2}}, {{0,3},{3,3},{6,4},{12,4}}, {{0,6},{6,6},{12,4}}, {{0,1},{1,1},{2,2},{6,2},{8,1},{9,1},{10,4}}, {{0,2},{3,3},{8,1},{9,1},{10,2},{12,4}} },
            { {{0,10,root},{10,6,root}}, {{0,6,root},{6,2,root},{8,8,root}}, {{0,3,root},{3,5,root},{11,5,octave}} } } },
        { Style::phonk, { ccp::Style::phonk, .4, .75, 3, 6, .8, .05, .2, false, false, .05, 0, 0, Texture::octaves, .3,
            { S::phrygian, S::naturalMinor },
            { {{0,2},{2,2},{4,1},{6,2},{8,2},{10,2},{12,4}}, {{0,3},{3,3},{6,2},{8,3},{11,3},{14,2}}, {{0,1},{1,1},{2,2},{4,1},{5,1},{6,2},{8,2},{10,2},{12,2},{14,2}} },
            { {{0,4,root},{6,2,root},{8,4,root},{14,2,octave}}, {{0,3,root},{3,3,root},{6,10,root}} } } },
        { Style::drill, { ccp::Style::trap, .3, .85, 3, 7, .75, .1, .1, false, false, .15, .1, .1, Texture::pedal, .2,
            { S::harmonicMinor, S::phrygian, S::naturalMinor },
            { {{0,3},{3,1},{4,4},{10,2},{12,4}}, {{0,2},{3,2},{6,2},{10,2},{12,4}}, {{0,3},{3,3},{8,3},{11,5}} },
            { {{0,3,root},{3,3,root},{6,4,octave},{11,5,root}}, {{0,6,root},{6,4,root},{10,6,octave}} } } },
        { Style::bossaNova, { ccp::Style::jazz, .12, 1.0, 5, 14, .55, .15, .2, true, false, .4, .1, .25, Texture::shell, .3,
            { S::major, S::dorian, S::lydian },
            { {{0,3},{3,3},{6,4},{10,2},{12,4}}, {{2,2},{4,3},{7,3},{10,6}}, {{0,2},{2,4},{6,2},{8,4},{14,2}}, {{0,6},{6,2},{8,2},{10,6}} },
            { {{0,6,root},{6,2,fifth},{8,6,fifth},{14,2,root}}, {{0,6,root},{6,2,root},{8,6,fifth},{14,2,approach}} } } },
        { Style::jazz, { ccp::Style::jazz, .08, .95, 5, 15, .45, .35, .3, true, true, .2, .25, .1, Texture::shell, .3,
            { S::major, S::dorian, S::melodicMinor },
            { {{0,2},{2,2},{4,2},{6,2},{8,2},{10,2},{12,4}}, {{2,2},{4,2},{6,4},{10,2},{12,2},{14,2}}, {{0,6},{6,2},{8,2},{10,2},{12,4}}, {{2,2},{4,2},{6,2},{8,4},{12,4}} },
            { {{0,4,root},{4,4,third},{8,4,fifth},{12,4,approach}}, {{0,4,root},{4,4,fifth},{8,4,octave},{12,4,approach}} } } },
        { Style::rnb, { ccp::Style::rnb, .12, 1.0, 4, 9, .7, .25, .2, true, false, .25, .3, .3, Texture::sixths, .2,
            { S::dorian, S::naturalMinor, S::major },
            { {{0,3},{3,1},{4,4},{10,2},{12,4}}, {{1,2},{3,3},{8,3},{12,4}}, {{0,1},{1,1},{2,4},{7,1},{8,6}}, {{0,4},{6,2},{8,2},{10,6}} },
            { {{0,10,root},{10,2,fifth},{12,4,approach}}, {{0,6,root},{7,3,root},{12,4,octave}} } } },
        { Style::reggaeton, { ccp::Style::latin, .3, .8, 3, 7, .85, .05, .2, false, false, .2, .1, .05, Texture::thirds, .2,
            { S::naturalMinor, S::harmonicMinor, S::major },
            { {{0,3},{3,3},{6,2},{8,3},{11,3},{14,2}}, {{0,2},{3,1},{4,4},{8,2},{11,1},{12,4}}, {{3,3},{6,2},{11,3},{14,2}} },
            { {{0,3,root},{3,3,root},{6,2,octave},{8,3,root},{11,3,root},{14,2,octave}} } } },
        { Style::house, { ccp::Style::house, .45, .75, 3, 7, .85, 0, .2, true, false, .1, 0, .05, Texture::octaves, .25,
            { S::dorian, S::naturalMinor, S::major },
            { {{2,2},{6,2},{10,2},{14,2}}, {{0,2},{2,2},{6,4},{10,2},{12,2}}, {{2,1},{4,2},{6,1},{10,1},{12,2},{14,1}} },
            { {{2,2,root},{6,2,root},{10,2,root},{14,2,octave}}, {{0,2,root},{3,3,root},{8,2,root},{11,5,root}} } } },
        { Style::loFi, { ccp::Style::loFi, .12, 1.0, 3, 8, .7, .2, .25, true, true, .2, .1, .35, Texture::shell, .2,
            { S::major, S::dorian, S::naturalMinor },
            { {{0,6},{8,2},{10,6}}, {{2,4},{8,4},{13,3}}, {{0,3},{4,2},{6,6}}, {{0,2},{2,2},{4,4},{10,6}} },
            { {{0,12,root},{12,4,approach}}, {{0,7,root},{8,8,fifth}} } } },
    };
    return table.at(s);
}

// Triplet feel: rhythm cells built on 8th-note triplets (trap flows, 6/8-ish rolls, R&B runs).
const std::vector<std::vector<Cell>>& tripletCells() {
    static const std::vector<std::vector<Cell>> c {
        { {0,T},{T,T},{2*T,T},{4,4},{8,2*T},{8+2*T,T},{12,4} },
        { {0,2*T},{2*T,T},{4,T},{4+T,T},{4+2*T,T},{8,4},{12,2*T},{12+2*T,T} },
        { {0,4},{4,T},{4+T,T},{4+2*T,T},{8,2*T},{8+2*T,T},{12,4} },
        { {2,2},{4,2*T},{4+2*T,T},{8,T},{8+T,T},{8+2*T,T},{12,4} },
        { {0,2*T},{2*T,2*T},{4+T,2*T},{8,4},{12,T},{12+T,T},{12+2*T,T} },
    };
    return c;
}

/* Harmonic rhythm per style, 4 bars at a time: each value is where that bar's chord starts, in 16ths from the
   bar line (H = hold the previous chord). */
constexpr int H = 99;
const std::vector<std::array<int, 4>>& harmonicRhythm(Style s) {
    static const std::map<Style, std::vector<std::array<int, 4>>> t {
        { Style::funkBR,    { { 0, H, 0, 0 }, { 0, 0, H, H }, { 0, H, 0, H }, { 0, 0, 0, 0 } } },
        { Style::pop,       { { 0, 0, 0, 0 }, { 0, -1, 0, -1 }, { 0, 0, 0, 8 }, { 0, 0, -1, 0 } } },
        { Style::trap,      { { 0, H, 0, H }, { 0, 8, 0, 8 }, { 0, H, 0, 0 }, { 0, 0, 0, 0 } } },
        { Style::phonk,     { { 0, H, 0, H }, { 0, 0, 0, 0 }, { 0, 0, 0, 8 } } },
        { Style::drill,     { { 0, H, 0, H }, { 0, 8, 0, 8 }, { 0, 0, 0, 0 } } },
        { Style::bossaNova, { { 0, H, 0, 0 }, { 0, -1, 0, -1 }, { 0, 0, -2, 0 } } },
        { Style::jazz,      { { 0, 8, 0, 8 }, { 0, 0, 0, 8 }, { 0, -2, 0, 0 } } },
        { Style::rnb,       { { 0, -1, 0, -1 }, { 0, 4, 0, 0 }, { 0, 0, -1, 0 }, { 0, 0, 0, 0 } } },
        { Style::reggaeton, { { 0, 0, 0, 0 }, { 0, 0, 0, -1 } } },
        { Style::house,     { { 0, 0, 0, 0 }, { 0, 8, 0, 0 }, { 0, -2, 0, -2 } } },
        { Style::loFi,      { { 0, H, 0, H }, { 0, 0, H, 0 }, { 0, -1, 0, 0 } } },
    };
    return t.at(s);
}

double chance(std::mt19937& r) { return std::uniform_real_distribution<double>(0, 1)(r); }
int dice(std::mt19937& r, int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(r); }
template <typename V> const typename V::value_type& pick(std::mt19937& r, const V& v) {
    return v[(size_t) std::uniform_int_distribution<int>(0, (int) v.size() - 1)(r)];
}
int wrap12(int n) { n %= 12; return n < 0 ? n + 12 : n; }
bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
int ticks(double sixteenths) { return (int) std::lround(sixteenths * step); }
bool consonant(int a, int b) { const int d = std::abs(a - b) % 12; return d == 0 || d == 3 || d == 4 || d == 5 || d == 7 || d == 8 || d == 9; }
int barOf(int tick) { return std::max(0, tick) / barTicks; }

void applySwing(std::vector<Note>& notes, const Profile& p, Feel feel) {
    if (feel != Feel::swing) return;
    for (auto& n : notes) {
        if (p.swing8) { if (n.start % ppq == ppq / 2) n.start += ppq / 6; }            // triplet-swung 8ths
        else if (n.start % (2 * step) == step) n.start += step / 3;                      // swung 16ths
    }
}
}

ccp::GenerationSettings harmonySettings(Style style, int key, ccp::ScaleType scale) {
    ccp::GenerationSettings g;
    g.key = key; g.scale = scale;
    g.style = style == Style::automatic ? ccp::Style::standard : profile(style).harmony;
    g.mood = ccp::Mood::standard;
    g.complexity = ccp::Complexity::simple;   // simple chords: triads, plain sevenths at most
    return g;
}

Engine::Engine(uint32_t seed) : rng(seed), harmony(seed ^ 0x9e3779b9u) {}

Style Engine::resolveStyle(Style s) {
    if (s != Style::automatic) return s;
    return (Style) dice(rng, 1, (int) Style::count - 1);
}

std::vector<int> Engine::chordPitchClasses(const ccp::Chord& c) {
    std::vector<int> pcs;
    for (int i = 0; i < c.noteCount; ++i) if (!has(pcs, wrap12(c.notes[(size_t) i]))) pcs.push_back(wrap12(c.notes[(size_t) i]));
    return pcs;
}

int Engine::bassRoot(const ccp::Chord& c) {
    int n = 36 + c.root;        // C2..B2
    if (n < 40) n += 12;        // keep it between E2 and D#3: low, but still clear in the same timbre
    return n;
}

std::vector<Note> Engine::bassFor(const Pattern& pat) {
    const auto& prof = profile(pat.style);
    const auto& tpl = pick(rng, prof.bass);
    const int bars = pat.bars();
    std::vector<Note> out;
    auto pitchFor = [&](int tick, int kind) {
        const int ci = pat.chordIndexAt(tick);
        const auto& chord = pat.progression.chords[(size_t) ci];
        const int r = bassRoot(chord);
        int nextCi = ci, t2 = tick;
        while (nextCi == ci && t2 < pat.lengthTicks()) { t2 += step; nextCi = pat.chordIndexAt(t2); }
        const int next = bassRoot(pat.progression.chords[(size_t) (t2 >= pat.lengthTicks() ? 0 : nextCi)]);
        int third = r + 4;
        for (int pc : chordPitchClasses(chord)) { const int iv = wrap12(pc - chord.root); if (iv == 3 || iv == 4) third = r + iv; }
        if (kind == octave) return r + 12 <= 55 ? r + 12 : r;
        if (kind == fifth) return r + 7 <= 55 ? r + 7 : r - 5;
        if (kind == BassKind::third) return third;
        if (kind == approach) return next == r ? (r + 7 <= 55 ? r + 7 : r - 5) : next + (next > r ? -1 : 1);
        return r;
    };
    for (int b = 0; b < bars; ++b) {
        for (size_t i = 0; i < tpl.size(); ++i) {
            const auto& h = tpl[i];
            if (pat.settings.density == Density::low && i > 0 && h.kind != root && i % 2 == 1) continue;
            Note n;
            n.start = b * barTicks + ticks(h.on);
            n.length = std::max(step / 2, ticks(h.len) - step / 4);
            n.pitch = pitchFor(n.start, h.kind);
            n.velocity = h.on == 0 ? 100 : 86;
            out.push_back(n);
        }
        const int change = b * barTicks + pat.chordShift[(size_t) b] * step;   // a chord off the bar line gets its own note
        if (b > 0 && pat.chordShift[(size_t) b] != 0) {
            out.erase(std::remove_if(out.begin(), out.end(), [&](const Note& n) { return std::abs(n.start - change) < step; }), out.end());
            out.push_back({ change, 2 * step, pitchFor(change, root), 98 });
        }
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    for (size_t i = 0; i + 1 < out.size(); ++i) out[i].length = std::min(out[i].length, std::max(step / 2, out[i + 1].start - out[i].start - step / 8));
    applySwing(out, prof, pat.settings.feel);
    return out;
}

void Engine::shapeHarmony(Pattern& p) {
    const auto gs = harmonySettings(p.style, p.key, p.scale);
    const bool sevenths = p.style == Style::jazz || p.style == Style::rnb || p.style == Style::loFi || p.style == Style::bossaNova;
    const int n = p.bars();
    for (int b = 0; b < n; ++b) {   // simple chords: diatonic triads, plain sevenths for the jazzy styles
        auto& c = p.progression.chords[(size_t) b];
        const auto kind = sevenths && chance(rng) < .7 ? ccp::ChordKind::seventh : ccp::ChordKind::triad;
        c = ccp::HarmonyEngine::buildChord(gs, c.degree, kind, b ? &p.progression.chords[(size_t) (b - 1)] : nullptr, b);
    }
    const auto& tpl = pick(rng, harmonicRhythm(p.style));
    p.chordShift = {};
    for (int b = 1; b < n; ++b) {
        int v = tpl[(size_t) (b % 4)];
        if (n == 8 && b >= 4 && chance(rng) < .3) v = 0;
        if (v == H) { p.progression.chords[(size_t) b] = p.progression.chords[(size_t) (b - 1)]; v = 0; }
        p.chordShift[(size_t) b] = v;
    }
}

uint64_t Engine::signature(const std::vector<Note>& m) const {
    uint64_t h = 1469598103934665603ull;
    for (auto& n : m) { h ^= (uint64_t) (n.start * 131 + n.pitch); h *= 1099511628211ull; }
    return h;
}

// A new melody for the pattern with a fresh seed, avoiding results heard recently.
std::vector<Note> Engine::melodyFor(Pattern& pat) {
    std::vector<Note> m;
    for (int tries = 0; tries < 4; ++tries) {
        pat.seed = (uint32_t) dice(rng, 1, 0x7fffffff);
        m = composer::compose(pat, pat.seed);
        if (std::find(recent.begin(), recent.end(), signature(m)) == recent.end()) break;
    }
    recent.push_back(signature(m));
    while (recent.size() > 64) recent.pop_front();
    return m;
}

Pattern Engine::generate(const Settings& s) {
    Pattern out;
    out.settings = s;
    out.settings.bars = std::clamp(s.bars, 1, maxBars);
    out.style = resolveStyle(s.style);
    const auto& prof = profile(out.style);
    out.key = s.key >= 0 && s.key < 12 ? s.key : dice(rng, 0, 11);
    out.scale = s.scale >= 0 && s.scale < (int) ccp::ScaleType::count ? (ccp::ScaleType) s.scale : pick(rng, prof.scales);
    out.progression = harmony.generate(harmonySettings(out.style, out.key, out.scale), nullptr, {}, out.settings.bars);
    shapeHarmony(out);
    out.bass = bassFor(out);
    out.melody = melodyFor(out);
    return out;
}

Pattern Engine::newMelody(const Pattern& p, const Settings& s) {
    // same context (key, scale, chords, bass), a different musical idea from a new seed
    Pattern out = p;
    out.settings.role = s.role; out.settings.density = s.density; out.settings.range = s.range;
    out.settings.feel = s.feel; out.settings.vibe = s.vibe;
    out.melody = melodyFor(out);
    if (s.feel != p.settings.feel) out.bass = bassFor(out);
    return out;
}

Pattern Engine::variation(const Pattern& p) {
    // a relative of the current melody (including the user's edits): a few aspects change, the identity stays
    Pattern out = p;
    out.seed = (uint32_t) dice(rng, 1, 0x7fffffff);
    out.melody = composer::vary(p, p.melody, out.seed);
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

std::vector<uint8_t> midiBytes(const std::vector<Note>& notes, int bpm, int channel, int lengthTicks) {
    struct Ev { int tick; bool on; int note, vel; };
    std::vector<Ev> ev;
    for (auto& n : notes) { ev.push_back({ n.start, true, n.pitch, n.velocity }); ev.push_back({ n.start + std::max(1, n.length), false, n.pitch, 0 }); }
    std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : (!a.on && b.on); });
    std::vector<uint8_t> tr;
    const uint32_t us = (uint32_t) (60000000 / std::max(1, bpm));
    vlq(tr, 0); tr.insert(tr.end(), { 0xff, 0x51, 0x03, (uint8_t) (us >> 16), (uint8_t) (us >> 8), (uint8_t) us });
    vlq(tr, 0); tr.insert(tr.end(), { 0xff, 0x58, 0x04, 4, 2, 24, 8 });   // 4/4
    int last = 0;
    const uint8_t ch = (uint8_t) (channel & 15);
    for (auto& e : ev) {
        vlq(tr, (uint32_t) std::max(0, e.tick - last)); last = e.tick;
        tr.push_back((uint8_t) ((e.on ? 0x90 : 0x80) | ch)); tr.push_back((uint8_t) std::clamp(e.note, 0, 127)); tr.push_back((uint8_t) std::clamp(e.vel, 0, 127));
    }
    vlq(tr, (uint32_t) std::max(0, lengthTicks - last)); tr.insert(tr.end(), { 0xff, 0x2f, 0 });   // end of track at the loop length
    std::vector<uint8_t> out { 'M', 'T', 'h', 'd' };
    be32(out, 6); be16(out, 0); be16(out, 1); be16(out, ppq);
    out.insert(out.end(), { 'M', 'T', 'r', 'k' }); be32(out, (uint32_t) tr.size());
    out.insert(out.end(), tr.begin(), tr.end());
    return out;
}

const char* styleName(Style x) {
    static const char* n[] { "PADRÃO", "FUNK BR", "POP", "TRAP", "PHONK", "DRILL", "BOSSA NOVA", "JAZZ", "R&B", "REGGAETON", "HOUSE", "LO-FI" };
    return n[(int) x];
}
const char* vibeName(Vibe x) { static const char* n[] { "CLEAN", "DREAMY", "DARK", "LO-FI TAPE", "SPACE" }; return n[(int) x]; }
const char* feelName(Feel x) { static const char* n[] { "STRAIGHT", "SWING", "TRIPLET" }; return n[(int) x]; }
}
