#define KEYBINDER_TESTING

#include "../include/HitsoundPlayer.h"
#include "../include/Logger.h"
#include "../include/SettingsManager.h"
#include "../include/IncomingDamageDetector.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

struct HookState {
    bool failInitialization = false;
    long long nowMs = 0;
    std::vector<std::string> rejectedSourceFragments;
    std::vector<std::string> logs;
};

TestState g_state;
HookState g_hooks;

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

std::chrono::steady_clock::time_point TestNow() {
    return std::chrono::steady_clock::time_point{} + std::chrono::milliseconds(g_hooks.nowMs);
}

bool CanLoadSource(keybinder::HitsoundPlayer::Direction, const std::string& sourceKey, bool) {
    for (const auto& fragment : g_hooks.rejectedSourceFragments) {
        if (sourceKey.find(fragment) != std::string::npos) {
            return false;
        }
    }

    return true;
}

void CaptureLog(keybinder::LogLevel, const char* message) {
    g_hooks.logs.emplace_back(message);
}

void ResetHooks() {
    g_hooks = HookState{};
    keybinder::HitsoundPlayer::ResetTestingHooks();
    keybinder::Logger::ResetTestSink();
}

void ApplyHooks() {
    keybinder::HitsoundPlayer::TestingHooks hooks;
    hooks.failInitialization = g_hooks.failInitialization;
    hooks.nowProvider = TestNow;
    hooks.canLoadSource = CanLoadSource;
    keybinder::HitsoundPlayer::SetTestingHooks(hooks);
    keybinder::Logger::SetTestSink(CaptureLog);
}

killtracker::DamageEvent MakeDamageEvent(bool isOutgoing, int weaponId = 24) {
    killtracker::DamageEvent event;
    event.targetId = isOutgoing ? 12 : 0;
    event.damage = 25.0f;
    event.weaponId = weaponId;
    event.bodyPart = 9;
    event.timestamp = std::time(nullptr);
    event.isOutgoing = isOutgoing;
    return event;
}

bool LogsContain(const std::string& needle) {
    return std::any_of(g_hooks.logs.begin(), g_hooks.logs.end(), [&](const std::string& line) {
        return line.find(needle) != std::string::npos;
    });
}

bool Test_DefaultFallbackQueuesAndPlaysBothDirections() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    ASSERT_TRUE(player.IsEnabled());
    ASSERT_TRUE(player.IsRuntimeAvailableForTesting());
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Incoming));

    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(false));

    ASSERT_EQ(player.GetPendingEventCountForTesting(), static_cast<std::size_t>(2));

    player.Update();

    ASSERT_EQ(player.GetPendingEventCountForTesting(), static_cast<std::size_t>(0));
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);
    return true;
}

bool Test_DirectionCooldownBlocksSpamUntil90ms() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(false));
    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);

    g_hooks.nowMs += 89;
    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);

    g_hooks.nowMs += 1;
    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 2);
    return true;
}

bool Test_ApplySettingsRespectsDisabledAndCustomSlots() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = false;
    settings.hitsound_outgoing_path = "";
    settings.hitsound_incoming_enabled = true;
    settings.hitsound_incoming_path = "hitsounds\\incoming_custom.wav";

    player.ApplySettings(settings);

    ASSERT_TRUE(!player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Incoming));

    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);
    return true;
}

bool Test_CustomLoadFailureLogsAndSkipsPlayback() {
    ResetHooks();
    g_hooks.rejectedSourceFragments.push_back("bad.wav");
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = true;
    settings.hitsound_outgoing_path = "hitsounds\\bad.wav";
    settings.hitsound_incoming_enabled = false;
    settings.hitsound_incoming_path = "";

    player.ApplySettings(settings);

    ASSERT_TRUE(!player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_TRUE(LogsContain("Failed to load custom outgoing hitsound"));
    return true;
}

bool Test_RuntimeInitializationFailureDisablesPlayerGracefully() {
    ResetHooks();
    g_hooks.failInitialization = true;
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    ASSERT_TRUE(!player.IsEnabled());
    ASSERT_TRUE(!player.IsRuntimeAvailableForTesting());

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_TRUE(LogsContain("XAudio2 initialization failed"));
    return true;
}

bool Test_OutgoingEventChoosesOutgoingSlot() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    return true;
}

bool Test_IncomingEventChoosesIncomingSlot() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);
    return true;
}

bool Test_SelfDamageWeaponDoesNotPlayHitsound() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(true, 54));
    player.OnDamageEvent(MakeDamageEvent(false, 54));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    return true;
}

