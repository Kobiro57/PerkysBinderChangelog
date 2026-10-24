#include "../include/OnlineEventDetector.h"
#include "../include/SettingsManager.h"
#include "../include/Logger.h"

#include <algorithm>
#include <cctype>

namespace keybinder {

namespace {

bool ContainsInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) {
        return false;
    }

    const auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        });
    return it != haystack.end();
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

OnlineDetectedEventType ParseEventType(const std::string& value) {
    const std::string typeLower = ToLowerCopy(value);
    if (typeLower == "activation" || typeLower == "gangwar" || typeLower == "revierkampf" || typeLower == "takeover") {
        return OnlineDetectedEventType::Activation;
    }

    if (typeLower == "refresh") {
        return OnlineDetectedEventType::Refresh;
    }

    if (typeLower == "deactivation") {
        return OnlineDetectedEventType::Deactivation;
    }

    return OnlineDetectedEventType::None;
}

} // namespace

OnlineEventDetector::OnlineEventDetector() = default;

OnlineEventDetector::~OnlineEventDetector() {
    Shutdown();
}

void OnlineEventDetector::Initialize(SettingsManager* settings) {
    settingsManager_ = settings;
}

void OnlineEventDetector::Shutdown() {
    settingsManager_ = nullptr;
    eventCallback_ = nullptr;
}

void OnlineEventDetector::OnChatLine(const std::string& text) {
    if (eventCallback_ == nullptr || settingsManager_ == nullptr) {
        return;
    }

    const std::string normalized = NormalizeChatLine(text);
    if (normalized.empty()) {
        return;
    }

    OnlineDetectedEvent event = ClassifyLine(normalized);
    if (event.type != OnlineDetectedEventType::None) {
        event.normalizedText = normalized;
        eventCallback_(event);
    }
}

void OnlineEventDetector::SetEventCallback(EventCallback callback) {
    eventCallback_ = std::move(callback);
}

void OnlineEventDetector::ReloadSettings() {
    // Settings are read dynamically from settingsManager_ on each call;
    // nothing extra to cache.
}

std::string OnlineEventDetector::NormalizeChatLine(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    std::size_t i = 0;
    while (i < text.size()) {
        // SA-MP color codes: {RRGGBB}
        if (text[i] == '{') {
            // Skip until closing brace or end
            std::size_t close = text.find('}', i + 1);
            if (close != std::string::npos && close - i == 7) {
                // Looks like a color code — skip it
                i = close + 1;
                continue;
            }
        }
        result += text[i];
        ++i;
    }

    // Trim leading/trailing whitespace
    const std::size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t end = result.find_last_not_of(" \t\r\n");
    result = result.substr(start, end - start + 1);

    return result;
}

bool OnlineEventDetector::MatchesAnyPattern(const std::string& normalized,
                                              const std::vector<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        if (ContainsInsensitive(normalized, pattern)) {
            return true;
        }
    }
    return false;
}

OnlineDetectedEvent OnlineEventDetector::ClassifyLine(const std::string& normalized) const {
    const Settings& s = settingsManager_->Get();

    for (const auto& placeholder : s.online_event_placeholders) {
        if (placeholder.patterns.empty()) {
            continue;
        }

        const std::string* matchedPattern = nullptr;
        for (const auto& pattern : placeholder.patterns) {
            if (ContainsInsensitive(normalized, pattern)) {
                matchedPattern = &pattern;
                break;
            }
        }

        if (matchedPattern == nullptr) {
            continue;
        }

        OnlineDetectedEvent event;
        event.matchedPattern = *matchedPattern;
        event.normalizedText = normalized;

        event.type = ParseEventType(placeholder.event_type);
        if (event.type == OnlineDetectedEventType::None) {
            // Unknown event type — skip
            continue;
        }

        return event;
    }

    return {};
}

} // namespace keybinder
