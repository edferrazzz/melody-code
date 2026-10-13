#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

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

class ActionButton : public juce::Button {
public:
    ActionButton(juce::String text, bool accent = false) : Button(text), isAccent(accent) {}
    void paintButton(juce::Graphics&, bool over, bool down) override;
    bool isAccent;
    bool lit {};
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
    void rebuildChordBoxes();

    MelodyCodeProcessor& proc;
    mc::Pattern pat;
    std::vector<mc::Note> notes;
    int lo { 55 }, hi { 79 };
    int selected { -1 };
    struct Drag { int index { -1 }; bool resize {}; double offset {}; int pitch0 {}, startPitch {}; bool moved {}; } drag;
    juce::OwnedArray<juce::ComboBox> chordBoxes;
    juce::OwnedArray<LockButton> lockButtons;
    std::vector<std::vector<ccp::Chord>> choices;
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
    void showHint(const juce::String&);

    MelodyCodeProcessor& proc;
    MelodyLook look;
    Content content { *this };
    juce::ComboBox keyBox, scaleBox, styleBox, vibeBox, soundBox;
    PianoRoll roll { proc };
    Segmented role { { "LEAD", "HOOK", "COUNTER" } }, density { { "LOW", "MID", "HIGH" } },
        range { { "LOW", "MID", "HIGH" } }, bars { { "2", "4", "8" } };
    juce::Slider bpm, subVol, melVol;
    ActionButton play { "OUVIR" }, vari { juce::String::fromUTF8("VARIAÇÃO") }, newMel { "NOVA MELODIA" }, gen { "GERAR", true },
        newSub { "NOVO SUB" }, historyBtn { juce::String::fromUTF8("HISTÓRICO") }, save { "SALVAR MIDI" };
    DragMidiButton dragBtn { "ARRASTAR MIDI" };
    juce::String hint;
    juce::uint32 hintUntil {};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComponentBoundsConstrainer constrainer;
};
