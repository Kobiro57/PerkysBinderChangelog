#include "../include/OnlineDataManager.h"

#include "../include/Logger.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace keybinder {

namespace {

constexpr const char* kPlayersApiUrl = "https://nova-legacy.de/api/players/online";

// ---------------------------------------------------------------------------
// Case-insensitive ASCII comparison
// ---------------------------------------------------------------------------
std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool CaseInsensitiveEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Faction alias table — matched case-insensitively
// LSPD = {"lspd", "los santos police department", "police department"}
// Army = {"army", "national guard", "san andreas army"}
// Fire Department = {"fire department", "feuerwehr", "lsfd"}
// ---------------------------------------------------------------------------
struct FactionAlias {
    const char* canonicalName;
    const char* aliases[4]; // max 4 aliases per faction, terminated by nullptr
};

constexpr FactionAlias kFactionAliases[] = {
    {"LSPD",              {"lspd", "los santos police department", "police department", nullptr}},
    {"Army",              {"army", "national guard", "san andreas army", nullptr}},
    {"Fire Department",   {"fire department", "feuerwehr", "lsfd", nullptr}},
};

// Canonical faction ordering for bucket output
constexpr const char* kFactionOrder[] = {"LSPD", "Army", "Fire Department"};
constexpr int kFactionOrderCount = 3;

// Freedom Front — case-insensitive, never emitted as a gang bucket
constexpr const char* kFreedomFrontAliases[] = {
    "freedom front",
    // Any other casing is handled by ToLowerAscii comparison
};

bool IsFreedomFrontLower(const std::string& lowerGang) {
    for (const char* alias : kFreedomFrontAliases) {
        if (alias == nullptr) break;
        if (lowerGang == alias) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Case-insensitive string comparison for sorting
// ---------------------------------------------------------------------------
bool CaseInsensitiveLess(const std::string& a, const std::string& b) {
    const std::size_t minLen = (std::min)(a.size(), b.size());
    for (std::size_t i = 0; i < minLen; ++i) {
        const unsigned char ca = std::tolower(static_cast<unsigned char>(a[i]));
        const unsigned char cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca < cb) return true;
        if (ca > cb) return false;
    }
    return a.size() < b.size();
}

// Zivilisten is always first gang bucket, then ascending
bool GangBucketOrder(const OnlineBucketSummary& a, const OnlineBucketSummary& b) {
    if (a.bucketName == "Zivilisten") return true;
    if (b.bucketName == "Zivilisten") return false;
    return CaseInsensitiveLess(a.bucketName, b.bucketName);
}

bool TryExtractUpdatedAtMs(const std::string& response, int& outUpdatedAtMs) {
    outUpdatedAtMs = 0;

    const std::string key = "\"updatedAt\"";
    const std::size_t keyPos = response.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const std::size_t colonPos = response.find(':', keyPos + key.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    std::size_t valuePos = colonPos + 1;
    while (valuePos < response.size() && std::isspace(static_cast<unsigned char>(response[valuePos]))) {
        ++valuePos;
    }

    bool negative = false;
    if (valuePos < response.size() && response[valuePos] == '-') {
        negative = true;
        ++valuePos;
    }

    const std::size_t digitsStart = valuePos;
    while (valuePos < response.size() && std::isdigit(static_cast<unsigned char>(response[valuePos]))) {
        ++valuePos;
    }

    if (digitsStart == valuePos) {
        return false;
    }

    long long parsedValue = 0;
    try {
        parsedValue = std::stoll(response.substr(digitsStart, valuePos - digitsStart));
    } catch (...) {
        return false;
    }

    if (negative) {
        parsedValue = -parsedValue;
    }

    if (parsedValue < static_cast<long long>((std::numeric_limits<int>::min)())
        || parsedValue > static_cast<long long>((std::numeric_limits<int>::max)())) {
        return false;
    }

    outUpdatedAtMs = static_cast<int>(parsedValue);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// OnlineDataManager lifecycle
// ---------------------------------------------------------------------------
OnlineDataManager::OnlineDataManager() = default;

OnlineDataManager::~OnlineDataManager() {
    Shutdown();
}

void OnlineDataManager::Initialize() {
    Shutdown();

    {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        snapshot_ = OnlineSnapshot{};
        hasSuccessfulSnapshot_ = false;
        lastFetchSucceeded_ = false;
    }

    running_.store(true);
    workerThread_ = std::thread(&OnlineDataManager::WorkerLoop, this);
}

void OnlineDataManager::Shutdown() {
    running_.store(false);
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void OnlineDataManager::UpdateSettings(bool enabled, const std::string& localPlayerName) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    onlineEnabled_ = enabled;
    localPlayerName_ = localPlayerName;
}

// ---------------------------------------------------------------------------
// UpdateFromPlayers — normalize raw API data into canonical snapshot
// ---------------------------------------------------------------------------
void OnlineDataManager::UpdateFromPlayers(const std::vector<OnlinePlayerInfo>& rawPlayers,
                                          int updatedAtMs,
                                          const std::string& localPlayerName) {
    // Normalize all players
    std::vector<OnlinePlayer> normalized;
    normalized.reserve(rawPlayers.size());
    for (const auto& raw : rawPlayers) {
        normalized.push_back(NormalizePlayer(raw));
    }

    // Build buckets
    std::vector<OnlineBucketSummary> factionBuckets;
    std::vector<OnlineBucketSummary> gangBuckets;
    BuildBuckets(normalized, factionBuckets, gangBuckets);

    // Resolve local affiliation
    OnlineAffiliation localAffil = ResolveLocalAffiliation(normalized, localPlayerName);

    // Build snapshot
    OnlineSnapshot snap;
    snap.fetchStatus = OnlineFetchStatus::Live;
    snap.snapshotTime = std::chrono::steady_clock::now();
    snap.totalPlayers = static_cast<int>(normalized.size());
    snap.updatedAtMs = updatedAtMs;
    snap.factionBuckets = std::move(factionBuckets);
    snap.gangBuckets = std::move(gangBuckets);
    snap.players = std::move(normalized);
    snap.localAffiliation = std::move(localAffil);

    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_ = std::move(snap);
    hasSuccessfulSnapshot_ = true;
    lastFetchSucceeded_ = true;
}

OnlineSnapshot OnlineDataManager::GetSnapshotCopy() const {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    return snapshot_;
}

// ---------------------------------------------------------------------------
// RefreshFetchStatus — recalculate Live/Stale/Offline from wall clock
// ---------------------------------------------------------------------------
void OnlineDataManager::RefreshFetchStatus() {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_.fetchStatus = ResolveFetchStatusLocked();
}

#ifdef KEYBINDER_TESTING
void OnlineDataManager::SetSnapshotForTesting(const OnlineSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    snapshot_ = snapshot;
    hasSuccessfulSnapshot_ = (snapshot.snapshotTime != std::chrono::steady_clock::time_point{});
    lastFetchSucceeded_ = hasSuccessfulSnapshot_;
}

void OnlineDataManager::PollNowForTesting() {
    PollOnce();
}

void OnlineDataManager::SetPollIntervalForTesting(std::chrono::milliseconds interval) {
    if (interval.count() < 0) {
        interval = std::chrono::milliseconds(0);
    }

    std::lock_guard<std::mutex> lock(stateMutex_);
    pollInterval_ = interval;
}
#endif

void OnlineDataManager::WorkerLoop() {
    while (running_.load()) {
        try {
            PollOnce();
        } catch (const std::exception& ex) {
            Logger::Error("[OnlineDataManager] PollOnce exception: %s", ex.what());
            ApplyFetchFailure();
        } catch (...) {
            Logger::Error("[OnlineDataManager] PollOnce exception: unknown error");
            ApplyFetchFailure();
        }

        std::chrono::milliseconds interval(10000);
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            interval = pollInterval_;
        }

        const std::chrono::milliseconds sleepStep(100);
        std::chrono::milliseconds slept(0);
        while (running_.load() && slept < interval) {
            const std::chrono::milliseconds remaining = interval - slept;
            const std::chrono::milliseconds nextSleep = remaining < sleepStep ? remaining : sleepStep;
            std::this_thread::sleep_for(nextSleep);
            slept += nextSleep;
        }
    }
}

void OnlineDataManager::PollOnce() {
    std::string localPlayerName;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!onlineEnabled_) {
            ApplyFetchFailure();
            return;
        }
        localPlayerName = localPlayerName_;
    }

    std::string body;
    std::string lastModified;
    std::string etag;
    unsigned long statusCode = 0;
    if (!httpClient_.Fetch(kPlayersApiUrl, body, std::string(), std::string(), lastModified, etag, statusCode)) {
        Logger::Warning("[OnlineDataManager] Fetch failed for %s", kPlayersApiUrl);
        ApplyFetchFailure();
        return;
    }

    if (statusCode != 200 || body.empty()) {
        Logger::Warning("[OnlineDataManager] API returned HTTP %lu for %s", statusCode, kPlayersApiUrl);
        ApplyFetchFailure();
        return;
    }

    std::vector<OnlinePlayerInfo> players;
    std::string parseError;
    if (!OnlinePlayerInfoService::ParsePlayersResponse(body, players, &parseError)) {
        Logger::Warning("[OnlineDataManager] Failed to parse online players response: %s",
                        parseError.empty() ? "unknown parse error" : parseError.c_str());
        ApplyFetchFailure();
        return;
    }

    int updatedAtMs = 0;
    TryExtractUpdatedAtMs(body, updatedAtMs);
    UpdateFromPlayers(players, updatedAtMs, localPlayerName);
}

void OnlineDataManager::ApplyFetchFailure() {
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    lastFetchSucceeded_ = false;
    snapshot_.fetchStatus = ResolveFetchStatusLocked();
}

OnlineFetchStatus OnlineDataManager::ResolveFetchStatusLocked() const {
    if (!hasSuccessfulSnapshot_ || snapshot_.snapshotTime == std::chrono::steady_clock::time_point{}) {
        return OnlineFetchStatus::Offline;
    }

    OnlineFetchStatus status = ComputeFetchStatus(snapshot_.snapshotTime,
                                                  liveThresholdSeconds_,
                                                  offlineThresholdSeconds_);
    if (!lastFetchSucceeded_ && status != OnlineFetchStatus::Offline) {
        status = OnlineFetchStatus::Stale;
    }

    return status;
}

// ---------------------------------------------------------------------------
// NormalizeFaction — resolve raw faction string to canonical name
// ---------------------------------------------------------------------------
std::string OnlineDataManager::NormalizeFaction(const std::string& raw) {
    const std::string lower = ToLowerAscii(raw);

    for (const auto& entry : kFactionAliases) {
        for (int i = 0; entry.aliases[i] != nullptr; ++i) {
            if (lower == entry.aliases[i]) {
                return entry.canonicalName;
            }
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// IsFreedomFront — case-insensitive check
// ---------------------------------------------------------------------------
bool OnlineDataManager::IsFreedomFront(const std::string& gang) {
    return IsFreedomFrontLower(ToLowerAscii(gang));
}

// ---------------------------------------------------------------------------
// NormalizeGang — return canonical gang name, or empty if Freedom Front
// ---------------------------------------------------------------------------
std::string OnlineDataManager::NormalizeGang(const std::string& raw) {
    const std::string lower = ToLowerAscii(raw);

    // Freedom Front is suppressed from gang buckets entirely
    if (IsFreedomFrontLower(lower)) {
        return {};
    }

    // Return the raw gang name as-is (gangs are dynamic, not from alias table)
    return raw;
}

// ---------------------------------------------------------------------------
// NormalizePlayer — classify a single raw player
// ---------------------------------------------------------------------------
OnlinePlayer OnlineDataManager::NormalizePlayer(const OnlinePlayerInfo& raw) {
    OnlinePlayer result;
    result.id = raw.id;
    result.name = raw.name;
    result.level = raw.level;
    result.skinId = raw.skinId;
    result.hasTeamRank = raw.hasTeamRank;
    result.teamRank = raw.teamRank;
    result.wanted = raw.wanted;
    // Step 1: Player cannot simultaneously belong to both faction and gang.
    // Faction takes priority if both are present.
    if (raw.hasFaction) {
        const std::string canonical = NormalizeFaction(raw.faction);
        if (!canonical.empty()) {
            result.isFaction = true;
            result.affiliation = canonical;
            return result;
        }
    }

    // Step 2: Try gang
    if (raw.hasGang) {
        const std::string gangName = NormalizeGang(raw.gang);
        if (!gangName.empty()) {
            result.isGang = true;
            result.affiliation = gangName;
            return result;
        }
    }

    // Step 3: faction == null && gang == null maps to Zivilisten
    // Also reached when faction doesn't match known aliases and no gang.
    result.isGang = true;
    result.affiliation = "Zivilisten";
    return result;
}

// ---------------------------------------------------------------------------
// BuildBuckets — group normalized players into faction and gang summaries
// ---------------------------------------------------------------------------
void OnlineDataManager::BuildBuckets(const std::vector<OnlinePlayer>& players,
                                    std::vector<OnlineBucketSummary>& outFactionBuckets,
                                    std::vector<OnlineBucketSummary>& outGangBuckets) {
    outFactionBuckets.clear();
    outGangBuckets.clear();

    // Temporary maps: affiliation name → player names
    std::map<std::string, std::vector<std::string>> factionMap;
    std::map<std::string, std::vector<std::string>> gangMap;

    for (const auto& player : players) {
        if (player.isFaction) {
            factionMap[player.affiliation].push_back(player.name);
        } else if (player.isGang) {
            gangMap[player.affiliation].push_back(player.name);
        }
    }

    // Build faction buckets in canonical order: LSPD, Army, Fire Department
    for (int i = 0; i < kFactionOrderCount; ++i) {
        const std::string factionName(kFactionOrder[i]);
        auto it = factionMap.find(factionName);
        if (it != factionMap.end()) {
            OnlineBucketSummary bucket;
            bucket.bucketName = factionName;
            bucket.isFaction = true;
            bucket.playerCount = static_cast<int>(it->second.size());

            // Sort player names ascending case-insensitively
            std::sort(it->second.begin(), it->second.end(), CaseInsensitiveLess);
            bucket.playerNames = std::move(it->second);

            outFactionBuckets.push_back(std::move(bucket));
        }
    }

    // Also emit any faction entries not in the canonical order (future-proofing)
    for (auto& [name, names] : factionMap) {
        bool alreadyEmitted = false;
        for (int i = 0; i < kFactionOrderCount; ++i) {
            if (name == kFactionOrder[i]) {
                alreadyEmitted = true;
                break;
            }
        }
        if (!alreadyEmitted) {
            OnlineBucketSummary bucket;
            bucket.bucketName = name;
            bucket.isFaction = true;
            bucket.playerCount = static_cast<int>(names.size());
            std::sort(names.begin(), names.end(), CaseInsensitiveLess);
            bucket.playerNames = std::move(names);
            outFactionBuckets.push_back(std::move(bucket));
        }
    }

    // Build gang buckets: Zivilisten first, then ascending alphabetical
    for (auto& [name, names] : gangMap) {
        OnlineBucketSummary bucket;
        bucket.bucketName = name;
        bucket.isFaction = false;
        bucket.playerCount = static_cast<int>(names.size());

        // Sort player names ascending case-insensitively
        std::sort(names.begin(), names.end(), CaseInsensitiveLess);
        bucket.playerNames = std::move(names);

        outGangBuckets.push_back(std::move(bucket));
    }

    // Sort gang buckets: Zivilisten first, then ascending
    std::sort(outGangBuckets.begin(), outGangBuckets.end(), GangBucketOrder);
}

// ---------------------------------------------------------------------------
// ResolveLocalAffiliation
// ---------------------------------------------------------------------------
OnlineAffiliation OnlineDataManager::ResolveLocalAffiliation(const std::vector<OnlinePlayer>& players,
                                                             const std::string& localPlayerName) {
    OnlineAffiliation result;

    if (localPlayerName.empty()) {
        return result;
    }

    const std::string lowerNeedle = ToLowerAscii(localPlayerName);

    for (const auto& player : players) {
        if (ToLowerAscii(player.name) == lowerNeedle) {
            result.isFaction = player.isFaction;
            result.name = player.affiliation;
            result.teamRank = player.hasTeamRank ? player.teamRank : "";
            return result;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// ComputeFetchStatus
// ---------------------------------------------------------------------------
OnlineFetchStatus OnlineDataManager::ComputeFetchStatus(
    const std::chrono::steady_clock::time_point& snapshotTime,
    int liveThresholdSeconds,
    int offlineThresholdSeconds) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - snapshotTime).count();

    if (elapsed <= liveThresholdSeconds) {
        return OnlineFetchStatus::Live;
    }
    if (elapsed <= offlineThresholdSeconds) {
        return OnlineFetchStatus::Stale;
    }
    return OnlineFetchStatus::Offline;
}

} // namespace keybinder
