#pragma once
#include <cstdint>

/* Tone Design Engine: the preset side of the preview synth.

   A preset is plain data, rendered by one family voice in Synth.cpp. Four layers:
     Source       family + algo pick the oscillators (FM, additive, supersaw, Karplus-Strong, modal bars)
     Shaping      envelope times, filter, harmonics, saturation, transients: driven by the macros
     Polish       per-preset low cut, stereo width, chorus and how much of the Vibe's echo / reverb it takes
     Performance  velocity curve (loudness AND brightness / modulation), note length vs envelope,
                  key tracking, optional vibrato and glide (only presets that ask for them)
   The Vibe bus (Synth::Fx) still sits after all of this and is shared by every preset.

   Macros are 0..1 and mean the same idea in every family, mapped differently per family:
     character  spectral identity (FM index, partial mix, pick position, waveform / cutoff)
     body       weight in the low mids (fundamental reinforcement, sub layer, string loss)
     attack     how fast and how defined the start is (attack time and transient layer)
     movement   modulation (tremolo, detune drift, filter LFO, PWM, beating partials)
     space      how much of the Vibe's ambience the preset takes, with pre-delay to keep the attack
     texture    complexity: saturation, noise / breath / air, extra inharmonic partials
     release    how the note ends after note-off, and how long percussive families ring
   0.5 is each preset's tuned starting point for attack and release (times scale x0.5 .. x2). */
namespace tone {

enum class Family { keys, bells, pad, pluck, lead, mallet };

struct Macros { float character, body, attack, movement, space, texture, release; };

struct Envelope { float attack, decay, sustain, release; };   // seconds, sustain 0..1 (0 = percussive)

struct Polish {
    float lowCut;        // Hz, high-pass on the preset's dry signal
    float width;         // 0 mono .. 1 as rendered .. 1.4 wider
    float chorus;        // added to the Vibe's chorus
};

struct Performance {
    float velCurve;      // exponent on velocity for loudness (<1 keeps soft notes audible)
    float velBright;     // how much velocity opens brightness / modulation, 0..1
    float vibratoCents;  // 0 = none
    float vibratoRate;   // Hz
    float glideMs;       // 0 = none; melody notes slide from the previous note
};

struct Preset {
    int id;                    // stable: saved in the plugin state, never reuse or renumber
    const char* key;           // stable text id
    const char* name;          // shown in SOUND
    Family family;
    int algo;                  // source model inside the family
    const char* description;
    Envelope env;
    Macros macros;
    Polish polish;
    Performance perf;
    float level;               // loudness match between presets (not a volume boost)
};

int count();
const Preset& get(int id);            // clamps unknown ids to 0
const char* familyName(Family);

// A related version of a preset's macros: a few selected macros move inside a small window, the
// family and source never change, so a Bell stays a Bell. Same seed, same result.
Macros vary(const Preset&, uint32_t seed, float amount = 0.12f);
}
