#include "PluginEditor.h"
#include "EmbeddedFonts.h"

namespace {
const char* noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* roman[] { "I", "II", "III", "IV", "V", "VI", "VII" };
juce::String utf8(const char* s) { return juce::String::fromUTF8(s); }
bool isBlack(int m) { const int pc = m % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
}

namespace Theme {
juce::Font font(float h, bool bold) {
    static auto regular = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interRegular, EmbeddedFonts::interRegularSize);
    static auto heavy = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interBold, EmbeddedFonts::interBoldSize);
    return juce::Font(juce::FontOptions(bold ? heavy : regular).withHeight(h));
}
juce::ColourGradient accent(juce::Rectangle<float> r) {
    return juce::ColourGradient(acc, r.getX(), r.getY(), acc2, r.getRight(), r.getBottom(), false);
}
}

// ---------------------------------------------------------------- look and feel
MelodyLook::MelodyLook() {
    using namespace Theme;
    setColour(juce::ComboBox::textColourId, fg);
    setColour(juce::ComboBox::backgroundColourId, panel2);
    setColour(juce::ComboBox::outlineColourId, line);
    setColour(juce::ComboBox::arrowColourId, mute);
    setColour(juce::PopupMenu::backgroundColourId, panel2);
    setColour(juce::PopupMenu::textColourId, fg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, acc.withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, acc);
    setColour(juce::PopupMenu::headerTextColourId, mute);
    setColour(juce::Slider::textBoxTextColourId, fg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void MelodyLook::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    const bool small = h < 34;
    g.setColour(small ? juce::Colour(0xaa000000) : Theme::panel2);
    g.fillRoundedRectangle(r, small ? 7.0f : 10.0f);
    g.setColour(box.hasKeyboardFocus(false) || box.isMouseOver(true) ? Theme::acc.withAlpha(0.6f) : Theme::line);
    g.drawRoundedRectangle(r, small ? 7.0f : 10.0f, 1.0f);
    juce::Path arrow;
    const float cx = (float) w - (small ? 13.0f : 20.0f), cy = (float) h * 0.5f;
    arrow.startNewSubPath(cx - 5, cy - 2.5f); arrow.lineTo(cx, cy + 2.5f); arrow.lineTo(cx + 5, cy - 2.5f);
    g.setColour(Theme::mute);
    g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MelodyLook::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    const bool small = box.getHeight() < 34;
    label.setBounds(small ? 6 : 12, 1, box.getWidth() - (small ? 26 : 40), box.getHeight() - 2);
    label.setFont(small ? Theme::font(13.0f, true) : Theme::font(16.0f, true));
}

void MelodyLook::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(Theme::panel2);
    g.setColour(Theme::line);
    g.drawRect(0, 0, w, h);
}

void MelodyLook::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                  juce::Slider::SliderStyle, juce::Slider&) {
    const float cy = (float) y + (float) h * 0.5f;
    auto track = juce::Rectangle<float>((float) x, cy - 3, (float) w, 6);
    g.setColour(Theme::line);
    g.fillRoundedRectangle(track, 3);
    auto fill = track.withRight(pos);
    g.setGradientFill(Theme::accent(track));
    g.fillRoundedRectangle(fill, 3);
    g.setColour(Theme::fg);
    g.fillEllipse(pos - 8, cy - 8, 16, 16);
    g.setColour(Theme::acc);
    g.drawEllipse(pos - 8, cy - 8, 16, 16, 2);
}

// ---------------------------------------------------------------- small widgets
Segmented::Segmented(juce::StringArray o) : options(std::move(o)) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

void Segmented::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0b0e15));
    g.fillRoundedRectangle(r, 9);
    const float w = (r.getWidth() - 6) / (float) options.size();
    for (int i = 0; i < options.size(); ++i) {
        auto c = juce::Rectangle<float>(3 + w * (float) i, 3, w, r.getHeight() - 6);
        if (i == selected) {
            g.setGradientFill(Theme::accent(c));
            g.fillRoundedRectangle(c, 7);
        }
        g.setColour(i == selected ? Theme::ink : Theme::mute);
        g.setFont(Theme::font(12.5f, true).withExtraKerningFactor(0.08f));
        g.drawText(options[i], c, juce::Justification::centred);
    }
}

void Segmented::mouseDown(const juce::MouseEvent& e) {
    const int i = juce::jlimit(0, options.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) options.size()));
    if (i != selected) { selected = i; repaint(); if (onChange) onChange(i); }
}

