#pragma once

// Note: StatsStorage is dormant. When used in future, filepath must be supplied by caller using PersistentPaths.

#include "KillTrackerTypes.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace killtracker {

// Handles persistence of weapon statistics
class StatsStorage {
public:
    explicit StatsStorage(const std::string& filepath);
    ~StatsStorage();

    // Load stats from file
    // Returns true on success, false on failure (file not found, corrupt, etc.)
    // On failure, stats will be empty but valid
    bool Load(std::unordered_map<int, WeaponStats>& stats) const;

    // Save stats to file
    // Returns true on success
    bool Save(const std::unordered_map<int, WeaponStats>& stats) const;

    // Get the file path
    std::string GetFilePath() const { return filepath_; }

    // Check if a stats file exists
    bool FileExists() const;

private:
    std::string filepath_;

    // JSON serialization helpers
    std::string SerializeStats(const std::unordered_map<int, WeaponStats>& stats) const;
    bool DeserializeStats(const std::string& json, std::unordered_map<int, WeaponStats>& stats) const;
};

} // namespace killtracker