bool Test_EmptyPathUsesEmbeddedDefault() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Incoming));

    ASSERT_EQ(player.GetOutgoingPath(), std::string(""));
    ASSERT_EQ(player.GetIncomingPath(), std::string(""));
    return true;
}

bool Test_DisabledDirectionSuppressesPlayback() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = false;
    settings.hitsound_incoming_enabled = true;

    player.ApplySettings(settings);

    ASSERT_TRUE(!player.IsOutgoingEnabled());
    ASSERT_TRUE(player.IsIncomingEnabled());

    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);

    settings.hitsound_incoming_enabled = false;
    player.ApplySettings(settings);

    ASSERT_TRUE(!player.IsIncomingEnabled());

    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);
    return true;
}

bool Test_CustomLogicalPathPreservedAndSelected() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    const std::string customPath = "hitsounds\\custom_deagle.wav";
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = true;
    settings.hitsound_outgoing_path = customPath;
    settings.hitsound_incoming_enabled = true;
    settings.hitsound_incoming_path = "";

    player.ApplySettings(settings);

    ASSERT_EQ(player.GetOutgoingPath(), customPath);

    ASSERT_EQ(player.GetIncomingPath(), std::string(""));

    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Incoming));

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);
    return true;
}

bool Test_CooldownSuppressesRapidRepeatedPlayback() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);

    g_hooks.nowMs += 89;
    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);

    g_hooks.nowMs += 1;
    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 2);

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);
    return true;
}

bool Test_InitAndLoadFailuresDegradeSafely() {
    ResetHooks();
    g_hooks.failInitialization = true;
    ApplyHooks();

    {
        keybinder::HitsoundPlayer player;
        ASSERT_TRUE(!player.IsEnabled());
        ASSERT_TRUE(!player.IsRuntimeAvailableForTesting());

        player.OnDamageEvent(MakeDamageEvent(true));
        player.OnDamageEvent(MakeDamageEvent(false));
        player.Update();
        ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
        ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    }

    ResetHooks();
    g_hooks.rejectedSourceFragments.push_back("embedded:");
    ApplyHooks();

    {
        keybinder::HitsoundPlayer player;
        ASSERT_TRUE(player.IsEnabled());
        ASSERT_TRUE(player.IsRuntimeAvailableForTesting());
        ASSERT_TRUE(!player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
        ASSERT_TRUE(!player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Incoming));

        player.OnDamageEvent(MakeDamageEvent(true));
        player.Update();
        ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    }

    ResetHooks();
    g_hooks.rejectedSourceFragments.push_back("nonexistent.wav");
    ApplyHooks();

    {
        keybinder::HitsoundPlayer player;
        keybinder::Settings settings;
        settings.hitsound_outgoing_enabled = true;
        settings.hitsound_outgoing_path = "nonexistent.wav";
        player.ApplySettings(settings);

        ASSERT_TRUE(player.IsEnabled());
        ASSERT_TRUE(!player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    }
    return true;
}

bool Test_PreviewMethodsPlayCorrectDirection() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = true;
    settings.hitsound_outgoing_path = "hitsounds\\out.wav";
    settings.hitsound_outgoing_volume = 0.15f;
    settings.hitsound_incoming_enabled = true;
    settings.hitsound_incoming_path = "hitsounds\\in.wav";
    settings.hitsound_incoming_volume = 0.65f;
    player.ApplySettings(settings);

    player.PreviewOutgoing();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    ASSERT_EQ(player.GetPreviewPlayCountForTesting(), 1);
    ASSERT_TRUE(player.HasLoadedPreviewSoundForTesting());
    ASSERT_TRUE(player.GetPreviewSourceKeyForTesting().find("hitsounds\\out.wav") != std::string::npos);
    ASSERT_EQ(player.GetPreviewVolumeForTesting(), 0.15f);

    g_hooks.nowMs += 100;
    player.PreviewIncoming();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    ASSERT_EQ(player.GetPreviewPlayCountForTesting(), 2);
    ASSERT_TRUE(player.HasLoadedPreviewSoundForTesting());
    ASSERT_TRUE(player.GetPreviewSourceKeyForTesting().find("hitsounds\\in.wav") != std::string::npos);
    ASSERT_EQ(player.GetPreviewVolumeForTesting(), 0.65f);
    return true;
}

