#include "Composer.h"
#include "Genres.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace mc::composer {
namespace {
using Rng = std::mt19937;
double U(Rng& r) { return std::uniform_real_distribution<double>(0, 1)(r); }
int D(Rng& r, int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(r); }
int wrap12(int n) { n %= 12; return n < 0 ? n + 12 : n; }
bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
int sgn(int x) { return (x > 0) - (x < 0); }

/* ---------------------------------------------------------------- style and vibe profiles
   Style = broad rhythmic and phrasing habits of a genre (never fixed MIDI). */
struct StyleTaste {
    double notesPerBar;   // typical events per bar at MID density
    double syncopation;   // how often a beat starts off the beat or ties over
    double sixteenths;    // how often a beat is split into 16ths
    double restiness;     // how often a beat (or half) is left empty
    double literal;       // how literally the motif comes back (hooky genres repeat more)
    double leapiness;     // tolerance for leaps
    double legato;        // note length as a share of the gap to the next note
    double twoBarMotif;   // chance the motif is two bars long (4 / 8 bar phrases)
};
StyleTaste styleTaste(Style s) {   // the genre engine's melody habits (Genres.cpp)
    const auto& g = genre::get(s);
    return { g.notesPerBar, g.syncopation, g.sixteenths, g.restiness, g.literal, g.leapiness, g.legato, g.twoBarMotif };
}

/* Vibe = the intention and character of the phrase (it also shapes the preview sound, separately).
   CLEAN      balanced, clear contour, normal spacing
   DREAMY     fewer, longer notes, stepwise and suspended, more space, a little higher
   DARK       lower register, falling contours, more half-steps against the chord, heavier
   LO-FI TAPE relaxed: slightly sparser, legato, small intervals, laid-back endings
   SPACE      sparse and wide: long notes, open 4ths/5ths/octaves, lots of air, high */
struct VibeTaste {
    double density, length, stepBias, leapBias, rests;
    int registerShift;
    double contour[5];   // arch, rise, fall, wave, valley
    double tension;      // bonus for prepared non-chord tones on strong beats (suspensions)
};
const VibeTaste& vibeTaste(Vibe v) {
    static const VibeTaste t[] {
        /* clean  */ { 1.00, 1.00, 0.0, 0.0, 1.0, 0, { 3, 2, 2, 2, 1 }, 0.0 },
        /* dreamy */ { 0.75, 1.50, 0.8, -0.4, 1.4, 3, { 3, 3, 1, 1, 1 }, 1.6 },
        /* dark   */ { 0.95, 1.10, 0.3, 0.0, 1.1, -5, { 1, 1, 3, 1, 2 }, 0.8 },
        /* tape   */ { 0.85, 1.25, 0.6, -0.3, 1.2, -2, { 2, 1, 2, 2, 2 }, 0.6 },
        /* space  */ { 0.60, 1.80, -0.6, 1.2, 1.6, 5, { 2, 3, 1, 1, 2 }, 0.4 },
    };
    return t[std::clamp((int) v, 0, (int) Vibe::count - 1)];
}

/* ---------------------------------------------------------------- musical context */
struct Context {
    const Pattern* pat {};
    int key {}, bars {}, length {};
    std::vector<int> scale;           // pitch classes
    Register reg {};
    StyleTaste style {};
    const genre::Engine* g {};
    VibeTaste vibe {};
    Role role {};
    Density density {};
    Feel feel {};
    double notesPerBar {};
    bool hook {}, counter {};
    const std::vector<Note>* lead {};

