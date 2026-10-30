#define KEYBINDER_TESTING

#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/KillTracker.h"
#include "../include/KillTrackerTypes.h"
#include "../include/KillfeedAdapter.h"
#include "../include/WeaponNames.h"
#include "../include/StatsStorage.h"
#include "../include/MessageVariables.h"
#include "../include/RakHook/include/RakNet/BitStream.h"

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
#include <limits>

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
    std::vector<std::string> serverMessages;
    std::vector<std::string> localMessages;
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
                serverMessages.push_back(lastSent);
            },
            [this](sampapi::v03dl::CLocalPlayer*, const char* text) {
                lastSent = text != nullptr ? text : "";
                serverMessages.push_back(lastSent);
            },
            [this](sampapi::v03dl::CChat*, unsigned long, const char* text) {
                lastLocalMessage = text != nullptr ? text : "";
                localMessages.push_back(lastLocalMessage);
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

struct CombatSoundContext : MockContext {
    std::aligned_storage_t<sizeof(sampapi::v03dl::CNetGame), alignof(sampapi::v03dl::CNetGame)> netGameStorage{};
    sampapi::v03dl::CNetGame* netGame = reinterpret_cast<sampapi::v03dl::CNetGame*>(&netGameStorage);
    bool connectedObjectAvailable = true;
    bool sampleAvailable = true;
    float health = 100.0f;
    float armour = 100.0f;
    int damageSource = 24;
    const void* pedIdentity = reinterpret_cast<const void*>(0x1000);
    int samplesRead = 0;
    unsigned long tick = 1000;

    CombatSoundContext() {
        std::memset(netGame, 0, sizeof(*netGame));
        netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
        localPlayer->m_pPed = reinterpret_cast<sampapi::v03dl::CPed*>(0x2000);
        localPlayer->m_bIsActive = TRUE;
    }

    SAMPBridge::TestingHooks BuildCombatHooks() {
        auto hooks = BuildHooks();
        hooks.getNetGame = [this]() { return connectedObjectAvailable ? netGame : nullptr; };
        hooks.getTickCount = [this]() { return tick; };
        hooks.readLocalDamageSample = [this](float& outHealth, float& outArmour, int& outSource, const void*& outIdentity) {
            ++samplesRead;
            outHealth = health;
            outArmour = armour;
            outSource = damageSource;
            outIdentity = pedIdentity;
            return sampleAvailable;
        };
        return hooks;
    }
};

bool EmitDamageRpc(SAMPBridge& bridge, float amount, bool incoming = true, int weapon = 24) {
    RakNet::BitStream payload;
    payload.Write(incoming); // RPC 115: 1 = take, 0 = give
    payload.Write(static_cast<unsigned short>(7));
    payload.Write(amount);
    payload.Write(static_cast<unsigned int>(weapon));
    payload.Write(static_cast<unsigned int>(9));
    const int readOffset = payload.GetReadOffset();
    bridge.ProcessDamageRpcForTesting(&payload);
    return payload.GetReadOffset() == readOffset;
}

std::size_t DamageLogCount(const MockContext& context) {
    return static_cast<std::size_t>(std::count_if(context.localMessages.begin(), context.localMessages.end(), [](const std::string& text) {
        return text.find(" HP {FFFFFF}Schaden (") != std::string::npos;
    }));
}

bool TestDamageLogAhkBlueprintReportsDirectServerLossWithoutRpc() {
    CombatSoundContext context;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    keybinder::SettingsManager settings;
    settings.GetMutable().damage_log_enabled = true;
    settings.GetMutable().kill_tracker_count_kills = false;
    settings.GetMutable().kill_tracker_count_deaths = false;
    settings.GetMutable().hitsound_incoming_enabled = false;
    settings.GetMutable().hitsound_outgoing_enabled = false;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(false);
    bridge.SetKillTrackerTrackDamage(false);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();

    // Nova applies damage with SetPlayerHealth/Armour. No take-damage RPC is
    // needed, and the hitsound suppression flag must not mute the chat log.
    bridge.RequestScriptedHealthSuppressionForTesting();
    context.health = 75.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    ASSERT_EQ(context.localMessages.back(), std::string("{33CCFF}[NL-BINDER] {FF0000}-25,0 HP {FFFFFF}Schaden (HP Schaden)"));
    bridge.RequestScriptedHealthSuppressionForTesting();
    context.armour = 87.5f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    ASSERT_TRUE(context.localMessages.back().find("-12,5 HP {FFFFFF}Schaden (Weste getroffen)") != std::string::npos);
    context.health -= 4.5f;
    context.armour -= 2.5f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(3));
    ASSERT_TRUE(context.localMessages.back().find("-7,0 HP {FFFFFF}Schaden (Weste + HP Schaden)") != std::string::npos);
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerEnabled(false);
    context.health -= 1.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(4));
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestDamageLogSeparatesHealingFromDamageAndFormatsOneDecimal() {
    CombatSoundContext context;
    context.health = 80.0f;
    context.armour = 50.0f;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    keybinder::SettingsManager settings;
    settings.GetMutable().damage_log_enabled = true;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    // A heal must not cancel a loss in the other pool.
    context.health += 20.0f;
    context.armour -= 5.25f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    ASSERT_TRUE(context.localMessages.back().find("-5,3 HP {FFFFFF}Schaden (Weste getroffen)") != std::string::npos);
    context.health -= 2.5f;
    context.armour += 10.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    ASSERT_TRUE(context.localMessages.back().find("-2,5 HP {FFFFFF}Schaden (HP Schaden)") != std::string::npos);
    // Match the supplied AHK threshold: at least one pool must lose >= 1 HP.
    context.health -= 0.5f;
    context.armour -= 0.5f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    context.health += 1.0f;
    context.armour += 1.0f;
    bridge.Update();
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    context.armour -= 1.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(3));
    ASSERT_TRUE(context.localMessages.back().find("-1,0 HP {FFFFFF}Schaden (Weste getroffen)") != std::string::npos);
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestDamageLogDoesNotRequireOrDuplicateDamageRpcs() {
    CombatSoundContext context;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    keybinder::SettingsManager settings;
    settings.GetMutable().damage_log_enabled = true;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    ASSERT_TRUE(EmitDamageRpc(bridge, 25.0f));
    ASSERT_TRUE(EmitDamageRpc(bridge, 12.5f));
    context.health -= 37.5f;
    bridge.Update();
    // Like the AHK code, one sampled change produces exactly one line. A
    // client RPC cannot invent another hit or split the sampled amount.
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    ASSERT_TRUE(context.localMessages.back().find("-37,5 HP {FFFFFF}Schaden (HP Schaden)") != std::string::npos);
    ASSERT_TRUE(EmitDamageRpc(bridge, 50.0f));
    context.tick += 500;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    context.damageSource = 54; // source metadata is irrelevant for this log
    context.health -= 5.0f;
    bridge.RequestScriptedHealthSuppressionForTesting();
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestDamageLogToggleDiscardsOldLossAndKeepsSoundFeedbackIndependent() {
    CombatSoundContext context;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    keybinder::SettingsManager settings;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(false);
    bridge.SetKillTrackerTrackDamage(false);
    int incomingSounds = 0, outgoingSounds = 0, hitmarkers = 0;
    bridge.SetHitsoundIncomingCallback([&]() { ++incomingSounds; });
    bridge.SetHitsoundDamageCallback([&](const killtracker::DamageEvent&) { ++outgoingSounds; });
    bridge.SetHitmarkerDamageCallback([&](const killtracker::DamageEvent&) { ++hitmarkers; });
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    context.health -= 10.0f;
    bridge.Update();
    ASSERT_EQ(incomingSounds, 1);
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(0));
    context.health -= 12.5f;
    settings.GetMutable().damage_log_enabled = true;
    bridge.Update(); // starts the log at the current values, preserves sound
    ASSERT_EQ(incomingSounds, 2);
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(0));
    context.health -= 5.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    settings.GetMutable().damage_log_enabled = false;
    context.health -= 3.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    ASSERT_TRUE(EmitDamageRpc(bridge, 25.0f, false));
    ASSERT_EQ(outgoingSounds, 1);
    ASSERT_EQ(hitmarkers, 1);
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestDamageLogSeedsLoginRespawnAndPedChangeAndReportsLethalLossOnce() {
    CombatSoundContext context;
    context.armour = 0.0f;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    keybinder::SettingsManager settings;
    settings.GetMutable().damage_log_enabled = true;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    ASSERT_TRUE(bridge.Initialize());
    context.localPlayer->m_bDoesSpectating = TRUE;
    bridge.Update();
    context.health = 50.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(0));
    context.localPlayer->m_bDoesSpectating = FALSE;
    bridge.Update(); // first playing values establish a baseline
    context.health = 100.0f;
    bridge.Update(); // healing is not damage
    bridge.RequestScriptedHealthSuppressionForTesting();
    context.health = 70.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(1));
    context.health = -20.0f;
    context.localPlayer->m_bIsWasted = TRUE;
    context.localPlayer->m_bIsActive = FALSE;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    ASSERT_TRUE(context.localMessages.back().find("-70,0 HP {FFFFFF}Schaden (HP Schaden)") != std::string::npos);
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    context.localPlayer->m_bIsWasted = FALSE;
    context.localPlayer->m_bIsActive = TRUE;
    context.health = 100.0f;
    bridge.Update();
    context.pedIdentity = reinterpret_cast<const void*>(0x3000);
    context.health = 60.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(2));
    context.health = 50.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(3));
    context.sampleAvailable = false;
    bridge.Update();
    context.sampleAvailable = true;
    context.health = 20.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(3));
    context.health = 19.0f;
    bridge.Update();
    ASSERT_EQ(DamageLogCount(context), static_cast<std::size_t>(4));
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestHitsoundsIgnoreLoginPreviewAndResumeImmediatelyAfterSpawn() {
    CombatSoundContext context;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    SAMPBridge bridge;
    int incoming = 0;
    int outgoing = 0;
    bridge.SetHitsoundIncomingCallback([&]() { ++incoming; });
    bridge.SetHitsoundDamageCallback([&](const killtracker::DamageEvent&) { ++outgoing; });
    ASSERT_TRUE(bridge.Initialize());

    killtracker::DamageEvent event;
    event.isOutgoing = true;
    event.weaponId = 24;
    event.damage = 25.0f;
    const auto previewHealthChange = [&]() {
        context.health = 100.0f;
        bridge.Update();
        context.health = 40.0f;
        bridge.Update();
        bridge.ProcessDamageFeedbackForTesting(event);
        return incoming == 0 && outgoing == 0 && context.samplesRead == 0;
    };

    context.connectedObjectAvailable = false;
    ASSERT_TRUE(previewHealthChange());
    context.connectedObjectAvailable = true;
    context.netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTING;
    ASSERT_TRUE(previewHealthChange());
    context.netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    context.localPlayer->m_bIsActive = FALSE;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayer->m_bIsActive = TRUE;
    context.localPlayer->m_bDoesSpectating = TRUE;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayer->m_bDoesSpectating = FALSE;
    context.localPlayer->m_classSelection.m_bIsActive = TRUE;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayer->m_classSelection.m_bIsActive = FALSE;
    context.localPlayer->m_classSelection.m_bWaitingForSpawnRequestReply = TRUE;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayer->m_classSelection.m_bWaitingForSpawnRequestReply = FALSE;
    context.localPlayer->m_bIsWasted = TRUE;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayer->m_bIsWasted = FALSE;
    context.localPlayerReady = false;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayerReady = true;
    auto* savedPed = context.localPlayer->m_pPed;
    context.localPlayer->m_pPed = nullptr;
    ASSERT_TRUE(previewHealthChange());
    context.localPlayer->m_pPed = savedPed;

    // Spawn establishes the current HP baseline; a hit on the very next
    // frame must work without a timed mute after logging in.
    context.health = 30.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 0);
    context.health = 20.0f;
    bridge.Update();
    bridge.ProcessDamageFeedbackForTesting(event);
    ASSERT_EQ(incoming, 1);
    ASSERT_EQ(outgoing, 1);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestIncomingHitsoundReseedsAcrossReconnectRespawnAndPedReplacement() {
    CombatSoundContext context;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    SAMPBridge bridge;
    int incoming = 0;
    bridge.SetHitsoundIncomingCallback([&]() { ++incoming; });
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    context.health = 80.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 1);

    context.netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_RESTARTING;
    bridge.Update();
    context.netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    context.health = 60.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 1);
    context.health = 50.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 2);

    context.localPlayer->m_bIsWasted = TRUE;
    bridge.Update();
    context.localPlayer->m_bIsWasted = FALSE;
    context.health = 40.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 2);
    context.health = 30.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 3);

    context.pedIdentity = reinterpret_cast<const void*>(0x3000);
    context.health = 20.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 3);
    context.health = 10.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 4);

    // Invalid/unavailable native samples must not leave a stale HP baseline.
    context.health = std::numeric_limits<float>::quiet_NaN();
    bridge.Update();
    context.health = 5.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 4);
    context.sampleAvailable = false;
    bridge.Update();
    context.sampleAvailable = true;
    context.health = 2.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 4);
    context.health = 1.0f;
    bridge.Update();
    ASSERT_EQ(incoming, 5);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

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