bool Test_PreviewPath_DoesNotMutateConfiguredSlotState() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = false;
    settings.hitsound_outgoing_path = "hitsounds\\configured.wav";
    settings.hitsound_outgoing_volume = 0.35f;
    player.ApplySettings(settings);

    player.PreviewPath(keybinder::HitsoundPlayer::Direction::Outgoing, "hitsounds\\preview.wav");

    ASSERT_EQ(player.GetOutgoingPath(), std::string("hitsounds\\configured.wav"));
    ASSERT_TRUE(!player.IsOutgoingEnabled());
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPreviewPlayCountForTesting(), 1);
    ASSERT_TRUE(player.HasLoadedPreviewSoundForTesting());
    ASSERT_TRUE(player.GetPreviewSourceKeyForTesting().find("hitsounds\\preview.wav") != std::string::npos);
    ASSERT_EQ(player.GetPreviewVolumeForTesting(), 0.35f);
    return true;
}

bool Test_PreviewOutgoing_PlaysEvenWhenSlotDisabled() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = false;
    settings.hitsound_outgoing_path = "hitsounds\\disabled.wav";
    settings.hitsound_outgoing_volume = 0.42f;
    player.ApplySettings(settings);

    player.PreviewOutgoing();

    ASSERT_TRUE(!player.IsOutgoingEnabled());
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    ASSERT_EQ(player.GetPreviewPlayCountForTesting(), 1);
    ASSERT_TRUE(player.HasLoadedPreviewSoundForTesting());
    ASSERT_TRUE(player.GetPreviewSourceKeyForTesting().find("hitsounds\\disabled.wav") != std::string::npos);
    ASSERT_EQ(player.GetPreviewVolumeForTesting(), 0.42f);
    return true;
}

bool Test_PreviewPath_BypassesAndDoesNotConsumeGameplayCooldown() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = true;
    settings.hitsound_outgoing_path = "hitsounds\\configured.wav";
    player.ApplySettings(settings);

    player.PreviewPath(keybinder::HitsoundPlayer::Direction::Outgoing, "hitsounds\\preview.wav");
    player.PreviewPath(keybinder::HitsoundPlayer::Direction::Outgoing, "hitsounds\\preview.wav");

    ASSERT_EQ(player.GetPreviewPlayCountForTesting(), 2);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);
    return true;
}

bool Test_ShutdownClearsPendingEvents() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(false));
    ASSERT_EQ(player.GetPendingEventCountForTesting(), static_cast<std::size_t>(2));

    player.Shutdown();

    ASSERT_EQ(player.GetPendingEventCountForTesting(), static_cast<std::size_t>(0));

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    return true;
}

bool Test_EachDirectionHasIndependentCooldown() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);

    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);

    player.OnDamageEvent(MakeDamageEvent(true));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 1);

    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);

    g_hooks.nowMs += 90;
    player.OnDamageEvent(MakeDamageEvent(true));
    player.OnDamageEvent(MakeDamageEvent(false));
    player.Update();
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 2);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 2);
    return true;
}

bool Test_ApplySettingsReloadsSlotsOnPathChange() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;

    ASSERT_EQ(player.GetOutgoingPath(), std::string(""));
    ASSERT_EQ(player.GetIncomingPath(), std::string(""));

    keybinder::Settings settings1;
    settings1.hitsound_outgoing_enabled = true;
    settings1.hitsound_outgoing_path = "hitsounds\\new_sound.wav";
    settings1.hitsound_incoming_enabled = true;
    settings1.hitsound_incoming_path = "";

    player.ApplySettings(settings1);
    ASSERT_EQ(player.GetOutgoingPath(), std::string("hitsounds\\new_sound.wav"));
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));

    keybinder::Settings settings2;
    settings2.hitsound_outgoing_enabled = true;
    settings2.hitsound_outgoing_path = "";
    settings2.hitsound_incoming_enabled = true;
    settings2.hitsound_incoming_path = "";

    player.ApplySettings(settings2);
    ASSERT_EQ(player.GetOutgoingPath(), std::string(""));
    ASSERT_TRUE(player.HasLoadedSoundForTesting(keybinder::HitsoundPlayer::Direction::Outgoing));
    return true;
}

bool Test_ApplySettingsStoresPerDirectionVolume() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_volume = 0.15f;
    settings.hitsound_incoming_volume = 0.65f;

    player.ApplySettings(settings);

    ASSERT_EQ(player.GetVolumeForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0.15f);
    ASSERT_EQ(player.GetVolumeForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0.65f);
    return true;
}

bool Test_IncomingDetector_SeedDoesNotFire() {
    keybinder::IncomingDamageDetector detector;
    ASSERT_TRUE(!detector.Observe(true, 150.0f, false));
    return true;
}

