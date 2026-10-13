#include "MelodyEngine.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace mc {
namespace {
struct Cell { int on, len; };                 // in 16ths
enum BassKind { root, octave, fifth, approach };
struct Hit { int on, len, kind; };

/* Style profiles. The melody rules come from how hooks are written in each genre:
   rep    chance of repeating the previous pitch on a weak beat
   leap   largest jump in scale steps
   stac   note-length factor (short = punchy, 1 = legato)
   span   melodic range in scale steps
   appr   chance of a chromatic approach note into the next chord tone
   arp    chance of outlining the chord instead of stepping
   ext    sevenths/ninths count as chord tones on strong beats
   glide  808 / lead portamento
   The sub templates follow Chord Code's bassline pulses per style. */
struct Profile {
    double rep, stac; int leap, span; double appr, arp; bool ext, glide;
    std::vector<std::vector<Cell>> cells;
    std::vector<std::vector<Hit>> bass;
};

const Profile& profile(Style s) {
    static const std::array<Profile, (size_t) Style::count> p {{
        // Funk BR: short repeated notes on the tamborzão grid
        { .4, .75, 3, 7, .05, .15, false, false,
          { {{0,2},{3,1},{6,2},{10,2},{12,3}}, {{0,1},{3,2},{6,1},{8,2},{11,1},{14,2}},
            {{0,3},{3,3},{6,2},{10,1},{12,2}}, {{0,1},{2,1},{3,2},{7,1},{10,2},{13,1}} },
          { {{0,2,root},{3,2,root},{6,2,octave},{10,2,root},{12,3,root}},
            {{0,3,root},{3,3,root},{7,1,root},{10,2,octave},{14,2,approach}},
            {{0,2,root},{3,3,root},{8,2,root},{11,2,octave},{14,2,root}} } },
        // Pop: singable, longer phrase endings
        { .15, .95, 4, 9, .08, .15, false, false,
          { {{0,4},{4,2},{6,2},{8,6}}, {{0,2},{2,2},{4,4},{10,2},{12,4}}, {{2,2},{4,2},{6,4},{12,4}},
            {{0,2},{2,2},{4,2},{6,2},{8,8}} },
          { {{0,6,root},{6,2,root},{8,6,root},{14,2,approach}}, {{0,8,root},{8,4,root},{12,4,fifth}},
            {{0,4,root},{4,4,root},{8,4,root},{12,4,octave}} } },
        // Trap: rolls, gaps, 808 slides
        { .35, .9, 5, 8, .1, .1, false, true,
          { {{0,1},{1,1},{2,2},{6,2},{8,1},{9,1},{10,4}}, {{0,3},{4,1},{5,1},{6,2},{12,3}},
            {{0,2},{3,3},{8,1},{9,1},{10,1},{12,4}} },
          { {{0,7,root},{7,3,root},{10,6,octave}}, {{0,4,root},{6,2,root},{8,8,root}},
            {{0,3,root},{3,4,root},{11,5,octave}} } },
        // Piseiro: arpeggios and thirds over a baião bass
        { .15, .8, 4, 10, .05, .45, false, false,
          { {{0,2},{2,2},{4,2},{6,2},{8,2},{10,2},{12,4}}, {{0,1},{2,1},{4,2},{8,1},{10,1},{12,4}},
            {{0,2},{2,1},{3,1},{4,2},{6,2},{8,4},{12,2},{14,2}} },
          { {{0,3,root},{3,1,fifth},{4,4,fifth},{8,3,root},{11,1,fifth},{12,4,fifth}},
            {{0,3,root},{3,5,root},{8,3,octave},{11,5,root}} } },
        // R&B: legato, chromatic approach notes, extended chord tones
        { .1, 1.0, 5, 10, .3, .2, true, true,
          { {{0,3},{3,1},{4,4},{10,2},{12,4}}, {{1,2},{3,3},{8,3},{12,4}}, {{0,1},{1,1},{2,4},{7,1},{8,6}} },
          { {{0,10,root},{10,2,fifth},{12,4,approach}}, {{0,6,root},{7,3,root},{12,4,octave}} } },
        // House: off-beat stabs
        { .45, .75, 3, 6, 0, .2, true, false,
          { {{2,2},{6,2},{10,2},{14,2}}, {{0,2},{2,2},{6,4},{10,2},{12,2}}, {{2,1},{4,2},{6,1},{10,1},{12,2},{14,1}} },
          { {{2,2,root},{6,2,root},{10,2,root},{14,2,octave}}, {{0,1,root},{2,2,root},{6,2,octave},{10,2,root},{14,2,root}} } },
        // Lo-fi: lazy, sparse, jazzy
        { .1, 1.0, 4, 8, .2, .25, true, false,
          { {{0,6},{8,2},{10,6}}, {{2,4},{8,4},{13,3}}, {{0,3},{4,2},{6,6}} },
          { {{0,12,root},{12,4,approach}}, {{0,7,root},{8,8,fifth}} } },
        // Phonk: cowbell-style repeated figures
        { .4, .7, 4, 7, .05, .2, false, true,
          { {{0,2},{2,2},{4,1},{6,2},{8,2},{10,2},{12,4}}, {{0,3},{3,3},{6,2},{8,3},{11,3},{14,2}} },
          { {{0,4,root},{6,2,root},{8,4,root},{14,2,octave}}, {{0,3,root},{3,3,root},{6,10,root}} } },
    }};
    return p[(size_t) s];
}

double chance(std::mt19937& r) { return std::uniform_real_distribution<double>(0, 1)(r); }
template <typename T> const T& pick(std::mt19937& r, const std::vector<T>& v) {
    return v[std::uniform_int_distribution<size_t>(0, v.size() - 1)(r)];
}
int wrap12(int n) { n %= 12; return n < 0 ? n + 12 : n; }
int lean(Vibe v) {
    switch (v) { case Vibe::dark: case Vibe::sad: return -1; case Vibe::happy: case Vibe::epic: return 1; default: return 0; }
}
}

