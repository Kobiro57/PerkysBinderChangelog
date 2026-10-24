#define KEYBINDER_TESTING

#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/KillTracker.h"
#include "../include/KillTrackerTypes.h"
#include "../include/WeaponNames.h"
#include "../include/StatsStorage.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <ctime>
#include <fstream>

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

struct MockContext {
    std::aligned_storage_t<sizeof(sampapi::v03dl::CInput), alignof(sampapi::v03dl::CInput)> inputStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CChat), alignof(sampapi::v03dl::CChat)> chatStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CDialog), alignof(sampapi::v03dl::CDialog)> dialogStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CGame), alignof(sampapi::v03dl::CGame)> gameStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CLocalPlayer), alignof(sampapi::v03dl::CLocalPlayer)> localPlayerStorage{};

    sampapi::v03dl::CInput* input = nullptr;
    sampapi::v03dl::CChat* chat = nullptr;
    sampapi::v03dl::CDialog* dialog = nullptr;
    sampapi::v03dl::CGame* game = nullptr;
    sampapi::v03dl::CLocalPlayer* localPlayer = nullptr;

    HMODULE moduleHandle = reinterpret_cast<HMODULE>(0x1);
    rakhook::samp_ver version = rakhook::samp_ver::v03dlr1;
    bool rakhookInitResult = true;
    bool inputReady = true;
    bool chatReady = true;
    bool localPlayerReady = true;
    int rakHookInitCalls = 0;
    int rakHookDestroyCalls = 0;
    std::string lastSent;
    std::string lastLocalMessage;
    std::unordered_map<std::string, sampapi::CMDPROC> commandHandlers;
    std::vector<std::string> logs;
    std::function<void(sampapi::v03dl::CChat*, const char*, unsigned long, const char*)> incomingChatInterceptor;

    MockContext() {
        input = reinterpret_cast<sampapi::v03dl::CInput*>(&inputStorage);
        chat = reinterpret_cast<sampapi::v03dl::CChat*>(&chatStorage);
        dialog = reinterpret_cast<sampapi::v03dl::CDialog*>(&dialogStorage);
        game = reinterpret_cast<sampapi::v03dl::CGame*>(&gameStorage);
        localPlayer = reinterpret_cast<sampapi::v03dl::CLocalPlayer*>(&localPlayerStorage);

        std::memset(input, 0, sizeof(*input));
        std::memset(chat, 0, sizeof(*chat));
        std::memset(dialog, 0, sizeof(*dialog));
        std::memset(game, 0, sizeof(*game));
        std::memset(localPlayer, 0, sizeof(*localPlayer));
    }

    SAMPBridge::TestingHooks BuildHooks() {
        return {
            [this](const char*) { return moduleHandle; },
            []() { return static_cast<uintptr_t>(1); },
            [this]() { return version; },
            [this]() {
                ++rakHookInitCalls;
                return rakhookInitResult;
            },
            [this]() { ++rakHookDestroyCalls; },
            [this]() { return inputReady ? input : nullptr; },
            [this]() { return chatReady ? chat : nullptr; },
            [this]() { return dialog; },
            [this]() { return game; },
            []() -> sampapi::v03dl::CNetGame* { return nullptr; },
            [this]() { return localPlayerReady ? localPlayer : nullptr; },
            [this](sampapi::v03dl::CInput*, const char* text) {
                lastSent = text != nullptr ? text : "";
            },
            [this](sampapi::v03dl::CLocalPlayer*, const char* text) {
                lastSent = text != nullptr ? text : "";
            },
            [this](sampapi::v03dl::CChat*, unsigned long, const char* text) {
                lastLocalMessage = text != nullptr ? text : "";
            },
            [this](sampapi::v03dl::CInput*, const char* name) -> sampapi::CMDPROC {
                const auto it = commandHandlers.find(name != nullptr ? name : "");
                return it != commandHandlers.end() ? it->second : nullptr;
            },
            [this](sampapi::v03dl::CInput* inputRef, const char* name, sampapi::CMDPROC handler) {
                commandHandlers[name != nullptr ? name : ""] = handler;
                ++inputRef->m_nCommandCount;
            },
            [](sampapi::v03dl::CInput* inputRef, sampapi::CMDPROC handler) {
                inputRef->m_pDefaultCommand = handler;
            },
            [this](const std::function<void(sampapi::v03dl::CChat*, const char*, unsigned long, const char*)>& callback) {
                incomingChatInterceptor = callback;
            },
            [this](const std::string& message) { logs.push_back(message); },
        };
    }
};

