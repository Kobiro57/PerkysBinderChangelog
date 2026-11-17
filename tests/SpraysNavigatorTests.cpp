#include "../include/SpraysNavigator.h"
#include "../include/SpraySpots.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <windows.h>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] SpraysNavigator: %s (line %d)\n", #value, __LINE__); } } while (0)

struct Fixture {
    keybinder::GpsPosition position{1966.9f, -1174.7f, 20.0f, 0};
    bool available = true;
    std::string session = "127.0.0.1:7777/PoliceOfficer";
    std::vector<std::string> notices;
    unsigned int renderedId = 0;
    keybinder::GpsPosition renderedPosition;
    keybinder::GpsManager gps;
    keybinder::SpraysNavigator sprays;
    std::filesystem::path path;

    Fixture() {
        static unsigned int fixtureId = 0;
        path = std::filesystem::path("build/sprays-navigation-tests")
            / (std::to_string(GetTickCount64()) + "-" + std::to_string(++fixtureId));
        std::filesystem::create_directories(path);
        gps.Initialize((path / "gps.xml").string(),
            [&](const auto& text) { notices.push_back(text); },
            [&](auto& current) { current = position; return available; },
            [&](const keybinder::GpsEntry* destination) {
                renderedId = destination ? destination->id : 0;
                if (destination) renderedPosition = destination->position;
            });
        sprays.Initialize(&gps, [&](const auto& text) { notices.push_back(text); },
            [&](auto& current) { current = position; return available; }, [&]() { return session; });
    }
    ~Fixture() { sprays.Shutdown(); gps.Shutdown(); }

    std::size_t Arrivals() const {
        return std::count_if(notices.begin(), notices.end(), [](const auto& text) {
            return text.find("Du hast dein Ziel erreicht:") != std::string::npos;
        });
    }
};

void CompleteCatalogHasStableIdsAndUsesAllSuppliedPositions() {
    CHECK(keybinder::kSpraySpots.size() == 100);
    std::set<unsigned int> ids;
    for (std::size_t index = 0; index < keybinder::kSpraySpots.size(); ++index) {
        const auto& point = keybinder::kSpraySpots[index];
        CHECK(point.id == index + 1);
        CHECK(ids.insert(point.id).second);
        CHECK(point.place && *point.place);
        CHECK(keybinder::GpsManager::IsValidPosition(point.position));
        CHECK(point.position.interior == 0);
        const auto rows = keybinder::SpraysNavigator::BuildRows(point.position);
        CHECK(rows.size() == 100);
        CHECK(rows.front().id == point.id);
        CHECK(rows.front().distance == 0.0f);
        CHECK(std::is_sorted(rows.begin(), rows.end(), [](const auto& left, const auto& right) {
            return left.distance == right.distance ? left.id < right.id : left.distance < right.distance;
        }));
    }
    CHECK(keybinder::kSpraySpots.front().position.x == 944.3f);
    CHECK(keybinder::kSpraySpots.front().position.y == -985.8f);
    CHECK(keybinder::kSpraySpots.front().position.z == 39.3f);
    CHECK(std::string(keybinder::kSpraySpots[13].place) == "Market");
    CHECK(keybinder::kSpraySpots[13].position.x == 1098.8f && keybinder::kSpraySpots[13].position.y == -1292.5f);
    CHECK(std::string(keybinder::kSpraySpots[18].place) == "Glen Park");
    CHECK(keybinder::kSpraySpots[18].position.x == 1966.9f && keybinder::kSpraySpots[18].position.y == -1174.7f);
    CHECK(keybinder::kSpraySpots.back().position.x == 1574.7f);
    CHECK(keybinder::kSpraySpots.back().position.y == -2691.9f);
    CHECK(keybinder::kSpraySpots.back().position.z == 13.6f);
}

