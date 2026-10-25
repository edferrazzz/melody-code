#include "Genres.h"
#include "MelodyEngine.h"

namespace mc::genre {
namespace {
using S = ccp::ScaleType;
using K = ccp::ChordKind;
constexpr K tr = K::triad, s7 = K::seventh, n9 = K::ninth, a9 = K::add9, su2 = K::sus2;
constexpr int H = 99;

const Engine& funk() {
    static const Engine e {
        "FUNK BR", "short catchy calls and answers on a syncopated tamborzao grid, punchy notes, deliberate gaps",
        { S::naturalMinor, S::phrygian, S::harmonicMinor, S::naturalMinor },
        { { { 0, 3, 0, 3 }, { tr, tr, tr, tr } }, { { 0, 6, 5, 6 }, { tr, tr, tr, tr } },
          { { 0, 5, 6, 0 }, { tr, tr, tr, tr } }, { { 0, 0, 3, 4 }, { tr, tr, tr, tr } } },
        { { { 0, 4, 5, 3 }, { tr, tr, tr, tr } }, { { 5, 3, 0, 4 }, { tr, tr, tr, tr } } },
        { { 0, 0, 0, 0 }, { 0, H, 0, H } },
        { { { 0, 2 }, { 3, 1 }, { 6, 2 }, { 10, 1 }, { 12, 2 } },
          { { 0, 1 }, { 3, 1 }, { 6, 1 }, { 8, 1 }, { 11, 1 }, { 14, 2 } },
          { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 10, 2 }, { 12, 2 } },
          { { 2, 1 }, { 3, 2 }, { 6, 1 }, { 7, 2 }, { 10, 2 }, { 12, 1 }, { 14, 1 } },
          { { 0, 1 }, { 3, 2 }, { 6, 1 }, { 10, 1 }, { 11, 2 } } },
        { { { 0, 1 }, { 3, 1 }, { 6, 1 }, { 10, 1 } }, { { 0, 2 }, { 6, 2 }, { 10, 1 }, { 12, 2 } } },
        5.5, .75, .5, .3, .9, .3, .45, .2, .1,
        { 1, 1, 1, 3, 1 },
        { "SOSC", "SQSC", "SSOC" },
        { .22, { 12 }, 0, 2 },
        { 3.5, 8.5, .35, .75, .25, .65, .65 }, 74,
    };
    return e;
}

const Engine& pop() {
    static const Engine e {
        "POP", "singable hook with a clear arc: question and answer, a lift to one peak, a satisfying landing",
        { S::major, S::naturalMinor, S::major },
        { { { 0, 5, 2, 6 }, { tr, tr, tr, tr } }, { { 0, 3, 5, 4 }, { tr, tr, tr, tr } }, { { 5, 3, 0, 4 }, { tr, tr, tr, tr } } },
        { { { 0, 4, 5, 3 }, { tr, tr, tr, tr } }, { { 5, 3, 0, 4 }, { tr, tr, tr, tr } },
          { { 0, 5, 3, 4 }, { tr, tr, a9, tr } }, { { 3, 0, 4, 5 }, { a9, tr, tr, tr } } },
        { { 0, 0, 0, 0 }, { 0, 0, -2, 0 } },
        { { { 0, 4 }, { 4, 2 }, { 6, 2 }, { 8, 6 } },
          { { 0, 2 }, { 2, 2 }, { 4, 4 }, { 10, 2 }, { 12, 4 } },
          { { 2, 2 }, { 4, 2 }, { 6, 4 }, { 12, 4 } },
          { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 8 } },
          { { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 2 }, { 8, 8 } } },
        { { { 0, 8 }, { 8, 8 } }, { { 0, 4 }, { 4, 4 }, { 8, 4 }, { 12, 4 } }, { { 0, 6 }, { 6, 2 }, { 8, 8 } } },
        4.3, .35, .2, .35, .75, .35, .75, .45, .15,
        { 4, 2, 1, 1, 1 },
        { "SQFC", "SOSC", "SDQC" },
        { .18, { 3, 4, 8, 9 }, .1, 2 },
        { 2.5, 6.0, .05, .45, .1, .45, .45 }, 66,
    };
    return e;
}

