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

void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    const juce::PathStrokeType stroke(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight(), cx = r.getCentreX(), cy = r.getCentreY();
    auto arrowHead = [&](juce::Point<float> tip, float angle) {
        juce::Path a;
        a.addTriangle(tip, tip + juce::Point<float>(std::cos(angle + 2.5f), std::sin(angle + 2.5f)) * 5.5f,
                      tip + juce::Point<float>(std::cos(angle - 2.5f), std::sin(angle - 2.5f)) * 5.5f);
        g.fillPath(a);
    };
    switch (icon) {
        case Icon::play: {
            juce::Path p; p.addTriangle(x + w * 0.22f, y + h * 0.12f, x + w * 0.22f, y + h * 0.88f, x + w * 0.88f, cy);
            g.fillPath(p); break;
        }
        case Icon::stop: g.fillRoundedRectangle(r.reduced(w * 0.2f), 2.0f); break;
        case Icon::variation: {   // two arcs chasing each other
            const float rad = w * 0.38f;
            juce::Path a, b;
            a.addCentredArc(cx, cy, rad, rad, 0, -2.6f, -0.3f, true);
            b.addCentredArc(cx, cy, rad, rad, 0, 0.55f, 2.85f, true);
            g.strokePath(a, stroke); g.strokePath(b, stroke);
            arrowHead({ cx + rad * std::sin(-0.3f), cy - rad * std::cos(-0.3f) }, -0.3f);
            arrowHead({ cx + rad * std::sin(2.85f), cy - rad * std::cos(2.85f) }, 2.85f);
            break;
        }
        case Icon::random: {      // crossing arrows
            juce::Path top, bottom;
            top.startNewSubPath(x + 1, y + h * 0.28f);
            top.cubicTo(cx - 2, y + h * 0.28f, cx + 1, y + h * 0.72f, x + w - 4, y + h * 0.72f);
            bottom.startNewSubPath(x + 1, y + h * 0.72f);
            bottom.cubicTo(cx - 2, y + h * 0.72f, cx + 1, y + h * 0.28f, x + w - 4, y + h * 0.28f);
            g.strokePath(top, stroke); g.strokePath(bottom, stroke);
            juce::Path a;
            a.addTriangle(x + w - 5, y + 1, x + w, y + h * 0.28f, x + w - 5, y + h * 0.5f);
            a.addTriangle(x + w - 5, y + h - 1, x + w, y + h * 0.72f, x + w - 5, y + h * 0.5f);
            g.fillPath(a);
            break;
        }
        case Icon::sparkle: {     // generate: a four-point star and a small one
            auto star = [&](float sx, float sy, float s) {
                juce::Path p;
                p.startNewSubPath(sx, sy - s);
                p.quadraticTo(sx, sy, sx + s, sy); p.quadraticTo(sx, sy, sx, sy + s);
                p.quadraticTo(sx, sy, sx - s, sy); p.quadraticTo(sx, sy, sx, sy - s);
                p.closeSubPath(); g.fillPath(p);
            };
            star(cx - w * 0.08f, cy + h * 0.06f, w * 0.42f);
            star(x + w * 0.86f, y + h * 0.16f, w * 0.18f);
            break;
        }
        case Icon::sub: {         // a low sine wave on a baseline
            juce::Path p;
            for (int i = 0; i <= 24; ++i) {
                const float t = (float) i / 24.0f;
                const juce::Point<float> pt { x + t * w, cy - std::sin(t * juce::MathConstants<float>::twoPi) * h * 0.32f };
                if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
            }
            g.strokePath(p, stroke);
            g.drawLine(x, y + h - 1, x + w, y + h - 1, 1.2f);
            break;
        }
        case Icon::midi: {
            g.drawRoundedRectangle(r.reduced(1), 2, 1.6f);
            const float keyY = y + h * 0.57f;
            for (int i = 1; i < 5; ++i) g.drawVerticalLine((int) std::round(x + w * (float) i / 5.0f), keyY, y + h - 1);
            for (float k : { 0.29f, 0.49f, 0.69f }) g.fillRect(x + w * k, keyY, 2.5f, h * 0.22f);
            break;
        }
        case Icon::save:
            g.drawLine(cx, y + 1, cx, y + h - 6, 1.8f);
            g.drawLine(cx, y + h - 6, cx - 5, y + h - 11, 1.8f);
            g.drawLine(cx, y + h - 6, cx + 5, y + h - 11, 1.8f);
            g.drawLine(x + 2, y + h - 1, x + w - 2, y + h - 1, 1.8f);
            break;
        case Icon::history:
            g.drawEllipse(r.reduced(2), 1.7f);
            g.drawLine(cx, y + 5, cx, cy, 1.7f);
            g.drawLine(cx, cy, x + w - 5, cy + 3, 1.7f);
            break;
        case Icon::gear: {
            const float outer = std::min(w, h) * 0.46f;
            juce::Path p;
            for (int i = 0; i < 16; ++i) {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 16.0f;
                const float rad = (i % 2 == 0) ? outer : outer * 0.77f;
                const auto pt = r.getCentre() + juce::Point<float>(std::cos(a), std::sin(a)) * rad;
                if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
            }
            p.closeSubPath();
            g.strokePath(p, juce::PathStrokeType(1.6f));
            g.drawEllipse(juce::Rectangle<float>(w * 0.36f, w * 0.36f).withCentre(r.getCentre()), 1.6f);
            break;
        }
        case Icon::none: break;
    }
}