void ActionButton::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    if (isAccent) {
        juce::Path p; p.addRoundedRectangle(r, 12);
        juce::DropShadow(Theme::acc.withAlpha(over ? 0.45f : 0.3f), 22, {}).drawForPath(g, p);
        g.setGradientFill(Theme::accent(r));
        g.fillPath(p);
        if (down) { g.setColour(juce::Colours::black.withAlpha(0.15f)); g.fillPath(p); }
        g.setColour(Theme::ink);
        g.setFont(Theme::font(24.0f, true).withExtraKerningFactor(0.3f));
    } else {
        g.setColour(down ? Theme::panel : Theme::panel2);
        g.fillRoundedRectangle(r, 12);
        g.setColour(lit || over ? Theme::acc : Theme::line);
        g.drawRoundedRectangle(r, 12, lit ? 1.6f : 1.0f);
        g.setColour(lit ? Theme::acc : Theme::fg);
        g.setFont(Theme::font(13.0f, true).withExtraKerningFactor(0.1f));
    }
    g.drawFittedText(getButtonText(), getLocalBounds().reduced(6), juce::Justification::centred, 2);
}

void DragMidiButton::mouseDrag(const juce::MouseEvent& e) {
    if (e.getDistanceFromDragStart() < 6 || !files) return;
    if (auto list = files(); !list.isEmpty())
        juce::DragAndDropContainer::performExternalDragDropOfFiles(list, false, this);
}

void LockButton::paintButton(juce::Graphics& g, bool over, bool) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(locked ? Theme::acc : juce::Colour(0xaa000000));
    g.fillRoundedRectangle(r, 7);
    g.setColour(locked ? Theme::acc : (over ? Theme::acc.withAlpha(0.6f) : Theme::line));
    g.drawRoundedRectangle(r, 7, 1);
    const auto c = locked ? Theme::ink : Theme::mute;
    auto body = juce::Rectangle<float>(0, 0, 12, 9).withCentre(r.getCentre().translated(0, 2.5f));
    g.setColour(c);
    g.fillRoundedRectangle(body, 2);
    juce::Path shackle;
    const float sx = body.getX() + 2.5f, ex = body.getRight() - 2.5f, top = body.getY() - 6;
    shackle.startNewSubPath(sx, body.getY());
    shackle.lineTo(sx, top + 3);
    shackle.quadraticTo(sx, top, (sx + ex) * 0.5f, top);
    shackle.quadraticTo(ex, top, ex, top + 3);
    if (locked) shackle.lineTo(ex, body.getY());
    g.strokePath(shackle, juce::PathStrokeType(1.8f));
}

// ---------------------------------------------------------------- piano roll
PianoRoll::PianoRoll(MelodyCodeProcessor& p) : proc(p) {
    setWantsKeyboardFocus(true);
    refresh();
}

juce::Rectangle<float> PianoRoll::noteArea() const {
    return getLocalBounds().toFloat().withTrimmedTop((float) headerH).withTrimmedBottom((float) subH).withTrimmedLeft((float) keyW);
}
float PianoRoll::stepW() const { return noteArea().getWidth() / (float) (pat.bars() * mc::stepsPerBar); }
float PianoRoll::rowH() const { return noteArea().getHeight() / (float) (hi - lo + 1); }
double PianoRoll::stepAt(float x) const { return (x - noteArea().getX()) / stepW(); }
int PianoRoll::pitchAt(float y) const { return hi - (int) std::floor((y - noteArea().getY()) / rowH()); }

juce::Rectangle<float> PianoRoll::noteRect(const mc::Note& n) const {
    const auto a = noteArea();
    const float x = a.getX() + (float) n.start / mc::step * stepW();
    const float w = juce::jmax(5.0f, (float) n.length / mc::step * stepW());
    return { x + 1, a.getY() + (float) (hi - n.pitch) * rowH() + 1, w - 2, rowH() - 2 };
}

PianoRoll::Hit PianoRoll::hit(juce::Point<float> p) const {
    for (int i = (int) notes.size() - 1; i >= 0; --i) {
        auto r = noteRect(notes[(size_t) i]).expanded(1, 0);
        if (r.contains(p)) return { i, p.x > r.getRight() - juce::jmin(8.0f, r.getWidth() * 0.35f) };
    }
    return {};
}

