#include "MelodyEngine.h"
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

/* Melody engine: written the way a producer builds a hook.
   - A ONE-BAR RHYTHM (the call) is chosen first; rhythm is what people remember. The response reuses the
     call's first half (an echo) and answers with its own tail, usually landing on a longer note.
   - The phrase is CALL / RESPONSE / CALL / RESPONSE (2 bars: call + response; 8 bars: twice, the second time
     lifted for a climax). The call ends open (2nd, 5th, 7th: a question); the middle response rests on a chord
     tone; the final response resolves to the tonic.
   - The returning call keeps its rhythm and contour but moves to fit the new chord (a sequence), so the hook
     is recognisable over every chord.
   - Pitches: chord tones on the beats, the pentatonic in between (the notes that "sing"), stepwise motion with
     repeated notes, a leap is answered by a step back, and a contour (arch, rise, fall, wave) per call.
   Many candidates are written and scored (see score); the best one that has not come out recently wins. */
/* Motif engine, modelled on the reference loops Ed sent (funk "maladeza espacial", trap/pop songstarters):
   - the line lives in a tiny cluster of 3-4 neighbouring scale notes (often within 3-5 semitones)
   - a 1- or 2-bar motif repeats almost unchanged: the repetition is the hook
   - only the end of each 4-bar phrase changes: a turnaround (octave flick, drop below, or a held note)
   - dark colour: chromatic passing notes between whole steps, phrygian / harmonic minor scales
   - the last note lands on the tonic and rings. */