bool TestNativeWastedWaitsForCemeteryChat() {
    CombatSoundContext context;
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerName = []() { return std::string("CemeteryPlayer"); };
    SAMPBridge::SetTestingHooks(hooks);

    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(bridge.GetKillTracker() != nullptr);
    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 0);

    context.localPlayer->m_bIsWasted = TRUE;
    for (int i = 0; i < 20; ++i) {
        bridge.Update();
    }

    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 0);
    ASSERT_TRUE(bridge.GetKillTracker()->GetRecentDeaths().empty());
    ASSERT_TRUE(context.localMessages.empty());
    bridge.ProcessIncomingChatLineForTesting(nullptr, "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abl\xC3\xA4uft.");
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 1);

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
    ASSERT_EQ(bridge.GetTotalDeaths(), 1);
    context.tick += 6000;
    bridge.ProcessIncomingChatLineForTesting(nullptr, "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abl\xC3\xA4uft.");
    bridge.Update();
    ASSERT_EQ(bridge.GetKillTracker()->GetTotalDeaths(), 2);

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestKillAndDeathCountersCanBeDisabledIndependently() {
    killtracker::KillTracker tracker;
    killtracker::KillEvent kill;
    kill.killer = "LocalPlayer";
    kill.victim = "Victim";
    kill.weaponId = 24;
    kill.timestamp = std::time(nullptr);
    kill.isLocalPlayer = true;

    tracker.SetCountKills(false);
    tracker.SetCountDeaths(true);
    ASSERT_TRUE(!tracker.ProcessKillEvent(kill));
    tracker.RecordDeath("Enemy", 31);
    ASSERT_EQ(tracker.GetTotalKills(), 0);
    ASSERT_EQ(tracker.GetSessionKills(), 0);
    ASSERT_TRUE(tracker.GetRecentKills().empty());
    ASSERT_EQ(tracker.GetTotalDeaths(), 1);
    ASSERT_EQ(tracker.GetSessionDeaths(), 1);

    tracker.SetCountKills(true);
    tracker.SetCountDeaths(false);
    ASSERT_TRUE(tracker.ProcessKillEvent(kill));
    tracker.RecordDeath("Enemy", 31);
    ASSERT_EQ(tracker.GetTotalKills(), 1);
    ASSERT_EQ(tracker.GetSessionKills(), 1);
    ASSERT_EQ(tracker.GetTotalDeaths(), 1);
    ASSERT_EQ(tracker.GetSessionDeaths(), 1);
    ASSERT_EQ(tracker.GetRecentDeaths().size(), static_cast<std::size_t>(1));

    tracker.SetEnabled(false);
    tracker.SetCountDeaths(true);
    kill.victim = "AnotherVictim";
    ASSERT_TRUE(!tracker.ProcessKillEvent(kill));
    tracker.RecordDeath("Enemy", 31);
    ASSERT_EQ(tracker.GetTotalKills(), 1);
    ASSERT_EQ(tracker.GetTotalDeaths(), 1);
    return true;
}

