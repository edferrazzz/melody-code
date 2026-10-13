#include "PluginProcessor.h"
#include "PluginEditor.h"

MelodyCodeProcessor::MelodyCodeProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    publish(engine.generate(current));
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

std::array<bool, mc::maxBars> MelodyCodeProcessor::lockArray() const {
    std::array<bool, mc::maxBars> l {};
    for (int i = 0; i < shown.bars(); ++i) l[(size_t) i] = locks[(size_t) i];
    return l;
}

void MelodyCodeProcessor::publish(mc::Pattern p, bool addHistory) {
    current = p.settings;
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

void MelodyCodeProcessor::generateAll() {
    const bool same = shown.bars() == current.bars;
    publish(engine.generate(current, same ? &shown : nullptr, same ? lockArray() : std::array<bool, mc::maxBars> {}));
}
void MelodyCodeProcessor::applySettings() {
    if (shown.bars() != current.bars) locks = {};
    generateAll();
}
void MelodyCodeProcessor::newMelody() { publish(engine.newMelody(shown, current, lockArray())); }
void MelodyCodeProcessor::newBass() { publish(engine.newBass(shown)); }
void MelodyCodeProcessor::variation() { publish(engine.variation(shown, lockArray())); }
void MelodyCodeProcessor::setChord(int bar, const ccp::Chord& c) { publish(engine.setChord(shown, bar, c)); }
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
    return "#" + juce::String((int) history.size() - i) + "  " + juce::String::fromUTF8(mc::styleName(p.style)) + " / " + juce::String::fromUTF8(mc::vibeName(p.vibe)) + "  " + names.joinIntoString(" - ");
}
void MelodyCodeProcessor::restoreHistory(int i) {
    if (i >= 0 && i < (int) history.size()) { locks = {}; publish(history[(size_t) i], false); }
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
    synth.setLevels(subLevel, melodyLevel);
    synth.setDelayTime(60.0 / tempo * 0.75);

    struct Ev { int offset; mc::Note note; bool sub; };
    std::vector<Ev> events;
    if (const int a = pendingAudition.exchange(-1); a >= 0)
        events.push_back({0, {0, (int) (mc::step * 2), a, 100, false}, false});

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
        synth.noteOn(e.note.pitch, e.note.velocity, lenSamples, e.note.glide, e.sub);
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
    const auto& s = shown.settings;
    root.setAttribute("key", s.key); root.setAttribute("scale", (int) s.scale); root.setAttribute("style", (int) s.style);
    root.setAttribute("vibe", (int) s.vibe); root.setAttribute("role", (int) s.role); root.setAttribute("density", (int) s.density);
    root.setAttribute("range", (int) s.range); root.setAttribute("bars", s.bars);
    root.setAttribute("bpm", bpm); root.setAttribute("sync", syncToHost.load()); root.setAttribute("midiOut", midiOut.load());
    root.setAttribute("resolvedStyle", (int) shown.style); root.setAttribute("resolvedVibe", (int) shown.vibe); root.setAttribute("preset", preset);
    root.setAttribute("subLevel", subLevel); root.setAttribute("melodyLevel", melodyLevel);
    for (int i = 0; i < shown.bars(); ++i) {
        const auto& c = shown.progression.chords[(size_t) i];
        auto* x = root.createNewChildElement("chord");
        x->setAttribute("degree", c.degree); x->setAttribute("kind", (int) c.kind); x->setAttribute("root", c.root); x->setAttribute("quality", c.quality);
    }
    for (int t = 0; t < 2; ++t)
        for (const auto& n : t ? shown.bass : shown.melody) {
            auto* x = root.createNewChildElement(t ? "bass" : "note");
            x->setAttribute("start", n.start); x->setAttribute("length", n.length); x->setAttribute("pitch", n.pitch);
            x->setAttribute("velocity", n.velocity); x->setAttribute("glide", n.glide);
        }
    copyXmlToBinary(root, dest);
}

void MelodyCodeProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("MelodyCode")) return;
    mc::Settings s;
    s.key = xml->getIntAttribute("key", 9); s.scale = (ccp::ScaleType) xml->getIntAttribute("scale", 1);
    s.style = (mc::Style) xml->getIntAttribute("style"); s.vibe = (mc::Vibe) xml->getIntAttribute("vibe");
    s.role = (mc::Role) xml->getIntAttribute("role"); s.density = (mc::Density) xml->getIntAttribute("density", 1);
    s.range = (mc::Range) xml->getIntAttribute("range", 1); s.bars = xml->getIntAttribute("bars", 4);
    bpm = xml->getIntAttribute("bpm", 120); preset = xml->getIntAttribute("preset");
    subLevel = (float) xml->getDoubleAttribute("subLevel", 0.8); melodyLevel = (float) xml->getDoubleAttribute("melodyLevel", 0.8);
    syncToHost = xml->getBoolAttribute("sync", true); midiOut = xml->getBoolAttribute("midiOut", true);
    auto inRange = [](int v, int count, int fallback) { return v > 0 && v < count ? v : fallback; };
    if ((int) s.style < 0 || s.style >= mc::Style::count) s.style = mc::Style::automatic;
    if ((int) s.vibe < 0 || s.vibe >= mc::Vibe::count) s.vibe = mc::Vibe::automatic;
    mc::Pattern p;
    p.settings = s;
    p.style = (mc::Style) inRange(xml->getIntAttribute("resolvedStyle"), (int) mc::Style::count, s.style == mc::Style::automatic ? (int) mc::Style::pop : (int) s.style);
    p.vibe = (mc::Vibe) inRange(xml->getIntAttribute("resolvedVibe"), (int) mc::Vibe::count, s.vibe == mc::Vibe::automatic ? (int) mc::Vibe::chill : (int) s.vibe);
    p.progression.settings = mc::harmonySettings(p.style, p.vibe, s);
    int count = 0;
    const ccp::Chord* prev = nullptr;
    for (auto* x : xml->getChildWithTagNameIterator("chord")) {
        if (count >= ccp::maxChords) break;
        ccp::Chord c; c.degree = x->getIntAttribute("degree"); c.kind = (ccp::ChordKind) x->getIntAttribute("kind");
        c.root = x->getIntAttribute("root"); c.quality = x->getIntAttribute("quality", -1);
        p.progression.chords[(size_t) count] = ccp::HarmonyEngine::rebuild(p.progression.settings, c, prev, count);
        prev = &p.progression.chords[(size_t) count];
        ++count;
    }
    if (count != s.bars) return;
    p.progression.chordCount = count;
    for (int t = 0; t < 2; ++t)
        for (auto* x : xml->getChildWithTagNameIterator(t ? "bass" : "note"))
            (t ? p.bass : p.melody).push_back({ x->getIntAttribute("start"), x->getIntAttribute("length"), x->getIntAttribute("pitch"),
                                                x->getIntAttribute("velocity", 100), x->getBoolAttribute("glide") });
    publish(std::move(p));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MelodyCodeProcessor(); }
