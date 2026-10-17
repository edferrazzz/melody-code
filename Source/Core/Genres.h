#pragma once
#include "Music.h"
#include <array>
#include <vector>

/* Genre composition engines.

   Each style the user can pick has its own engine: plain data read by the shared machinery (Composer for the
   melody, MelodyEngine for chords / bass / accompaniment). Shared parts: pitch scoring, motif development,
   evaluation, MIDI. Per genre, and different in every engine:
     harmony     progressions by scale degree (minor-ish and major-ish scales), chord colours, harmonic rhythm
     groove      the one-bar rhythm vocabulary the motif is built from, how syncopated / busy / legato it is
     phrasing    phrase forms (statement / sequence / develop / open / cadence ...), motif length, repetition
     melody      leaps, passing / approach notes, contour taste
     layering    which intervals may sound under the melody (dyads, doubling, held notes) and how often
     comping     the rhythm the chords are played in, under the melody
     evaluation  the windows a finished melody should fall in (notes per bar, syncopation, rests, repetition)
   Values are starting points tuned by ear; they describe habits, not fixed MIDI. */
namespace mc {
enum class Style;
namespace genre {

struct Hit { int on, len; };                   // 16ths inside one bar
using Bar = std::vector<Hit>;

struct Prog { std::array<int, 4> degree; std::array<ccp::ChordKind, 4> kind; };   // 0-based scale degrees

struct Layer {
    double chance;                 // share of eligible melody notes that get a second voice
    std::vector<int> below;        // intervals (semitones) the added voice may sit under the melody note
    double holdUnder;              // chance the previous note keeps ringing under the next (consonant only)
    int maxVoices;                 // most notes sounding at once in the melody part
};

struct Targets { double notesLo, notesHi, syncLo, syncHi, restLo, restHi, repeat; };

struct Engine {
    const char* name;
    const char* character;                         // one line: what this engine is going for
    std::vector<ccp::ScaleType> scales;            // what PADRÃO picks from
    std::vector<Prog> minorProgs, majorProgs;      // 4-chord cycles; 2 bars use the first two, 8 bars two cycles
    std::vector<std::array<int, 4>> harmonicRhythm;   // per bar: chord start in 16ths from the bar line, 99 = hold
    std::vector<Bar> groove;                       // melody rhythm vocabulary
    std::vector<Bar> comp;                         // chord accompaniment rhythms
    // melody habits
    double notesPerBar, syncopation, sixteenths, restiness, literal, leapiness, legato, twoBarMotif, passing;
    std::array<double, 5> contour;                 // arch, rise, fall, wave, valley (multiplies the Vibe's taste)
    std::vector<const char*> forms;                // 4-unit forms: S statement, Q sequence, D develop, F fragment, O open, X climax, C cadence
    Layer layer;
    Targets targets;
    int compVelocity;
};

const Engine& get(Style);
}
}