ccp::GenerationSettings harmonySettings(const Settings& s) {
    static constexpr ccp::Style styles[] { ccp::Style::funkBrasileiro, ccp::Style::pop, ccp::Style::trap, ccp::Style::latin,
                                           ccp::Style::rnb, ccp::Style::house, ccp::Style::loFi, ccp::Style::phonk };
    static constexpr ccp::Mood moods[] { ccp::Mood::dark, ccp::Mood::fun, ccp::Mood::melancholic, ccp::Mood::mysterious,
                                         ccp::Mood::epic, ccp::Mood::atmospheric, ccp::Mood::romantic };
    ccp::GenerationSettings g;
    g.key = s.key; g.scale = s.scale; g.style = styles[(int) s.style]; g.mood = moods[(int) s.vibe];
    g.complexity = ccp::Complexity::automatic;
    return g;
}

Engine::Engine(uint32_t seed) : rng(seed), harmony(seed ^ 0x9e3779b9u) {}

std::vector<int> Engine::chordPitchClasses(const ccp::Chord& c) {
    std::vector<int> pcs;
    for (int i = 0; i < c.noteCount; ++i) {
        const int pc = wrap12(c.notes[(size_t) i]);
        if (std::find(pcs.begin(), pcs.end(), pc) == pcs.end()) pcs.push_back(pc);
    }
    return pcs;
}

int Engine::subPitch(const ccp::Chord& c) {
    int n = 24 + c.root;        // C1..B1
    if (n < 28) n += 12;        // keep it between E1 and D#2
    return n;
}

