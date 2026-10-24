#include "../include/WeaponNames.h"
#include <unordered_map>

namespace killtracker {

namespace {

// SA-MP weapon names mapping
const std::unordered_map<int, std::string> weaponNames = {
    {0, "Fist"},
    {1, "Brass Knuckles"},
    {2, "Golf Club"},
    {3, "Nightstick"},
    {4, "Knife"},
    {5, "Baseball Bat"},
    {6, "Shovel"},
    {7, "Pool Cue"},
    {8, "Katana"},
    {9, "Chainsaw"},
    {10, "Purple Dildo"},
    {11, "Dildo"},
    {12, "Vibrator"},
    {13, "Silver Vibrator"},
    {14, "Flowers"},
    {15, "Cane"},
    {16, "Grenade"},
    {17, "Tear Gas"},
    {18, "Molotov Cocktail"},
    {19, "Vehicle"},
    {20, "Vehicle"},
    {21, "Vehicle"},
    {22, "Pistol"},
    {23, "Silenced Pistol"},
    {24, "Desert Eagle"},
    {25, "Shotgun"},
    {26, "Sawnoff Shotgun"},
    {27, "Combat Shotgun"},
    {28, "Micro SMG"},
    {29, "SMG"},
    {30, "AK-47"},
    {31, "M4"},
    {32, "Tec-9"},
    {33, "Country Rifle"},
    {34, "Sniper Rifle"},
    {35, "Rocket Launcher"},
    {36, "Heat Seeker"},
    {37, "Flamethrower"},
    {38, "Minigun"},
    {39, "Satchel Charge"},
    {40, "Detonator"},
    {41, "Spraycan"},
    {42, "Fire Extinguisher"},
    {43, "Camera"},
    {44, "Night Vision Goggles"},
    {45, "Thermal Goggles"},
    {46, "Parachute"},
    {47, "Fake Pistol"},
    {48, "Pistol Whip"},
    {49, "Vehicle"},
    {50, "Helicopter Blades"},
    {51, "Explosion"},
    {52, "Car"},
    {53, "Car"},
    {54, "Suicide"},
    {55, "Drowning"},
    {56, "Collision"},
    {57, "Splat"}
};

} // anonymous namespace

std::string GetWeaponName(int weaponId) {
    auto it = weaponNames.find(weaponId);
    if (it != weaponNames.end()) {
        return it->second;
    }
    return "Unknown Weapon";
}

std::string FormatKillMessage(const std::string& victim, int weaponId) {
    return "[Kill] You killed " + victim + " with " + GetWeaponName(weaponId);
}

std::string FormatKillMessage(const std::string& killer, const std::string& victim, int weaponId) {
    return "[Kill] " + killer + " killed " + victim + " with " + GetWeaponName(weaponId);
}

bool IsValidWeaponId(int weaponId) {
    return weaponId >= 0 && weaponId <= 57;
}

} // namespace killtracker
