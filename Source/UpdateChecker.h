#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>

class UpdateChecker final : private juce::Thread {
public:
    struct Result {
        bool succeeded {};
        bool updateAvailable {};
        juce::String installedVersion;
        juce::String latestVersion;
        juce::String downloadUrl;
        juce::String releaseNotes;
        juce::String error;
    };

    using Callback = std::function<void(Result)>;

    UpdateChecker();
    ~UpdateChecker() override;

    bool start(juce::String installedVersion, Callback);
    bool isChecking() const { return isThreadRunning(); }
    void cancelAndWait();

private:
    void run() override;
    static int compareVersions(const juce::String&, const juce::String&);

    juce::String installed;
    Callback completionCallback;
    std::atomic<juce::WebInputStream*> activeStream { nullptr };
};
