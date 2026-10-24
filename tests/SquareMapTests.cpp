#define KEYBINDER_TESTING

#include "../include/SquareMap.h"
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
    SquareMap squareMap;

    ASSERT_TRUE(!squareMap.IsEnabled());
    ASSERT_TRUE(!squareMap.IsApplied());
    ASSERT_TRUE(!squareMap.IsInitializedForTesting());
    return true;
}

bool TestApplyBeforeInitializeDefersNativeApplication() {
    Settings settings;
    settings.squaremap_enabled = true;

    SquareMap squareMap;
    ASSERT_TRUE(squareMap.ApplySettings(settings));
    ASSERT_TRUE(squareMap.IsEnabled());
    ASSERT_TRUE(!squareMap.IsApplied());
    ASSERT_TRUE(!squareMap.IsInitializedForTesting());
    return true;
}

bool TestInitializeAppliesDeferredEnabledState() {
    Settings settings;
    settings.squaremap_enabled = true;

    SquareMap squareMap;
    ASSERT_TRUE(squareMap.ApplySettings(settings));
    squareMap.Initialize();

    ASSERT_TRUE(squareMap.IsInitializedForTesting());
    ASSERT_TRUE(squareMap.IsEnabled());
    ASSERT_TRUE(squareMap.IsApplied());
    return true;
}

bool TestDisablingAfterEnablingClearsAppliedState() {
    Settings settings;
    settings.squaremap_enabled = true;

    SquareMap squareMap;
    squareMap.Initialize();
    ASSERT_TRUE(squareMap.ApplySettings(settings));
    ASSERT_TRUE(squareMap.IsApplied());

    settings.squaremap_enabled = false;
    ASSERT_TRUE(squareMap.ApplySettings(settings));
    ASSERT_TRUE(!squareMap.IsEnabled());
    ASSERT_TRUE(!squareMap.IsApplied());
    return true;
}

bool TestDeviceLostAndResetRestoreOnlyWhenEnabled() {
    Settings settings;
    settings.squaremap_enabled = true;

    SquareMap squareMap;
    squareMap.Initialize();
    ASSERT_TRUE(squareMap.ApplySettings(settings));
    ASSERT_TRUE(squareMap.IsApplied());

    squareMap.OnDeviceLost();
    ASSERT_TRUE(squareMap.IsEnabled());
    ASSERT_TRUE(!squareMap.IsApplied());

    squareMap.OnDeviceReset();
    ASSERT_TRUE(squareMap.IsEnabled());
    ASSERT_TRUE(squareMap.IsApplied());
    return true;
}

bool TestShutdownRestoresDisabledAppliedState() {
    Settings settings;
    settings.squaremap_enabled = true;

    SquareMap squareMap;
    squareMap.Initialize();
    ASSERT_TRUE(squareMap.ApplySettings(settings));
    ASSERT_TRUE(squareMap.IsApplied());

    squareMap.Shutdown();
    ASSERT_TRUE(!squareMap.IsInitializedForTesting());
    ASSERT_TRUE(!squareMap.IsEnabled());
    ASSERT_TRUE(!squareMap.IsApplied());
    return true;
}

} // namespace

int RunSquareMapTests() {
    std::printf("\n=== SquareMap Tests ===\n\n");
    g_state = TestState();

    RUN_TEST(TestDefaultDisabledBehavior);
    RUN_TEST(TestApplyBeforeInitializeDefersNativeApplication);
    RUN_TEST(TestInitializeAppliesDeferredEnabledState);
    RUN_TEST(TestDisablingAfterEnablingClearsAppliedState);
    RUN_TEST(TestDeviceLostAndResetRestoreOnlyWhenEnabled);
    RUN_TEST(TestShutdownRestoresDisabledAppliedState);

    std::printf("\nSquareMap Tests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
