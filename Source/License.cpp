#include "License.h"
#include <array>

namespace {
constexpr auto endpoint = "https://download.chordcode.audiomakers.club/api/license/check";
constexpr int maximumResponseBytes = 16 * 1024;
const auto revalidateInterval = juce::RelativeTime::minutes(10);

juce::String utf(const char* text) { return juce::String::fromUTF8(text); }

juce::PropertiesFile::Options storageOptions() {
    juce::PropertiesFile::Options options;
    options.applicationName = "Melody Code License";
    options.folderName = "Audio Makers" + juce::File::getSeparatorString() + "Melody Code";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    options.processLock = nullptr;
    return options;
}

juce::String statusName(LicenseManager::Status status) {
    switch (status) {
        case LicenseManager::Status::active: return "active";
        case LicenseManager::Status::revoked: return "revoked";
        case LicenseManager::Status::none: break;
    }
    return "none";
}
}

class LicenseManager::Request final : public juce::ThreadPoolJob {
public:
    Request(juce::WeakReference<LicenseManager> o, juce::String e, std::function<void(Outcome)> c)
        : juce::ThreadPoolJob("License check"), owner(std::move(o)), email(std::move(e)), completion(std::move(c)) {}

    JobStatus runJob() override {
        Outcome outcome { Outcome::Kind::unavailable,
                          utf("Não foi possível falar com o servidor. Confira sua conexão e tente novamente.") };

        auto body = std::make_unique<juce::DynamicObject>();
        body->setProperty("email", email);
        body->setProperty("product", "melody-code");
        body->setProperty("version", JucePlugin_VersionString);
        const auto url = juce::URL(endpoint).withPOSTData(juce::JSON::toString(juce::var(body.release()), true));

        juce::WebInputStream stream(url, true);
        stream.withConnectionTimeout(8000).withNumRedirectsToFollow(0)
              .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\nUser-Agent: Melody-Code/"
                                JucePlugin_VersionString "\r\n");
        if (stream.connect(nullptr) && !shouldExit()) {
            const int code = stream.getStatusCode();
            juce::MemoryOutputStream output;
            std::array<char, 2048> buffer {};
            while (!shouldExit() && !stream.isExhausted() && output.getDataSize() <= maximumResponseBytes) {
                const int read = stream.read(buffer.data(), (int) buffer.size());
                if (read <= 0) break;
                output.write(buffer.data(), (size_t) read);
            }
            const auto json = juce::JSON::parse(output.toString());
            auto* object = json.getDynamicObject();
            // Only a well-formed 200 answer can grant or revoke access.
            if (code == 200 && object != nullptr && object->hasProperty("authorized")) {
                const bool authorized = (bool) object->getProperty("authorized");
                outcome.kind = authorized ? Outcome::Kind::authorized : Outcome::Kind::denied;
                outcome.message = authorized ? utf("Licença ativada.")
                                             : utf("Não encontramos um acesso ativo ao Melody Code para este e-mail. "
                                                   "Use o mesmo e-mail da compra.");
            } else if (code == 400) {
                outcome.message = utf("Digite um e-mail válido.");
            }
        }

        if (shouldExit()) return jobHasFinished;
        juce::MessageManager::callAsync([o = owner, e = email, c = std::move(completion), outcome] {
            if (auto* manager = o.get()) {
                if (outcome.kind == Outcome::Kind::authorized) manager->store(Status::active, e);
                else if (outcome.kind == Outcome::Kind::denied && e == manager->getEmail()
                         && manager->getStatus() == Status::active)
                    manager->store(Status::revoked, e);
            }
            if (c) c(outcome);
        });
        return jobHasFinished;
    }

private:
    juce::WeakReference<LicenseManager> owner;
    juce::String email;
    std::function<void(Outcome)> completion;
};

LicenseManager::LicenseManager() : file(std::make_unique<juce::PropertiesFile>(storageOptions())) {
    email = normaliseEmail(file->getValue("email"));
    const auto saved = file->getValue("status");
    status = !isValidEmail(email) ? Status::none : saved == "active" ? Status::active
                                                 : saved == "revoked" ? Status::revoked : Status::none;
}

LicenseManager::~LicenseManager() {
    pool.removeAllJobs(true, 4000);
    masterReference.clear();
}

LicenseManager::Status LicenseManager::getStatus() const { const juce::ScopedLock sl(lock); return status; }
juce::String LicenseManager::getEmail() const { const juce::ScopedLock sl(lock); return email; }

juce::String LicenseManager::normaliseEmail(const juce::String& value) { return value.trim().toLowerCase(); }

bool LicenseManager::isValidEmail(const juce::String& value) {
    const auto at = value.indexOfChar('@');
    return value.length() <= 254 && !value.containsAnyOf(" \t\r\n") && at > 0
        && value.lastIndexOfChar('@') == at && value.substring(at + 1).containsChar('.')
        && !value.endsWithChar('.');
}

void LicenseManager::activate(const juce::String& candidate, std::function<void(Outcome)> completion) {
    const auto normalised = normaliseEmail(candidate);
    if (!isValidEmail(normalised)) {
        if (completion) completion({ Outcome::Kind::denied, utf("Digite um e-mail válido.") });
        return;
    }
    startRequest(normalised, std::move(completion));
}

void LicenseManager::revalidate(bool force) {
    juce::String current;
    {
        const juce::ScopedLock sl(lock);
        if (status != Status::active || !isValidEmail(email)) return;
        if (!force && juce::Time::getCurrentTime() - lastCheck < revalidateInterval) return;
        lastCheck = juce::Time::getCurrentTime();
        current = email;
    }
    startRequest(current, {});
}

void LicenseManager::startRequest(const juce::String& candidate, std::function<void(Outcome)> completion) {
    pool.addJob(new Request(this, candidate, std::move(completion)), true);
}

void LicenseManager::store(Status newStatus, const juce::String& newEmail) {
    {
        const juce::ScopedLock sl(lock);
        status = newStatus;
        email = newEmail;
        lastCheck = juce::Time::getCurrentTime();
        file->setValue("email", email);
        file->setValue("status", statusName(status));
        file->setValue("verifiedAt", juce::Time::getCurrentTime().toISO8601(true));
        file->saveIfNeeded();
    }
    sendChangeMessage();
}

float LicenseManager::getWindowScale() const {
    const juce::ScopedLock sl(lock);
    return juce::jlimit(0.5f, 2.0f, (float) file->getDoubleValue("windowScale", 1.0));
}

void LicenseManager::setWindowScale(float scale) {
    const juce::ScopedLock sl(lock);
    file->setValue("windowScale", (double) scale);
    file->saveIfNeeded();
}
