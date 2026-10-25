#include "Sampler.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <BinaryData.h>
#include <cmath>
#include <cstring>

struct Sampler::Zone {
    int root {}, layer {}, loopStart {}, loopEnd {};
    std::vector<float> data;   // mono, 44.1 kHz
};

namespace {
struct Entry { const char* inst; int root, layer, loopStart, loopEnd; const char* resource; };
const Entry table[] {
#include "SampleTable.inc"
};
const char* instNames[] { "piano", "nylon", "guitar", "ebass", "harp", "violin", "cello" };

// per instrument: lowest note it plays (lower notes fold up an octave), attack and release seconds, level
struct Shape { int lowest; float attack, release, level; };
const Shape shapes[] {
    { 21, 0.002f, 0.45f, 1.00f },   // piano
    { 40, 0.002f, 0.30f, 1.05f },   // nylon
    { 40, 0.002f, 0.35f, 0.85f },   // guitar
    { 28, 0.003f, 0.20f, 1.10f },   // ebass
    { 24, 0.002f, 0.70f, 0.95f },   // harp
    { 55, 0.120f, 0.40f, 0.60f },   // violin
    { 36, 0.040f, 0.25f, 0.95f },   // cello
};

struct Bank {
    std::array<std::vector<Sampler::Zone*>, Sampler::instCount> zones;
    std::vector<std::unique_ptr<Sampler::Zone>> owned;
    Bank() {
        juce::OggVorbisAudioFormat ogg;
        for (const auto& e : table) {
            int size = 0;
            const char* data = BinaryData::getNamedResource(e.resource, size);
            if (!data) continue;
            std::unique_ptr<juce::AudioFormatReader> reader(ogg.createReaderFor(new juce::MemoryInputStream(data, (size_t) size, false), true));
            if (!reader) continue;
            auto z = std::make_unique<Sampler::Zone>();
            z->root = e.root; z->layer = e.layer;
            juce::AudioBuffer<float> buf(1, (int) reader->lengthInSamples);
            reader->read(&buf, 0, buf.getNumSamples(), 0, true, false);
            z->data.assign(buf.getReadPointer(0), buf.getReadPointer(0) + buf.getNumSamples());
            z->loopEnd = juce::jmin(e.loopEnd, (int) z->data.size());
            z->loopStart = e.loopEnd > 0 && e.loopStart < z->loopEnd ? e.loopStart : 0;
            if (z->loopStart == 0) z->loopEnd = 0;
            for (int i = 0; i < Sampler::instCount; ++i)
                if (std::strcmp(e.inst, instNames[i]) == 0) zones[(size_t) i].push_back(z.get());
            owned.push_back(std::move(z));
        }
    }
    const Sampler::Zone* find(int inst, int pitch, int layer) const {
        const Sampler::Zone* best = nullptr;
        int bestD = 1 << 20;
        for (auto* z : zones[(size_t) inst]) {
            // nearest root; the right velocity layer wins; a root just below beats one just above (shifting up sounds better)
            const int d = std::abs(z->root - pitch) * 4 + (z->root > pitch ? 1 : 0) + (z->layer == layer ? 0 : 1000);
            if (d < bestD) { bestD = d; best = z; }
        }
        return best;
    }
};
const Bank& bank() { static Bank b; return b; }

// Names and descriptions are UTF-8 (shown in Portuguese). synth >= 0: a synthesized kit, played by Synth with that Tone preset.
const Sampler::Kit kits[Sampler::kitCount] {
    { "PIANO SOMBRIO",   "Piano de cauda suave e escuro nas tr\xc3\xaas camadas",              Sampler::piano, Sampler::piano,  Sampler::piano, true },
    { "VIOL\xc3\x83O",   "Melodia no viol\xc3\xa3o nylon, acordes no viol\xc3\xa3o a\xc3\xa7o, baixo el\xc3\xa9trico", Sampler::nylon, Sampler::guitar, Sampler::ebass, false },
    { "CL\xc3\x81SSICO", "Melodia na harpa, acordes no violino, grave no cello",             Sampler::harp,  Sampler::violin, Sampler::cello, false },
    { "PIANO & CORDAS",  "Melodia no piano, acordes no violino, grave no cello",             Sampler::piano, Sampler::violin, Sampler::cello, true },
    { "CORDAS",          "Melodia e acordes no violino, grave no cello",                     Sampler::violin, Sampler::violin, Sampler::cello, false },
    { "HARPA & PIANO",   "Melodia na harpa, acordes no piano, grave no cello",               Sampler::harp,  Sampler::piano,  Sampler::cello, true },
    { "AC\xc3\x9aSTICO", "Melodia no viol\xc3\xa3o a\xc3\xa7o, acordes no piano, baixo el\xc3\xa9trico", Sampler::guitar, Sampler::piano, Sampler::ebass, false },
    { "SYNTH KEYS",      "Piano el\xc3\xa9trico sintetizado, claro e com corpo",              Sampler::piano, Sampler::piano,  Sampler::piano, false, 0 },
    { "SYNTH PLUCK",     "Pluck de onda quadrada sintetizado, ataque definido",              Sampler::piano, Sampler::piano,  Sampler::piano, false, 3 },
};
}