void PianoRoll::refresh() {
    pat = proc.pattern();
    if (drag.index < 0) notes = pat.melody;
    if (selected >= (int) notes.size()) selected = -1;
    if (!notes.empty() && drag.index < 0) {
        int mn = 127, mx = 0;
        for (auto& n : notes) { mn = std::min(mn, n.pitch); mx = std::max(mx, n.pitch); }
        lo = mn - 3; hi = mx + 3;
        if (hi - lo < 20) { const int extra = 20 - (hi - lo); lo -= extra / 2; hi += extra - extra / 2; }
    }
    rebuildChordBoxes();
    resized();
    repaint();
}

void PianoRoll::rebuildChordBoxes() {
    const int bars = pat.bars();
    while (chordBoxes.size() > bars) { chordBoxes.removeLast(); lockButtons.removeLast(); }
    while (chordBoxes.size() < bars) {
        const int bar = chordBoxes.size();
        auto* box = chordBoxes.add(new juce::ComboBox());
        box->setTooltip("Acorde do compasso " + juce::String(bar + 1));
        box->onChange = [this, bar, box] {
            const int id = box->getSelectedId();
            if (id >= 10 && bar < (int) choices.size() && id - 10 < (int) choices[(size_t) bar].size())
                proc.setChord(bar, choices[(size_t) bar][(size_t) (id - 10)]);
        };
        addAndMakeVisible(box);
        auto* lock = lockButtons.add(new LockButton());
        lock->onClick = [this, bar] { proc.toggleLock(bar); };
        addAndMakeVisible(lock);
    }
    const auto gs = mc::harmonySettings(pat.settings);
    choices.assign((size_t) bars, {});
    for (int b = 0; b < bars; ++b) {
        auto* box = chordBoxes[b];
        box->clear(juce::dontSendNotification);
        const auto& cur = pat.progression.chords[(size_t) b];
        box->addItem(juce::String(roman[cur.degree % 7]) + "  " + cur.name, 1);
        box->addSeparator();
        for (int d = 0; d < 7; ++d)
            for (auto kind : { ccp::ChordKind::triad, ccp::ChordKind::seventh, ccp::ChordKind::sus2 }) {
                auto c = ccp::HarmonyEngine::buildChord(gs, d, kind);
                choices[(size_t) b].push_back(c);
                box->addItem(juce::String(roman[d]) + "  " + c.name, 10 + (int) choices[(size_t) b].size() - 1);
            }
        box->setSelectedId(1, juce::dontSendNotification);
        lockButtons[b]->locked = proc.isLocked(b);
        lockButtons[b]->repaint();
    }
}

void PianoRoll::resized() {
    const auto a = noteArea();
    for (int b = 0; b < chordBoxes.size(); ++b) {
        const float x = a.getX() + (float) (b * mc::stepsPerBar) * stepW();
        const float w = (float) mc::stepsPerBar * stepW();
        const int boxW = (int) juce::jmin(150.0f, w - 44);
        chordBoxes[b]->setBounds((int) x + 6, 6, boxW, 26);
        lockButtons[b]->setBounds((int) (x + w) - 34, 6, 28, 26);
    }
}