void ActionButton::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour textColour;
    juce::Font f = Theme::font(13.0f, true).withExtraKerningFactor(0.1f);
    float iconSize = 16.0f;
    if (isAccent) {
        juce::Path p; p.addRoundedRectangle(r, 12);
        juce::DropShadow(Theme::acc.withAlpha(over ? 0.45f : 0.3f), 22, {}).drawForPath(g, p);
        g.setGradientFill(Theme::accent(r));
        g.fillPath(p);
        if (down) { g.setColour(juce::Colours::black.withAlpha(0.15f)); g.fillPath(p); }
        textColour = Theme::ink;
        f = Theme::font(24.0f, true).withExtraKerningFactor(0.3f);
        iconSize = 22.0f;
    } else {
        g.setColour(down ? Theme::panel : Theme::panel2);
        g.fillRoundedRectangle(r, 12);
        g.setColour(lit || over ? Theme::acc : Theme::line);
        g.drawRoundedRectangle(r, 12, lit ? 1.6f : 1.0f);
        textColour = lit ? Theme::acc : Theme::fg;
    }
    g.setFont(f);
    const auto text = getButtonText();
    if (icon == Icon::none) {
        g.setColour(textColour);
        g.drawFittedText(text, getLocalBounds().reduced(6), juce::Justification::centred, 2);
        return;
    }
    const float gap = 8.0f;
    const float maxText = r.getWidth() - iconSize - gap - 16.0f;
    float tw = juce::GlyphArrangement::getStringWidth(f, text);
    if (!isAccent)   // tighten the label before falling back to the stacked layout
        for (float size : { 12.5f, 12.0f, 11.5f }) {
            if (tw <= maxText) break;
            f = Theme::font(size, true).withExtraKerningFactor(0.04f);
            g.setFont(f);
            tw = juce::GlyphArrangement::getStringWidth(f, text);
        }
    if (tw <= maxText) {   // icon and label on one line
        const float total = iconSize + gap + tw;
        const float x0 = r.getCentreX() - total / 2;
        drawIcon(g, icon, { x0, r.getCentreY() - iconSize / 2, iconSize, iconSize }, isAccent ? Theme::ink : (lit ? Theme::acc : Theme::acc.interpolatedWith(Theme::fg, 0.35f)));
        g.setColour(textColour);
        g.drawText(text, juce::Rectangle<float>(x0 + iconSize + gap, r.getY(), tw + 4, r.getHeight()), juce::Justification::centredLeft);
    } else {               // narrow button: icon above, label below
        drawIcon(g, icon, { r.getCentreX() - 8, r.getY() + 9, 16, 16 }, lit ? Theme::acc : Theme::acc.interpolatedWith(Theme::fg, 0.35f));
        g.setColour(textColour);
        g.setFont(Theme::font(11.5f, true).withExtraKerningFactor(0.08f));
        g.drawFittedText(text, getLocalBounds().reduced(4).withTrimmedTop(26), juce::Justification::centred, 1);
    }
}

void IconButton::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(down ? Theme::panel : over ? Theme::panel2.brighter(0.08f) : Theme::panel2);
    g.fillEllipse(r);
    g.setColour(over ? Theme::acc : Theme::line);
    g.drawEllipse(r, 1.0f);
    drawIcon(g, icon, r.reduced(r.getWidth() * 0.26f), over ? Theme::acc : Theme::mute.brighter(0.3f));
}

