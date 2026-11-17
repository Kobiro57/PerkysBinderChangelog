#include "../include/WaypointTour.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] WaypointTour: %s (line %d)\n", #value, __LINE__); } } while (0)
constexpr unsigned int mask = keybinder::GpsManager::kTemporaryIdMask;

struct Fixture {
    keybinder::GpsPosition position{0, 0, 0, 0};
    bool gpsAvailable = true, tourAvailable = true;
    std::string session = "127.0.0.1:7777/PoliceOfficer";
    std::vector<keybinder::GpsEntry> points{
        {1, "First", {10, 0, 0, 0}}, {2, "Second", {20, 0, 0, 0}}, {3, "Third", {50, 0, 0, 0}}};
    std::vector<std::string> messages;
    std::vector<std::pair<keybinder::GpsEntry, std::uint64_t>> arrivals;
    std::optional<keybinder::GpsEntry> rendered;
    keybinder::GpsManager gps;
    keybinder::WaypointTour tour;

    Fixture() {
        static unsigned int fixtureId = 0;
        const auto directory = std::filesystem::path("build/waypoint-tour-tests") /
            (std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())
                + "-" + std::to_string(++fixtureId));
        std::filesystem::create_directories(directory);
        gps.Initialize((directory / "gps.xml").string(), [&](const auto& text) { messages.push_back(text); },
            [&](auto& current) { current = position; return gpsAvailable; },
            [&](const keybinder::GpsEntry* destination) {
                if (destination) rendered = *destination; else rendered.reset();
            });
        tour.Initialize(&gps, "Spray-Tour", "/sprayend", "Du bist kein Polizist.",
            [&](const auto& text) { messages.push_back(text); },
            [&](auto& current) { current = position; return tourAvailable; },
            [&]() { return session; }, [&]() -> const std::vector<keybinder::GpsEntry>& { return points; });
        gps.SetArrivalCallback([&](const keybinder::GpsEntry& destination, std::uint64_t generation) {
            arrivals.emplace_back(destination, generation);
            tour.OnArrival(destination, generation);
        });
    }
    ~Fixture() { tour.Shutdown(); gps.Shutdown(); }

    unsigned int CurrentId() const {
        const auto* current = gps.ActiveDestination();
        return current ? current->id : 0;
    }
    std::size_t MessagesContaining(const std::string& part) const {
        return std::count_if(messages.begin(), messages.end(), [&](const auto& text) {
            return text.find(part) != std::string::npos;
        });
    }
    void Arrive() {
        const auto* destination = gps.ActiveDestination();
        CHECK(destination != nullptr);
        if (!destination) return;
        position = destination->position;
        gps.Update();
        tour.Update();
    }
    void ManualRoute(unsigned int id = 100) {
        CHECK(gps.NavigateTemporary({mask | id, "Manual", {100, 100, 0, 0}}));
    }
};

void NearestSelectionUsesCurrentPositionAndVisitsEveryPointOnce() {
    Fixture fixture;
    fixture.points = {{2, "South", {15, -1, 0, 0}}, {1, "Near", {10, 0, 0, 0}},
        {3, "North", {15, 1, 0, 0}}};
    CHECK(fixture.tour.Start());
    CHECK(fixture.tour.IsActive() && fixture.CurrentId() == (mask | 1));
    CHECK(fixture.MessagesContaining("Spray-Tour gestartet.") == 1);
    CHECK(fixture.MessagesContaining("/sprayend") == 1);
    CHECK(fixture.gps.Entries().empty());
    // Both next points tie from the target center. Actual arrival north of it
    // must choose North, rather than using the start or target-center position.
    fixture.position = {10, 2.5f, 0, 0};
    fixture.gps.Update();
    CHECK(fixture.CurrentId() == (mask | 3));
    const auto firstArrival = fixture.arrivals.front();
    fixture.tour.OnArrival(firstArrival.first, firstArrival.second);
    CHECK(fixture.CurrentId() == (mask | 3)); // Duplicate old callback is ignored.
    fixture.Arrive();
    CHECK(fixture.CurrentId() == (mask | 2));
    fixture.Arrive();
    CHECK(!fixture.tour.IsActive() && !fixture.gps.ActiveDestination() && !fixture.rendered);
    CHECK(fixture.arrivals.size() == 3);
    CHECK(fixture.arrivals[0].first.id == (mask | 1));
    CHECK(fixture.arrivals[1].first.id == (mask | 3));
    CHECK(fixture.arrivals[2].first.id == (mask | 2));
    CHECK(fixture.MessagesContaining("abgeschlossen.") == 1);
    for (int index = 0; index < 5; ++index) {
        fixture.gps.Update(); fixture.tour.Update();
        fixture.tour.OnArrival(firstArrival.first, firstArrival.second);
    }
    CHECK(fixture.arrivals.size() == 3 && fixture.MessagesContaining("abgeschlossen.") == 1);
}