void PianoRoll::paint(juce::Graphics& g) {
    auto all = getLocalBounds().toFloat();
    juce::Path clip; clip.addRoundedRectangle(all, 14);
    g.reduceClipRegion(clip);
    g.fillAll(juce::Colour(0xff0f121a));
    const auto a = noteArea();
    const float rh = rowH(), sw = stepW();
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.settings.key, pat.settings.scale);
    auto inScale = [&](int m) { return std::find(scale.begin(), scale.end(), m % 12) != scale.end(); };
    const int bars = pat.bars(), steps = bars * mc::stepsPerBar;

    // rows + keyboard
    for (int m = hi; m >= lo; --m) {
        const float y = a.getY() + (float) (hi - m) * rh;
        g.setColour(inScale(m) ? juce::Colour(0xff131826) : juce::Colour(0xff0c0f16));
        g.fillRect(a.getX(), y, a.getWidth(), rh);
        g.setColour(isBlack(m) ? juce::Colour(0xff1a1f2c) : juce::Colour(0xffd7dce8));
        g.fillRect(0.0f, y, (float) keyW - 2, rh - 1);
        if (m % 12 == 0) {
            g.setColour(Theme::quiet);
            g.setFont(Theme::font(10.0f));
            g.drawText("C" + juce::String(m / 12 - 1), juce::Rectangle<float>(0, y, (float) keyW - 6, rh), juce::Justification::centredRight);
        }
    }
    // chord tones per bar
    for (int b = 0; b < bars; ++b) {
        auto pcs = mc::Engine::chordPitchClasses(pat.progression.chords[(size_t) b]);
        for (int m = hi; m >= lo; --m)
            if (std::find(pcs.begin(), pcs.end(), m % 12) != pcs.end()) {
                g.setColour(Theme::acc.withAlpha(0.06f));
                g.fillRect(a.getX() + (float) (b * 16) * sw, a.getY() + (float) (hi - m) * rh, 16 * sw, rh);
            }
        if (proc.isLocked(b)) {
            g.setColour(Theme::acc.withAlpha(0.07f));
            g.fillRect(a.getX() + (float) (b * 16) * sw, 0.0f, 16 * sw, all.getHeight());
        }
    }
    // header + sub lane backgrounds
    g.setColour(juce::Colour(0xff0f121a));
    g.fillRect(0.0f, 0.0f, all.getWidth(), (float) headerH);
    const auto lane = juce::Rectangle<float>(a.getX(), a.getBottom(), a.getWidth(), (float) subH);
    g.setColour(juce::Colour(0xff0a0c12));
    g.fillRect(0.0f, lane.getY(), all.getWidth(), lane.getHeight());
    g.setColour(Theme::quiet);
    g.setFont(Theme::font(11.0f, true));
    g.drawText("SUB", juce::Rectangle<float>(0, lane.getY(), (float) keyW, lane.getHeight()), juce::Justification::centred);
    // grid
    for (int s = 0; s <= steps; ++s) {
        g.setColour(s % 16 == 0 ? juce::Colour(0xff3a4360) : s % 4 == 0 ? juce::Colour(0xff222838) : juce::Colour(0xff171b26));
        g.fillRect(a.getX() + (float) s * sw, s % 16 == 0 ? 0.0f : a.getY(), 1.0f, all.getHeight());
    }
    // sub (ghost)
    int subLo = 127, subHi = 0;
    for (auto& n : pat.bass) { subLo = std::min(subLo, n.pitch); subHi = std::max(subHi, n.pitch); }
    const float laneH = lane.getHeight() - 14;
    for (auto& n : pat.bass) {
        const float x = a.getX() + (float) n.start / mc::step * sw;
        const float w = juce::jmax(4.0f, (float) n.length / mc::step * sw - 2);
        const float rel = subHi > subLo ? (float) (n.pitch - subLo) / (float) (subHi - subLo) : 0.5f;
        auto r = juce::Rectangle<float>(x + 1, lane.getY() + 7 + (1.0f - rel) * (laneH - 12), w, 12);
        g.setColour(Theme::mute.withAlpha(0.18f));
        g.fillRoundedRectangle(r, 4);
        juce::Path outline; outline.addRoundedRectangle(r, 4);
        juce::Path dashed; const float dashes[] { 4, 3 };
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
        g.setColour(Theme::mute.withAlpha(0.5f));
        g.fillPath(dashed);
        if (n.start % (mc::stepsPerBar * mc::step) == 0 && w > 26) {
            g.setColour(Theme::mute);
            g.setFont(Theme::font(10.0f, true));
            g.drawText(juce::String(noteNames[n.pitch % 12]) + juce::String(n.pitch / 12 - 1), r.withTrimmedLeft(4), juce::Justification::centredLeft);
        }
    }
    // melody
    for (int i = 0; i < (int) notes.size(); ++i) {
        const auto& n = notes[(size_t) i];
        auto r = noteRect(n);
        juce::Path p; p.addRoundedRectangle(r, 4);
        juce::DropShadow(Theme::acc.withAlpha(i == selected ? 0.7f : 0.35f), i == selected ? 14 : 9, {}).drawForPath(g, p);
        g.setGradientFill(juce::ColourGradient(Theme::acc, r.getX(), 0, Theme::acc2, r.getRight() + 30, 0, false));
        g.setOpacity(0.55f + 0.45f * (float) n.velocity / 127.0f);
        g.fillPath(p);
        g.setOpacity(1.0f);
        if (i == selected) { g.setColour(juce::Colours::white); g.strokePath(p, juce::PathStrokeType(2.0f)); }
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.fillRect(r.getRight() - 3, r.getY() + 3, 1.5f, r.getHeight() - 6);
    }
    // playhead
    if (const double t = proc.playheadTicks(); t >= 0) {
        g.setColour(juce::Colours::white);
        g.fillRect(a.getX() + (float) (t / mc::step) * sw, 0.0f, 2.0f, all.getHeight());
    }
    g.setColour(Theme::line);
    g.drawRoundedRectangle(all.reduced(0.5f), 14, 1);
}

