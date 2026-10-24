#include "../include/SettingsManager.h"
#include "../include/SpeedometerManager.h"
#include "../include/TextDrawAdapter.h"
#include "../include/TextDrawRPC.h"
#include "../include/Logger.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

struct FocusedTestResult {
    int passed = 0;
    int failed = 0;
    std::string last_error;
};

FocusedTestResult g_results;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        g_results.failed++; \
        g_results.last_error = "Assertion failed: " #cond; \
        return false; \
    } \
    g_results.passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        g_results.failed++; \
        g_results.last_error = "Equality check failed: " #a " != " #b; \
        return false; \
    } \
    g_results.passed++; \
} while(0)

#define ASSERT_NEAR(a, b, epsilon) do { \
    if (std::fabs((a) - (b)) > (epsilon)) { \
        g_results.failed++; \
        g_results.last_error = "Near check failed: " #a " != " #b; \
        return false; \
    } \
    g_results.passed++; \
} while(0)

#define RUN_TEST(name) do { \
    if (name()) { \
        std::printf("[PASS] %s\n", #name); \
    } else { \
        std::printf("[FAIL] %s: %s\n", #name, g_results.last_error.c_str()); \
    } \
} while(0)

} // namespace

bool TestSpeedometerFocusedSettingsRoundTrip() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "speedometer_enabled": true,
  "speedometer_style": "tachometer_arc",
  "speedometer_hide_textdraw_ids": [5, 10, 15],
  "speedometer_fuel_textdraw_ids": [20]
})json"));
    ASSERT_EQ(settingsManager.Get().speedometer_enabled, false);
    ASSERT_TRUE(settingsManager.GetSettingsJson().find("\"speedometer_enabled\": false") != std::string::npos);

    keybinder::SettingsManager roundTrip;
    ASSERT_TRUE(roundTrip.LoadFromJson(settingsManager.GetSettingsJson()));
    ASSERT_EQ(roundTrip.Get().speedometer_enabled, false);
    ASSERT_EQ(roundTrip.Get().speedometer_style, std::string("tachometer_arc"));
    ASSERT_TRUE(roundTrip.GetSettingsJson().find("\"speedometer_hide_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(roundTrip.GetSettingsJson().find("\"speedometer_fuel_textdraw_ids\"") == std::string::npos);
    keybinder::SettingsManager analogRoundTrip;
    ASSERT_TRUE(analogRoundTrip.LoadFromJson(R"json({ "version": 1, "speedometer_style": "analog_arc" })json"));
    ASSERT_EQ(analogRoundTrip.Get().speedometer_style, std::string("analog_arc"));
    ASSERT_TRUE(analogRoundTrip.GetSettingsJson().find("\"speedometer_style\": \"analog_arc\"") != std::string::npos);

    keybinder::SettingsManager minimalRoundTrip;
    ASSERT_TRUE(minimalRoundTrip.LoadFromJson(R"json({ "version": 1, "speedometer_style": "minimal_digital" })json"));
    ASSERT_EQ(minimalRoundTrip.Get().speedometer_style, std::string("minimal_digital"));
    ASSERT_TRUE(minimalRoundTrip.GetSettingsJson().find("\"speedometer_style\": \"minimal_digital\"") != std::string::npos);
    return true;
}