std::vector<Note> Engine::bassFor(const Settings& s, const ccp::Progression& p) {
    const auto& prof = profile(s.style);
    const auto& tpl = pick(rng, prof.bass);
    std::vector<Note> out;
    for (int b = 0; b < p.chordCount; ++b) {
        const auto& chord = p.chords[(size_t) b];
        const int sub = subPitch(chord);
        const int next = subPitch(p.chords[(size_t) ((b + 1) % p.chordCount)]);
        int prev = -1;
        for (size_t i = 0; i < tpl.size(); ++i) {
            const auto& h = tpl[i];
            if (s.density == Density::low && i > 0 && h.kind != root && i % 2 == 1) continue;
            int pitch = sub;
            if (h.kind == octave) pitch = sub + 12;
            else if (h.kind == fifth) pitch = sub + 7 > 45 ? sub - 5 : sub + 7;
            else if (h.kind == approach) pitch = next == sub ? sub + 12 : next + (next > sub ? -1 : 1);
            Note n;
            n.start = (b * stepsPerBar + h.on) * step;
            n.length = std::max(step / 2, h.len * step - step / 4);
            n.pitch = pitch;
            n.velocity = h.on == 0 ? 112 : 96;
            n.glide = prof.glide && prev >= 0 && prev != pitch;
            out.push_back(n);
            prev = pitch;
        }
    }
    return out;
}

/* Phrase engine (period form A, B, A', cadence):
   bar 1 states a motif; bar 2 answers with a contrasting rhythm; bar 3 restates the motif moved to its chord
   (a sequence); bar 4 cadences and lands on the tonic. Inside a bar: chord tones on strong beats, passing and
   neighbour tones on weak beats, a leap is followed by a step back the other way (gap-fill), and the line
   arches up then settles. The engine writes several candidates and keeps the best-scoring one. */
