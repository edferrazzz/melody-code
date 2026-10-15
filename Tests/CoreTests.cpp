#include "Core/MelodyEngine.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace mc;
int main() {
    Engine e(42);
    int failures = 0;
    for (int st = 0; st < (int) Style::count; ++st)
        for (int v = 0; v < (int) Feel::count; ++v)
            for (int bars : {2, 4, 8}) {
                Settings s; s.style = (Style) st; s.feel = (Feel) v; s.bars = bars; s.key = (st + bars) % 3 == 0 ? autoKey : (st * 5 + v) % 12; if (bars == 8) s.scale = autoScale;
                auto p = e.generate(s);
                bool ok = p.bars() == bars && !p.bass.empty() && p.melody.size() >= 3;
                for (auto& n : p.melody) ok &= n.start >= 0 && n.start < p.lengthTicks() && n.length > 0 && n.pitch >= 40 && n.pitch <= 96;
                for (auto& n : p.bass) ok &= n.pitch >= 36 && n.pitch <= 55 && n.start < p.lengthTicks();
                // the lead's last note (highest note of the last onset) is the tonic
                int lastStart = 0, lastTop = 0;
                for (auto& n : p.melody) lastStart = std::max(lastStart, n.start);
                for (auto& n : p.melody) if (n.start == lastStart) lastTop = std::max(lastTop, n.pitch);
                ok &= (lastTop % 12) == p.key;
                if (s.key >= 0) ok &= p.key == s.key;
                ok &= p.style != Style::automatic && p.key >= 0 && p.key < 12;
                for (int b = 0; b < bars; ++b) {   // simple chords only
                    const auto k = p.progression.chords[(size_t) b].kind;
                    ok &= k == ccp::ChordKind::triad || k == ccp::ChordKind::seventh;
                }
                if (st != 0) ok &= p.style == (Style) st;
                if (!ok) { ++failures; std::printf("fail style %d feel %d bars %d key %d/%d last %d n %zu bass", st, v, bars, s.key, p.key, lastTop, p.melody.size());
                  for (auto& n : p.bass) std::printf(" %d", n.pitch); for (int b = 0; b < bars; ++b) std::printf(" k%d", (int) p.progression.chords[(size_t) b].kind); std::puts(""); }
            }
    Settings s; auto p = e.generate(s);
    std::set<uint64_t> seen;
    for (int i = 0; i < 20; ++i) { auto q = e.newMelody(p, s); uint64_t h = 0; for (auto& n : q.melody) h = h * 31 + n.pitch * 7 + n.start; seen.insert(h); }
    if (seen.size() < 18) { ++failures; std::printf("repetition: %zu unique of 20\n", seen.size()); }
    // triplet feel puts notes on the triplet grid (multiples of a triplet 8th = ppq/3)
    { Settings t; t.feel = Feel::triplet; int on = 0, all = 0;
      for (int i = 0; i < 20; ++i) { auto r = e.generate(t); for (auto& n : r.melody) { ++all; if (n.start % (ppq / 3) == 0 && n.start % step != 0) ++on; } }
      std::printf("triplet onsets off the 16th grid: %d of %d\n", on, all); if (on * 5 < all) { ++failures; std::puts("triplet"); } }
    // bass stays low but in the same instrument range (no sub below E1)
    { int lo = 127; for (int i = 0; i < 20; ++i) { auto r = e.generate(s); for (auto& n : r.bass) lo = std::min(lo, n.pitch); } if (lo < 36) { ++failures; std::puts("bass too low"); } }
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
    // the vibe never changes the notes
    { Settings v = s; v.vibe = Vibe::dark; auto q = e.newMelody(p, v); if (q.bass.size() != p.bass.size()) { ++failures; std::puts("vibe changed bass"); } }
    // harmonic rhythm varies
    { int shifted = 0; for (int i = 0; i < 40; ++i) { auto r = e.generate(s); for (int b = 1; b < r.bars(); ++b) if (r.chordShift[(size_t) b] || r.progression.chords[(size_t) b].name == r.progression.chords[(size_t) b - 1].name) { ++shifted; break; } }
      std::printf("patterns with varied chord lengths: %d of 40\n", shifted); if (shifted < 15) ++failures; }
    // print one example
    for (auto& n : p.melody) std::printf("%d:%d ", n.start / 240, n.pitch);
    std::printf("\nchords:"); for (int i = 0; i < p.bars(); ++i) std::printf(" %s@%d", p.progression.chords[(size_t) i].name.c_str(), p.chordShift[(size_t) i]);
    std::printf("\nbass:"); for (auto& n : p.bass) std::printf(" %d:%d", n.start / 240, n.pitch);
    std::printf("\n%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
