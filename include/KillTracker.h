#pragma once

#include "KillTrackerTypes.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>

namespace killtracker {

// Main kill tracker class - pure domain logic
class KillTracker {
public:
    KillTracker();
    ~KillTracker();

    // Process a kill event (returns true if it was a new, valid kill)
    bool ProcessKillEvent(const KillEvent& event);
    bool CompleteServerStatsKill(const KillEvent& event);
    // Enrich an already counted observation without incrementing totals or
    // producing a second bind. Unknown weapons never create weapon statistics.
    bool EnrichKillObservation(std::uint64_t observationId, const KillEvent& event);

    // Process a damage event (accumulates into weapon stats)
    void ProcessDamageEvent(const DamageEvent& event);

    // Get stats for a specific weapon
    const WeaponStats* GetWeaponStats(int weaponId) const;

    // Get all weapon stats
    std::vector<WeaponStats> GetAllWeaponStats() const;

    // Get total damage across all weapons
    float GetTotalDamage() const;

    // Get total kill count (includes imported historical data)
    int GetTotalKills() const;

    // Get session-only kill/death counts (reset to 0 on each load, never persisted)
    int GetSessionKills() const { return sessionKills_; }
    int GetSessionDeaths() const { return sessionDeaths_; }

    // Record a player death (local player was killed)
    // killer may be empty if unknown; weaponId -1 if unknown
    void RecordDeath(const std::string& killer, int weaponId);

    // Get total death count
    int GetTotalDeaths() const;

    // Get recent deaths (most-recent last, max 50)
    std::vector<KillEvent> GetRecentDeaths() const { return recentDeaths_; }

    // Import/export recent deaths for persistence
    void ImportRecentDeaths(const std::vector<KillEvent>& deaths);
    std::vector<KillEvent> ExportRecentDeaths() const;
    void ImportKillCount(int count) { kills_ = count; }
    void ImportDeathCount(int count) { deaths_ = count; }

    // Enable/disable tracking
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void SetCountKills(bool enabled) { countKills_ = enabled; }
    void SetCountDeaths(bool enabled) { countDeaths_ = enabled; }
    bool IsCountingDeaths() const { return enabled_ && countDeaths_; }

    // Check if a kill event is a duplicate
    bool IsDuplicateKill(const KillEvent& event) const;

    // Reset all stats
    void ResetStats();
    
    // Import weapon stats from storage (used on load)
    void ImportWeaponStats(const std::unordered_map<int, WeaponStats>& stats);

    // Export weapon stats for saving
    std::unordered_map<int, WeaponStats> ExportWeaponStats() const;

    // Import recent kills from storage (used on load)
    void ImportRecentKills(const std::vector<KillEvent>& kills);

    // Export recent kills for saving
    std::vector<KillEvent> ExportRecentKills() const;

    // Death window deduplication
    void UpdateDeathWindowState(const std::vector<DeathWindowEntry>& entries);
    bool IsNewDeathWindowEntry(const DeathWindowEntry& entry) const;

    // Get recent kills (for kill history UI)
    std::vector<KillEvent> GetRecentKills() const { return recentKills_; }

    // Record damage dealt to a victim (accumulates until victim dies)
    void RecordDamageToVictim(const std::string& victim, int weaponId, float damage);

#ifdef KEYBINDER_TESTING
    void MarkVictimDamageStaleForTesting(const std::string& victim);
#endif

private:
    bool enabled_;
    bool countKills_ = true;
    bool countDeaths_ = true;
    int kills_;
    int deaths_;
    int sessionKills_;
    int sessionDeaths_;
    std::unordered_map<int, WeaponStats> weaponStats_;
    std::vector<KillEvent> recentKills_;
    std::vector<KillEvent> recentDeaths_;
    std::vector<DeathWindowEntry> lastDeathWindowState_;

    // Track damage per victim (victim name -> total damage + last update timestamp)
    // This is accessed from both game thread (ProcessKillEvent) and RPC thread (RecordDamageToVictim)
    struct VictimDamageEntry {
        float totalDamage = 0.0f;
        std::time_t lastUpdate = 0;
    };
    std::unordered_map<std::string, VictimDamageEntry> victimDamage_;
    mutable std::shared_mutex victimDamageMutex_;

    static constexpr size_t MAX_RECENT_KILLS = 50;
    static constexpr size_t MAX_RECENT_DEATHS = 50;
    static constexpr int DUPLICATE_TIME_WINDOW_SECONDS = 5;
    static constexpr int VICTIM_DAMAGE_TTL_SECONDS = 60;  // Stale damage entries cleaned after 60s
    static constexpr int VICTIM_DAMAGE_CLEANUP_INTERVAL_SECONDS = 10;
    void AddWeaponDamage(int weaponId, float damage);
    void IncrementWeaponKill(int weaponId);
    void TrimRecentKills();
    void TrimRecentDeaths();
    void CleanupStaleVictimDamage();  // Removes entries older than TTL
    void CleanupStaleVictimDamageLocked(std::time_t now);

    std::time_t lastVictimDamageCleanup_ = 0;
};

} // namespace killtracker
