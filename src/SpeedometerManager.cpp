#include "../include/SpeedometerManager.h"

#include "../include/SAMPBridge.h"
#include "../include/TextDrawAdapter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/CPlayerPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CVehicle.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/enums/ePedState.h"
#include "common.h"
#endif

#ifndef KEYBINDER_TESTING
extern std::atomic<int> g_shutdown_phase;
#endif

namespace keybinder {

namespace {

// Calibrated against NL/SA-MP physical speed: an Infernus that reports 221 km/h
// with the generic GTA 180x conversion should display about 198 km/h here.
constexpr float kSpeedMultiplierKmh = 161.3f;
constexpr float kMaxDisplayedSpeedKmh = 300.0f;
constexpr float kMaxVehicleHealth = 1000.0f;
constexpr float kMaxAccelerationKmhPerSecond = 95.0f;
constexpr unsigned char kMaxDisplayGear = 6;
constexpr float kFallbackGearSpeedBandsKmh[] = {0.0f, 35.0f, 70.0f, 110.0f, 155.0f, 205.0f, 260.0f};
// Tachometer range inside a single gear: 0 km/h within the gear band maps to
// kGearLowRpmPct (just above idle), redline maps to kGearHighRpmPct. Neutral
// / stationary / engine-off maps to kIdleRpmPct. The model is purely gear+speed
// driven -- real RPM follows wheel speed through the current gear ratio, so
// throttle and acceleration do not artificially boost the needle here. The UI
// layer handles temporal smoothing on top of this target.
constexpr float kIdleRpmPct = 0.12f;
constexpr float kGearLowRpmPct = 0.20f;
constexpr float kGearHighRpmPct = 0.92f;

constexpr float kMinimumMeasuredGearSpanKmh = 15.0f;
constexpr float kMovingRpmSpeedThresholdKmh = 5.0f;
constexpr float kGearBandSpeedToleranceKmh = 8.0f;
constexpr float kUncertainGearBandProgressPct = 0.35f;
constexpr float kMinimumRpmGearSpansKmh[] = {42.0f, 46.0f, 54.0f, 64.0f, 78.0f, 92.0f};

void CopyText(char* destination, std::size_t destinationSize, const std::string& source) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }

    std::snprintf(destination, destinationSize, "%s", source.c_str());
}

float ClampFinite(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        return minValue;
    }

    return (std::max)(minValue, (std::min)(value, maxValue));
}

float Clamp01(float value) {
    return ClampFinite(value, 0.0f, 1.0f);
}

unsigned char ClampDisplayGear(unsigned char gear) {
    return (std::min)(gear, kMaxDisplayGear);
}

bool HasValidGearSpeedBand(float lowerSpeedKmh, float upperSpeedKmh) {
    return std::isfinite(lowerSpeedKmh) &&
        std::isfinite(upperSpeedKmh) &&
        lowerSpeedKmh >= 0.0f &&
        upperSpeedKmh >= lowerSpeedKmh + kMinimumMeasuredGearSpanKmh;
}

bool HasValidGearStartSpeed(float speedKmh) {
    return std::isfinite(speedKmh) && speedKmh >= 0.0f;
}

float MinimumRpmGearSpanKmh(unsigned char gear) {
    if (gear == 0) {
        return kMinimumRpmGearSpansKmh[0];
    }

    const unsigned char displayGear = ClampDisplayGear(gear);
    return kMinimumRpmGearSpansKmh[displayGear - 1];
}

float FallbackGearStartSpeedKmh(unsigned char gear) {
    const unsigned char displayGear = ClampDisplayGear(gear);
    if (displayGear == 0) {
        return 0.0f;
    }

    return kFallbackGearSpeedBandsKmh[displayGear - 1];
}

