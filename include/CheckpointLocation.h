#pragma once

#include "LocationNames.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace keybinder {

// GTA:SA 1.0 US stores navigation/map records in 0x20 bytes. The bundled
// legacy SDK's CZone has an embedded CZoneInfo and therefore the wrong stride
// and type/level offsets. Read a record returned by the native indexed getter;
// never walk the game array using sizeof(the legacy CZone).
// Layout reference: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Zone.h
struct NativeCheckpointZoneRecord {
    char infoLabel[8];
    char textLabel[8];
    std::int16_t x1, y1, z1, x2, y2, z2;
    std::int16_t infoIndex;
    std::uint8_t type, level;
};
static_assert(sizeof(NativeCheckpointZoneRecord) == 0x20);
static_assert(offsetof(NativeCheckpointZoneRecord, x1) == 0x10);
static_assert(offsetof(NativeCheckpointZoneRecord, type) == 0x1e);
static_assert(offsetof(NativeCheckpointZoneRecord, level) == 0x1f);

struct CheckpointLocationZone {
    float x1, y1, z1, x2, y2, z2;
    unsigned int type;
    unsigned int level;
    std::string name;
};

inline std::string CleanCheckpointPlace(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto text = value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
    if (text.size() > 100 || text == "Unknown" || text == "unknown") return {};
    for (unsigned char ch : text)
        if (ch < 0x20 || ch == 0x7f || ch == '{' || ch == '}' || ch == '[' || ch == ']') return {};
    return text;
}

inline std::string FormatCheckpointLocation(const std::string& cityValue, const std::string& districtValue) {
    const auto city = CleanCheckpointPlace(cityValue);
    const auto district = CleanCheckpointPlace(districtValue);
    if (district.empty()) return {}; // A city alone is not a resolved target district.
    if (city.empty()) return district;
    auto folded = [](std::string text) {
        for (char& ch : text) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        return text;
    };
    if (folded(city) == folded(district)) return city;
    return city + " " + district;
}

// Resolve the map position, not the officer's location. Checkpoints can carry
// roof/interior heights outside a district's vertical box, so their map district
// is selected by X/Y. Small navigation areas beat county/world containers.
inline const CheckpointLocationZone* SmallestCheckpointZone(float x, float y, float z,
    const std::vector<CheckpointLocationZone>& zones, bool mapZones) {
    const CheckpointLocationZone* result = nullptr;
    double bestArea = std::numeric_limits<double>::infinity();
    float bestHeightDistance = std::numeric_limits<float>::infinity();
    for (const auto& zone : zones) {
        if ((mapZones ? zone.type != 3 : zone.type > 1) || CleanCheckpointPlace(zone.name).empty()) continue;
        if (!std::isfinite(zone.x1) || !std::isfinite(zone.x2) || !std::isfinite(zone.y1)
            || !std::isfinite(zone.y2) || !std::isfinite(zone.z1) || !std::isfinite(zone.z2)
            || zone.x1 >= zone.x2 || zone.y1 >= zone.y2 || zone.z1 > zone.z2
            || x < zone.x1 || x > zone.x2 || y < zone.y1 || y > zone.y2) continue;
        const double area = (static_cast<double>(zone.x2) - zone.x1) * (static_cast<double>(zone.y2) - zone.y1);
        const float heightDistance = z < zone.z1 ? zone.z1 - z : z > zone.z2 ? z - zone.z2 : 0.0f;
        if (area < bestArea || (area == bestArea && heightDistance < bestHeightDistance)) {
            result = &zone;
            bestArea = area;
            bestHeightDistance = heightDistance;
        }
    }
    return result;
}

inline std::string ResolveCheckpointLocation(float x, float y, float z,
    const std::vector<CheckpointLocationZone>& navigation, const std::vector<CheckpointLocationZone>& map) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
        || std::abs(x) > 20000.0f || std::abs(y) > 20000.0f || std::abs(z) > 20000.0f) return {};
    const auto* district = SmallestCheckpointZone(x, y, z, navigation, false);
    if (!district) return {};
    const auto* region = SmallestCheckpointZone(x, y, z, map, true);
    // Map zones supply counties as well as cities. Navigation level is a useful
    // city fallback; level zero must not invent a city called "San Andreas".
    const std::string city = district->level >= 1 && district->level <= 3
        ? CityNameFromGtaLevel(district->level) : region ? region->name : "";
    return FormatCheckpointLocation(city, district->name);
}

} // namespace keybinder
