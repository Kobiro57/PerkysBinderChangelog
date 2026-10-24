#include "../include/Hitmarker.h"
#include "../include/PersistentPaths.h"
#include "../include/SettingsManager.h"

namespace {

keybinder::HitmarkerShape ToHitmarkerShape(int shapeValue) {
    switch (shapeValue) {
    case 0:
        return keybinder::HitmarkerShape::CrossX;
    case 1:
        return keybinder::HitmarkerShape::Plus;
    case 2:
        return keybinder::HitmarkerShape::Circle;
    case 3:
        return keybinder::HitmarkerShape::Diamond;
    case 4:
        return keybinder::HitmarkerShape::CustomPNG;
    default:
        return keybinder::HitmarkerShape::CrossX;
    }
}

bool ShouldIgnoreWeaponForMarker(int weaponId) {
    // Fists and the sniper rifle are excluded by design; weapon 54 mirrors the
    // hitsound's self/fall-damage filter ("do the rest like the hitsound").
    return weaponId == keybinder::Hitmarker::FIST_WEAPON_ID ||
           weaponId == keybinder::Hitmarker::SNIPER_WEAPON_ID ||
           weaponId == keybinder::Hitmarker::SUICIDE_WEAPON_ID;
}

} // namespace

namespace keybinder {

Hitmarker::Hitmarker()
    : presets_(GetDefaultPresets()) {}

Hitmarker::~Hitmarker() = default;

void Hitmarker::OnDamageEvent(const killtracker::DamageEvent& event) {
    if (!event.isOutgoing) {
        return;
    }

    if (ShouldIgnoreWeaponForMarker(event.weaponId)) {
        return;
    }

    std::lock_guard<std::mutex> lock(markerMutex_);
    pendingEvents_.push_back(event);
}

void Hitmarker::OnKillEvent(const killtracker::KillEvent& event) {
    if (!event.isLocalPlayer) {
        return;
    }

    if (!enabled_) {
        return;
    }

    if (ShouldIgnoreWeaponForMarker(event.weaponId)) {
        return;
    }

    if (activeMarkers_.empty()) {
        ActiveMarker marker;
        marker.preset = &presets_[activePresetIndex_];
        marker.spawnTime = std::chrono::steady_clock::now();
        marker.isKillConfirmed = true;
        marker.currentOpacity = marker.preset->opacity;
        activeMarkers_.push_back(marker);
    } else {
        activeMarkers_.back().isKillConfirmed = true;
    }
}

void Hitmarker::Update() {
    std::vector<killtracker::DamageEvent> localEvents;
    {
        std::lock_guard<std::mutex> lock(markerMutex_);
        localEvents = std::move(pendingEvents_);
        pendingEvents_.clear();
    }

    for (const auto& event : localEvents) {
        if (!enabled_) {
            continue;
        }

        if (ShouldIgnoreWeaponForMarker(event.weaponId)) {
            continue;
        }

        ActiveMarker marker;
        marker.preset = &presets_[activePresetIndex_];
        marker.spawnTime = std::chrono::steady_clock::now();
        marker.isKillConfirmed = false;
        marker.currentOpacity = marker.preset->opacity;

        if (activeMarkers_.size() >= MAX_ACTIVE_MARKERS) {
            activeMarkers_.erase(activeMarkers_.begin());
        }

        activeMarkers_.push_back(marker);
    }

    const auto now = std::chrono::steady_clock::now();
    for (auto it = activeMarkers_.begin(); it != activeMarkers_.end();) {
        const auto elapsed = std::chrono::duration<float>(now - it->spawnTime).count();
        if (elapsed > it->preset->duration) {
            it = activeMarkers_.erase(it);
        } else {
            if (it->preset->fadeOut) {
                it->currentOpacity = it->preset->opacity * (1.0f - elapsed / it->preset->duration);
            }
            ++it;
        }
    }
}

void Hitmarker::ApplySettings(const Settings& settings) {
    enabled_ = settings.hitmarker_enabled;

    if (presets_.empty()) {
        presets_ = GetDefaultPresets();
    }

    if (presets_.empty()) {
        activePresetIndex_ = 0;
        return;
    }

    const auto maxIndex = static_cast<int>(presets_.size()) - 1;
    auto clampedIndex = settings.hitmarker_active_preset_index;
    if (clampedIndex < 0) {
        clampedIndex = 0;
    } else if (clampedIndex > maxIndex) {
        clampedIndex = maxIndex;
    }
    activePresetIndex_ = static_cast<std::size_t>(clampedIndex);

    HitmarkerPreset& activePreset = presets_[activePresetIndex_];
    activePreset.shape = ToHitmarkerShape(settings.hitmarker_shape);
    activePreset.color = HitmarkerColor{
        settings.hitmarker_color_r,
        settings.hitmarker_color_g,
        settings.hitmarker_color_b,
        settings.hitmarker_color_a
    };
    activePreset.killConfirmedColor = HitmarkerColor{
        settings.hitmarker_kill_color_r,
        settings.hitmarker_kill_color_g,
        settings.hitmarker_kill_color_b,
        settings.hitmarker_kill_color_a
    };
    activePreset.size = settings.hitmarker_size;
    activePreset.opacity = settings.hitmarker_opacity;
    activePreset.duration = settings.hitmarker_duration;
    activePreset.fadeOut = settings.hitmarker_fade_out;
    activePreset.customImagePath = settings.hitmarker_custom_image_path.empty()
        ? std::string{}
        : PersistentPaths::ResolveAssetPath(settings.hitmarker_custom_image_path);
}

void Hitmarker::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

bool Hitmarker::IsEnabled() const {
    return enabled_;
}

const std::vector<HitmarkerPreset>& Hitmarker::GetPresets() const {
    return presets_;
}

const HitmarkerPreset* Hitmarker::GetPreset(std::size_t index) const {
    if (index >= presets_.size()) {
        return nullptr;
    }

    return &presets_[index];
}

void Hitmarker::SetActivePresetIndex(int index) {
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        activePresetIndex_ = static_cast<std::size_t>(index);
    }
}

std::size_t Hitmarker::GetActivePresetIndex() const {
    return activePresetIndex_;
}

const std::vector<ActiveMarker>& Hitmarker::GetActiveMarkers() const {
    return activeMarkers_;
}

void Hitmarker::ClearActiveMarkers() {
    activeMarkers_.clear();
}

std::size_t Hitmarker::GetActiveMarkerCount() const {
    return activeMarkers_.size();
}

void Hitmarker::EnqueueDamageEvent(const killtracker::DamageEvent& event) {
    OnDamageEvent(event);
}

void Hitmarker::EnqueueKillEvent(const killtracker::KillEvent& event) {
    OnKillEvent(event);
}

void Hitmarker::ProcessQueuedEvents() {
    Update();
}

std::vector<HitmarkerPreset> Hitmarker::GetDefaultPresets() {
    return {
        HitmarkerPreset{
            "Aktuell",
            HitmarkerShape::CrossX,
            HitmarkerColor{1.0f, 1.0f, 1.0f, 1.0f},
            HitmarkerColor{1.0f, 0.2f, 0.2f, 1.0f},
            20.0f,
            1.0f,
            0.5f,
            true,
            {}
        }
    };
}

} // namespace keybinder
