#include "MelodyEngine.h"
#include "Composer.h"
#include "Genres.h"
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
        { Style::funkBR, { ccp::Style::funkBrasileiro, .45, .8, 3, 5, .9, .0, .15, false, false, .1, .05, 0, Texture::octaves, .15, {}, {},
            { {{0,3,root},{3,3,root},{6,2,octave},{10,2,root},{12,4,root}}, {{0,6,root},{6,2,octave},{8,6,root},{14,2,approach}} } } },
        { Style::pop, { ccp::Style::pop, .3, .92, 3, 7, .8, .04, .15, false, false, .3, .06, .12, Texture::thirds, .12, {}, {},
            { {{0,8,root},{8,6,root},{14,2,approach}}, {{0,6,root},{6,2,fifth},{8,8,root}}, {{0,4,root},{4,4,root},{8,4,root},{12,4,octave}} } } },
        { Style::trap, { ccp::Style::trap, .35, .9, 3, 6, .85, .06, .12, false, false, .1, .05, .12, Texture::pedal, .2, {}, {},
            { {{0,10,root},{10,6,root}}, {{0,6,root},{6,2,root},{8,8,root}}, {{0,3,root},{3,5,root},{11,5,octave}} } } },
        { Style::rnb, { ccp::Style::rnb, .12, 1.0, 4, 9, .7, .25, .2, true, false, .25, .3, .3, Texture::sixths, .2, {}, {},
            { {{0,10,root},{10,2,fifth},{12,4,approach}}, {{0,6,root},{7,3,root},{12,4,octave}} } } },
        { Style::reggaeton, { ccp::Style::latin, .3, .8, 3, 7, .85, .05, .2, false, false, .2, .1, .05, Texture::thirds, .2, {}, {},
            { {{0,3,root},{3,3,root},{6,2,octave},{8,3,root},{11,3,root},{14,2,octave}} } } },
        { Style::house, { ccp::Style::house, .45, .75, 3, 7, .85, 0, .2, true, false, .1, 0, .05, Texture::octaves, .25, {}, {},
            { {{2,2,root},{6,2,root},{10,2,root},{14,2,octave}}, {{0,2,root},{3,3,root},{8,2,root},{11,5,root}} } } },
        { Style::afrobeats, { ccp::Style::pop, .3, .8, 3, 7, .8, .1, .2, false, false, .2, .1, .1, Texture::thirds, .2, {}, {},
            { {{0,3,root},{3,3,fifth},{7,2,root},{10,4,octave},{14,2,approach}}, {{0,6,root},{6,1,root},{7,3,fifth},{12,4,root}} } } },
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

constexpr int H = 99;   // harmonic rhythm: hold the previous chord

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

// Diminished and augmented triads (the leading-tone chords of harmonic minor, dorian's vi) are avoided in these
// genres: such a degree is borrowed from the plain major / natural minor scale instead.
bool unstable(const ccp::Chord& c) {
    std::vector<int> iv; for (int i = 0; i < c.noteCount; ++i) iv.push_back(wrap12(c.notes[(size_t) i] - c.root));
    return (has(iv, 3) && has(iv, 6) && !has(iv, 7)) || (has(iv, 4) && has(iv, 8) && !has(iv, 7));
}
ccp::Chord stableChord(const ccp::GenerationSettings& gs, int degree, ccp::ChordKind kind, const ccp::Chord* prev, int pos) {
    auto c = ccp::HarmonyEngine::buildChord(gs, degree, kind, prev, pos);
    if (!unstable(c)) return c;
    auto plain = gs;
    const auto sn = ccp::HarmonyEngine::scaleNotes(gs.key, gs.scale);
    plain.scale = wrap12(sn[2] - sn[0]) == 3 ? ccp::ScaleType::naturalMinor : ccp::ScaleType::major;
    c = ccp::HarmonyEngine::buildChord(plain, degree, kind, prev, pos);
    if (!unstable(c)) {   // stored as a chromatic chord so voice leading / reloading keep the borrowed quality
        std::vector<int> iv; for (int i = 0; i < c.noteCount; ++i) iv.push_back(wrap12(c.notes[(size_t) i] - c.root));
        const bool minor = has(iv, 3), sev = has(iv, 10), maj7 = has(iv, 11), nine = has(iv, 2);
        const int q = nine ? (minor ? 14 : maj7 ? 13 : 12) : sev ? (minor ? 4 : 2) : maj7 ? 3 : minor ? 1 : 0;
        return ccp::HarmonyEngine::buildChromaticChord(gs, c.root, q, prev, pos);
    }
    return ccp::HarmonyEngine::buildChord(gs, (degree + 5) % 7, kind, prev, pos);   // last resort: the relative a third below
}

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