void SortingShowsRealMeterDistancesAndThreeCompletePages() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.IsOpen());
    CHECK(fixture.sprays.Rows().front().id == 19);
    CHECK(fixture.sprays.Page() == 0 && fixture.sprays.PageCount() == 3);
    std::set<unsigned int> covered;
    for (std::size_t page = 0; page < 3; ++page) {
        fixture.sprays.SetPage(page);
        CHECK(fixture.sprays.Page() == page);
        const auto begin = page * keybinder::SpraysNavigator::kRowsPerPage;
        const auto end = (std::min)(begin + keybinder::SpraysNavigator::kRowsPerPage, fixture.sprays.Rows().size());
        CHECK(end - begin == (page == 2 ? 32 : 34));
        for (std::size_t index = begin; index < end; ++index)
            CHECK(covered.insert(fixture.sprays.Rows()[index].id).second);
    }
    CHECK(covered.size() == 100);
    fixture.sprays.SetPage(99);
    CHECK(fixture.sprays.Page() == 2);
    fixture.position = keybinder::kSpraySpots.back().position;
    fixture.position.z += 10.0f;
    fixture.sprays.Update();
    CHECK(fixture.sprays.Page() == 2); // Live sort must not jump to the first page.
    CHECK(fixture.sprays.Rows().front().id == 100);
    CHECK(std::abs(fixture.sprays.Rows().front().distance - 10.0f) < 0.01f);
    for (const auto& row : fixture.sprays.Rows()) {
        const auto& target = keybinder::kSpraySpots[row.id - 1].position;
        const double dx = static_cast<double>(fixture.position.x) - target.x;
        const double dy = static_cast<double>(fixture.position.y) - target.y;
        const double dz = static_cast<double>(fixture.position.z) - target.z;
        CHECK(std::abs(row.distance - std::sqrt(dx * dx + dy * dy + dz * dz)) < 0.01);
    }
    fixture.sprays.Close();
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Page() == 0);
}

void SelectionUsesPointIdentityAndSharedGpsCheckpointWithoutSaving() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    fixture.sprays.SetPage(2);
    const auto chosenId = fixture.sprays.Rows()[68].id;
    const auto expected = keybinder::kSpraySpots[chosenId - 1].position;
    CHECK(fixture.sprays.Select(chosenId));
    CHECK(!fixture.sprays.IsOpen());
    CHECK(fixture.gps.ActiveDestination() != nullptr);
    CHECK(fixture.gps.ActiveDestination()->id == (keybinder::GpsManager::kTemporaryIdMask | chosenId));
    CHECK(fixture.renderedId == fixture.gps.ActiveDestination()->id);
    CHECK(fixture.renderedPosition.x == expected.x && fixture.renderedPosition.y == expected.y && fixture.renderedPosition.z == expected.z);
    CHECK(fixture.gps.Entries().empty());
    CHECK(!std::filesystem::exists(fixture.path / "gps.xml"));
    CHECK(fixture.sprays.Open());
    CHECK(!fixture.sprays.Select(0));
    CHECK(!fixture.sprays.Select(101));
    CHECK(fixture.gps.ActiveDestination()->id == fixture.renderedId);
    // Stable point identity must also work for a point now first in the sort.
    fixture.position = {1574.7f, -2691.9f, 23.6f, 0};
    fixture.sprays.Update();
    CHECK(fixture.sprays.Rows().front().id == 100);
    CHECK(fixture.sprays.Select(100));
    CHECK(fixture.gps.ActiveDestination()->name == "Graffiti 100 - Los Santos International");
    CHECK(fixture.renderedPosition.x == 1574.7f && fixture.renderedPosition.y == -2691.9f && fixture.renderedPosition.z == 13.6f);
}

void NavigationFitsAlongsideTwentySavedGpsEntriesAndArrivesExactlyOnce() {
    Fixture fixture;
    for (unsigned int index = 0; index < 20; ++index) CHECK(fixture.gps.Set("Privates Ziel " + std::to_string(index + 1)));
    const auto path = fixture.path / "gps.xml";
    std::ifstream beforeFile(path, std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(beforeFile)), {});
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Select(14));
    CHECK(fixture.gps.Entries().size() == 20);
    std::ifstream afterFile(path, std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(afterFile)), {});
    CHECK(before == after);
    CHECK(fixture.renderedId == (keybinder::GpsManager::kTemporaryIdMask | 14));
    CHECK(fixture.Arrivals() == 0);
    fixture.position = {1098.8f, -1292.5f, 17.1f, 0};
    fixture.gps.Update();
    fixture.sprays.Update();
    CHECK(fixture.Arrivals() == 1);
    CHECK(fixture.gps.ActiveDestination() == nullptr && fixture.renderedId == 0);
    for (int frame = 0; frame < 10; ++frame) { fixture.sprays.Update(); fixture.gps.Update(); }
    CHECK(fixture.Arrivals() == 1);
    CHECK(fixture.gps.Entries().size() == 20);
}