const Engine& trap() {
    static const Engine e {
        "TRAP", "dark hypnotic motif with space, quick rolls only as accents, long tails ringing under the next note",
        { S::naturalMinor, S::harmonicMinor, S::phrygian },
        { { { 0, 5, 3, 4 }, { tr, tr, tr, tr } }, { { 0, 0, 5, 5 }, { tr, tr, tr, tr } },
          { { 0, 5, 6, 4 }, { tr, tr, tr, tr } }, { { 0, 3, 5, 4 }, { tr, tr, tr, tr } } },
        { { { 5, 3, 0, 4 }, { tr, tr, tr, tr } }, { { 5, 4, 3, 4 }, { tr, tr, tr, tr } } },
        { { 0, H, 0, H }, { 0, 0, 0, 0 }, { 0, 8, 0, 8 } },
        { { { 0, 6 }, { 6, 6 }, { 12, 4 } },
          { { 0, 3 }, { 3, 3 }, { 6, 4 }, { 12, 4 } },
          { { 0, 2 }, { 3, 3 }, { 8, 1 }, { 9, 1 }, { 10, 2 }, { 12, 4 } },
          { { 0, 1 }, { 1, 1 }, { 2, 4 }, { 8, 8 } },
          { { 0, 3 }, { 3, 3 }, { 8, 3 }, { 11, 5 } } },
        { { { 0, 16 } }, { { 0, 12 }, { 12, 4 } } },
        3.8, .5, .35, .55, .8, .4, .6, .35, .05,
        { 1, 1, 3, 2, 2 },
        { "SSDC", "SQSC", "SQDC" },
        { .2, { 12, 7 }, .35, 2 },
        { 2.0, 6.0, .2, .6, .25, .7, .55 }, 60,
    };
    return e;
}

const Engine& rnb() {
    static const Engine e {
        "R&B", "smooth flowing lines over seventh and ninth chords, approach notes, late landings, thirds and sixths underneath",
        { S::dorian, S::naturalMinor, S::major },
        { { { 0, 3, 6, 2 }, { n9, n9, s7, s7 } }, { { 0, 3, 0, 4 }, { s7, n9, s7, s7 } }, { { 0, 6, 5, 4 }, { n9, s7, s7, s7 } } },
        { { { 1, 4, 0, 5 }, { s7, s7, s7, s7 } }, { { 0, 2, 5, 1 }, { s7, s7, s7, n9 } }, { { 3, 2, 1, 0 }, { s7, s7, s7, n9 } } },
        { { 0, -1, 0, -1 }, { 0, 0, -1, 0 }, { 0, 0, 0, 0 } },
        { { { 0, 3 }, { 3, 1 }, { 4, 4 }, { 10, 2 }, { 12, 4 } },
          { { 1, 2 }, { 3, 3 }, { 8, 3 }, { 12, 4 } },
          { { 0, 1 }, { 1, 1 }, { 2, 4 }, { 7, 1 }, { 8, 6 } },
          { { 0, 4 }, { 6, 2 }, { 8, 2 }, { 10, 6 } },
          { { 2, 2 }, { 4, 3 }, { 7, 1 }, { 8, 4 }, { 13, 3 } } },
        { { { 0, 10 }, { 10, 6 } }, { { 0, 16 } }, { { 0, 6 }, { 6, 10 } } },
        4.3, .55, .35, .35, .6, .45, .85, .5, .35,
        { 3, 1, 2, 2, 1 },
        { "SDOC", "SQDC", "SOSC" },
        { .4, { 3, 4, 8, 9 }, .3, 3 },
        { 2.5, 7.0, .25, .65, .08, .45, .35 }, 58,
    };
    return e;
}