    std::vector<int> chordAt(int tick) const {
        if (pat->bars() <= 0) {   // no progression: the tonic triad of the scale
            return { scale[0], scale[2 % scale.size()], scale[4 % scale.size()] };
        }
        auto pcs = Engine::chordPitchClasses(pat->chordAt(std::clamp(tick, 0, length - 1)));
        if (pcs.size() > 4) pcs.resize(4);
        return pcs;
    }
    bool inScale(int m) const { return has(scale, wrap12(m)); }
    int degreeOf(int m) const { for (size_t i = 0; i < scale.size(); ++i) if (scale[i] == wrap12(m)) return (int) i; return -1; }
    int leadAt(int tick) const {   // lead pitch sounding at tick (counter role), 0 if none
        if (!lead) return 0;
        for (const auto& n : *lead) if (n.start <= tick && tick < n.start + n.length) return n.pitch;
        return 0;
    }
};

Context makeContext(const Pattern& p, const std::vector<Note>* lead) {
    Context c;
    c.pat = &p;
    c.key = p.key;
    c.bars = std::max(1, p.bars() > 0 ? p.bars() : p.settings.bars);
    c.length = c.bars * barTicks;
    const auto sn = ccp::HarmonyEngine::scaleNotes(p.key, p.scale);
    c.scale.assign(sn.begin(), sn.end());
    for (auto& x : c.scale) x = wrap12(x);
    c.reg = registerFor(p.settings);
    c.style = styleTaste(p.style);
    c.g = &genre::get(p.style);
    c.vibe = vibeTaste(p.settings.vibe);
    c.role = p.settings.role;
    c.density = p.settings.density;
    c.feel = p.settings.feel;
    c.hook = c.role == Role::hook;
    c.counter = c.role == Role::counter;
    const double dens = c.density == Density::low ? 0.55 : c.density == Density::high ? 1.6 : 1.0;
    c.notesPerBar = c.style.notesPerBar * dens * c.vibe.density * (c.counter ? 0.7 : 1.0) * (c.hook ? 0.9 : 1.0);
    c.lead = lead;
    return c;
}

int beatStrength(int tick) {   // 3 bar line, 2 beat 3, 1 beats 2/4, 0 off-beat
    const int inBar = tick % barTicks;
    if (inBar == 0) return 3;
    if (inBar == 2 * ppq) return 2;
    if (inBar % ppq == 0) return 1;
    return 0;
}

/* ---------------------------------------------------------------- rhythm */
struct Ev { int start, length; };

// One-beat cells, in ticks inside the beat. An empty cell is a rest; `tie` holds the previous note.
struct Cell { std::vector<std::pair<int, int>> notes; bool tie = false; bool triplet = false; double synco = 0; int sixteenths = 0; };
const std::vector<Cell>& cells() {
    const int q = ppq, e = ppq / 2, s = ppq / 4, t = ppq / 3;
    static const std::vector<Cell> c {
        { { { 0, q } } },                                         // quarter
        { { { 0, e }, { e, e } } },                               // two 8ths
        { { { 0, e } } },                                         // 8th + rest
        { { { e, e } }, false, false, 1.0 },                      // rest + off-beat 8th
        { { { 0, s }, { s, s }, { e, s }, { 3 * s, s } }, false, false, 0, 4 },   // four 16ths
        { { { 0, s }, { s, s }, { e, e } }, false, false, 0, 2 },                 // 16-16-8
        { { { 0, e }, { e, s }, { 3 * s, s } }, false, false, 0, 2 },             // 8-16-16
        { { { 0, 3 * s }, { 3 * s, s } }, false, false, 0.3, 1 },                 // dotted 8th + 16th
        { { { 0, s }, { s, 3 * s } }, false, false, 0.8, 1 },                     // 16th + dotted 8th (snap)
        { { { s, s }, { e, e } }, false, false, 1.0, 1 },                         // rest-16-8 (pushed)
        { { { 3 * s, s } }, false, false, 1.0, 1 },                               // pickup 16th into the next beat
        { {}, false },                                            // rest
        { {}, true },                                             // tie / hold
        { { { 0, t }, { t, t }, { 2 * t, t } }, false, true },    // triplet 8ths
        { { { 0, 2 * t }, { 2 * t, t } }, false, true, 0.2 },     // triplet swing pair
        { { { t, t }, { 2 * t, t } }, false, true, 0.9 },         // triplet off-beats
    };
    return c;
}

int beatStrength(int tick);

// Genre groove: each bar starts from one of the genre's own rhythm figures, then is thinned or split to the
// wanted density and given a breath. Triplet feel uses the generic cells below (every genre can roll).
std::vector<Ev> grooveRhythm(const Context& ctx, Rng& r, int bars, double densityScale) {
    const auto& G = ctx.g->groove;
    const double want = ctx.notesPerBar * densityScale;
    std::vector<Ev> out;
    for (int b = 0; b < bars; ++b) {
        std::vector<double> w;
        for (const auto& bar : G) w.push_back(std::exp(-0.6 * std::abs((double) bar.size() - want)) + 0.05);
        std::discrete_distribution<int> pickBar(w.begin(), w.end());
        static thread_local int hookBar = 0;
        const int chosen = ctx.hook && b > 0 ? hookBar : pickBar(r);   // a hook repeats one figure
        hookBar = chosen;
        std::vector<Ev> notes;
        for (const auto& h : G[(size_t) chosen]) notes.push_back({ h.on * step, h.len * step });
        // thin: drop the weakest (off-beat, short) note until close to the wanted count
        while ((double) notes.size() > want + 1.0 && notes.size() > 2) {
            size_t worst = 1; double ws = 1e9;
            for (size_t i = 1; i < notes.size(); ++i) {
                const double sc = beatStrength(notes[i].start) * 2 + notes[i].length / (double) step + U(r) * .5;
                if (sc < ws) { ws = sc; worst = i; }
            }
            notes.erase(notes.begin() + (long) worst);
        }
        // split: halve the longest note until close to the wanted count
        while ((double) notes.size() < want - 1.0) {
            size_t longest = 0;
            for (size_t i = 1; i < notes.size(); ++i) if (notes[i].length > notes[longest].length) longest = i;
            if (notes[longest].length < 2 * step) break;
            const int half = notes[longest].length / 2 / step * step;
            notes.insert(notes.begin() + (long) longest + 1, Ev { notes[longest].start + half, notes[longest].length - half });
            notes[longest].length = half;
        }
        // breath: sometimes leave a weak note out (rests are part of the phrase)
        if (notes.size() > 2 && U(r) < ctx.style.restiness * ctx.vibe.rests * .35) {
            const size_t i = 1 + (size_t) D(r, 0, (int) notes.size() - 2);
            if (beatStrength(notes[i].start) < 2) notes.erase(notes.begin() + (long) i);
        }
        for (auto e : notes) out.push_back({ b * barTicks + e.start, e.length });
    }
    if (out.empty()) out.push_back({ 0, ppq });
    return out;
}

// Rhythm for `bars` bars, aiming at ctx.notesPerBar events per bar.
std::vector<Ev> makeRhythm(const Context& ctx, Rng& r, int bars, double densityScale = 1.0) {
    if (ctx.feel != Feel::triplet && ctx.g && !ctx.g->groove.empty()) return grooveRhythm(ctx, r, bars, densityScale);
    const auto& C = cells();
    const double perBeat = ctx.notesPerBar * densityScale / 4.0;
    const bool tripletFeel = ctx.feel == Feel::triplet;
    std::vector<Ev> out;
    int restBeats = 0;
    for (int b = 0; b < bars * 4; ++b) {
        const int beatStart = b * ppq;
        std::vector<double> w(C.size(), 0.0);
        for (size_t i = 0; i < C.size(); ++i) {
            const auto& c = C[i];
            if (c.triplet && !tripletFeel) continue;
            const double n = (double) c.notes.size();
            double x = std::exp(-1.6 * std::abs(n - perBeat));                    // close to the wanted activity
            if (c.notes.empty() && !c.tie) x = 0.35 * ctx.style.restiness * ctx.vibe.rests * (perBeat < 1.2 ? 2.0 : 0.6);
            if (c.tie) x = (b % 4 == 0 || out.empty()) ? 0 : 0.25 * ctx.vibe.length * (perBeat < 1.3 ? 1.5 : 0.5);
            if (c.synco > 0) x *= 0.4 + 1.6 * ctx.style.syncopation * c.synco;
            if (c.sixteenths > 0) x *= (ctx.density == Density::low ? 0.15 : 0.3) + 1.4 * ctx.style.sixteenths * (ctx.density == Density::high ? 1.6 : 1.0);
            if (tripletFeel) x *= c.triplet ? 4.0 : 0.35;
            if (b % 4 == 0 && c.notes.empty()) x *= b == 0 ? 0.05 : 0.5;          // phrases start, bars rarely open on silence
            w[i] = std::max(0.0, x);
        }
        // breathing: after several busy beats, a rest or a held note becomes likely
        if (restBeats == 0 && b >= 3 && out.size() >= 4) { w[11] *= 2.5; w[12] *= 2.0; }
        const double total = std::accumulate(w.begin(), w.end(), 0.0);
        size_t pickI = 0;
        if (total > 0) {
            double x = U(r) * total;
            for (size_t i = 0; i < w.size(); ++i) { x -= w[i]; if (x <= 0) { pickI = i; break; } }
        }
        const auto& c = C[pickI];
        if (c.tie && !out.empty()) { out.back().length = beatStart + ppq - out.back().start; continue; }
        if (c.notes.empty()) { ++restBeats; continue; }
        for (auto [on, len] : c.notes) out.push_back({ beatStart + on, len });
    }
    if (out.empty()) out.push_back({ 0, ppq });
    return out;
}

/* ---------------------------------------------------------------- pitch */
enum class Target { none, open, tonic, chord };
struct PitchState {
    int prev = 0, prevInterval = 0, repeats = 0, lastInterval2 = 0;
};

int contourDir(int contour, double phase) {   // +1 up, -1 down
    switch (contour) {
        case 1:  return phase < .8 ? 1 : -1;                         // rise
        case 2:  return phase < .15 ? 1 : -1;                        // fall
        case 3:  return std::sin(phase * 6.2832 * 1.5) >= 0 ? 1 : -1; // wave
        case 4:  return phase < .5 ? -1 : 1;                         // valley
        default: return phase < .6 ? 1 : -1;                         // arch
    }
}

int choosePitch(const Context& ctx, Rng& r, const PitchState& st, int tick, int dir, int motifInterval, Target target,
                int lo, int hi, int centre) {
    const auto& W = pitchWeights();
    const auto chord = ctx.chordAt(tick);
    const int strength = beatStrength(tick);
    const bool strong = strength >= 2;
    struct Cand { int m; double s; };
    std::vector<Cand> cand;
    for (int m = lo; m <= hi; ++m) {
        if (st.prev && std::abs(m - st.prev) > 12) continue;
        double s = 0;
        const bool inS = ctx.inScale(m);
        if (!inS) s += W.outOfScale;
        const bool ct = has(chord, wrap12(m));
        if (strong) {
            if (ct) s += W.chordStrong;
            else {
                // a suspension: the previous note held into the strong beat, resolving down by step later
                const bool prepared = st.prev == m && inS;
                s += prepared ? W.nonChordStrong + ctx.vibe.tension * 1.4 : W.nonChordStrong - (strength == 3 ? 1.0 : 0.0);
            }
        } else if (ct) s += W.chordWeak * (strength == 1 ? 1.5 : 1.0);
        else if (inS && st.prev && std::abs(m - st.prev) <= 2) s += 2.0 * ctx.g->passing;   // passing / neighbour note (genre taste)
        if (st.prev) {
            const int iv = m - st.prev, a = std::abs(iv);
            if (a == 0) s += st.repeats >= 2 ? W.tooManyRepeats : W.repeat * (0.5 + ctx.style.literal);
            else if (a <= 2) s += W.step + ctx.vibe.stepBias;
            else if (a <= 4) s += W.third;
            else if (a <= 7) s += W.fourthFifth + ctx.vibe.leapBias + ctx.style.leapiness - 0.3;
            else s += W.bigLeap + ctx.vibe.leapBias * 0.8 + (a == 12 ? 0.8 : 0.0);
            if (a > 0 && sgn(iv) == dir) s += W.contour;
            if (std::abs(st.prevInterval) >= 5) {
                if (a > 0 && a <= 2 && sgn(iv) != sgn(st.prevInterval)) s += W.leapRecovery;
                if (a >= 5 && sgn(iv) == sgn(st.prevInterval)) s += W.leapAgain;
            }
            if (iv != 0 && iv == st.prevInterval && iv == st.lastInterval2) s += W.sequenceRepeat;
            if (motifInterval != 999) {
                if (iv == motifInterval) s += W.motifShape * (0.6 + ctx.style.literal);
                else if (sgn(iv) == sgn(motifInterval)) s += W.motifDirection;
            }
        }
        s -= W.registerPull * std::abs(m - centre);
        const int deg = ctx.degreeOf(m);
        switch (target) {
            case Target::tonic: if (deg == 0) s += W.cadence; else if (ct) s += W.cadence * .25; break;
            case Target::open:  if (deg == 1 || deg == 4 || deg == 6) s += W.cadence * .7; else if (ct && deg != 0) s += W.cadence * .4; break;
            case Target::chord: if (ct) s += W.cadence * .5; break;
            default: break;
        }
        if (ctx.lead) {
            if (const int l = ctx.leadAt(tick)) {
                const int iv = std::abs(l - m) % 12;
                if (iv == 3 || iv == 4 || iv == 8 || iv == 9) s += W.counterConsonance;
                else if (iv == 0 || iv == 1 || iv == 2 || iv == 6 || iv == 11) s -= W.counterConsonance;
                if (m >= l) s -= 1.0;   // stay under the lead
            }
        }
        cand.push_back({ m, s });
    }
    if (cand.empty()) return std::clamp(st.prev ? st.prev : centre, lo, hi);
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.s > b.s; });
    const int n = std::min<int>(W.candidates, (int) cand.size());
    std::vector<double> p((size_t) n);
    for (int i = 0; i < n; ++i) p[(size_t) i] = std::exp((cand[(size_t) i].s - cand[0].s) / W.temperature);
    std::discrete_distribution<int> pick(p.begin(), p.end());
    return cand[(size_t) pick(r)].m;
}

