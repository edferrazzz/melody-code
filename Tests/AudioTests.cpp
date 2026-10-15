#include "PluginProcessor.h"
#include <cmath>
#include <cstdio>
// Renders a few loops of every style and preset offline and checks the output is audible and finite.
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
    std::printf("%s\n", failures ? "FAILED" : "OK");
    return failures;
}