bool TestInitFailureWithoutSampModule() {
    MockContext context;
    context.moduleHandle = nullptr;

    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(!bridge.Initialize());
    ASSERT_EQ(bridge.GetVersion(), SAMPBridge::Version::Unknown);
    ASSERT_EQ(context.rakHookInitCalls, 0);

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestInitSuccessAndDirectCommandRegistration() {
    MockContext context;
    bool kbTriggered = false;
    bool keybinderTriggered = false;

    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.RegisterCommand("kb", [&](const std::string&) { kbTriggered = true; }));
    ASSERT_TRUE(bridge.RegisterCommand("keybinder", [&](const std::string&) { keybinderTriggered = true; }));
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_EQ(bridge.GetVersion(), SAMPBridge::Version::V03DLR1);
    ASSERT_EQ(context.rakHookInitCalls, 1);
    ASSERT_EQ(context.commandHandlers.size(), static_cast<std::size_t>(2));

    context.commandHandlers["kb"]("show");
    context.commandHandlers["keybinder"]("");
    ASSERT_TRUE(kbTriggered);
    ASSERT_TRUE(keybinderTriggered);

    bridge.Shutdown();
    ASSERT_EQ(context.rakHookDestroyCalls, 1);
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestUnsupportedVersionDetection() {
    MockContext context;
    context.version = rakhook::samp_ver::unknown;

    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(!bridge.Initialize());
    ASSERT_EQ(bridge.GetVersion(), SAMPBridge::Version::Unsupported);
    ASSERT_EQ(context.rakHookInitCalls, 0);
    ASSERT_TRUE(!context.logs.empty());

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestSendChatFunctionality() {
    MockContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(bridge.SendChat("Hello world"));
    ASSERT_EQ(context.lastSent, std::string("Hello world"));
    ASSERT_TRUE(bridge.SendCommand("kb"));
    ASSERT_EQ(context.lastSent, std::string("/kb"));

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestDeferredCommandRegistration() {
    MockContext context;
    context.inputReady = false;

    bool kbTriggered = false;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.RegisterCommand("kb", [&](const std::string&) { kbTriggered = true; }));
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_EQ(context.commandHandlers.size(), static_cast<std::size_t>(0));

    context.inputReady = true;
    bridge.Update();
    ASSERT_EQ(context.commandHandlers.size(), static_cast<std::size_t>(1));

    context.commandHandlers["kb"]("");
    ASSERT_TRUE(kbTriggered);

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestStartupMessageUsesLocalChatWhenReady() {
    MockContext context;
    context.chatReady = false;

    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());

    for (int i = 0; i < 40; ++i) {
        bridge.Update();
    }
    ASSERT_TRUE(context.lastLocalMessage.empty());

    context.chatReady = true;
    bridge.Update();
    ASSERT_TRUE(!context.lastLocalMessage.empty());
    ASSERT_TRUE(context.lastSent.empty());

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestStartupMessageIncludesConfiguredUiOptions() {
    MockContext context;
    context.chatReady = false;

    SAMPBridge::SetTestingHooks(context.BuildHooks());

    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().ui_open_command_primary = "menu";
    settingsManager.GetMutable().ui_open_command_secondary = "binder";
    settingsManager.GetMutable().ui_toggle_hotkey = 'K';

    SAMPBridge bridge;
    bridge.SetSettingsManager(&settingsManager);
    ASSERT_TRUE(bridge.Initialize());

    for (int i = 0; i < 40; ++i) {
        bridge.Update();
    }
    ASSERT_TRUE(context.lastLocalMessage.empty());

    context.chatReady = true;
    bridge.Update();
    ASSERT_TRUE(context.lastLocalMessage.find("/menu") != std::string::npos);
    ASSERT_TRUE(context.lastLocalMessage.find("/binder") != std::string::npos);
    ASSERT_TRUE(context.lastLocalMessage.find("K") != std::string::npos);
    ASSERT_TRUE(context.lastLocalMessage.find("oeffnen") == std::string::npos);

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestQueuedLocalMessageWaitsForChatReady() {
    MockContext context;
    context.chatReady = false;

    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());

    bridge.QueueLocalMessage("queued rss message");
    bridge.Update();
    ASSERT_TRUE(context.lastLocalMessage.empty());

    context.chatReady = true;
    bridge.Update();
    ASSERT_EQ(context.lastLocalMessage, std::string("queued rss message"));

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}


bool TestFallbackInterceptorIgnoresPlainChatText() {
    MockContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    int interceptorCalls = 0;
    std::string interceptedText;
    bridge.SetSlashCommandInterceptor([&](const std::string& text) {
        ++interceptorCalls;
        interceptedText = text;
        return text == "/keybinder";
    });

    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(context.input->m_pDefaultCommand != nullptr);

    context.input->m_pDefaultCommand("Keybinder gefangen");
    ASSERT_EQ(interceptorCalls, 0);
    ASSERT_TRUE(interceptedText.empty());

    context.input->m_pDefaultCommand("/keybinder");
    ASSERT_EQ(interceptorCalls, 1);
    ASSERT_EQ(interceptedText, std::string("/keybinder"));

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestJobChatAdapterLifecycleHooksAreWired() {
    MockContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(static_cast<bool>(context.incomingChatInterceptor));

    bridge.Shutdown();
    ASSERT_TRUE(!context.incomingChatInterceptor);
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestTrackedIncomeShowsLocalKeybinderMessage() {
    MockContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());
    bridge.SendLocalMessage(SAMPBridge::BuildTrackedIncomeMessageForTesting(157, 157));

    ASSERT_TRUE(!context.lastLocalMessage.empty());
    ASSERT_TRUE(context.lastLocalMessage.find("Einkommen erfasst") != std::string::npos);
    ASSERT_TRUE(context.lastLocalMessage.find("157$") != std::string::npos);
    ASSERT_TRUE(context.lastLocalMessage.find("Heute") != std::string::npos);

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestKillTrackerProcessKillEvent() {
    killtracker::KillTracker tracker;
    
    killtracker::KillEvent event;
    event.killer = "Player1";
    event.victim = "Victim1";
    event.weaponId = 24;
    event.timestamp = std::time(nullptr);
    event.isLocalPlayer = true;
    event.isSuicide = false;
    event.isSystem = false;
    
    ASSERT_TRUE(tracker.ProcessKillEvent(event));
    ASSERT_EQ(tracker.GetTotalKills(), 1);
    
    const killtracker::WeaponStats* stats = tracker.GetWeaponStats(24);
    ASSERT_TRUE(stats != nullptr);
    ASSERT_EQ(stats->killCount, 1);
    ASSERT_EQ(stats->weaponId, 24);
    
    ASSERT_TRUE(!tracker.ProcessKillEvent(event));
    ASSERT_EQ(tracker.GetTotalKills(), 1);
    
    return true;
}

bool TestKillTrackerDuplicateSuppression() {
    killtracker::KillTracker tracker;
    
    killtracker::KillEvent event1;
    event1.killer = "Player1";
    event1.victim = "Victim1";
    event1.weaponId = 24;
    event1.timestamp = std::time(nullptr);
    event1.isLocalPlayer = true;
    event1.isSuicide = false;
    
    ASSERT_TRUE(tracker.ProcessKillEvent(event1));
    
    ASSERT_TRUE(!tracker.ProcessKillEvent(event1));
    
    killtracker::KillEvent event2;
    event2.killer = "Player1";
    event2.victim = "Victim2";
    event2.weaponId = 24;
    event2.timestamp = std::time(nullptr);
    event2.isLocalPlayer = true;
    event2.isSuicide = false;
    
    ASSERT_TRUE(tracker.ProcessKillEvent(event2));
    ASSERT_EQ(tracker.GetTotalKills(), 2);
    
    return true;
}

bool TestKillTrackerSuicideFiltering() {
    killtracker::KillTracker tracker;
    
    killtracker::KillEvent event;
    event.killer = "Player1";
    event.victim = "Player1";
    event.weaponId = 54;
    event.timestamp = std::time(nullptr);
    event.isLocalPlayer = true;
    event.isSuicide = true;
    
    ASSERT_TRUE(!tracker.ProcessKillEvent(event));
    ASSERT_EQ(tracker.GetTotalKills(), 0);
    
    return true;
}

bool TestKillTrackerDamageAccumulation() {
    killtracker::KillTracker tracker;
    
    killtracker::DamageEvent event1;
    event1.targetId = 1;
    event1.damage = 25.5f;
    event1.weaponId = 24;
    event1.timestamp = std::time(nullptr);
    
    killtracker::DamageEvent event2;
    event2.targetId = 2;
    event2.damage = 30.0f;
    event2.weaponId = 24;
    event2.timestamp = std::time(nullptr);
    
    tracker.ProcessDamageEvent(event1);
    tracker.ProcessDamageEvent(event2);
    
    ASSERT_EQ(tracker.GetTotalDamage(), 55.5f);
    
    const killtracker::WeaponStats* stats = tracker.GetWeaponStats(24);
    ASSERT_TRUE(stats != nullptr);
    ASSERT_EQ(stats->totalDamage, 55.5f);
    
    return true;
}

bool TestKillTrackerEnabledToggle() {
    killtracker::KillTracker tracker;
    
    ASSERT_TRUE(tracker.IsEnabled());
    
    killtracker::KillEvent event;
    event.killer = "Player1";
    event.victim = "Victim1";
    event.weaponId = 24;
    event.timestamp = std::time(nullptr);
    event.isLocalPlayer = true;
    event.isSuicide = false;
    
    ASSERT_TRUE(tracker.ProcessKillEvent(event));
    ASSERT_EQ(tracker.GetTotalKills(), 1);
    
    tracker.SetEnabled(false);
    ASSERT_TRUE(!tracker.IsEnabled());
    
    killtracker::KillEvent event2;
    event2.killer = "Player1";
    event2.victim = "Victim2";
    event2.weaponId = 22;
    event2.timestamp = std::time(nullptr);
    event2.isLocalPlayer = true;
    event2.isSuicide = false;
    
    ASSERT_TRUE(!tracker.ProcessKillEvent(event2));
    ASSERT_EQ(tracker.GetTotalKills(), 1);
    
    return true;
}

bool TestKillTrackerStaleVictimDamageCleanup() {
    killtracker::KillTracker tracker;

    tracker.RecordDamageToVictim("VictimA", 24, 18.5f);
    tracker.RecordDamageToVictim("VictimB", 24, 42.0f);

    const std::time_t now = std::time(nullptr);
    tracker.MarkVictimDamageStaleForTesting("VictimA");

    killtracker::KillEvent victimBKill;
    victimBKill.killer = "Player1";
    victimBKill.victim = "VictimB";
    victimBKill.weaponId = 24;
    victimBKill.timestamp = now;
    victimBKill.isLocalPlayer = true;
    victimBKill.isSuicide = false;
    victimBKill.isSystem = false;

    ASSERT_TRUE(tracker.ProcessKillEvent(victimBKill));
    ASSERT_EQ(tracker.GetRecentKills().size(), static_cast<size_t>(1));
    ASSERT_EQ(tracker.GetRecentKills().back().damageDealt, 42.0f);

    killtracker::KillEvent victimAKill;
    victimAKill.killer = "Player1";
    victimAKill.victim = "VictimA";
    victimAKill.weaponId = 24;
    victimAKill.timestamp = now + 1;
    victimAKill.isLocalPlayer = true;
    victimAKill.isSuicide = false;
    victimAKill.isSystem = false;

    ASSERT_TRUE(tracker.ProcessKillEvent(victimAKill));
    ASSERT_EQ(tracker.GetRecentKills().size(), static_cast<size_t>(2));
    ASSERT_EQ(tracker.GetRecentKills().back().damageDealt, 0.0f);

    return true;
}

bool TestKillTrackerCountsLocalDeathWithoutRpc55() {
    MockContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());

    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(bridge.GetKillTracker() != nullptr);
    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 0);

    context.localPlayer->m_bIsWasted = TRUE;
    for (int i = 0; i < 20; ++i) {
        bridge.Update();
    }

    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 1);
    ASSERT_EQ(bridge.GetKillTracker()->GetRecentDeaths().size(), static_cast<size_t>(1));
    ASSERT_TRUE(bridge.GetKillTracker()->GetRecentDeaths().back().isSuicide);

    for (int i = 0; i < 5; ++i) {
        bridge.Update();
    }
    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 1);

    context.localPlayer->m_bIsWasted = FALSE;
    bridge.Update();

    context.localPlayer->m_bIsWasted = TRUE;
    for (int i = 0; i < 20; ++i) {
        bridge.Update();
    }

    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 2);

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestWeaponNamesKnownWeapons() {
    ASSERT_EQ(killtracker::GetWeaponName(0), std::string("Fist"));
    ASSERT_EQ(killtracker::GetWeaponName(24), std::string("Desert Eagle"));
    ASSERT_EQ(killtracker::GetWeaponName(31), std::string("M4"));
    ASSERT_EQ(killtracker::GetWeaponName(34), std::string("Sniper Rifle"));
    ASSERT_EQ(killtracker::GetWeaponName(46), std::string("Parachute"));
    
    return true;
}

bool TestWeaponNamesUnknownWeapon() {
    ASSERT_EQ(killtracker::GetWeaponName(-1), std::string("Unknown Weapon"));
    ASSERT_EQ(killtracker::GetWeaponName(100), std::string("Unknown Weapon"));
    ASSERT_EQ(killtracker::GetWeaponName(999), std::string("Unknown Weapon"));
    
    return true;
}

bool TestWeaponNamesValidation() {
    ASSERT_TRUE(killtracker::IsValidWeaponId(0));
    ASSERT_TRUE(killtracker::IsValidWeaponId(24));
    ASSERT_TRUE(killtracker::IsValidWeaponId(46));
    ASSERT_TRUE(killtracker::IsValidWeaponId(57));
    
    ASSERT_TRUE(!killtracker::IsValidWeaponId(-1));
    ASSERT_TRUE(!killtracker::IsValidWeaponId(58));
    ASSERT_TRUE(!killtracker::IsValidWeaponId(100));
    
    return true;
}

bool TestFormatKillMessage() {
    std::string msg1 = killtracker::FormatKillMessage("Victim1", 24);
    ASSERT_TRUE(msg1.find("You killed") != std::string::npos);
    ASSERT_TRUE(msg1.find("Victim1") != std::string::npos);
    ASSERT_TRUE(msg1.find("Desert Eagle") != std::string::npos);
    
    std::string msg2 = killtracker::FormatKillMessage("Killer1", "Victim1", 31);
    ASSERT_TRUE(msg2.find("Killer1") != std::string::npos);
    ASSERT_TRUE(msg2.find("Victim1") != std::string::npos);
    ASSERT_TRUE(msg2.find("M4") != std::string::npos);
    
    return true;
}

bool TestStatsStorageRoundTrip() {
    std::string testFile = "test_stats.json";
    
    std::remove(testFile.c_str());
    
    {
        killtracker::StatsStorage storage(testFile);
        
        std::unordered_map<int, killtracker::WeaponStats> stats;
        stats[24] = killtracker::WeaponStats(24);
        stats[24].totalDamage = 150.5f;
        stats[24].killCount = 5;
        
        stats[31] = killtracker::WeaponStats(31);
        stats[31].totalDamage = 200.0f;
        stats[31].killCount = 3;
        
        ASSERT_TRUE(storage.Save(stats));
        ASSERT_TRUE(storage.FileExists());
    }
    
    {
        killtracker::StatsStorage storage(testFile);
        
        std::unordered_map<int, killtracker::WeaponStats> loadedStats;
        ASSERT_TRUE(storage.Load(loadedStats));
        ASSERT_EQ(loadedStats.size(), static_cast<size_t>(2));
        
        auto it24 = loadedStats.find(24);
        ASSERT_TRUE(it24 != loadedStats.end());
        ASSERT_EQ(it24->second.weaponId, 24);
        ASSERT_EQ(it24->second.killCount, 5);
        ASSERT_TRUE(it24->second.totalDamage > 150.0f && it24->second.totalDamage < 151.0f);
        
        auto it31 = loadedStats.find(31);
        ASSERT_TRUE(it31 != loadedStats.end());
        ASSERT_EQ(it31->second.weaponId, 31);
        ASSERT_EQ(it31->second.killCount, 3);
    }
    
    std::remove(testFile.c_str());
    
    return true;
}

bool TestStatsStorageCorruptFile() {
    std::string testFile = "test_stats_corrupt.json";
    
    {
        std::ofstream file(testFile);
        file << "{ invalid json content }";
    }
    
    {
        killtracker::StatsStorage storage(testFile);
        
        std::unordered_map<int, killtracker::WeaponStats> loadedStats;
        storage.Load(loadedStats);
    }
    
    std::remove(testFile.c_str());
    
    return true;
}

bool TestStatsStorageEmptyFile() {
    std::string testFile = "test_stats_empty.json";
    
    std::remove(testFile.c_str());
    
    {
        killtracker::StatsStorage storage(testFile);
        
        std::unordered_map<int, killtracker::WeaponStats> loadedStats;
        ASSERT_TRUE(storage.Load(loadedStats));
        ASSERT_EQ(loadedStats.size(), static_cast<size_t>(0));
    }
    
    std::remove(testFile.c_str());
    
    return true;
}

} // namespace