const Sampler::Kit& Sampler::kit(int k) { return kits[k < 0 || k >= kitCount ? 0 : k]; }

void Sampler::prepare(double rate) {
    sr = rate;
    bank();   // decode once, off the audio thread
    for (auto& v : voices) v.active = false;
}

void Sampler::allNotesOff() {
    for (auto& v : voices) if (v.active) { v.releasing = true; v.rel = std::exp(-1.0f / (0.015f * (float) sr)); }
}

int Sampler::activeVoices() const {
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Sampler::noteOn(int pitch, int velocity, int lengthSamples, int layer) {
    const auto& K = kit(kitIndex);
    const int inst = layer == 1 ? K.bass : layer == 2 ? K.chords : K.melody;
    const auto& S = shapes[inst];
    while (pitch < S.lowest) pitch += 12;
    const float vel = juce::jlimit(0.0f, 1.0f, (float) velocity / 127.0f);
    const auto* z = bank().find(inst, pitch, inst == piano && vel > 0.72f ? 1 : 0);
    if (!z || z->data.empty()) return;

    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) { v = &voices[0]; for (auto& x : voices) if (x.env < v->env) v = &x; }
    // a repeated note on the same layer lets the old one go quickly (a real player re-strikes)
    for (auto& x : voices)
        if (x.active && !x.releasing && x.layer == layer && x.zone == z && &x != v) { x.releasing = true; x.rel = std::exp(-1.0f / (0.05f * (float) sr)); }

    v->active = true; v->releasing = false; v->zone = z; v->layer = layer;
    v->pos = 0.0;
    v->inc = std::pow(2.0, (pitch - z->root) / 12.0) * 44100.0 / sr;
    v->gain = S.level * (0.25f + 0.75f * std::pow(vel, 1.4f)) * (layer == 1 ? bassLevel : melodyLevel) * 0.55f;
    v->env = S.attack < 0.005f ? 1.0f : 0.0f;
    v->atk = 1.0f / juce::jmax(1.0f, S.attack * (float) sr);
    v->rel = std::exp(-1.0f / (S.release * (float) sr));
    v->left = juce::jmax(1, lengthSamples);
    // chords spread a little, melody centre, bass centre
    float pan = 0.0f;
    if (layer == 2) { panFlip ^= 1; pan = panFlip ? 0.22f : -0.22f; }
    v->panL = std::sqrt(0.5f * (1.0f - pan)); v->panR = std::sqrt(0.5f * (1.0f + pan));
    // the dark piano rolls the top off, harder notes open it a little
    const float cutoff = K.dark && inst == piano ? 1400.0f + 2600.0f * vel : 0.0f;
    v->lpK = cutoff > 0 ? 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff / (float) sr) : 1.0f;
    v->lp = 0.0f;
}

void Sampler::render(float& l, float& r) {
    for (auto& v : voices) {
        if (!v.active) continue;
        const auto& d = v.zone->data;
        const int i = (int) v.pos;
        if (i + 1 >= (int) d.size()) { v.active = false; continue; }
        const float f = (float) (v.pos - i);
        float x = d[(size_t) i] + (d[(size_t) i + 1] - d[(size_t) i]) * f;
        v.pos += v.inc;
        if (v.zone->loopEnd > 0 && v.pos >= v.zone->loopEnd) v.pos -= v.zone->loopEnd - v.zone->loopStart;

        if (!v.releasing) {
            if (v.env < 1.0f) v.env = juce::jmin(1.0f, v.env + v.atk);
            if (--v.left <= 0) v.releasing = true;
        } else {
            v.env *= v.rel;
            if (v.env < 0.0005f) { v.active = false; continue; }
        }
        v.lp += v.lpK * (x - v.lp);
        x = v.lp * v.env * v.gain;
        l += x * v.panL;
        r += x * v.panR;
    }
}