bool TestCounterSettingsAppliedBeforeTrackerCreation() {
    keybinder::SettingsManager settings;
    settings.GetMutable().kill_tracker_count_kills = false;
    settings.GetMutable().kill_tracker_count_deaths = false;

    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(true);
    killtracker::KillEvent kill;
    kill.killer = "LocalPlayer";
    kill.victim = "Victim";
    kill.weaponId = 24;
    kill.timestamp = std::time(nullptr);
    kill.isLocalPlayer = true;

    ASSERT_TRUE(!bridge.GetKillTracker()->ProcessKillEvent(kill));
    bridge.GetKillTracker()->RecordDeath("Enemy", 31);
    ASSERT_EQ(bridge.GetTotalKills(), 0);
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    return true;
}

bool TestDisabledDeathCounterSendsNoNotifications() {
    MockContext context;
    SAMPBridge::SetTestingHooks(context.BuildHooks());
    keybinder::SettingsManager settings;
    settings.GetMutable().kill_tracker_count_deaths = false;

    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerLocalDeathMessageEnabled(true);
    bridge.SetKillTrackerServerDeathMessageEnabled(true);
    bridge.SetKillTrackerLocalDeathMessageTemplate("Death {killer}: {total_deaths}");
    bridge.SetKillTrackerServerDeathMessageTemplate("/f Deaths: {total_deaths}");
    ASSERT_TRUE(bridge.Initialize());

    bridge.RecordLocalPlayerDeath("Enemy", 31);
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    ASSERT_TRUE(bridge.GetKillTracker()->GetRecentDeaths().empty());
    ASSERT_TRUE(context.lastLocalMessage.empty());
    ASSERT_TRUE(context.lastSent.empty());

    settings.GetMutable().kill_tracker_count_deaths = true;
    bridge.SetSettingsManager(&settings);
    bridge.RecordLocalPlayerDeath("Enemy", 31);
    ASSERT_EQ(bridge.GetTotalDeaths(), 1);
    ASSERT_EQ(context.lastLocalMessage, std::string("{33CCFF}[NL-BINDER]{FFFFFF} Du bist gestorben. Deine aktuelle Tode: 1"));
    ASSERT_TRUE(context.lastSent.empty());

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestCombatMessageVariablesAndSlashCommandRouting() {
    MockContext context;
    auto hooks = context.BuildHooks();
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("KOBiRO"); };
    hooks.getPlayerName = [](int) { return std::string("Enemy"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    bridge.SetKillTrackerLocalKillMessageTemplate("{zone}|{ZONE}|{kd}|{KD}|{total_kills}|{total_deaths}|{session_kills}");
    bridge.SetKillTrackerServerKillMessageTemplate("{FF0000}/f {killer} {weapon}: {total_kills}/{total_deaths} {kd} {KD}");
    ASSERT_TRUE(bridge.Initialize());
    bridge.ImportKillCount(7);
    bridge.ImportDeathCount(4);

    bridge.ProcessDeathMessageForTesting({5, 7, 31, std::time(nullptr), true});
    ASSERT_EQ(context.lastLocalMessage, std::string("Unknown|UNKNOWN|2.00|2.00|8|4|1"));
    ASSERT_EQ(context.lastSent, std::string("/f KOBiRO M4: 8/4 2.00 2.00"));

    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestVitalsUseCurrentPedInsteadOfOldVehicleAndOnFootSync() {
    CombatSoundContext context;
    SAMPBridge::SetTestingHooks(context.BuildCombatHooks());
    SAMPBridge bridge;
    context.localPlayer->m_incarData.m_nDriverHealth = 99;
    context.localPlayer->m_incarData.m_nDriverArmor = 88;
    context.localPlayer->m_onfootData.m_nHealth = 55;
    context.localPlayer->m_onfootData.m_nArmor = 44;
    context.health = 75.0f;
    context.armour = 25.0f;
    ASSERT_EQ(bridge.GetPlayerHP(), 75.0f);
    ASSERT_EQ(bridge.GetPlayerArmour(), 25.0f);
    context.health = 40.0f;
    context.armour = 0.0f;
    ASSERT_EQ(bridge.GetPlayerHP(), 40.0f);
    ASSERT_EQ(bridge.GetPlayerArmour(), 0.0f);
    context.sampleAvailable = false;
    ASSERT_EQ(bridge.GetPlayerHP(), 0.0f);
    ASSERT_EQ(bridge.GetPlayerArmour(), 0.0f);
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestEveryOfferedCombatVariableResolvesWithoutServerQueries() {
    CombatSoundContext context;
    context.health = 75.0f;
    context.armour = 26.0f;
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("KOBiRO"); };
    hooks.getPlayerName = [](int) { return std::string("Enemy"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(false);
    std::string message;
    for (const auto& variable : keybinder::kCombatMessageVariables) {
        const std::string tag = variable.tag;
        ASSERT_TRUE(tag != "carhp" && tag != "carname");
        if (!message.empty()) message += "|";
        message += "{" + tag + "}";
    }
    bool vehicleSupportedInBindings = false;
    for (const auto& variable : keybinder::kTextBindingVariables)
        if (std::string(variable.tag) == "carname") vehicleSupportedInBindings = true;
    ASSERT_TRUE(vehicleSupportedInBindings);
    bridge.SetKillTrackerLocalKillMessageTemplate(message);
    ASSERT_TRUE(bridge.Initialize());
    bridge.ImportKillCount(7);
    bridge.ImportDeathCount(4);
    bridge.ProcessDeathMessageForTesting({5, 7, 31, std::time(nullptr), true});
    ASSERT_EQ(context.lastLocalMessage, std::string("Unknown|Unknown|75|26|8|4|1|0|2.00"));
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.SetKillTrackerLocalKillMessageTemplate("{victim}|{killer}|{weapon}");
    bridge.ProcessDeathMessageForTesting({5, 9, -1, std::time(nullptr), true});
    ASSERT_EQ(context.lastLocalMessage, std::string("Enemy|KOBiRO|Unbekannt"));
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestCounterSettingsAndSlashTemplatesRoundTrip() {
    keybinder::SettingsManager legacySettings;
    ASSERT_TRUE(legacySettings.LoadFromJson("{\"kill_tracker_enabled\":true}"));
    ASSERT_TRUE(legacySettings.Get().kill_tracker_count_kills);
    ASSERT_TRUE(legacySettings.Get().kill_tracker_count_deaths);

    for (bool countKills : {false, true}) {
        for (bool countDeaths : {false, true}) {
            keybinder::SettingsManager settings;
            settings.GetMutable().kill_tracker_enabled = countKills || countDeaths;
            settings.GetMutable().kill_tracker_count_kills = countKills;
            settings.GetMutable().kill_tracker_count_deaths = countDeaths;
            settings.GetMutable().kill_tracker_server_kill_message_enabled = true;
            settings.GetMutable().kill_tracker_server_kill_message_template = "/f Kill in {zone}. Kills: {total_kills} \"confirmed\" C:\\logs";
            settings.GetMutable().kill_tracker_server_death_message_template = "/f Deaths: {total_deaths}";

            keybinder::SettingsManager restored;
            ASSERT_TRUE(restored.LoadFromJson(settings.GetSettingsJson()));
            ASSERT_EQ(restored.Get().kill_tracker_enabled, countKills || countDeaths);
            ASSERT_EQ(restored.Get().kill_tracker_count_kills, countKills);
            ASSERT_EQ(restored.Get().kill_tracker_count_deaths, countDeaths);
            ASSERT_TRUE(restored.Get().kill_tracker_server_kill_message_enabled);
            ASSERT_EQ(restored.Get().kill_tracker_server_kill_message_template,
                settings.Get().kill_tracker_server_kill_message_template);
            ASSERT_EQ(restored.Get().kill_tracker_server_death_message_template,
                settings.Get().kill_tracker_server_death_message_template);
        }
    }
    return true;
}

bool TestKillBinderWorksWithoutDamageTracking() {
    MockContext context;
    auto hooks = context.BuildHooks();
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("KOBiRO"); };
    hooks.getPlayerName = [](int id) { return id == 7 ? std::string("Victim") : std::string("Other"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerTrackDamage(false);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    bridge.SetKillTrackerServerKillMessageTemplate("/f {killer} hat {victim} getoetet: {total_kills}");
    ASSERT_TRUE(bridge.Initialize());
    const killtracker::DeathMessageEvent death{5, 7, 24, std::time(nullptr), true};
    bridge.ProcessDeathMessageForTesting(death);
    ASSERT_EQ(bridge.GetTotalKills(), 1);
    ASSERT_EQ(context.lastSent, std::string("/f KOBiRO hat Victim getoetet: 1"));
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    bridge.ProcessDeathMessageForTesting(death);
    ASSERT_EQ(bridge.GetTotalKills(), 1);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    bridge.SetKillTrackerServerKillMessageTemplate("");
    bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalKills(), 2);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestReceivedServerTotalsAndKillfeedCountExactlyOnce() {
    MockContext context;
    auto hooks = context.BuildHooks();
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("KOBiRO"); };
    hooks.getPlayerName = [](int id) { return id == 7 ? std::string("Victim") : std::string("Other"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    bridge.SetKillTrackerLocalDeathMessageEnabled(false);
    bridge.SetKillTrackerServerDeathMessageEnabled(true);
    bridge.SetKillTrackerServerKillMessageTemplate("/f Kills: {total_kills}");
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(!!context.incomingChatInterceptor);
    auto receiveStats = [&](int kills, int deaths) {
        context.incomingChatInterceptor(context.chat, nullptr, 0, "*** KOBiRO ***");
        const std::string line = "{FFFFFF}Kills: [" + std::to_string(kills) + "] Tode: [" +
            std::to_string(deaths) + "] Fraktion: [Red County Brotherhood] Gang: [-]";
        context.incomingChatInterceptor(context.chat, nullptr, 0, line.c_str());
    };
    receiveStats(63, 205);
    ASSERT_EQ(bridge.GetTotalKills(), 63);
    ASSERT_EQ(bridge.GetTotalDeaths(), 205);
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_TRUE(context.localMessages.empty());
    receiveStats(64, 205);
    ASSERT_EQ(bridge.GetTotalKills(), 64);
    ASSERT_EQ(bridge.GetKillTracker()->GetSessionKills(), 1);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_EQ(context.lastSent, std::string("/f Kills: 64"));
    receiveStats(64, 205);
    bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalKills(), 64);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_EQ(bridge.GetKillTracker()->GetRecentKills().back().victim, std::string("Victim"));
    ASSERT_EQ(bridge.GetKillTracker()->GetWeaponStats(24)->killCount, 1);
    bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalKills(), 65);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(2));
    receiveStats(64, 205); // delayed old statistics must not undo the new RPC kill
    ASSERT_EQ(bridge.GetTotalKills(), 65);
    receiveStats(65, 205);
    ASSERT_EQ(bridge.GetTotalKills(), 65);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(2));
    receiveStats(65, 206);
    ASSERT_EQ(bridge.GetTotalDeaths(), 206);
    ASSERT_EQ(bridge.GetKillTracker()->GetSessionDeaths(), 0);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(2));
    bridge.Update();
    bridge.ProcessDeathMessageForTesting({9, 5, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalDeaths(), 206);
    bridge.NotifyIncomingChat("KOBiRO sagt: Kills: [999] Tode: [999] Fraktion: [LSPD]");
    ASSERT_EQ(bridge.GetTotalKills(), 65);
    ASSERT_EQ(bridge.GetTotalDeaths(), 206);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestServerStatsRequireOwnPlayerHeaderAndConsumeContext() {
    MockContext context;
    unsigned long now = 1000;
    auto hooks = context.BuildHooks();
    hooks.getTickCount = [&]() { return now; };
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("KOBiRO"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    bridge.SetKillTrackerServerKillMessageTemplate("Kills: {total_kills}");
    ASSERT_TRUE(bridge.Initialize());

    const std::string ownTotals = "Kills: [63] Tode: [205] Fraktion: [Red County Brotherhood] Gang: [-]";
    const std::string increasedTotals = "Kills: [64] Tode: [206] Fraktion: [Red County Brotherhood] Gang: [-]";
    bridge.NotifyIncomingChat(ownTotals); // a bare totals line cannot establish identity
    ASSERT_EQ(bridge.GetTotalKills(), 0);
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    bridge.NotifyIncomingChat("*** Victim ***");
    bridge.NotifyIncomingChat(ownTotals);
    ASSERT_EQ(bridge.GetTotalKills(), 0);
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);

    bridge.NotifyIncomingChat("*** KOBiRO ***");
    bridge.NotifyIncomingChat("Level:[17] Erfahrungspunkte [603/28176] Geschlecht: [Männlich]");
    bridge.NotifyIncomingChat(ownTotals);
    ASSERT_EQ(bridge.GetTotalKills(), 63);
    ASSERT_EQ(bridge.GetTotalDeaths(), 205);
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_TRUE(context.localMessages.empty());

    bridge.NotifyIncomingChat(increasedTotals); // the preceding header was already consumed
    ASSERT_EQ(bridge.GetTotalKills(), 63);
    ASSERT_EQ(bridge.GetTotalDeaths(), 205);
    bridge.NotifyIncomingChat("*** KOBiRO ***");
    bridge.NotifyIncomingChat("*** Other ***"); // a foreign header invalidates the local context
    bridge.NotifyIncomingChat(increasedTotals);
    ASSERT_EQ(bridge.GetTotalKills(), 63);
    ASSERT_EQ(bridge.GetTotalDeaths(), 205);
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_TRUE(context.localMessages.empty());

    bridge.NotifyIncomingChat("*** KOBiRO ***");
    now += 3001;
    bridge.NotifyIncomingChat(increasedTotals); // stale identity cannot authorize a later line
    ASSERT_EQ(bridge.GetTotalKills(), 63);
    ASSERT_EQ(bridge.GetTotalDeaths(), 205);
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_TRUE(context.localMessages.empty());

    bridge.NotifyIncomingChat("*** KOBiRO ***");
    bridge.NotifyIncomingChat(increasedTotals);
    ASSERT_EQ(bridge.GetTotalKills(), 64);
    ASSERT_EQ(bridge.GetTotalDeaths(), 206);
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_EQ(context.localMessages.size(), std::size_t(1)); // Only the local kill confirmation; stats do not notify a death.
    context.localPlayer->m_bIsWasted = true;
    bridge.Update();
    context.localPlayer->m_bIsWasted = false;
    bridge.Update();
    bridge.ProcessDeathMessageForTesting({9, 5, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalDeaths(), 206); // A killfeed victim is not a cemetery admission.
    ASSERT_EQ(context.localMessages.size(), std::size_t(1));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestKillfeedStartsWithBaselineAndDoesNotReplayAfterReset() {
    killtracker::KillfeedAdapter adapter;
    killtracker::DeathWindowEntry historical;
    historical.killer = "KOBiRO";
    historical.victim = "Historical";
    historical.weaponId = 24;
    historical.valid = true;
    killtracker::DeathWindowEntry current = historical;
    current.victim = "Current";
    killtracker::DeathWindowEntry newest = historical;
    newest.victim = "Newest";

    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({historical}).empty());
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({historical}).empty());
    const auto newKills = adapter.ProcessSnapshotForTesting({historical, current});
    ASSERT_EQ(newKills.size(), std::size_t(1));
    ASSERT_EQ(newKills.front().victim, std::string("Current"));
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({historical, current}).empty());

    adapter.ClearState();
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({historical, current}).empty());
    const auto afterReset = adapter.ProcessSnapshotForTesting({historical, current, newest});
    ASSERT_EQ(afterReset.size(), std::size_t(1));
    ASSERT_EQ(afterReset.front().victim, std::string("Newest"));

    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({}, false).empty()); // native window disabled/unavailable
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({historical, current, newest}).empty()); // no history replay on return
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({}, false).empty());
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({current}).empty()); // unknown empty is not a readable empty baseline

    adapter.ClearState();
    ASSERT_TRUE(adapter.ProcessSnapshotForTesting({}).empty());
    ASSERT_EQ(adapter.ProcessSnapshotForTesting({current}).size(), std::size_t(1));
    return true;
}

