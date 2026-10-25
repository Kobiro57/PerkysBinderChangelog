#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace keybinder {

struct WeaponIconDefinition {
    int weaponId;
    const char* name;
    const char* folder;
};

// HUD inventory weapons only. Fist is already configured under Mods / Hud.
inline constexpr WeaponIconDefinition kWeaponIconCatalog[] = {
    {1, "Brass Knuckles", "Brass Knuckles"}, {2, "Golf Club", "Golf Club"},
    {3, "Nightstick", "Nightstick"}, {4, "Knife", "Knife"},
    {5, "Baseball Bat", "Baseball Bat"}, {6, "Shovel", "Shovel"},
    {7, "Pool Cue", "Pool Cue"}, {8, "Katana", "Katana"}, {9, "Chainsaw", "Chainsaw"},
    {10, "Purple Dildo", "Purple Dildo"}, {11, "Dildo", "Dildo"},
    {12, "Vibrator", "Vibrator"}, {13, "Silver Vibrator", "Silver Vibrator"},
    {14, "Flowers", "Flowers"}, {15, "Cane", "Cane"},
    {16, "Grenade", "Grenade"}, {17, "Tear Gas", "Tear Gas"}, {18, "Molotov", "Molotov"},
    {22, "Pistol", "Pistol"}, {23, "Silenced Pistol", "Silenced Pistol"},
    {24, "Desert Eagle", "Desert Eagle"}, {25, "Shotgun", "Shotgun"},
    {26, "Sawed-Off Shotgun", "Sawed-Off Shotgun"}, {27, "Combat Shotgun", "Combat Shotgun"},
    {28, "Micro SMG", "Micro SMG"}, {29, "MP5", "MP5"}, {30, "AK-47", "AK-47"},
    {31, "M4", "M4"}, {32, "Tec-9", "Tec-9"}, {33, "Rifle", "Rifle"},
    {34, "Sniper Rifle", "Sniper Rifle"}, {35, "Rocket Launcher", "Rocket Launcher"},
    {36, "Heat-Seeking Rocket Launcher", "Heat-Seeking Rocket Launcher"},
    {37, "Flamethrower", "Flamethrower"}, {38, "Minigun", "Minigun"},
    {39, "Satchel Charges", "Satchel Charges"}, {40, "Detonator", "Detonator"},
    {41, "Spraycan", "Spraycan"}, {42, "Fire Extinguisher", "Fire Extinguisher"},
    {43, "Camera", "Camera"}, {44, "Night Vision", "Night Vision"},
    {45, "Infrared Goggles", "Infrared Goggles"}, {46, "Parachute", "Parachute"},
};

inline const WeaponIconDefinition* FindWeaponIconDefinition(int weaponId) {
    for (const auto& item : kWeaponIconCatalog) if (item.weaponId == weaponId) return &item;
    return nullptr;
}

inline std::string LowerWeaponIconPath(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

// Persist exactly one PNG directly in the matching weapon's asset directory.
// Never interpret a configured filename as an arbitrary filesystem location.
inline std::string NormalizeWeaponIconPath(int weaponId, std::string path) {
    const auto* definition = FindWeaponIconDefinition(weaponId);
    if (!definition || path.empty()) return {};
    std::replace(path.begin(), path.end(), '/', '\\');
    if (path.front() == '\\' || path.find('\0') != std::string::npos) return {};
    for (unsigned char ch : path) if (ch < 32 || std::string("<>:\"|?*").find(ch) != std::string::npos) return {};
    const std::string expected = std::string("Waffenicons\\") + definition->folder + "\\";
    if (path.size() <= expected.size() || LowerWeaponIconPath(path.substr(0, expected.size())) != LowerWeaponIconPath(expected)) return {};
    const std::string filename = path.substr(expected.size());
    if (filename.find('\\') != std::string::npos || filename == "." || filename == ".."
        || filename.back() == ' ' || filename.back() == '.' || filename.size() > 240) return {};
    const auto dot = filename.find_last_of('.');
    if (dot == std::string::npos || dot == 0 || LowerWeaponIconPath(filename.substr(dot)) != ".png") return {};
    return expected + filename;
}

} // namespace keybinder
