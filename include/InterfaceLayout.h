#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace keybinder {

enum class HudElement : std::size_t {
    Health, Armour, Breath, Money, Clock, Weapon, Ammo, Wanted,
    VehicleName, ZoneName, Radar, Count
};

struct HudElementStyle {
    // Resolution-independent displacement from the native GTA position.
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    bool custom_color = false;
    std::array<float, 4> color{{1.0f, 1.0f, 1.0f, 1.0f}};
};

constexpr std::size_t kHudElementCount = static_cast<std::size_t>(HudElement::Count);
using HudLayout = std::array<HudElementStyle, kHudElementCount>;

struct HudElementDefinition {
    HudElement element;
    const char* key;
    const char* label;
};

inline constexpr std::array<HudElementDefinition, kHudElementCount> kHudElements{{
    {HudElement::Health, "health", "Leben"},
    {HudElement::Armour, "armour", "Rüstung"},
    {HudElement::Breath, "breath", "Atemluft"},
    {HudElement::Money, "money", "Geldanzeige"},
    {HudElement::Clock, "clock", "Uhrzeit"},
    {HudElement::Weapon, "weapon", "Waffenicon"},
    {HudElement::Ammo, "ammo", "Munition"},
    {HudElement::Wanted, "wanted", "Wanted-Sterne"},
    {HudElement::VehicleName, "vehicle_name", "Fahrzeugname"},
    {HudElement::ZoneName, "zone_name", "Gebietsname"},
    {HudElement::Radar, "radar", "Minimap"},
}};

inline HudElementStyle SanitizeHudElementStyle(HudElementStyle style) {
    auto finiteClamp = [](float value, float low, float high, float fallback) {
        return std::isfinite(value) ? (std::max)(low, (std::min)(value, high)) : fallback;
    };
    style.offset_x = finiteClamp(style.offset_x, -1.0f, 1.0f, 0.0f);
    style.offset_y = finiteClamp(style.offset_y, -1.0f, 1.0f, 0.0f);
    for (float& channel : style.color) channel = finiteClamp(channel, 0.0f, 1.0f, 1.0f);
    return style;
}

} // namespace keybinder