bool TestSpeedometerFocusedStyleHelpersNormalize() {
    ASSERT_EQ(std::string(keybinder::speedometer::SpeedometerStyleId(keybinder::speedometer::SpeedometerStyle::TachometerArc)), std::string("tachometer_arc"));
    ASSERT_EQ(std::string(keybinder::speedometer::SpeedometerStyleId(keybinder::speedometer::SpeedometerStyle::AnalogArc)), std::string("analog_arc"));
    ASSERT_EQ(std::string(keybinder::speedometer::SpeedometerStyleId(keybinder::speedometer::SpeedometerStyle::MinimalDigital)), std::string("minimal_digital"));
    ASSERT_EQ(std::string(keybinder::speedometer::SpeedometerStyleLabel(keybinder::speedometer::SpeedometerStyle::TachometerArc)), std::string("Tachometer Arc"));
    ASSERT_EQ(std::string(keybinder::speedometer::SpeedometerStyleLabel(keybinder::speedometer::SpeedometerStyle::AnalogArc)), std::string("Analog Arc"));
    ASSERT_EQ(std::string(keybinder::speedometer::SpeedometerStyleLabel(keybinder::speedometer::SpeedometerStyle::MinimalDigital)), std::string("Minimal Tachometer"));
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("tachometer_arc") == keybinder::speedometer::SpeedometerStyle::TachometerArc);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("Tachometer Arc") == keybinder::speedometer::SpeedometerStyle::TachometerArc);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("analog_arc") == keybinder::speedometer::SpeedometerStyle::AnalogArc);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("Analog Arc") == keybinder::speedometer::SpeedometerStyle::AnalogArc);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("minimal_digital") == keybinder::speedometer::SpeedometerStyle::MinimalDigital);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("Minimal Digital") == keybinder::speedometer::SpeedometerStyle::MinimalDigital);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("Minimal Tachometer") == keybinder::speedometer::SpeedometerStyle::MinimalDigital);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("unknown") == keybinder::speedometer::SpeedometerStyle::AnalogArc);
    ASSERT_TRUE(keybinder::speedometer::ParseSpeedometerStyle("classic") == keybinder::speedometer::SpeedometerStyle::AnalogArc);
    ASSERT_EQ(keybinder::speedometer::NormalizeSpeedometerStyleId(""), std::string("analog_arc"));
    ASSERT_EQ(keybinder::speedometer::NormalizeSpeedometerStyleId("classic"), std::string("analog_arc"));
    ASSERT_EQ(keybinder::speedometer::NormalizeSpeedometerStyleId("tachometer-arc"), std::string("tachometer_arc"));
    ASSERT_EQ(keybinder::speedometer::NormalizeSpeedometerStyleId("analog-arc"), std::string("analog_arc"));
    ASSERT_EQ(keybinder::speedometer::NormalizeSpeedometerStyleId("minimal-digital"), std::string("minimal_digital"));
    ASSERT_EQ(keybinder::speedometer::NormalizeSpeedometerStyleId("minimal-tachometer"), std::string("minimal_digital"));
    return true;
}

bool TestSpeedometerFocusedNormalizesInvalidStyle() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "speedometer_style": "unknown"
})json"));

    ASSERT_EQ(settingsManager.Get().speedometer_style, std::string("analog_arc"));
    ASSERT_TRUE(settingsManager.GetSettingsJson().find("\"speedometer_style\": \"analog_arc\"") != std::string::npos);
    return true;
}

bool TestSpeedometerFocusedIgnoresLegacyTextDrawIds() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "speedometer_hide_textdraw_ids": [5, 9999, -1],
  "speedometer_fuel_textdraw_ids": [2303, 2304]
})json"));

    const std::string json = settingsManager.GetSettingsJson();
    ASSERT_TRUE(json.find("\"speedometer_hide_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_fuel_textdraw_ids\"") == std::string::npos);
    return true;
}

bool TestSpeedometerFocusedTextDrawParsing() {
    ASSERT_EQ(textdraw::RPC_SCR_SHOW_TEXTDRAW, static_cast<unsigned char>(134));
    ASSERT_EQ(textdraw::RPC_SCR_HIDE_TEXTDRAW, static_cast<unsigned char>(135));
    ASSERT_EQ(textdraw::RPC_SCR_TEXTDRAW_SET_STRING, static_cast<unsigned char>(105));
    ASSERT_EQ(keybinder::TextDrawAdapter::StripColorCodesForTesting("~g~75~w~%"), std::string("75%"));
    ASSERT_EQ(keybinder::TextDrawAdapter::StripColorCodesForTesting("{FF0000}Fuel: ~y~42~w~%"), std::string("Fuel: 42%"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("~g~75~w~%"), 75.0f);
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("{FF0000}Fuel: ~y~42~w~%"), 42.0f);
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("Fuel unavailable"), -1.0f);
    return true;
}

bool TestSpeedometerFocusedTelemetryDerivesSharedValues() {
    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = 123.4f;
    input.vehicleHealth = 750.0f;
    input.fuelValue = 42.0f;
    input.currentGear = 3;
    input.engineOn = true;

    const auto telemetry = keybinder::SpeedometerManager::BuildTelemetry(input);
    ASSERT_TRUE(telemetry.isInVehicle);
    ASSERT_NEAR(telemetry.speedKmh, 123.4f, 0.01f);
    ASSERT_NEAR(telemetry.vehicleHealthPct, 0.75f, 0.01f);
    ASSERT_NEAR(telemetry.dlPct, 0.75f, 0.01f);
    ASSERT_TRUE(telemetry.hasFuel);
    ASSERT_NEAR(telemetry.fuelPct, 0.42f, 0.01f);
    ASSERT_EQ(telemetry.currentGear, static_cast<unsigned char>(3));
    ASSERT_EQ(telemetry.displayGear, static_cast<unsigned char>(3));
    ASSERT_EQ(std::string(telemetry.gearText), std::string("3"));
    ASSERT_TRUE(telemetry.engineOn);
    ASSERT_TRUE(telemetry.estimatedRpmPct > 0.0f);
    return true;
}