float EstimateTargetRpmPct(float speedKmh, unsigned char gear, float gearMinSpeedKmh, float gearMaxSpeedKmh) {
    const unsigned char displayGear = ClampDisplayGear(gear);
    if (displayGear == 0 || speedKmh < kMovingRpmSpeedThresholdKmh) {
        // Neutral, clutched-in or essentially stationary: show engine idle.
        return kIdleRpmPct;
    }

    float lowerSpeed = kFallbackGearSpeedBandsKmh[displayGear - 1];
    float upperSpeed = kFallbackGearSpeedBandsKmh[displayGear];

    if (HasValidGearStartSpeed(gearMinSpeedKmh)) {
        lowerSpeed = gearMinSpeedKmh;
        if (HasValidGearSpeedBand(gearMinSpeedKmh, gearMaxSpeedKmh)) {
            upperSpeed = gearMaxSpeedKmh;
        }
    }

    const float minimumGearRange = MinimumRpmGearSpanKmh(displayGear);
    if (speedKmh < lowerSpeed - kGearBandSpeedToleranceKmh) {
        // Observed gear-start is further ahead than current speed (downshift or
        // stale sample): pull the lower bound back so the needle sits a little
        // above idle instead of pinning to 0 or to the old band.
        lowerSpeed = (std::max)(0.0f, speedKmh - minimumGearRange * kUncertainGearBandProgressPct);
    }

    // Guard against pathological handling data where the gear band is narrower
    // than the minimum expected sweep; ensures the gauge doesn't peg instantly.
    upperSpeed = (std::max)(upperSpeed, lowerSpeed + minimumGearRange);

    const float gearRange = upperSpeed - lowerSpeed;
    const float gearPct = Clamp01((speedKmh - lowerSpeed) / gearRange);
    return kGearLowRpmPct + gearPct * (kGearHighRpmPct - kGearLowRpmPct);
}

void FormatGearText(unsigned char gear, char* destination, std::size_t destinationSize) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }

    if (gear == 0) {
        std::snprintf(destination, destinationSize, "N");
        return;
    }

    std::snprintf(destination, destinationSize, "%u", static_cast<unsigned int>(gear));
}

#ifndef KEYBINDER_TESTING
speedometer::VehicleRuntimeState CaptureVehicleRuntimeState(SAMPBridge* bridge) {
    speedometer::VehicleRuntimeState result;
    if (bridge == nullptr || bridge->GetVersion() != SAMPBridge::Version::V03DLR1) return result;
    auto* netGame = bridge->GetNetGame();
    if (netGame == nullptr || netGame->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED) return result;
    result.connected = true;
    auto* localPlayer = bridge->GetLocalPlayer();
    ::CPed* ped = localPlayer != nullptr && localPlayer->m_pPed != nullptr
        ? localPlayer->m_pPed->m_pGamePed : nullptr;
    if (ped == nullptr || !ped->bInVehicle || ped->m_pVehicle == nullptr || ped->m_fHealth <= 0.0f) return result;
    CVehicle* vehicle = ped->m_pVehicle;
    // The seat owner is authoritative; SA-MP's passenger flag and ped animation
    // state can lag while entering, leaving or being placed into a vehicle.
    result.isDriver = vehicle->m_pDriver == ped;
    result.vehicleIdentity = reinterpret_cast<std::uintptr_t>(vehicle);
    auto& input = result.input;
    input.isInVehicle = true;
    input.speedKmh = vehicle->m_vecMoveSpeed.Magnitude() * kSpeedMultiplierKmh;
    input.vehicleHealth = vehicle->m_fHealth;
    input.currentGear = vehicle->m_nCurrentGear;
    input.engineOn = vehicle->bEngineOn;
    input.audioChangingGear = vehicle->bAudioChangingGear;
    input.hasThrottleValue = true;
    input.throttleValue = vehicle->m_fGasPedal;
    if (vehicle->m_pHandlingData != nullptr && input.currentGear > 0) {
        const cTransmission& transmission = vehicle->m_pHandlingData->m_transmissionData;
        const unsigned int gearIndex = static_cast<unsigned int>(input.currentGear - 1);
        if (gearIndex < static_cast<unsigned int>(transmission.m_nNumberOfGears) && gearIndex < kMaxDisplayGear) {
            const tTransmissionGear& gear = transmission.m_aGears[gearIndex];
            input.currentGearMinSpeedKmh = gear.m_fChangeDownVelocity * kSpeedMultiplierKmh;
            input.currentGearMaxSpeedKmh = gear.m_fMaxVelocity * kSpeedMultiplierKmh;
        }
    }
    return result;
}
#endif

} // namespace

SpeedometerManager::SpeedometerManager() = default;

SpeedometerManager::~SpeedometerManager() {
#ifndef KEYBINDER_TESTING
    if (g_shutdown_phase.load() != 2) {
        return;
    }
#endif
    if (initialized_.load()) {
        Shutdown();
    }
}

bool SpeedometerManager::Initialize(TextDrawAdapter* adapter, SAMPBridge* bridge) {
    if (adapter == nullptr || bridge == nullptr) {
        return false;
    }

    adapter_ = adapter;
    bridge_ = bridge;
    StoreClearedState();
    initialized_ = true;
    return true;
}

void SpeedometerManager::Shutdown() {
    initialized_ = false;
    StoreClearedState();

    if (adapter_ != nullptr) {
        adapter_->Shutdown();
    }
}