// Harmony first: the genre's progression (by scale degree, with its chord colours), voice-led chord to chord.
void Engine::buildProgression(Pattern& p) {
    const auto& g = genre::get(p.style);
    const auto sn = ccp::HarmonyEngine::scaleNotes(p.key, p.scale);
    const bool minorish = wrap12(sn[2] - sn[0]) == 3;
    const auto& pool = minorish ? g.minorProgs : g.majorProgs;
    const auto gs = harmonySettings(p.style, p.key, p.scale);
    const int n = std::clamp(p.settings.bars, 1, maxBars);
    const auto& first = pick(rng, pool);
    const auto& second = chance(rng) < .5 ? first : pick(rng, pool);
    p.progression = {};
    p.progression.settings = gs;
    p.progression.chordCount = n;
    for (int b = 0; b < n; ++b) {
        const auto& pr = b < 4 ? first : second;
        const ccp::Chord* prev = b ? &p.progression.chords[(size_t) (b - 1)] : nullptr;
        p.progression.chords[(size_t) b] = stableChord(gs, pr.degree[(size_t) (b % 4)], pr.kind[(size_t) (b % 4)], prev, b);
    }
    // 8 bars ending the second cycle on the tonic when the cycle does not already resolve
    if (n == 8 && p.progression.chords[7].degree != 0 && p.progression.chords[6].degree != 0 && chance(rng) < .5)
        p.progression.chords[7] = stableChord(gs, 4, ccp::ChordKind::triad, &p.progression.chords[6], 7);
    shapeHarmony(p);
}

void Engine::shapeHarmony(Pattern& p) {
    const auto& g = genre::get(p.style);
    const auto& tpl = pick(rng, g.harmonicRhythm);
    const int n = p.bars();
    p.chordShift = {};
    for (int b = 1; b < n; ++b) {
        int v = tpl[(size_t) (b % 4)];
        if (v == H) {
            // a held chord keeps sounding; it only holds if it does not lose the cycle's turnaround
            if (b % 4 != 3) p.progression.chords[(size_t) b] = p.progression.chords[(size_t) (b - 1)];
            v = 0;
        }
        p.chordShift[(size_t) b] = v;
    }
}

// Melody informs harmony: a bar whose strong melody notes clash with its chord may swap the chord for a
// relative with the same function (a third away: I-vi-iii, IV-ii, V-vii), if that fits the melody better.
void Engine::fitChords(Pattern& p) {
    const auto gs = p.progression.settings;
    const auto line = topLine(p.melody);
    auto fit = [&](const ccp::Chord& c, int bar) {
        const auto pcs = chordPitchClasses(c);
        double f = 0, total = 0;
        for (const auto& n : line) {
            if (p.chordIndexAt(n.start) != bar) continue;
            const int inBar = n.start % barTicks;
            const double w = (inBar % ppq == 0 ? 2.0 : 0.6) * std::min(4.0, n.length / (double) step);
            total += w;
            if (has(pcs, wrap12(n.pitch))) f += w;
        }
        return total > 0 ? f / total : 1.0;
    };
    for (int b = 0; b < p.bars(); ++b) {
        auto& c = p.progression.chords[(size_t) b];
        if (c.quality >= 0) continue;
        if (b > 0 && p.progression.chords[(size_t) (b - 1)].degree == c.degree && p.progression.chords[(size_t) (b - 1)].kind == c.kind) continue;
        const double now = fit(c, b);
        if (now >= .6) continue;
        const ccp::Chord* prev = b ? &p.progression.chords[(size_t) (b - 1)] : nullptr;
        ccp::Chord best = c; double bestFit = now + .15;   // a swap must be clearly better
        for (int d : { 2, 5 }) {
            const int deg = (c.degree + d) % 7;
            if (b == 0 && deg != 0 && c.degree == 0) continue;   // the loop still starts at home
            auto alt = ccp::HarmonyEngine::buildChord(gs, deg, c.kind, prev, b);
            if (unstable(alt)) continue;   // no diminished / augmented swaps
            const double f = fit(alt, b);
            if (f > bestFit) { best = alt; bestFit = f; }
        }
        c = best;
    }
    for (int b = 1; b < p.bars(); ++b)   // voice leading again after any swap
        p.progression.chords[(size_t) b] = ccp::HarmonyEngine::rebuild(gs, p.progression.chords[(size_t) b], &p.progression.chords[(size_t) (b - 1)], b);
}