/* ---------------------------------------------------------------- motif and phrase */
struct Motif {
    std::vector<Ev> rhythm;   // relative to the motif start
    std::vector<int> pitch;
    int bars = 1;
    int contour = 0;
};

// Writes pitches for a rhythm placed at `offset`. Optional `shape` gives motif intervals to keep.
std::vector<int> writePitches(const Context& ctx, Rng& r, PitchState& st, const std::vector<Ev>& rhythm, int offset,
                              int contour, const std::vector<int>* shape, Target ending, int lift) {
    std::vector<int> out;
    const int lo = ctx.reg.lo, hi = ctx.reg.hi, centre = std::clamp(ctx.reg.centre + lift, lo + 3, hi - 3);
    const int span = rhythm.empty() ? 1 : rhythm.back().start + rhythm.back().length;
    for (size_t i = 0; i < rhythm.size(); ++i) {
        const int tick = offset + rhythm[i].start;
        const double phase = (double) rhythm[i].start / std::max(1, span);
        const int dir = contourDir(contour, phase);
        int mi = 999;
        if (shape && i > 0 && i < shape->size()) mi = (*shape)[i] - (*shape)[i - 1];
        const Target t = i + 1 == rhythm.size() ? ending : Target::none;
        int m;
        if (!st.prev) {   // first note of the melody: a chord tone near the centre
            const auto chord = ctx.chordAt(tick);
            m = centre;
            for (int d = 0; d < 12; ++d) {
                if (has(chord, wrap12(centre - d))) { m = centre - d; break; }
                if (has(chord, wrap12(centre + d))) { m = centre + d; break; }
            }
            if (U(r) < .5) { const int up = m + (has(chord, wrap12(m + 3)) ? 3 : 4); if (up <= hi && has(chord, wrap12(up))) m = up; }
        } else {
            m = choosePitch(ctx, r, st, tick, dir, mi, t, lo, hi, centre);
        }
        const int iv = st.prev ? m - st.prev : 0;
        st.repeats = st.prev && iv == 0 ? st.repeats + 1 : 0;
        st.lastInterval2 = st.prevInterval;
        st.prevInterval = iv;
        st.prev = m;
        out.push_back(m);
    }
    return out;
}