std::vector<Note> Engine::melodyAttempt(const Settings& s, const ccp::Progression& p, const std::vector<Note>& keep,
                                        const std::array<bool, maxBars>& locked) {
    const auto& st = profile(s.style);
    const auto scale = ccp::HarmonyEngine::scaleNotes(s.key, s.scale);
    const int lv = lean(s.vibe);
    const bool counter = s.role == Role::counter;
    const int span = counter ? std::max(5, st.span - 2) : st.span;

    int base = s.range == Range::low ? 55 : s.range == Range::high ? 69 : 62;
    if (counter) base -= 5;
    std::vector<int> pool;
    for (int m = base - 7; m <= base + 17; ++m)
        if (std::find(scale.begin(), scale.end(), wrap12(m)) != scale.end()) pool.push_back(m);
    const int mid = (int) pool.size() / 2;
    const int lo = std::max(0, mid - span / 2), hi = std::min((int) pool.size() - 1, lo + span);
    auto clamp = [&](int i) { return std::clamp(i, lo, hi); };

    // nearest pool index whose pitch class is in pcs; spread>1 picks randomly among the closest few
    auto chordIdx = [&](const std::vector<int>& pcs, int from, int spread) {
        std::vector<int> c;
        for (int i = lo; i <= hi; ++i) if (std::find(pcs.begin(), pcs.end(), wrap12(pool[(size_t) i])) != pcs.end()) c.push_back(i);
        if (c.empty()) return clamp(from);
        std::shuffle(c.begin(), c.end(), rng);
        std::stable_sort(c.begin(), c.end(), [&](int a, int b) { return std::abs(a - from) < std::abs(b - from); });
        return c[(size_t) std::min<int>((int) c.size() - 1, std::uniform_int_distribution<int>(0, std::max(0, spread - 1))(rng))];
    };

    double rep = std::min(.7, st.rep + (s.vibe == Vibe::hypnotic ? .2 : 0) + (s.role == Role::hook ? .1 : 0));
    auto thin = [&](std::vector<Cell> c) {
        Density d = counter ? Density::low : s.density;
        if (d == Density::low) {
            std::vector<Cell> o;
            for (size_t i = 0; i < c.size(); ++i) if (i % 2 == 0 || i + 1 == c.size()) o.push_back(c[i]);
            for (size_t i = 0; i + 1 < o.size(); ++i) o[i].len = o[i + 1].on - o[i].on; // fill the gaps: longer notes
            return o;
        }
        if (d == Density::high) {
            std::vector<Cell> o;
            for (auto x : c) if (x.len >= 2 && chance(rng) < .6) { o.push_back({x.on, x.len / 2}); o.push_back({x.on + x.len / 2, x.len - x.len / 2}); } else o.push_back(x);
            return o;
        }
        return c;
    };
    const auto A = thin(pick(rng, st.cells));
    auto Bsrc = pick(rng, st.cells);
    const auto B = thin(Bsrc);

    std::vector<Note> notes;
    int idx = clamp((lo + hi) / 2 + lv), lastLeap = 0;
    std::vector<int> motif;
    const int bars = p.chordCount;
    for (int b = 0; b < bars; ++b) {
        const auto& chord = p.chords[(size_t) b];
        auto pcs = chordPitchClasses(chord);
        std::vector<int> triad { chord.root, wrap12(chord.root + (chord.quality >= 0 ? 7 : scale[(size_t) ((chord.degree + 4) % 7)] - scale[(size_t) chord.degree])) };
        for (int pc : pcs) if (pc != triad[0] && std::abs(wrap12(pc - chord.root) - 3.5) <= .5) { triad.push_back(pc); break; }
        if (!st.ext) pcs = triad;
        const bool last = b == bars - 1, half = (b % 4) == 1 && bars >= 4;
        if (locked[(size_t) b]) {
            for (const auto& n : keep) if (n.start / (stepsPerBar * step) == b) notes.push_back(n);
            if (!notes.empty()) {
                auto it = std::find(pool.begin(), pool.end(), notes.back().pitch);
                if (it != pool.end()) idx = (int) (it - pool.begin());
            }
            continue;
        }
        const int section = b % 4;
        const auto& cell = (section == 0 || section == 2 || s.role == Role::hook) ? A : B;
        std::vector<std::array<int, 3>> bn; // on, len, poolIndex
        const bool restate = (section == 2 || (s.role == Role::hook && b > 0)) && !motif.empty() && !last;
        if (restate) {
            const int start = chordIdx(triad, motif[0], 1), shift = start - motif[0];
            for (size_t i = 0; i < cell.size(); ++i) {
                int ni = clamp((i < motif.size() ? motif[i] : motif.back()) + shift);
                const bool strong = cell[i].on % 4 == 0;
                if (strong && std::find(pcs.begin(), pcs.end(), wrap12(pool[(size_t) ni])) == pcs.end() && chance(rng) < .7)
                    ni = chordIdx(pcs, ni, 1);
                bn.push_back({cell[i].on, cell[i].len, ni});
            }
        } else {
            for (size_t i = 0; i < cell.size(); ++i) {
                const auto [o, l] = cell[i];
                const bool strong = o % 8 == 0 || (o % 4 == 0 && chance(rng) < .6);
                const double phase = (double) (b * stepsPerBar + o) / (bars * stepsPerBar);
                const int arch = phase < .55 ? 1 : -1;
                const bool lastInBar = i + 1 == cell.size();
                int ni;
                if (i == 0) ni = chordIdx(triad, idx + (section == 1 ? arch : 0), 2);
                else if (std::abs(lastLeap) >= 3) ni = idx - (lastLeap > 0 ? 1 : -1);          // gap-fill
                else if (!strong && chance(rng) < rep) ni = idx;
                else if (chance(rng) < st.arp) {
                    int found = -1;
                    if (arch > 0) { for (int k = idx + 1; k <= hi; ++k) if (std::find(triad.begin(), triad.end(), wrap12(pool[(size_t) k])) != triad.end()) { found = k; break; } }
                    else { for (int k = idx - 1; k >= lo; --k) if (std::find(triad.begin(), triad.end(), wrap12(pool[(size_t) k])) != triad.end()) { found = k; break; } }
                    ni = found >= 0 ? found : chordIdx(triad, idx, 2);
                }
                else if (strong || lastInBar) ni = chordIdx(pcs, idx + arch, 2);
                else {
                    const int dir = chance(rng) < .7 ? arch : -arch;
                    const int leap = chance(rng) < .15 ? std::min(st.leap, 2 + (int) (chance(rng) * 3)) : 1;
                    ni = idx + dir * leap;
                }
                ni = clamp(ni);
                lastLeap = ni - idx; idx = ni;
                bn.push_back({o, l, ni});
            }
        }
        if (section == 0 || (section == 2 && motif.empty())) { motif.clear(); for (auto& x : bn) motif.push_back(x[2]); }
        if ((last || half) && !bn.empty()) {
            auto& t = bn.back();
            t[2] = last ? chordIdx({wrap12(s.key)}, t[2], 1) : chordIdx(triad, t[2], 1);
            t[1] = std::max(t[1], 4);
            if (t[0] + t[1] > 16) t[1] = 16 - t[0];
        }
        for (size_t i = 0; i < bn.size(); ++i) {
            const auto [o, l, ni] = bn[i];
            idx = ni;
            const int tick = (b * stepsPerBar + o) * step;
            const bool phraseEnd = (last || half) && i + 1 == bn.size();
            const double factor = phraseEnd ? 1.0 : st.stac;
            const int vel = (o % 4 == 0 ? 100 : 84) + (int) (chance(rng) * 16);
            if (i + 1 < bn.size() && st.appr > 0 && l >= 2 && bn[i + 1][0] % 4 == 0 && chance(rng) < st.appr) {
                // chromatic approach: half-step below the next target, on the last 16th
                notes.push_back({tick, (int) std::max<double>(step / 2, (l - 1) * step * factor), pool[(size_t) ni], vel, false});
                notes.push_back({tick + (l - 1) * step, step, pool[(size_t) bn[i + 1][2]] - 1, vel - 14, false});
            } else {
                notes.push_back({tick, (int) std::max<double>(step / 2, l * step * factor - (factor < 1 ? 0 : step / 8)), pool[(size_t) ni], vel, false});
            }
        }
    }
    if (profile(s.style).glide)
        for (size_t i = 1; i < notes.size(); ++i)
            notes[i].glide = notes[i - 1].start + notes[i - 1].length >= notes[i].start - step / 2 && chance(rng) < .25;
    return notes;
}

