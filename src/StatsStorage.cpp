#include "../include/StatsStorage.h"
#include "../include/Logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>

// Note: StatsStorage is dormant. When used in future, filepath must be supplied by caller using PersistentPaths.

namespace killtracker {

StatsStorage::StatsStorage(const std::string& filepath)
    : filepath_(filepath) {
}

StatsStorage::~StatsStorage() = default;

bool StatsStorage::Load(std::unordered_map<int, WeaponStats>& stats) const {
    stats.clear();

    if (!FileExists()) {
        // No file yet is not an error - just start with empty stats
        return true;
    }

    std::ifstream file(filepath_);
    if (!file.is_open()) {
        keybinder::Logger::Warning("[StatsStorage] Failed to open stats file: %s", filepath_.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    if (!DeserializeStats(buffer.str(), stats)) {
        keybinder::Logger::Warning("[StatsStorage] Failed to parse stats file: %s", filepath_.c_str());
        // Start with empty stats on parse failure
        stats.clear();
        return false;
    }

    keybinder::Logger::Info("[StatsStorage] Loaded stats for %zu weapons from %s", 
        stats.size(), filepath_.c_str());
    return true;
}

bool StatsStorage::Save(const std::unordered_map<int, WeaponStats>& stats) const {
    std::string json = SerializeStats(stats);

    std::ofstream file(filepath_);
    if (!file.is_open()) {
        keybinder::Logger::Error("[StatsStorage] Failed to open stats file for writing: %s", 
            filepath_.c_str());
        return false;
    }

    file << json;
    bool writeSuccess = file.good();
    file.close();
    
    if (!writeSuccess) {
        keybinder::Logger::Error("[StatsStorage] Failed to write stats to file: %s", 
            filepath_.c_str());
        return false;
    }

    keybinder::Logger::Info("[StatsStorage] Saved stats for %zu weapons to %s", 
        stats.size(), filepath_.c_str());
    return true;
}

bool StatsStorage::FileExists() const {
    std::ifstream file(filepath_);
    return file.good();
}

std::string StatsStorage::SerializeStats(const std::unordered_map<int, WeaponStats>& stats) const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": 1,\n";
    json << "  \"weapons\": [\n";

    bool first = true;
    for (const auto& pair : stats) {
        const WeaponStats& ws = pair.second;
        
        // Skip weapons with no activity
        if (ws.totalDamage <= 0.0f && ws.killCount <= 0) {
            continue;
        }

        if (!first) {
            json << ",\n";
        }
        first = false;

        json << "    {\n";
        json << "      \"weaponId\": " << ws.weaponId << ",\n";
        json << "      \"totalDamage\": " << std::fixed << std::setprecision(2) << ws.totalDamage << ",\n";
        json << "      \"killCount\": " << ws.killCount << "\n";
        json << "    }";
    }

    json << "\n  ]\n";
    json << "}\n";

    return json.str();
}

bool StatsStorage::DeserializeStats(const std::string& json, 
    std::unordered_map<int, WeaponStats>& stats) const {
    
    // Simple JSON parser for our specific format
    // Looking for pattern: "weaponId": N, "totalDamage": D, "killCount": K
    
    // Clear output stats first
    stats.clear();
    
    size_t pos = 0;
    bool foundAnyValidEntry = false;
    
    while (pos < json.length()) {
        // Find weapon object start
        size_t weaponStart = json.find("\"weaponId\"", pos);
        if (weaponStart == std::string::npos) {
            break;
        }

        WeaponStats ws;
        bool hasValidWeaponId = false;

        // Parse weaponId
        size_t idStart = json.find(":", weaponStart);
        if (idStart == std::string::npos) {
            pos = weaponStart + 1;  // Skip and continue
            continue;
        }
        idStart++;
        while (idStart < json.length() && std::isspace(static_cast<unsigned char>(json[idStart]))) {
            idStart++;
        }
        size_t idEnd = idStart;
        while (idEnd < json.length() && (std::isdigit(static_cast<unsigned char>(json[idEnd])) || json[idEnd] == '-')) {
            idEnd++;
        }
        try {
            ws.weaponId = std::stoi(json.substr(idStart, idEnd - idStart));
            hasValidWeaponId = true;
        } catch (...) {
            pos = idEnd;
            continue;  // Skip this malformed entry
        }

        // Parse totalDamage
        size_t damageStart = json.find("\"totalDamage\"", idEnd);
        if (damageStart != std::string::npos) {
            size_t valStart = json.find(":", damageStart);
            if (valStart != std::string::npos) {
                valStart++;
                while (valStart < json.length() && std::isspace(static_cast<unsigned char>(json[valStart]))) {
                    valStart++;
                }
                size_t valEnd = valStart;
                while (valEnd < json.length() && 
                       (std::isdigit(static_cast<unsigned char>(json[valEnd])) || json[valEnd] == '.' || json[valEnd] == '-')) {
                    valEnd++;
                }
                try {
                    ws.totalDamage = std::stof(json.substr(valStart, valEnd - valStart));
                } catch (...) {
                    ws.totalDamage = 0.0f;
                }
            }
        }

        // Parse killCount
        size_t killStart = json.find("\"killCount\"", idEnd);
        if (killStart != std::string::npos) {
            size_t valStart = json.find(":", killStart);
            if (valStart != std::string::npos) {
                valStart++;
                while (valStart < json.length() && std::isspace(static_cast<unsigned char>(json[valStart]))) {
                    valStart++;
                }
                size_t valEnd = valStart;
                while (valEnd < json.length() && (std::isdigit(static_cast<unsigned char>(json[valEnd])) || json[valEnd] == '-')) {
                    valEnd++;
                }
                try {
                    ws.killCount = std::stoi(json.substr(valStart, valEnd - valStart));
                } catch (...) {
                    ws.killCount = 0;
                }
            }
        }

        // Store the weapon stats if valid
        if (hasValidWeaponId && ws.weaponId >= 0) {
            stats[ws.weaponId] = ws;
            foundAnyValidEntry = true;
        }

        pos = idEnd;
    }
    
    // Return true only if we found at least one valid entry OR the JSON was empty/valid but had no entries
    // Return false for completely malformed JSON
    return foundAnyValidEntry || json.find("\"weaponId\"") == std::string::npos;
}

} // namespace killtracker
