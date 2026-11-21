#include "Sets.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstring>

namespace {
constexpr double twoPi = 6.283185307179586;
constexpr float pi = 3.14159265f;

enum class Osc { none, sine, tri, saw, square, supersaw, fm, additive, pluck };
enum class Filter { none, lp12, lp24, bp, hp };
struct Partial { float ratio, level, decay; };   // decay: seconds to -60 dB, 0 = holds
}

/* One patch = one playable instrument. Fields left out keep their defaults.
   center / lo / hi   where a line should sit (the whole line moves by octaves toward center), and the range
                      single notes fold into
   osc1 / osc2        sources; unison + detune (cents, spread across the unison voices) thicken saw, square
                      and supersaw; osc2 has its own interval and detune; sub = sine an octave down
   noise              a burst (or breath, with a long noiseDecay) through the filter
   fm                 phase modulation: carrier sine with two modulators (ratio r1 / r2, index i1 / i2) whose
                      depth decays to iSus over iDecay seconds, harder with velocity
   additive           a table of partials (ratio, level, own decay): organs, music box, bells
   pluck              a plucked string (Karplus-Strong), ksBright = pick brightness, ksDecay = ring time
   filter             cutoff (Hz at C4), keyTrack (1 = follows the note), fEnv octaves above cutoff that fall
                      to fSus over fDecay, velCut octaves for a hard hit
   envelope           attack, decay (to -60 dB toward sustain), sustain level, release (to -60 dB)
   pitchEnv           semitones that fall to 0 over pitchDecay (808 thump, chip blip)
   glideMs / mono     a held bass or acid line slides into the next note instead of retriggering */
struct Sets::Patch {
    const char* name;
    int center, lo, hi;
    Osc osc1 = Osc::saw; float level1 = 1.0f;
    Osc osc2 = Osc::none; float semi2 = 0.0f, detune2 = 0.0f, level2 = 0.0f;
    int unison = 1; float detune = 0.0f;
    float sub = 0.0f;
    float noise = 0.0f, noiseDecay = 0.03f, noiseTone = 6000.0f;
    float r1 = 1.0f, i1 = 0.0f, r2 = 0.0f, i2 = 0.0f, iDecay = 0.5f, iSus = 0.0f;
    const Partial* partials = nullptr; int partialCount = 0;
    float ksBright = 0.5f, ksDecay = 1.5f;
    Filter filter = Filter::none; float cutoff = 18000.0f, res = 0.1f, fEnv = 0.0f, fDecay = 0.2f, fSus = 0.0f, keyTrack = 0.6f, velCut = 0.5f;
    float attack = 0.002f, decay = 1.0f, sustain = 1.0f, release = 0.1f;
    float pitchEnv = 0.0f, pitchDecay = 0.03f;
    float drive = 0.0f;
    float glideMs = 0.0f; bool mono = false;
    float vibrato = 0.0f, vibRate = 5.0f, vibDelay = 0.3f;
    float tremolo = 0.0f, tremRate = 4.5f;
    float width = 0.3f;
    float level = 1.0f;
};