void DistanceIsThreeDimensionalAndEqualDistancesUseStableIds() {
    Fixture fixture;
    fixture.points = {{7, "High", {0, 0, 20, 0}}, {4, "West", {-5, 0, 0, 0}},
        {2, "East", {5, 0, 0, 0}}};
    CHECK(fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 2));
    fixture.tour.Stop(false);
    fixture.points = {{7, "High", {0, 0, 4, 0}}, {2, "East", {5, 0, 0, 0}}};
    CHECK(fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 7));
}

void AlreadyInsideTargetsAndRepeatedStartDoNotLoopOrResetProgress() {
    Fixture fixture;
    fixture.points = {{1, "Here", {0, 0, 0, 0}}, {2, "Next", {10, 0, 0, 0}}};
    CHECK(fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 1));
    fixture.gps.Update(); // Tour targets deliberately allow arrival from inside.
    CHECK(fixture.CurrentId() == (mask | 2) && fixture.arrivals.size() == 1);
    const auto generation = fixture.gps.ActiveNavigationGeneration();
    CHECK(!fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 2) && fixture.gps.ActiveNavigationGeneration() == generation);
    CHECK(fixture.MessagesContaining("/sprayend") == 2);
    fixture.Arrive();
    CHECK(!fixture.tour.IsActive() && fixture.arrivals.size() == 2);
    CHECK(fixture.MessagesContaining("abgeschlossen.") == 1);
    fixture.position = {0, 0, 0, 0};
    CHECK(fixture.tour.Start()); // A newly started tour has a new visited set.
    CHECK(fixture.CurrentId() == (mask | 1));
}

void ManualCancellationAndSameIdReplacementNeverStartAnotherTarget() {
    Fixture fixture;
    CHECK(fixture.tour.Start());
    const auto original = *fixture.gps.ActiveDestination();
    const auto originalGeneration = fixture.gps.ActiveNavigationGeneration();
    CHECK(fixture.gps.NavigateTemporary(original)); // Same ID, different owner receipt.
    const auto replacementGeneration = fixture.gps.ActiveNavigationGeneration();
    CHECK(replacementGeneration != originalGeneration);
    fixture.tour.Update();
    CHECK(!fixture.tour.IsActive() && fixture.gps.ActiveDestination());
    CHECK(fixture.gps.ActiveNavigationGeneration() == replacementGeneration);
    fixture.tour.OnArrival(original, originalGeneration);
    fixture.tour.Stop(false);
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveNavigationGeneration() == replacementGeneration);
    CHECK(fixture.arrivals.empty());

    CHECK(fixture.tour.Start());
    fixture.gps.StopNavigation();
    fixture.tour.Update();
    CHECK(!fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
    fixture.gps.Update(); fixture.tour.Update();
    CHECK(!fixture.gps.ActiveDestination() && fixture.arrivals.empty());
    CHECK(fixture.tour.Start());
    fixture.tour.Stop();
    CHECK(!fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
    CHECK(fixture.MessagesContaining("abgebrochen.") == 1);
    fixture.tour.Stop();
    CHECK(fixture.MessagesContaining("Keine Spray-Tour aktiv.") == 1);
}

void RoleAndSessionLossClearOnlyTheOwnedRouteAndNeverReplayArrivals() {
    for (const bool replaceFirst : {false, true}) {
        Fixture fixture;
        CHECK(fixture.tour.Start());
        const auto original = *fixture.gps.ActiveDestination();
        const auto generation = fixture.gps.ActiveNavigationGeneration();
        if (replaceFirst) fixture.ManualRoute(1);
        const auto replacementGeneration = fixture.gps.ActiveNavigationGeneration();
        fixture.session.clear();
        fixture.tour.Update();
        CHECK(!fixture.tour.IsActive());
        CHECK((fixture.gps.ActiveDestination() != nullptr) == replaceFirst);
        if (replaceFirst) CHECK(fixture.gps.ActiveNavigationGeneration() == replacementGeneration);
        fixture.session = "127.0.0.1:7777/PoliceOfficer";
        fixture.tour.OnArrival(original, generation);
        fixture.tour.Update();
        CHECK(!fixture.tour.IsActive() && fixture.arrivals.empty());
    }
    Fixture fixture;
    CHECK(fixture.tour.Start());
    fixture.session = "127.0.0.1:7777/AnotherOfficer";
    fixture.tour.Update();
    CHECK(!fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
}

void CurrentCatalogChangesAreUsedWithoutVisitingDeletedPoints() {
    Fixture fixture;
    CHECK(fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 1));
    fixture.points.erase(fixture.points.begin());
    fixture.tour.Update();
    CHECK(fixture.CurrentId() == (mask | 2) && fixture.arrivals.empty());
    fixture.points.push_back({4, "Added nearby", {21, 0, 0, 0}});
    fixture.Arrive();
    CHECK(fixture.CurrentId() == (mask | 4));
    fixture.Arrive();
    CHECK(fixture.CurrentId() == (mask | 3));
    fixture.points.clear();
    fixture.tour.Update();
    CHECK(!fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
    CHECK(fixture.MessagesContaining("abgeschlossen.") == 1);

    fixture.points = {{1, "Moved", {10, 0, 0, 0}}, {2, "Nearer", {20, 0, 0, 0}}};
    fixture.position = {0, 0, 0, 0};
    CHECK(fixture.tour.Start());
    fixture.points.front().position.x = 100;
    fixture.tour.Update();
    CHECK(fixture.CurrentId() == (mask | 2));
}

void EmptyOrInvalidStartsPreserveExistingNavigation() {
    Fixture fixture;
    fixture.ManualRoute();
    const auto generation = fixture.gps.ActiveNavigationGeneration();
    fixture.session.clear();
    CHECK(!fixture.tour.Start());
    CHECK(fixture.MessagesContaining("Du bist kein Polizist.") == 1);
    fixture.session = "host/Officer";
    fixture.points.clear();
    CHECK(!fixture.tour.Start());
    fixture.points = {{0, "Zero", {1, 2, 3, 0}}, {mask, "High-bit", {1, 2, 3, 0}},
        {1, "", {1, 2, 3, 0}}, {2, "Interior", {1, 2, 3, 1}},
        {3, "NaN", {std::numeric_limits<float>::quiet_NaN(), 0, 0, 0}},
        {4, std::string(65, 'A'), {1, 2, 3, 0}}};
    CHECK(!fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 100) && fixture.gps.ActiveNavigationGeneration() == generation);
    fixture.points.push_back({5, "Valid", {10, 0, 0, 0}});
    fixture.position.interior = 1;
    CHECK(!fixture.tour.Start());
    fixture.position.interior = 0;
    fixture.tourAvailable = false;
    CHECK(!fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 100) && fixture.gps.ActiveNavigationGeneration() == generation);
    fixture.tourAvailable = true;
    CHECK(fixture.tour.Start());
    CHECK(fixture.CurrentId() == (mask | 5));
}