// The chords, voiced around C4 (under the melody), in the genre's comping rhythm; changes follow the harmonic rhythm.
std::vector<Note> Engine::compFor(const Pattern& p) {
    const auto& g = genre::get(p.style);
    const auto& rhythm = pick(rng, g.comp);
    int melLo = 127; for (const auto& n : p.melody) melLo = std::min(melLo, n.pitch);
    const int top = std::clamp(melLo - 1, 62, 71);
    std::vector<Note> out;
    for (int b = 0; b < p.bars(); ++b) {
        std::vector<int> starts;
        for (const auto& h : rhythm) starts.push_back(h.on);
        const int shift = b > 0 ? p.chordShift[(size_t) b] : 0;
        for (size_t i = 0; i < rhythm.size(); ++i) {
            int on = b * barTicks + rhythm[i].on * step, len = rhythm[i].len * step;
            if (i == 0 && shift < 0) { on += shift * step; len -= shift * step; }   // the chord arrives early: the hit moves with it
            if (on < 0) continue;
            const int end = std::min(on + len, p.lengthTicks());
            // split a long hit where the chord changes
            int cut = end;
            for (int t = on + step; t < end; t += step) if (p.chordIndexAt(t) != p.chordIndexAt(on)) { cut = t; break; }
            for (int seg = on, segEnd = cut; seg < end; seg = segEnd, segEnd = end) {
                const auto& c = p.chordAt(seg);
                std::vector<int> v(c.notes.begin(), c.notes.begin() + c.noteCount);
                std::sort(v.begin(), v.end());
                while (!v.empty() && v.back() > top) { const int x = v.back() - 12; v.pop_back(); v.insert(v.begin(), x); std::sort(v.begin(), v.end()); }
                while (!v.empty() && v.front() < 48) { const int x = v.front() + 12; v.erase(v.begin()); v.push_back(x); std::sort(v.begin(), v.end()); }
                v.erase(std::unique(v.begin(), v.end()), v.end());
                const int vel = g.compVelocity + (seg % barTicks == 0 ? 6 : 0);
                for (int m : v) out.push_back({ seg, std::max(step / 2, segEnd - seg - step / 8), m, vel });
                if (segEnd >= end) break;
            }
        }
    }
    applySwing(out, profile(p.style), p.settings.feel);
    return out;
}