const Engine& reggaeton() {
    static const Engine e {
        "REGGAETON", "cyclic dembow-shaped phrases (3-3-2), few well placed notes, the tail of each loop slightly changed",
        { S::naturalMinor, S::harmonicMinor, S::major },
        { { { 0, 5, 2, 6 }, { tr, tr, tr, tr } }, { { 0, 3, 6, 2 }, { tr, tr, tr, tr } }, { { 0, 6, 5, 6 }, { tr, tr, tr, tr } } },
        { { { 5, 3, 0, 4 }, { tr, tr, tr, tr } }, { { 0, 4, 5, 3 }, { tr, tr, tr, tr } } },
        { { 0, 0, 0, 0 } },
        { { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 3 }, { 11, 3 }, { 14, 2 } },
          { { 0, 2 }, { 3, 1 }, { 4, 4 }, { 8, 2 }, { 11, 1 }, { 12, 4 } },
          { { 3, 3 }, { 6, 2 }, { 11, 3 }, { 14, 2 } },
          { { 0, 3 }, { 3, 1 }, { 6, 2 }, { 8, 2 }, { 11, 3 } } },
        { { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 3 }, { 11, 3 }, { 14, 2 } }, { { 3, 1 }, { 6, 2 }, { 11, 1 }, { 14, 2 } } },
        4.8, .65, .25, .35, .9, .3, .55, .25, .08,
        { 1, 1, 1, 3, 2 },
        { "SSDC", "SOSC", "SSQC" },
        { .28, { 3, 4 }, 0, 2 },
        { 3.0, 7.0, .35, .75, .12, .5, .7 }, 62,
    };
    return e;
}

const Engine& house() {
    static const Engine e {
        "HOUSE", "off-beat ostinato that loops in 1-2 bar cycles, changing a note or two at a time over minor-seventh stabs",
        { S::dorian, S::naturalMinor, S::major },
        { { { 0, 3, 0, 3 }, { s7, s7, s7, s7 } }, { { 0, 6, 5, 6 }, { s7, tr, s7, tr } }, { { 0, 5, 3, 4 }, { s7, s7, s7, s7 } } },
        { { { 5, 3, 0, 4 }, { s7, s7, s7, tr } }, { { 1, 4, 0, 5 }, { s7, tr, s7, s7 } } },
        { { 0, 0, 0, 0 } },
        { { { 2, 2 }, { 6, 2 }, { 10, 2 }, { 14, 2 } },
          { { 0, 2 }, { 3, 1 }, { 6, 2 }, { 10, 2 }, { 12, 1 }, { 14, 2 } },
          { { 2, 1 }, { 4, 2 }, { 6, 1 }, { 10, 1 }, { 12, 2 }, { 14, 1 } },
          { { 0, 2 }, { 2, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 } } },
        { { { 2, 1 }, { 6, 1 }, { 10, 1 }, { 14, 1 } }, { { 0, 2 }, { 3, 2 }, { 6, 2 }, { 10, 2 }, { 12, 2 } } },
        4.8, .8, .25, .3, .95, .25, .55, .2, 0,
        { 1, 1, 1, 4, 1 },
        { "SSSC", "SSDC", "SSQC" },
        { .3, { 5, 7, 12 }, 0, 2 },
        { 3.0, 7.0, .45, .9, .2, .6, .85 }, 64,
    };
    return e;
}

const Engine& afrobeats() {
    static const Engine e {
        "AFROBEATS", "lilting, displaced phrases that talk to the groove, calls and answers, short notes against held ones",
        { S::major, S::mixolydian, S::dorian, S::naturalMinor },
        { { { 0, 3, 6, 2 }, { tr, tr, tr, tr } }, { { 0, 3, 0, 6 }, { s7, s7, s7, tr } }, { { 0, 5, 6, 0 }, { tr, tr, tr, s7 } } },
        { { { 0, 3, 5, 4 }, { tr, tr, tr, tr } }, { { 1, 4, 0, 5 }, { s7, s7, a9, s7 } },
          { { 3, 4, 2, 5 }, { tr, tr, tr, tr } }, { { 0, 4, 5, 3 }, { a9, tr, tr, tr } } },
        { { 0, -1, 0, -1 }, { 0, 0, -2, 0 }, { 0, 0, 0, 0 } },
        { { { 0, 3 }, { 3, 1 }, { 4, 2 }, { 7, 2 }, { 10, 3 }, { 14, 2 } },
          { { 2, 2 }, { 4, 1 }, { 6, 3 }, { 10, 2 }, { 12, 1 }, { 13, 3 } },
          { { 0, 2 }, { 3, 3 }, { 7, 1 }, { 8, 2 }, { 11, 3 } },
          { { 1, 2 }, { 3, 1 }, { 6, 2 }, { 9, 1 }, { 10, 4 }, { 14, 2 } } },
        { { { 0, 3 }, { 3, 3 }, { 7, 2 }, { 10, 3 }, { 13, 3 } }, { { 0, 2 }, { 3, 1 }, { 6, 2 }, { 10, 2 }, { 12, 2 } } },
        4.6, .7, .3, .35, .7, .3, .6, .4, .15,
        { 2, 1, 1, 3, 2 },
        { "SOSC", "SDSC", "SOQC" },
        { .28, { 3, 4, 8, 9 }, .15, 2 },
        { 3.0, 7.0, .4, .8, .12, .5, .55 }, 62,
    };
    return e;
}

