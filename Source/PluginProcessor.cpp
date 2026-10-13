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
void MelodyCodeProcessor::newMelody() { publish(engine.newMelody(shown, lockArray())); }
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
    return "#" + juce::String((int) history.size() - i) + "  " + mc::styleName(p.settings.style) + "  " + names.joinIntoString(" - ");
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
    if (resetPosition.exchange(false)) { position = 0.0; synth.allNotesOff(); }
    double tempo = bpm;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition(); pos && pos->getIsPlaying())
            if (auto b = pos->getBpm()) tempo = *b;
    synth.setPreset(preset);
    synth.setLevels(subLevel, melodyLevel);
    synth.setDelayTime(60.0 / tempo * 0.75);
    const double samplesPerTick = sr * 60.0 / tempo / mc::ppq;

    struct Ev { int offset; mc::Note note; bool sub; };
    std::vector<Ev> events;
    if (const int a = pendingAudition.exchange(-1); a >= 0)
        events.push_back({0, {0, (int) (mc::step * 2), a, 100, false}, false});

    if (playing) {
        const juce::SpinLock::ScopedTryLockType sl(patternLock);
        if (sl.isLocked()) {
            const double len = audioPattern.lengthTicks();
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
    auto* l = buffer.getWritePointer(0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : l;
    int done = 0;
    for (const auto& e : events) {
        const int at = juce::jlimit(0, n, e.offset);
        if (at > done) { synth.render(l + done, r + done, at - done); done = at; }
        synth.noteOn(e.note.pitch, e.note.velocity, (int) (e.note.length * samplesPerTick), e.note.glide, e.sub);
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
    root.setAttribute("bpm", bpm); root.setAttribute("preset", preset);
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
    mc::Pattern p;
    p.settings = s;
    p.progression.settings = mc::harmonySettings(s);
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