// Diatonic shift of a whole pitch list by `steps` scale steps (a sequence).
std::vector<int> shiftDiatonic(const Context& ctx, std::vector<int> p, int steps) {
    if (!steps) return p;
    for (auto& m : p) {
        const int dir = sgn(steps);
        for (int i = 0; i < std::abs(steps); ++i) { m += dir; while (!ctx.inScale(m)) m += dir; }
    }
    return p;
}

// Rhythmic variation of one unit: split, merge or displace one note, keeping the start.
std::vector<Ev> varyRhythm(const Context& ctx, Rng& r, std::vector<Ev> rh) {
    if (rh.size() < 2) return rh;
    const int op = D(r, 0, 2);
    const size_t i = (size_t) D(r, 1, (int) rh.size() - 1);
    if (op == 0 && rh[i].length >= ppq / 2 && ctx.density != Density::low) {        // split
        const int half = rh[i].length / 2;
        rh[i].length = half;
        rh.insert(rh.begin() + (long) i + 1, { rh[i].start + half, half });
    } else if (op == 1 && i + 1 < rh.size()) {                                        // merge
        rh[i].length = rh[i + 1].start + rh[i + 1].length - rh[i].start;
        rh.erase(rh.begin() + (long) i + 1);
    } else {                                                                          // displace by a 16th
        const int d = (U(r) < .5 ? -1 : 1) * step;
        const int lo = rh[i - 1].start + step, hi = (i + 1 < rh.size() ? rh[i + 1].start : rh[i].start + rh[i].length) - step;
        const int ns = rh[i].start + d;
        if (ns >= lo && ns <= hi) { const int end = rh[i].start + rh[i].length; rh[i].start = ns; rh[i].length = std::max(step / 2, end - ns); }
    }
    return rh;
}

// Phrase plan: what each motif-sized unit does.
enum class Unit { statement, sequence, develop, fragment, climax, open, cadence };

Unit unitOf(char c) {
    switch (c) {
        case 'Q': return Unit::sequence; case 'D': return Unit::develop; case 'F': return Unit::fragment;
        case 'O': return Unit::open; case 'X': return Unit::climax; case 'C': return Unit::cadence; default: return Unit::statement;
    }
}

// Phrase form from the genre engine (hook role keeps its repeat-heavy form).
std::vector<Unit> plan(const Context& ctx, Rng& r, int units) {
    using U_ = Unit;
    if (units == 1) return { U_::cadence };
    if (units == 2) return { U_::statement, U_::cadence };
    if (ctx.hook) {
        if (units == 4) return { U_::statement, U_::statement, U_::develop, U_::cadence };
        return { U_::statement, U_::statement, U_::develop, U_::open, U_::statement, U_::statement, U_::climax, U_::cadence };
    }
    const auto& forms = ctx.g->forms;
    const char* a = forms[(size_t) D(r, 0, (int) forms.size() - 1)];
    std::vector<Unit> out;
    for (int i = 0; i < 4; ++i) out.push_back(unitOf(a[i]));
    if (units == 4) return out;
    // 8 units: the first half ends open, the second half reaches the climax before its close
    const char* b = forms[(size_t) D(r, 0, (int) forms.size() - 1)];
    if (out[3] == U_::cadence) out[3] = U_::open;
    for (int i = 0; i < 4; ++i) out.push_back(unitOf(b[i]));
    if (out[6] == U_::statement || out[6] == U_::develop || out[6] == U_::sequence) out[6] = U_::climax;
    return out;
}