const Engine& orchestral() {
    static const Engine e {
        "ORQUESTRAL", "broad cinematic theme: long singing notes, stepwise climb to one big peak, octaves and sixths underneath",
        { S::naturalMinor, S::harmonicMinor, S::major, S::naturalMinor },
        { { { 0, 5, 2, 6 }, { tr, tr, tr, tr } }, { { 0, 3, 5, 4 }, { tr, tr, a9, tr } },
          { { 0, 5, 3, 4 }, { tr, a9, tr, tr } }, { { 5, 3, 0, 6 }, { tr, tr, tr, tr } } },
        { { { 0, 4, 5, 3 }, { tr, tr, tr, a9 } }, { { 3, 0, 4, 5 }, { a9, tr, su2, tr } }, { { 0, 5, 3, 4 }, { tr, tr, a9, tr } } },
        { { 0, H, 0, H }, { 0, 0, 0, 0 }, { 0, 0, 8, 0 } },
        { { { 0, 8 }, { 8, 8 } },
          { { 0, 6 }, { 6, 2 }, { 8, 8 } },
          { { 0, 4 }, { 4, 4 }, { 8, 8 } },
          { { 0, 12 }, { 12, 2 }, { 14, 2 } },
          { { 0, 2 }, { 2, 2 }, { 4, 12 } } },
        { { { 0, 16 } }, { { 0, 8 }, { 8, 8 } } },
        2.6, .12, .1, .2, .7, .4, .95, .55, .25,
        { 4, 3, 1, 1, 1 },
        { "SQXC", "SDXC", "SQDC" },
        { .45, { 12, 8, 9, 7 }, .3, 3 },
        { 1.5, 4.5, .0, .3, .02, .3, .4 }, 56,
    };
    return e;
}

const Engine& drill() {
    static const Engine e {
        "DRILL", "dark sliding minor motif: off-beat stabs, gaps that leave room for the 808, a half-step that bites",
        { S::harmonicMinor, S::phrygian, S::naturalMinor },
        { { { 0, 5, 0, 5 }, { tr, tr, tr, tr } }, { { 0, 3, 0, 3 }, { tr, tr, tr, tr } },
          { { 0, 3, 5, 4 }, { tr, tr, tr, tr } }, { { 0, 0, 5, 3 }, { tr, tr, tr, tr } } },
        { { { 5, 4, 5, 4 }, { tr, tr, tr, tr } }, { { 5, 3, 4, 4 }, { tr, tr, tr, tr } } },
        { { 0, H, 0, H }, { 0, H, 0, 8 } },
        { { { 0, 2 }, { 3, 2 }, { 6, 1 }, { 9, 3 }, { 14, 2 } },
          { { 0, 3 }, { 4, 1 }, { 6, 2 }, { 10, 4 } },
          { { 2, 2 }, { 5, 1 }, { 6, 3 }, { 11, 1 }, { 12, 3 } },
          { { 0, 1 }, { 1, 1 }, { 2, 2 }, { 6, 2 }, { 9, 1 }, { 10, 3 } },
          { { 0, 4 }, { 6, 2 }, { 8, 1 }, { 11, 3 } } },
        { { { 0, 16 } }, { { 0, 6 }, { 6, 10 } } },
        4.0, .7, .45, .5, .85, .35, .45, .3, .2,
        { 1, 1, 3, 2, 2 },
        { "SSDC", "SOSC", "SQSC" },
        { .15, { 12 }, .1, 2 },
        { 2.5, 6.5, .35, .8, .25, .7, .6 }, 60,
    };
    return e;
}