void InteriorAndPositionStallsPauseWithoutFalseCompletionOrStealingGps() {
    Fixture fixture;
    CHECK(fixture.tour.Start());
    fixture.position.interior = 1;
    fixture.gps.Update(); fixture.tour.Update();
    CHECK(fixture.tour.IsActive() && fixture.CurrentId() == (mask | 1) && !fixture.rendered);
    fixture.position.interior = 0;
    fixture.tourAvailable = false;
    fixture.position.x = 10;
    fixture.gps.Update(); // Arrival confirmed, but the next position sample is unavailable.
    CHECK(fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
    fixture.tour.Update();
    CHECK(fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
    fixture.tourAvailable = true;
    fixture.tour.Update();
    CHECK(fixture.CurrentId() == (mask | 2));
    fixture.tourAvailable = false;
    fixture.position.x = 20;
    fixture.gps.Update();
    CHECK(fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
    fixture.ManualRoute();
    const auto replacement = fixture.gps.ActiveNavigationGeneration();
    fixture.tourAvailable = true;
    fixture.tour.Update();
    CHECK(!fixture.tour.IsActive() && fixture.CurrentId() == (mask | 100));
    CHECK(fixture.gps.ActiveNavigationGeneration() == replacement);
}

void ClaimsConfigurationUsesItsOwnDeniedMessageAndStopCommand() {
    Fixture fixture;
    fixture.tour.Initialize(&fixture.gps, "Flaggen-Tour", "/claimsend",
        "Du bist kein Mitglied der Red County Brotherhood.",
        [&](const auto& text) { fixture.messages.push_back(text); },
        [&](auto& position) { position = fixture.position; return true; },
        [&]() { return fixture.session; }, [&]() -> const std::vector<keybinder::GpsEntry>& { return fixture.points; });
    fixture.session.clear();
    CHECK(!fixture.tour.Start());
    CHECK(fixture.MessagesContaining("Du bist kein Mitglied der Red County Brotherhood.") == 1);
    fixture.session = "host/RCBMember";
    CHECK(fixture.tour.Start());
    CHECK(fixture.MessagesContaining("Flaggen-Tour gestartet.") == 1);
    CHECK(fixture.MessagesContaining("/claimsend") == 1);
    fixture.tour.Shutdown();
    CHECK(!fixture.tour.IsActive() && !fixture.gps.ActiveDestination());
}
} // namespace

int RunWaypointTourTests() {
    checks = failures = 0;
    NearestSelectionUsesCurrentPositionAndVisitsEveryPointOnce();
    DistanceIsThreeDimensionalAndEqualDistancesUseStableIds();
    AlreadyInsideTargetsAndRepeatedStartDoNotLoopOrResetProgress();
    ManualCancellationAndSameIdReplacementNeverStartAnotherTarget();
    RoleAndSessionLossClearOnlyTheOwnedRouteAndNeverReplayArrivals();
    CurrentCatalogChangesAreUsedWithoutVisitingDeletedPoints();
    EmptyOrInvalidStartsPreserveExistingNavigation();
    InteriorAndPositionStallsPauseWithoutFalseCompletionOrStealingGps();
    ClaimsConfigurationUsesItsOwnDeniedMessageAndStopCommand();
    std::printf("WaypointTourTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
