#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MelodyEngine.h"
#include "Synth.h"

class MelodyCodeProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster {
public:
    MelodyCodeProcessor();

    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // message-thread API
    mc::Pattern pattern() const;
    mc::Settings& settings() { return current; }
    void generateAll();
    void newMelody();
    void variation();
    void setMelody(std::vector<mc::Note>);
    void applySettings();       // key/scale/style/bars changed: generate again
    void setVibe(mc::Vibe);     // preview sound now, phrase character from the next generation
    bool melodyEdited() const { return edited; }
    int historySize() const { return (int) history.size(); }
    juce::String historyLabel(int) const;
    void restoreHistory(int);

    void setPlaying(bool);
    bool isPlaying() const { return playing.load(); }
    double playheadTicks() const { return playhead.load(); }
    void audition(int pitch) { pendingAudition.store(pitch); }

    bool hostIsPlaying() const { return hostPlaying.load(); }

    int bpm { 120 };
    std::atomic<bool> syncToHost { true };   // follow the DAW transport and tempo
    std::atomic<bool> midiOut { true };      // also send the notes out as MIDI (melody ch 1, bass ch 2)
    int preset { 0 };
    // BASS and MELODY are the preview mix levels of the two layers (low line, main line). They never change notes:
    // the composition is controlled by Role / Feel / Density / Range, so these stay a balance control.
    float bassLevel { 0.7f }, melodyLevel { 0.8f };

private:
    void publish(mc::Pattern, bool addHistory = true);

    mc::Engine engine;
    mc::Settings current;
    mc::Pattern shown;                      // message thread copy
    mc::Pattern audioPattern;               // audio thread copy
    juce::SpinLock patternLock;
    std::atomic<bool> resetPosition { false };
    std::vector<mc::Pattern> history;
    bool edited {};                         // notes changed by hand since the last generation

    Synth synth;
    double sr { 44100.0 };
    std::atomic<bool> playing { false };
    std::atomic<double> playhead { -1.0 };
    double position {};
    std::atomic<int> pendingAudition { -1 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<int> soundVibe { (int) mc::Vibe::dreamy };
    bool wasRunning {};
    struct Off { int pitch, channel; double samplesLeft; };
    std::vector<Off> midiOffs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodyCodeProcessor)
};
