#include "Core/Composer.h"
#include "Core/MelodyEngine.h"
#include <chrono>
#include <cstdio>
#include <set>
#include <map>
#include <algorithm>
using namespace mc;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: " __VA_ARGS__); std::puts(""); } } while (0)

static int wrap12(int n) { n %= 12; return n < 0 ? n + 12 : n; }
static double avg(const std::vector<double>& v) { double s = 0; for (double x : v) s += x; return v.empty() ? 0 : s / v.size(); }

// basic validity every generated melody must have
static bool valid(const Pattern& p, const std::vector<Note>& m, const char* what) {
    bool ok = m.size() >= 2;
    for (auto& n : m) ok &= n.length > 0 && n.start >= 0 && n.start + n.length <= p.lengthTicks() && n.velocity > 0 && n.velocity <= 127 && n.pitch >= 30 && n.pitch <= 100;
    for (size_t i = 1; i < m.size(); ++i) ok &= m[i].start >= m[i - 1].start;
    if (!ok) std::printf("invalid melody (%s)\n", what);
    return ok;
}

// minimal Standard MIDI File reader for the round trip test
static std::vector<Note> readMidi(const std::vector<uint8_t>& b, int& tempoUs, int& lastTick) {
    std::vector<Note> out;
    if (b.size() < 22 || b[0] != 'M') return out;
    size_t i = 14 + 8;   // header chunk, then track header
    const size_t end = 14 + 8 + ((size_t) b[18] << 24 | (size_t) b[19] << 16 | (size_t) b[20] << 8 | b[21]);
    int tick = 0; uint8_t status = 0;
    std::map<int, std::pair<int, int>> on;
    auto vlq = [&]() { uint32_t v = 0; uint8_t c; do { c = b[i++]; v = (v << 7) | (c & 127); } while (c & 128); return v; };
    while (i < end) {
        tick += (int) vlq();
        uint8_t c = b[i];
        if (c == 0xff) { const uint8_t type = b[i + 1]; i += 2; const uint32_t len = vlq(); if (type == 0x51) tempoUs = b[i] << 16 | b[i + 1] << 8 | b[i + 2]; i += len; lastTick = tick; continue; }
        if (c & 0x80) { status = c; ++i; }
        const int note = b[i++], vel = b[i++];
        if ((status & 0xf0) == 0x90 && vel > 0) on[note] = { tick, vel };
        else if (on.count(note)) { out.push_back({ on[note].first, tick - on[note].first, note, on[note].second }); on.erase(note); }
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& c) { return a.start < c.start; });
    return out;
}

