#include "UpdateChecker.h"

namespace {
constexpr auto endpoint = "https://updates.audiomakers.club/melody-code.json";
constexpr int maximumResponseBytes = 64 * 1024;
}

UpdateChecker::UpdateChecker() : juce::Thread("Melody Code Update Checker") {}

UpdateChecker::~UpdateChecker() {
    cancelAndWait();
}

bool UpdateChecker::start(juce::String installedVersion, Callback completion) {
    if (isThreadRunning()) return false;
    installed = std::move(installedVersion);
    completionCallback = std::move(completion);
    return startThread(juce::Thread::Priority::low);
}

void UpdateChecker::cancelAndWait() {
    signalThreadShouldExit();
    if (auto* stream = activeStream.load(std::memory_order_acquire)) stream->cancel();
    stopThread(6000);
    activeStream.store(nullptr, std::memory_order_release);
    completionCallback = {};
}

int UpdateChecker::compareVersions(const juce::String& a, const juce::String& b) {
    const auto left = juce::StringArray::fromTokens(a.trim(), ".", "");
    const auto right = juce::StringArray::fromTokens(b.trim(), ".", "");
    for (int i = 0; i < juce::jmax(3, juce::jmax(left.size(), right.size())); ++i) {
        const int l = i < left.size() ? left[i].getIntValue() : 0;
        const int r = i < right.size() ? right[i].getIntValue() : 0;
        if (l != r) return l < r ? -1 : 1;
    }
    return 0;
}

void UpdateChecker::run() {
    Result result;
    result.installedVersion = installed;

    juce::WebInputStream stream(juce::URL(endpoint), false);
    stream.withConnectionTimeout(5000).withNumRedirectsToFollow(3)
          .withExtraHeaders("Accept: application/json\r\nUser-Agent: Melody-Code/" + installed + "\r\n");
    activeStream.store(&stream, std::memory_order_release);

    if (!stream.connect(nullptr) || stream.isError() || stream.getStatusCode() != 200) {
        result.error = juce::String::fromUTF8("Não foi possível verificar atualizações. Confira sua conexão e tente novamente.");
    } else if (stream.getTotalLength() > maximumResponseBytes) {
        result.error = juce::String::fromUTF8("O servidor retornou uma resposta inválida.");
    } else {
        juce::MemoryOutputStream output;
        std::array<char, 4096> buffer {};
        while (!threadShouldExit() && !stream.isExhausted() && output.getDataSize() <= maximumResponseBytes) {
            const int read = stream.read(buffer.data(), (int) buffer.size());
            if (read <= 0) break;
            output.write(buffer.data(), (size_t) read);
        }

        if (!threadShouldExit() && output.getDataSize() <= maximumResponseBytes) {
            const auto value = juce::JSON::parse(output.toString());
            if (auto* object = value.getDynamicObject()) {
                const auto product = object->getProperty("product").toString();
                result.latestVersion = object->getProperty("latestVersion").toString().trim();
                result.downloadUrl = object->getProperty("downloadUrl").toString().trim();
                result.releaseNotes = object->getProperty("releaseNotes").toString().trim();
                const juce::URL download(result.downloadUrl);
                if (product == "melody-code" && result.latestVersion.isNotEmpty()
                    && download.getScheme().equalsIgnoreCase("https")) {
                    result.succeeded = true;
                    result.updateAvailable = compareVersions(result.installedVersion, result.latestVersion) < 0;
                } else {
                    result.error = juce::String::fromUTF8("O servidor retornou dados de atualização inválidos.");
                }
            } else {
                result.error = juce::String::fromUTF8("O servidor retornou um JSON inválido.");
            }
        }
    }

    activeStream.store(nullptr, std::memory_order_release);
    if (threadShouldExit()) return;
    auto callbackForMessageThread = completionCallback;
    auto resultForMessageThread = std::move(result);
    juce::MessageManager::callAsync([callbackToRun = std::move(callbackForMessageThread),
                                     resultToDeliver = std::move(resultForMessageThread)]() mutable {
        if (callbackToRun) callbackToRun(std::move(resultToDeliver));
    });
}