double Engine::score(const Settings& s, const ccp::Progression& p, const std::vector<Note>& m) {
    if (m.size() < 3) return -100;
    double sc = 0;
    int strong = 0, strongHits = 0, steps = 0, leaps = 0, runs = 0, run = 1;
    int lo = 127, hi = 0;
    std::vector<int> distinct;
    for (size_t i = 0; i < m.size(); ++i) {
        const auto& n = m[i];
        lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch);
        if (std::find(distinct.begin(), distinct.end(), n.pitch) == distinct.end()) distinct.push_back(n.pitch);
        const int bar = std::min(p.chordCount - 1, n.start / (stepsPerBar * step));
        if ((n.start / step) % 4 == 0) {
            ++strong;
            auto pcs = chordPitchClasses(p.chords[(size_t) bar]);
            if (std::find(pcs.begin(), pcs.end(), wrap12(n.pitch)) != pcs.end()) ++strongHits;
        }
        if (i) {
            const int d = std::abs(n.pitch - m[i - 1].pitch);
            if (d > 0 && d <= 2) ++steps;
            if (d > 7) ++leaps;
            if (d == 0) { if (++run > 3) ++runs; } else run = 1;
        }
    }
    sc += strong ? 12.0 * strongHits / strong : 0;
    sc += 5.0 * steps / (double) m.size();
    sc -= 2.5 * leaps + 1.5 * runs;
    const int range = hi - lo;
    sc += range >= 5 && range <= 14 ? 3 : -2;
    sc += distinct.size() >= 4 ? 2 : -3;
    if (wrap12(m.back().pitch) == wrap12(s.key)) sc += 3;
    return sc;
}

