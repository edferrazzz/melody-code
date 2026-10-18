#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UpdateChecker.h"

namespace Theme {
constexpr int width = 1240, height = 820;
const juce::Colour bg { 0xff0c0e13 }, panel { 0xff141821 }, panel2 { 0xff1b2030 }, line { 0xff262d40 },
    fg { 0xffe8ecf5 }, mute { 0xff8590a8 }, quiet { 0xff4a5470 }, acc { 0xff2ee6c5 }, acc2 { 0xff7c5cff }, ink { 0xff04201b };
juce::Font font(float h, bool bold = false);
juce::ColourGradient accent(juce::Rectangle<float>);
}

class MelodyLook : public juce::LookAndFeel_V4 {
public:
    MelodyLook();
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return Theme::font(15.0f, true); }
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return Theme::font(15.0f, true); }
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

class Segmented : public juce::Component {
public:
    Segmented(juce::StringArray options);
    void setSelected(int i) { selected = i; repaint(); }
    int getSelected() const { return selected; }
    std::function<void(int)> onChange;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    juce::StringArray options;
    int selected {};
};

enum class Icon { none, play, stop, variation, random, sparkle, sub, midi, wave, save, history, gear };
void drawIcon(juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);

class ActionButton : public juce::Button {
public:
    ActionButton(juce::String text, Icon i = Icon::none, bool accent = false) : Button(text), icon(i), isAccent(accent) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
    Icon icon;
    bool isAccent;
    bool lit {};
};

// Round header button (the settings gear).
class IconButton : public juce::Button {
public:
    explicit IconButton(Icon i) : Button("icon"), icon(i) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    Icon icon;
    bool dot {};   // yellow dot: an update is available
};

// What the update check found; owned by the editor, shown in the settings panel.
struct UpdateStatus {
    enum State { idle, checking, upToDate, available, failed } state { idle };
    juce::String latest, url;
};

// Full-window layer that blurs the editor behind a centred panel (same behaviour as Chord Code's pop-ups).
class ModalOverlay : public juce::Component, private juce::Timer {
public:
    ModalOverlay() { setWantsKeyboardFocus(true); setVisible(false); }
    void present(juce::Component& owner, std::unique_ptr<juce::Component> panel, float scale);
    void dismiss();
    bool isPresenting() const { return content != nullptr; }
    void layoutPanel();
    void paint(juce::Graphics&) override;
    void resized() override { layoutPanel(); }
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    void timerCallback() override;
    juce::Image backdrop;
    std::unique_ptr<juce::Component> content;
    float uiScale { 1.0f }, reveal {};
};

class SettingsPanel : public juce::Component, private juce::Timer {
public:
    SettingsPanel(MelodyCodeProcessor&, UpdateStatus&, std::function<void()> close, std::function<void()> changed, std::function<void()> checkUpdate);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent& e) override { mouse = e.position; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { mouse = { -1, -1 }; repaint(); }
    void mouseUp(const juce::MouseEvent&) override;
    static constexpr int panelWidth = 340;
private:
    void timerCallback() override { repaint(); }
    static constexpr std::array<int, 8> bpmValues { 80, 90, 100, 110, 120, 128, 140, 160 };
    juce::Rectangle<float> syncRow() const { return { 18, 100, panelWidth - 36.0f, 40 }; }
    juce::Rectangle<float> midiRow() const { return { 18, 288, panelWidth - 36.0f, 40 }; }
    juce::Rectangle<float> dragRow() const { return { 18, 402, panelWidth - 36.0f, 40 }; }
    juce::Rectangle<float> updateRow() const { return { 18, 516, panelWidth - 36.0f, 40 }; }
    juce::Rectangle<float> closeArea() const { return { panelWidth - 46.0f, 14, 30, 30 }; }
    juce::Rectangle<float> chip(size_t i) const { return { 18.0f + (float) (i % 4) * 77.0f, 152.0f + (float) (i / 4) * 38.0f, 70, 32 }; }
    bool isOver(juce::Rectangle<float> r) const { return r.contains(mouse); }
    MelodyCodeProcessor& proc;
    UpdateStatus& update;
    std::function<void()> close, changed, checkUpdate;
    juce::Point<float> mouse { -1, -1 };
};

class DragMidiButton : public ActionButton {
public:
    using ActionButton::ActionButton;
    std::function<juce::StringArray()> files;
    void mouseDrag(const juce::MouseEvent&) override;
};

class LockButton : public juce::Button {
public:
    LockButton() : Button("lock") {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
    bool locked {};
};

class PianoRoll : public juce::Component {
public:
    explicit PianoRoll(MelodyCodeProcessor&);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    static constexpr int headerH = 38, subH = 52, keyW = 52;
private:
    struct Hit { int index { -1 }; bool edge {}; };
    juce::Rectangle<float> noteArea() const;
    float stepW() const;
    float rowH() const;
    double stepAt(float x) const;
    int pitchAt(float y) const;
    Hit hit(juce::Point<float>) const;
    juce::Rectangle<float> noteRect(const mc::Note&) const;
    void commit();

    MelodyCodeProcessor& proc;
    mc::Pattern pat;
    std::vector<mc::Note> notes;
    int lo { 55 }, hi { 79 };
    int selected { -1 };
    struct Drag { int index { -1 }; bool resize {}; double offset {}; int pitch0 {}, startPitch {}; bool moved {}; } drag;
};

class MelodyCodeEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer,
                         private juce::ChangeListener, private juce::Timer {
public:
    explicit MelodyCodeEditor(MelodyCodeProcessor&);
    ~MelodyCodeEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Content : juce::Component {
        MelodyCodeEditor& owner;
        explicit Content(MelodyCodeEditor& o) : owner(o) {}
        void paint(juce::Graphics& g) override { owner.paintContent(g); }
    };
    void paintContent(juce::Graphics&);
    void layout();
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void syncControls();
    juce::StringArray writeMidiFiles(const juce::File& folder, const juce::String& base);
    void saveMidi();
    void refreshAfterSetting(bool harmonyToo);
    void showHint(const juce::String&);

    MelodyCodeProcessor& proc;
    MelodyLook look;
    Content content { *this };
    juce::ComboBox keyBox, scaleBox, styleBox, vibeBox, soundBox;
    PianoRoll roll { proc };
    Segmented role { { "LEAD", "HOOK", "COUNTER" } }, feel { { "STRAIGHT", "SWING", "TRIPLET" } }, density { { "LOW", "MID", "HIGH" } },
        range { { "LOW", "MID", "HIGH" } }, bars { { "2", "4", "8" } };
    juce::Slider bpm, bassVol, melVol;
    ActionButton play { "OUVIR", Icon::play }, vari { juce::String::fromUTF8("VARIAÇÃO"), Icon::variation },
        newMel { "NOVA MELODIA", Icon::random }, gen { "GERAR", Icon::none, true },
        historyBtn { juce::String::fromUTF8("HISTÓRICO"), Icon::history }, save { "SALVAR MIDI", Icon::save };
    DragMidiButton dragBtn { "ARRASTAR MIDI", Icon::midi }, dragWav { "ARRASTAR WAV", Icon::wave };
    IconButton gear { Icon::gear };
    ModalOverlay overlay;
    void openSettings();
    void checkForUpdates(bool silent);
    UpdateStatus update;
    UpdateChecker updateChecker;
    juce::String hint;
    juce::uint32 hintUntil {};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComponentBoundsConstrainer constrainer;
};
