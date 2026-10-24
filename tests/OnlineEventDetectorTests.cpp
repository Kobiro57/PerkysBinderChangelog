#ifndef KEYBINDER_TESTING
#define KEYBINDER_TESTING
#endif

#include "../include/OnlineEventDetector.h"
#include "../include/SettingsManager.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace keybinder;

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

TestState g_state;

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        g_state.error = "Assertion failed: " #condition; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define ASSERT_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        g_state.error = "Assertion failed: " #actual " == " #expected; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

OnlineEventPlaceholder BuildPlaceholder(const std::string& type,
                                        const std::vector<std::string>& patterns) {
    OnlineEventPlaceholder placeholder;
    placeholder.event_type = type;
    placeholder.patterns = patterns;
    return placeholder;
}

bool TestNormalizeChatLineStripsColorCodesAndTrimsWhitespace() {
    ASSERT_EQ(
        OnlineEventDetector::NormalizeChatLine("  {FFAA00}Gangwar gestartet in LS  \r\n"),
        std::string("Gangwar gestartet in LS"));
    ASSERT_EQ(OnlineEventDetector::NormalizeChatLine(" \t \r\n "), std::string());
    return true;
}

bool TestNormalizeChatLinePreservesNonColorBraces() {
    ASSERT_EQ(
        OnlineEventDetector::NormalizeChatLine(" {warn} Angriff beginnt "),
        std::string("{warn} Angriff beginnt"));
    return true;
}

bool TestMatchesAnyPatternIsCaseInsensitiveSubstringMatch() {
    ASSERT_TRUE(OnlineEventDetector::MatchesAnyPattern(
        "Ballas greifen LSPD an",
        std::vector<std::string>{"ballas", "grove"}));
    ASSERT_TRUE(OnlineEventDetector::MatchesAnyPattern(
        "Ballas greifen LSPD an",
        std::vector<std::string>{"lspd AN"}));
    ASSERT_TRUE(!OnlineEventDetector::MatchesAnyPattern(
        "Ballas greifen LSPD an",
        std::vector<std::string>{"army", "takeover"}));
    return true;
}

bool TestOnChatLineEmitsConfiguredActivationRefreshAndDeactivationEvents() {
    SettingsManager settings;
    settings.GetMutable().online_event_placeholders = {
        BuildPlaceholder("activation", std::vector<std::string>{"gangwar gestartet", "takeover beginnt"}),
        BuildPlaceholder("refresh", std::vector<std::string>{"gangwar laeuft weiter"}),
        BuildPlaceholder("deactivation", std::vector<std::string>{"gangwar beendet"}),
    };

    OnlineEventDetector detector;
    std::vector<OnlineDetectedEvent> events;
    detector.Initialize(&settings);
    detector.SetEventCallback([&events](const OnlineDetectedEvent& event) {
        events.push_back(event);
    });

    detector.OnChatLine(" {ABCDEF}Gangwar gestartet in LS ");
    detector.OnChatLine("gangwar laeuft weiter...");
    detector.OnChatLine("gangwar beendet");

    ASSERT_EQ(events.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(events[0].type, OnlineDetectedEventType::Activation);
    ASSERT_EQ(events[0].normalizedText, std::string("Gangwar gestartet in LS"));
    ASSERT_EQ(events[1].type, OnlineDetectedEventType::Refresh);
    ASSERT_EQ(events[1].normalizedText, std::string("gangwar laeuft weiter..."));
    ASSERT_EQ(events[2].type, OnlineDetectedEventType::Deactivation);
    ASSERT_EQ(events[2].normalizedText, std::string("gangwar beendet"));
    return true;
}

bool TestReloadSettingsUsesUpdatedPatterns() {
    SettingsManager settings;
    settings.GetMutable().online_event_placeholders = {
        BuildPlaceholder("activation", std::vector<std::string>{"alpha"}),
    };

    OnlineEventDetector detector;
    std::vector<OnlineDetectedEvent> events;
    detector.Initialize(&settings);
    detector.SetEventCallback([&events](const OnlineDetectedEvent& event) {
        events.push_back(event);
    });

    detector.OnChatLine("beta");
    ASSERT_EQ(events.size(), static_cast<std::size_t>(0));

    settings.GetMutable().online_event_placeholders = {
        BuildPlaceholder("refresh", std::vector<std::string>{"beta"}),
    };
    detector.ReloadSettings();
    detector.OnChatLine("BETA startet erneut");

    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].type, OnlineDetectedEventType::Refresh);
    ASSERT_EQ(events[0].normalizedText, std::string("BETA startet erneut"));
    return true;
}

bool TestOnChatLineIgnoresUnknownEventTypes() {
    SettingsManager settings;
    settings.GetMutable().online_event_placeholders = {
        BuildPlaceholder("mystery", std::vector<std::string>{"angriff"}),
    };

    OnlineEventDetector detector;
    std::vector<OnlineDetectedEvent> events;
    detector.Initialize(&settings);
    detector.SetEventCallback([&events](const OnlineDetectedEvent& event) {
        events.push_back(event);
    });

    detector.OnChatLine("Angriff beginnt jetzt");
    ASSERT_EQ(events.size(), static_cast<std::size_t>(0));
    return true;
}

bool TestOnChatLineRecognizesLegacyDefaultActivationTypes() {
    SettingsManager settings;
    settings.GetMutable().online_event_placeholders = {
        BuildPlaceholder("gangwar", std::vector<std::string>{"gangwar"}),
        BuildPlaceholder("revierkampf", std::vector<std::string>{"revierkampf"}),
        BuildPlaceholder("takeover", std::vector<std::string>{"takeover"}),
    };

    OnlineEventDetector detector;
    std::vector<OnlineDetectedEvent> events;
    detector.Initialize(&settings);
    detector.SetEventCallback([&events](const OnlineDetectedEvent& event) {
        events.push_back(event);
    });

    detector.OnChatLine("Gangwar gestartet");
    detector.OnChatLine("Revierkampf aktiv");
    detector.OnChatLine("Takeover beginnt");

    ASSERT_EQ(events.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(events[0].type, OnlineDetectedEventType::Activation);
    ASSERT_EQ(events[1].type, OnlineDetectedEventType::Activation);
    ASSERT_EQ(events[2].type, OnlineDetectedEventType::Activation);
    return true;
}

} // namespace

int RunOnlineEventDetectorTests() {
    g_state = {};

    RUN_TEST(TestNormalizeChatLineStripsColorCodesAndTrimsWhitespace);
    RUN_TEST(TestNormalizeChatLinePreservesNonColorBraces);
    RUN_TEST(TestMatchesAnyPatternIsCaseInsensitiveSubstringMatch);
    RUN_TEST(TestOnChatLineEmitsConfiguredActivationRefreshAndDeactivationEvents);
    RUN_TEST(TestReloadSettingsUsesUpdatedPatterns);
    RUN_TEST(TestOnChatLineIgnoresUnknownEventTypes);
    RUN_TEST(TestOnChatLineRecognizesLegacyDefaultActivationTypes);

    std::printf("OnlineEventDetectorTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