bool Test_IncomingDetector_HealthDropFires() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 150.0f, false);
    ASSERT_TRUE(detector.Observe(true, 118.0f, false));
    return true;
}

bool Test_IncomingDetector_IncreaseAndStaticDoNotFire() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);
    ASSERT_TRUE(!detector.Observe(true, 100.0f, false));  // no change
    ASSERT_TRUE(!detector.Observe(true, 200.0f, false));  // heal / armour pickup
    return true;
}

bool Test_IncomingDetector_EnvironmentalDamageSuppressed() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);
    ASSERT_TRUE(!detector.Observe(true, 90.0f, true));   // fall/drown suppressed
    ASSERT_TRUE(detector.Observe(true, 80.0f, false));   // baseline advanced; real hit fires
    return true;
}

bool Test_IncomingDetector_ReseedsAfterDeath() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);
    ASSERT_TRUE(!detector.Observe(false, 0.0f, false));  // dead -> reset, no fire
    ASSERT_TRUE(!detector.Observe(true, 100.0f, false)); // respawn reseeds
    ASSERT_TRUE(detector.Observe(true, 64.0f, false));   // next hit fires
    return true;
}

bool Test_NonCombatDamageSourceClassification() {
    ASSERT_TRUE(keybinder::IsNonCombatDamageSource(54));   // fall / collision / self
    ASSERT_TRUE(keybinder::IsNonCombatDamageSource(53));   // drowning
    ASSERT_TRUE(keybinder::IsNonCombatDamageSource(50));   // run over by car
    ASSERT_TRUE(keybinder::IsNonCombatDamageSource(49));   // rammed by car
    ASSERT_TRUE(!keybinder::IsNonCombatDamageSource(24));  // desert eagle -> combat
    ASSERT_TRUE(!keybinder::IsNonCombatDamageSource(34));  // sniper still plays incoming hitsound
    ASSERT_TRUE(!keybinder::IsNonCombatDamageSource(0));   // fists still play incoming hitsound
    return true;
}

bool Test_IncomingDetector_ScriptedSuppressionConsumesNextDrop() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);  // seed
    detector.ArmScriptedSuppression();
    ASSERT_TRUE(detector.IsScriptedSuppressionArmed());
    // The script-driven drop is suppressed (not a combat hit).
    ASSERT_TRUE(!detector.Observe(true, 80.0f, false));
    // Window consumed; a subsequent real hit fires normally.
    ASSERT_TRUE(detector.Observe(true, 60.0f, false));
    return true;
}

bool Test_IncomingDetector_ScriptedSuppressionDoesNotMuteLaterHit() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);  // seed
    detector.ArmScriptedSuppression();
    // No drop yet; after frame-cap expires, a real hit still fires.
    for (int i = 0; i < keybinder::IncomingDamageDetector::kScriptedSuppressionFrameCap; ++i) {
        ASSERT_TRUE(!detector.Observe(true, 100.0f, false));  // static, no drop
    }
    ASSERT_TRUE(!detector.IsScriptedSuppressionArmed());
    ASSERT_TRUE(detector.Observe(true, 70.0f, false));  // real hit fires
    return true;
}

bool Test_IncomingDetector_ScriptedSuppressionNotConsumedByEnvironmental() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);  // seed
    detector.ArmScriptedSuppression();
    // Environmental drop is suppressed by its own filter and must NOT consume
    // the scripted window.
    ASSERT_TRUE(!detector.Observe(true, 90.0f, true));
    ASSERT_TRUE(detector.IsScriptedSuppressionArmed());
    // The subsequent script-driven drop consumes the window.
    ASSERT_TRUE(!detector.Observe(true, 75.0f, false));
    ASSERT_TRUE(!detector.IsScriptedSuppressionArmed());
    return true;
}

bool Test_IncomingDetector_ScriptedSuppressionClearsOnDeath() {
    keybinder::IncomingDamageDetector detector;
    detector.Observe(true, 100.0f, false);  // seed
    detector.ArmScriptedSuppression();
    ASSERT_TRUE(detector.IsScriptedSuppressionArmed());
    ASSERT_TRUE(!detector.Observe(false, 0.0f, false));  // death clears window
    ASSERT_TRUE(!detector.IsScriptedSuppressionArmed());
    // After respawn reseed, a real hit fires normally (no stale suppression).
    ASSERT_TRUE(!detector.Observe(true, 100.0f, false));
    ASSERT_TRUE(detector.Observe(true, 64.0f, false));
    return true;
}