const Engine& mpb() {
    static const Engine e {
        "MPB", "bossa-flavoured singing line: syncopated phrases that float over the beat, chromatic approaches, rich seventh chords",
        { S::major, S::dorian, S::mixolydian, S::naturalMinor },
        { { { 0, 3, 5, 4 }, { s7, s7, s7, s7 } }, { { 0, 3, 0, 4 }, { n9, s7, s7, s7 } }, { { 0, 5, 3, 4 }, { s7, s7, s7, s7 } } },
        { { { 1, 4, 0, 5 }, { s7, s7, s7, s7 } }, { { 0, 5, 1, 4 }, { s7, s7, s7, s7 } },
          { { 0, 1, 2, 1 }, { n9, s7, s7, s7 } }, { { 3, 2, 1, 4 }, { s7, s7, s7, s7 } } },
        { { 0, -2, 0, -2 }, { 0, 0, 0, 0 }, { 0, -1, 0, 0 } },
        { { { 0, 3 }, { 3, 3 }, { 6, 4 }, { 10, 2 }, { 12, 4 } },
          { { 2, 2 }, { 4, 3 }, { 7, 3 }, { 10, 6 } },
          { { 0, 2 }, { 2, 1 }, { 3, 3 }, { 6, 2 }, { 8, 3 }, { 11, 5 } },
          { { 1, 2 }, { 3, 4 }, { 8, 2 }, { 10, 2 }, { 12, 4 } } },
        { { { 0, 3 }, { 3, 3 }, { 6, 4 }, { 10, 3 }, { 13, 3 } }, { { 0, 2 }, { 3, 2 }, { 6, 2 }, { 10, 2 }, { 13, 3 } } },
        4.4, .65, .3, .3, .6, .4, .8, .45, .4,
        { 3, 1, 2, 3, 1 },
        { "SQDC", "SOSC", "SDOC" },
        { .3, { 3, 4, 8, 9 }, .2, 2 },
        { 2.5, 6.5, .35, .8, .08, .45, .35 }, 56,
    };
    return e;
}

const Engine& lofi() {
    static const Engine e {
        "LO-FI", "lazy laid-back phrases behind the beat, lots of air, ninth chords, a sigh at the end of the loop",
        { S::dorian, S::naturalMinor, S::major },
        { { { 0, 3, 6, 2 }, { n9, s7, s7, s7 } }, { { 0, 5, 3, 4 }, { s7, s7, n9, s7 } }, { { 0, 3, 0, 4 }, { n9, n9, s7, s7 } } },
        { { { 1, 4, 0, 5 }, { s7, s7, n9, s7 } }, { { 3, 2, 1, 0 }, { s7, s7, s7, n9 } }, { { 3, 4, 2, 5 }, { n9, s7, s7, s7 } } },
        { { 0, 0, 0, 0 }, { 0, -1, 0, -1 }, { 0, H, 0, H } },
        { { { 2, 4 }, { 8, 2 }, { 10, 6 } },
          { { 0, 6 }, { 7, 1 }, { 8, 4 } },
          { { 3, 3 }, { 6, 2 }, { 12, 4 } },
          { { 0, 2 }, { 2, 2 }, { 4, 6 }, { 14, 2 } } },
        { { { 0, 12 }, { 12, 4 } }, { { 0, 16 } }, { { 0, 6 }, { 6, 10 } } },
        3.0, .5, .2, .6, .7, .35, .85, .5, .3,
        { 2, 1, 3, 2, 1 },
        { "SOSC", "SDOC", "SQFC" },
        { .35, { 3, 4, 8, 9 }, .3, 3 },
        { 1.5, 5.0, .2, .65, .25, .7, .45 }, 52,
    };
    return e;
}
}

const Engine& get(Style s) {
    switch (s) {
        case Style::funkBR: return funk();
        case Style::trap: return trap();
        case Style::rnb: return rnb();
        case Style::reggaeton: return reggaeton();
        case Style::house: return house();
        case Style::afrobeats: return afrobeats();
        case Style::orchestral: return orchestral();
        case Style::drill: return drill();
        case Style::mpb: return mpb();
        case Style::lofi: return lofi();
        default: return pop();
    }
}
}