// ---------------------------------------------------------------------------

void ModalOverlay::present(juce::Component& owner, std::unique_ptr<juce::Component> panel, float scale) {
    const bool was = isPresenting();
    if (content) removeChildComponent(content.get());
    content = std::move(panel);
    uiScale = scale;
    if (!was) {
        setVisible(false);
        // blur: shrink the snapshot, soften it, stretch it back when drawn
        const auto snap = owner.createComponentSnapshot(owner.getLocalBounds(), true, 1.0f);
        const int w = juce::jmax(1, snap.getWidth() / 6), h = juce::jmax(1, snap.getHeight() / 6);
        juce::Image small(juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g(small);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(snap, 0, 0, w, h, 0, 0, snap.getWidth(), snap.getHeight());
        }
        juce::ImageConvolutionKernel kernel(7);
        kernel.createGaussianBlur(2.2f);
        kernel.applyToImage(small, small, small.getBounds());
        backdrop = small;
        reveal = 0;
    }
    setBounds(owner.getLocalBounds());
    addAndMakeVisible(*content);
    layoutPanel();
    setVisible(true);
    toFront(true);
    if (!was) startTimerHz(60);
}

void ModalOverlay::dismiss() {
    if (!content) return;
    stopTimer();
    removeChildComponent(content.get());
    juce::MessageManager::callAsync([old = std::shared_ptr<juce::Component>(content.release())] {});
    backdrop = {};
    setVisible(false);
}

void ModalOverlay::layoutPanel() {
    if (!content) return;
    const float ease = 1.0f - std::pow(1.0f - reveal, 3.0f);
    const float w = (float) content->getWidth() * uiScale, h = (float) content->getHeight() * uiScale;
    const float x = ((float) getWidth() - w) * 0.5f, y = ((float) getHeight() - h) * 0.5f + (1.0f - ease) * 14.0f;
    content->setTopLeftPosition(0, 0);
    content->setTransform(juce::AffineTransform::scale(uiScale).translated(x, y));
    content->setAlpha(ease);
}

void ModalOverlay::timerCallback() {
    reveal = juce::jmin(1.0f, reveal + 1.0f / 9.0f);
    layoutPanel();
    repaint();
    if (reveal >= 1.0f) stopTimer();
}

void ModalOverlay::paint(juce::Graphics& g) {
    const float ease = 1.0f - std::pow(1.0f - reveal, 3.0f);
    if (backdrop.isValid()) {
        g.setOpacity(ease);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(backdrop, getLocalBounds().toFloat());
        g.setOpacity(1.0f);
    }
    g.setColour(juce::Colour(0xff040506).withAlpha(0.58f * ease));
    g.fillAll();
    if (content) {
        juce::Path shape;
        shape.addRoundedRectangle(content->getBounds().toFloat().transformedBy(content->getTransform()), 16.0f * uiScale);
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f * ease), juce::roundToInt(40.0f * uiScale), { 0, juce::roundToInt(14.0f * uiScale) }).drawForPath(g, shape);
    }
}

void ModalOverlay::mouseUp(const juce::MouseEvent& e) {
    if (content && !content->getBounds().toFloat().transformedBy(content->getTransform()).contains(e.position)) dismiss();
}

bool ModalOverlay::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey) { dismiss(); return true; }
    return false;
}

// ---------------------------------------------------------------------------

SettingsPanel::SettingsPanel(MelodyCodeProcessor& p, std::function<void()> c, std::function<void()> ch)
    : proc(p), close(std::move(c)), changed(std::move(ch)) {
    setSize(panelWidth, 412);
    startTimerHz(4);   // keeps the DAW tempo readout fresh
}