std::vector<Note> writeOne(const Context& ctx, uint32_t seed) {
    Rng r(seed);
    // motif length: 1 bar, or 2 bars in longer phrases for some styles
    const int bars = ctx.bars;
    int motifBars = 1;
    if (bars >= 4 && !ctx.hook && U(r) < ctx.style.twoBarMotif) motifBars = 2;
    const int units = bars / motifBars;
    const auto& vt = ctx.vibe;
    double cw[5]; for (int i = 0; i < 5; ++i) cw[i] = vt.contour[i] * ctx.g->contour[(size_t) i];
    std::discrete_distribution<int> contourPick(cw, cw + 5);

    Motif motif;
    motif.bars = motifBars;
    motif.contour = contourPick(r);
    motif.rhythm = makeRhythm(ctx, r, motifBars);
    // hook: a clearer identity, so fewer events and one repeated rhythmic figure
    if (ctx.hook && motif.rhythm.size() > 3 && U(r) < .5) {
        const int half = motifBars * barTicks / 2;
        std::vector<Ev> h;
        for (auto e : motif.rhythm) if (e.start < half) h.push_back(e);
        for (auto e : h) motif.rhythm.push_back({ e.start + half, e.length });
        std::sort(motif.rhythm.begin(), motif.rhythm.end(), [](const Ev& a, const Ev& b) { return a.start < b.start; });
        motif.rhythm.erase(std::unique(motif.rhythm.begin(), motif.rhythm.end(), [](const Ev& a, const Ev& b) { return a.start == b.start; }), motif.rhythm.end());
    }

    PitchState st;
    std::vector<Note> out;
    const auto unitsPlan = plan(ctx, r, units);
    int prevSeqShift = 0;
    for (int u = 0; u < units; ++u) {
        const int offset = u * motifBars * barTicks;
        const Unit kind = unitsPlan[(size_t) u];
        std::vector<Ev> rh = motif.rhythm;
        std::vector<int> pitches;
        Target ending = Target::none;
        int lift = 0;
        if (kind == Unit::open) ending = Target::open;
        if (kind == Unit::cadence) ending = Target::tonic;
        if (kind == Unit::climax) lift = 4;

        if (u == 0) {
            pitches = writePitches(ctx, r, st, rh, offset, motif.contour, nullptr, units == 1 ? Target::tonic : Target::none, 0);
            motif.pitch = pitches;
        } else {
            switch (kind) {
                case Unit::statement: {
                    // the motif returns; if it no longer fits the chord, it is moved diatonically to fit
                    int best = 0; double bestFit = -1e9;
                    for (int sh : { 0, -1, 1, -2, 2, 3, -3 }) {
                        const auto p = shiftDiatonic(ctx, motif.pitch, sh);
                        double fit = -0.3 * std::abs(sh);
                        for (size_t i = 0; i < p.size(); ++i)
                            if (beatStrength(offset + rh[i].start) >= 2) fit += has(ctx.chordAt(offset + rh[i].start), wrap12(p[i])) ? 1.0 : -1.0;
                        if (fit > bestFit) { bestFit = fit; best = sh; }
                    }
                    if (!ctx.hook && U(r) > ctx.style.literal) rh = varyRhythm(ctx, r, rh);   // a hook keeps its rhythm
                    if (rh.size() == motif.rhythm.size()) pitches = shiftDiatonic(ctx, motif.pitch, best);
                    else { st.prev = st.prev; pitches = writePitches(ctx, r, st, rh, offset, motif.contour, &motif.pitch, ending, 0); }
                    break;
                }
                case Unit::sequence: {
                    int sh = prevSeqShift ? prevSeqShift : (U(r) < .5 ? 1 : -1) * D(r, 1, 2);
                    prevSeqShift = sh;
                    pitches = shiftDiatonic(ctx, motif.pitch, sh);
                    // keep strong beats consonant: nudge any clashing strong-beat note to the nearest chord tone
                    for (size_t i = 0; i < pitches.size(); ++i) {
                        const int t = offset + rh[i].start;
                        if (beatStrength(t) >= 2 && !has(ctx.chordAt(t), wrap12(pitches[i]))) {
                            for (int d = 1; d <= 2; ++d) {
                                if (has(ctx.chordAt(t), wrap12(pitches[i] - d)) && ctx.inScale(pitches[i] - d)) { pitches[i] -= d; break; }
                                if (has(ctx.chordAt(t), wrap12(pitches[i] + d)) && ctx.inScale(pitches[i] + d)) { pitches[i] += d; break; }
                            }
                        }
                    }
                    break;
                }
                case Unit::fragment: {
                    // the first half of the motif, twice: development by fragmentation
                    const int half = motifBars * barTicks / 2;
                    std::vector<Ev> f; std::vector<int> fp;
                    for (size_t i = 0; i < rh.size(); ++i) if (rh[i].start < half) { f.push_back(rh[i]); fp.push_back(motif.pitch[i]); }
                    if (f.empty()) { pitches = writePitches(ctx, r, st, rh, offset, motif.contour, &motif.pitch, ending, 0); break; }
                    rh = f;
                    for (auto e : f) rh.push_back({ e.start + half, e.length });
                    auto second = shiftDiatonic(ctx, fp, U(r) < .5 ? 1 : -1);
                    pitches = fp; pitches.insert(pitches.end(), second.begin(), second.end());
                    break;
                }
                case Unit::develop:
                case Unit::climax:
                case Unit::open:
                case Unit::cadence: {
                    rh = (kind == Unit::develop || kind == Unit::climax) ? varyRhythm(ctx, r, rh) : rh;
                    if (kind == Unit::cadence || kind == Unit::open) {
                        // the phrase ending: first part of the motif, then a longer arrival note
                        const int unitLen = motifBars * barTicks;
                        const int arrive = kind == Unit::cadence ? unitLen - barTicks / 2 - (U(r) < .5 ? barTicks / 2 : 0) : unitLen - barTicks / 4 * (1 + D(r, 0, 1));
                        std::vector<Ev> c;
                        for (auto e : rh) if (e.start < arrive - step) c.push_back(e);
                        for (auto& e : c) e.length = std::min(e.length, arrive - e.start);
                        c.push_back({ std::max(arrive, c.empty() ? 0 : c.back().start + step), unitLen - std::max(arrive, c.empty() ? 0 : c.back().start + step) });
                        rh = c;
                    }
                    pitches = writePitches(ctx, r, st, rh, offset, kind == Unit::cadence ? 2 : kind == Unit::climax ? 1 : motif.contour,
                                           kind == Unit::develop ? &motif.pitch : nullptr, ending, lift);
                    break;
                }
            }
        }
        if (!pitches.empty()) { st.prev = pitches.back(); st.prevInterval = pitches.size() > 1 ? pitches.back() - pitches[pitches.size() - 2] : 0; }
        for (size_t i = 0; i < rh.size() && i < pitches.size(); ++i)
            out.push_back({ offset + rh[i].start, rh[i].length, std::clamp(pitches[i], ctx.reg.lo - 2, ctx.reg.hi + 2), 96 });
    }
    return out;
}