void PianoRoll::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const auto p = e.position;
    if (!noteArea().contains(p)) return;
    auto h = hit(p);
    if (e.mods.isPopupMenu()) {
        if (h.index >= 0) { notes.erase(notes.begin() + h.index); selected = -1; commit(); }
        return;
    }
    if (h.index < 0) {
        const int st = (int) std::floor(stepAt(p.x));
        const int bar = st / mc::stepsPerBar;
        if (bar < 0 || bar >= pat.bars() || proc.isLocked(bar)) return;
        notes.push_back({ st * mc::step, 2 * mc::step, pitchAt(p.y), 100, false });
        h = { (int) notes.size() - 1, false };
    }
    auto& n = notes[(size_t) h.index];
    selected = h.index;
    drag = { h.index, h.edge, stepAt(p.x) - (double) n.start / mc::step, pitchAt(p.y), n.pitch, false };
    proc.audition(n.pitch);
    repaint();
}

void PianoRoll::mouseDrag(const juce::MouseEvent& e) {
    if (drag.index < 0 || drag.index >= (int) notes.size()) return;
    auto& n = notes[(size_t) drag.index];
    const int total = pat.bars() * mc::stepsPerBar;
    if (drag.resize) {
        const int startStep = n.start / mc::step;
        n.length = juce::jlimit(1, total - startStep, (int) std::round(stepAt(e.position.x) - startStep)) * mc::step;
    } else {
        const int s = juce::jlimit(0, total - 1, (int) std::round(stepAt(e.position.x) - drag.offset));
        n.start = s * mc::step;
        const int np = juce::jlimit(lo, hi, drag.startPitch + (pitchAt(e.position.y) - drag.pitch0));
        if (np != n.pitch) { n.pitch = np; proc.audition(np); }
    }
    drag.moved = true;
    repaint();
}

void PianoRoll::mouseUp(const juce::MouseEvent&) {
    if (drag.index >= 0) {
        // keep the selection on the same note after sorting
        const auto keep = notes[(size_t) drag.index];
        drag.index = -1;
        commit();
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[(size_t) i].start == keep.start && notes[(size_t) i].pitch == keep.pitch) selected = i;
        repaint();
    }
}

void PianoRoll::mouseMove(const juce::MouseEvent& e) {
    if (!noteArea().contains(e.position)) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }
    const auto h = hit(e.position);
    setMouseCursor(h.index < 0 ? juce::MouseCursor::CrosshairCursor
                   : h.edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
}

void PianoRoll::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto h = hit(e.position);
    if (h.index >= 0) { notes.erase(notes.begin() + h.index); selected = -1; commit(); }
}

bool PianoRoll::keyPressed(const juce::KeyPress& k) {
    if (selected < 0 || selected >= (int) notes.size()) return false;
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) {
        notes.erase(notes.begin() + selected); selected = -1; commit(); return true;
    }
    if (k.getKeyCode() == juce::KeyPress::upKey || k.getKeyCode() == juce::KeyPress::downKey) {
        const int d = (k.getKeyCode() == juce::KeyPress::upKey ? 1 : -1) * (k.getModifiers().isShiftDown() ? 12 : 1);
        auto& n = notes[(size_t) selected];
        n.pitch = juce::jlimit(24, 108, n.pitch + d);
        proc.audition(n.pitch);
        const auto keep = n;
        commit();
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[(size_t) i].start == keep.start && notes[(size_t) i].pitch == keep.pitch) selected = i;
        return true;
    }
    return false;
}

void PianoRoll::commit() {
    proc.setMelody(notes);   // triggers refresh() through the change broadcaster
}