void SettingsPanel::paint(juce::Graphics& g) {
    using namespace Theme;
    auto all = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(panel2, 0, 0, panel, 0, all.getHeight(), false));
    g.fillRoundedRectangle(all, 16);
    g.setGradientFill(juce::ColourGradient(acc.withAlpha(0.10f), all.getCentreX(), 0, acc.withAlpha(0.0f), all.getCentreX(), 90, false));
    g.fillRoundedRectangle(all, 16);
    g.setColour(line);
    g.drawRoundedRectangle(all.reduced(0.5f), 16, 1);

    auto section = [&](const juce::String& t, float y) {
        g.setColour(mute); g.setFont(font(11.5f, true).withExtraKerningFactor(0.15f));
        g.drawText(t, juce::Rectangle<float>(18, y, 260, 16), juce::Justification::centredLeft);
    };
    auto divider = [&](float y) { g.setColour(line); g.fillRect(18.0f, y, panelWidth - 36.0f, 1.0f); };
    auto toggle = [&](juce::Rectangle<float> row, const juce::String& title, bool on, const juce::String& note) {
        g.setColour(isOver(row) ? panel2.brighter(0.06f) : panel2);
        g.fillRoundedRectangle(row, 10);
        g.setColour(fg); g.setFont(font(14.5f, true));
        g.drawText(title, row.withTrimmedLeft(12), juce::Justification::centredLeft);
        auto track = juce::Rectangle<float>(40, 22).withCentre({ row.getRight() - 32, row.getCentreY() });
        if (on) g.setGradientFill(accent(track)); else g.setColour(juce::Colour(0xff2a2f3d));
        g.fillRoundedRectangle(track, 11);
        g.setColour(fg);
        g.fillEllipse(juce::Rectangle<float>(16, 16).withCentre({ on ? track.getRight() - 11 : track.getX() + 11, track.getCentreY() }));
        if (note.isNotEmpty()) { g.setColour(mute); g.setFont(font(12.5f)); g.drawText(note, row.withTrimmedRight(62), juce::Justification::centredRight); }
    };

    auto mark = juce::Rectangle<float>(18, 18, 34, 34);
    g.setGradientFill(accent(mark)); g.fillRoundedRectangle(mark, 9);
    drawIcon(g, Icon::gear, mark.reduced(8), ink);
    g.setColour(mute); g.setFont(font(11.5f, true).withExtraKerningFactor(0.15f));
    g.drawText(juce::String::fromUTF8("CONFIGURAÇÕES"), 62, 17, 200, 16, juce::Justification::centredLeft);
    g.setColour(fg); g.setFont(font(19.0f, true));
    g.drawText("Melody Code", 62, 32, 140, 24, juce::Justification::centredLeft);
    auto ver = juce::Rectangle<float>(190, 34, 62, 22);
    g.setColour(acc.withAlpha(0.12f)); g.fillRoundedRectangle(ver, 11);
    g.setColour(acc.withAlpha(0.4f)); g.drawRoundedRectangle(ver.reduced(0.5f), 11, 1);
    g.setColour(acc); g.setFont(font(12.5f, true));
    g.drawText("v" JucePlugin_VersionString, ver, juce::Justification::centred);
    {
        const auto c = closeArea();
        g.setColour(isOver(c) ? panel2.brighter(0.1f) : panel2); g.fillEllipse(c);
        g.setColour(isOver(c) ? fg : mute);
        const auto x = c.reduced(c.getWidth() * 0.34f);
        g.drawLine({ x.getTopLeft(), x.getBottomRight() }, 1.6f);
        g.drawLine({ x.getBottomLeft(), x.getTopRight() }, 1.6f);
    }
    divider(70);

    section("TEMPO", 80);
    const bool synced = proc.syncToHost.load();
    toggle(syncRow(), "Sincronizar com a DAW", synced, synced && proc.hostIsPlaying() ? juce::String::fromUTF8("tocando junto") : juce::String());
    for (size_t i = 0; i < bpmValues.size(); ++i) {
        const auto r = chip(i);
        const bool active = proc.bpm == bpmValues[i];
        if (active) { g.setGradientFill(accent(r)); g.fillRoundedRectangle(r, 8); }
        else {
            g.setColour(isOver(r) ? panel2.brighter(0.06f) : panel2); g.fillRoundedRectangle(r, 8);
            g.setColour(isOver(r) ? acc.withAlpha(0.7f) : line); g.drawRoundedRectangle(r.reduced(0.5f), 8, 1);
        }
        g.setColour(active ? ink : fg); g.setFont(font(14.5f, true));
        g.drawText(juce::String(bpmValues[i]), r, juce::Justification::centred);
    }
    g.setColour(quiet.brighter(0.4f)); g.setFont(font(12.0f));
    g.drawFittedText(juce::String::fromUTF8("Com a DAW tocando, o loop segue o tempo e o compasso dela. Parada, usa o BPM acima."),
                     juce::Rectangle<int>(18, 224, panelWidth - 36, 32), juce::Justification::centredLeft, 2, 1.0f);
    divider(258);

    section("MIDI", 266);
    toggle(midiRow(), "Enviar MIDI para a DAW", proc.midiOut.load(), {});
    g.setColour(quiet.brighter(0.4f)); g.setFont(font(12.0f));
    g.drawFittedText(juce::String::fromUTF8("Melodia no canal 1 e grave no canal 2, pra gravar ou tocar com outro instrumento."),
                     juce::Rectangle<int>(18, 334, panelWidth - 36, 32), juce::Justification::centredLeft, 2, 1.0f);
    divider(372);
    g.setColour(mute); g.setFont(font(12.0f));
    g.drawText("Audio Makers", juce::Rectangle<float>(18, 380, 200, 20), juce::Justification::centredLeft);
}

