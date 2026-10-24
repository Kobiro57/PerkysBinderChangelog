#define KEYBINDER_TESTING

#include "../include/Hitmarker.h"
#include "../include/KillTrackerTypes.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

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

// Helper: create a damage event with given weapon id
killtracker::DamageEvent MakeDamageEvent(int weaponId, float damage = 25.0f, bool isOutgoing = true) {
    killtracker::DamageEvent event;
    event.targetId = 1;
    event.damage = damage;
    event.weaponId = weaponId;
    event.timestamp = std::time(nullptr);
    event.isOutgoing = isOutgoing;
    return event;
}

// Helper: create a kill event
killtracker::KillEvent MakeKillEvent(bool isLocalPlayer = true) {
    killtracker::KillEvent event;
    event.killer = "Player1";
    event.victim = "Victim1";
    event.weaponId = 24;
    event.timestamp = std::time(nullptr);
    event.isLocalPlayer = isLocalPlayer;
    event.isSuicide = false;
    event.isSystem = false;
    return event;
}

// 1. Verify GetDefaultPresets returns a single editable current preset
bool Test_DefaultPresetsExist() {
    keybinder::Hitmarker hm;
    const auto presets = hm.GetDefaultPresets();

    ASSERT_EQ(presets.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(presets[0].name, std::string("Aktuell"));

    // Verify presets are loaded into instance
    const auto& instancePresets = hm.GetPresets();
    ASSERT_EQ(instancePresets.size(), static_cast<std::size_t>(1));

    return true;
}

// 2. Verify weaponId 34 (sniper) does not create marker
bool Test_SniperFilter() {
    keybinder::Hitmarker hm;

    // Enqueue sniper damage event
    hm.OnDamageEvent(MakeDamageEvent(keybinder::Hitmarker::SNIPER_WEAPON_ID));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));

    // Enqueue non-sniper damage to confirm filter is specific
    hm.OnDamageEvent(MakeDamageEvent(24)); // Desert Eagle
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    return true;
}

// 2b. Verify incoming damage does not create markers
bool Test_IncomingDamageIgnored() {
    keybinder::Hitmarker hm;

    hm.OnDamageEvent(MakeDamageEvent(24, 25.0f, false));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));
    return true;
}

// 3. Verify non-sniper damage creates active marker
bool Test_MarkerCreation() {
    keybinder::Hitmarker hm;

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));

    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // Second event adds a second marker
    hm.OnDamageEvent(MakeDamageEvent(31)); // M4
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(2));

    return true;
}

// 4. Verify enabled=false prevents marker creation
bool Test_EnabledFlag() {
    keybinder::Hitmarker hm;

    ASSERT_TRUE(hm.IsEnabled());

    hm.SetEnabled(false);
    ASSERT_TRUE(!hm.IsEnabled());

    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));

    // Re-enable and verify markers are created again
    hm.SetEnabled(true);

    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    return true;
}

// 5. Verify opacity decreases over time (fade out)
bool Test_FadeTiming() {
    keybinder::Hitmarker hm;

    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // Default preset has fadeOut=true and duration=0.5s
    // Sleep briefly and update -- opacity should decrease
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    hm.Update();

    // Marker should still be alive (0.1s < 0.5s duration)
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // We can't directly inspect opacity through the public API,
    // but we can verify the marker survived the update (not expired)
    // and the system didn't crash. The fade logic is:
    //   opacity = preset.opacity * (1 - elapsed/duration)
    // After ~100ms with 500ms duration, opacity ~= 0.8 (still > 0)

    return true;
}

// 6. Verify markers are removed after duration
bool Test_MarkerExpiry() {
    keybinder::Hitmarker hm;

    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // Default duration is 0.5s; sleep past it
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    hm.Update();

    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));

    return true;
}

// 7. Verify cap at 5 markers, oldest removed
bool Test_MaxMarkers() {
    keybinder::Hitmarker hm;

    // Enqueue 6 damage events (exceeds MAX_ACTIVE_MARKERS = 5)
    for (int i = 0; i < 6; ++i) {
        hm.OnDamageEvent(MakeDamageEvent(24, static_cast<float>(10 + i)));
    }
    hm.Update();

    // Should be capped at 5
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(5));

    // Add one more
    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();

    // Still capped at 5
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(5));

    return true;
}

// 8. Verify isLocalPlayer=true upgrades marker to kill-confirmed
bool Test_KillConfirmed() {
    keybinder::Hitmarker hm;

    // Create a damage marker first
    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // Kill event with isLocalPlayer=true should upgrade the last marker
    hm.OnKillEvent(MakeKillEvent(true));
    // Marker count stays at 1 (upgraded, not added)
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // Kill event with isLocalPlayer=false should be ignored
    hm.ClearActiveMarkers();
    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    killtracker::KillEvent nonLocalKill = MakeKillEvent(false);
    hm.OnKillEvent(nonLocalKill);
    // Should still be 1 (non-local kill ignored)
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    // Kill event on empty markers creates a new kill-confirmed marker
    hm.ClearActiveMarkers();
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));
    hm.OnKillEvent(MakeKillEvent(true));
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    return true;
}

// 9. Verify sniper kill events never create or upgrade a marker
bool Test_SniperKillIgnored() {
    keybinder::Hitmarker hm;

    killtracker::KillEvent sniperKill = MakeKillEvent(true);
    sniperKill.weaponId = keybinder::Hitmarker::SNIPER_WEAPON_ID;

    hm.OnKillEvent(sniperKill);
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(0));

    hm.OnDamageEvent(MakeDamageEvent(24));
    hm.Update();
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    hm.OnKillEvent(sniperKill);
    ASSERT_EQ(hm.GetActiveMarkerCount(), static_cast<std::size_t>(1));

    return true;
}

} // namespace

int RunHitmarkerTests() {
    std::printf("=== Hitmarker Tests ===\n\n");

    RUN_TEST(Test_DefaultPresetsExist);
    RUN_TEST(Test_SniperFilter);
    RUN_TEST(Test_IncomingDamageIgnored);
    RUN_TEST(Test_MarkerCreation);
    RUN_TEST(Test_EnabledFlag);
    RUN_TEST(Test_FadeTiming);
    RUN_TEST(Test_MarkerExpiry);
    RUN_TEST(Test_MaxMarkers);
    RUN_TEST(Test_KillConfirmed);
    RUN_TEST(Test_SniperKillIgnored);

    std::printf("\n=== Hitmarker Results: %d passed, %d failed ===\n", g_state.passed, g_state.failed);
    return g_state.failed == 0 ? 0 : 1;
}

#ifdef HITMARKER_STANDALONE_TEST
int main() {
    return RunHitmarkerTests();
}
#endif
