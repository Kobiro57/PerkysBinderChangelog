#define KEYBINDER_TESTING

#include "../include/OnlinePlayerInfoService.h"
#include "../include/RssHttpClient.h"

#include <cstdio>
#include <string>
#include <unordered_map>

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
    std::string lastModified;
    std::string etag;
    unsigned long statusCode = 200;
};

TestState g_state;
std::unordered_map<std::string, MockFetchResponse> g_mockResponses;

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

bool ContainsText(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool MockFetch(const std::string& url,
               std::string& outBody,
               const std::string& ifModifiedSince,
               const std::string& ifNoneMatch,
               std::string& outLastModified,
               std::string& outEtag,
               unsigned long& outStatusCode) {
    (void)ifModifiedSince;
    (void)ifNoneMatch;

    std::unordered_map<std::string, MockFetchResponse>::const_iterator it = g_mockResponses.find(url);
    if (it == g_mockResponses.end()) {
        return false;
    }

    const MockFetchResponse& response = it->second;
    outBody = response.body;
    outLastModified = response.lastModified;
    outEtag = response.etag;
    outStatusCode = response.statusCode;
    return response.success;
}

struct FetchHookScope {
    FetchHookScope() {
        g_mockResponses.clear();
        RssHttpClient::SetTestFetchHandler(&MockFetch);
    }

    ~FetchHookScope() {
        RssHttpClient::ResetTestFetchHandler();
        g_mockResponses.clear();
    }
};

bool TestParsePlayersResponseHandlesNullableFields() {
    const std::string json = R"({
        "updatedAt": 1776877920045,
        "count": 2,
        "players": [
            {
                "id": "4fccfce6-5d28-46ab-94f0-c2be52f97a03",
                "name": "Finn",
                "level": 1,
                "faction": null,
                "gang": null,
                "skinId": 47,
                "teamRank": "Developer"
            },
            {
                "id": "84beed8c-183b-4ab6-8674-1b70da0d6849",
                "name": "perkys",
                "level": 3,
                "faction": null,
                "gang": null,
                "skinId": 2,
                "teamRank": null
            }
        ]
    })";

    std::vector<OnlinePlayerInfo> players;
    ASSERT_TRUE(OnlinePlayerInfoService::ParsePlayersResponse(json, players));
    ASSERT_EQ(players.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(players[0].name, std::string("Finn"));
    ASSERT_EQ(players[0].level, 1);
    ASSERT_TRUE(!players[0].hasFaction);
    ASSERT_TRUE(!players[0].hasGang);
    ASSERT_TRUE(players[0].hasTeamRank);
    ASSERT_EQ(players[0].teamRank, std::string("Developer"));
    ASSERT_TRUE(!players[1].hasTeamRank);
    return true;
}

bool TestParsePlayersResponseSkipsNullNamesAndAcceptsNullSkinIds() {
    const std::string json = R"({
        "updatedAt": 1779557205032,
        "count": 3,
        "players": [
            {
                "id": "65bbcd91-7f83-4f80-ab9b-d746c6158951",
                "name": null,
                "level": 2,
                "faction": "Police Department",
                "gang": null,
                "skinId": 28,
                "teamRank": null
            },
            {
                "id": "980edf6c-e2a2-43c4-8495-7db77c0401a9",
                "name": "Yoshi",
                "level": 1,
                "faction": null,
                "gang": null,
                "skinId": null,
                "teamRank": null
            },
            {
                "id": "241491b8-2b2a-44da-90d4-4a7e705fc120",
                "name": "Welf",
                "level": 4,
                "faction": "Police Department",
                "gang": null,
                "skinId": 31,
                "teamRank": null
            }
        ]
    })";

    std::vector<OnlinePlayerInfo> players;
    ASSERT_TRUE(OnlinePlayerInfoService::ParsePlayersResponse(json, players));
    ASSERT_EQ(players.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(players[0].name, std::string("Yoshi"));
    ASSERT_EQ(players[0].skinId, 0);
    ASSERT_EQ(players[1].name, std::string("Welf"));
    ASSERT_TRUE(players[1].hasFaction);
    ASSERT_EQ(players[1].faction, std::string("Police Department"));
    return true;
}

