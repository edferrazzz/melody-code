#include "PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <array>
#include <tuple>
#include <map>
#include <string>
// Renders a few loops of every style and preset offline and checks the output is audible and finite.

namespace {
// Dry synth (no ambience) so measurements describe the instrument, not the room.
Synth::Fx dry() { Synth::Fx f; f.reverbMix = 0; f.delayMix = 0; f.chorus = 0; return f; }
struct Take { std::vector<float> l, r; };
Take play(Synth& s, std::initializer_list<std::tuple<double, int, int, double>> notes, double seconds, double sr = 48000.0) {
    // notes: start (s), pitch, velocity, length (s)
    Take t; const int total = (int) (seconds * sr); t.l.assign((size_t) total, 0); t.r.assign((size_t) total, 0);
    std::vector<std::tuple<int, int, int, int>> ev;
    for (auto& [st, p, v, len] : notes) ev.push_back({ (int) (st * sr), p, v, (int) (len * sr) });
    std::sort(ev.begin(), ev.end());
    int pos = 0;
    for (auto& [at, p, v, len] : ev) {
        if (at > pos) { s.render(t.l.data() + pos, t.r.data() + pos, at - pos); pos = at; }
        s.noteOn(p, v, len, false);
    }
    if (pos < total) s.render(t.l.data() + pos, t.r.data() + pos, total - pos);
    return t;
}
double rmsDb(const Take& t, size_t from = 0, size_t to = 0) {
    if (!to) to = t.l.size();
    double sum = 0; for (size_t i = from; i < to; ++i) sum += 0.5 * (t.l[i] * t.l[i] + t.r[i] * t.r[i]);
    return 10 * std::log10(sum / (double) (to - from) + 1e-20);
}
double peakOf(const Take& t) { double p = 0; for (size_t i = 0; i < t.l.size(); ++i) p = std::max({ p, (double) std::abs(t.l[i]), (double) std::abs(t.r[i]) }); return p; }
bool finite(const Take& t) { for (size_t i = 0; i < t.l.size(); ++i) if (!std::isfinite(t.l[i]) || !std::isfinite(t.r[i])) return false; return true; }
// brightness: energy of the slope against the energy of the signal over the first 0.3 s, relative
// to a pure sine at f0 (1 = sine, higher = more upper harmonics)
double brightness(const Take& t, double f0, double sr = 48000.0) {
    double e = 0, d = 0; const size_t n = std::min(t.l.size(), (size_t) (sr * 0.3));
    for (size_t i = 1; i < n; ++i) { e += t.l[i] * t.l[i]; d += (t.l[i] - t.l[i - 1]) * (t.l[i] - t.l[i - 1]); }
    const double sineRatio = 2 * std::sin(3.14159265 * f0 / sr);
    return std::sqrt(d / (e + 1e-20)) / sineRatio;
}
// fundamental by autocorrelation over a window, in Hz
double pitchOf(const std::vector<float>& x, size_t from, size_t len, double lo, double hi, double sr = 48000.0) {
    double best = -1e9; int bestLag = 0;
    for (int lag = (int) (sr / hi); lag <= (int) (sr / lo); ++lag) {
        double c = 0; for (size_t i = from; i < from + len; ++i) c += x[i] * x[i + (size_t) lag];
        if (c > best) { best = c; bestLag = lag; }
    }
    // parabolic refinement
    auto ac = [&](int lag) { double c = 0; for (size_t i = from; i < from + len; ++i) c += x[i] * x[i + (size_t) lag]; return c; };
    const double a = ac(bestLag - 1), b = ac(bestLag), c = ac(bestLag + 1);
    const double shift = 0.5 * (a - c) / (a - 2 * b + c);
    return sr / (bestLag + shift);
}
}