void SettingsPanel::mouseUp(const juce::MouseEvent& e) {
    const auto p = e.position;
    if (closeArea().contains(p)) { if (close) close(); return; }
    if (syncRow().contains(p)) proc.syncToHost = !proc.syncToHost.load();
    else if (midiRow().contains(p)) proc.midiOut = !proc.midiOut.load();
    else for (size_t i = 0; i < bpmValues.size(); ++i) if (chip(i).contains(p)) proc.bpm = bpmValues[i];
    if (changed) changed();
    repaint();
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
    repaint();
}

void PianoRoll::resized() {}

void PianoRoll::paint(juce::Graphics& g) {
    auto all = getLocalBounds().toFloat();
    juce::Path clip; clip.addRoundedRectangle(all, 14);
    g.reduceClipRegion(clip);
    g.fillAll(juce::Colour(0xff0f121a));
    const auto a = noteArea();
    const float rh = rowH(), sw = stepW();
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.key, pat.scale);
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
    for (int st = 0; st < bars * 16; ++st) {   // chord tones follow where each chord really sounds
        auto pcs = mc::Engine::chordPitchClasses(pat.chordAt(st * mc::step));
        for (int m = hi; m >= lo; --m)
            if (std::find(pcs.begin(), pcs.end(), m % 12) != pcs.end()) {
                g.setColour(Theme::acc.withAlpha(0.06f));
                g.fillRect(a.getX() + (float) st * sw, a.getY() + (float) (hi - m) * rh, sw, rh);
            }
    }
    for (int b = 0; b < bars; ++b) {
        if (b > 0 && pat.chordShift[(size_t) b] != 0) {   // chord change off the bar line
            const float x = a.getX() + (float) (b * 16 + pat.chordShift[(size_t) b]) * sw;
            g.setColour(Theme::acc2.withAlpha(0.8f));
            g.fillRect(x - 1.0f, a.getY(), 2.0f, a.getHeight());
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
    g.drawText("BASS", juce::Rectangle<float>(0, lane.getY(), (float) keyW, lane.getHeight()), juce::Justification::centred);
    // chord names where each chord really starts (generated for the whole loop, not picked per bar)
    for (int b = 0; b < bars; ++b) {
        if (b > 0 && pat.progression.chords[(size_t) b].name == pat.progression.chords[(size_t) (b - 1)].name && pat.chordShift[(size_t) b] == 0) continue;
        const auto& c = pat.progression.chords[(size_t) b];
        const float x = a.getX() + (float) (b * 16 + pat.chordShift[(size_t) b]) * sw;
        auto chip = juce::Rectangle<float>(x + 6, 7, 120, 24);
        g.setColour(Theme::panel2);
        g.fillRoundedRectangle(chip, 7);
        g.setColour(Theme::acc.withAlpha(0.8f));
        g.setFont(Theme::font(11.0f, true));
        g.drawText(juce::String(roman[c.degree % 7]), chip.removeFromLeft(30), juce::Justification::centred);
        g.setColour(Theme::fg);
        g.setFont(Theme::font(13.0f, true));
        g.drawText(c.name, chip.withTrimmedRight(6), juce::Justification::centredLeft);
    }
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
        if (bar < 0 || bar >= pat.bars()) return;
        notes.push_back({ st * mc::step, 2 * mc::step, pitchAt(p.y), 100 });
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

    keyBox.addItem(utf8("PADR\xc3\x83O"), 1);
    for (int i = 0; i < 12; ++i) keyBox.addItem(noteNames[i], i + 2);
    scaleBox.addItem(utf8("PADR\xc3\x83O"), 1);
    for (int i = 0; i < (int) ccp::ScaleType::count; ++i) scaleBox.addItem(utf8(ccp::scaleName((ccp::ScaleType) i)), i + 2);
    for (int i = 0; i < (int) mc::Style::count; ++i) styleBox.addItem(utf8(mc::styleName((mc::Style) i)), i + 1);
    for (int i = 0; i < (int) mc::Vibe::count; ++i) vibeBox.addItem(utf8(mc::vibeName((mc::Vibe) i)), i + 1);
    for (int i = 0; i < Synth::presetCount; ++i) soundBox.addItem(Synth::presetName(i), i + 1);
    auto onSetting = [this] {
        auto& s = proc.settings();
        s.key = keyBox.getSelectedId() - 2;      // id 1 = PADRÃO (-1)
        s.scale = scaleBox.getSelectedId() - 2;
        s.style = (mc::Style) (styleBox.getSelectedId() - 1);
        proc.applySettings();
    };
    for (auto* b : { &keyBox, &scaleBox, &styleBox }) { b->onChange = onSetting; content.addAndMakeVisible(b); }
    vibeBox.onChange = [this] { proc.setVibe((mc::Vibe) (vibeBox.getSelectedId() - 1)); };   // sound only, notes stay
    content.addAndMakeVisible(vibeBox);
    soundBox.onChange = [this] { proc.preset = soundBox.getSelectedId() - 1; };
    content.addAndMakeVisible(soundBox);
    content.addAndMakeVisible(roll);

    role.onChange = [this](int i) { proc.settings().role = (mc::Role) i; proc.newMelody(); };
    density.onChange = [this](int i) { proc.settings().density = (mc::Density) i; proc.newMelody(); };
    range.onChange = [this](int i) { proc.settings().range = (mc::Range) i; proc.newMelody(); };
    feel.onChange = [this](int i) { proc.settings().feel = (mc::Feel) i; proc.newMelody(); };
    bars.onChange = [this](int i) { proc.settings().bars = i == 0 ? 2 : i == 1 ? 4 : 8; proc.applySettings(); };
    for (auto* s : { &role, &feel, &density, &range, &bars }) content.addAndMakeVisible(s);

    auto setupSlider = [this](juce::Slider& s, double lo, double hi, double v, std::function<void(double)> f) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.setRange(lo, hi, 1);
        s.setValue(v, juce::dontSendNotification);
        s.onValueChange = [&s, f] { f(s.getValue()); };
        content.addAndMakeVisible(s);
    };
    setupSlider(bpm, 70, 170, proc.bpm, [this](double v) { proc.bpm = (int) v; content.repaint(); });
    setupSlider(bassVol, 0, 100, proc.bassLevel * 100, [this](double v) { proc.bassLevel = (float) v / 100.0f; content.repaint(); });
    setupSlider(melVol, 0, 100, proc.melodyLevel * 100, [this](double v) { proc.melodyLevel = (float) v / 100.0f; content.repaint(); });

    play.onClick = [this] { proc.setPlaying(!proc.isPlaying()); };
    vari.onClick = [this] { proc.variation(); };
    newMel.onClick = [this] { proc.newMelody(); };
    gen.onClick = [this] { proc.generateAll(); };
    save.onClick = [this] { saveMidi(); };
    dragBtn.onClick = [this] { showHint(utf8("Arraste este bot\xc3\xa3o para a sua DAW")); };
    dragBtn.files = [this] { return writeMidiFiles(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("MelodyCode"), "Melody Code"); };
    historyBtn.onClick = [this] {
        juce::PopupMenu m;
        for (int i = 0; i < proc.historySize(); ++i) m.addItem(i + 1, proc.historyLabel(i));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(historyBtn), [this](int r) { if (r > 0) proc.restoreHistory(r - 1); });
    };
    for (auto* b : std::initializer_list<juce::Button*> { &play, &vari, &newMel, &gen, &historyBtn, &save, &dragBtn }) {
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        content.addAndMakeVisible(b);
    }

    gear.onClick = [this] { openSettings(); };
    content.addAndMakeVisible(gear);
    addChildComponent(overlay);

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
    keyBox.setSelectedId(s.key + 2, juce::dontSendNotification);
    scaleBox.setSelectedId(s.scale + 2, juce::dontSendNotification);
    styleBox.setSelectedId((int) s.style + 1, juce::dontSendNotification);
    vibeBox.setSelectedId((int) s.vibe + 1, juce::dontSendNotification);
    soundBox.setSelectedId(proc.preset + 1, juce::dontSendNotification);
    role.setSelected((int) s.role);
    density.setSelected((int) s.density);
    range.setSelected((int) s.range);
    feel.setSelected((int) s.feel);
    bars.setSelected(s.bars == 2 ? 0 : s.bars == 4 ? 1 : 2);
    bpm.setValue(proc.bpm, juce::dontSendNotification);
    play.setButtonText(proc.isPlaying() ? "PARAR" : "OUVIR");
    play.icon = proc.isPlaying() ? Icon::stop : Icon::play;
    play.lit = proc.isPlaying();
    play.repaint();
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
    write(p.bass, "Bass", 1);
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

