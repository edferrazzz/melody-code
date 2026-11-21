#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <functional>

// Purchase-email license shared by every Melody Code instance on this computer.
// The first activation needs the internet; afterwards the saved license works
// offline and is re-checked whenever the server can be reached. Only an explicit
// "not authorized" answer from the server revokes it, never a network failure.
class LicenseManager final : public juce::ChangeBroadcaster {
public:
    enum class Status { none, active, revoked };

    struct Outcome {
        enum class Kind { authorized, denied, unavailable } kind {};
        juce::String message;
    };

    LicenseManager();
    ~LicenseManager() override;

    Status getStatus() const;
    juce::String getEmail() const;

    // Checks a new purchase email; on success it becomes the saved license.
    void activate(const juce::String& email, std::function<void(Outcome)> completion);
    // Re-checks the saved license in the background, at most every few minutes.
    void revalidate(bool force = false);

    // Default window size (1.0 = 100%), saved in the same per-computer settings file.
    float getWindowScale() const;
    void setWindowScale(float);

    static juce::String normaliseEmail(const juce::String&);
    static bool isValidEmail(const juce::String&);

private:
    class Request;
    void startRequest(const juce::String& email, std::function<void(Outcome)> completion);
    void store(Status, const juce::String& email);

    mutable juce::CriticalSection lock;
    std::unique_ptr<juce::PropertiesFile> file;
    Status status { Status::none };
    juce::String email;
    juce::Time lastCheck;
    juce::ThreadPool pool { juce::ThreadPoolOptions().withThreadName("Melody Code License").withNumberOfThreads(1) };

    JUCE_DECLARE_WEAK_REFERENCEABLE(LicenseManager)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};
