/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Kill Tracker UI - Statistics display with settings tab
*/
#pragma once

#include <string>
#include <vector>
#include <functional>
#include "KillTrackerTypes.h"

// Forward declarations
class SAMPBridge;

struct ImVec4;

namespace keybinder {

// Kill tracker settings
struct KillTrackerSettings {
    bool enabled = false;
    bool countKills = true;
    bool countDeaths = true;
    bool showLocalKillMessage = true;
    bool showLocalDeathMessage = true;
    bool sendServerKillMessage = false;
    bool sendServerDeathMessage = false;
    bool trackDamage = true;
    std::string customLocalKillMessage = "{FF6600}[Kill] {FFFFFF}{victim} {AAAAAA}mit {FFCC00}{weapon}";
    std::string customLocalDeathMessage = "{FF3333}[Tod] {FFFFFF}{killer} {AAAAAA}mit {FFCC00}{weapon}";
    std::string customServerKillMessage = "Ich habe {victim} mit {weapon} getoetet.";
    std::string customServerDeathMessage = "Ich wurde von {killer} mit {weapon} getoetet.";

    // Manual stat editing
    bool allowManualEdits = false;
};

class KillTrackerUI {
public:
    KillTrackerUI();
    ~KillTrackerUI() = default;

    // Render the main view
    void Render();
    
    // Set bridge for data access
    void SetSAMPBridge(SAMPBridge* bridge) { bridge_ = bridge; }
    
    // Callback when settings change
    std::function<void()> OnSettingsChanged;
    
    // Get current settings
    const KillTrackerSettings& GetSettings() const { return settings_; }
    void SetSettings(const KillTrackerSettings& settings);

private:
    SAMPBridge* bridge_ = nullptr;
    KillTrackerSettings settings_;
    
    // UI state
    int selectedTab_ = 0; // 0 = Stats, 1 = History, 2 = Settings
    bool showMessageVariables_ = false;
    char customLocalKillMessageBuf_[512] = {0};
    char customLocalDeathMessageBuf_[512] = {0};
    char customServerKillMessageBuf_[512] = {0};
    char customServerDeathMessageBuf_[512] = {0};
    int selectedWeaponForEdit_ = -1;
    float editDamageBuf_ = 0.0f;
    int editKillsBuf_ = 0;
    
    // PERF: Cached stats to avoid redundant computations per frame
    mutable std::vector<killtracker::WeaponStats> cachedStats_;
    mutable float cachedTotalDamage_ = 0.0f;
    mutable int cachedTotalKills_ = 0;
    
    // Tab rendering
    void RenderStatsTab();
    void RenderHistoryTab();
    void RenderSettingsTab();

    // Stats display helpers
    void RenderSummaryCards();
    void RenderWeaponStatsTable();
    void RenderWeaponStatCard(const killtracker::WeaponStats& stats, int index, float totalDamage, float cardWidth);

    // History display helpers
    void RenderKillHistoryTable();
    void RenderKillHistoryRow(const killtracker::KillEvent& kill, int index);
    void RenderDeathHistoryTable();

    // Settings helpers
    void RenderGeneralSettings();
    void RenderMessageSettings();
    void RenderDataManagement();
    
    // Utility
    std::string FormatDamage(float damage) const;
    std::string FormatPercentage(float value, float total) const;
    std::string GetWeaponIcon(int weaponId) const;
    ImVec4 GetWeaponColor(int weaponId) const;
    
    // Data retrieval
    std::vector<killtracker::WeaponStats> GetSortedWeaponStats() const;
    float GetTotalDamage() const;
    int GetTotalKills() const;
    int GetTotalWeapons() const;
};

} // namespace keybinder