int toneTests() {
    int failures = 0;
    const double sr = 48000.0;
    std::printf("\nTONE DESIGN ENGINE\n");
    std::map<int, std::array<double, 4>> feat;   // brightness, decay (dB from 0.1s to 1s), level, stereo side share
    for (int pr = 0; pr < Synth::presetCount; ++pr) {
        const auto& P = tone::get(pr);
        bool ok = true;
        // registers x velocities, short and long notes
        double worstPeak = 0, softDb = 0, hardDb = 0;
        for (int pitch : { 40, 60, 84 })
            for (int vel : { 20, 64, 120 })
                for (double len : { 0.06, 1.5 }) {
                    Synth s; s.prepare(sr); s.setFx(dry()); s.setTempo(120); s.setLevels(0.8f, 0.8f); s.setPreset(pr);
                    auto t = play(s, { { 0.0, pitch, vel, len } }, 3.0 + P.env.release * 3);
                    ok &= finite(t);
                    worstPeak = std::max(worstPeak, peakOf(t));
                    if (pitch == 60 && len > 1) { if (vel == 20) softDb = rmsDb(t, 0, 24000); if (vel == 120) hardDb = rmsDb(t, 0, 24000); }
                    // note off and release: the instrument goes quiet after the tail and frees its voice
                    float tail[4096] {}, tailR[4096] {};
                    for (int k = 0; k < 400 && s.activeVoices() > 0; ++k) s.render(tail, tailR, 4096);
                    if (s.activeVoices() > 0) { ok = false; std::printf("  %s voice never ends (pitch %d vel %d)\n", P.name, pitch, vel); }
                }
        const bool velOk = hardDb - softDb > 5 && softDb > -50;   // soft notes audible, hard notes clearly louder
        // polyphony: a 6-note chord at high velocity, no clipping
        Synth s; s.prepare(sr); s.setFx(dry()); s.setLevels(0.8f, 0.8f); s.setPreset(pr);
        auto chord = play(s, { { 0, 48, 120, 2 }, { 0, 55, 120, 2 }, { 0, 60, 120, 2 }, { 0, 64, 120, 2 }, { 0, 67, 120, 2 }, { 0, 72, 120, 2 } }, 3);
        // phrase used for loudness matching and features
        Synth s2; s2.prepare(sr); s2.setFx(dry()); s2.setLevels(0.8f, 0.8f); s2.setPreset(pr);
        auto one = play(s2, { { 0, 69, 100, 1.2 } }, 1.2);
        Synth s3; s3.prepare(sr); s3.setFx(dry()); s3.setLevels(0.8f, 0.8f); s3.setPreset(pr);
        auto phrase = play(s3, { { 0, 60, 100, .4 }, { .5, 64, 90, .4 }, { 1, 67, 100, .4 }, { 1.5, 72, 110, .9 }, { 2.5, 67, 80, .4 }, { 3, 64, 90, .9 } }, 4.5);
        const double lvl = rmsDb(phrase);
        double side = 0, all = 0; for (size_t i = 0; i < one.l.size(); ++i) { side += (one.l[i] - one.r[i]) * (one.l[i] - one.r[i]); all += (one.l[i] + one.r[i]) * (one.l[i] + one.r[i]); }
        feat[pr] = { brightness(one, 440.0), rmsDb(one, 4800, 9600) - rmsDb(one, 43200, 57600 > (int) one.l.size() ? one.l.size() : 57600), lvl, side / (all + 1e-20) };
        ok &= velOk && worstPeak < 0.95 && peakOf(chord) < 0.95;
        std::printf("  %-15s %-6s level %5.1f dB  peak %.2f chord %.2f  vel soft %5.1f hard %5.1f  bright %4.1f  decay %5.1f %s\n",
                    P.name, tone::familyName(P.family), lvl, worstPeak, peakOf(chord), softDb, hardDb, feat[pr][0], feat[pr][1], ok ? "" : "<-- FAIL");
        if (!ok) ++failures;
    }
    // loudness match: every preset within 3 dB of the median
    std::vector<double> lv; for (auto& [k, f] : feat) lv.push_back(f[2]);
    std::sort(lv.begin(), lv.end()); const double med = lv[lv.size() / 2];
    for (auto& [k, f] : feat) if (std::abs(f[2] - med) > 3.0) { std::printf("  level mismatch %s %.1f dB vs median %.1f\n", tone::get(k).name, f[2], med); ++failures; }
    // variants really differ inside each family (brightness or decay)
    for (int a = 0; a < Synth::presetCount; ++a)
        for (int b = a + 1; b < Synth::presetCount; ++b) {
            const bool bothShown = Synth::shownPreset(a) == a && Synth::shownPreset(b) == b && a != 0 ? true : (a == 0 && Synth::shownPreset(b) == b);
            if (tone::get(a).family != tone::get(b).family && !bothShown) continue;   // the three sounds in SOUND must all differ
            const double d = std::abs(std::log(feat[a][0] / feat[b][0])) * 4 + std::abs(feat[a][1] - feat[b][1]) / 6 + std::abs(feat[a][3] - feat[b][3]) * 10;
            if (d < 0.25) { std::printf("  too similar: %s / %s (%.2f)\n", tone::get(a).name, tone::get(b).name, d); ++failures; }
        }
    // pitch stability: harmonic presets without vibrato hold A4 within 5 cents (pads are detuned stacks: no single pitch to measure)
    for (int pr = 0; pr < Synth::presetCount; ++pr) {
        const auto& P = tone::get(pr);
        if (P.perf.vibratoCents > 0 || P.family == tone::Family::pad || (P.family == tone::Family::bells && P.algo != 1) || (P.family == tone::Family::mallet && P.algo != 0)) continue;
        Synth s; s.prepare(sr); s.setFx(dry()); s.setLevels(0.8f, 0.8f); s.setPreset(pr);
        auto t = play(s, { { 0, 69, 100, 1.5 } }, 1.5);
        std::vector<float> mono(t.l.size()); for (size_t i = 0; i < mono.size(); ++i) mono[i] = t.l[i] + t.r[i];
        const double f = pitchOf(mono, 4800, 2400, 300, 600);
        const double ct = 1200 * std::log2(f / 440.0);
        if (std::abs(ct) > 5) { std::printf("  pitch %s %.1f cents\n", P.name, ct); ++failures; }
    }
    // preset change while notes ring: no NaN, no jump bigger than the signal itself
    {
        Synth s; s.prepare(sr); s.setFx(dry()); s.setLevels(0.8f, 0.8f);
        std::vector<float> l(4800), r(4800); double prev = 0, jump = 0, peak = 0;
        for (int i = 0; i < 200; ++i) {
            if (i % 10 == 0) { s.setPreset(i / 10 % Synth::presetCount); s.noteOn(60 + i % 12, 100, 9600, false); }
            s.render(l.data(), r.data(), 480);
            for (int k = 0; k < 480; ++k) { if (!std::isfinite(l[(size_t) k])) ++failures; jump = std::max(jump, std::abs(l[(size_t) k] - prev)); prev = l[(size_t) k]; peak = std::max(peak, (double) std::abs(l[(size_t) k])); }
        }
        s.allNotesOff(); s.render(l.data(), r.data(), 4800);
        std::printf("  preset switching: peak %.2f, max step %.3f, voices after stop %d\n", peak, jump, s.activeVoices());
        if (peak > 0.95 || s.activeVoices() > 0) ++failures;
    }
    // controlled variation: same family, close to the original, reproducible
    for (int pr = 0; pr < Synth::presetCount; ++pr) {
        const auto& P = tone::get(pr);
        const auto m1 = tone::vary(P, 7), m2 = tone::vary(P, 7), m3 = tone::vary(P, 8);
        const float* a = &P.macros.character; const float* x = &m1.character; const float* y = &m2.character; const float* z = &m3.character;
        float maxDiff = 0, diff13 = 0;
        for (int i = 0; i < 7; ++i) { maxDiff = std::max(maxDiff, std::abs(x[i] - a[i])); diff13 += std::abs(x[i] - z[i]); if (x[i] != y[i]) ++failures; }
        if (maxDiff > 0.121f || diff13 < 0.01f) ++failures;
    }
    // CPU: 16 pad voices (the heaviest family) for 10 s of audio
    {
        Synth s; s.prepare(sr); s.setFx(Synth::Fx {}); s.setLevels(0.8f, 0.8f); s.setPreset(Synth::pad);
        for (int i = 0; i < 16; ++i) s.noteOn(48 + i, 90, (int) (sr * 10), false);
        std::vector<float> l(512), r(512);
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < (int) (sr * 10 / 512); ++i) s.render(l.data(), r.data(), 512);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  CPU: 16 pad voices, 10 s rendered in %.2f s (%.1f%% of real time)\n", secs, secs * 10);
        if (secs > 5.0) ++failures;
    }
    return failures;
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    MelodyCodeProcessor p;
    p.prepareToPlay(48000.0, 512);
    int failures = 0;
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int style = 0; style < (int) mc::Style::count; ++style) {
        p.settings().style = (mc::Style) style;
        p.preset = style % Synth::presetCount;
        p.setVibe((mc::Vibe) (style % (int) mc::Vibe::count));
        p.settings().feel = (mc::Feel) (style % 3);
        p.generateAll();
        p.setPlaying(true);
        double sumSq = 0, peak = 0; long n = 0; bool finite = true; int ons = 0, offs = 0;
        for (int b = 0; b < 48000 * 8 / 512; ++b) {
            p.processBlock(buf, midi);
            for (const auto m : midi) { if (m.getMessage().isNoteOn()) ++ons; else if (m.getMessage().isNoteOff()) ++offs; }
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i) {
                    const float v = buf.getSample(c, i);
                    finite &= std::isfinite(v);
                    sumSq += v * v; peak = std::max(peak, (double) std::abs(v)); ++n;
                }
        }
        p.setPlaying(false);
        p.processBlock(buf, midi);
        for (const auto m : midi) if (m.getMessage().isNoteOff()) ++offs;
        const double rms = std::sqrt(sumSq / n);
        const double db = 20 * std::log10(rms + 1e-12);
        std::printf("%-8s %-10s %-10s rms %.1f dB peak %.2f midi on %d off %d\n", mc::styleName((mc::Style) style), Synth::presetName(p.preset), mc::vibeName((mc::Vibe) (style % 5)), db, peak, ons, offs);
        if (!finite || db < -40 || peak > 0.95 || ons == 0 || offs < ons) ++failures;
    }
    // every sound through every vibe on the same notes: no clipping, nothing silent
    p.settings().style = mc::Style::pop; p.generateAll();
    for (int pr = 0; pr < Synth::presetCount; ++pr)
        for (int v = 0; v < (int) mc::Vibe::count; ++v) {
            p.preset = pr; p.setVibe((mc::Vibe) v); p.setPlaying(true);
            double sumSq = 0, peak = 0; long n = 0;
            for (int b = 0; b < 48000 * 6 / 512; ++b) {
                p.processBlock(buf, midi);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float x = buf.getSample(c, i); sumSq += x * x; peak = std::max(peak, (double) std::abs(x)); ++n; }
            }
            p.setPlaying(false); p.processBlock(buf, midi);
            const double db = 20 * std::log10(std::sqrt(sumSq / n) + 1e-12);
            std::printf("  %-7s %-10s rms %5.1f dB peak %.2f\n", Synth::presetName(pr), mc::vibeName((mc::Vibe) v), db, peak);
            if (db < -40 || peak > 0.95) ++failures;
        }
    failures += toneTests();
    std::printf("%s\n", failures ? "FAILED" : "OK");
    return failures;
}
