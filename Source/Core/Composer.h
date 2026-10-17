#pragma once
#include "MelodyEngine.h"
#include <cstdint>
#include <vector>

/* Melody Code composition engine.

   Pipeline (one call to compose):
     Context        key, scale, chords over time, bars, register, style / vibe / role / feel / density
     Rhythm         a motif rhythm built from one-beat cells chosen by density, style and feel
     Motif          that rhythm plus pitches chosen by the Pitch generator, with a contour
     Pitch          every candidate note is scored (scale, chord, interval, metre, contour, register,
                    phrase role, recent history, cadence) and one is drawn from the best few by weight
     Phrase         the motif is developed into 2 / 4 / 8 bars (sentence, period, hook forms):
                    statement, sequence, rhythmic and interval variation, fragmentation, climax, cadence
     Evaluator      whole-melody score; a bounded set of candidates is written and a good one picked
   Everything is driven by a seed, so the same context and seed always give the same melody.

   The weights below are starting points tuned by listening; they are plain data so tests and future
   tuning can change them. A high score does not guarantee a good melody, it only filters bad ones. */
namespace mc::composer {

struct PitchWeights {
    double chordStrong = 3.0;      // chord tone on a strong beat
    double nonChordStrong = -1.2;  // non-chord tone on a strong beat (unless it is a prepared suspension)
    double chordWeak = 0.6;        // chord tone on a weak position
    double outOfScale = -9.0;      // chromatic note
    double repeat = 0.5;           // same note again
    double tooManyRepeats = -3.0;  // a third repetition in a row
    double step = 2.0;             // 1-2 semitones
    double third = 1.1;            // 3-4 semitones
    double fourthFifth = 0.1;      // 5-7 semitones
    double bigLeap = -1.6;         // 8-12 semitones
    double contour = 1.4;          // moving the way the phrase contour wants
    double leapRecovery = 2.2;     // a step back after a leap
    double leapAgain = -2.0;       // a second leap the same way
    double registerPull = 0.09;    // per semitone away from the register centre
    double motifShape = 2.6;       // keeping the motif's interval (identity)
    double motifDirection = 1.0;   // keeping at least the motif's direction
    double cadence = 4.0;          // landing on the phrase ending's target
    double sequenceRepeat = -1.0;  // the same interval three times running
    double counterConsonance = 1.6;// counter line: 3rds/6ths against the lead, not unisons/2nds
    double temperature = 0.85;     // lower = closer to the best candidate
    int candidates = 6;            // how many of the best candidates can be drawn
};

struct EvalWeights {
    double harmonic = 10.0;        // chord tones on strong beats
    double rhythmVariety = 3.0;
    double rests = 3.0;            // breathing space inside the density's window
    double climax = 3.0;           // one highest point, in the second half
    double range = 2.0;
    double leapBalance = 2.0;
    double repetition = 3.0;       // motif comes back, but not every bar identical
    double ending = 4.0;
    double density = 4.0;          // notes per bar inside the density's window
    double monotony = 2.0;         // long same-pitch or same-direction runs
    double genre = 5.0;            // syncopation / notes / space inside the genre engine's windows
};

PitchWeights& pitchWeights();
EvalWeights& evalWeights();

constexpr int candidateCount = 16;   // melodies written per request before one is chosen

// A complete melody for the pattern's context. `lead` (optional) is the line a COUNTER role answers.
std::vector<Note> compose(const Pattern&, uint32_t seed, const std::vector<Note>* lead = nullptr);
// A related alternative: keeps most of `source` and changes a few chosen aspects.
std::vector<Note> vary(const Pattern&, const std::vector<Note>& source, uint32_t seed);
// Whole-melody score used to choose between candidates.
double evaluate(const Pattern&, const std::vector<Note>&);
// Share of notes kept (same start and pitch) between two melodies, 0..1.
double similarity(const std::vector<Note>& a, const std::vector<Note>& b);
// Register used for a range / role / vibe.
struct Register { int lo, hi, centre; };
Register registerFor(const Settings&);
}
