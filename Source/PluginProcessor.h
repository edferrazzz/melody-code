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
    bool producesMidi() const override { return false; }
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
    void newBass();
    void variation();
    void setChord(int bar, const ccp::Chord&);
    void setMelody(std::vector<mc::Note>);
    void applySettings();       // key/scale/style/vibe/bars changed: regenerate unlocked bars
    void toggleLock(int bar) { locks[(size_t) bar] = !locks[(size_t) bar]; sendChangeMessage(); }
    bool isLocked(int bar) const { return locks[(size_t) bar]; }
    int historySize() const { return (int) history.size(); }
    juce::String historyLabel(int) const;
    void restoreHistory(int);

    void setPlaying(bool);
    bool isPlaying() const { return playing.load(); }
    double playheadTicks() const { return playhead.load(); }
    void audition(int pitch) { pendingAudition.store(pitch); }

    int bpm { 120 };
    int preset { 0 };
    float subLevel { 0.8f }, melodyLevel { 0.8f };

private:
    void publish(mc::Pattern, bool addHistory = true);
    std::array<bool, mc::maxBars> lockArray() const;

    mc::Engine engine;
    mc::Settings current;
    mc::Pattern shown;                      // message thread copy
    mc::Pattern audioPattern;               // audio thread copy
    juce::SpinLock patternLock;
    std::atomic<bool> resetPosition { false };
    std::vector<mc::Pattern> history;
    std::array<bool, mc::maxBars> locks {};

    Synth synth;
    double sr { 44100.0 };
    std::atomic<bool> playing { false };
    std::atomic<double> playhead { -1.0 };
    double position {};
    std::atomic<int> pendingAudition { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodyCodeProcessor)
};