uint64_t Engine::signature(const std::vector<Note>& m) const {
    uint64_t h = 1469598103934665603ull;
    for (auto& n : m) { h ^= (uint64_t) (n.start * 131 + n.pitch); h *= 1099511628211ull; }
    return h;
}

std::vector<Note> Engine::melodyFor(const Settings& s, const ccp::Progression& p, const std::vector<Note>& keep,
                                    const std::array<bool, maxBars>& locked) {
    std::vector<Note> best;
    double bestScore = -1e9;
    std::uniform_real_distribution<double> jitter(-1.5, 1.5);
    for (int attempt = 0; attempt < 24; ++attempt) {
        auto m = melodyAttempt(s, p, keep, locked);
        if (std::find(recent.begin(), recent.end(), signature(m)) != recent.end()) continue;
        const double sc = score(s, p, m) + jitter(rng);
        if (sc > bestScore) { bestScore = sc; best = std::move(m); }
    }
    if (best.empty()) best = melodyAttempt(s, p, keep, locked);
    recent.push_back(signature(best));
    while (recent.size() > 64) recent.pop_front();
    std::sort(best.begin(), best.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    return best;
}

Pattern Engine::generate(const Settings& s, const Pattern* current, const std::array<bool, maxBars>& locked) {
    Pattern out;
    out.settings = s;
    std::array<bool, ccp::maxChords> chordLocks {};
    const bool sameShape = current && current->bars() == s.bars;
    if (sameShape) for (int i = 0; i < s.bars; ++i) chordLocks[(size_t) i] = locked[(size_t) i];
    out.progression = harmony.generate(harmonySettings(s), sameShape ? &current->progression : nullptr, chordLocks, s.bars);
    out.bass = bassFor(s, out.progression);
    if (sameShape)  // locked bars keep their exact sub too
        for (int b = 0; b < s.bars; ++b) if (locked[(size_t) b]) {
            const int a = b * stepsPerBar * step, e = a + stepsPerBar * step;
            out.bass.erase(std::remove_if(out.bass.begin(), out.bass.end(), [&](const Note& n) { return n.start >= a && n.start < e; }), out.bass.end());
            for (auto& n : current->bass) if (n.start >= a && n.start < e) out.bass.push_back(n);
        }
    std::sort(out.bass.begin(), out.bass.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
    std::array<bool, maxBars> none {};
    out.melody = melodyFor(s, out.progression, sameShape ? current->melody : std::vector<Note> {}, sameShape ? locked : none);
    return out;
}

Pattern Engine::newMelody(const Pattern& p, const std::array<bool, maxBars>& locked) {
    Pattern out = p;
    out.melody = melodyFor(p.settings, p.progression, p.melody, locked);
    return out;
}

Pattern Engine::newBass(const Pattern& p) {
    Pattern out = p;
    out.bass = bassFor(p.settings, p.progression);
    return out;
}

Pattern Engine::variation(const Pattern& p, const std::array<bool, maxBars>& locked) {
    Pattern out = p;
    const auto scale = ccp::HarmonyEngine::scaleNotes(p.settings.key, p.settings.scale);
    auto inScale = [&](int m) { return std::find(scale.begin(), scale.end(), wrap12(m)) != scale.end(); };
    bool changed = false;
    for (int tries = 0; tries < 6 && !changed; ++tries)
        for (auto& n : out.melody) {
            const int bar = n.start / (stepsPerBar * step);
            if (bar < maxBars && locked[(size_t) bar]) continue;
            if ((n.start / step) % 4 != 0 && chance(rng) < .4) {
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
    const auto gs = harmonySettings(p.settings);
    const ccp::Chord* prev = bar ? &out.progression.chords[(size_t) (bar - 1)] : nullptr;
    out.progression.chords[(size_t) bar] = ccp::HarmonyEngine::rebuild(gs, chord, prev, bar);
    for (int i = bar + 1; i < out.bars(); ++i)
        out.progression.chords[(size_t) i] = ccp::HarmonyEngine::rebuild(gs, out.progression.chords[(size_t) i], &out.progression.chords[(size_t) (i - 1)], i);
    // move this bar's sub to the new root (keeping its rhythm), then rewrite only this bar's melody
    const int a = bar * stepsPerBar * step, e = a + stepsPerBar * step;
    const int shift = subPitch(out.progression.chords[(size_t) bar]) - subPitch(p.progression.chords[(size_t) bar]);
    for (auto& n : out.bass) if (n.start >= a && n.start < e) n.pitch += shift;
    if (bar > 0) { // the previous bar's approach note should lead into the new root
        for (auto& n : out.bass) if (n.start >= a - stepsPerBar * step && n.start < a && n.start % (stepsPerBar * step) >= 12 * step && n.pitch != subPitch(out.progression.chords[(size_t) (bar - 1)]) && n.pitch != subPitch(out.progression.chords[(size_t) (bar - 1)]) + 12)
            n.pitch = subPitch(out.progression.chords[(size_t) bar]) - 1;
    }
    std::array<bool, maxBars> locks {};
    for (int i = 0; i < out.bars(); ++i) locks[(size_t) i] = i != bar;
    out.melody = melodyFor(out.settings, out.progression, out.melody, locks);
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
    for (auto& n : notes) { ev.push_back({n.start, true, n.pitch, n.velocity}); ev.push_back({n.start + std::max(1, n.length), false, n.pitch, 0}); }
    std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : (!a.on && b.on); });
    std::vector<uint8_t> tr;
    const uint32_t us = (uint32_t) (60000000 / std::max(1, bpm));
    vlq(tr, 0); tr.insert(tr.end(), {0xff, 0x51, 0x03, (uint8_t) (us >> 16), (uint8_t) (us >> 8), (uint8_t) us});
    int last = 0;
    const uint8_t ch = (uint8_t) (channel & 15);
    for (auto& e : ev) {
        vlq(tr, (uint32_t) std::max(0, e.tick - last)); last = e.tick;
        tr.push_back((uint8_t) ((e.on ? 0x90 : 0x80) | ch)); tr.push_back((uint8_t) std::clamp(e.note, 0, 127)); tr.push_back((uint8_t) std::clamp(e.vel, 0, 127));
    }
    vlq(tr, 0); tr.insert(tr.end(), {0xff, 0x2f, 0});
    std::vector<uint8_t> out { 'M', 'T', 'h', 'd' };
    be32(out, 6); be16(out, 0); be16(out, 1); be16(out, ppq);
    out.insert(out.end(), { 'M', 'T', 'r', 'k' }); be32(out, (uint32_t) tr.size());
    out.insert(out.end(), tr.begin(), tr.end());
    return out;
}

const char* styleName(Style x) { static const char* n[] { "FUNK BR", "POP", "TRAP", "PISEIRO", "R&B", "HOUSE", "LO-FI", "PHONK" }; return n[(int) x]; }
const char* vibeName(Vibe x) { static const char* n[] { "DARK", "HAPPY", "SAD", "HYPNOTIC", "EPIC", "CHILL", "ROMANTIC" }; return n[(int) x]; }
const char* roleName(Role x) { static const char* n[] { "LEAD", "HOOK", "COUNTER" }; return n[(int) x]; }
const char* densityName(Density x) { static const char* n[] { "LOW", "MID", "HIGH" }; return n[(int) x]; }
const char* rangeName(Range x) { static const char* n[] { "LOW", "MID", "HIGH" }; return n[(int) x]; }
}