bool TestSpeedometerFocusedTelemetryAccelerationUsesPreviousSample() {
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 40.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.fuelValue = 80.0f;
    previousInput.currentGear = 2;
    previousInput.engineOn = true;
    const auto previous = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input = previousInput;
    input.speedKmh = 60.0f;
    const auto telemetry = keybinder::SpeedometerManager::BuildTelemetry(input, &previous, 0.5f);

    ASSERT_NEAR(telemetry.accelerationKmhPerSecond, 40.0f, 0.01f);
    ASSERT_NEAR(telemetry.accelerationPct, 40.0f / 95.0f, 0.01f);
    ASSERT_TRUE(telemetry.estimatedRpmPct >= previous.estimatedRpmPct);
    return true;
}

bool TestSpeedometerFocusedTelemetryInvalidValuesClampSafely() {
    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = std::numeric_limits<float>::infinity();
    input.vehicleHealth = -10.0f;
    input.fuelValue = -1.0f;
    input.currentGear = 0;
    input.engineOn = false;

    const auto telemetry = keybinder::SpeedometerManager::BuildTelemetry(input);
    ASSERT_NEAR(telemetry.speedKmh, 0.0f, 0.01f);
    ASSERT_NEAR(telemetry.vehicleHealthPct, 0.0f, 0.01f);
    ASSERT_NEAR(telemetry.dlPct, 0.0f, 0.01f);
    ASSERT_TRUE(!telemetry.hasFuel);
    ASSERT_NEAR(telemetry.fuelPct, 0.0f, 0.01f);
    ASSERT_EQ(std::string(telemetry.gearText), std::string("N"));
    ASSERT_NEAR(telemetry.estimatedRpmPct, 0.0f, 0.01f);
    return true;
}

bool TestSpeedometerFocusedTelemetryBoundaryValues() {
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 120.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.fuelValue = 50.0f;
    previousInput.currentGear = 3;
    previousInput.engineOn = true;
    const auto previous = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = 999.0f;
    input.vehicleHealth = 5000.0f;
    input.fuelValue = 150.0f;
    input.currentGear = 255;
    input.engineOn = true;

    const auto telemetry = keybinder::SpeedometerManager::BuildTelemetry(input, &previous, 0.0f);
    ASSERT_NEAR(telemetry.speedKmh, 300.0f, 0.01f);
    ASSERT_NEAR(telemetry.vehicleHealthPct, 1.0f, 0.01f);
    ASSERT_NEAR(telemetry.fuelValue, 100.0f, 0.01f);
    ASSERT_NEAR(telemetry.fuelPct, 1.0f, 0.01f);
    ASSERT_EQ(telemetry.currentGear, static_cast<unsigned char>(255));
    ASSERT_EQ(telemetry.displayGear, static_cast<unsigned char>(6));
    ASSERT_EQ(std::string(telemetry.gearText), std::string("6"));
    ASSERT_NEAR(telemetry.accelerationPct, 0.0f, 0.01f);
    ASSERT_TRUE(telemetry.estimatedRpmPct <= 1.0f);

    input.engineOn = false;
    const auto movingEngineFlagOffTelemetry = keybinder::SpeedometerManager::BuildTelemetry(input, &previous, 1.0f);
    ASSERT_TRUE(movingEngineFlagOffTelemetry.estimatedRpmPct > 0.0f);

    input.speedKmh = 0.0f;
    const auto stoppedEngineOffTelemetry = keybinder::SpeedometerManager::BuildTelemetry(input, &previous, 1.0f);
    ASSERT_NEAR(stoppedEngineOffTelemetry.estimatedRpmPct, 0.0f, 0.01f);
    return true;
}

int main() {
    std::printf("=== Speedometer Focused Tests ===\n\n");
    keybinder::Logger::Initialize("test_speedometer_focused.log");
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    RUN_TEST(TestSpeedometerFocusedSettingsRoundTrip);
    RUN_TEST(TestSpeedometerFocusedStyleHelpersNormalize);
    RUN_TEST(TestSpeedometerFocusedNormalizesInvalidStyle);
    RUN_TEST(TestSpeedometerFocusedIgnoresLegacyTextDrawIds);
    RUN_TEST(TestSpeedometerFocusedTextDrawParsing);
    RUN_TEST(TestSpeedometerFocusedTelemetryDerivesSharedValues);
    RUN_TEST(TestSpeedometerFocusedTelemetryAccelerationUsesPreviousSample);
    RUN_TEST(TestSpeedometerFocusedTelemetryInvalidValuesClampSafely);
    RUN_TEST(TestSpeedometerFocusedTelemetryBoundaryValues);

    keybinder::Logger::Shutdown();
    std::remove("test_speedometer_focused.log");
    std::printf("\n=== Speedometer Focused Results: %d passed, %d failed ===\n", g_results.passed, g_results.failed);
    return g_results.failed;
}