bool TestDeathRpcAndWastedCannotCountWithoutCemetery() {
    CombatSoundContext context;
    auto deathNoticeCount = [&]() {
        std::size_t count = 0;
        for (const std::string& message : context.localMessages) {
            if (message.find("Du bist gestorben.") != std::string::npos) ++count;
        }
        return count;
    };
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("KOBiRO"); };
    hooks.getPlayerName = [](int) { return std::string("Other"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    ASSERT_TRUE(bridge.Initialize());

    bridge.ProcessDeathMessageForTesting({9, 5, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    ASSERT_EQ(deathNoticeCount(), std::size_t(0));
    bridge.Update(); // RPC arrives one frame before local player state changes
    context.localPlayer->m_bIsWasted = true;
    for (int i = 0; i < 20; ++i) bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    ASSERT_EQ(deathNoticeCount(), std::size_t(0));
    bridge.ProcessDeathMessageForTesting({9, 5, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalDeaths(), 0); // Duplicate RPCs cannot count either.

    context.localPlayer->m_bIsWasted = false;
    bridge.Update(); // observed respawn, including a second death inside five seconds
    bridge.ProcessDeathMessageForTesting({9, 5, 31, std::time(nullptr), true});
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    ASSERT_EQ(deathNoticeCount(), std::size_t(0));
    context.localPlayer->m_bIsWasted = true;
    for (int i = 0; i < 20; ++i) bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 0);
    ASSERT_EQ(bridge.GetKillTracker()->GetSessionDeaths(), 0);
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestAutoEngineStartUsesDriverTransitionsAndHeldInput() {
    MockContext context;
    unsigned long now = 1000;
    unsigned short vehicle = 10;
    bool driver = false;
    bool engineOn = false;
    bool inputAvailable = true;
    std::vector<bool> keyStates;
    auto hooks = context.BuildHooks();
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = vehicle; running = engineOn; return driver; };
    hooks.isEngineInputAvailable = [&]() { return inputAvailable; };
    hooks.setEngineStartKeyDown = [&](bool down) { keyStates.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    ASSERT_TRUE(keyStates.empty());
    driver = true; // no outgoing enter-vehicle RPC is needed
    bridge.Update();
    now = 1099;
    bridge.Update();
    ASSERT_TRUE(keyStates.empty());
    now = 1100;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(1));
    ASSERT_TRUE(keyStates[0]);
    now = 1200;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(1));
    now = 1250;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(2));
    ASSERT_TRUE(!keyStates[1]);
    engineOn = true;
    now = 6000;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(2));
    vehicle = 11;
    bridge.Update();
    now = 8000;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(2)); // running engine must not be toggled off
    engineOn = false;
    vehicle = 12;
    inputAvailable = false;
    bridge.Update();
    now = 10000;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(2)); // chat/cursor/focus gating
    inputAvailable = true;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(3));
    bridge.SetAutoEngineStart(false);
    ASSERT_EQ(keyStates.size(), std::size_t(4));
    ASSERT_TRUE(!keyStates.back());
    now = 14000;
    bridge.Update();
    ASSERT_EQ(keyStates.size(), std::size_t(4));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestAutoEngineStartDoesNotTypeIntoBinder() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool binderOpen = true;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [](unsigned short& id, bool& engineOn) { id = 10; engineOn = false; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetEngineInputBlockedCallback([&]() { return binderOpen; });
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 3000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    binderOpen = false;
    now = 3100;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1));
    ASSERT_TRUE(keys.back());
    binderOpen = true;
    now = 3120;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(!keys.back());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestAutoEngineStartWaitsForServerEngineStateAfterEntry() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool engineOn = true; // transient GTA state during entry
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = 20; running = engineOn; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1200;
    bridge.Update();
    engineOn = false; // server applies engine-off after seating
    now = 1600;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1)); // no extra 1.5-second wait after engine-off
    ASSERT_TRUE(keys.back());
    bridge.SetAutoEngineStart(true); // repeated settings application is harmless
    now = 2500;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(!keys.back());
    now = 2650;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(!keys.back());
    engineOn = true;
    now = 5000;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestAutoEngineStartPressesOnlyOnceUntilActualExit() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool driver = true;
    bool engineOn = false;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = 21; running = engineOn; return driver; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 2500;
    bridge.Update();
    now = 2650;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(keys.front() && !keys.back());
    // The server may leave bEngineOn false throughout the start animation.
    // Its toggle command must never be repeated, even after the old retry times.
    for (const auto tick : {4300UL, 6100UL, 10000UL, 20000UL}) {
        now = tick;
        bridge.Update();
    }
    ASSERT_EQ(keys.size(), std::size_t(2));
    engineOn = true;
    bridge.Update();
    engineOn = false; // a later manual stop is not another entry
    bridge.Update();
    bridge.SetAutoEngineStart(true);
    bridge.SetAutoEngineStart(false);
    bridge.Update();
    bridge.SetAutoEngineStart(true);
    now = 24000;
    bridge.Update();
    now = 26000;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    driver = false;
    bridge.Update();
    driver = true; // exit and re-enter the very same vehicle
    now = 27000;
    bridge.Update();
    now = 28500;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(3));
    ASSERT_TRUE(keys.back());
    now = 28650;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(4));
    ASSERT_TRUE(!keys.back());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestAutoEngineStartWaitsUntilChatCanReleaseInput() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool inputAvailable = false;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [](unsigned short& id, bool& running) { id = 22; running = false; return true; };
    hooks.isEngineInputAvailable = [&]() { return inputAvailable; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 25000; // a long-open chat/binder must not discard the one pending press
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    inputAvailable = true;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1));
    ASSERT_TRUE(keys.back());
    now = 25150;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(!keys.back());
    now = 26800;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2)); // no retry after releasing the one press
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestAutoEngineStartDoesNotRestartManuallyStoppedEngine() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool engineOn = true;
    bool driver = true;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = 23; running = engineOn; return driver; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 2500;
    bridge.Update(); // already running: consume this entry without a key press
    ASSERT_TRUE(keys.empty());
    engineOn = false;
    now = 10000;
    bridge.Update();
    bridge.SetAutoEngineStart(false);
    bridge.SetAutoEngineStart(true);
    now = 20000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    driver = false;
    bridge.Update();
    driver = true;
    now = 21000;
    bridge.Update();
    now = 22500;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1));
    ASSERT_TRUE(keys.back());
    bridge.Shutdown();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(!keys.back());
    SAMPBridge::ResetTestingHooks();
    return true;
}

