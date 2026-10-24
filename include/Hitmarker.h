#pragma once

#include "KillTrackerTypes.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace keybinder {

struct Settings;

enum class HitmarkerShape {
    CrossX,
    Plus,
    Circle,
    Diamond,
    CustomPNG
};

struct HitmarkerColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct HitmarkerPreset {
    std::string name;
    HitmarkerShape shape = HitmarkerShape::CrossX;
    HitmarkerColor color{};
    HitmarkerColor killConfirmedColor{};
    float size = 20.0f;
    float opacity = 1.0f;
    float duration = 0.5f;
    bool fadeOut = true;
    std::string customImagePath;
};

struct ActiveMarker {
    const HitmarkerPreset* preset = nullptr;
    std::chrono::steady_clock::time_point spawnTime{};
    bool isKillConfirmed = false;
    float currentOpacity = 1.0f;
};

class Hitmarker {
public:
    static constexpr int SNIPER_WEAPON_ID = 34;
    static constexpr int FIST_WEAPON_ID = 0;
    static constexpr int SUICIDE_WEAPON_ID = 54;
    static constexpr std::size_t MAX_ACTIVE_MARKERS = 5;

    Hitmarker();
    ~Hitmarker();

    void OnDamageEvent(const killtracker::DamageEvent& event);
    void OnKillEvent(const killtracker::KillEvent& event);
    void Update();
    void ApplySettings(const Settings& settings);

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    const std::vector<HitmarkerPreset>& GetPresets() const;
    const HitmarkerPreset* GetPreset(std::size_t index) const;
    void SetActivePresetIndex(int index);
    void SetActivePreset(std::size_t index) { SetActivePresetIndex(static_cast<int>(index)); }
    std::size_t GetActivePresetIndex() const;

    const std::vector<ActiveMarker>& GetActiveMarkers() const;
    void ClearActiveMarkers();
    std::size_t GetActiveMarkerCount() const;

    void EnqueueDamageEvent(const killtracker::DamageEvent& event);
    void EnqueueKillEvent(const killtracker::KillEvent& event);
    void ProcessQueuedEvents();

    static std::vector<HitmarkerPreset> GetDefaultPresets();

private:
    bool enabled_ = true;
    std::vector<HitmarkerPreset> presets_;
    std::size_t activePresetIndex_ = 0;
    std::vector<ActiveMarker> activeMarkers_;
    std::vector<killtracker::DamageEvent> pendingEvents_;
    mutable std::mutex markerMutex_;
};

} // namespace keybinder