// ---------------------------------------------------------------- editor
MelodyCodeEditor::MelodyCodeEditor(MelodyCodeProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&look);
    addAndMakeVisible(content);

    for (int i = 0; i < 12; ++i) keyBox.addItem(noteNames[i], i + 1);
    for (int i = 0; i < (int) ccp::ScaleType::count; ++i) scaleBox.addItem(utf8(ccp::scaleName((ccp::ScaleType) i)), i + 1);
    for (int i = 0; i < (int) mc::Style::count; ++i) styleBox.addItem(mc::styleName((mc::Style) i), i + 1);
    for (int i = 0; i < (int) mc::Vibe::count; ++i) vibeBox.addItem(mc::vibeName((mc::Vibe) i), i + 1);
    for (int i = 0; i < Synth::presetCount; ++i) soundBox.addItem(Synth::presetName(i), i + 1);
    auto onSetting = [this] {
        auto& s = proc.settings();
        s.key = keyBox.getSelectedId() - 1;
        s.scale = (ccp::ScaleType) (scaleBox.getSelectedId() - 1);
        s.style = (mc::Style) (styleBox.getSelectedId() - 1);
        s.vibe = (mc::Vibe) (vibeBox.getSelectedId() - 1);
        proc.applySettings();
    };
    for (auto* b : { &keyBox, &scaleBox, &styleBox, &vibeBox }) { b->onChange = onSetting; content.addAndMakeVisible(b); }
    soundBox.onChange = [this] { proc.preset = soundBox.getSelectedId() - 1; };
    content.addAndMakeVisible(soundBox);
    content.addAndMakeVisible(roll);

    role.onChange = [this](int i) { proc.settings().role = (mc::Role) i; proc.newMelody(); };
    density.onChange = [this](int i) { proc.settings().density = (mc::Density) i; proc.newMelody(); };
    range.onChange = [this](int i) { proc.settings().range = (mc::Range) i; proc.newMelody(); };
    bars.onChange = [this](int i) { proc.settings().bars = i == 0 ? 2 : i == 1 ? 4 : 8; proc.applySettings(); };
    for (auto* s : { &role, &density, &range, &bars }) content.addAndMakeVisible(s);

    auto setupSlider = [this](juce::Slider& s, double lo, double hi, double v, std::function<void(double)> f) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.setRange(lo, hi, 1);
        s.setValue(v, juce::dontSendNotification);
        s.onValueChange = [&s, f] { f(s.getValue()); };
        content.addAndMakeVisible(s);
    };
    setupSlider(bpm, 70, 170, proc.bpm, [this](double v) { proc.bpm = (int) v; content.repaint(); });
    setupSlider(subVol, 0, 100, proc.subLevel * 100, [this](double v) { proc.subLevel = (float) v / 100.0f; content.repaint(); });
    setupSlider(melVol, 0, 100, proc.melodyLevel * 100, [this](double v) { proc.melodyLevel = (float) v / 100.0f; content.repaint(); });

    play.onClick = [this] { proc.setPlaying(!proc.isPlaying()); };
    vari.onClick = [this] { proc.variation(); };
    newMel.onClick = [this] { proc.newMelody(); };
    gen.onClick = [this] { proc.generateAll(); };
    newSub.onClick = [this] { proc.newBass(); };
    save.onClick = [this] { saveMidi(); };
    dragBtn.onClick = [this] { showHint(utf8("Arraste este bot\xc3\xa3o para a sua DAW")); };
    dragBtn.files = [this] { return writeMidiFiles(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("MelodyCode"), "Melody Code"); };
    historyBtn.onClick = [this] {
        juce::PopupMenu m;
        for (int i = 0; i < proc.historySize(); ++i) m.addItem(i + 1, proc.historyLabel(i));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(historyBtn), [this](int r) { if (r > 0) proc.restoreHistory(r - 1); });
    };
    for (auto* b : std::initializer_list<juce::Button*> { &play, &vari, &newMel, &gen, &newSub, &historyBtn, &save, &dragBtn }) {
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        content.addAndMakeVisible(b);
    }

    proc.addChangeListener(this);
    syncControls();
    constrainer.setFixedAspectRatio((double) Theme::width / Theme::height);
    constrainer.setSizeLimits(Theme::width / 2, Theme::height / 2, Theme::width * 2, Theme::height * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(Theme::width, Theme::height);
    startTimerHz(30);
}