speedometer::SpeedometerState SpeedometerManager::MakeClearedState() {
    speedometer::SpeedometerState state;
    FormatGearText(0, state.telemetry.gearText, sizeof(state.telemetry.gearText));
    std::snprintf(state.speedText, sizeof(state.speedText), "0 km/h");
    std::snprintf(state.healthText, sizeof(state.healthText), "0%%");
    std::snprintf(state.fuelDisplayText, sizeof(state.fuelDisplayText), "");
    state.fuelValue = -1.0f;
    return state;
}

speedometer::VehicleTelemetry SpeedometerManager::BuildTelemetry(
    const speedometer::VehicleTelemetryInput& input,
    const speedometer::VehicleTelemetry* previousTelemetry,
    float deltaSeconds) {
    speedometer::VehicleTelemetry telemetry;
    telemetry.isInVehicle = input.isInVehicle;
    FormatGearText(0, telemetry.gearText, sizeof(telemetry.gearText));

    if (!input.isInVehicle) {
        return telemetry;
    }

    telemetry.speedKmh = ClampFinite(input.speedKmh, 0.0f, kMaxDisplayedSpeedKmh);
    telemetry.speedPct = Clamp01(telemetry.speedKmh / kMaxDisplayedSpeedKmh);
    telemetry.vehicleHealth = ClampFinite(input.vehicleHealth, 0.0f, kMaxVehicleHealth);
    telemetry.vehicleHealthPct = Clamp01(telemetry.vehicleHealth / kMaxVehicleHealth);
    telemetry.dlPct = telemetry.vehicleHealthPct;
    telemetry.hasFuel = std::isfinite(input.fuelValue) && input.fuelValue >= 0.0f;
    telemetry.fuelValue = telemetry.hasFuel ? ClampFinite(input.fuelValue, 0.0f, 100.0f) : -1.0f;
    telemetry.fuelPct = telemetry.hasFuel ? Clamp01(telemetry.fuelValue / 100.0f) : 0.0f;
    telemetry.currentGear = input.currentGear;
    telemetry.displayGear = ClampDisplayGear(input.currentGear);
    telemetry.engineOn = input.engineOn;
    telemetry.audioChangingGear = input.audioChangingGear;
    FormatGearText(telemetry.displayGear, telemetry.gearText, sizeof(telemetry.gearText));
    telemetry.throttlePct = input.hasThrottleValue
        ? Clamp01(std::fabs(input.throttleValue))
        : 0.0f;

    if (previousTelemetry != nullptr && previousTelemetry->isInVehicle && std::isfinite(deltaSeconds) && deltaSeconds > 0.001f) {
        telemetry.accelerationKmhPerSecond = (telemetry.speedKmh - previousTelemetry->speedKmh) / deltaSeconds;
        telemetry.accelerationPct = telemetry.accelerationKmhPerSecond > 0.0f
            ? Clamp01(telemetry.accelerationKmhPerSecond / kMaxAccelerationKmhPerSecond)
            : 0.0f;
    }

    bool resetRpmAtShift = false;
    float gearStartSpeedKmh = input.currentGearMinSpeedKmh;
    if (previousTelemetry != nullptr && previousTelemetry->isInVehicle) {
        const bool gameGearChanged = previousTelemetry->currentGear != telemetry.currentGear;
        const bool audioShiftStarted = telemetry.audioChangingGear && !previousTelemetry->audioChangingGear;

        resetRpmAtShift = gameGearChanged || audioShiftStarted;
        if (resetRpmAtShift) {
            gearStartSpeedKmh = telemetry.speedKmh;
        } else if (previousTelemetry->currentGear == telemetry.currentGear &&
            HasValidGearStartSpeed(previousTelemetry->observedGearStartSpeedKmh)) {
            gearStartSpeedKmh = previousTelemetry->observedGearStartSpeedKmh;
        }
    }

    if (!HasValidGearStartSpeed(gearStartSpeedKmh)) {
        gearStartSpeedKmh = FallbackGearStartSpeedKmh(telemetry.displayGear);
    }

    if (!resetRpmAtShift && gearStartSpeedKmh > telemetry.speedKmh + kGearBandSpeedToleranceKmh) {
        gearStartSpeedKmh = (std::max)(0.0f, telemetry.speedKmh - MinimumRpmGearSpanKmh(telemetry.displayGear) * kUncertainGearBandProgressPct);
    }

    telemetry.observedGearStartSpeedKmh = HasValidGearStartSpeed(gearStartSpeedKmh)
        ? ClampFinite(gearStartSpeedKmh, 0.0f, kMaxDisplayedSpeedKmh)
        : 0.0f;

    const bool rpmActive = telemetry.engineOn || telemetry.speedKmh >= kMovingRpmSpeedThresholdKmh;
    const float rpmGearMinSpeedKmh = telemetry.observedGearStartSpeedKmh;
    telemetry.estimatedRpmPct = rpmActive
        ? EstimateTargetRpmPct(telemetry.speedKmh, telemetry.displayGear, rpmGearMinSpeedKmh, input.currentGearMaxSpeedKmh)
        : 0.0f;
    return telemetry;
}