// The melody part may sound more than one note: the genre's intervals under chosen notes (dyads, doublings) and
// notes left ringing under the next one. Only consonant, in-scale, in-chord additions; never a semitone rub.
std::vector<Note> Engine::layered(const Pattern& p, std::vector<Note> line, uint32_t seed) const {
    const auto& g = genre::get(p.style);
    std::mt19937 r(seed ^ 0x2545f491u);
    const double roleF = p.settings.role == Role::counter ? .3 : p.settings.role == Role::hook ? .8 : 1.0;
    const double densF = p.settings.density == Density::low ? .7 : p.settings.density == Density::high ? 1.2 : 1.0;
    const auto sn = ccp::HarmonyEngine::scaleNotes(p.key, p.scale);
    auto inScale = [&](int m) { for (int x : sn) if (wrap12(x) == wrap12(m)) return true; return false; };
    std::sort(line.begin(), line.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    std::vector<Note> out = line;
    auto sounding = [&](int t0, int t1) { std::vector<int> v; for (auto& n : out) if (n.start < t1 && n.start + n.length > t0) v.push_back(n.pitch); return v; };
    auto clashes = [&](int m, int t0, int t1) { for (int o : sounding(t0, t1)) { const int d = std::abs(o - m) % 12; if (o != m && (d == 1 || d == 2 || d == 6 || d == 10 || d == 11)) return true; } return false; };
    for (size_t i = 0; i < line.size(); ++i) {
        const auto& n = line[i];
        // held note: the previous one rings until this one ends, if they agree
        if (i > 0 && chance(r) < g.layer.holdUnder * roleF) {
            const auto& prevN = line[i - 1];
            const int d = std::abs(n.pitch - prevN.pitch) % 12;
            const bool cons = d == 3 || d == 4 || d == 5 || d == 7 || d == 8 || d == 9;
            if (cons && prevN.pitch < n.pitch && has(chordPitchClasses(p.chordAt(n.start)), wrap12(prevN.pitch)) && (int) sounding(n.start, n.start + n.length).size() < g.layer.maxVoices)
                for (auto& o : out) if (o.start == prevN.start && o.pitch == prevN.pitch) { o.length = std::min(n.start + n.length, p.lengthTicks()) - o.start; break; }
        }
        if (n.length < 2 * step || g.layer.below.empty()) continue;
        if (chance(r) > g.layer.chance * roleF * densF * (n.start % ppq == 0 ? 1.3 : 0.6)) continue;
        if ((int) sounding(n.start, n.start + n.length).size() >= g.layer.maxVoices) continue;
        const auto chord = chordPitchClasses(p.chordAt(n.start));
        std::vector<int> opts;
        for (int iv : g.layer.below) {
            const int m = n.pitch - iv;
            if (m < 45 || !inScale(m)) continue;
            if (iv != 12 && !has(chord, wrap12(m))) continue;
            if (clashes(m, n.start, n.start + n.length)) continue;
            opts.push_back(m);
        }
        if (opts.empty()) continue;
        const int m = opts[(size_t) std::uniform_int_distribution<int>(0, (int) opts.size() - 1)(r)];
        out.push_back({ n.start, n.length, m, std::max(40, n.velocity - 18) });
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch > b.pitch; });
    return out;
}

std::vector<Note> Engine::topLine(const std::vector<Note>& m) {
    std::vector<Note> v = m;
    std::sort(v.begin(), v.end(), [](const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch > b.pitch; });
    v.erase(std::unique(v.begin(), v.end(), [](const Note& a, const Note& b) { return a.start == b.start; }), v.end());
    // a note that was only extended to ring under the next keeps its own share
    for (size_t i = 0; i + 1 < v.size(); ++i) v[i].length = std::min(v[i].length, std::max(step / 3, v[i + 1].start - v[i].start));
    return v;
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
        m = layered(pat, composer::compose(pat, pat.seed), pat.seed);
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
    out.key = s.key >= 0 && s.key < 12 ? s.key : dice(rng, 0, 11);
    out.scale = s.scale >= 0 && s.scale < (int) ccp::ScaleType::count ? (ccp::ScaleType) s.scale : pick(rng, genre::get(out.style).scales);
    buildProgression(out);          // harmony first
    out.melody = melodyFor(out);    // the melody is composed over it
    fitChords(out);                 // then the chords listen back to the melody
    out.bass = bassFor(out);
    out.chords = compFor(out);
    return out;
}

Pattern Engine::newMelody(const Pattern& p, const Settings& s) {
    // same context (key, scale, chords, bass), a different musical idea from a new seed
    Pattern out = p;
    out.settings.role = s.role; out.settings.density = s.density; out.settings.range = s.range;
    out.settings.feel = s.feel; out.settings.vibe = s.vibe;
    out.melody = melodyFor(out);
    if (s.feel != p.settings.feel) { out.bass = bassFor(out); out.chords = compFor(out); }
    return out;
}

Pattern Engine::variation(const Pattern& p) {
    // a relative of the current melody (including the user's edits): a few aspects change, the identity stays
    Pattern out = p;
    out.seed = (uint32_t) dice(rng, 1, 0x7fffffff);
    out.melody = layered(out, composer::vary(p, topLine(p.melody), out.seed), out.seed);
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
    static const char* n[] { "PADRÃO", "FUNK BR", "POP", "TRAP", "R&B", "REGGAETON", "HOUSE", "AFROBEATS" };
    return n[(int) x];
}
const char* vibeName(Vibe x) { static const char* n[] { "CLEAN", "DREAMY", "DARK", "LO-FI TAPE", "SPACE" }; return n[(int) x]; }
const char* feelName(Feel x) { static const char* n[] { "STRAIGHT", "SWING", "TRIPLET" }; return n[(int) x]; }
}