/* ---------------------------------------------------------------- finishing: feel, articulation, velocity */
void finish(const Context& ctx, std::vector<Note>& m) {
    std::sort(m.begin(), m.end(), [](const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch > b.pitch; });
    m.erase(std::unique(m.begin(), m.end(), [](const Note& a, const Note& b) { return a.start == b.start; }), m.end());
    // swing: only straight off-beat 8ths (or 16ths in 16th-swing styles) move, to the triplet position
    if (ctx.feel == Feel::swing) {
        const bool sw16 = ctx.style.sixteenths >= .35;
        for (auto& n : m) {
            if (!sw16 && n.start % ppq == ppq / 2) n.start += ppq / 6;
            else if (sw16 && n.start % (ppq / 2) == ppq / 4) n.start += ppq / 12;
        }
    }
    // articulation: legato share of the gap, scaled by the vibe; the last note of a phrase rings
    const double leg = std::min(1.0, ctx.style.legato * ctx.vibe.length);
    int peak = 0; for (auto& n : m) peak = std::max(peak, n.pitch);
    for (size_t i = 0; i < m.size(); ++i) {
        const int next = i + 1 < m.size() ? m[i + 1].start : ctx.length;
        const int gap = next - m[i].start;
        int len = std::max(m[i].length, (int) (gap * leg * 0.9));
        len = std::min(len, gap - (leg >= .95 ? 0 : step / 8));
        m[i].length = std::clamp(len, step / 3, std::max(step / 3, ctx.length - m[i].start));
        const int bs = beatStrength(m[i].start);
        int v = 88 + (bs == 3 ? 14 : bs == 2 ? 8 : bs == 1 ? 3 : -6) + (m[i].pitch == peak ? 6 : 0);
        m[i].velocity = std::clamp(v + (int) ((m[i].start * 2654435761u >> 28) % 7) - 3, 50, 120);
    }
    m.erase(std::remove_if(m.begin(), m.end(), [&](const Note& n) { return n.start < 0 || n.start >= ctx.length || n.length <= 0; }), m.end());
}
} // namespace

PitchWeights& pitchWeights() { static PitchWeights w; return w; }
EvalWeights& evalWeights() { static EvalWeights w; return w; }

Register registerFor(const Settings& s) {
    Register g = s.range == Range::low ? Register { 48, 67, 57 } : s.range == Range::high ? Register { 65, 88, 76 } : Register { 55, 79, 67 };
    const int shift = vibeTaste(s.vibe).registerShift / 2 + (s.role == Role::counter ? -7 : 0);
    g.lo += shift; g.hi += shift; g.centre += shift;
    return g;
}