MelodyCodeEditor::~MelodyCodeEditor() {
    proc.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void MelodyCodeEditor::syncControls() {
    const auto& s = proc.settings();
    keyBox.setSelectedId(s.key + 1, juce::dontSendNotification);
    scaleBox.setSelectedId((int) s.scale + 1, juce::dontSendNotification);
    styleBox.setSelectedId((int) s.style + 1, juce::dontSendNotification);
    vibeBox.setSelectedId((int) s.vibe + 1, juce::dontSendNotification);
    soundBox.setSelectedId(proc.preset + 1, juce::dontSendNotification);
    role.setSelected((int) s.role);
    density.setSelected((int) s.density);
    range.setSelected((int) s.range);
    bars.setSelected(s.bars == 2 ? 0 : s.bars == 4 ? 1 : 2);
    bpm.setValue(proc.bpm, juce::dontSendNotification);
    play.setButtonText(proc.isPlaying() ? "PARAR" : "OUVIR");
    play.lit = proc.isPlaying();
    roll.refresh();
    content.repaint();
}

void MelodyCodeEditor::changeListenerCallback(juce::ChangeBroadcaster*) { syncControls(); }

void MelodyCodeEditor::timerCallback() {
    if (proc.isPlaying()) roll.repaint();
    if (hintUntil && juce::Time::getMillisecondCounter() > hintUntil) { hintUntil = 0; hint.clear(); content.repaint(); }
}

void MelodyCodeEditor::showHint(const juce::String& text) {
    hint = text;
    hintUntil = juce::Time::getMillisecondCounter() + 2500;
    content.repaint();
}

juce::StringArray MelodyCodeEditor::writeMidiFiles(const juce::File& folder, const juce::String& base) {
    folder.createDirectory();
    const auto p = proc.pattern();
    juce::StringArray out;
    auto write = [&](const std::vector<mc::Note>& notes, const juce::String& suffix, int channel) {
        auto bytes = mc::midiBytes(notes, proc.bpm, channel);
        auto f = folder.getChildFile(base + " - " + suffix + ".mid");
        if (f.replaceWithData(bytes.data(), bytes.size())) out.add(f.getFullPathName());
    };
    write(p.melody, "Melody", 0);
    write(p.bass, "Sub", 1);
    return out;
}

void MelodyCodeEditor::saveMidi() {
    chooser = std::make_unique<juce::FileChooser>("Salvar MIDI", juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Melody Code.mid"), "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            const auto files = writeMidiFiles(f.getParentDirectory(), f.getFileNameWithoutExtension());
            showHint(files.size() == 2 ? "Salvo: " + f.getFileNameWithoutExtension() + " - Melody.mid e - Sub.mid" : utf8("N\xc3\xa3o foi poss\xc3\xadvel salvar nessa pasta"));
        });
}

void MelodyCodeEditor::paint(juce::Graphics& g) { g.fillAll(Theme::bg); }

void MelodyCodeEditor::resized() {
    content.setBounds(0, 0, Theme::width, Theme::height);
    content.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) Theme::width));
    layout();
}

namespace L {
constexpr int pad = 24, selY = 112, rollY = 172, rollH = 368, ctlY = 572, ctl2Y = 660, actY = 744;
}

void MelodyCodeEditor::layout() {
    const int W = Theme::width - 2 * L::pad;
    {
        const int gap = 12, w = (W - 4 * gap) / 5;
        int x = L::pad;
        for (auto* b : { &keyBox, &scaleBox, &styleBox, &vibeBox, &soundBox }) { b->setBounds(x, L::selY, w, 44); x += w + gap; }
    }
    roll.setBounds(L::pad, L::rollY, W, L::rollH);
    {
        const int gap = 12, w = (W - 3 * gap) / 4;
        int x = L::pad;
        for (auto* s : { &role, &density, &range, &bars }) { s->setBounds(x + 14, L::ctlY + 32, w - 28, 40); x += w + gap; }
        x = L::pad;
        for (auto* s : { &bpm, &subVol, &melVol }) { s->setBounds(x + 14, L::ctl2Y + 36, w - 28 - 46, 24); x += w + gap; }
        historyBtn.setBounds(x, L::ctl2Y, w, 70);
    }
    {
        const int gap = 10, unit = (W - 6 * gap) / 8;
        int x = L::pad;
        for (auto* b : std::initializer_list<juce::Button*> { &play, &vari, &newMel, &gen, &newSub, &dragBtn, &save }) {
            const int w = b == &gen ? unit * 2 + gap : unit;
            b->setBounds(x, L::actY, w, 56);
            x += w + gap;
        }
    }
}