void SpeedometerManager::StoreState(const speedometer::SpeedometerState& state) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_ = state;
}

void SpeedometerManager::ResetTelemetrySample() {
    hasTelemetrySample_ = false;
    lastTelemetry_ = speedometer::VehicleTelemetry{};
    lastTelemetryTime_ = std::chrono::steady_clock::time_point{};
    lastVehicleIdentity_ = 0;
}

void SpeedometerManager::StoreClearedState() {
    if (adapter_ != nullptr) {
        adapter_->SetVehicleContext(false);
    }

    ResetTelemetrySample();
    StoreState(MakeClearedState());
}

void SpeedometerManager::Update() {
    if (!initialized_.load()) {
        return;
    }

    if (!enabled_.load()) {
        StoreClearedState();
        return;
    }

    if (bridge_ == nullptr || adapter_ == nullptr) {
        StoreClearedState();
        return;
    }

    speedometer::VehicleRuntimeState runtime;
#ifdef KEYBINDER_TESTING
    if (vehicleStateProvider_) runtime = vehicleStateProvider_();
#else
    runtime = CaptureVehicleRuntimeState(bridge_);
#endif
    if (!runtime.connected || !runtime.isDriver || !runtime.input.isInVehicle || runtime.vehicleIdentity == 0) {
        StoreClearedState();
        return;
    }
    if (lastVehicleIdentity_ != runtime.vehicleIdentity) {
        adapter_->SetVehicleContext(false);
        ResetTelemetrySample();
        lastVehicleIdentity_ = runtime.vehicleIdentity;
    }
    adapter_->SetVehicleContext(true);
    if (!adapter_->IsRenderSuppressionReady()) {
        StoreClearedState();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const float deltaSeconds = hasTelemetrySample_
        ? std::chrono::duration<float>(now - lastTelemetryTime_).count()
        : 0.0f;

    speedometer::VehicleTelemetryInput telemetryInput = runtime.input;
    telemetryInput.fuelValue = adapter_->GetFuelValue();

    speedometer::SpeedometerState nextState;
    nextState.isInVehicle = true;
    nextState.telemetry = BuildTelemetry(telemetryInput, hasTelemetrySample_ ? &lastTelemetry_ : nullptr, deltaSeconds);
    lastTelemetry_ = nextState.telemetry;
    lastTelemetryTime_ = now;
    hasTelemetrySample_ = true;

    nextState.speedKmh = nextState.telemetry.speedKmh;
    nextState.vehicleHealth = nextState.telemetry.vehicleHealth;
    nextState.fuelValue = nextState.telemetry.fuelValue;
    CopyText(nextState.fuelText, sizeof(nextState.fuelText), adapter_->GetFuelText());

    const int speed = static_cast<int>(std::lround(nextState.speedKmh));
    const int healthPct = static_cast<int>(std::lround((nextState.vehicleHealth / kMaxVehicleHealth) * 100.0f));
    std::snprintf(nextState.speedText, sizeof(nextState.speedText), "%d km/h", speed);
    std::snprintf(nextState.healthText, sizeof(nextState.healthText), "%d%%", (std::max)(0, (std::min)(100, healthPct)));

    if (nextState.fuelValue >= 0.0f) {
        const int fuelPct = static_cast<int>(std::lround(ClampFinite(nextState.fuelValue, 0.0f, 100.0f)));
        std::snprintf(nextState.fuelDisplayText, sizeof(nextState.fuelDisplayText), "%d%%", fuelPct);
    } else {
        nextState.fuelDisplayText[0] = '\0';
    }

    StoreState(nextState);
}

#ifdef KEYBINDER_TESTING
void SpeedometerManager::SetVehicleStateProviderForTesting(std::function<speedometer::VehicleRuntimeState()> provider) {
    vehicleStateProvider_ = std::move(provider);
}
#endif

speedometer::SpeedometerState SpeedometerManager::GetState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

void SpeedometerManager::OnSettingsChanged(bool enabled) {
    const bool effectiveEnabled = speedometer::kCustomTachoAvailable && enabled;
    enabled_ = effectiveEnabled;

    if (adapter_ != nullptr) {
        adapter_->SetSpeedometerEnabled(effectiveEnabled);
    }

    if (!effectiveEnabled) {
        StoreClearedState();
    }
}


} // namespace keybinder
