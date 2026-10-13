#include "Core/MelodyEngine.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace mc;
int main() {
    Engine e(42);
    int failures = 0;
    for (int st = 0; st < (int) Style::count; ++st)
        for (int v = 0; v < (int) Vibe::count; ++v)
            for (int bars : {2, 4, 8}) {
                Settings s; s.style = (Style) st; s.vibe = (Vibe) v; s.bars = bars; s.key = (st * 5 + v) % 12;
                auto p = e.generate(s);
                bool ok = p.bars() == bars && !p.bass.empty() && p.melody.size() >= 3;
                for (auto& n : p.melody) ok &= n.start >= 0 && n.start < p.lengthTicks() && n.length > 0 && n.pitch >= 40 && n.pitch <= 96;
                for (auto& n : p.bass) ok &= n.pitch >= 24 && n.pitch <= 52 && n.start < p.lengthTicks();
                // the lead's last note (highest note of the last onset) is the tonic
                int lastStart = 0, lastTop = 0;
                for (auto& n : p.melody) lastStart = std::max(lastStart, n.start);
                for (auto& n : p.melody) if (n.start == lastStart) lastTop = std::max(lastTop, n.pitch);
                ok &= (lastTop % 12) == s.key;
                ok &= p.style != Style::automatic && p.vibe != Vibe::automatic;
                if (st != 0) ok &= p.style == (Style) st;
                if (!ok) { ++failures; std::printf("fail style %d vibe %d bars %d\n", st, v, bars); }
            }
    Settings s; auto p = e.generate(s);
    std::set<uint64_t> seen;
    for (int i = 0; i < 20; ++i) { auto q = e.newMelody(p, s); uint64_t h = 0; for (auto& n : q.melody) h = h * 31 + n.pitch * 7 + n.start; seen.insert(h); }
    if (seen.size() < 18) { ++failures; std::printf("repetition: %zu unique of 20\n", seen.size()); }
    std::array<bool, maxBars> lk {}; lk[1] = true;
    auto q = e.generate(s, &p, lk);
    for (int t = 0; t < 2; ++t) {
        auto& a = t ? q.bass : q.melody; auto& b = t ? p.bass : p.melody; std::vector<std::pair<int,int>> x, y;
        for (auto& n : a) if (n.start / 3840 == 1) x.push_back({n.start, n.pitch});
        for (auto& n : b) if (n.start / 3840 == 1) y.push_back({n.start, n.pitch});
        if (x != y) { ++failures; std::puts("lock not kept"); }
    }
    if (q.progression.chords[1].name != p.progression.chords[1].name) { ++failures; std::puts("locked chord changed"); }
    auto c = e.setChord(p, 2, ccp::HarmonyEngine::buildChord(ccp::GenerationSettings {}, 3, ccp::ChordKind::triad));
    if (c.progression.chords[2].degree != 3) { ++failures; std::puts("setChord"); }
    auto bytes = midiBytes(p.melody, 120, 0);
    if (bytes.size() < 30 || bytes[0] != 'M') { ++failures; std::puts("midi"); }
    // overlapping notes per style (second voices, legato tails, pedals)
    for (int st = 1; st < (int) Style::count; ++st) {
        int overlaps = 0, total = 0;
        for (int i = 0; i < 20; ++i) {
            Settings x; x.style = (Style) st; auto r = e.generate(x);
            for (size_t a = 0; a + 1 < r.melody.size(); ++a) { ++total; if (r.melody[a].start + r.melody[a].length > r.melody[a + 1].start) ++overlaps; }
        }
        std::printf("%-10s overlap %4.1f%%\n", styleName((Style) st), 100.0 * overlaps / std::max(1, total));
    }
    // print one example
    for (auto& n : p.melody) std::printf("%d:%d ", n.start / 240, n.pitch);
    std::printf("\nchords:"); for (int i = 0; i < p.bars(); ++i) std::printf(" %s", p.progression.chords[(size_t) i].name.c_str());
    std::printf("\nsub:"); for (auto& n : p.bass) std::printf(" %d:%d", n.start / 240, n.pitch);
    std::printf("\n%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
