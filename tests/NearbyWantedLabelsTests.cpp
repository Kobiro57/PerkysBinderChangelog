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
    s.streamed = s.targetAlive = s.targetVisible = s.sameInterior = true;
    s.serverAllowsLabels = s.remoteAllowsLabels = s.cameraLineOfSight = s.localLineOfSight = true;
    s.projectedOnScreen = true;
    s.localPlayerId = 10;
    s.playerId = 20;
    s.wanted = 15;
    s.distanceSquared = 4.0f;
    s.serverDrawDistance = 30.0f;
    return s;
}
}

int RunNearbyWantedLabelsTests() {
    using namespace keybinder;
    checks = failures = 0;
    CHECK(kNearbyWantedLabelMaximumDistance == 5.0f);
    CHECK(!ShouldShowNearbyWantedLabel({}));
    auto s = VisibleSample();
    CHECK(ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = 25.0f;
    CHECK(ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = 25.0001f;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = 36.0f; // Six metres vertically also exceeds the fixed 3D radius.
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = 0.0f;
    CHECK(ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = -1.0f;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = std::numeric_limits<float>::infinity();
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.distanceSquared = std::numeric_limits<float>::quiet_NaN();
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s = VisibleSample();
    s.serverDrawDistance = 1.0f;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.serverDrawDistance = 2.0f;
    CHECK(ShouldShowNearbyWantedLabel(s));
    s.serverDrawDistance = 0.0f;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.serverDrawDistance = -1.0f;
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.serverDrawDistance = std::numeric_limits<float>::quiet_NaN();
    CHECK(!ShouldShowNearbyWantedLabel(s));
    s.serverDrawDistance = std::numeric_limits<float>::infinity();
    CHECK(!ShouldShowNearbyWantedLabel(s));

    bool NearbyWantedLabelState::* const guards[] = {
        &NearbyWantedLabelState::overlayEnabled, &NearbyWantedLabelState::policeMember,
        &NearbyWantedLabelState::connected, &NearbyWantedLabelState::localActive,
        &NearbyWantedLabelState::localAlive, &NearbyWantedLabelState::streamed,
        &NearbyWantedLabelState::targetAlive, &NearbyWantedLabelState::targetVisible,
        &NearbyWantedLabelState::sameInterior, &NearbyWantedLabelState::serverAllowsLabels,
        &NearbyWantedLabelState::remoteAllowsLabels, &NearbyWantedLabelState::cameraLineOfSight,
        &NearbyWantedLabelState::localLineOfSight, &NearbyWantedLabelState::projectedOnScreen
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
    CHECK(BuildNearbyWantedLabel(15) == "[GESUCHT | 15 Wanteds]");
    CHECK(BuildNearbyWantedLabel(1) == "[GESUCHT | 1 Wanteds]");
    CHECK(BuildNearbyWantedLabel(0).empty());
    CHECK(BuildNearbyWantedLabel(-1).empty());
    std::printf("NearbyWantedLabelsTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