bool TestParsePlayersResponseReportsPlayerFieldError() {
    const std::string json = R"({"updatedAt":1,"count":1,"players":[{"id":"1","name":"Finn","level":"bad","faction":null,"gang":null,"skinId":47,"teamRank":null}]})";

    std::vector<OnlinePlayerInfo> players;
    std::string parseError;
    ASSERT_TRUE(!OnlinePlayerInfoService::ParsePlayersResponse(json, players, &parseError));
    ASSERT_TRUE(ContainsText(parseError, "players[0].level"));
    ASSERT_TRUE(ContainsText(parseError, "expected integer digit"));
    ASSERT_TRUE(ContainsText(parseError, "near"));
    return true;
}

bool TestParsePlayersResponseHandlesWantedField() {
    // wanted present as integer
    const std::string json = R"({
        "updatedAt": 1,
        "count": 2,
        "players": [
            {"id":"1","name":"WantedOne","level":5,"faction":null,"gang":null,"skinId":2,"teamRank":null,"wanted":158},
            {"id":"2","name":"Clean","level":5,"faction":null,"gang":null,"skinId":2,"teamRank":null,"wanted":0}
        ]
    })";

    std::vector<OnlinePlayerInfo> players;
    ASSERT_TRUE(OnlinePlayerInfoService::ParsePlayersResponse(json, players));
    ASSERT_EQ(players.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(players[0].name, std::string("WantedOne"));
    ASSERT_EQ(players[0].wanted, 158);
    ASSERT_EQ(players[1].name, std::string("Clean"));
    ASSERT_EQ(players[1].wanted, 0);
    return true;
}

bool TestParsePlayersResponseWantedAbsentDefaultsToZero() {
    // Old API response without wanted field — must still parse, wanted defaults to 0.
    const std::string json = R"({
        "updatedAt": 1,
        "count": 1,
        "players": [
            {"id":"1","name":"Legacy","level":5,"faction":null,"gang":null,"skinId":2,"teamRank":null}
        ]
    })";

    std::vector<OnlinePlayerInfo> players;
    ASSERT_TRUE(OnlinePlayerInfoService::ParsePlayersResponse(json, players));
    ASSERT_EQ(players.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(players[0].name, std::string("Legacy"));
    ASSERT_EQ(players[0].wanted, 0);
    return true;
}

bool TestParsePlayersResponseWantedNullDefaultsToZero() {
    const std::string json = R"({
        "updatedAt": 1,
        "count": 1,
        "players": [
            {"id":"1","name":"NullWanted","level":5,"faction":null,"gang":null,"skinId":2,"teamRank":null,"wanted":null}
        ]
    })";

    std::vector<OnlinePlayerInfo> players;
    ASSERT_TRUE(OnlinePlayerInfoService::ParsePlayersResponse(json, players));
    ASSERT_EQ(players.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(players[0].wanted, 0);
    return true;
}

bool TestFindPlayerByNameIsCaseInsensitive() {
    OnlinePlayerInfo firstPlayer;
    firstPlayer.id = "1";
    firstPlayer.name = "Finn";
    firstPlayer.level = 1;
    firstPlayer.skinId = 47;
    firstPlayer.hasTeamRank = true;
    firstPlayer.teamRank = "Developer";

    OnlinePlayerInfo secondPlayer;
    secondPlayer.id = "2";
    secondPlayer.name = "perkys";
    secondPlayer.level = 3;
    secondPlayer.skinId = 2;

    std::vector<OnlinePlayerInfo> players;
    players.push_back(firstPlayer);
    players.push_back(secondPlayer);

    OnlinePlayerInfo player;
    ASSERT_TRUE(OnlinePlayerInfoService::FindPlayerByName(players, "fInn", player));
    ASSERT_EQ(player.name, std::string("Finn"));
    ASSERT_TRUE(OnlinePlayerInfoService::FindPlayerByName(players, "PERKYS", player));
    ASSERT_EQ(player.name, std::string("perkys"));
    return true;
}