namespace {
using P = Sets::Patch;
// partial tables (ratio, level, decay)
const Partial musicBox[] { { 1.0f, 1.0f, 2.2f }, { 3.0f, 0.22f, 0.7f }, { 5.4f, 0.16f, 0.35f }, { 8.9f, 0.07f, 0.15f }, { 2.0f, 0.12f, 1.0f } };
const Partial drawbars[] { { 1.0f, 0.8f, 0 }, { 2.0f, 0.65f, 0 }, { 3.0f, 0.45f, 0 }, { 4.0f, 0.4f, 0 }, { 6.0f, 0.22f, 0 },
                           { 8.0f, 0.18f, 0 }, { 0.5f, 0.35f, 0 }, { 3.0f, 0.6f, 0.18f } };   // last: percussion click
const Partial marimbaBar[] { { 1.0f, 1.0f, 0.9f }, { 3.93f, 0.35f, 0.12f }, { 9.2f, 0.08f, 0.04f } };

// ------------------------------------------------------------------ the patches
const P patches[] {
    // melody
    { .name = "bell_glass", .center = 76, .lo = 62, .hi = 92, .osc1 = Osc::fm, .r1 = 3.5f, .i1 = 2.4f, .r2 = 1.0f, .i2 = 0.6f, .iDecay = 1.2f, .iSus = 0.1f, .attack = 0.001f, .decay = 3.2f, .sustain = 0.0f, .release = 0.9f, .width = 0.2f, .level = 0.9f },
    { .name = "flute_soft", .center = 74, .lo = 60, .hi = 88, .osc1 = Osc::tri, .osc2 = Osc::sine, .semi2 = 12.0f, .level2 = 0.12f, .noise = 0.05f, .noiseDecay = 4.0f, .noiseTone = 3500.0f, .filter = Filter::lp12, .cutoff = 2600.0f, .keyTrack = 1.0f, .attack = 0.035f, .decay = 0.6f, .sustain = 0.9f, .release = 0.14f, .vibrato = 14.0f, .vibRate = 5.2f, .vibDelay = 0.25f, .width = 0.1f, .level = 1.0f },
    { .name = "music_box", .center = 79, .lo = 67, .hi = 96, .osc1 = Osc::additive, .noise = 0.08f, .noiseDecay = 0.004f, .noiseTone = 9000.0f, .partials = musicBox, .partialCount = 5, .attack = 0.001f, .decay = 4.0f, .sustain = 0.0f, .release = 0.6f, .width = 0.15f, .level = 0.9f },
    { .name = "bleep", .center = 74, .lo = 60, .hi = 90, .osc1 = Osc::square, .filter = Filter::lp12, .cutoff = 3800.0f, .keyTrack = 0.5f, .attack = 0.001f, .decay = 0.45f, .sustain = 0.25f, .release = 0.07f, .pitchEnv = 12.0f, .pitchDecay = 0.008f, .width = 0.1f, .level = 0.55f },
    { .name = "air_pluck", .center = 72, .lo = 58, .hi = 86, .osc1 = Osc::supersaw, .unison = 5, .detune = 18.0f, .filter = Filter::lp24, .cutoff = 650.0f, .res = 0.15f, .fEnv = 4.0f, .fDecay = 0.18f, .fSus = 0.05f, .attack = 0.002f, .decay = 0.7f, .sustain = 0.12f, .release = 0.28f, .width = 0.8f, .level = 0.9f },
    { .name = "marimba", .center = 74, .lo = 58, .hi = 92, .osc1 = Osc::additive, .noise = 0.12f, .noiseDecay = 0.003f, .noiseTone = 2500.0f, .partials = marimbaBar, .partialCount = 3, .attack = 0.001f, .decay = 1.2f, .sustain = 0.0f, .release = 0.08f, .width = 0.1f, .level = 1.0f },
    { .name = "house_piano", .center = 72, .lo = 58, .hi = 88, .osc1 = Osc::fm, .osc2 = Osc::sine, .semi2 = 12.0f, .detune2 = 4.0f, .level2 = 0.25f, .r1 = 1.0f, .i1 = 1.5f, .r2 = 7.0f, .i2 = 0.5f, .iDecay = 0.3f, .iSus = 0.25f, .attack = 0.001f, .decay = 1.8f, .sustain = 0.08f, .release = 0.22f, .width = 0.25f, .level = 0.85f },
    { .name = "acid", .center = 60, .lo = 48, .hi = 72, .osc1 = Osc::saw, .filter = Filter::lp24, .cutoff = 280.0f, .res = 0.8f, .fEnv = 4.2f, .fDecay = 0.17f, .fSus = 0.05f, .keyTrack = 0.4f, .velCut = 1.2f, .attack = 0.002f, .decay = 0.5f, .sustain = 0.85f, .release = 0.05f, .drive = 0.5f, .glideMs = 55.0f, .mono = true, .width = 0.0f, .level = 0.75f },
    { .name = "saw_lead", .center = 72, .lo = 58, .hi = 86, .osc1 = Osc::supersaw, .unison = 7, .detune = 22.0f, .filter = Filter::lp12, .cutoff = 3200.0f, .fEnv = 1.4f, .fDecay = 0.3f, .fSus = 0.5f, .attack = 0.005f, .decay = 0.8f, .sustain = 0.85f, .release = 0.22f, .vibrato = 8.0f, .vibDelay = 0.35f, .width = 0.9f, .level = 0.6f },
    { .name = "dark_lead", .center = 67, .lo = 55, .hi = 82, .osc1 = Osc::saw, .osc2 = Osc::square, .semi2 = -12.0f, .detune2 = 5.0f, .level2 = 0.4f, .filter = Filter::lp24, .cutoff = 900.0f, .res = 0.45f, .fEnv = 2.6f, .fDecay = 0.24f, .fSus = 0.2f, .attack = 0.002f, .decay = 0.45f, .sustain = 0.45f, .release = 0.18f, .width = 0.4f, .level = 0.75f },
    { .name = "cowbell", .center = 68, .lo = 58, .hi = 82, .osc1 = Osc::square, .osc2 = Osc::square, .semi2 = 6.8f, .level2 = 0.8f, .filter = Filter::bp, .cutoff = 1100.0f, .res = 0.35f, .keyTrack = 0.9f, .velCut = 0.0f, .attack = 0.001f, .decay = 0.4f, .sustain = 0.0f, .release = 0.06f, .width = 0.0f, .level = 1.2f },
    { .name = "synth_brass", .center = 67, .lo = 53, .hi = 80, .osc1 = Osc::supersaw, .unison = 3, .detune = 9.0f, .filter = Filter::lp12, .cutoff = 700.0f, .fEnv = 2.4f, .fDecay = 0.35f, .fSus = 0.45f, .velCut = 0.8f, .attack = 0.03f, .decay = 0.8f, .sustain = 0.85f, .release = 0.16f, .vibrato = 6.0f, .vibDelay = 0.4f, .width = 0.5f, .level = 0.75f },
    // chords
    { .name = "fm_ep", .center = 60, .lo = 46, .hi = 76, .osc1 = Osc::fm, .r1 = 1.0f, .i1 = 1.6f, .r2 = 14.0f, .i2 = 0.5f, .iDecay = 0.5f, .iSus = 0.15f, .attack = 0.002f, .decay = 2.6f, .sustain = 0.0f, .release = 0.3f, .tremolo = 0.15f, .width = 0.3f, .level = 1.0f },
    { .name = "rhodes", .center = 60, .lo = 46, .hi = 76, .osc1 = Osc::fm, .osc2 = Osc::sine, .semi2 = 0.0f, .detune2 = 3.0f, .level2 = 0.3f, .r1 = 1.0f, .i1 = 1.1f, .r2 = 14.0f, .i2 = 0.3f, .iDecay = 0.7f, .iSus = 0.1f, .attack = 0.002f, .decay = 3.0f, .sustain = 0.0f, .release = 0.35f, .tremolo = 0.35f, .tremRate = 4.8f, .width = 0.4f, .level = 1.0f },
    { .name = "soft_pluck", .center = 62, .lo = 48, .hi = 78, .osc1 = Osc::pluck, .ksBright = 0.35f, .ksDecay = 1.8f, .filter = Filter::lp12, .cutoff = 2600.0f, .keyTrack = 0.5f, .velCut = 0.0f, .attack = 0.001f, .decay = 6.0f, .sustain = 1.0f, .release = 0.18f, .width = 0.3f, .level = 1.1f },
    { .name = "warm_pad", .center = 58, .lo = 46, .hi = 74, .osc1 = Osc::supersaw, .unison = 5, .detune = 14.0f, .filter = Filter::lp12, .cutoff = 1500.0f, .res = 0.15f, .keyTrack = 0.3f, .velCut = 0.0f, .attack = 0.35f, .decay = 1.0f, .sustain = 0.85f, .release = 0.8f, .width = 0.9f, .level = 0.7f },
    { .name = "saw_stab", .center = 62, .lo = 48, .hi = 76, .osc1 = Osc::supersaw, .unison = 3, .detune = 14.0f, .filter = Filter::lp24, .cutoff = 850.0f, .res = 0.2f, .fEnv = 2.6f, .fDecay = 0.12f, .fSus = 0.05f, .attack = 0.001f, .decay = 0.28f, .sustain = 0.05f, .release = 0.06f, .width = 0.6f, .level = 0.9f },
    { .name = "deep_stab", .center = 60, .lo = 48, .hi = 74, .osc1 = Osc::supersaw, .osc2 = Osc::square, .detune2 = -6.0f, .level2 = 0.35f, .unison = 2, .detune = 10.0f, .filter = Filter::lp24, .cutoff = 650.0f, .res = 0.3f, .fEnv = 2.2f, .fDecay = 0.2f, .fSus = 0.12f, .attack = 0.002f, .decay = 0.5f, .sustain = 0.12f, .release = 0.22f, .width = 0.6f, .level = 0.9f },
    { .name = "organ", .center = 60, .lo = 48, .hi = 76, .osc1 = Osc::additive, .noise = 0.05f, .noiseDecay = 0.004f, .noiseTone = 4000.0f, .partials = drawbars, .partialCount = 8, .filter = Filter::lp12, .cutoff = 5000.0f, .keyTrack = 0.3f, .velCut = 0.0f, .attack = 0.004f, .decay = 0.6f, .sustain = 0.8f, .release = 0.07f, .width = 0.3f, .level = 0.55f },
    { .name = "dark_pad", .center = 56, .lo = 44, .hi = 70, .osc1 = Osc::square, .osc2 = Osc::saw, .semi2 = 12.0f, .detune2 = 7.0f, .level2 = 0.3f, .unison = 3, .detune = 8.0f, .filter = Filter::lp12, .cutoff = 750.0f, .res = 0.2f, .keyTrack = 0.3f, .velCut = 0.0f, .attack = 0.5f, .decay = 1.0f, .sustain = 0.9f, .release = 1.0f, .width = 0.9f, .level = 0.6f },
    { .name = "string_pad", .center = 60, .lo = 46, .hi = 76, .osc1 = Osc::supersaw, .unison = 7, .detune = 12.0f, .filter = Filter::lp12, .cutoff = 2300.0f, .keyTrack = 0.4f, .velCut = 0.0f, .attack = 0.28f, .decay = 1.0f, .sustain = 0.9f, .release = 0.6f, .vibrato = 4.0f, .vibRate = 4.6f, .vibDelay = 0.5f, .width = 1.0f, .level = 0.6f },
    { .name = "glass_pad", .center = 60, .lo = 46, .hi = 76, .osc1 = Osc::fm, .osc2 = Osc::sine, .semi2 = 0.0f, .detune2 = 8.0f, .level2 = 0.7f, .r1 = 2.0f, .i1 = 1.0f, .iDecay = 1.5f, .iSus = 0.55f, .attack = 0.4f, .decay = 1.0f, .sustain = 0.9f, .release = 1.1f, .tremolo = 0.12f, .tremRate = 0.4f, .width = 0.8f, .level = 0.75f },
    { .name = "trance_chords", .center = 62, .lo = 48, .hi = 78, .osc1 = Osc::supersaw, .unison = 7, .detune = 25.0f, .filter = Filter::lp12, .cutoff = 4200.0f, .fEnv = 1.0f, .fDecay = 0.25f, .fSus = 0.5f, .attack = 0.004f, .decay = 0.5f, .sustain = 0.6f, .release = 0.25f, .width = 1.0f, .level = 0.6f },
    // bass
    { .name = "sub_round", .center = 36, .lo = 26, .hi = 48, .osc1 = Osc::sine, .osc2 = Osc::sine, .semi2 = 12.0f, .level2 = 0.18f, .attack = 0.004f, .decay = 1.0f, .sustain = 1.0f, .release = 0.05f, .drive = 0.15f, .mono = true, .width = 0.0f, .level = 1.0f },
    { .name = "sub_warm", .center = 38, .lo = 26, .hi = 50, .osc1 = Osc::sine, .osc2 = Osc::tri, .semi2 = 12.0f, .level2 = 0.3f, .attack = 0.004f, .decay = 0.9f, .sustain = 0.6f, .release = 0.06f, .drive = 0.3f, .mono = true, .width = 0.0f, .level = 1.0f },
    { .name = "bass808", .center = 33, .lo = 24, .hi = 45, .osc1 = Osc::sine, .osc2 = Osc::sine, .semi2 = 12.0f, .level2 = 0.08f, .attack = 0.001f, .decay = 2.2f, .sustain = 0.0f, .release = 0.08f, .pitchEnv = 7.0f, .pitchDecay = 0.025f, .drive = 0.35f, .glideMs = 70.0f, .mono = true, .width = 0.0f, .level = 1.0f },
    { .name = "bass808_heavy", .center = 33, .lo = 24, .hi = 45, .osc1 = Osc::sine, .osc2 = Osc::tri, .semi2 = 12.0f, .level2 = 0.12f, .attack = 0.001f, .decay = 2.4f, .sustain = 0.0f, .release = 0.08f, .pitchEnv = 10.0f, .pitchDecay = 0.03f, .drive = 0.85f, .glideMs = 80.0f, .mono = true, .width = 0.0f, .level = 0.85f },
    { .name = "deep_bass", .center = 36, .lo = 26, .hi = 48, .osc1 = Osc::sine, .osc2 = Osc::saw, .level2 = 0.35f, .filter = Filter::lp12, .cutoff = 260.0f, .fEnv = 1.3f, .fDecay = 0.08f, .keyTrack = 0.8f, .velCut = 0.0f, .attack = 0.003f, .decay = 0.4f, .sustain = 0.55f, .release = 0.06f, .drive = 0.2f, .mono = true, .width = 0.0f, .level = 1.0f },
    { .name = "reese", .center = 36, .lo = 28, .hi = 48, .osc1 = Osc::supersaw, .unison = 3, .detune = 18.0f, .sub = 0.7f, .filter = Filter::lp24, .cutoff = 260.0f, .res = 0.2f, .fEnv = 0.6f, .fDecay = 0.3f, .fSus = 0.4f, .keyTrack = 0.5f, .velCut = 0.0f, .attack = 0.004f, .decay = 1.0f, .sustain = 1.0f, .release = 0.06f, .drive = 0.3f, .glideMs = 40.0f, .mono = true, .width = 0.0f, .level = 0.8f },
    { .name = "rolling", .center = 36, .lo = 28, .hi = 48, .osc1 = Osc::saw, .sub = 0.8f, .filter = Filter::lp24, .cutoff = 200.0f, .res = 0.25f, .fEnv = 2.2f, .fDecay = 0.09f, .keyTrack = 0.6f, .velCut = 0.5f, .attack = 0.002f, .decay = 0.25f, .sustain = 0.2f, .release = 0.05f, .drive = 0.2f, .mono = true, .width = 0.0f, .level = 1.0f },
    { .name = "pluck_bass", .center = 36, .lo = 28, .hi = 48, .osc1 = Osc::square, .sub = 0.6f, .filter = Filter::lp24, .cutoff = 240.0f, .res = 0.2f, .fEnv = 2.4f, .fDecay = 0.1f, .fSus = 0.1f, .keyTrack = 0.6f, .attack = 0.002f, .decay = 0.35f, .sustain = 0.3f, .release = 0.05f, .drive = 0.2f, .mono = true, .width = 0.0f, .level = 0.9f },
};
constexpr int patchCount = (int) (sizeof(patches) / sizeof(patches[0]));

const P& patch(const char* name) {
    for (const auto& p : patches) if (std::strcmp(p.name, name) == 0) return p;
    jassertfalse;   // a set names a patch that does not exist
    return patches[0];
}

// Names and descriptions are UTF-8 (shown in Portuguese).
const Sets::Kit kits[Sets::kitCount] {
    { "CRISTAL",  "Sino de vidro FM na melodia, piano el\xc3\xa9trico nos acordes, sub redondo",          "bell_glass",  "fm_ep",         "sub_round" },
    { "VELUDO",   "Flauta suave na melodia, Rhodes nos acordes, sub quente",                                "flute_soft",  "rhodes",        "sub_warm" },
    { "NINAR",    "Caixinha de m\xc3\xbasica na melodia, pluck de corda nos acordes, sub quente",          "music_box",   "soft_pluck",    "sub_warm" },
    { "NUVEM",    "Pluck a\xc3\xa9reo na melodia, pad quente segurando os acordes, 808",                    "air_pluck",   "warm_pad",      "bass808", true },
    { "PIXEL",    "Bleep de videogame na melodia, stabs de serra nos acordes, 808",                         "bleep",       "saw_stab",      "bass808" },
    { "DEEP",     "Piano house na melodia, stab de deep house nos acordes, baixo redondo",                  "house_piano", "deep_stab",     "deep_bass" },
    { "CLUBE",    "Marimba na melodia, \xc3\xb3rg\xc3\xa3o de house nos acordes, baixo pluck",              "marimba",     "organ",         "pluck_bass" },
    { "NEON",     "Lead de supersaw na melodia, acordes de trance, sub redondo",                             "saw_lead",    "trance_chords", "sub_round" },
    { "\xc3\x81" "CIDO", "Linha \xc3\xa1" "cida na melodia, \xc3\xb3rg\xc3\xa3o nos acordes, baixo rolante", "acid",        "organ",         "rolling" },
    { "NOTURNO",  "Lead escuro na melodia, pad de vidro segurando os acordes, baixo reese",                "dark_lead",   "glass_pad",     "reese", true },
    { "PHONK",    "Cowbell na melodia, pad escuro segurando os acordes, 808 pesada",                        "cowbell",     "dark_pad",      "bass808_heavy", true },
    { "\xc3\x89PICO", "Metais de synth na melodia, cordas de synth segurando os acordes, sub quente",        "synth_brass", "string_pad",    "sub_warm", true },
};

// per set and layer (melody, bass, chords): dB, measured so every set exports at a similar loudness with
// chords a few dB under the melody and the bass level with it
const float kitGainsDb[Sets::kitCount][3] {
    { 2.0f, 2.4f, 3.6f },
    { 1.3f, 3.7f, 1.9f },
    { 8.6f, 4.9f, 12.4f },
    { 11.6f, 5.2f, 0.6f },
    { 10.9f, 5.1f, 14.1f },
    { 7.0f, 11.0f, 8.5f },
    { 9.5f, 12.2f, 3.9f },
    { 4.4f, 6.3f, 2.5f },
    { 3.1f, 15.0f, 3.5f },
    { 6.8f, 12.3f, -2.5f },
    { 11.1f, 6.3f, -3.7f },
    { 3.1f, 3.9f, 1.3f },
};

struct KitPatches { const P* melody; const P* chords; const P* bass; };
const KitPatches& kitPatches(int k) {
    static const auto all = [] {
        std::array<KitPatches, Sets::kitCount> a {};
        for (int i = 0; i < Sets::kitCount; ++i) a[(size_t) i] = { &patch(kits[i].melody), &patch(kits[i].chords), &patch(kits[i].bass) };
        return a;
    }();
    return all[(size_t) k];
}

double hz(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }
float blep(double t, double dt) {
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
float saw(double ph, double dt) { return (float) (2.0 * ph - 1.0) - blep(ph, dt); }
float square(double ph, double dt) {
    double ph2 = ph + 0.5; if (ph2 >= 1.0) ph2 -= 1.0;
    return (ph < 0.5 ? 1.0f : -1.0f) + blep(ph, dt) - blep(ph2, dt);
}
float tri(double ph) { return (float) (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph); }
void advance(double& ph, double inc) { ph += inc; ph -= std::floor(ph); }
float coef(float seconds, double sr) { return std::exp(-6.9f / (juce::jmax(0.0005f, seconds) * (float) sr)); }   // -60 dB in `seconds`
float white(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }

float basic(Osc o, double ph, double dt) {
    switch (o) {
        case Osc::sine: return (float) std::sin(twoPi * ph);
        case Osc::tri: return tri(ph);
        case Osc::saw: case Osc::supersaw: return saw(ph, dt);
        case Osc::square: return square(ph, dt);
        default: return 0.0f;
    }
}
}

const Sets::Kit& Sets::kit(int k) { return kits[k < 0 || k >= kitCount ? 0 : k]; }

void Sets::prepare(double rate) {
    sr = rate;
    for (auto& v : voices) { v.active = false; v.ks.assign((size_t) (sr / 20.0) + 4, 0.0f); }
}

void Sets::allNotesOff() {
    for (auto& v : voices) if (v.active) { v.releasing = true; v.kill = coef(0.03f, sr); }
}

int Sets::activeVoices() const {
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Sets::noteOn(int pitch, int velocity, int lengthSamples, int layer) {
    layer = juce::jlimit(0, 2, layer);
    const auto& K = kitPatches(kitIndex);
    const P& p = *(layer == 1 ? K.bass : layer == 2 ? K.chords : K.melody);
    // the whole line moves by octaves toward the patch's centre, then single notes fold into its range
    const int c = centers[(size_t) layer];
    if (c > 0) pitch += 12 * (int) std::lround((p.center - c) / 12.0);
    while (pitch < p.lo) pitch += 12;
    while (pitch > p.hi) pitch -= 12;
    const float vel = juce::jlimit(0.0f, 1.0f, (float) velocity / 127.0f);
    const int length = juce::jmax(1, lengthSamples);

    const Voice* legato = nullptr;
    if (p.mono) {   // one note at a time: a held note slides into the next one, anything else is cut short
        for (auto& x : voices) {
            if (!x.active || x.layer != layer || x.p != &p) continue;
            if (!x.releasing && p.glideMs > 0) {
                x.freqTarget = hz(pitch); x.pitch = pitch; x.left = length;
                x.glideK = 1.0 - std::exp(-1.0 / (p.glideMs * 0.001 * sr));
                x.vel = vel;
                return;
            }
            x.releasing = true; x.kill = coef(0.012f, sr);
        }
    } else {   // a repeated note lets the old one go quickly (a real player re-strikes)
        for (auto& x : voices)
            if (x.active && !x.releasing && x.layer == layer && x.pitch == pitch) { x.releasing = true; x.kill = coef(0.06f, sr); }
    }

    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) { v = &voices[0]; for (auto& x : voices) if (x.env < v->env) v = &x; }
    start(*v, p, pitch, vel, length, layer, legato);
}

void Sets::start(Voice& v, const Patch& p, int pitch, float vel, int length, int layer, const Voice*) {
    v.active = true; v.releasing = false; v.p = &p; v.layer = layer; v.pitch = pitch; v.left = length; v.age = 0; v.vel = vel;
    v.freq = v.freqTarget = hz(pitch); v.glideK = 1.0;
    const float layerLevel = layer == 1 ? bassLevel : layer == 2 ? chordLevel : melodyLevel;
    v.gain = p.level * (0.3f + 0.7f * std::pow(vel, 1.3f)) * layerLevel * 0.3f
           * juce::Decibels::decibelsToGain(kitGainsDb[kitIndex][layer]);
    v.env = 0.0f; v.kill = 0.0f;
    v.atkK = 1.0f / juce::jmax(1.0f, p.attack * (float) sr);
    v.decK = coef(p.decay, sr);
    v.relK = coef(p.release, sr);
    v.fenv = 1.0f; v.fK = coef(p.fDecay, sr);
    v.noiseLp = 0.0f;
    v.rng = 0x9e3779b9u ^ (uint32_t) (pitch * 7919 + layer * 104729 + panFlip * 31);
    for (auto& side : v.svf) for (auto& st : side) st[0] = st[1] = 0.0f;
    // unison voices start spread so the stack does not click; single oscillators start at zero (same loudness every note)
    for (int i = 0; i < maxUnison; ++i) { v.ph[0][i] = p.unison > 1 ? std::fmod(i * 0.618, 1.0) : 0.0; v.ph[1][i] = 0.0; }
    v.mod[0] = v.mod[1] = 0.0; v.sub = 0.0;
    for (auto& x : v.part) x = 0.0;
    float pan = 0.0f;
    if (layer == 2) { panFlip ^= 1; pan = panFlip ? 0.2f : -0.2f; }
    v.panL = std::sqrt(0.5f * (1.0f - pan)); v.panR = std::sqrt(0.5f * (1.0f + pan));

    if (p.osc1 == Osc::pluck) {   // a burst of filtered noise in a tuned delay line
        std::fill(v.ks.begin(), v.ks.end(), 0.0f);
        v.ksDelay = juce::jlimit(2.0, (double) v.ks.size() - 3.0, sr / v.freq - 0.5);
        const int n = (int) std::ceil(v.ksDelay) + 2;
        float lp = 0.0f;
        const float b = juce::jlimit(0.05f, 1.0f, p.ksBright * (0.6f + 0.6f * vel));
        for (int i = 0; i < n; ++i) { lp += b * (white(v.rng) - lp); v.ks[(size_t) i] = lp; }
        v.ksPos = n; v.ksLast = 0.0f;
        const float t60 = juce::jlimit(0.3f, 6.0f, p.ksDecay * std::pow(2.0f, -(float) (pitch - 60) / 24.0f));
        v.ksLoss = std::pow(0.001f, 1.0f / (t60 * (float) v.freq));
    }
}

void Sets::renderVoice(Voice& v, float& outL, float& outR) {
    const P& p = *v.p;
    const float t = (float) v.age / (float) sr;
    ++v.age;
    if (v.glideK < 1.0) v.freq += (v.freqTarget - v.freq) * v.glideK;
    double semis = 0.0;
    if (p.pitchEnv != 0.0f) semis += p.pitchEnv * std::exp(-t / p.pitchDecay);
    if (p.vibrato > 0.0f) semis += p.vibrato * 0.01 * std::sin(twoPi * p.vibRate * t) * juce::jlimit(0.0f, 1.0f, (t - p.vibDelay) * 3.0f);
    const double f = semis != 0.0 ? v.freq * std::pow(2.0, semis / 12.0) : v.freq;
    const double dt = f / sr;

    // envelopes
    if (!v.releasing) {
        if (--v.left <= 0) v.releasing = true;
        if (v.env < 1.0f && v.age <= (int) (p.attack * (float) sr) + 1) v.env = juce::jmin(1.0f, v.env + v.atkK);
        else v.env = p.sustain + (v.env - p.sustain) * v.decK;
    } else {
        v.env *= v.kill > 0.0f ? v.kill : v.relK;
    }
    if (v.releasing && v.env < 0.0003f) { v.active = false; return; }
    v.fenv *= v.fK;

    float l = 0.0f, r = 0.0f;
    // source 1
    switch (p.osc1) {
        case Osc::fm: {
            const float depth = (p.iSus + (1.0f - p.iSus) * std::exp(-t / p.iDecay)) * (0.45f + 0.55f * v.vel);
            const float m = p.i1 * depth * (float) std::sin(twoPi * v.mod[0]) + p.i2 * depth * (float) std::sin(twoPi * v.mod[1]);
            const float s = (float) std::sin(twoPi * v.ph[0][0] + m);
            advance(v.ph[0][0], dt); advance(v.mod[0], dt * p.r1); advance(v.mod[1], dt * p.r2);
            l += s * p.level1; r += s * p.level1;
            break;
        }
        case Osc::additive: {
            float s = 0.0f;
            for (int i = 0; i < p.partialCount && i < 8; ++i) {
                const auto& q = p.partials[i];
                const double inc = dt * q.ratio;
                if (inc >= 0.45) continue;   // above Nyquist: skip
                float a = q.level;
                if (q.decay > 0) a *= std::exp(-6.9f * t / (q.decay * std::pow(2.0f, -(float) (v.pitch - 60) / 36.0f)));
                s += a * (float) std::sin(twoPi * v.part[i]);
                advance(v.part[i], inc);
            }
            l += s * p.level1; r += s * p.level1;
            break;
        }
        case Osc::pluck: {
            double rp = v.ksPos - v.ksDelay;
            const int size = (int) v.ks.size();
            while (rp < 0) rp += size;
            const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
            const float fr = (float) (rp - std::floor(rp));
            const float y = v.ks[(size_t) i0] + fr * (v.ks[(size_t) i1] - v.ks[(size_t) i0]);
            const float fb = (0.5f * y + 0.5f * v.ksLast) * v.ksLoss;
            v.ksLast = y;
            v.ks[(size_t) (v.ksPos % size)] = fb;
            v.ksPos = (v.ksPos + 1) % size;
            l += y * 1.6f * p.level1; r += y * 1.6f * p.level1;
            break;
        }
        case Osc::none: break;
        default: {
            const int n = juce::jlimit(1, maxUnison, p.unison);
            float sl = 0.0f, sr2 = 0.0f;
            for (int i = 0; i < n; ++i) {
                const float pos = n > 1 ? 2.0f * (float) i / (float) (n - 1) - 1.0f : 0.0f;   // -1 .. 1
                const double inc = dt * std::pow(2.0, p.detune * pos / 1200.0);
                const float s = basic(p.osc1, v.ph[0][i], inc);
                advance(v.ph[0][i], inc);
                const float pr = 0.5f + 0.5f * pos * p.width;
                sl += s * (1.0f - pr); sr2 += s * pr;
            }
            const float norm = 2.0f / std::sqrt((float) n);
            l += sl * norm * p.level1; r += sr2 * norm * p.level1;
        }
    }
    // source 2
    if (p.osc2 != Osc::none) {
        const double inc = dt * std::pow(2.0, (p.semi2 * 100.0 + p.detune2) / 1200.0);
        if (inc < 0.45) {
            const float s = basic(p.osc2, v.ph[1][0], inc) * p.level2;
            advance(v.ph[1][0], inc);
            l += s; r += s;
        }
    }
    if (p.sub > 0.0f) { const float s = p.sub * (float) std::sin(twoPi * v.sub); advance(v.sub, dt * 0.5); l += s; r += s; }
    if (p.noise > 0.0f) {
        const float nk = 1.0f - std::exp(-2.0f * pi * p.noiseTone / (float) sr);
        v.noiseLp += nk * (white(v.rng) - v.noiseLp);
        const float s = p.noise * 3.0f * v.noiseLp * std::exp(-t / p.noiseDecay);
        l += s; r += s;
    }

    // filter (coefficients refreshed every 8 samples)
    if (p.filter != Filter::none) {
        if ((v.age & 7) == 1) {
            const float cutoff = p.cutoff * std::pow(2.0f, p.keyTrack * (float) (v.pitch - 60) / 12.0f + p.fEnv * (p.fSus + (1.0f - p.fSus) * v.fenv)
                                                     + p.velCut * (v.vel - 0.75f));
            v.g = std::tan(pi * juce::jlimit(20.0f, (float) sr * 0.45f, cutoff) / (float) sr);
            v.k = 2.0f - 1.95f * p.res;
        }
        const int stages = p.filter == Filter::lp24 ? 2 : 1;
        auto run = [&](float x, int side) {
            for (int s = 0; s < stages; ++s) {
                auto& st = v.svf[side][s];
                const float hp = (x - (v.k + v.g) * st[0] - st[1]) / (1.0f + v.g * (v.g + v.k));
                const float bp = v.g * hp + st[0];
                const float lp = v.g * bp + st[1];
                st[0] = v.g * hp + bp; st[1] = v.g * bp + lp;
                x = p.filter == Filter::bp ? bp : p.filter == Filter::hp ? hp : lp;
            }
            return x;
        };
        l = run(l, 0); r = run(r, 1);
    }
    if (p.drive > 0.0f) {
        const float d = 1.0f + p.drive * 5.0f, n = 1.0f / std::tanh(d);
        l = std::tanh(l * d) * n; r = std::tanh(r * d) * n;
    }
    if (p.tremolo > 0.0f) {
        const float lfo = (float) std::sin(twoPi * p.tremRate * t);
        l *= 1.0f - p.tremolo * (0.5f + 0.5f * lfo); r *= 1.0f - p.tremolo * (0.5f - 0.5f * lfo);
    }
    const float a = v.env * v.gain;
    outL += l * a * v.panL; outR += r * a * v.panR;
}

void Sets::render(float& l, float& r, float& dryL, float& dryR) {
    for (auto& v : voices) {
        if (!v.active) continue;
        if (v.layer == 1) renderVoice(v, dryL, dryR);
        else renderVoice(v, l, r);
    }
}