int RunSAMPBridgeTests() {
    std::printf("=== SAMPBridge Tests ===\n\n");

    RUN_TEST(TestInitFailureWithoutSampModule);
    RUN_TEST(TestInitSuccessAndDirectCommandRegistration);
    RUN_TEST(TestUnsupportedVersionDetection);
    RUN_TEST(TestSendChatFunctionality);
    RUN_TEST(TestDeferredCommandRegistration);
    RUN_TEST(TestStartupMessageUsesLocalChatWhenReady);
    RUN_TEST(TestStartupMessageIncludesConfiguredUiOptions);
    RUN_TEST(TestQueuedLocalMessageWaitsForChatReady);
    RUN_TEST(TestFallbackInterceptorIgnoresPlainChatText);
    RUN_TEST(TestJobChatAdapterLifecycleHooksAreWired);
    RUN_TEST(TestTrackedIncomeShowsLocalKeybinderMessage);
    
    std::printf("\n--- Kill Tracker Tests ---\n");
    RUN_TEST(TestKillTrackerProcessKillEvent);
    RUN_TEST(TestKillTrackerDuplicateSuppression);
    RUN_TEST(TestKillTrackerSuicideFiltering);
    RUN_TEST(TestKillTrackerDamageAccumulation);
    RUN_TEST(TestKillTrackerEnabledToggle);
    RUN_TEST(TestKillTrackerStaleVictimDamageCleanup);
    RUN_TEST(TestKillTrackerCountsLocalDeathWithoutRpc55);
    
    std::printf("\n--- Weapon Names Tests ---\n");
    RUN_TEST(TestWeaponNamesKnownWeapons);
    RUN_TEST(TestWeaponNamesUnknownWeapon);
    RUN_TEST(TestWeaponNamesValidation);
    RUN_TEST(TestFormatKillMessage);
    
    std::printf("\n--- Stats Storage Tests ---\n");
    RUN_TEST(TestStatsStorageRoundTrip);
    RUN_TEST(TestStatsStorageCorruptFile);
    RUN_TEST(TestStatsStorageEmptyFile);

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_state.passed, g_state.failed);
    return g_state.failed == 0 ? 0 : 1;
}
