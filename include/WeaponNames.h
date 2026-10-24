#pragma once

#include <string>

namespace killtracker {

// Get display name for a weapon ID
// Returns "Unknown Weapon" for invalid/unknown IDs
std::string GetWeaponName(int weaponId);

// Format a kill message for chat output
// Format: "[Kill] You killed {victim} with {weapon}"
std::string FormatKillMessage(const std::string& victim, int weaponId);

// Format a kill message with explicit killer (for testing/debugging)
// Format: "[Kill] {killer} killed {victim} with {weapon}"
std::string FormatKillMessage(const std::string& killer, const std::string& victim, int weaponId);

// Check if a weapon ID is valid (0-46 range for SA-MP)
bool IsValidWeaponId(int weaponId);

} // namespace killtracker