std::vector<Note> Engine::motifAttempt(const Pattern& pat) {
    const auto& s = pat.settings;
    const auto& st = profile(pat.style);
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.key, pat.scale);
    const std::vector<int> scalePcs(scale.begin(), scale.end());
    const bool counter = s.role == Role::counter, hook = s.role == Role::hook;
    const int center = (s.range == Range::low ? 58 : s.range == Range::high ? 74 : 66) - (counter ? 5 : 0);
    std::vector<int> pool;
    for (int m = center - 14; m <= center + 16; ++m) if (has(scalePcs, wrap12(m))) pool.push_back(m);
    const int lo = 0, hi = (int) pool.size() - 1;
    auto pc = [&](int i) { return wrap12(pool[(size_t) std::clamp(i, lo, hi)]); };
    auto chordTones = [&](int tick) { auto pcs = chordPitchClasses(pat.chordAt(tick)); if (pcs.size() > 3) pcs.resize(3); return pcs; };
    auto nearestTo = [&](const std::vector<int>& pcs, int from) {
        for (int d = 0; d <= 7; ++d) for (int sg : { 1, -1 }) { const int i = from + sg * d; if (i >= lo && i <= hi && has(pcs, pc(i))) return i; }
        return std::clamp(from, lo, hi);
    };

    // anchor: a chord tone of the first chord near the centre; the cluster is anchor-1 .. anchor+width
    int anchor = (int) (std::lower_bound(pool.begin(), pool.end(), center) - pool.begin());
    anchor = nearestTo(chordTones(0), anchor + dice(rng, -1, 1));
    const int width = st.span <= 5 ? 2 : st.span <= 7 ? 3 : 4;   // scale steps above the anchor
    const int cLo = std::max(lo, anchor - 1), cHi = std::min(hi, anchor + width);

    // rhythm: one or two bars of cells
    const auto& cellPool = s.feel == Feel::triplet ? tripletCells() : st.cells;
    auto thin = [&](std::vector<Cell> c) {
        const Density d = counter ? Density::low : s.density;
        if (d == Density::low && c.size() > 3) {
            std::vector<Cell> o;
            for (size_t i = 0; i < c.size(); ++i) if (i % 2 == 0 || i + 1 == c.size()) o.push_back(c[i]);
            for (size_t i = 0; i + 1 < o.size(); ++i) o[i].len = o[i + 1].on - o[i].on;
            return o;
        }
        if (d == Density::high) {
            std::vector<Cell> o;
            for (auto x : c) if (x.len >= 2 && chance(rng) < .45) { o.push_back({ x.on, x.len / 2 }); o.push_back({ x.on + x.len / 2, x.len / 2 }); } else o.push_back(x);
            return o;
        }
        return c;
    };
    const int motifBars = hook || pat.bars() < 2 ? 1 : (chance(rng) < .55 ? 2 : 1);
    std::vector<std::vector<Cell>> motifCells;
    motifCells.push_back(thin(pick(rng, cellPool)));
    if (motifBars == 2) motifCells.push_back(chance(rng) < .4 ? motifCells[0] : thin(pick(rng, cellPool)));

    // pitches: a small walk inside the cluster that keeps coming back to the anchor
    std::vector<std::vector<int>> motifIdx;
    const int shape = dice(rng, 0, 3);   // 0 neighbour above, 1 neighbour below, 2 climb and fall, 3 pendulum
    for (int mb = 0; mb < motifBars; ++mb) {
        std::vector<int> idx;
        int cur = mb == 0 ? anchor : motifIdx[0].back();
        const auto& cells = motifCells[(size_t) mb];
        for (size_t i = 0; i < cells.size(); ++i) {
            const double ph = cells[i].on / 16.0;
            int target;
            switch (shape) {
                case 0:  target = anchor + (i % 3 == 1 ? 1 : i % 3 == 2 ? 2 : 0); break;
                case 1:  target = anchor + (i % 2 ? -1 : (i % 4 == 2 ? 1 : 0)); break;
                case 2:  target = anchor + (int) std::round(width * std::sin(ph * 3.14159)); break;
                default: target = anchor + (i % 2 ? width : 0) - (i % 4 == 3 ? 1 : 0); break;
            }
            if (mb == 1) target += (i < cells.size() / 2 ? 1 : 0);                   // second bar answers a step higher, then returns
            if (i > 0 && std::fmod(cells[i].on, 4.0) > .01 && chance(rng) < st.rep) target = cur;   // repeated note
            if (chance(rng) < .15) target += chance(rng) < .5 ? 1 : -1;               // a little life
            cur = std::clamp(target, cLo, cHi);
            idx.push_back(cur);
        }
        motifIdx.push_back(idx);
    }

    const bool dark = pat.style == Style::funkBR || pat.style == Style::trap || pat.style == Style::phonk || pat.style == Style::drill
                   || pat.scale == ccp::ScaleType::phrygian || pat.scale == ccp::ScaleType::harmonicMinor;
    const int bars = pat.bars(), tonic = wrap12(pat.key);
    const int turnType = dice(rng, 0, 2);   // 0 octave flick, 1 drop below, 2 held note
    std::vector<Note> out;
    for (int b = 0; b < bars; ++b) {
        const int mb = b % motifBars;
        auto cells = motifCells[(size_t) mb];
        auto idx = motifIdx[(size_t) mb];
        const bool last = b == bars - 1;
        const bool phraseEnd = !last && (b % 4 == 3) && bars >= 4;
        // only fix strong-beat clashes with the chord (a semitone rub), keep the motif otherwise
        for (size_t i = 0; i < cells.size(); ++i) {
            const int tick = b * barTicks + ticks(cells[i].on);
            if (std::fmod(cells[i].on, 8.0) > .01) continue;
            const auto ct = chordTones(tick);
            bool clash = false;
            for (int c : ct) if (wrap12(pc(idx[i]) - c) == 1 || wrap12(c - pc(idx[i])) == 1) clash = !has(ct, pc(idx[i]));
            if (clash) idx[i] = nearestTo(ct, idx[i]);
        }
        if (phraseEnd || last) {
            // turnaround on the second half of the bar
            std::vector<Cell> keep; std::vector<int> keepIdx;
            for (size_t i = 0; i < cells.size(); ++i) if (cells[i].on < 8) { keep.push_back(cells[i]); keepIdx.push_back(idx[i]); }
            if (keep.empty()) { keep.push_back({ 0, 4 }); keepIdx.push_back(anchor); }
            cells = keep; idx = keepIdx;
            if (last) {
                const int t = nearestTo({ tonic }, idx.back());
                cells.push_back({ 8, 8 }); idx.push_back(t);
            } else if (turnType == 0) {
                cells.push_back({ 8, 2 }); idx.push_back(idx.back() + 7);              // octave flick (7 scale steps)
                cells.push_back({ 10, 2 }); idx.push_back(idx.back() - 1);
                cells.push_back({ 12, 4 }); idx.push_back(anchor);
            } else if (turnType == 1) {
                cells.push_back({ 8, 2 }); idx.push_back(anchor - 1);
                cells.push_back({ 10, 2 }); idx.push_back(anchor - 2);
                cells.push_back({ 12, 4 }); idx.push_back(anchor - 1);
            } else {
                cells.push_back({ 8, 8 }); idx.push_back(nearestTo(chordTones(b * barTicks + 8 * step), anchor + 1));
            }
            for (size_t i = 0; i + 1 < cells.size(); ++i) cells[i].len = std::min(cells[i].len, cells[i + 1].on - cells[i].on);
        }
        for (size_t i = 0; i < cells.size(); ++i) {
            const int start = b * barTicks + ticks(cells[i].on);
            const bool ring = i + 1 == cells.size() && (last || phraseEnd);
            const int len = std::max(step / 2, (int) (ticks(cells[i].len) * (ring ? 1.0 : st.stac)) - step / 10);
            const double o = cells[i].on;
            const int vel = (std::fmod(o, 4.0) < .01 ? 104 : std::fmod(o, 2.0) < .01 ? 92 : 84) + dice(rng, -6, 6);
            const int pitch = pool[(size_t) std::clamp(idx[i], lo, hi)];
            // chromatic passing note between two notes a whole step apart (the "dark" slide-like colour)
            if (dark && i + 1 < cells.size() && cells[i].len >= 2 && chance(rng) < .3) {
                const int nx = pool[(size_t) std::clamp(idx[i + 1], lo, hi)];
                if (std::abs(nx - pitch) == 2) {
                    out.push_back({ start, std::max(step / 2, len - step), pitch, vel });
                    out.push_back({ start + ticks(cells[i].len) - step, step - step / 8, (pitch + nx) / 2, vel - 14 });
                    continue;
                }
            }
            out.push_back({ start, len, pitch, vel });
        }
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

std::vector<Note> Engine::melodyAttempt(const Pattern& pat) {
    if (pat.style != Style::jazz && pat.style != Style::bossaNova) return motifAttempt(pat);
    const auto& s = pat.settings;
    const auto& st = profile(pat.style);
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.key, pat.scale);
    const std::vector<int> scalePcs(scale.begin(), scale.end());
    const bool minorish = wrap12(scale[2] - pat.key) == 3;
    std::vector<int> pent;   // pentatonic: minor drops the 2nd and 6th, major drops the 4th and 7th
    for (int i = 0; i < 7; ++i) if (minorish ? (i != 1 && i != 5) : (i != 3 && i != 6)) pent.push_back(scale[(size_t) i]);
    const bool counter = s.role == Role::counter, hook = s.role == Role::hook;

    const int center = (s.range == Range::low ? 60 : s.range == Range::high ? 74 : 67) - (counter ? 5 : 0);
    const int span = st.span - (counter ? 3 : 0);
    std::vector<int> pool;
    for (int m = center - span / 2 - 2; m <= center + span / 2 + 2; ++m) if (has(scalePcs, wrap12(m))) pool.push_back(m);
    const int lo = 0, hi = (int) pool.size() - 1;
    auto pc = [&](int i) { return wrap12(pool[(size_t) std::clamp(i, lo, hi)]); };
    auto nearestIn = [&](const std::vector<int>& pcs, int from, int dir) {   // dir 0 = either way
        for (int d = 0; d <= 7; ++d)
            for (int sgn : { dir >= 0 ? 1 : -1, dir >= 0 ? -1 : 1 }) {
                if (d == 0 && sgn < 0) continue;
                const int i = from + sgn * d;
                if (i >= lo && i <= hi && has(pcs, pc(i)) && (dir == 0 || d == 0 || sgn == dir || d > 2)) return i;
            }
        return std::clamp(from, lo, hi);
    };
    auto chordTones = [&](int tick) {
        auto pcs = chordPitchClasses(pat.chordAt(tick));
        if (!st.ext && pcs.size() > 3) pcs.resize(3);
        return pcs;
    };

    // ---- rhythm: call cell, response derived from it
    const auto& cellPool = s.feel == Feel::triplet ? tripletCells() : st.cells;
    auto thin = [&](std::vector<Cell> c) {
        const Density d = counter ? Density::low : s.density;
        if (d == Density::low && c.size() > 3) {
            std::vector<Cell> o;
            for (size_t i = 0; i < c.size(); ++i) if (i % 2 == 0 || i + 1 == c.size()) o.push_back(c[i]);
            for (size_t i = 0; i + 1 < o.size(); ++i) o[i].len = o[i + 1].on - o[i].on;
            return o;
        }
        if (d == Density::high) {
            std::vector<Cell> o;
            for (auto x : c) if (x.len >= 2 && chance(rng) < .5) { o.push_back({ x.on, x.len / 2 }); o.push_back({ x.on + x.len / 2, x.len / 2 }); } else o.push_back(x);
            return o;
        }
        return c;
    };
    const auto call = thin(pick(rng, cellPool));
    auto makeResponse = [&]() {
        std::vector<Cell> r;
        if (hook) { r = call; }
        else {
            const double cut = chance(rng) < .5 ? 8.0 : 6.0;
            for (auto c : call) if (c.on < cut) r.push_back(c);                       // echo of the call
            if (chance(rng) < .55) {                                                 // answer with another cell's tail
                const auto other = thin(pick(rng, cellPool));
                for (auto c : other) if (c.on >= cut) r.push_back(c);
            } else {                                                                  // or land and hold
                r.push_back({ cut + (chance(rng) < .5 ? 0 : 2), 16 - cut });
            }
        }
        std::sort(r.begin(), r.end(), [](const Cell& a, const Cell& b) { return a.on < b.on; });
        r.erase(std::unique(r.begin(), r.end(), [](const Cell& a, const Cell& b) { return std::abs(a.on - b.on) < .01; }), r.end());
        for (size_t i = 0; i + 1 < r.size(); ++i) r[i].len = std::min(r[i].len, r[i + 1].on - r[i].on);
        if (!r.empty()) r.back().len = std::max(r.back().len, std::min(6.0, 16.0 - r.back().on));   // land on a longer note
        return r;
    };
    const auto response = makeResponse(), response2 = makeResponse();

    // ---- phrase plan
    const int bars = pat.bars();
    struct Unit { bool isCall; int variant; };
    std::vector<Unit> plan;
    for (int b = 0; b < bars; ++b) plan.push_back({ b % 2 == 0, (b / 2) % 2 });

    enum Contour { arch, rise, fall, wave };
    const Contour contour = (Contour) dice(rng, 0, 3);
    auto direction = [&](double phase) {
        switch (contour) {
            case rise: return phase < .8 ? 1 : -1;
            case fall: return phase < .2 ? 1 : -1;
            case wave: return std::sin(phase * 6.28318 * 1.5) >= 0 ? 1 : -1;
            default:   return phase < .55 ? 1 : -1;
        }
    };

    auto writeLine = [&](const std::vector<Cell>& cells, int bar, int startIdx, const std::vector<int>* echo, int lift) {
        std::vector<int> idx;
        int cur = startIdx, lastMove = 0;
        for (size_t i = 0; i < cells.size(); ++i) {
            const double o = cells[i].on;
            const int tick = bar * barTicks + ticks(o);
            const bool beat = std::fmod(o, 4.0) < .01, down = std::fmod(o, 8.0) < .01;
            const int dir = direction(o / 16.0);
            const auto ct = chordTones(tick);
            int ni;
            if (echo && i < echo->size() && i < 3 && cells[i].on < 8) ni = (*echo)[i];                 // echo the call
            else if (i == 0) ni = nearestIn(ct, cur + lift, 0);
            else if (std::abs(lastMove) >= 3) ni = cur - (lastMove > 0 ? 1 : -1);                         // gap-fill
            else if (!beat && chance(rng) < st.rep) ni = cur;                                              // repeated note
            else if (down || (beat && chance(rng) < .7)) ni = nearestIn(ct, cur + dir, dir);               // chord tone on the beat
            else if (chance(rng) < st.arp) ni = nearestIn(ct, cur + 2 * dir, dir);                         // outline the chord
            else {
                const int leap = chance(rng) < .12 ? std::min(st.leap, dice(rng, 2, 4)) : 1;
                ni = cur + dir * leap;
                if (!has(pent, pc(ni)) && chance(rng) < st.pent) ni += dir;                                // stay on the singable notes
            }
            if (ni < lo) ni = lo + 1;
            if (ni > hi) ni = hi - 1;
            lastMove = ni - cur; cur = ni;
            idx.push_back(ni);
        }
        return idx;
    };

    std::vector<Note> out;
    std::vector<int> callIdx;
    int prevEnd = (int) pool.size() / 2;
    const int tonic = wrap12(pat.key);
    for (int b = 0; b < bars; ++b) {
        const auto& u = plan[(size_t) b];
        const bool last = b == bars - 1;
        const bool secondHalf = bars == 8 && b >= 4;
        const int lift = secondHalf && u.isCall ? 2 : 0;
        const auto& cells = u.isCall ? call : (u.variant ? response2 : response);
        std::vector<int> idx;
        if (u.isCall && !callIdx.empty()) {
            // the call returns: same rhythm and shape, moved to fit this chord (a sequence)
            const auto ct = chordTones(b * barTicks + ticks(cells[0].on));
            const int target = nearestIn(ct, callIdx[0] + lift, 0);
            const int shift = has(ct, pc(callIdx[0] + lift)) && chance(rng) < .5 ? lift : target - callIdx[0];
            for (size_t i = 0; i < cells.size(); ++i) {
                int ni = std::clamp(callIdx[std::min(i, callIdx.size() - 1)] + shift, lo, hi);
                const bool beat = std::fmod(cells[i].on, 4.0) < .01;
                const auto cti = chordTones(b * barTicks + ticks(cells[i].on));
                if (beat && !has(cti, pc(ni)) && chance(rng) < .8) ni = nearestIn(cti, ni, 0);
                idx.push_back(ni);
            }
        } else {
            idx = writeLine(cells, b, prevEnd, u.isCall ? nullptr : &callIdx, lift);
        }
        // phrase endings
        if (!idx.empty()) {
            int& e = idx.back();
            const int endTick = b * barTicks + ticks(cells.back().on);
            if (last) { e = nearestIn({ tonic }, e, 0); }                                              // resolve home
            else if (u.isCall) {                                                                        // a question: open ending
                std::vector<int> open { scale[1], scale[4], scale[6] };
                const auto ct = chordTones(endTick);
                std::vector<int> both;
                for (int x : open) if (has(ct, x)) both.push_back(x);
                e = nearestIn(both.empty() ? open : both, e, 0);
            } else {                                                                                    // middle answer: a chord tone
                const auto ct = chordTones(endTick);
                e = nearestIn(ct, e, 0);
            }
        }
        if (u.isCall && callIdx.empty()) callIdx = idx;
        prevEnd = idx.empty() ? prevEnd : idx.back();

        for (size_t i = 0; i < cells.size(); ++i) {
            const int start = b * barTicks + ticks(cells[i].on);
            const bool phraseEnd = i + 1 == cells.size() && !u.isCall;
            const double factor = phraseEnd ? 1.0 : st.stac;
            const int len = std::max(step / 2, (int) (ticks(cells[i].len) * factor) - (factor >= 1 ? step / 10 : 0));
            const double o = cells[i].on;
            const int vel = (std::fmod(o, 4.0) < .01 ? 104 : std::fmod(o, 2.0) < .01 ? 92 : 84) + dice(rng, -6, 6);
            const int pitch = pool[(size_t) std::clamp(idx[i], lo, hi)];
            if (i + 1 < cells.size() && st.appr > 0 && cells[i].len >= 2 && std::fmod(cells[i + 1].on, 4.0) < .01 && chance(rng) < st.appr) {
                // chromatic approach into the next beat note, on the last 16th
                const int target = pool[(size_t) std::clamp(idx[i + 1], lo, hi)];
                const int app = pat.style == Style::jazz && chance(rng) < .4 ? target + 1 : target - 1;
                out.push_back({ start, std::max(step / 2, len - step), pitch, vel });
                out.push_back({ start + ticks(cells[i].len) - step, step - step / 8, app, vel - 16 });
            } else {
                out.push_back({ start, len, pitch, vel });
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return out;
}

double Engine::score(const Pattern& pat, const std::vector<Note>& m) {
    if (m.size() < 3) return -100;
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.key, pat.scale);
    const int leadingTone = wrap12(pat.key + 11);
    const bool repeatsOk = pat.style == Style::funkBR || pat.style == Style::phonk || pat.style == Style::house || pat.style == Style::trap;
    double sc = 0;
    int strong = 0, strongHits = 0, steps = 0, leaps = 0, bigLeaps = 0, gapFills = 0, runs = 0, run = 1, sameDir = 0, longSameDir = 0;
    int lo = 127, hi = 0;
    std::vector<int> distinct;
    for (size_t i = 0; i < m.size(); ++i) {
        const auto& n = m[i];
        lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch);
        if (!has(distinct, n.pitch)) distinct.push_back(n.pitch);
        if (n.start % ppq == 0) {
            ++strong;
            if (has(chordPitchClasses(pat.chordAt(n.start)), wrap12(n.pitch))) ++strongHits;
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
                sc += wrap12(n.pitch) == wrap12(pat.key) ? 1.0 : -0.5;
        }
    }
    sc += strong ? 12.0 * strongHits / strong : 0;
    sc += 5.0 * steps / (double) m.size();
    sc += 1.5 * gapFills;
    sc -= 2.5 * leaps + 4.0 * bigLeaps + (repeatsOk ? 0.5 : 1.5) * runs + 1.5 * longSameDir;
    const int range = hi - lo;
    const bool motif = pat.style != Style::jazz && pat.style != Style::bossaNova;
    if (motif) {
        // the references sit in a small range (3-9 semitones without the flicks) and repeat themselves
        sc += range <= 9 ? 3 : range <= 14 ? 0 : -3;
        sc += distinct.size() >= 3 ? 1 : -3;
        std::vector<std::vector<int>> bars(16);
        for (auto& n : m) bars[(size_t) std::min(15, barOf(n.start))].push_back(n.pitch * 10000 + n.start % barTicks);
        int same = 0;
        for (int b = 1; b < 16; ++b) if (!bars[(size_t) b].empty() && (bars[(size_t) b] == bars[(size_t) b - 1] || (b >= 2 && bars[(size_t) b] == bars[(size_t) b - 2]))) ++same;
        sc += 1.5 * same;
    } else {
        sc += range >= 5 && range <= 15 ? 3 : -2;
        sc += distinct.size() >= 4 ? 2 : -3;
    }
    int climaxBars = 0, prevBar = -1;
    for (auto& n : m) if (n.pitch == hi && barOf(n.start) != prevBar) { ++climaxBars; prevBar = barOf(n.start); }
    if (climaxBars == 1) sc += 2;
    if (wrap12(m.back().pitch) == wrap12(pat.key)) sc += 4;
    return sc;
}

uint64_t Engine::signature(const std::vector<Note>& m) const {
    uint64_t h = 1469598103934665603ull;
    for (auto& n : m) { h ^= (uint64_t) (n.start * 131 + n.pitch); h *= 1099511628211ull; }
    return h;
}

std::vector<Note> Engine::melodyFor(const Pattern& pat) {
    std::vector<Note> best;
    double bestScore = -1e9;
    std::uniform_real_distribution<double> jitter(-1.5, 1.5);
    for (int attempt = 0; attempt < 40; ++attempt) {
        auto m = melodyAttempt(pat);
        if (std::find(recent.begin(), recent.end(), signature(m)) != recent.end()) continue;
        const double sc = score(pat, m) + jitter(rng);
        if (sc > bestScore) { bestScore = sc; best = std::move(m); }
    }
    if (best.empty()) best = melodyAttempt(pat);
    recent.push_back(signature(best));
    while (recent.size() > 64) recent.pop_front();

    // ---- decorations on the winning line
    const auto& st = profile(pat.style);
    const auto& s = pat.settings;
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.key, pat.scale);
    std::vector<int> pool;
    for (int m = 30; m <= 100; ++m) if (has({ scale.begin(), scale.end() }, wrap12(m))) pool.push_back(m);
    auto poolIndex = [&](int pitch) { auto f = std::lower_bound(pool.begin(), pool.end(), pitch); return (int) (f - pool.begin()); };
    auto main = best;

    // anticipation: a downbeat note arrives one 16th early
    if (s.feel != Feel::triplet)
        for (size_t i = 1; i < main.size(); ++i) {
            auto& n = main[i];
            if (n.start % barTicks == 0 && main[i - 1].start + main[i - 1].length <= n.start - step && chance(rng) < st.anticipate)
                { n.start -= step; n.length += step; }
        }
    // legato overlap: ring into the next note when the interval is consonant
    for (size_t i = 0; i + 1 < main.size(); ++i) {
        auto& n = main[i]; const auto& nx = main[i + 1];
        if (nx.start - n.start >= step && consonant(n.pitch, nx.pitch) && n.pitch != nx.pitch && chance(rng) < st.overlap)
            n.length = std::max(n.length, nx.start - n.start + step * (1 + dice(rng, 0, 1)));
    }
    // second voice
    const double voice = st.voice * (s.role == Role::counter ? .3 : 1.0);
    std::vector<Note> voices;
    if (st.texture == Texture::pedal) {
        for (int b = 0; b < pat.bars(); ++b) {
            if (chance(rng) > voice) continue;
            int top = 0;
            for (auto& n : main) if (barOf(n.start) == b) top = std::max(top, n.pitch);
            if (!top) continue;
            const auto pcs = chordPitchClasses(pat.chordAt(b * barTicks));
            for (int p = top + 1; p <= top + 9; ++p)
                if (has(pcs, wrap12(p)) && has({ scale.begin(), scale.end() }, wrap12(p))) { voices.push_back({ b * barTicks, barTicks - step, p, 64 }); break; }
        }
    } else if (st.texture != Texture::none) {
        for (auto& n : main) {
            if (n.length < 2 * step || chance(rng) > voice) continue;
            const int pi = poolIndex(n.pitch);
            int v = -1;
            switch (st.texture) {
                case Texture::thirds: if (pi >= 2) v = pool[(size_t) (pi - 2)]; break;
                case Texture::sixths: if (pi >= 5) v = pool[(size_t) (pi - 5)]; break;
                case Texture::octaves: v = n.pitch - 12; break;
                case Texture::shell: {
                    const auto pcs = chordPitchClasses(pat.chordAt(n.start));
                    for (int p = n.pitch - 3; p >= n.pitch - 9; --p) if (has(pcs, wrap12(p)) && consonant(p, n.pitch)) { v = p; break; }
                    break;
                }
                default: break;
            }
            if (v > 52) voices.push_back({ n.start, n.length, v, std::max(40, n.velocity - 24) });
        }
    }
    // grace notes into a strong note
    std::vector<Note> graces;
    for (size_t i = 1; i < main.size(); ++i) {
        const auto& n = main[i];
        if (n.start % ppq != 0 || chance(rng) > st.grace) continue;
        if (main[i - 1].start + main[i - 1].length > n.start - step / 2) continue;
        const bool chromatic = (pat.style == Style::jazz || pat.style == Style::rnb) && chance(rng) < .5;
        const int g = chromatic ? n.pitch - 1 : pool[(size_t) std::min((int) pool.size() - 1, poolIndex(n.pitch) + 1)];
        graces.push_back({ n.start - step / 2, step / 2 - 10, g, std::max(40, n.velocity - 22) });
    }

    std::vector<Note> all = main;
    all.insert(all.end(), voices.begin(), voices.end());
    all.insert(all.end(), graces.begin(), graces.end());
    applySwing(all, st, s.feel);
    for (auto& n : all) n.length = std::min(n.length, pat.lengthTicks() - n.start);
    all.erase(std::remove_if(all.begin(), all.end(), [](const Note& n) { return n.length <= 0 || n.start < 0; }), all.end());
    std::sort(all.begin(), all.end(), [](const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch > b.pitch; });
    return all;
}