void MelodyCodeEditor::openSettings() {
    auto panel = std::make_unique<SettingsPanel>(proc, [this] { overlay.dismiss(); }, [this] { bpm.setValue(proc.bpm, juce::dontSendNotification); content.repaint(); });
    overlay.present(*this, std::move(panel), (float) getWidth() / (float) Theme::width);
    overlay.grabKeyboardFocus();
}

void MelodyCodeEditor::paint(juce::Graphics& g) { g.fillAll(Theme::bg); }

void MelodyCodeEditor::resized() {
    content.setBounds(0, 0, Theme::width, Theme::height);
    content.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) Theme::width));
    layout();
    overlay.dismiss();
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
    gear.setBounds(Theme::width - L::pad - 40, 22, 40, 40);
    {
        const int gap = 12, w5 = (W - 4 * gap) / 5, w = (W - 3 * gap) / 4;
        int x = L::pad;
        for (auto* s : { &role, &feel, &density, &range, &bars }) { s->setBounds(x + 14, L::ctlY + 32, w5 - 28, 40); x += w5 + gap; }
        x = L::pad;
        for (auto* s : { &bpm, &bassVol, &melVol }) { s->setBounds(x + 14, L::ctl2Y + 36, w - 28 - 46, 24); x += w + gap; }
        historyBtn.setBounds(x, L::ctl2Y, w, 70);
    }
    {
        const int gap = 10, unit = (W - 6 * gap) / 7;
        int x = L::pad;
        for (auto* b : std::initializer_list<juce::Button*> { &play, &vari, &newMel, &gen, &dragBtn, &save }) {
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
    const auto shownPat = proc.pattern();
    const auto arrow = utf8(" \xe2\x86\x92 ");
    const auto padrao = utf8("PADR\xc3\x83O") + arrow;
    const juce::String info = (s.style == mc::Style::automatic ? padrao : juce::String()) + utf8(mc::styleName(shownPat.style))
                            + dot + (s.key < 0 || s.scale < 0 ? padrao : juce::String()) + juce::String(noteNames[shownPat.key]) + " " + utf8(ccp::scaleName(shownPat.scale))
                            + dot + utf8(mc::vibeName(s.vibe)) + dot + juce::String(s.bars) + " BARS" + dot + juce::String(proc.bpm) + " BPM";
    g.setFont(font(12.0f, true).withExtraKerningFactor(0.1f));
    const float iw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), info) + 34;
    auto pill = juce::Rectangle<float>((float) (width - L::pad) - 52 - iw, 26, iw, 32);
    g.setColour(acc.withAlpha(0.5f)); g.drawRoundedRectangle(pill, 16, 1);
    g.setColour(acc); g.drawText(info, pill, juce::Justification::centred);
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
    const int gap = 12, w = (width - 2 * L::pad - 3 * gap) / 4, w5 = (width - 2 * L::pad - 4 * gap) / 5;
    const char* titles[] { "ROLE", "FEEL", "DENSITY", "RANGE", "BARS" };
    for (int i = 0; i < 5; ++i) box({ L::pad + i * (w5 + gap), L::ctlY, w5, 80 }, titles[i], {});
    box({ L::pad, L::ctl2Y, w, 70 }, "BPM", juce::String(proc.bpm));
    box({ L::pad + (w + gap), L::ctl2Y, w, 70 }, "BASS", juce::String((int) std::round(proc.bassLevel * 100)));
    box({ L::pad + 2 * (w + gap), L::ctl2Y, w, 70 }, "MELODY", juce::String((int) std::round(proc.melodyLevel * 100)));
}