void PoliceAccessAndSessionChangesNeverCancelAnotherGpsDestination() {
    Fixture fixture;
    fixture.session.clear();
    CHECK(!fixture.sprays.Open());
    CHECK(!fixture.sprays.IsOpen() && fixture.gps.ActiveDestination() == nullptr);
    CHECK(fixture.notices.back().find("Du bist kein Polizist.") != std::string::npos);
    const auto notices = fixture.notices.size();
    fixture.sprays.Update();
    fixture.sprays.Update();
    CHECK(fixture.notices.size() == notices);
    fixture.session = "127.0.0.1:7777/OfficerA";
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Select(14));
    fixture.session.clear();
    fixture.sprays.Update();
    CHECK(fixture.gps.ActiveDestination() == nullptr && fixture.renderedId == 0);
    fixture.session = "127.0.0.1:7777/OfficerA";
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Select(100));
    CHECK(fixture.sprays.Open());
    fixture.session = "127.0.0.1:7777/OfficerB";
    CHECK(!fixture.sprays.Select(14)); // A stale row must not act for a new account.
    CHECK(!fixture.sprays.IsOpen() && fixture.gps.ActiveDestination() == nullptr);
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Select(14));
    CHECK(fixture.gps.Set("Mein eigenes GPS-Ziel"));
    const auto savedId = fixture.gps.Entries().front().id;
    CHECK(fixture.gps.Navigate(savedId));
    fixture.session.clear();
    fixture.sprays.Update();
    CHECK(fixture.gps.ActiveDestination() != nullptr && fixture.gps.ActiveDestination()->id == savedId);
    fixture.sprays.Shutdown();
    CHECK(fixture.gps.ActiveDestination() != nullptr && fixture.gps.ActiveDestination()->id == savedId);
}

void MissingPositionsAndInteriorsNeverProduceMisleadingPoliceErrors() {
    Fixture fixture;
    fixture.available = false;
    CHECK(!fixture.sprays.Open());
    CHECK(fixture.notices.back().find("Du bist kein Polizist.") == std::string::npos);
    fixture.available = true;
    fixture.position.interior = 1;
    CHECK(!fixture.sprays.Open());
    CHECK(fixture.notices.back().find("Innenr") != std::string::npos);
    fixture.position.interior = 0;
    CHECK(fixture.sprays.Open());
    fixture.available = false;
    fixture.sprays.Update();
    CHECK(fixture.sprays.Rows().empty());
    CHECK(!fixture.sprays.Select(14));
    fixture.available = true;
    fixture.position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!fixture.sprays.Select(14));
    CHECK(fixture.gps.ActiveDestination() == nullptr);
    CHECK(keybinder::SpraysNavigator::BuildRows(fixture.position).empty());
    fixture.position = {1966.9f, -1174.7f, 20.0f, 0};
    CHECK(fixture.sprays.Select(14));
    CHECK(fixture.sprays.Open());
    fixture.position.interior = 1;
    fixture.sprays.Update();
    CHECK(!fixture.sprays.IsOpen());
    fixture.gps.Update();
    CHECK(fixture.renderedId == 0 && fixture.gps.ActiveDestination() != nullptr);
    fixture.position.interior = 0;
    fixture.gps.Update();
    CHECK(fixture.renderedId == (keybinder::GpsManager::kTemporaryIdMask | 14));
    fixture.sprays.Shutdown();
    CHECK(fixture.gps.ActiveDestination() == nullptr && fixture.renderedId == 0);
}

} // namespace

int RunSpraysNavigatorTests() {
    checks = failures = 0;
    CompleteCatalogHasStableIdsAndUsesAllSuppliedPositions();
    SortingShowsRealMeterDistancesAndThreeCompletePages();
    SelectionUsesPointIdentityAndSharedGpsCheckpointWithoutSaving();
    NavigationFitsAlongsideTwentySavedGpsEntriesAndArrivesExactlyOnce();
    PoliceAccessAndSessionChangesNeverCancelAnotherGpsDestination();
    MissingPositionsAndInteriorsNeverProduceMisleadingPoliceErrors();
    std::printf("SpraysNavigatorTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
