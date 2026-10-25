#define KEYBINDER_TESTING

#include "../include/JobChatAdapter.h"
#include "../include/SAMPBridge.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

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

struct IncomingChatHookContext {
    std::function<void(sampapi::v03dl::CChat*, const char*, unsigned long, const char*)> interceptor;

    SAMPBridge::TestingHooks BuildHooks() {
        SAMPBridge::TestingHooks hooks{};
        hooks.installIncomingChatInterceptor = [this](const std::function<void(sampapi::v03dl::CChat*, const char*, unsigned long, const char*)>& callback) {
            interceptor = callback;
        };
        return hooks;
    }

    void Emit(const char* prefix, const char* text) {
        if (interceptor) {
            interceptor(nullptr, prefix, 0xFFFFFFFFUL, text);
        }
    }
};

bool TestCapturesAndNormalizesLandwirtCompletion() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    jobs::JobChatAdapter adapter;
    std::vector<jobs::JobChatEvent> events;
    adapter.SetCallback([&events](const jobs::JobChatEvent& event) {
        events.push_back(event);
    });

    ASSERT_TRUE(adapter.Initialize());
    context.Emit(nullptr, "[18:28:35] Landwirt: {FFFFFF}Für das erfolgreiche Beenden der Route erhältst du $1234.");

    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].rawLine, std::string("[18:28:35] Landwirt: {FFFFFF}Für das erfolgreiche Beenden der Route erhältst du $1234."));
    ASSERT_EQ(events[0].timestampFreeLine, std::string("Landwirt: {FFFFFF}Für das erfolgreiche Beenden der Route erhältst du $1234."));
    ASSERT_EQ(events[0].normalizedLine, std::string("Landwirt: Für das erfolgreiche Beenden der Route erhältst du $1234."));
    ASSERT_EQ(events[0].jobPrefix, std::string("Landwirt"));
    ASSERT_TRUE(events[0].arrivalTime != 0);

    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestCombinesPrefixAndTextBeforeNormalization() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    jobs::JobChatAdapter adapter;
    std::vector<jobs::JobChatEvent> events;
    adapter.SetCallback([&events](const jobs::JobChatEvent& event) {
        events.push_back(event);
    });

    ASSERT_TRUE(adapter.Initialize());
    context.Emit("[18:28:35] Landwirt", "{FFFFFF}Route abgeschlossen");

    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].rawLine, std::string("[18:28:35] Landwirt: {FFFFFF}Route abgeschlossen"));
    ASSERT_EQ(events[0].normalizedLine, std::string("Landwirt: Route abgeschlossen"));
    ASSERT_EQ(events[0].jobPrefix, std::string("Landwirt"));

    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestCapturesLiveSplitLandwirtCompletionFormat() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    jobs::JobChatAdapter adapter;
    std::vector<jobs::JobChatEvent> events;
    adapter.SetCallback([&events](const jobs::JobChatEvent& event) {
        events.push_back(event);
    });

    ASSERT_TRUE(adapter.Initialize());
    context.Emit("Landwirt", "{FFFFFF}Für das erfolgreiche Beenden deiner Tätigkeit erhältst du 151$ zum nächsten Zahltag.");

    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].rawLine, std::string("Landwirt: {FFFFFF}Für das erfolgreiche Beenden deiner Tätigkeit erhältst du 151$ zum nächsten Zahltag."));
    ASSERT_EQ(events[0].normalizedLine, std::string("Landwirt: Für das erfolgreiche Beenden deiner Tätigkeit erhältst du 151$ zum nächsten Zahltag."));
    ASSERT_EQ(events[0].jobPrefix, std::string("Landwirt"));

    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestConvertsAnsiUmlautCompletionLineToUtf8() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    jobs::JobChatAdapter adapter;
    std::vector<jobs::JobChatEvent> events;
    adapter.SetCallback([&events](const jobs::JobChatEvent& event) {
        events.push_back(event);
    });

    ASSERT_TRUE(adapter.Initialize());

    std::string ansiText = "{FFFFFF}F";
    ansiText.push_back(static_cast<char>(0xFC)); // ü in CP1252
    ansiText += "r das erfolgreiche Beenden deiner T";
    ansiText.push_back(static_cast<char>(0xE4)); // ä in CP1252
    ansiText += "tigkeit erh";
    ansiText.push_back(static_cast<char>(0xE4)); // ä in CP1252
    ansiText += "ltst du 151$ zum n";
    ansiText.push_back(static_cast<char>(0xE4)); // ä in CP1252
    ansiText += "chsten Zahltag.";

    context.Emit("Landwirt", ansiText.c_str());

    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].normalizedLine, std::string("Landwirt: Für das erfolgreiche Beenden deiner Tätigkeit erhältst du 151$ zum nächsten Zahltag."));
    ASSERT_EQ(events[0].jobPrefix, std::string("Landwirt"));

    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestEmptyAndMalformedLinesAreIgnoredSafely() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    jobs::JobChatAdapter adapter;
    int callbacks = 0;
    adapter.SetCallback([&callbacks](const jobs::JobChatEvent&) {
        ++callbacks;
    });

    ASSERT_TRUE(adapter.Initialize());
    context.Emit(nullptr, "");
    context.Emit(nullptr, "    ");
    context.Emit(nullptr, "{FFFFFF}");
    context.Emit(nullptr, "[18:28:35]   {FFFFFF}   ");
    ASSERT_EQ(callbacks, 0);

    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestShutdownRemovesTestingInterceptor() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    jobs::JobChatAdapter adapter;
    ASSERT_TRUE(adapter.Initialize());
    ASSERT_TRUE(static_cast<bool>(context.interceptor));

    adapter.Shutdown();
    ASSERT_TRUE(!context.interceptor);

    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestVisibilityDefaultsToShownAndHiddenLinesStillReachCallback() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());
    jobs::JobChatAdapter adapter;
    std::vector<std::string> order;
    std::vector<jobs::JobChatEvent> received;
    adapter.SetCallback([&](const jobs::JobChatEvent& event) {
        order.push_back("callback:" + event.normalizedLine);
        received.push_back(event);
    });
    ASSERT_TRUE(adapter.Initialize());
    ASSERT_TRUE(adapter.DispatchLineForTesting(nullptr, "Normale Chatzeile"));
    ASSERT_EQ(received.size(), static_cast<std::size_t>(1));

    adapter.SetVisibilityFilter([&](const jobs::JobChatEvent& event) {
        order.push_back("filter:" + event.normalizedLine);
        return event.normalizedLine.find("Kills:") != 0;
    });
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, "[18:28:35] {FFFFFF}Kills: [64] Tode: [205]"));
    ASSERT_EQ(received.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(order[1], std::string("filter:Kills: [64] Tode: [205]"));
    ASSERT_EQ(order[2], std::string("callback:Kills: [64] Tode: [205]"));
    ASSERT_TRUE(adapter.DispatchLineForTesting("Landwirt", "Route abgeschlossen"));
    ASSERT_EQ(received.back().jobPrefix, std::string("Landwirt"));
    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestVisibilityFilterReceivesEmptyLinesWithoutNormalCallbacks() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());
    jobs::JobChatAdapter adapter;
    int filters = 0;
    int callbacks = 0;
    bool allNormalizedEmpty = true;
    adapter.SetCallback([&](const jobs::JobChatEvent&) { ++callbacks; });
    adapter.SetVisibilityFilter([&](const jobs::JobChatEvent& event) {
        ++filters;
        allNormalizedEmpty = allNormalizedEmpty && event.normalizedLine.empty();
        return false;
    });
    ASSERT_TRUE(adapter.Initialize());
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, ""));
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, "{FFFFFF}  "));
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, "[18:28:35] {FFFFFF}  "));
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, nullptr));
    ASSERT_EQ(filters, 4);
    ASSERT_EQ(callbacks, 0);
    ASSERT_TRUE(allNormalizedEmpty);
    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestVisibilityFilterNormalizesEncodingWithoutAnEventCallback() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());
    jobs::JobChatAdapter adapter;
    std::string normalized;
    adapter.SetVisibilityFilter([&](const jobs::JobChatEvent& event) {
        normalized = event.normalizedLine;
        return false;
    });
    ASSERT_TRUE(adapter.Initialize());
    std::string ansi = "[18:28:35] {FFFFFF}Verm";
    ansi.push_back(static_cast<char>(0xF6));
    ansi += "gen: [1.234$]";
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, ansi.c_str()));
    ASSERT_EQ(normalized, std::string("Vermögen: [1.234$]"));
    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestShutdownClearsVisibilityFilterIncludingBeforeInitialization() {
    IncomingChatHookContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());
    jobs::JobChatAdapter adapter;
    int calls = 0;
    adapter.SetVisibilityFilter([&](const jobs::JobChatEvent&) { ++calls; return false; });
    adapter.Shutdown();
    ASSERT_TRUE(adapter.Initialize());
    ASSERT_TRUE(adapter.DispatchLineForTesting(nullptr, "Nicht ausblenden"));
    ASSERT_EQ(calls, 0);
    adapter.SetVisibilityFilter([&](const jobs::JobChatEvent&) { ++calls; return false; });
    ASSERT_TRUE(!adapter.DispatchLineForTesting(nullptr, "Ausblenden"));
    ASSERT_EQ(calls, 1);
    adapter.Shutdown();
    ASSERT_TRUE(adapter.DispatchLineForTesting(nullptr, "Nach Shutdown anzeigen"));
    ASSERT_EQ(calls, 1);
    ASSERT_TRUE(adapter.Initialize());
    ASSERT_TRUE(adapter.DispatchLineForTesting(nullptr, "Nach Neustart anzeigen"));
    ASSERT_EQ(calls, 1);
    adapter.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

} // namespace

int RunJobChatAdapterTests() {
    g_state = {};

    RUN_TEST(TestCapturesAndNormalizesLandwirtCompletion);
    RUN_TEST(TestCombinesPrefixAndTextBeforeNormalization);
    RUN_TEST(TestCapturesLiveSplitLandwirtCompletionFormat);
    RUN_TEST(TestConvertsAnsiUmlautCompletionLineToUtf8);
    RUN_TEST(TestEmptyAndMalformedLinesAreIgnoredSafely);
    RUN_TEST(TestShutdownRemovesTestingInterceptor);
    RUN_TEST(TestVisibilityDefaultsToShownAndHiddenLinesStillReachCallback);
    RUN_TEST(TestVisibilityFilterReceivesEmptyLinesWithoutNormalCallbacks);
    RUN_TEST(TestVisibilityFilterNormalizesEncodingWithoutAnEventCallback);
    RUN_TEST(TestShutdownClearsVisibilityFilterIncludingBeforeInitialization);

    std::printf("JobChatAdapterTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
