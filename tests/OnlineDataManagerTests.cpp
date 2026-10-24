#define KEYBINDER_TESTING

#include "../include/OnlineDataManager.h"
#include "../include/RssHttpClient.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace keybinder;

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

struct MockFetchResponse {
    bool success = true;
    std::string body;
    unsigned long statusCode = 200;
    int delayMs = 0;
};

struct MockFetchState {
    std::mutex mutex;
    std::vector<MockFetchResponse> responses;
    std::atomic<int> callCount{0};
    std::atomic<int> inFlight{0};
    std::atomic<int> maxConcurrent{0};
};

TestState g_state;
MockFetchState g_fetchState;

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        g_state.error = "Assertion failed: " #condition; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define ASSERT_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        g_state.error = "Assertion failed: " #actual " == " #expected; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

bool MockFetch(const std::string& url,
               std::string& outBody,
               const std::string& ifModifiedSince,
               const std::string& ifNoneMatch,
               std::string& outLastModified,
               std::string& outEtag,
               unsigned long& outStatusCode) {
    (void)ifModifiedSince;
    (void)ifNoneMatch;
    (void)outLastModified;
    (void)outEtag;

    const std::string expectedUrl = "https://nova-legacy.de/api/players/online";
    if (url != expectedUrl) {
        return false;
    }

    const int currentInFlight = ++g_fetchState.inFlight;
    int observedMax = g_fetchState.maxConcurrent.load();
    while (currentInFlight > observedMax
           && !g_fetchState.maxConcurrent.compare_exchange_weak(observedMax, currentInFlight)) {
    }

    MockFetchResponse response;
    {
        std::lock_guard<std::mutex> lock(g_fetchState.mutex);
        if (!g_fetchState.responses.empty()) {
            response = g_fetchState.responses.front();
            if (g_fetchState.responses.size() > 1) {
                g_fetchState.responses.erase(g_fetchState.responses.begin());
            }
        }
    }

    ++g_fetchState.callCount;
    if (response.delayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(response.delayMs));
    }

    outBody = response.body;
    outStatusCode = response.statusCode;

    --g_fetchState.inFlight;
    return response.success;
}

struct FetchHookScope {
    FetchHookScope() {
        {
            std::lock_guard<std::mutex> lock(g_fetchState.mutex);
            g_fetchState.responses.clear();
        }
        g_fetchState.callCount.store(0);
        g_fetchState.inFlight.store(0);
        g_fetchState.maxConcurrent.store(0);
        RssHttpClient::SetTestFetchHandler(&MockFetch);
    }

    ~FetchHookScope() {
        RssHttpClient::ResetTestFetchHandler();
        std::lock_guard<std::mutex> lock(g_fetchState.mutex);
        g_fetchState.responses.clear();
    }
};

void SetResponses(const std::vector<MockFetchResponse>& responses) {
    std::lock_guard<std::mutex> lock(g_fetchState.mutex);
    g_fetchState.responses = responses;
}