int main() {
    Engine e(42);

    // 1. every key x scale x bars: valid, in scale on strong beats, chord tones on strong beats mostly, ends home
    {
        int strong = 0, fit = 0, total = 0, inScale = 0, endsHome = 0, runs = 0;
        for (int key = 0; key < 12; ++key)
            for (int sc = 0; sc < (int) ccp::ScaleType::count; ++sc)
                for (int bars : { 2, 4, 8 }) {
                    Settings s; s.key = key; s.scale = sc; s.bars = bars; s.style = (Style) (1 + (key + sc) % ((int) Style::count - 1));
                    auto p = e.generate(s);
                    CHECK(p.key == key && (int) p.scale == sc && p.bars() == bars, "context not respected key %d scale %d bars %d", key, sc, bars);
                    CHECK(valid(p, p.melody, "key/scale"), "key %d scale %d bars %d", key, sc, bars);
                    const auto scale = ccp::HarmonyEngine::scaleNotes(key, (ccp::ScaleType) sc);
                    for (auto& n : p.melody) {
                        ++total;
                        inScale += std::find(scale.begin(), scale.end(), wrap12(n.pitch)) != scale.end() || std::find(scale.begin(), scale.end(), wrap12(n.pitch) + 12) != scale.end();
                        if (n.start % (2 * ppq) == 0) {
                            ++strong;
                            const auto pcs = Engine::chordPitchClasses(p.chordAt(n.start));
                            fit += std::find(pcs.begin(), pcs.end(), wrap12(n.pitch)) != pcs.end();
                        }
                    }
                    endsHome += wrap12(p.melody.back().pitch) == key;
                    ++runs;
                }
        const double scaleShare = (double) inScale / total, fitShare = (double) fit / std::max(1, strong), homeShare = (double) endsHome / runs;
        std::printf("in scale %.1f%%, chord tones on strong beats %.1f%%, ends on tonic %.1f%%\n", 100 * scaleShare, 100 * fitShare, 100 * homeShare);
        CHECK(scaleShare > .97, "too many chromatic notes");
        CHECK(fitShare > .75, "strong beats do not follow the chords");
        CHECK(homeShare > .6, "phrases rarely resolve");
    }

    // 2. density changes the amount of activity (LOW < MID < HIGH), not just doubles notes
    {
        double n[3] {};
        for (int d = 0; d < 3; ++d) {
            std::vector<double> v;
            for (int i = 0; i < 40; ++i) { Settings s; s.density = (Density) d; s.bars = 4; s.key = 0; s.scale = 1; auto p = e.generate(s); v.push_back((double) p.melody.size() / p.bars()); }
            n[d] = avg(v);
        }
        std::printf("notes per bar  LOW %.1f  MID %.1f  HIGH %.1f\n", n[0], n[1], n[2]);
        CHECK(n[0] < n[1] && n[1] < n[2], "density not ordered");
    }

    // 3. range changes the register from the start (LOW < MID < HIGH)
    {
        double c[3] {};
        for (int g = 0; g < 3; ++g) {
            std::vector<double> v;
            for (int i = 0; i < 30; ++i) { Settings s; s.range = (Range) g; s.key = 2; s.scale = 1; auto p = e.generate(s); double a = 0; for (auto& n : p.melody) a += n.pitch; v.push_back(a / p.melody.size()); }
            c[g] = avg(v);
            const auto r = composer::registerFor([&] { Settings s; s.range = (Range) g; return s; }());
            CHECK(c[g] > r.lo - 2 && c[g] < r.hi + 2, "range %d outside its register", g);
        }
        std::printf("average pitch  LOW %.1f  MID %.1f  HIGH %.1f\n", c[0], c[1], c[2]);
        CHECK(c[0] + 4 < c[1] && c[1] + 4 < c[2], "range not ordered");
    }

    // 4. feel: straight on the 16th grid, triplet on the triplet grid, swing moves off-beats only
    {
        int straightOff = 0, tripletOn = 0, tripletAll = 0, swungOff = 0, swungAll = 0;
        for (int i = 0; i < 30; ++i) {
            Settings s; s.key = 5; s.scale = 1; s.style = Style::pop;
            s.feel = Feel::straight; auto a = e.generate(s);
            for (auto& n : a.melody) straightOff += n.start % (ppq / 4) != 0;
            s.feel = Feel::triplet; auto t = e.generate(s);
            for (auto& n : t.melody) { ++tripletAll; tripletOn += n.start % (ppq / 3) == 0 && n.start % (ppq / 4) != 0; }
            s.feel = Feel::swing; auto w = e.generate(s);
            for (auto& n : w.melody) { ++swungAll; swungOff += n.start % (ppq / 4) != 0; CHECK(n.start % ppq != ppq / 2, "unswung 8th left in swing"); }
            CHECK(valid(t, t.melody, "triplet") && valid(w, w.melody, "swing"), "feel melodies invalid");
        }
        std::printf("triplet onsets %d of %d, swung onsets %d of %d\n", tripletOn, tripletAll, swungOff, swungAll);
        CHECK(straightOff == 0, "straight feel off the grid");
        CHECK(tripletOn * 6 > tripletAll, "triplet feel has too few triplets");
        CHECK(swungOff > 0, "swing feel changed nothing");
    }

    // 5. roles: hook repeats its rhythm more, counter sits lower than the lead and answers it
    {
        auto rhythmRepeat = [](const Pattern& p) {
            std::vector<std::vector<int>> bars((size_t) p.bars());
            for (auto& n : p.melody) bars[(size_t) (n.start / barTicks)].push_back(n.start % barTicks);
            int same = 0; for (int b = 1; b < p.bars(); ++b) for (int a = 0; a < b; ++a) if (bars[(size_t) a] == bars[(size_t) b]) { ++same; break; }
            return (double) same / (p.bars() - 1);
        };
        std::vector<double> hook, lead, leadPitch, counterPitch;
        for (int i = 0; i < 30; ++i) {
            Settings s; s.key = 9; s.scale = 1; s.bars = 8; s.style = Style::trap;
            s.role = Role::lead; auto l = e.generate(s); lead.push_back(rhythmRepeat(l));
            double a = 0; for (auto& n : l.melody) a += n.pitch; leadPitch.push_back(a / l.melody.size());
            s.role = Role::hook; auto h = e.generate(s); hook.push_back(rhythmRepeat(h));
            s.role = Role::counter; auto c = e.newMelody(l, s);
            double b = 0; for (auto& n : c.melody) b += n.pitch; counterPitch.push_back(b / c.melody.size());
            CHECK(valid(c, c.melody, "counter"), "counter invalid");
        }
        std::printf("rhythm repetition  hook %.2f  lead %.2f; average pitch lead %.1f counter %.1f\n", avg(hook), avg(lead), avg(leadPitch), avg(counterPitch));
        CHECK(avg(hook) > avg(lead), "hook not more repetitive than lead");
        CHECK(avg(counterPitch) + 3 < avg(leadPitch), "counter not under the lead");
    }

    // 6. seeds: same context + seed = same melody; different seeds differ
    {
        Settings s; s.key = 3; s.scale = 1; auto p = e.generate(s);
        const auto a = composer::compose(p, 1234), b = composer::compose(p, 1234), c = composer::compose(p, 999);
        CHECK(composer::similarity(a, b) == 1.0, "seed not reproducible");
        CHECK(composer::similarity(a, c) < .8, "different seeds too similar");
    }

    // 7. new melody explores; variation stays related but changes
    {
        std::vector<double> nm, va;
        for (int i = 0; i < 30; ++i) {
            Settings s; s.key = i % 12; s.scale = 1; s.bars = 4; auto p = e.generate(s);
            auto n = e.newMelody(p, s); nm.push_back(composer::similarity(p.melody, n.melody));
            auto v = e.variation(p); va.push_back(composer::similarity(p.melody, v.melody));
            CHECK(valid(v, v.melody, "variation"), "variation invalid");
            CHECK(n.progression.chords[0].name == p.progression.chords[0].name, "new melody changed the chords");
        }
        std::printf("similarity to the original  new melody %.2f  variation %.2f\n", avg(nm), avg(va));
        CHECK(avg(nm) < .35, "new melody too close to the old one");
        CHECK(avg(va) > .45 && avg(va) < .95, "variation not a relative");
        // variation works on a user-edited melody too
        Settings s; auto p = e.generate(s); p.melody.push_back({ p.lengthTicks() - ppq, ppq / 2, 72, 90 });
        auto v = e.variation(p); CHECK(valid(v, v.melody, "edited variation"), "variation of an edited melody invalid");
    }

    // 8. not literal: an 8-bar phrase is not one bar copied eight times, and phrases have rests
    {
        int literal = 0, noRest = 0;
        for (int i = 0; i < 40; ++i) {
            Settings s; s.bars = 8; auto p = e.generate(s);
            std::vector<std::vector<int>> bars(8);
            for (auto& n : p.melody) bars[(size_t) (n.start / barTicks)].push_back(n.pitch * 10000 + n.start % barTicks);
            bool all = true; for (int b = 1; b < 8; ++b) all &= bars[(size_t) b] == bars[0];
            literal += all;
            int sounding = 0; for (auto& n : p.melody) sounding += n.length;
            noRest += sounding >= p.lengthTicks() - step;
        }
        CHECK(literal == 0, "%d melodies repeat one bar literally", literal);
        CHECK(noRest < 8, "%d melodies never breathe", noRest);
    }

    // 9. no progression: the composer still writes from key and scale
    {
        Pattern p; p.key = 7; p.scale = ccp::ScaleType::major; p.settings.bars = 4; p.progression.chordCount = 0;
        const auto m = composer::compose(p, 77);
        bool ok = m.size() >= 2;
        for (auto& n : m) ok &= n.start < 4 * barTicks && n.length > 0;
        CHECK(ok, "no-chord context failed");
    }

    // 10. MIDI round trip keeps pitches, positions, lengths, velocities, tempo and length
    {
        Settings s; s.bars = 8; auto p = e.generate(s);
        const auto bytes = midiBytes(p.melody, 97, 0, p.lengthTicks());
        int tempo = 0, last = 0;
        const auto back = readMidi(bytes, tempo, last);
        bool same = back.size() == p.melody.size();
        for (size_t i = 0; same && i < back.size(); ++i)
            same = back[i].start == p.melody[i].start && back[i].length == p.melody[i].length && back[i].pitch == p.melody[i].pitch && back[i].velocity == p.melody[i].velocity;
        CHECK(same, "MIDI round trip changed the notes");
        CHECK(tempo == 60000000 / 97, "MIDI tempo wrong");
        CHECK(last >= p.lengthTicks(), "MIDI shorter than the pattern");
    }

    // 11. every style x vibe x role x density works, and generation is fast
    {
        const auto t0 = std::chrono::steady_clock::now();
        int n = 0;
        for (int st = 0; st < (int) Style::count; ++st)
            for (int v = 0; v < (int) Vibe::count; ++v)
                for (int r = 0; r < 3; ++r) {
                    Settings s; s.style = (Style) st; s.vibe = (Vibe) v; s.role = (Role) r; s.density = (Density) ((st + v) % 3); s.bars = 4;
                    auto p = e.generate(s); ++n;
                    CHECK(valid(p, p.melody, "style/vibe/role"), "style %d vibe %d role %d", st, v, r);
                }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / n;
        std::printf("generation %.1f ms per pattern\n", ms);
        CHECK(ms < 60, "generation too slow");
    }

    // 12. vibes shape the phrase: dreamy and space are sparser and longer than clean
    {
        double notes[3] {}, len[3] {};
        const Vibe vs[] { Vibe::clean, Vibe::dreamy, Vibe::space };
        for (int k = 0; k < 3; ++k) {
            std::vector<double> a, b;
            for (int i = 0; i < 30; ++i) { Settings s; s.vibe = vs[k]; s.key = 0; s.scale = 0; auto p = e.generate(s); a.push_back((double) p.melody.size()); double l = 0; for (auto& n : p.melody) l += n.length; b.push_back(l / p.melody.size()); }
            notes[k] = avg(a); len[k] = avg(b);
        }
        std::printf("vibe notes/len  clean %.1f/%.0f  dreamy %.1f/%.0f  space %.1f/%.0f\n", notes[0], len[0], notes[1], len[1], notes[2], len[2]);
        CHECK(notes[1] < notes[0] && notes[2] < notes[0], "vibes do not change the phrase");
    }

    // print one example
    Settings s; s.bars = 4; s.key = 9; s.scale = 1; s.style = Style::pop; auto p = e.generate(s);
    std::printf("example:"); for (auto& n : p.melody) std::printf(" %d:%d", n.start / step, n.pitch);
    std::printf("\nchords:"); for (int i = 0; i < p.bars(); ++i) std::printf(" %s", p.progression.chords[(size_t) i].name.c_str());
    std::printf("\n%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