bool Test_OnIncomingDamagePlaysIncomingSlot() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = true;
    settings.hitsound_incoming_enabled = true;
    player.ApplySettings(settings);

    player.OnIncomingDamage();
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 1);
    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Outgoing), 0);
    return true;
}

bool Test_OnIncomingDamageRespectsDisabledIncomingSlot() {
    ResetHooks();
    ApplyHooks();

    keybinder::HitsoundPlayer player;
    keybinder::Settings settings;
    settings.hitsound_outgoing_enabled = true;
    settings.hitsound_incoming_enabled = false;
    player.ApplySettings(settings);

    player.OnIncomingDamage();
    player.Update();

    ASSERT_EQ(player.GetPlayCountForTesting(keybinder::HitsoundPlayer::Direction::Incoming), 0);
    return true;
}

} // namespace

int RunHitsoundTests() {
    std::printf("=== Hitsound Tests ===\n\n");

    std::printf("--- Core Playback Tests ---\n");
    RUN_TEST(Test_DefaultFallbackQueuesAndPlaysBothDirections);
    RUN_TEST(Test_OutgoingEventChoosesOutgoingSlot);
    RUN_TEST(Test_IncomingEventChoosesIncomingSlot);
    RUN_TEST(Test_SelfDamageWeaponDoesNotPlayHitsound);
    RUN_TEST(Test_EmptyPathUsesEmbeddedDefault);
    RUN_TEST(Test_DisabledDirectionSuppressesPlayback);
    RUN_TEST(Test_CustomLogicalPathPreservedAndSelected);
    RUN_TEST(Test_PreviewMethodsPlayCorrectDirection);
    RUN_TEST(Test_PreviewPath_DoesNotMutateConfiguredSlotState);
    RUN_TEST(Test_PreviewOutgoing_PlaysEvenWhenSlotDisabled);
    RUN_TEST(Test_PreviewPath_BypassesAndDoesNotConsumeGameplayCooldown);

    std::printf("\n--- Cooldown Tests ---\n");
    RUN_TEST(Test_DirectionCooldownBlocksSpamUntil90ms);
    RUN_TEST(Test_CooldownSuppressesRapidRepeatedPlayback);
    RUN_TEST(Test_EachDirectionHasIndependentCooldown);

    std::printf("\n--- Settings Reload Tests ---\n");
    RUN_TEST(Test_ApplySettingsRespectsDisabledAndCustomSlots);
    RUN_TEST(Test_ApplySettingsReloadsSlotsOnPathChange);
    RUN_TEST(Test_ApplySettingsStoresPerDirectionVolume);

    std::printf("\n--- Failure Degradation Tests ---\n");
    RUN_TEST(Test_CustomLoadFailureLogsAndSkipsPlayback);
    RUN_TEST(Test_RuntimeInitializationFailureDisablesPlayerGracefully);
    RUN_TEST(Test_InitAndLoadFailuresDegradeSafely);
    RUN_TEST(Test_ShutdownClearsPendingEvents);

    std::printf("\n--- Incoming Damage Detection Tests ---\n");
    RUN_TEST(Test_IncomingDetector_SeedDoesNotFire);
    RUN_TEST(Test_IncomingDetector_HealthDropFires);
    RUN_TEST(Test_IncomingDetector_IncreaseAndStaticDoNotFire);
    RUN_TEST(Test_IncomingDetector_EnvironmentalDamageSuppressed);
    RUN_TEST(Test_IncomingDetector_ReseedsAfterDeath);
    RUN_TEST(Test_NonCombatDamageSourceClassification);
    RUN_TEST(Test_IncomingDetector_ScriptedSuppressionConsumesNextDrop);
    RUN_TEST(Test_IncomingDetector_ScriptedSuppressionDoesNotMuteLaterHit);
    RUN_TEST(Test_IncomingDetector_ScriptedSuppressionNotConsumedByEnvironmental);
    RUN_TEST(Test_IncomingDetector_ScriptedSuppressionClearsOnDeath);
    RUN_TEST(Test_OnIncomingDamagePlaysIncomingSlot);
    RUN_TEST(Test_OnIncomingDamageRespectsDisabledIncomingSlot);

    keybinder::Logger::ResetTestSink();
    keybinder::HitsoundPlayer::ResetTestingHooks();

    std::printf("\n=== Hitsound Results: %d passed, %d failed ===\n", g_state.passed, g_state.failed);
    return g_state.failed == 0 ? 0 : 1;
}