const char* kServerAutoEngineNotice = "INFO: Automatischer Motorstart: Der Motor deines Fahrzeugs wurde automatisch gestartet.";

bool TestServerAutoEngineNoticeConsumesCurrentEntryBeforeLaggingEngineFlag() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [](unsigned short& id, bool& running) { id = 30; running = false; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1100;
    ASSERT_TRUE(bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice));
    bridge.Update();
    for (const auto tick : {2500UL, 5000UL, 20000UL}) { now = tick; bridge.Update(); }
    ASSERT_TRUE(keys.empty());
    bridge.SetAutoEngineStart(false);
    bridge.SetAutoEngineStart(true);
    now = 30000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestServerAutoEngineNoticeImmediatelyBeforeFirstDriverFrameAndNextEntry() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool driver = false;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = 31; running = false; return driver; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1100;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
    bridge.Update(); // native seat is not ready yet
    driver = true;
    now = 1200;
    bridge.Update();
    now = 3000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    driver = false;
    now = 3100;
    bridge.Update();
    driver = true;
    now = 3200;
    bridge.Update();
    now = 4700;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1)); // previous server notice cannot consume a new entry
    ASSERT_TRUE(keys.back());
    bridge.Shutdown();
    ASSERT_EQ(keys.size(), std::size_t(2));
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestServerAutoEngineNoticeRejectsPlayerPrefixAndReleasesOwnHeldKey() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [](unsigned short& id, bool& running) { id = 32; running = false; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1100;
    bridge.ProcessIncomingChatLineForTesting("INFO", "Automatischer Motorstart: Der Motor deines Fahrzeugs wurde automatisch gestartet.");
    bridge.ProcessIncomingChatLineForTesting("Other_Player", kServerAutoEngineNotice);
    bridge.ProcessIncomingChatLineForTesting(nullptr, "INFO: Automatischer Motorstart: anderer Text");
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1));
    ASSERT_TRUE(keys.back());
    now = 1120;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    ASSERT_TRUE(!keys.back());
    now = 5000;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestFastAutoEngineAddonWinsAtTheLastGuardFrame() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [](unsigned short& id, bool& running) { id = 61; running = false; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1099;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    // The native engine flag can still be false after the server's addon
    // already started it. Its notice must beat the fast-path motor toggle.
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
    now = 1100;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    now = 4000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestFastAutoEngineDoesNotToggleAnAddonRunningBeforeItsNotice() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    bool engineOn = false;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = 62; running = engineOn; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    engineOn = true;
    now = 1100;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    now = 1150;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
    bridge.Update();
    engineOn = false; // later manual stop is deliberately left alone
    now = 1200;
    bridge.Update();
    now = 5000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestServerAutoEngineNoticeIsBoundToVehicleGenerationAndFreshness() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    unsigned short vehicle = 40;
    bool driver = true;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [&](unsigned short& id, bool& running) { id = vehicle; running = false; return driver; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1100;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice); // tags old vehicle 40
    vehicle = 41;
    bridge.Update();
    now = 2600;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(1));
    now = 2750;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(2));
    driver = false;
    now = 2800;
    bridge.Update();
    now = 2810;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
    bridge.Update();
    driver = true;
    vehicle = 42;
    now = 3900;
    bridge.Update(); // stale pre-seat message must not suppress this entry
    now = 5400;
    bridge.Update();
    ASSERT_EQ(keys.size(), std::size_t(3));
    bridge.Shutdown();
    ASSERT_EQ(keys.size(), std::size_t(4));
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestServerAutoEngineNoticeWhileOptionDisabledRemainsConsumedOnEnable() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long now = 1000;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() { return now; };
    hooks.getDriverVehicle = [](unsigned short& id, bool& running) { id = 33; running = false; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    now = 1100;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
    bridge.Update();
    bridge.SetAutoEngineStart(true);
    now = 3000;
    bridge.Update();
    now = 5000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestServerAutoEngineNoticeNewerThanCachedFrameClockIsNotDiscarded() {
    MockContext context;
    auto hooks = context.BuildHooks();
    unsigned long clock = 1000;
    bool injectDuringClockRead = false;
    SAMPBridge* bridgePointer = nullptr;
    std::vector<bool> keys;
    hooks.getTickCount = [&]() {
        if (injectDuringClockRead) {
            injectDuringClockRead = false;
            clock = 1101;
            bridgePointer->ProcessIncomingChatLineForTesting(nullptr, kServerAutoEngineNotice);
            return 1100UL; // frame snapshot precedes the just-arrived chat
        }
        return clock;
    };
    hooks.getDriverVehicle = [](unsigned short& id, bool& running) { id = 34; running = false; return true; };
    hooks.isEngineInputAvailable = []() { return true; };
    hooks.setEngineStartKeyDown = [&](bool down) { keys.push_back(down); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridgePointer = &bridge;
    bridge.SetAutoEngineStart(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    injectDuringClockRead = true;
    bridge.Update();
    clock = 3000;
    bridge.Update();
    ASSERT_TRUE(keys.empty());
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

std::size_t LocalDeathNoticeCount(const MockContext& context) {
    return std::count_if(context.localMessages.begin(), context.localMessages.end(), [](const std::string& value) {
        return value.find("Du bist gestorben.") != std::string::npos;
    });
}

constexpr const char* kCemeteryDeathNotice = "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abl\xC3\xA4uft.";

bool TestExplosionOverkillAndDelayedCemeteryCountOneDeathPerLife() {
    CombatSoundContext context;
    context.health = 50.0f; context.armour = 0.0f;
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerName = []() { return std::string("ExplosionPlayer"); };
    hooks.getLocalPlayerId = []() { return 5; };
    SAMPBridge::SetTestingHooks(hooks);
    keybinder::SettingsManager settings;
    settings.GetMutable().damage_log_enabled = true;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.ImportDeathCount(212);
    bridge.Update();
    context.damageSource = 51; // Explosion, with a large GTA negative overkill value.
    context.health = -8000000.0f;
    context.localPlayer->m_bIsWasted = TRUE;
    context.localPlayer->m_bIsActive = FALSE;
    for (int i = 0; i < 20; ++i) bridge.Update();
    ASSERT_EQ(DamageLogCount(context), std::size_t(1));
    ASSERT_TRUE(context.localMessages.front().find("{33CCFF}[NL-BINDER] {FF0000}-50,0 HP") != std::string::npos);
    ASSERT_EQ(bridge.GetTotalDeaths(), 212);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(0));
    // Clearing the native wasted flag does not resurrect a zero/negative-HP ped.
    context.localPlayer->m_bIsWasted = FALSE;
    context.localPlayer->m_bIsActive = TRUE;
    context.tick += 10000;
    bridge.Update();
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 213);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(1));
    context.health = 8000050.0f; // Script/protection state is not a new life.
    for (int i = 0; i < 3; ++i) bridge.Update();
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 213);
    ASSERT_EQ(DamageLogCount(context), std::size_t(1));
    context.health = 100.0f;
    bridge.Update(); bridge.Update(); // Actual controllable, healthy respawn.
    context.health = -8000000.0f;
    for (int i = 0; i < 20; ++i) bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 213);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(1));
    context.tick += 6000;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 214);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(2));
    ASSERT_EQ(DamageLogCount(context), std::size_t(2));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestCemeteryIsTheOnlyDeathCountAndDuplicateNoticeIsIgnored() {
    CombatSoundContext context;
    context.health = 50.0f;
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerName = []() { return std::string("CemeteryIgnoredPlayer"); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.ImportDeathCount(213);
    bridge.Update();

    context.health = 0.0f;
    context.localPlayer->m_bIsWasted = FALSE;
    for (int i = 0; i < 20; ++i) bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 213); // A native 0-HP sample never counts.
    ASSERT_EQ(bridge.GetKillTracker()->GetSessionDeaths(), 0);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(0));

    context.tick += 1000;
    context.health = 100.0f;
    context.localPlayer->m_bIsWasted = FALSE;
    bridge.Update(); bridge.Update(); // A genuinely healthy new life rearms detection.
    context.tick += 2000;
    const std::string timestamped = std::string("{33CCFF}[23:57:06] {FFFFFF}") + kCemeteryDeathNotice;
    bridge.ProcessIncomingChatLineForTesting(nullptr, timestamped.c_str());
    bridge.ProcessIncomingChatLineForTesting(nullptr, "INFO: {FFFFFF}Gib /graveyard ein, um herauszufinden, wie lange du noch hier bist.");
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 214);
    ASSERT_EQ(bridge.GetKillTracker()->GetSessionDeaths(), 1);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(1));
    ASSERT_TRUE(context.serverMessages.empty());

    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 214);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(1));
    context.health = 0.0f;
    context.localPlayer->m_bIsWasted = FALSE;
    for (int i = 0; i < 20; ++i) bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 214); // The next native death also waits for the server.
    ASSERT_EQ(bridge.GetKillTracker()->GetSessionDeaths(), 1);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(1));
    context.tick += 6000;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 215);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(2));
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestLaterCemeteryCountsEvenWithPermanentlyHealthyControlledNativePed() {
    CombatSoundContext context;
    context.health = 100.0f;
    bool controllable = true;
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerName = []() { return std::string("ChatDeathPlayer"); };
    hooks.isLocalPlayerControllable = [&]() { return controllable; };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    ASSERT_TRUE(bridge.Initialize());
    bridge.Update();
    const char* criminalDeathNotice = "<< Enemy_One hat den gesuchten Verbrecher ChatDeathPlayer get\xC3\xB6tet. >>";
    bridge.ProcessIncomingChatLineForTesting(nullptr, criminalDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 0); // An explicit victim announcement is not the admission trigger.
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 1);
    for (int i = 0; i < 3; ++i) bridge.Update();
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 1); // A duplicate callback has no second count.
    controllable = false;
    for (int i = 0; i < 4; ++i) bridge.Update();
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 1); // Healthy but frozen funeral ped.
    controllable = true;
    bridge.Update(); bridge.Update();
    context.tick += 6000;
    bridge.ProcessIncomingChatLineForTesting(nullptr, kCemeteryDeathNotice);
    bridge.Update();
    ASSERT_EQ(bridge.GetTotalDeaths(), 2);
    ASSERT_EQ(LocalDeathNoticeCount(context), std::size_t(2));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestDamageLogRejectsScriptSentinelsWithoutCappingEnhancedVitals() {
    keybinder::IncomingDamageLog log;
    ASSERT_TRUE(!log.Observe(8000050.0f, 0.0f).HasDamage());
    ASSERT_TRUE(!log.Observe(0.0f, 0.0f).HasDamage());
    ASSERT_TRUE(!log.Observe(5000.0f, 500.0f).HasDamage());
    const auto normal = log.Observe(4900.0f, 450.0f);
    ASSERT_EQ(normal.healthDamage, 100.0f);
    ASSERT_EQ(normal.armourDamage, 50.0f);
    const auto explosion = log.Observe(-8000000.0f, -8000000.0f);
    ASSERT_EQ(explosion.healthDamage, 4900.0f);
    ASSERT_EQ(explosion.armourDamage, 450.0f);
    ASSERT_TRUE(!log.Observe(8000050.0f, 8000050.0f).HasDamage());
    ASSERT_TRUE(!log.Observe(50.0f, 10.0f).HasDamage());
    ASSERT_EQ(log.Observe(40.0f, 10.0f).healthDamage, 10.0f);
    return true;
}