double evaluate(const Pattern& p, const std::vector<Note>& m) {
    const auto& W = evalWeights();
    if (m.size() < 2) return -1e6;
    const auto ctx = makeContext(p, nullptr);
    double score = 0;
    // harmony on strong beats
    int strong = 0, fit = 0;
    for (auto& n : m) if (beatStrength(n.start) >= 2) { ++strong; fit += has(ctx.chordAt(n.start), wrap12(n.pitch)) ? 1 : 0; }
    score += W.harmonic * (strong ? (double) fit / strong : .5);
    // rhythm variety
    std::vector<int> lens, gaps;
    for (size_t i = 0; i < m.size(); ++i) { lens.push_back(m[i].length / (step / 3)); if (i) gaps.push_back(m[i].start - m[i - 1].start); }
    std::sort(lens.begin(), lens.end()); lens.erase(std::unique(lens.begin(), lens.end()), lens.end());
    std::sort(gaps.begin(), gaps.end()); gaps.erase(std::unique(gaps.begin(), gaps.end()), gaps.end());
    score += W.rhythmVariety * (std::min<int>(3, (int) gaps.size()) / 3.0 + (lens.size() >= 2 ? .3 : -.5));
    // rests
    int sounding = 0; for (auto& n : m) sounding += n.length;
    const double rest = 1.0 - std::min(1.0, (double) sounding / ctx.length);
    const double rlo = ctx.density == Density::low ? .25 : ctx.density == Density::high ? .03 : .10;
    const double rhi = ctx.density == Density::low ? .65 : ctx.density == Density::high ? .35 : .45;
    score += W.rests * (rest >= rlo && rest <= rhi ? 1.0 : -std::min(1.0, std::min(std::abs(rest - rlo), std::abs(rest - rhi)) * 4));
    // climax
    int hi = 0, lo = 127; for (auto& n : m) { hi = std::max(hi, n.pitch); lo = std::min(lo, n.pitch); }
    std::vector<int> peakBars; for (auto& n : m) if (n.pitch == hi && !has(peakBars, n.start / barTicks)) peakBars.push_back(n.start / barTicks);
    score += W.climax * ((peakBars.size() == 1 ? 1.0 : peakBars.size() == 2 ? .3 : -.3) + (peakBars.front() >= ctx.bars / 2 ? .5 : 0));
    // range
    const int span = hi - lo;
    score += W.range * (span >= 5 && span <= 16 ? 1.0 : span > 19 ? -1.0 : 0.0);
    // leap balance
    int leaps = 0, recovered = 0;
    for (size_t i = 1; i + 1 < m.size(); ++i) {
        const int a = m[i].pitch - m[i - 1].pitch, b = m[i + 1].pitch - m[i].pitch;
        if (std::abs(a) >= 5) { ++leaps; if (b != 0 && sgn(b) != sgn(a) && std::abs(b) <= 4) ++recovered; }
    }
    score += W.leapBalance * (leaps ? (double) recovered / leaps : 1.0) - (leaps > (int) m.size() / 3 ? W.leapBalance : 0);
    // repetition: the motif should come back, but bars must not all be identical
    std::vector<std::vector<int>> bars((size_t) ctx.bars), rhythms((size_t) ctx.bars);
    for (auto& n : m) { const size_t b = (size_t) std::min(ctx.bars - 1, n.start / barTicks); bars[b].push_back(n.pitch * 4096 + n.start % barTicks); rhythms[b].push_back(n.start % barTicks); }
    int sameRhythm = 0, identical = 0;
    for (int b = 1; b < ctx.bars; ++b) {
        bool r = false, id = false;
        for (int a = 0; a < b; ++a) { r |= rhythms[(size_t) a] == rhythms[(size_t) b] && !rhythms[(size_t) b].empty(); id |= bars[(size_t) a] == bars[(size_t) b] && !bars[(size_t) b].empty(); }
        sameRhythm += r; identical += id;
    }
    if (ctx.bars > 1) {
        const double rr = (double) sameRhythm / (ctx.bars - 1), ii = (double) identical / (ctx.bars - 1);
        const double want = ctx.hook ? std::min(.95, ctx.g->targets.repeat + .2) : ctx.g->targets.repeat;
        score += W.repetition * (1.0 - std::abs(rr - want) * 1.5) - (ii >= .99 ? 3 * W.repetition : ii > .6 ? W.repetition : 0);
    }
    // ending
    const auto& last = m.back();
    score += W.ending * ((wrap12(last.pitch) == wrap12(p.key) ? 1.0 : has(ctx.chordAt(last.start), wrap12(last.pitch)) ? .3 : -.5)
                         + (last.length >= ppq ? .3 : 0));
    // density
    const double perBar = (double) m.size() / ctx.bars, want = ctx.notesPerBar;
    score += W.density * (1.0 - std::min(2.0, std::abs(perBar - want) / std::max(1.0, want * .5)));
    // genre fidelity: syncopation, busyness and space inside the genre engine's windows
    {
        const auto& T = ctx.g->targets;
        int off = 0; for (auto& n : m) off += n.start % ppq != 0;
        const double sync = (double) off / m.size();
        const double densF = ctx.density == Density::low ? .6 : ctx.density == Density::high ? 1.5 : 1.0;
        auto window = [](double x, double lo, double hi) { return x >= lo && x <= hi ? 1.0 : -std::min(1.5, std::min(std::abs(x - lo), std::abs(x - hi)) * 3); };
        score += W.genre * (window(sync, T.syncLo, T.syncHi) + window(perBar, T.notesLo * densF, T.notesHi * densF) * .7 + window(rest, T.restLo, T.restHi) * .5);
    }
    // monotony
    int run = 1, worstRun = 1, dirRun = 0, worstDir = 0;
    for (size_t i = 1; i < m.size(); ++i) {
        const int iv = m[i].pitch - m[i - 1].pitch;
        run = iv == 0 ? run + 1 : 1; worstRun = std::max(worstRun, run);
        if (i > 1 && iv != 0 && sgn(iv) == sgn(m[i - 1].pitch - m[i - 2].pitch)) worstDir = std::max(worstDir, ++dirRun); else dirRun = 0;
    }
    score -= W.monotony * ((worstRun > 3 ? worstRun - 3 : 0) + (worstDir > 5 ? worstDir - 5 : 0));
    return score;
}

std::vector<Note> compose(const Pattern& p, uint32_t seed, const std::vector<Note>* lead) {
    std::vector<Note> internalLead;
    if (p.settings.role == Role::counter && !lead) {   // no lead given: write one to answer (not returned)
        Pattern lp = p; lp.settings.role = Role::lead;
        internalLead = compose(lp, seed ^ 0x5bd1e995u, nullptr);
        lead = &internalLead;
    }
    const auto ctx = makeContext(p, lead);
    Rng r(seed);
    struct Cand { std::vector<Note> m; double s; };
    std::vector<Cand> cands;
    for (int i = 0; i < candidateCount; ++i) {
        auto m = writeOne(ctx, seed * 2654435761u + (uint32_t) i * 40503u + 17u);
        finish(ctx, m);
        if (m.size() < 2) continue;
        double s = evaluate(p, m);
        if (lead) {   // the counter should not shadow the lead's rhythm
            int same = 0; for (auto& n : m) for (auto& l : *lead) if (l.start == n.start) { ++same; break; }
            s -= 4.0 * same / (double) m.size();
        }
        cands.push_back({ std::move(m), s });
    }
    if (cands.empty()) return {};
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.s > b.s; });
    // not always the top score: one of the best three, weighted
    const double w[] { .6, .25, .15 };
    std::discrete_distribution<int> pick(w, w + std::min<int>(3, (int) cands.size()));
    return cands[(size_t) pick(r)].m;
}

