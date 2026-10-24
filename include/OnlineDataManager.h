#pragma once

#include "OnlinePlayerInfoService.h"
#include "RssHttpClient.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace keybinder {

// ---------------------------------------------------------------------------
// Fetch-status metadata
// ---------------------------------------------------------------------------
enum class OnlineFetchStatus {
    Live,    // snapshot age <= 20 s
    Stale,   // snapshot age > 20 s, or last fetch failed
    Offline, // no snapshot, or snapshot age > 60 s
};

// ---------------------------------------------------------------------------
// Normalized player domain model
// ---------------------------------------------------------------------------
struct OnlinePlayer {
    std::string id;
    std::string name;
    int level = 0;
    int skinId = 0;

    // Normalized affiliation — exactly one kind is set per player.
    // A player cannot simultaneously belong to both faction and gang.
    bool isFaction = false;   // true => factionName is a recognized faction
    bool isGang = false;      // true => gangName is a dynamic gang or Zivilisten
    std::string affiliation;  // canonical name: LSPD, Army, Fire Department, gang name, or Zivilisten

    bool hasTeamRank = false;
    std::string teamRank;
    int wanted = 0;  // raw wanted level from API (0 = none)
};

// ---------------------------------------------------------------------------
// Affiliation bucket view for main tab / quick overlay
// ---------------------------------------------------------------------------
struct OnlineBucketSummary {
    std::string bucketName;          // canonical: LSPD, Army, Fire Department, gang, Zivilisten
    bool isFaction = false;
    int playerCount = 0;
    std::vector<std::string> playerNames; // ascending, case-insensitive
};

// ---------------------------------------------------------------------------
// Local affiliation — for the local-player overlay tab
// ---------------------------------------------------------------------------
struct OnlineAffiliation {
    bool isFaction = false;
    std::string name;               // canonical affiliation name
    std::string teamRank;           // empty if no team rank
};


// ---------------------------------------------------------------------------
// Quick overlay filter state
// ---------------------------------------------------------------------------
struct OnlineOverlayFilter {
    bool showFactions = true;
    bool showGangs = true;
    bool showCivilians = true;
};

// ---------------------------------------------------------------------------
// Canonical snapshot — single source of truth for all surfaces
// ---------------------------------------------------------------------------
struct OnlineSnapshot {
    OnlineFetchStatus fetchStatus = OnlineFetchStatus::Offline;
    std::chrono::steady_clock::time_point snapshotTime{};
    int totalPlayers = 0;
    int updatedAtMs = 0;

    // Faction buckets in canonical order: LSPD, Army, Fire Department, Red County Brotherhood
    std::vector<OnlineBucketSummary> factionBuckets;

    // Gang buckets: Zivilisten first, then ascending alphabetical
    std::vector<OnlineBucketSummary> gangBuckets;

    // All normalized players for detailed views
    std::vector<OnlinePlayer> players;

    // Local-player affiliation (empty if not resolved)
    OnlineAffiliation localAffiliation;

    bool IsLive() const { return fetchStatus == OnlineFetchStatus::Live; }
    bool IsUsable() const { return fetchStatus != OnlineFetchStatus::Offline; }
};

// ---------------------------------------------------------------------------
// Manager — maintains snapshot, normalizes raw API data
// ---------------------------------------------------------------------------
class OnlineDataManager {
public:
    OnlineDataManager();
    ~OnlineDataManager();

    void Initialize();
    void Shutdown();
    void UpdateSettings(bool enabled = true, const std::string& localPlayerName = "");

    // Feed raw API players + timestamp, produce normalized snapshot
    void UpdateFromPlayers(const std::vector<OnlinePlayerInfo>& rawPlayers,
                           int updatedAtMs,
                           const std::string& localPlayerName = "");

    // Current immutable snapshot
    OnlineSnapshot GetSnapshot() const { return GetSnapshotCopy(); }
    OnlineSnapshot GetSnapshotCopy() const;

    // Force fetch-status recalculation from wall clock
    void RefreshFetchStatus();

    // Set the snapshot age thresholds (for testing override)
    void SetLiveThresholdSeconds(int seconds) { liveThresholdSeconds_ = seconds; }
    void SetOfflineThresholdSeconds(int seconds) { offlineThresholdSeconds_ = seconds; }

#ifdef KEYBINDER_TESTING
    // Direct snapshot injection for tests
    void SetSnapshotForTesting(const OnlineSnapshot& snapshot);
    void PollNowForTesting();
    void SetPollIntervalForTesting(std::chrono::milliseconds interval);
#endif

    // ----- Static normalization helpers (pure, testable) -----

    // Resolve raw faction string to canonical name.
    // Returns canonical name if matched, empty string otherwise.
    static std::string NormalizeFaction(const std::string& raw);

    // Check if a gang name is the suppressed "Freedom Front" (case-insensitive).
    static bool IsFreedomFront(const std::string& gang);

    // Resolve raw gang string.
    // Returns canonical gang name, or empty if Freedom Front.
    // Does NOT map to Zivilisten — that happens when faction+gang are both empty.
    static std::string NormalizeGang(const std::string& raw);

    // Classify a single raw player into normalized OnlinePlayer.
    static OnlinePlayer NormalizePlayer(const OnlinePlayerInfo& raw);

    // Build buckets from normalized players.
    static void BuildBuckets(const std::vector<OnlinePlayer>& players,
                            std::vector<OnlineBucketSummary>& outFactionBuckets,
                            std::vector<OnlineBucketSummary>& outGangBuckets);

    // Resolve local player's affiliation from snapshot
    static OnlineAffiliation ResolveLocalAffiliation(const std::vector<OnlinePlayer>& players,
                                                     const std::string& localPlayerName);

    // Compute fetch status from snapshot age
    static OnlineFetchStatus ComputeFetchStatus(
        const std::chrono::steady_clock::time_point& snapshotTime,
        int liveThresholdSeconds,
        int offlineThresholdSeconds);

private:
    void WorkerLoop();
    void PollOnce();
    void ApplyFetchFailure();
    OnlineFetchStatus ResolveFetchStatusLocked() const;

    OnlineSnapshot snapshot_;
    RssHttpClient httpClient_;
    std::atomic<bool> running_{false};
    std::thread workerThread_;

    mutable std::mutex snapshotMutex_;
    mutable std::mutex stateMutex_;
    bool onlineEnabled_ = true;
    bool hasSuccessfulSnapshot_ = false;
    bool lastFetchSucceeded_ = false;
    std::string localPlayerName_;
    std::chrono::milliseconds pollInterval_{10000};

    int liveThresholdSeconds_ = 20;     // <= 20 s → Live
    int offlineThresholdSeconds_ = 60;  // > 60 s → Offline
};

} // namespace keybinder