template <typename Predicate>
bool WaitUntil(Predicate predicate, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

OnlineSnapshot BuildSnapshotSecondsAgo(int secondsAgo) {
    OnlineSnapshot snapshot;
    snapshot.snapshotTime = std::chrono::steady_clock::now() - std::chrono::seconds(secondsAgo);
    snapshot.totalPlayers = 1;
    snapshot.updatedAtMs = 123;

    OnlinePlayer player;
    player.id = "1";
    player.name = "Finn";
    player.isGang = true;
    player.affiliation = "Zivilisten";
    snapshot.players.push_back(player);
    return snapshot;
}

OnlinePlayerInfo BuildRawPlayer(const std::string& id,
                                const std::string& name,
                                const std::string& faction = std::string(),
                                const std::string& gang = std::string(),
                                const std::string& teamRank = std::string()) {
    OnlinePlayerInfo player;
    player.id = id;
    player.name = name;
    player.level = 10;
    player.skinId = 7;

    if (!faction.empty()) {
        player.hasFaction = true;
        player.faction = faction;
    }

    if (!gang.empty()) {
        player.hasGang = true;
        player.gang = gang;
    }

    if (!teamRank.empty()) {
        player.hasTeamRank = true;
        player.teamRank = teamRank;
    }

    return player;
}

bool TestNormalizeFactionMatchesAliasesCaseInsensitively() {
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("lspd"), std::string("LSPD"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("Los Santos Police Department"), std::string("LSPD"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("Police Department"), std::string("LSPD"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("NATIONAL GUARD"), std::string("Army"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("San Andreas Army"), std::string("Army"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("Fire Department"), std::string("Fire Department"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("Feuerwehr"), std::string("Fire Department"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("RCB"), std::string("Red County Brotherhood"));
    ASSERT_EQ(OnlineDataManager::NormalizeFaction("rEd CoUnTy BrOtHeRhOoD"), std::string("Red County Brotherhood"));
    ASSERT_TRUE(OnlineDataManager::NormalizeFaction("Taxi") .empty());
    return true;
}

bool TestNormalizeGangSuppressesFreedomFrontOnly() {
    ASSERT_TRUE(OnlineDataManager::IsFreedomFront("Freedom Front"));
    ASSERT_TRUE(OnlineDataManager::IsFreedomFront("freedom front"));
    ASSERT_TRUE(OnlineDataManager::NormalizeGang("Freedom Front").empty());
    ASSERT_EQ(OnlineDataManager::NormalizeGang("Ballas"), std::string("Ballas"));
    return true;
}

bool TestNormalizePlayerAppliesFactionPriorityGangFallbackAndCivilians() {
    OnlinePlayerInfo factionPriority = BuildRawPlayer("1", "Finn", "LSPD", "Ballas", "Officer");
    OnlinePlayer normalizedFaction = OnlineDataManager::NormalizePlayer(factionPriority);
    ASSERT_TRUE(normalizedFaction.isFaction);
    ASSERT_TRUE(!normalizedFaction.isGang);
    ASSERT_EQ(normalizedFaction.affiliation, std::string("LSPD"));
    ASSERT_EQ(normalizedFaction.teamRank, std::string("Officer"));

    OnlinePlayerInfo gangFallback = BuildRawPlayer("2", "Berta", "Unknown Faction", "Grove");
    OnlinePlayer normalizedGang = OnlineDataManager::NormalizePlayer(gangFallback);
    ASSERT_TRUE(!normalizedGang.isFaction);
    ASSERT_TRUE(normalizedGang.isGang);
    ASSERT_EQ(normalizedGang.affiliation, std::string("Grove"));

    OnlinePlayerInfo civilians = BuildRawPlayer("3", "Anton", "Unknown Faction");
    OnlinePlayer normalizedCivilian = OnlineDataManager::NormalizePlayer(civilians);
    ASSERT_TRUE(normalizedCivilian.isGang);
    ASSERT_EQ(normalizedCivilian.affiliation, std::string("Zivilisten"));

    OnlinePlayerInfo suppressedGang = BuildRawPlayer("4", "Dora", std::string(), "Freedom Front");
    OnlinePlayer normalizedSuppressedGang = OnlineDataManager::NormalizePlayer(suppressedGang);
    ASSERT_TRUE(normalizedSuppressedGang.isGang);
    ASSERT_EQ(normalizedSuppressedGang.affiliation, std::string("Zivilisten"));
    return true;
}

bool TestBuildBucketsUsesCanonicalOrderingAndSortedNames() {
    std::vector<OnlinePlayer> players;
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("1", "bravo", "LSPD")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("2", "alpha", "LSPD")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("3", "Charlie", "San Andreas Army")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("4", "delta", "Feuerwehr")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("5", "zulu", std::string(), "Grove")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("6", "Echo", std::string(), "Ballas")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("7", "foxtrot")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("8", "Brotherhood", "RCB")));

    std::vector<OnlineBucketSummary> factionBuckets;
    std::vector<OnlineBucketSummary> gangBuckets;
    OnlineDataManager::BuildBuckets(players, factionBuckets, gangBuckets);

    ASSERT_EQ(factionBuckets.size(), static_cast<std::size_t>(4));
    ASSERT_EQ(factionBuckets[0].bucketName, std::string("LSPD"));
    ASSERT_EQ(factionBuckets[1].bucketName, std::string("Army"));
    ASSERT_EQ(factionBuckets[2].bucketName, std::string("Fire Department"));
    ASSERT_EQ(factionBuckets[3].bucketName, std::string("Red County Brotherhood"));
    ASSERT_EQ(factionBuckets[3].playerNames[0], std::string("Brotherhood"));
    ASSERT_EQ(factionBuckets[0].playerNames.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(factionBuckets[0].playerNames[0], std::string("alpha"));
    ASSERT_EQ(factionBuckets[0].playerNames[1], std::string("bravo"));

    ASSERT_EQ(gangBuckets.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(gangBuckets[0].bucketName, std::string("Zivilisten"));
    ASSERT_EQ(gangBuckets[1].bucketName, std::string("Ballas"));
    ASSERT_EQ(gangBuckets[2].bucketName, std::string("Grove"));
    ASSERT_EQ(gangBuckets[0].playerNames[0], std::string("foxtrot"));
    ASSERT_EQ(gangBuckets[1].playerNames[0], std::string("Echo"));
    return true;
}

bool TestResolveLocalAffiliationMatchesCaseInsensitiveName() {
    std::vector<OnlinePlayer> players;
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("1", "Finn", "LSPD", std::string(), "Officer")));
    players.push_back(OnlineDataManager::NormalizePlayer(BuildRawPlayer("2", "Berta", std::string(), "Ballas")));

    const OnlineAffiliation affiliation = OnlineDataManager::ResolveLocalAffiliation(players, "fInn");
    ASSERT_TRUE(affiliation.isFaction);
    ASSERT_EQ(affiliation.name, std::string("LSPD"));
    ASSERT_EQ(affiliation.teamRank, std::string("Officer"));
    return true;
}

bool TestComputeFetchStatusRespectsThresholdBoundaries() {
    ASSERT_EQ(
        OnlineDataManager::ComputeFetchStatus(
            std::chrono::steady_clock::now() - std::chrono::seconds(20),
            20,
            60),
        OnlineFetchStatus::Live);
    ASSERT_EQ(
        OnlineDataManager::ComputeFetchStatus(
            std::chrono::steady_clock::now() - std::chrono::seconds(21),
            20,
            60),
        OnlineFetchStatus::Stale);
    ASSERT_EQ(
        OnlineDataManager::ComputeFetchStatus(
            std::chrono::steady_clock::now() - std::chrono::seconds(60),
            20,
            60),
        OnlineFetchStatus::Stale);
    ASSERT_EQ(
        OnlineDataManager::ComputeFetchStatus(
            std::chrono::steady_clock::now() - std::chrono::seconds(61),
            20,
            60),
        OnlineFetchStatus::Offline);
    return true;
}

bool TestUpdateFromPlayersBuildsCanonicalSnapshot() {
    std::vector<OnlinePlayerInfo> rawPlayers;
    rawPlayers.push_back(BuildRawPlayer("1", "Finn", "Los Santos Police Department", std::string(), "Officer"));
    rawPlayers.push_back(BuildRawPlayer("2", "Anton", std::string(), "Grove"));
    rawPlayers.push_back(BuildRawPlayer("3", "Berta"));

    OnlineDataManager manager;
    manager.UpdateFromPlayers(rawPlayers, 777, "FINN");
    const OnlineSnapshot snapshot = manager.GetSnapshotCopy();

    ASSERT_EQ(snapshot.fetchStatus, OnlineFetchStatus::Live);
    ASSERT_EQ(snapshot.totalPlayers, 3);
    ASSERT_EQ(snapshot.updatedAtMs, 777);
    ASSERT_EQ(snapshot.factionBuckets.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(snapshot.factionBuckets[0].bucketName, std::string("LSPD"));
    ASSERT_EQ(snapshot.gangBuckets.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(snapshot.gangBuckets[0].bucketName, std::string("Zivilisten"));
    ASSERT_EQ(snapshot.gangBuckets[1].bucketName, std::string("Grove"));
    ASSERT_EQ(snapshot.localAffiliation.name, std::string("LSPD"));
    ASSERT_EQ(snapshot.localAffiliation.teamRank, std::string("Officer"));
    return true;
}

bool TestPollingWorkerStoresLiveSnapshot() {
    FetchHookScope scope;
    SetResponses({
        {true, R"({"updatedAt":12345,"count":3,"players":[{"id":"1","name":"Finn","level":10,"faction":"LSPD","gang":null,"skinId":7,"teamRank":"Officer"},{"id":"2","name":"Berta","level":12,"faction":null,"gang":"Ballas","skinId":9,"teamRank":null},{"id":"3","name":"anton","level":3,"faction":null,"gang":null,"skinId":5,"teamRank":null}]})", 200, 0}
    });

    OnlineDataManager manager;
    manager.UpdateSettings(true, "Finn");
    manager.SetPollIntervalForTesting(std::chrono::milliseconds(25));
    manager.Initialize();

    const bool gotSnapshot = WaitUntil([&manager]() {
        return manager.GetSnapshotCopy().totalPlayers == 3;
    }, 500);

    manager.Shutdown();

    ASSERT_TRUE(gotSnapshot);
    const OnlineSnapshot snapshot = manager.GetSnapshotCopy();
    ASSERT_EQ(snapshot.fetchStatus, OnlineFetchStatus::Live);
    ASSERT_EQ(snapshot.updatedAtMs, 12345);
    ASSERT_EQ(snapshot.totalPlayers, 3);
    ASSERT_EQ(snapshot.factionBuckets.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(snapshot.factionBuckets[0].bucketName, std::string("LSPD"));
    ASSERT_EQ(snapshot.gangBuckets.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(snapshot.gangBuckets[0].bucketName, std::string("Zivilisten"));
    ASSERT_EQ(snapshot.gangBuckets[1].bucketName, std::string("Ballas"));
    ASSERT_EQ(snapshot.localAffiliation.name, std::string("LSPD"));
    ASSERT_EQ(snapshot.localAffiliation.teamRank, std::string("Officer"));
    ASSERT_TRUE(g_fetchState.callCount.load() >= 1);
    return true;
}

bool TestPollingAcceptsLiveApiNullPlayerFields() {
    FetchHookScope scope;
    SetResponses({
        {true, R"({"updatedAt":1779557205032,"count":3,"players":[{"id":"65bbcd91-7f83-4f80-ab9b-d746c6158951","name":null,"level":2,"faction":"Police Department","gang":null,"skinId":28,"teamRank":null},{"id":"980edf6c-e2a2-43c4-8495-7db77c0401a9","name":"Yoshi","level":1,"faction":null,"gang":null,"skinId":null,"teamRank":null},{"id":"241491b8-2b2a-44da-90d4-4a7e705fc120","name":"Welf","level":4,"faction":"Police Department","gang":null,"skinId":31,"teamRank":null}]})", 200, 0}
    });

    OnlineDataManager manager;
    manager.UpdateSettings(true, "Yoshi");
    manager.PollNowForTesting();
    const OnlineSnapshot snapshot = manager.GetSnapshotCopy();

    ASSERT_EQ(snapshot.fetchStatus, OnlineFetchStatus::Live);
    ASSERT_EQ(snapshot.totalPlayers, 2);
    ASSERT_EQ(snapshot.players.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(snapshot.players[0].name, std::string("Yoshi"));
    ASSERT_EQ(snapshot.players[0].skinId, 0);
    ASSERT_EQ(snapshot.localAffiliation.name, std::string("Zivilisten"));
    ASSERT_EQ(snapshot.factionBuckets.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(snapshot.factionBuckets[0].bucketName, std::string("LSPD"));
    ASSERT_EQ(snapshot.gangBuckets.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(snapshot.gangBuckets[0].bucketName, std::string("Zivilisten"));
    return true;
}

bool TestInitializeRestartKeepsSingleWorker() {
    FetchHookScope scope;
    SetResponses({
        {true, R"({"updatedAt":1,"count":1,"players":[{"id":"1","name":"Finn","level":1,"faction":null,"gang":null,"skinId":47,"teamRank":null}]})", 200, 60}
    });

    OnlineDataManager manager;
    manager.SetPollIntervalForTesting(std::chrono::milliseconds(0));
    manager.Initialize();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    manager.Initialize();

    const bool fetched = WaitUntil([&manager]() {
        return manager.GetSnapshotCopy().totalPlayers == 1;
    }, 500);

    manager.Shutdown();

    ASSERT_TRUE(fetched);
    ASSERT_EQ(g_fetchState.maxConcurrent.load(), 1);
    return true;
}

bool TestMalformedJsonMarksOfflineWithoutSnapshot() {
    FetchHookScope scope;
    SetResponses({
        {true, "{not-json", 200, 0}
    });

    OnlineDataManager manager;
    manager.PollNowForTesting();
    const OnlineSnapshot snapshot = manager.GetSnapshotCopy();

    ASSERT_EQ(snapshot.fetchStatus, OnlineFetchStatus::Offline);
    ASSERT_EQ(snapshot.totalPlayers, 0);
    ASSERT_TRUE(snapshot.players.empty());
    return true;
}

bool TestEmptyPayloadMarksOfflineWithoutSnapshot() {
    FetchHookScope scope;
    SetResponses({
        {true, "", 200, 0}
    });

    OnlineDataManager manager;
    manager.PollNowForTesting();
    const OnlineSnapshot snapshot = manager.GetSnapshotCopy();

    ASSERT_EQ(snapshot.fetchStatus, OnlineFetchStatus::Offline);
    ASSERT_EQ(snapshot.totalPlayers, 0);
    return true;
}

bool TestHttpFailureKeepsLastGoodSnapshotAndMarksStale() {
    FetchHookScope scope;
    SetResponses({
        {true, R"({"updatedAt":11,"count":1,"players":[{"id":"1","name":"Finn","level":1,"faction":null,"gang":null,"skinId":47,"teamRank":null}]})", 200, 0},
        {false, "", 500, 0}
    });

    OnlineDataManager manager;
    manager.SetLiveThresholdSeconds(120);
    manager.SetOfflineThresholdSeconds(240);
    manager.PollNowForTesting();
    const OnlineSnapshot firstSnapshot = manager.GetSnapshotCopy();
    manager.PollNowForTesting();
    const OnlineSnapshot secondSnapshot = manager.GetSnapshotCopy();

    ASSERT_EQ(firstSnapshot.fetchStatus, OnlineFetchStatus::Live);
    ASSERT_EQ(secondSnapshot.fetchStatus, OnlineFetchStatus::Stale);
    ASSERT_EQ(secondSnapshot.totalPlayers, 1);
    ASSERT_EQ(secondSnapshot.updatedAtMs, 11);
    ASSERT_EQ(secondSnapshot.players.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(secondSnapshot.players[0].name, std::string("Finn"));
    return true;
}

bool TestRefreshFetchStatusTransitionsToStaleAndOffline() {
    OnlineDataManager manager;
    manager.SetLiveThresholdSeconds(20);
    manager.SetOfflineThresholdSeconds(60);

    OnlineSnapshot staleSnapshot = BuildSnapshotSecondsAgo(30);
    manager.SetSnapshotForTesting(staleSnapshot);
    manager.RefreshFetchStatus();
    ASSERT_EQ(manager.GetSnapshotCopy().fetchStatus, OnlineFetchStatus::Stale);

    OnlineSnapshot offlineSnapshot = BuildSnapshotSecondsAgo(61);
    manager.SetSnapshotForTesting(offlineSnapshot);
    manager.RefreshFetchStatus();
    ASSERT_EQ(manager.GetSnapshotCopy().fetchStatus, OnlineFetchStatus::Offline);
    return true;
}

bool TestOldSuccessfulSnapshotTransitionsToOfflineAfterFailure() {
    FetchHookScope scope;
    SetResponses({
        {false, "", 500, 0}
    });

    OnlineDataManager manager;
    manager.SetLiveThresholdSeconds(20);
    manager.SetOfflineThresholdSeconds(60);
    manager.SetSnapshotForTesting(BuildSnapshotSecondsAgo(61));
    manager.PollNowForTesting();

    const OnlineSnapshot snapshot = manager.GetSnapshotCopy();
    ASSERT_EQ(snapshot.fetchStatus, OnlineFetchStatus::Offline);
    ASSERT_EQ(snapshot.totalPlayers, 1);
    ASSERT_EQ(snapshot.players.size(), static_cast<std::size_t>(1));
    return true;
}

bool TestSnapshotCopyRemainsImmutableAfterSubsequentUpdate() {
    FetchHookScope scope;
    SetResponses({
        {true, R"({"updatedAt":21,"count":1,"players":[{"id":"1","name":"Finn","level":1,"faction":null,"gang":null,"skinId":47,"teamRank":null}]})", 200, 0},
        {true, R"({"updatedAt":22,"count":2,"players":[{"id":"1","name":"Finn","level":1,"faction":null,"gang":null,"skinId":47,"teamRank":null},{"id":"2","name":"Berta","level":2,"faction":"San Andreas Army","gang":null,"skinId":7,"teamRank":null}]})", 200, 0}
    });

    OnlineDataManager manager;
    manager.PollNowForTesting();
    const OnlineSnapshot firstCopy = manager.GetSnapshotCopy();
    manager.PollNowForTesting();
    const OnlineSnapshot secondCopy = manager.GetSnapshotCopy();

    ASSERT_EQ(firstCopy.totalPlayers, 1);
    ASSERT_EQ(firstCopy.updatedAtMs, 21);
    ASSERT_EQ(firstCopy.players.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(firstCopy.players[0].name, std::string("Finn"));
    ASSERT_EQ(secondCopy.totalPlayers, 2);
    ASSERT_EQ(secondCopy.updatedAtMs, 22);
    ASSERT_EQ(secondCopy.players.size(), static_cast<std::size_t>(2));
    return true;
}

} // namespace

int RunOnlineDataManagerTests() {
    g_state = {};

    RUN_TEST(TestNormalizeFactionMatchesAliasesCaseInsensitively);
    RUN_TEST(TestNormalizeGangSuppressesFreedomFrontOnly);
    RUN_TEST(TestNormalizePlayerAppliesFactionPriorityGangFallbackAndCivilians);
    RUN_TEST(TestBuildBucketsUsesCanonicalOrderingAndSortedNames);
    RUN_TEST(TestResolveLocalAffiliationMatchesCaseInsensitiveName);
    RUN_TEST(TestComputeFetchStatusRespectsThresholdBoundaries);
    RUN_TEST(TestUpdateFromPlayersBuildsCanonicalSnapshot);
    RUN_TEST(TestPollingWorkerStoresLiveSnapshot);
    RUN_TEST(TestPollingAcceptsLiveApiNullPlayerFields);
    RUN_TEST(TestInitializeRestartKeepsSingleWorker);
    RUN_TEST(TestMalformedJsonMarksOfflineWithoutSnapshot);
    RUN_TEST(TestEmptyPayloadMarksOfflineWithoutSnapshot);
    RUN_TEST(TestHttpFailureKeepsLastGoodSnapshotAndMarksStale);
    RUN_TEST(TestRefreshFetchStatusTransitionsToStaleAndOffline);
    RUN_TEST(TestOldSuccessfulSnapshotTransitionsToOfflineAfterFailure);
    RUN_TEST(TestSnapshotCopyRemainsImmutableAfterSubsequentUpdate);

    std::printf("OnlineDataManagerTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}

#ifdef ONLINE_DATA_MANAGER_TEST_MAIN
int main() {
    return RunOnlineDataManagerTests();
}
#endif