bool TestLocalKillNoticeMigratesWithoutChangingPublicBind() {
    keybinder::SettingsManager settings;
    ASSERT_TRUE(settings.LoadFromJson(R"({"kill_tracker_enabled":true,"kill_tracker_local_kill_message_template":"/r My public kill text {total_kills}","kill_tracker_server_kill_message_enabled":true,"kill_tracker_server_kill_message_template":"/r My public kill text {total_kills}"})"));
    ASSERT_EQ(settings.Get().kill_tracker_local_kill_message_template,
        std::string(keybinder::kDefaultLocalKillMessageTemplate));
    ASSERT_EQ(settings.Get().kill_tracker_server_kill_message_template, "/r My public kill text {total_kills}");
    ASSERT_TRUE(settings.Get().kill_tracker_server_kill_message_enabled);
    keybinder::SettingsManager reloaded;
    ASSERT_TRUE(reloaded.LoadFromJson(settings.GetSettingsJson()));
    ASSERT_EQ(reloaded.Get().kill_tracker_local_kill_message_template, settings.Get().kill_tracker_local_kill_message_template);
    reloaded.GetMutable().kill_tracker_local_kill_message_template = "An explicitly edited local notice {kills}";
    keybinder::SettingsManager custom;
    ASSERT_TRUE(custom.LoadFromJson(reloaded.GetSettingsJson()));
    ASSERT_EQ(custom.Get().kill_tracker_local_kill_message_template, reloaded.Get().kill_tracker_local_kill_message_template);

    CombatSoundContext context;
    auto hooks = context.BuildCombatHooks();
    hooks.getLocalPlayerId = []() { return 5; };
    hooks.getLocalPlayerName = []() { return std::string("LocalPlayer"); };
    hooks.getPlayerName = [](int id) { return "Enemy_" + std::to_string(id); };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerLocalKillMessageTemplate(settings.Get().kill_tracker_local_kill_message_template);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    bridge.SetKillTrackerServerKillMessageTemplate(settings.Get().kill_tracker_server_kill_message_template);
    ASSERT_TRUE(bridge.Initialize());
    bridge.ImportKillCount(64);
    const killtracker::DeathMessageEvent event{5, 7, 31, std::time(nullptr), true};
    bridge.ProcessDeathMessageForTesting(event);
    ASSERT_EQ(context.localMessages.size(), std::size_t(1));
    ASSERT_EQ(context.lastLocalMessage, "{33CCFF}[NL-BINDER]{FFFFFF} Du hast einen Kill gemacht! Aktuell hast du: 65 Kills.");
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_EQ(context.serverMessages[0], "/r My public kill text 65");
    bridge.ProcessDeathMessageForTesting(event);
    ASSERT_EQ(context.localMessages.size(), std::size_t(1));
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    bridge.SetKillTrackerLocalKillMessageTemplate("Kills: {kills}");
    bridge.ProcessDeathMessageForTesting({5, 8, 31, std::time(nullptr), true});
    ASSERT_EQ(context.lastLocalMessage, "Kills: 66");
    ASSERT_EQ(context.serverMessages.size(), std::size_t(2));
    bridge.SetKillTrackerLocalKillMessageEnabled(false);
    bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    ASSERT_EQ(context.localMessages.size(), std::size_t(2));
    ASSERT_EQ(context.serverMessages.size(), std::size_t(3));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

int g_manualInputHudFixCalls = 0;

bool TestManualInputDefersHudScaleCommandUntilAnswered() {
    CombatSoundContext context;
    auto hooks = context.BuildCombatHooks();
    hooks.isStartupStatsReady = []() { return false; };
    hooks.isStartupJobSkillsReady = []() { return false; };
    g_manualInputHudFixCalls = 0;
    context.commandHandlers["hudscalefix"] = [](const char*) { ++g_manualInputHudFixCalls; };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    bridge.SetAutoHudScaleFix(true);
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(bridge.BeginManualTextInput());
    strcpy_s(context.input->m_szInput, "not yet confirmed");
    for (int i = 0; i < 125; ++i) bridge.Update();
    ASSERT_EQ(g_manualInputHudFixCalls, 0);
    ASSERT_TRUE(bridge.IsChatOpen());
    std::string answer;
    ASSERT_EQ(bridge.PollManualTextInput(answer), keybinder::ManualTextInputStatus::Waiting);
    ASSERT_EQ(std::string(context.input->m_szInput), "not yet confirmed");
    ASSERT_TRUE(context.serverMessages.empty());
    SAMPBridge::ProcessNativeChatInputForTesting(context.input);
    ASSERT_EQ(bridge.PollManualTextInput(answer), keybinder::ManualTextInputStatus::Submitted);
    ASSERT_EQ(answer, "not yet confirmed");
    bridge.Update();
    ASSERT_EQ(g_manualInputHudFixCalls, 1);
    ASSERT_TRUE(context.serverMessages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestManualInputCapturesNativeChatAndCancelsCleanly() {
    using Status = keybinder::ManualTextInputStatus;
    CombatSoundContext context;
    auto hooks = context.BuildCombatHooks();
    int opened = 0, closed = 0;
    hooks.openChatInput = [&](sampapi::v03dl::CInput* input) { ++opened; input->m_bEnabled = TRUE; };
    hooks.closeChatInput = [&](sampapi::v03dl::CInput* input) { ++closed; input->m_bEnabled = FALSE; };
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());
    ASSERT_TRUE(bridge.BeginManualTextInput());
    ASSERT_EQ(opened, 1);
    ASSERT_TRUE(context.lastLocalMessage.find("Input-Text") != std::string::npos);
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_TRUE(!bridge.BeginManualTextInput());
    std::string answer;
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Waiting);
    ASSERT_TRUE(bridge.InterceptManualTextInput("/q {city}"));
    ASSERT_EQ(closed, 1);
    ASSERT_TRUE(!bridge.IsChatOpen());
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Submitted);
    ASSERT_EQ(answer, "/q {city}");
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Cancelled);
    ASSERT_TRUE(!bridge.InterceptManualTextInput("ordinary chat"));

    ASSERT_TRUE(bridge.BeginManualTextInput());
    context.input->m_bEnabled = FALSE; // Native Escape closes the chat.
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Cancelled);
    ASSERT_TRUE(!bridge.InterceptManualTextInput("after escape"));
    ASSERT_TRUE(context.serverMessages.empty());

    ASSERT_TRUE(bridge.BeginManualTextInput());
    context.netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_WAITCONNECT;
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Cancelled);
    ASSERT_TRUE(!bridge.IsChatOpen());
    ASSERT_TRUE(!bridge.BeginManualTextInput());
    context.netGame->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    context.dialog->m_bIsActive = TRUE;
    ASSERT_TRUE(!bridge.BeginManualTextInput());
    context.dialog->m_bIsActive = FALSE;

    ASSERT_TRUE(bridge.BeginManualTextInput());
    context.localPlayer->m_bIsWasted = TRUE;
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Cancelled);
    context.localPlayer->m_bIsWasted = FALSE;
    ASSERT_TRUE(bridge.BeginManualTextInput());
    context.dialog->m_bIsActive = TRUE;
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Cancelled);
    context.dialog->m_bIsActive = FALSE;
    ASSERT_TRUE(context.serverMessages.empty());
    ASSERT_TRUE(bridge.BeginManualTextInput());
    bridge.Shutdown();
    ASSERT_TRUE(!context.input->m_bEnabled);
    ASSERT_EQ(bridge.PollManualTextInput(answer), Status::Cancelled);
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestManualInputBindSendsOnlyExpandedNewsCommand() {
    CombatSoundContext context;
    auto hooks = context.BuildCombatHooks();
    SAMPBridge::SetTestingHooks(hooks);
    SAMPBridge bridge;
    ASSERT_TRUE(bridge.Initialize());
    keybinder::ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    executor.Execute({keybinder::Action(keybinder::ActionType::Chat,
        "/News In {input} befindet sich ein Blitzer. Bitte vorsichtig fahren.")});
    for (int i = 0; i < 3; ++i) executor.Update();
    ASSERT_TRUE(bridge.IsChatOpen());
    ASSERT_TRUE(context.serverMessages.empty());
    strcpy_s(context.input->m_szInput, "BSN");
    SAMPBridge::ProcessNativeChatInputForTesting(context.input);
    ASSERT_TRUE(!bridge.IsChatOpen());
    ASSERT_EQ(context.input->m_szInput[0], '\0');
    for (int i = 0; i < 3; ++i) executor.Update();
    ASSERT_TRUE(!executor.IsExecuting());
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_EQ(context.serverMessages[0], "/News In BSN befindet sich ein Blitzer. Bitte vorsichtig fahren.");
    executor.Execute({keybinder::Action(keybinder::ActionType::Chat, "In {input}")});
    for (int i = 0; i < 3; ++i) executor.Update();
    context.input->m_bEnabled = FALSE;
    executor.Update();
    ASSERT_TRUE(!executor.IsExecuting());
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));

    std::string answer;
    ASSERT_TRUE(bridge.BeginManualTextInput());
    const std::wstring umlaut = L"B\u00fcrgerhaus";
    const int length = WideCharToMultiByte(CP_ACP, 0, umlaut.data(), static_cast<int>(umlaut.size()), nullptr, 0, nullptr, nullptr);
    std::string gameText(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_ACP, 0, umlaut.data(), static_cast<int>(umlaut.size()), gameText.data(), length, nullptr, nullptr);
    ASSERT_TRUE(bridge.InterceptManualTextInput(gameText));
    ASSERT_EQ(bridge.PollManualTextInput(answer), keybinder::ManualTextInputStatus::Submitted);
    ASSERT_EQ(answer, std::string("B\xC3\xBCrgerhaus"));
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_TRUE(bridge.BeginManualTextInput());
    strcpy_s(context.input->m_szInput, "/q");
    SAMPBridge::ProcessNativeChatInputForTesting(context.input);
    ASSERT_EQ(bridge.PollManualTextInput(answer), keybinder::ManualTextInputStatus::Submitted);
    ASSERT_EQ(answer, "/q");
    ASSERT_EQ(context.serverMessages.size(), std::size_t(1));
    ASSERT_TRUE(bridge.BeginManualTextInput());
    SAMPBridge::ProcessNativeChatInputForTesting(context.input); // Empty Enter cancels.
    ASSERT_EQ(bridge.PollManualTextInput(answer), keybinder::ManualTextInputStatus::Cancelled);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

} // namespace

