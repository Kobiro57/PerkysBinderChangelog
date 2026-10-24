#define KEYBINDER_TESTING

#include "../include/HudSpriteManager.h"
#include "../include/SettingsManager.h"

#include <cstdio>
#include <string>

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

#define RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

bool TestDefaultDisabledBehavior() {
    HudSpriteManager manager;

    ASSERT_TRUE(!manager.IsApplied());
    ASSERT_TRUE(!manager.IsFistCustomApplied());
    ASSERT_TRUE(!manager.IsInitializedForTesting());
    return true;
}

bool TestApplyBeforeInitializeDefersFistApplication() {
    Settings settings;
    settings.fist_texture_path = "fists/custom.png";

    HudSpriteManager manager;
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(!manager.IsApplied());
    ASSERT_TRUE(!manager.IsFistCustomApplied());
    ASSERT_TRUE(!manager.IsInitializedForTesting());
    return true;
}

bool TestInitializeAppliesDeferredFistPath() {
    Settings settings;
    settings.fist_texture_path = "fists/custom.png";

    HudSpriteManager manager;
    ASSERT_TRUE(manager.ApplySettings(settings));
    manager.Initialize();

    ASSERT_TRUE(manager.IsInitializedForTesting());
    ASSERT_TRUE(manager.IsApplied());
    ASSERT_TRUE(manager.IsFistCustomApplied());
    return true;
}

bool TestEmptyFistPathKeepsNativeFist() {
    Settings settings;
    settings.fist_texture_path = "";

    HudSpriteManager manager;
    manager.Initialize();
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(!manager.IsApplied());
    ASSERT_TRUE(!manager.IsFistCustomApplied());
    return true;
}

bool TestCustomFistPathApplies() {
    Settings settings;
    settings.fist_texture_path = "fists/custom.png";

    HudSpriteManager manager;
    manager.Initialize();
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(manager.IsApplied());
    ASSERT_TRUE(manager.IsFistCustomApplied());
    return true;
}

bool TestClearingFistPathRestoresNativeFist() {
    Settings settings;
    settings.fist_texture_path = "fists/custom.png";

    HudSpriteManager manager;
    manager.Initialize();
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(manager.IsFistCustomApplied());

    settings.fist_texture_path.clear();
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(!manager.IsApplied());
    ASSERT_TRUE(!manager.IsFistCustomApplied());
    return true;
}

bool TestDeviceLostAndResetRestoreOnlyFist() {
    Settings settings;
    settings.fist_texture_path = "fists/custom.png";

    HudSpriteManager manager;
    manager.Initialize();
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(manager.IsApplied());

    manager.OnDeviceLost();
    ASSERT_TRUE(!manager.IsApplied());

    manager.OnDeviceReset();
    ASSERT_TRUE(manager.IsApplied());
    ASSERT_TRUE(manager.IsFistCustomApplied());
    return true;
}

bool TestShutdownRestoresDisabledAppliedState() {
    Settings settings;
    settings.fist_texture_path = "fists/custom.png";

    HudSpriteManager manager;
    manager.Initialize();
    ASSERT_TRUE(manager.ApplySettings(settings));
    ASSERT_TRUE(manager.IsApplied());

    manager.Shutdown();
    ASSERT_TRUE(!manager.IsInitializedForTesting());
    ASSERT_TRUE(!manager.IsApplied());
    ASSERT_TRUE(!manager.IsFistCustomApplied());
    return true;
}

} // namespace

int RunHudSpriteManagerTests() {
    std::printf("\n=== HudSpriteManager Tests ===\n\n");
    g_state = TestState();

    RUN_TEST(TestDefaultDisabledBehavior);
    RUN_TEST(TestApplyBeforeInitializeDefersFistApplication);
    RUN_TEST(TestInitializeAppliesDeferredFistPath);
    RUN_TEST(TestEmptyFistPathKeepsNativeFist);
    RUN_TEST(TestCustomFistPathApplies);
    RUN_TEST(TestClearingFistPathRestoresNativeFist);
    RUN_TEST(TestDeviceLostAndResetRestoreOnlyFist);
    RUN_TEST(TestShutdownRestoresDisabledAppliedState);

    std::printf("\nHudSpriteManager Tests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
