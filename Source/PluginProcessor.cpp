#include "PluginProcessor.h"
#include "PluginEditor.h"

MelodyCodeProcessor::MelodyCodeProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    publish(engine.generate(current));
}

namespace {
// The Vibe is the sound's aesthetic: tone, space, saturation and tape. It never touches the notes.
Synth::Fx fxFor(mc::Vibe v) {
    Synth::Fx f;
    switch (v) {
        case mc::Vibe::clean:   // close and clear: a small room, a light echo
            f.lowpass = 16000; f.chorus = 0.15f; f.delayMix = 0.08f; f.delayBeats = 0.75f; f.delayFeedback = 0.25f;
            f.reverbMix = 0.18f; f.reverbTime = 1.1f; f.reverbDamp = 0.35f; f.preDelayMs = 8; break;
        case mc::Vibe::dreamy:  // wide and soft: chorus, shimmer, a long bright hall
            f.lowpass = 11000; f.chorus = 0.55f; f.octaveUp = 0.35f; f.delayMix = 0.18f; f.delayBeats = 0.75f; f.delayFeedback = 0.4f;
            f.reverbMix = 0.45f; f.reverbTime = 4.0f; f.reverbDamp = 0.25f; f.preDelayMs = 30; break;
        case mc::Vibe::dark:    // filtered and heavy: low-pass, warm drive, a dark long space
            f.lowpass = 2600; f.highpass = 40; f.drive = 0.35f; f.chorus = 0.2f; f.delayMix = 0.14f; f.delayBeats = 1.0f; f.delayFeedback = 0.35f;
            f.reverbMix = 0.35f; f.reverbTime = 3.5f; f.reverbDamp = 0.7f; f.preDelayMs = 20; break;
        case mc::Vibe::tape:    // lo-fi: wow and flutter, reduced bits, rolled-off highs and lows, saturation
            f.lowpass = 4800; f.highpass = 160; f.drive = 0.3f; f.tape = 0.7f; f.chorus = 0.1f; f.delayMix = 0.1f; f.delayBeats = 0.5f; f.delayFeedback = 0.3f;
            f.reverbMix = 0.22f; f.reverbTime = 1.8f; f.reverbDamp = 0.6f; f.preDelayMs = 12; break;
        case mc::Vibe::space:   // huge: ping-pong echoes and a very long reverb
            f.lowpass = 13000; f.chorus = 0.35f; f.octaveUp = 0.15f; f.delayMix = 0.34f; f.delayBeats = 0.75f; f.delayFeedback = 0.5f;
            f.reverbMix = 0.5f; f.reverbTime = 6.5f; f.reverbDamp = 0.35f; f.preDelayMs = 45; break;
        default: break;
    }
    return f;
}
const char* keyNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

bool MelodyCodeProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MelodyCodeProcessor::prepareToPlay(double rate, int) {
    sr = rate;
    synth.prepare(rate);
    midiOffs.clear();
    midiOffs.reserve(256);
}

mc::Pattern MelodyCodeProcessor::pattern() const { return shown; }


void MelodyCodeProcessor::publish(mc::Pattern p, bool addHistory) {
    current = p.settings;
    soundVibe = (int) p.settings.vibe;
    shown = p;
    {
        const juce::SpinLock::ScopedLockType sl(patternLock);
        audioPattern = std::move(p);
    }
    if (addHistory) {
        history.insert(history.begin(), shown);
        if (history.size() > 30) history.pop_back();
    }
    sendChangeMessage();
}

void MelodyCodeProcessor::generateAll() { publish(engine.generate(current)); }
void MelodyCodeProcessor::applySettings() { generateAll(); }
void MelodyCodeProcessor::newMelody() { publish(engine.newMelody(shown, current)); }
void MelodyCodeProcessor::setVibe(mc::Vibe v) {
    current.vibe = v;
    shown.settings.vibe = v;
    soundVibe = (int) v;
    sendChangeMessage();
}
void MelodyCodeProcessor::variation() { publish(engine.variation(shown)); }
void MelodyCodeProcessor::setMelody(std::vector<mc::Note> notes) {
    std::sort(notes.begin(), notes.end(), [](const mc::Note& a, const mc::Note& b) { return a.start < b.start; });
    auto p = shown;
    p.melody = std::move(notes);
    publish(std::move(p), false);
}

juce::String MelodyCodeProcessor::historyLabel(int i) const {
    const auto& p = history[(size_t) i];
    juce::StringArray names;
    for (int c = 0; c < p.bars(); ++c) names.add(p.progression.chords[(size_t) c].name);
    return "#" + juce::String((int) history.size() - i) + "  " + juce::String::fromUTF8(mc::styleName(p.style)) + "  " + keyNames[p.key] + " "
         + juce::String::fromUTF8(ccp::scaleName(p.scale)) + "  " + names.joinIntoString(" - ");
}
void MelodyCodeProcessor::restoreHistory(int i) {
    if (i >= 0 && i < (int) history.size()) { auto p = history[(size_t) i]; p.settings.vibe = current.vibe; publish(std::move(p), false); }
}

void MelodyCodeProcessor::setPlaying(bool on) {
    resetPosition = true;
    playing = on;
    if (!on) playhead = -1.0;
    sendChangeMessage();
}

void MelodyCodeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();
    const int n = buffer.getNumSamples();
    const bool sendMidi = midiOut.load();
    if (resetPosition.exchange(false)) { position = 0.0; synth.allNotesOff(); }

