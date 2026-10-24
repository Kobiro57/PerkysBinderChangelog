#pragma once

#include <functional>
#include <string>
#include <vector>

namespace keybinder {

struct OnlineEventPlaceholder;
class SettingsManager;

// ---------------------------------------------------------------------------
// Event types produced by the detector
// ---------------------------------------------------------------------------
enum class OnlineDetectedEventType {
    None,        // no match
    Activation,  // start of hostile event
    Refresh,     // refresh/extend hostile event
    Deactivation,// end of hostile event
};

struct OnlineDetectedEvent {
    OnlineDetectedEventType type = OnlineDetectedEventType::None;
    std::string matchedPattern;  // the pattern string that matched
    std::string normalizedText;  // the normalized chat line
};

// ---------------------------------------------------------------------------
// OnlineEventDetector — matches incoming chat lines against placeholder
// patterns from settings and emits detected events.
// ---------------------------------------------------------------------------
class OnlineEventDetector {
public:
    using EventCallback = std::function<void(const OnlineDetectedEvent&)>;

    OnlineEventDetector();
    ~OnlineEventDetector();

    void Initialize(SettingsManager* settings);
    void Shutdown();

    // Feed a raw chat line (with color codes etc.) to be checked.
    void OnChatLine(const std::string& text);

    void SetEventCallback(EventCallback callback);
    void ReloadSettings();

    // Static helpers for testing
    static std::string NormalizeChatLine(const std::string& text);
    static bool MatchesAnyPattern(const std::string& normalized,
                                   const std::vector<std::string>& patterns);

private:
    OnlineDetectedEvent ClassifyLine(const std::string& normalized) const;

    SettingsManager* settingsManager_ = nullptr;
    EventCallback eventCallback_;
};

} // namespace keybinder