void MelodyCodeEditor::paintContent(juce::Graphics& g) {
    using namespace Theme;
    auto all = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.fillAll(bg);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x5520706a), 120, -60, juce::Colour(0x0020706a), 620, 300, true));
    g.fillRect(all);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x553a2a80), 1180, -40, juce::Colour(0x003a2a80), 760, 320, true));
    g.fillRect(all);

    // header
    auto logo = juce::Rectangle<float>((float) L::pad, 22, 40, 40);
    g.setGradientFill(accent(logo));
    g.fillRoundedRectangle(logo, 11);
    {   // eighth note glyph
        g.setColour(ink);
        auto c = logo.getCentre();
        g.fillEllipse(c.x - 8, c.y + 2, 10, 8);
        g.fillRect(c.x + 0.5f, c.y - 11, 2.2f, 17.0f);
        juce::Path flag; flag.startNewSubPath(c.x + 2.5f, c.y - 11); flag.quadraticTo(c.x + 10, c.y - 7, c.x + 7, c.y - 1);
        g.strokePath(flag, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setFont(font(28.0f, true).withExtraKerningFactor(0.02f));
    g.setColour(fg);
    const float tx = logo.getRight() + 14;
    g.drawText("MELODY", juce::Rectangle<float>(tx, 22, 140, 40), juce::Justification::centredLeft);
    const float mw = juce::GlyphArrangement::getStringWidth(font(28.0f, true).withExtraKerningFactor(0.02f), "MELODY ");
    auto codeR = juce::Rectangle<float>(tx + mw, 22, 100, 40);
    g.setGradientFill(juce::ColourGradient(acc, codeR.getX(), 0, acc2, codeR.getX() + 90, 0, false));
    g.drawText("CODE", codeR, juce::Justification::centredLeft);

    const auto& s = proc.settings();
    const auto dot = utf8("  \xc2\xb7  ");
    const juce::String info = juce::String(noteNames[s.key]) + " " + utf8(ccp::scaleName(s.scale)) + dot + juce::String(s.bars) + " BARS" + dot + juce::String(proc.bpm) + " BPM";
    g.setFont(font(12.0f, true).withExtraKerningFactor(0.1f));
    const float iw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), info) + 34;
    auto pill = juce::Rectangle<float>((float) (width - L::pad) - 66 - iw, 26, iw, 32);
    g.setColour(acc.withAlpha(0.5f)); g.drawRoundedRectangle(pill, 16, 1);
    g.setColour(acc); g.drawText(info, pill, juce::Justification::centred);
    auto ver = juce::Rectangle<float>((float) (width - L::pad) - 56, 26, 56, 32);
    g.setColour(line); g.drawRoundedRectangle(ver, 16, 1);
    g.setColour(mute); g.drawText("v1.0", ver, juce::Justification::centred);
    g.setColour(line);
    g.fillRect((float) L::pad, 80.0f, (float) (width - 2 * L::pad), 1.0f);

    auto label = [&](const juce::String& t, float x, float y) {
        g.setColour(mute);
        g.setFont(font(11.0f, true).withExtraKerningFactor(0.2f));
        g.drawText(t, juce::Rectangle<float>(x, y, 300, 16), juce::Justification::centredLeft);
    };
    label("TOM", (float) keyBox.getX(), 92); label("ESCALA", (float) scaleBox.getX(), 92); label("STYLE", (float) styleBox.getX(), 92);
    label("VIBE", (float) vibeBox.getX(), 92); label("SOUND", (float) soundBox.getX(), 92);

    g.setColour(mute);
    g.setFont(font(12.0f));
    g.drawText(hint.isNotEmpty() ? hint
               : utf8("Clique no grid pra criar nota  \xc2\xb7  arraste pra mover  \xc2\xb7  puxe a ponta pra alongar  \xc2\xb7  duplo clique ou Delete apaga  \xc2\xb7  \xe2\x86\x91\xe2\x86\x93 muda a nota"),
               juce::Rectangle<float>((float) L::pad, (float) (L::rollY + L::rollH + 8), 900, 18), juce::Justification::centredLeft);
    if (hint.isNotEmpty()) { g.setColour(acc); g.drawText(hint, juce::Rectangle<float>((float) L::pad, (float) (L::rollY + L::rollH + 8), 900, 18), juce::Justification::centredLeft); }

    auto box = [&](juce::Rectangle<int> r, const juce::String& t, const juce::String& value) {
        g.setColour(panel2);
        g.fillRoundedRectangle(r.toFloat(), 12);
        g.setColour(line);
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 12, 1);
        label(t, (float) r.getX() + 14, (float) r.getY() + 10);
        if (value.isNotEmpty()) {
            g.setColour(fg);
            g.setFont(font(13.0f, true));
            g.drawText(value, r.withTrimmedRight(14).withTrimmedTop(32).withHeight(28).toFloat(), juce::Justification::centredRight);
        }
    };
    const int gap = 12, w = (width - 2 * L::pad - 3 * gap) / 4;
    const char* titles[] { "ROLE", "DENSITY", "RANGE", "BARS" };
    for (int i = 0; i < 4; ++i) box({ L::pad + i * (w + gap), L::ctlY, w, 80 }, titles[i], {});
    box({ L::pad, L::ctl2Y, w, 70 }, "BPM", juce::String(proc.bpm));
    box({ L::pad + (w + gap), L::ctl2Y, w, 70 }, "SUB", juce::String((int) std::round(proc.subLevel * 100)));
    box({ L::pad + 2 * (w + gap), L::ctl2Y, w, 70 }, "MELODY", juce::String((int) std::round(proc.melodyLevel * 100)));
}
