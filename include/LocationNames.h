#pragma once

namespace keybinder {

// Matches GTA:SA's eLevelName returned by CTheZones::GetLevelFromPosition.
// A city is intentionally different from the smallest navigation zone:
// e.g. level 1 + district Market -> Los Santos / Market.
inline const char* CityNameFromGtaLevel(int level) {
    switch (level) {
    case 0: return "San Andreas";
    case 1: return "Los Santos";
    case 2: return "San Fierro";
    case 3: return "Las Venturas";
    default: return "Unknown";
    }
}

} // namespace keybinder