int RunSAMPBridgeTests() {
    std::printf("=== SAMPBridge Tests ===\n\n");
    RUN_TEST(TestManualInputCapturesNativeChatAndCancelsCleanly);
    RUN_TEST(TestManualInputBindSendsOnlyExpandedNewsCommand);
    RUN_TEST(TestManualInputDefersHudScaleCommandUntilAnswered);
    RUN_TEST(TestLocalKillNoticeMigratesWithoutChangingPublicBind);
    RUN_TEST(TestExplosionOverkillAndDelayedCemeteryCountOneDeathPerLife);
    RUN_TEST(TestCemeteryIsTheOnlyDeathCountAndDuplicateNoticeIsIgnored);
    RUN_TEST(TestLaterCemeteryCountsEvenWithPermanentlyHealthyControlledNativePed);
    RUN_TEST(TestDamageLogRejectsScriptSentinelsWithoutCappingEnhancedVitals);

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
    RUN_TEST(TestDamageLogAhkBlueprintReportsDirectServerLossWithoutRpc);
    RUN_TEST(TestDamageLogSeparatesHealingFromDamageAndFormatsOneDecimal);
    RUN_TEST(TestDamageLogDoesNotRequireOrDuplicateDamageRpcs);
    RUN_TEST(TestDamageLogToggleDiscardsOldLossAndKeepsSoundFeedbackIndependent);
    RUN_TEST(TestDamageLogSeedsLoginRespawnAndPedChangeAndReportsLethalLossOnce);
    RUN_TEST(TestHitsoundsIgnoreLoginPreviewAndResumeImmediatelyAfterSpawn);
    RUN_TEST(TestIncomingHitsoundReseedsAcrossReconnectRespawnAndPedReplacement);
    
    std::printf("\n--- Kill Tracker Tests ---\n");
    RUN_TEST(TestKillTrackerProcessKillEvent);
    RUN_TEST(TestKillTrackerDuplicateSuppression);
    RUN_TEST(TestKillTrackerSuicideFiltering);
    RUN_TEST(TestKillTrackerDamageAccumulation);
    RUN_TEST(TestKillTrackerEnabledToggle);
    RUN_TEST(TestKillTrackerStaleVictimDamageCleanup);
    RUN_TEST(TestNativeWastedWaitsForCemeteryChat);
    RUN_TEST(TestKillAndDeathCountersCanBeDisabledIndependently);
    RUN_TEST(TestCounterSettingsAppliedBeforeTrackerCreation);
    RUN_TEST(TestDisabledDeathCounterSendsNoNotifications);
    RUN_TEST(TestCombatMessageVariablesAndSlashCommandRouting);
    RUN_TEST(TestVitalsUseCurrentPedInsteadOfOldVehicleAndOnFootSync);
    RUN_TEST(TestEveryOfferedCombatVariableResolvesWithoutServerQueries);
    RUN_TEST(TestCounterSettingsAndSlashTemplatesRoundTrip);
    RUN_TEST(TestKillBinderWorksWithoutDamageTracking);
    RUN_TEST(TestReceivedServerTotalsAndKillfeedCountExactlyOnce);
    RUN_TEST(TestServerStatsRequireOwnPlayerHeaderAndConsumeContext);
    RUN_TEST(TestKillfeedStartsWithBaselineAndDoesNotReplayAfterReset);
    RUN_TEST(TestDeathRpcAndWastedCannotCountWithoutCemetery);
    RUN_TEST(TestAutoEngineStartUsesDriverTransitionsAndHeldInput);
    RUN_TEST(TestAutoEngineStartDoesNotTypeIntoBinder);
    RUN_TEST(TestAutoEngineStartWaitsForServerEngineStateAfterEntry);
    RUN_TEST(TestAutoEngineStartPressesOnlyOnceUntilActualExit);
    RUN_TEST(TestAutoEngineStartWaitsUntilChatCanReleaseInput);
    RUN_TEST(TestAutoEngineStartDoesNotRestartManuallyStoppedEngine);
    RUN_TEST(TestServerAutoEngineNoticeConsumesCurrentEntryBeforeLaggingEngineFlag);
    RUN_TEST(TestServerAutoEngineNoticeImmediatelyBeforeFirstDriverFrameAndNextEntry);
    RUN_TEST(TestServerAutoEngineNoticeRejectsPlayerPrefixAndReleasesOwnHeldKey);
    RUN_TEST(TestFastAutoEngineAddonWinsAtTheLastGuardFrame);
    RUN_TEST(TestFastAutoEngineDoesNotToggleAnAddonRunningBeforeItsNotice);
    RUN_TEST(TestServerAutoEngineNoticeIsBoundToVehicleGenerationAndFreshness);
    RUN_TEST(TestServerAutoEngineNoticeWhileOptionDisabledRemainsConsumedOnEnable);
    RUN_TEST(TestServerAutoEngineNoticeNewerThanCachedFrameClockIsNotDiscarded);
    
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
