#include "../include/NearbyWantedLabels.h"
#include "../include/OnlineDataManager.h"

#include <cstdio>
#include <limits>

namespace {
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("[FAIL] NearbyWantedLabels: %s at %d\n", #x, __LINE__); } } while (0)

keybinder::NearbyWantedLabelState VisibleSample() {
    keybinder::NearbyWantedLabelState s;
    s.overlayEnabled = s.policeMember = s.connected = s.localActive = s.localAlive = true;
    s.streamed = s.targetAlive = true;
    s.serverAllowsLabels = s.remoteAllowsLabels = s.nativeNameTagVisible = true;
    s.localPlayerId = 10;
    s.playerId = 20;
    s.wanted = 15;
    return s;
}
}

int RunNearbyWantedLabelsTests() {
    using namespace keybinder;
    checks = failures = 0;
    CHECK(!ShouldShowNearbyWantedLabel({}));
    auto s = VisibleSample();
    CHECK(ShouldShowNearbyWantedLabel(s));
    // Native visibility is authoritative for both vehicle occupants and foot
    // players at every distance where their actual name is drawn.
    s.nativeNameTagVisible = false;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.nativeNameTagVisible = true;
    CHECK(ShouldShowNearbyWantedLabel(s));

    bool NearbyWantedLabelState::* const guards[] = {
        &NearbyWantedLabelState::overlayEnabled, &NearbyWantedLabelState::policeMember,
        &NearbyWantedLabelState::connected, &NearbyWantedLabelState::localActive,
        &NearbyWantedLabelState::localAlive, &NearbyWantedLabelState::streamed,
        &NearbyWantedLabelState::targetAlive, &NearbyWantedLabelState::serverAllowsLabels,
        &NearbyWantedLabelState::remoteAllowsLabels, &NearbyWantedLabelState::nativeNameTagVisible
    };
    for (const auto guard : guards) {
        s = VisibleSample();
        s.*guard = false;
        CHECK(!ShouldShowNearbyWantedLabel(s));
    }
    s = VisibleSample();
    s.wanted = 0;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.wanted = -1;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s = VisibleSample();
    s.playerId = s.localPlayerId;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.playerId = -1;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s = VisibleSample();
    s.localPlayerId = -1;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.localPlayerId = 1004;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s = VisibleSample();
    s.playerId = 1004;
    CHECK(!ShouldShowNearbyWantedLabel(s));

    OnlineSnapshot snapshot;
    snapshot.fetchStatus = OnlineFetchStatus::Live;
    OnlinePlayer player;
    player.name = "Lesch";
    player.wanted = 15;
    snapshot.players.push_back(player);
    CHECK(FindLiveWantedForName(snapshot, "Lesch") == 15);
    CHECK(FindLiveWantedForName(snapshot, " lesCH ") == 15);
    CHECK(!FindLiveWantedForName(snapshot, "Lesch_2"));
    CHECK(!FindLiveWantedForName(snapshot, "Lesc"));
    CHECK(!FindLiveWantedForName(snapshot, ""));
    CHECK(!FindLiveWantedForName(snapshot, "  "));
    snapshot.fetchStatus = OnlineFetchStatus::Stale;
    CHECK(!FindLiveWantedForName(snapshot, "Lesch"));
    snapshot.fetchStatus = OnlineFetchStatus::Offline;
    CHECK(!FindLiveWantedForName(snapshot, "Lesch"));
    snapshot.fetchStatus = OnlineFetchStatus::Live;
    snapshot.players[0].wanted = 0;
    CHECK(!FindLiveWantedForName(snapshot, "Lesch"));
    snapshot.players[0].wanted = -1;
    CHECK(!FindLiveWantedForName(snapshot, "Lesch"));
    snapshot.players[0].wanted = 15;
    snapshot.players.push_back(player);
    CHECK(!FindLiveWantedForName(snapshot, "Lesch")); // Duplicate name, even identical count, is ambiguous.
    snapshot.players[1].wanted = 0;
    CHECK(!FindLiveWantedForName(snapshot, "Lesch"));
    snapshot.players[1].name = "Lesch_2";
    CHECK(FindLiveWantedForName(snapshot, "Lesch") == 15);
    CHECK(!FindLiveWantedForName(snapshot, "Lesch_2"));
    for (const auto& name : {std::string("Lesch"), std::string("[RCB]Lesch"), std::string("Nick(3)")}) {
        for (int id : {0, 3, 1003}) {
            CHECK(MatchesNativePlayerNameTag(name, name, id));
            CHECK(MatchesNativePlayerNameTag(name + " (" + std::to_string(id) + ")", name, id));
            CHECK(MatchesNativePlayerNameTag(name + "(" + std::to_string(id) + ")", name, id));
            CHECK(MatchesNativePlayerNameTag("{FF0000}" + name + " (" + std::to_string(id) + "){FFFFFFFF}", name, id));
            CHECK(!MatchesNativePlayerNameTag("Other (" + std::to_string(id) + ")", name, id));
            CHECK(!MatchesNativePlayerNameTag(name + " (" + std::to_string(id + 1) + ")", name, id));
            CHECK(!MatchesNativePlayerNameTag(name + "Suffix", name, id));
        }
    }
    CHECK(MatchesNativePlayerNameTag(" lesCH (20) ", "Lesch", 20));
    CHECK(!MatchesNativePlayerNameTag("Lesch", "Lesch", -1));
    CHECK(!MatchesNativePlayerNameTag("Lesch (1004)", "Lesch", 1004));
    CHECK(!MatchesNativePlayerNameTag("Lesch (20) extra", "Lesch", 20));
    CHECK(!MatchesNativePlayerNameTag("Lesch\n(20)", "Lesch", 20));
    CHECK(!MatchesNativePlayerNameTag("[GESUCHT | 15 Wanteds]", "Lesch", 20));
    CHECK(!MatchesNativePlayerNameTag(std::string(193, 'x'), "Lesch", 20));
    s = VisibleSample();
    CHECK(BuildWantedLabelForNativeDraw(s, snapshot, "Lesch (20)", "Lesch") == "[GESUCHT | 15 Wanteds]");
    s.nativeNameTagVisible = false;
    CHECK(BuildWantedLabelForNativeDraw(s, snapshot, "Lesch (20)", "Lesch").empty());
    s = VisibleSample();
    CHECK(BuildWantedLabelForNativeDraw(s, snapshot, "Lesch (21)", "Lesch").empty());
    s.policeMember = false;
    CHECK(BuildWantedLabelForNativeDraw(s, snapshot, "Lesch (20)", "Lesch").empty());
    s = VisibleSample();
    snapshot.fetchStatus = OnlineFetchStatus::Stale;
    CHECK(BuildWantedLabelForNativeDraw(s, snapshot, "Lesch (20)", "Lesch").empty());
    snapshot.fetchStatus = OnlineFetchStatus::Live;
    CHECK(BuildNearbyWantedLabel(15) == "[GESUCHT | 15 Wanteds]");
    CHECK(BuildNearbyWantedLabel(1) == "[GESUCHT | 1 Wanteds]");
    CHECK(BuildNearbyWantedLabel(0).empty());
    CHECK(BuildNearbyWantedLabel(-1).empty());
    std::printf("NearbyWantedLabelsTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