    // transport: with sync on and the DAW playing, the loop follows the DAW's bar position and tempo
    double tempo = bpm;
    std::optional<double> hostPpq;
    bool host = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition(); pos && pos->getIsPlaying()) {
            if (auto b = pos->getBpm()) tempo = *b;
            if (syncToHost.load()) { host = true; if (auto q = pos->getPpqPosition()) hostPpq = *q; }
        }
    hostPlaying = host;
    const bool running = host || playing.load();
    const double samplesPerTick = sr * 60.0 / tempo / mc::ppq;

    auto flushOffs = [&](int at) {
        for (auto& o : midiOffs) midi.addEvent(juce::MidiMessage::noteOff(o.channel, o.pitch), at);
        midiOffs.clear();
    };
    if (wasRunning && !running) {
        synth.allNotesOff();
        flushOffs(0);
        for (int ch = 1; ch <= 2; ++ch) midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        playhead = -1.0;
    }
    wasRunning = running;

    synth.setPreset(preset);
    synth.setFx(fxFor((mc::Vibe) soundVibe.load()));
    synth.setLevels(bassLevel, melodyLevel);
    synth.setTempo(tempo);

    struct Ev { int offset; mc::Note note; bool sub; };
    std::vector<Ev> events;
    if (const int a = pendingAudition.exchange(-1); a >= 0)
        events.push_back({0, {0, (int) (mc::step * 2), a, 100}, false});

    if (running) {
        const juce::SpinLock::ScopedTryLockType sl(patternLock);
        if (sl.isLocked() && audioPattern.lengthTicks() > 0) {
            const double len = audioPattern.lengthTicks();
            if (host && hostPpq) {
                const double t = std::fmod(*hostPpq * mc::ppq, len);
                position = t < 0 ? t + len : t;
            }
            const double from = position, to = position + n / samplesPerTick;
            auto collect = [&](const std::vector<mc::Note>& notes, bool sub) {
                for (const auto& note : notes)
                    for (double base : { 0.0, len }) {
                        const double t = note.start + base;
                        if (t >= from && t < to)
                            events.push_back({ (int) ((t - from) * samplesPerTick), note, sub });
                    }
            };
            collect(audioPattern.bass, true);
            collect(audioPattern.melody, false);
            position = to;
            if (position >= len) position -= len;
            playhead = position;
        }
    }
    std::sort(events.begin(), events.end(), [](const Ev& a, const Ev& b) { return a.offset < b.offset; });

    // MIDI out: note-offs that fall in this block first, then the new notes
    if (sendMidi) {
        for (auto it = midiOffs.begin(); it != midiOffs.end();) {
            if (it->samplesLeft < n) { midi.addEvent(juce::MidiMessage::noteOff(it->channel, it->pitch), juce::jmax(0, (int) it->samplesLeft)); it = midiOffs.erase(it); }
            else { it->samplesLeft -= n; ++it; }
        }
    } else if (!midiOffs.empty()) flushOffs(0);

    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : l;
    int done = 0;
    for (const auto& e : events) {
        const int at = juce::jlimit(0, n - 1, e.offset);
        if (at > done) { synth.render(l + done, r + done, at - done); done = at; }
        const int lenSamples = (int) (e.note.length * samplesPerTick);
        synth.noteOn(e.note.pitch, e.note.velocity, lenSamples, e.sub);
        if (sendMidi && running) {
            const int ch = e.sub ? 2 : 1;
            for (auto it = midiOffs.begin(); it != midiOffs.end(); ++it)   // retrigger: close the old note first
                if (it->pitch == e.note.pitch && it->channel == ch) { midi.addEvent(juce::MidiMessage::noteOff(ch, it->pitch), at); midiOffs.erase(it); break; }
            midi.addEvent(juce::MidiMessage::noteOn(ch, e.note.pitch, (juce::uint8) juce::jlimit(1, 127, e.note.velocity)), at);
            const double end = at + juce::jmax(1, lenSamples);
            if (end < n) midi.addEvent(juce::MidiMessage::noteOff(ch, e.note.pitch), (int) end);
            else midiOffs.push_back({ e.note.pitch, ch, end - n });
        }
    }
    if (done < n) synth.render(l + done, r + done, n - done);
}

juce::AudioProcessorEditor* MelodyCodeProcessor::createEditor() { return new MelodyCodeEditor(*this); }

void MelodyCodeProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::XmlElement root("MelodyCode");
    const auto& s = current;
    root.setAttribute("version", 3);
    root.setAttribute("key", s.key); root.setAttribute("scale", s.scale); root.setAttribute("style", (int) s.style);
    root.setAttribute("vibe", (int) s.vibe); root.setAttribute("feel", (int) s.feel); root.setAttribute("role", (int) s.role);
    root.setAttribute("density", (int) s.density); root.setAttribute("range", (int) s.range); root.setAttribute("bars", s.bars);
    root.setAttribute("bpm", bpm); root.setAttribute("sync", syncToHost.load()); root.setAttribute("midiOut", midiOut.load());
    root.setAttribute("resolvedStyle", (int) shown.style); root.setAttribute("resolvedKey", shown.key); root.setAttribute("resolvedScale", (int) shown.scale);
    root.setAttribute("preset", preset); root.setAttribute("bassLevel", bassLevel); root.setAttribute("melodyLevel", melodyLevel);
    for (int i = 0; i < shown.bars(); ++i) {
        const auto& c = shown.progression.chords[(size_t) i];
        auto* x = root.createNewChildElement("chord");
        x->setAttribute("shift", shown.chordShift[(size_t) i]); x->setAttribute("degree", c.degree); x->setAttribute("kind", (int) c.kind); x->setAttribute("root", c.root); x->setAttribute("quality", c.quality);
    }
    for (int t = 0; t < 2; ++t)
        for (const auto& n : t ? shown.bass : shown.melody) {
            auto* x = root.createNewChildElement(t ? "bass" : "note");
            x->setAttribute("start", n.start); x->setAttribute("length", n.length); x->setAttribute("pitch", n.pitch); x->setAttribute("velocity", n.velocity);
        }
    copyXmlToBinary(root, dest);
}

void MelodyCodeProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("MelodyCode") || xml->getIntAttribute("version") < 3) return;   // older sessions: start fresh
    auto clampEnum = [&](const char* name, int count, int fallback) { const int v = xml->getIntAttribute(name, fallback); return v >= 0 && v < count ? v : fallback; };
    mc::Settings s;
    s.key = juce::jlimit(-1, 11, xml->getIntAttribute("key", -1));
    s.scale = juce::jlimit(-1, (int) ccp::ScaleType::count - 1, xml->getIntAttribute("scale", -1));
    s.style = (mc::Style) clampEnum("style", (int) mc::Style::count, 0);
    s.vibe = (mc::Vibe) clampEnum("vibe", (int) mc::Vibe::count, 1);
    s.feel = (mc::Feel) clampEnum("feel", (int) mc::Feel::count, 0);
    s.role = (mc::Role) clampEnum("role", (int) mc::Role::count, 0);
    s.density = (mc::Density) clampEnum("density", (int) mc::Density::count, 1);
    s.range = (mc::Range) clampEnum("range", (int) mc::Range::count, 1);
    s.bars = xml->getIntAttribute("bars", 4);
    if (s.bars != 2 && s.bars != 4 && s.bars != 8) s.bars = 4;
    bpm = juce::jlimit(60, 180, xml->getIntAttribute("bpm", 120)); preset = clampEnum("preset", Synth::presetCount, 0);
    bassLevel = (float) xml->getDoubleAttribute("bassLevel", 0.7); melodyLevel = (float) xml->getDoubleAttribute("melodyLevel", 0.8);
    syncToHost = xml->getBoolAttribute("sync", true); midiOut = xml->getBoolAttribute("midiOut", true);
    mc::Pattern p;
    p.settings = s;
    p.style = (mc::Style) juce::jlimit(1, (int) mc::Style::count - 1, xml->getIntAttribute("resolvedStyle", 2));
    p.key = juce::jlimit(0, 11, xml->getIntAttribute("resolvedKey", 9));
    p.scale = (ccp::ScaleType) clampEnum("resolvedScale", (int) ccp::ScaleType::count, 1);
    p.progression.settings = mc::harmonySettings(p.style, p.key, p.scale);
    int count = 0;
    const ccp::Chord* prev = nullptr;
    for (auto* x : xml->getChildWithTagNameIterator("chord")) {
        if (count >= ccp::maxChords) break;
        ccp::Chord c; c.degree = x->getIntAttribute("degree"); c.kind = (ccp::ChordKind) x->getIntAttribute("kind");
        c.root = x->getIntAttribute("root"); c.quality = x->getIntAttribute("quality", -1);
        p.progression.chords[(size_t) count] = ccp::HarmonyEngine::rebuild(p.progression.settings, c, prev, count);
        prev = &p.progression.chords[(size_t) count];
        p.chordShift[(size_t) count] = juce::jlimit(-8, 8, x->getIntAttribute("shift"));
        ++count;
    }
    if (count != s.bars) return;
    p.progression.chordCount = count;
    for (int t = 0; t < 2; ++t)
        for (auto* x : xml->getChildWithTagNameIterator(t ? "bass" : "note"))
            (t ? p.bass : p.melody).push_back({ x->getIntAttribute("start"), x->getIntAttribute("length"), x->getIntAttribute("pitch"), x->getIntAttribute("velocity", 100) });
    publish(std::move(p));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MelodyCodeProcessor(); }