double similarity(const std::vector<Note>& a, const std::vector<Note>& b) {
    if (a.empty() || b.empty()) return 0;
    int same = 0;
    for (auto& x : a) for (auto& y : b) if (x.start == y.start && x.pitch == y.pitch) { ++same; break; }
    return (double) same / (double) std::max(a.size(), b.size());
}

std::vector<Note> vary(const Pattern& p, const std::vector<Note>& source, uint32_t seed) {
    if (source.size() < 2) return compose(p, seed);
    const auto ctx = makeContext(p, nullptr);
    Rng r(seed);
    std::vector<Note> best = source;
    double bestScore = -1e9;
    for (int attempt = 0; attempt < 8; ++attempt) {
        auto m = source;
        std::sort(m.begin(), m.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
        // two or three of: rhythm, passing notes, ending, local contour, leap, distribution
        std::vector<int> ops { 0, 1, 2, 3, 4, 5 };
        std::shuffle(ops.begin(), ops.end(), r);
        ops.resize((size_t) D(r, 2, 3));
        for (int op : ops) {
            const int bar = D(r, 0, ctx.bars - 1);
            auto inBar = [&](const Note& n) { return n.start / barTicks == bar; };
            switch (op) {
                case 0: {   // rhythm: split a long note or join two short ones in one bar
                    for (size_t i = 0; i < m.size(); ++i) {
                        if (!inBar(m[i])) continue;
                        if (m[i].length >= ppq && U(r) < .6) { const int h = m[i].length / 2; m[i].length = h; Note n = m[i]; n.start += h; n.pitch = choosePitch(ctx, r, { m[i].pitch }, n.start, U(r) < .5 ? 1 : -1, 999, Target::none, ctx.reg.lo, ctx.reg.hi, ctx.reg.centre); m.insert(m.begin() + (long) i + 1, n); break; }
                        if (i + 1 < m.size() && inBar(m[i + 1]) && beatStrength(m[i + 1].start) == 0 && U(r) < .5) { m[i].length = m[i + 1].start + m[i + 1].length - m[i].start; m.erase(m.begin() + (long) i + 1); break; }
                    }
                    break;
                }
                case 1: {   // passing notes: weak-position notes move to another fitting note nearby
                    for (size_t i = 1; i + 1 < m.size(); ++i)
                        if (beatStrength(m[i].start) == 0 && U(r) < .45) {
                            PitchState st; st.prev = m[i - 1].pitch;
                            const int dir = sgn(m[i + 1].pitch - m[i - 1].pitch);
                            m[i].pitch = choosePitch(ctx, r, st, m[i].start, dir ? dir : 1, 999, Target::none,
                                                     std::max(ctx.reg.lo, m[i].pitch - 3), std::min(ctx.reg.hi, m[i].pitch + 3), m[i].pitch);
                        }
                    break;
                }
                case 2: {   // a different ending: the last notes approach the close another way
                    const int from = ctx.length - barTicks / 2;
                    PitchState st;
                    for (auto& n : m) {
                        if (n.start < from) { st.prev = n.pitch; continue; }
                        const bool last = &n == &m.back();
                        n.pitch = choosePitch(ctx, r, st, n.start, -1, 999, last ? (U(r) < .75 ? Target::tonic : Target::chord) : Target::none, ctx.reg.lo, ctx.reg.hi, ctx.reg.centre);
                        st.prevInterval = n.pitch - st.prev; st.prev = n.pitch;
                    }
                    break;
                }
                case 3: {   // local contour: mirror a short group inside one bar
                    std::vector<size_t> idx;
                    for (size_t i = 0; i < m.size(); ++i) if (inBar(m[i])) idx.push_back(i);
                    if (idx.size() >= 3) {
                        const size_t a = idx[(size_t) D(r, 0, (int) idx.size() - 3)];
                        const int pivot = m[a].pitch;
                        for (size_t k = a + 1; k < a + 3; ++k) {
                            int x = pivot - (m[k].pitch - pivot);
                            while (!ctx.inScale(x)) ++x;
                            if (x >= ctx.reg.lo && x <= ctx.reg.hi) m[k].pitch = x;
                        }
                    }
                    break;
                }
                case 4: {   // leaps: the widest jump becomes smaller, or a step becomes a leap
                    size_t at = 0; int widest = -1;
                    for (size_t i = 1; i < m.size(); ++i) { const int a = std::abs(m[i].pitch - m[i - 1].pitch); if (a > widest) { widest = a; at = i; } }
                    if (at && widest >= 7) m[at].pitch += m[at].pitch > m[at - 1].pitch ? -12 : 12;
                    else if (at) { int x = m[at].pitch + (U(r) < .5 ? 5 : -5); while (!ctx.inScale(x)) ++x; if (x >= ctx.reg.lo && x <= ctx.reg.hi) m[at].pitch = x; }
                    if (m[at].pitch < ctx.reg.lo || m[at].pitch > ctx.reg.hi) m[at].pitch = std::clamp(m[at].pitch, ctx.reg.lo, ctx.reg.hi);
                    break;
                }
                default: {  // distribution: a note moves to a nearby empty 8th
                    for (size_t i = 1; i + 1 < m.size(); ++i) {
                        if (!inBar(m[i]) || beatStrength(m[i].start) >= 2) continue;
                        const int ns = m[i].start + (U(r) < .5 ? -1 : 1) * (ppq / 2);
                        if (ns > m[i - 1].start && ns < m[i + 1].start) { const int end = m[i].start + m[i].length; m[i].start = ns; m[i].length = std::max(step / 2, std::min(end, m[i + 1].start) - ns); break; }
                    }
                    break;
                }
            }
        }
        finish(ctx, m);
        const double sim = similarity(source, m);
        if (sim < .45 || sim > .93) continue;   // must stay related, and must actually change
        const double s = evaluate(p, m) + 4.0 * (1.0 - std::abs(sim - .72));
        if (s > bestScore) { bestScore = s; best = m; }
    }
    return best;
}
}