Pattern Engine::generate(const Settings& s) {
    Pattern out;
    out.settings = s;
    out.style = resolveStyle(s.style);
    const auto& prof = profile(out.style);
    out.key = s.key >= 0 && s.key < 12 ? s.key : dice(rng, 0, 11);
    out.scale = s.scale >= 0 && s.scale < (int) ccp::ScaleType::count ? (ccp::ScaleType) s.scale : pick(rng, prof.scales);
    out.progression = harmony.generate(harmonySettings(out.style, out.key, out.scale), nullptr, {}, s.bars);
    shapeHarmony(out);
    out.bass = bassFor(out);
    out.melody = melodyFor(out);
    return out;
}

Pattern Engine::newMelody(const Pattern& p, const Settings& s) {
    Pattern out = p;
    out.settings.role = s.role; out.settings.density = s.density; out.settings.range = s.range;
    out.settings.feel = s.feel; out.settings.vibe = s.vibe;
    out.melody = melodyFor(out);
    if (s.feel != p.settings.feel) out.bass = bassFor(out);
    return out;
}

Pattern Engine::variation(const Pattern& p) {
    Pattern out = p;
    const auto scale = ccp::HarmonyEngine::scaleNotes(p.key, p.scale);
    auto inScale = [&](int m) { return has({ scale.begin(), scale.end() }, wrap12(m)); };
    bool changed = false;
    for (int tries = 0; tries < 6 && !changed; ++tries)
        for (auto& n : out.melody)
            if (n.start % ppq != 0 && chance(rng) < .4) {
                const int dir = chance(rng) < .5 ? 1 : -1;
                int m = n.pitch + dir;
                while (!inScale(m)) m += dir;
                if (std::abs(m - n.pitch) <= 3) { n.pitch = m; changed = true; }
            }
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
    static const char* n[] { "PADRÃO", "FUNK BR", "POP", "TRAP", "PHONK", "DRILL", "BOSSA NOVA", "JAZZ", "R&B", "REGGAETON", "HOUSE", "LO-FI" };
    return n[(int) x];
}
const char* vibeName(Vibe x) { static const char* n[] { "CLEAN", "DREAMY", "DARK", "LO-FI TAPE", "SPACE" }; return n[(int) x]; }
const char* feelName(Feel x) { static const char* n[] { "STRAIGHT", "SWING", "TRIPLET" }; return n[(int) x]; }
}
