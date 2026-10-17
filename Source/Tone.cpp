#include "Tone.h"
#include <algorithm>
#include <random>

namespace tone {
namespace {
using F = Family;
// ids 0..5 are the original six sounds (saved sessions keep loading them); variants follow.
//                                                                       env: atk   decay sus  rel     macros: char body atk  move space tex  rel       polish: cut  width chorus   perf: vel bright vib rate glide   level
const Preset presets[] {
    { 0, "keys.warm",      "KEYS",            F::keys,   0, "Warm electric piano, soft bark when hit hard",
      { 0.002f, 2.4f, 0.0f, 0.30f }, { 0.40f, 0.65f, 0.50f, 0.15f, 0.45f, 0.20f, 0.50f }, { 45, 0.9f, 0.10f }, { 0.75f, 0.9f, 0, 0, 0 }, 1.04f },
    { 1, "bells.crystal",  "BELLS",           F::bells,  0, "Clear glass bell, inharmonic shimmer that fades fast",
      { 0.0015f, 3.2f, 0.0f, 0.80f }, { 0.50f, 0.45f, 0.50f, 0.20f, 0.55f, 0.25f, 0.50f }, { 120, 1.0f, 0.00f }, { 0.70f, 0.9f, 0, 0, 0 }, 1.12f },
    { 2, "pad.warm",       "PAD",             F::pad,    0, "Warm supersaw pad, slow breathing filter",
      { 0.45f, 1.0f, 0.85f, 1.20f }, { 0.40f, 0.55f, 0.50f, 0.40f, 0.55f, 0.15f, 0.50f }, { 60, 1.0f, 0.10f }, { 0.8f, 0.4f, 0, 0, 0 }, 0.97f },
    { 3, "pluck.string",   "PLUCK",           F::pluck,  0, "Plucked string, bright pick and a natural decay",
      { 0.001f, 2.5f, 0.0f, 0.18f }, { 0.55f, 0.45f, 0.55f, 0.00f, 0.40f, 0.10f, 0.50f }, { 50, 0.9f, 0.00f }, { 0.7f, 0.8f, 0, 0, 0 }, 0.66f },
    { 4, "lead.soft",      "LEAD",            F::lead,   0, "Soft analog lead, two saws and a square, late vibrato",
      { 0.010f, 0.6f, 0.90f, 0.20f }, { 0.45f, 0.50f, 0.50f, 0.30f, 0.35f, 0.15f, 0.50f }, { 80, 0.8f, 0.00f }, { 0.6f, 0.8f, 8, 5.0f, 0 }, 0.68f },
    { 5, "mallet.marimba", "MALLET",          F::mallet, 0, "Wooden marimba bar with a soft mallet",
      { 0.0008f, 1.4f, 0.0f, 0.08f }, { 0.45f, 0.55f, 0.50f, 0.00f, 0.40f, 0.10f, 0.50f }, { 50, 0.8f, 0.00f }, { 0.7f, 0.9f, 0, 0, 0 }, 0.90f },

    { 6, "keys.clean",     "KEYS CLEAN",      F::keys,   1, "Clean tine piano, round and close, little bark",
      { 0.002f, 2.0f, 0.0f, 0.25f }, { 0.25f, 0.55f, 0.55f, 0.00f, 0.30f, 0.00f, 0.45f }, { 50, 0.8f, 0.05f }, { 0.75f, 0.7f, 0, 0, 0 }, 1.27f },
    { 7, "keys.suitcase",  "KEYS ELECTRIC",   F::keys,   2, "Driven suitcase piano with stereo tremolo",
      { 0.002f, 2.6f, 0.0f, 0.35f }, { 0.70f, 0.60f, 0.60f, 0.60f, 0.40f, 0.50f, 0.50f }, { 50, 1.1f, 0.00f }, { 0.8f, 1.0f, 0, 0, 0 }, 1.06f },
    { 8, "keys.modern",    "KEYS MODERN",     F::keys,   3, "Digital FM keys, glassy top and a firm attack",
      { 0.001f, 1.8f, 0.0f, 0.25f }, { 0.70f, 0.40f, 0.65f, 0.20f, 0.45f, 0.15f, 0.45f }, { 70, 1.0f, 0.15f }, { 0.7f, 1.0f, 0, 0, 0 }, 1.53f },

    { 9, "bells.soft",     "BELLS SOFT",      F::bells,  1, "Soft harmonic bell, round like a music box",
      { 0.003f, 2.4f, 0.0f, 0.60f }, { 0.25f, 0.55f, 0.35f, 0.10f, 0.50f, 0.05f, 0.45f }, { 100, 0.9f, 0.05f }, { 0.85f, 0.7f, 0, 0, 0 }, 1.20f },
    { 10, "bells.metal",   "BELLS METAL",     F::bells,  2, "Metallic struck bell, dense inharmonic partials",
      { 0.001f, 3.6f, 0.0f, 0.90f }, { 0.70f, 0.40f, 0.65f, 0.25f, 0.45f, 0.45f, 0.50f }, { 140, 1.0f, 0.00f }, { 0.75f, 1.0f, 0, 0, 0 }, 1.16f },
    { 11, "bells.glass",   "BELLS GLASS",     F::bells,  3, "Organic glass / bowl partials with a soft strike",
      { 0.002f, 3.0f, 0.0f, 0.70f }, { 0.50f, 0.50f, 0.45f, 0.30f, 0.50f, 0.30f, 0.50f }, { 110, 1.0f, 0.00f }, { 0.7f, 0.8f, 0, 0, 0 }, 1.06f },
    { 12, "bells.atmos",   "BELLS ATMOS",     F::bells,  4, "Airy bell with a slower bloom and beating partials",
      { 0.035f, 4.5f, 0.0f, 1.40f }, { 0.45f, 0.40f, 0.30f, 0.70f, 0.80f, 0.20f, 0.60f }, { 150, 1.25f, 0.20f }, { 0.9f, 0.7f, 0, 0, 0 }, 1.00f },

    { 13, "pad.air",       "PAD AIR",         F::pad,    1, "Bright airy pad with a breath layer",
      { 0.60f, 1.0f, 0.80f, 1.50f }, { 0.75f, 0.30f, 0.45f, 0.45f, 0.65f, 0.55f, 0.55f }, { 120, 1.2f, 0.15f }, { 0.8f, 0.5f, 0, 0, 0 }, 1.08f },
    { 14, "pad.glass",     "PAD GLASS",       F::pad,    2, "FM glass pad whose shimmer slowly evolves",
      { 0.50f, 1.0f, 0.85f, 1.40f }, { 0.55f, 0.40f, 0.50f, 0.60f, 0.60f, 0.20f, 0.55f }, { 90, 1.1f, 0.10f }, { 1.0f, 0.6f, 0, 0, 0 }, 1.02f },
    { 15, "pad.dark",      "PAD DARK",        F::pad,    3, "Dark hollow pad, square waves under a low filter",
      { 0.40f, 1.0f, 0.90f, 1.10f }, { 0.20f, 0.70f, 0.50f, 0.30f, 0.50f, 0.25f, 0.50f }, { 50, 0.9f, 0.05f }, { 0.8f, 0.4f, 0, 0, 0 }, 0.72f },

    { 16, "pluck.synth",   "PLUCK SYNTH",     F::pluck,  1, "Synth pluck, resonant filter snapping shut",
      { 0.001f, 0.45f, 0.0f, 0.12f }, { 0.60f, 0.50f, 0.65f, 0.15f, 0.45f, 0.20f, 0.45f }, { 60, 1.0f, 0.10f }, { 1.0f, 1.0f, 0, 0, 0 }, 1.78f },
    { 17, "pluck.felt",    "PLUCK SOFT",      F::pluck,  2, "Felt-soft dark string with a woody body",
      { 0.002f, 1.6f, 0.0f, 0.15f }, { 0.25f, 0.70f, 0.30f, 0.00f, 0.40f, 0.05f, 0.50f }, { 50, 0.8f, 0.00f }, { 0.85f, 0.6f, 0, 0, 0 }, 0.83f },
    { 18, "pluck.digital", "PLUCK DIGITAL",   F::pluck,  3, "Short FM pluck, hollow and percussive",
      { 0.001f, 0.6f, 0.0f, 0.10f }, { 0.65f, 0.40f, 0.70f, 0.10f, 0.45f, 0.15f, 0.40f }, { 80, 1.0f, 0.05f }, { 0.85f, 1.0f, 0, 0, 0 }, 1.32f },

    { 19, "lead.square",   "LEAD SQUARE",     F::lead,   1, "Hollow pulse lead with slow pulse-width movement",
      { 0.006f, 0.5f, 0.85f, 0.15f }, { 0.50f, 0.45f, 0.55f, 0.45f, 0.35f, 0.10f, 0.45f }, { 90, 0.7f, 0.00f }, { 0.8f, 0.7f, 0, 0, 0 }, 0.56f },
    { 20, "lead.flute",    "LEAD FLUTE",      F::lead,   2, "Breathy flute-like lead with vibrato and a short glide",
      { 0.030f, 0.5f, 0.90f, 0.15f }, { 0.35f, 0.50f, 0.40f, 0.40f, 0.45f, 0.40f, 0.50f }, { 120, 0.7f, 0.00f }, { 0.85f, 0.6f, 14, 5.2f, 45 }, 0.43f },
    { 21, "lead.saw",      "LEAD SAW",        F::lead,   3, "Bright focused saw lead, present in the mids",
      { 0.004f, 0.4f, 0.85f, 0.12f }, { 0.70f, 0.45f, 0.65f, 0.15f, 0.35f, 0.35f, 0.40f }, { 110, 0.9f, 0.05f }, { 0.85f, 0.9f, 0, 0, 0 }, 0.85f },

    { 22, "mallet.vibes",  "MALLET VIBES",    F::mallet, 1, "Vibraphone bar with motor tremolo",
      { 0.001f, 2.6f, 0.0f, 0.25f }, { 0.40f, 0.50f, 0.45f, 0.55f, 0.50f, 0.05f, 0.55f }, { 60, 1.0f, 0.05f }, { 0.7f, 0.8f, 0, 0, 0 }, 0.83f },
    { 23, "mallet.kalimba","MALLET KALIMBA",  F::mallet, 2, "Kalimba tine with a thumb click and a little buzz",
      { 0.0008f, 1.2f, 0.0f, 0.12f }, { 0.55f, 0.50f, 0.60f, 0.00f, 0.40f, 0.35f, 0.50f }, { 70, 0.9f, 0.00f }, { 0.7f, 0.9f, 0, 0, 0 }, 1.00f },
};
constexpr int presetTotal = (int) (sizeof(presets) / sizeof(presets[0]));
}

int count() { return presetTotal; }
const Preset& get(int id) { return presets[id >= 0 && id < presetTotal ? id : 0]; }

const char* familyName(Family f) {
    static const char* n[] { "KEYS", "BELLS", "PAD", "PLUCK", "LEAD", "MALLET" };
    return n[(int) f];
}

Macros vary(const Preset& p, uint32_t seed, float amount) {
    std::mt19937 rng(seed ^ (uint32_t) (p.id * 2654435761u));
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    Macros m = p.macros;
    // timbre macros move freely inside the window; attack and release only a little so the
    // articulation (and the family's identity) stays
    float* free[] { &m.character, &m.body, &m.movement, &m.space, &m.texture };
    for (auto* x : free) *x = std::clamp(*x + u(rng) * amount, 0.0f, 1.0f);
    m.attack = std::clamp(m.attack + u(rng) * amount * 0.5f, 0.0f, 1.0f);
    m.release = std::clamp(m.release + u(rng) * amount * 0.5f, 0.0f, 1.0f);
    return m;
}
}