bool TestBuildChatMessageOmitsNullFields() {
    OnlinePlayerInfo player;
    player.name = "Finn";
    player.level = 1;
    player.hasTeamRank = true;
    player.teamRank = "Developer";

    const std::string message = OnlinePlayerInfoService::BuildChatMessage(player);
    ASSERT_TRUE(ContainsText(message, "Infos für Finn: Level: 1"));
    ASSERT_TRUE(ContainsText(message, "Team: Developer"));
    ASSERT_TRUE(!ContainsText(message, "Fraktion:"));
    ASSERT_TRUE(!ContainsText(message, "Gang:"));
    return true;
}

bool TestLookupNowReturnsSuccessMessage() {
    FetchHookScope scope;
    g_mockResponses["https://nova-legacy.de/api/players/online"] = {
        true,
        R"({"updatedAt":1,"count":1,"players":[{"id":"1","name":"Finn","level":1,"faction":null,"gang":null,"skinId":47,"teamRank":"Developer"}]})",
        "",
        "",
        200
    };

    OnlinePlayerInfoService service;
    const OnlinePlayerLookupResult result = service.LookupNow("finn");
    ASSERT_EQ(result.status, OnlinePlayerLookupResult::Status::Success);
    ASSERT_TRUE(ContainsText(result.chatMessage, "Infos für Finn: Level: 1"));
    ASSERT_TRUE(ContainsText(result.chatMessage, "Team: Developer"));
    ASSERT_TRUE(!ContainsText(result.chatMessage, "Fraktion:"));
    return true;
}

bool TestLookupNowReturnsNotFoundMessage() {
    FetchHookScope scope;
    g_mockResponses["https://nova-legacy.de/api/players/online"] = {
        true,
        R"({"updatedAt":1,"count":1,"players":[{"id":"1","name":"Finn","level":1,"faction":null,"gang":null,"skinId":47,"teamRank":"Developer"}]})",
        "",
        "",
        200
    };

    OnlinePlayerInfoService service;
    const OnlinePlayerLookupResult result = service.LookupNow("MissingUser");
    ASSERT_EQ(result.status, OnlinePlayerLookupResult::Status::NotFound);
    ASSERT_TRUE(ContainsText(result.chatMessage, "Kein Online-Spieler mit dem Namen MissingUser gefunden."));
    return true;
}

} // namespace

int RunOnlinePlayerInfoTests() {
    g_state = {};

    RUN_TEST(TestParsePlayersResponseHandlesNullableFields);
    RUN_TEST(TestParsePlayersResponseSkipsNullNamesAndAcceptsNullSkinIds);
    RUN_TEST(TestParsePlayersResponseReportsPlayerFieldError);
    RUN_TEST(TestParsePlayersResponseHandlesWantedField);
    RUN_TEST(TestParsePlayersResponseWantedAbsentDefaultsToZero);
    RUN_TEST(TestParsePlayersResponseWantedNullDefaultsToZero);
    RUN_TEST(TestFindPlayerByNameIsCaseInsensitive);
    RUN_TEST(TestBuildChatMessageOmitsNullFields);
    RUN_TEST(TestLookupNowReturnsSuccessMessage);
    RUN_TEST(TestLookupNowReturnsNotFoundMessage);

    std::printf("OnlinePlayerInfoTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}

#ifdef ONLINE_PLAYER_INFO_TEST_MAIN
int main() {
    return RunOnlinePlayerInfoTests();
}
#endif
