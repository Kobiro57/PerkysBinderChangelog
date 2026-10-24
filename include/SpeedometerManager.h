#pragma once

#include <algorithm>
#include <cctype>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class SAMPBridge;

namespace keybinder {
class TextDrawAdapter;

namespace speedometer {
inline constexpr bool kCustomTachoAvailable = true;

enum class SpeedometerStyle {
    TachometerArc = 0,
    AnalogArc = 1,
    MinimalDigital = 2,
};

namespace detail {
inline std::string NormalizeSpeedometerStyleToken(std::string value) {
    auto trimBegin = std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    auto trimEnd = std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();

    if (trimBegin >= trimEnd) {
        return {};
    }

    std::string normalized(trimBegin, trimEnd);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    for (char& ch : normalized) {
        if (ch == '-' || ch == ' ') {
            ch = '_';
        }
    }

    return normalized;
}
} // namespace detail

inline const char* SpeedometerStyleId(SpeedometerStyle style) {
    switch (style) {
    case SpeedometerStyle::TachometerArc:
        return "tachometer_arc";
    case SpeedometerStyle::MinimalDigital:
        return "minimal_digital";
    case SpeedometerStyle::AnalogArc:
    default:
        return "analog_arc";
    }
}

inline const char* SpeedometerStyleLabel(SpeedometerStyle style) {
    switch (style) {
    case SpeedometerStyle::TachometerArc:
        return "Tachometer Arc";
    case SpeedometerStyle::MinimalDigital:
        return "Minimal Tachometer";
    case SpeedometerStyle::AnalogArc:
    default:
        return "Analog Arc";
    }
}

inline SpeedometerStyle ParseSpeedometerStyle(const std::string& value) {
    const std::string normalized = detail::NormalizeSpeedometerStyleToken(value);
    if (normalized == "tachometer_arc") {
        return SpeedometerStyle::TachometerArc;
    }
    if (normalized == "minimal_digital" ||
        normalized == "minimal_tachometer" ||
        normalized == "minimal_tacho" ||
        normalized == "digital") {
        return SpeedometerStyle::MinimalDigital;
    }

    return SpeedometerStyle::AnalogArc;
}

inline std::string NormalizeSpeedometerStyleId(const std::string& value) {
    return SpeedometerStyleId(ParseSpeedometerStyle(value));
}


struct VehicleTelemetryInput {
    bool isInVehicle = false;
    float speedKmh = 0.0f;
    float vehicleHealth = 0.0f;
    float fuelValue = -1.0f;
    unsigned char currentGear = 0;
    float currentGearMinSpeedKmh = -1.0f;
    float currentGearMaxSpeedKmh = -1.0f;
    bool hasThrottleValue = false;
    float throttleValue = 0.0f;
    bool engineOn = false;
    bool audioChangingGear = false;
};

struct VehicleTelemetry {
    bool isInVehicle = false;
    float speedKmh = 0.0f;
    float speedPct = 0.0f;
    float vehicleHealth = 0.0f;
    float vehicleHealthPct = 0.0f;
    float dlPct = 0.0f;
    float fuelValue = -1.0f;
    float fuelPct = 0.0f;
    bool hasFuel = false;
    unsigned char currentGear = 0;
    unsigned char displayGear = 0;
    char gearText[8] = {};
    bool engineOn = false;
    bool audioChangingGear = false;
    float accelerationKmhPerSecond = 0.0f;
    float accelerationPct = 0.0f;
    float throttlePct = 0.0f;
    float observedGearStartSpeedKmh = 0.0f;
    float estimatedRpmPct = 0.0f;
};

struct SpeedometerState {
    bool isInVehicle = false;
    VehicleTelemetry telemetry;
    float speedKmh = 0.0f;
    float vehicleHealth = 0.0f;
    float fuelValue = -1.0f;
    char fuelText[64] = {};
    char speedText[16] = {};
    char healthText[16] = {};
    char fuelDisplayText[16] = {};
};

struct VehicleRuntimeState {
    bool connected = false;
    bool isDriver = false;
    std::uintptr_t vehicleIdentity = 0;
    VehicleTelemetryInput input;
};

} // namespace speedometer

class SpeedometerManager {
public:
    SpeedometerManager();
    ~SpeedometerManager();

    bool Initialize(TextDrawAdapter* adapter, SAMPBridge* bridge);
    void Shutdown();

    void Update();
    speedometer::SpeedometerState GetState() const;

    static speedometer::VehicleTelemetry BuildTelemetry(
        const speedometer::VehicleTelemetryInput& input,
        const speedometer::VehicleTelemetry* previousTelemetry = nullptr,
        float deltaSeconds = 0.0f);

    void OnSettingsChanged(bool enabled);

#ifdef KEYBINDER_TESTING
    void SetVehicleStateProviderForTesting(std::function<speedometer::VehicleRuntimeState()> provider);
#endif

private:
    void StoreState(const speedometer::SpeedometerState& state);
    void StoreClearedState();
    void ResetTelemetrySample();
    static speedometer::SpeedometerState MakeClearedState();

    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{true};
    TextDrawAdapter* adapter_ = nullptr;
    SAMPBridge* bridge_ = nullptr;
    speedometer::SpeedometerState state_{};
    speedometer::VehicleTelemetry lastTelemetry_{};
    std::chrono::steady_clock::time_point lastTelemetryTime_{};
    bool hasTelemetrySample_ = false;
    std::uintptr_t lastVehicleIdentity_ = 0;
#ifdef KEYBINDER_TESTING
    std::function<speedometer::VehicleRuntimeState()> vehicleStateProvider_;
#endif
    mutable std::mutex stateMutex_;
};

} // namespace keybinder
