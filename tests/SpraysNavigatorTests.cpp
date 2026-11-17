#include "../include/SpraysNavigator.h"
#include "../include/SpraySpots.h"
#include "../include/ThemeManager.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/imgui-1.92.7/imgui_internal.h"

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
        ReloadSprays();
    }
    ~Fixture() { sprays.Shutdown(); gps.Shutdown(); }

    void ReloadSprays() {
        sprays.Initialize(&gps, [&](const auto& text) { notices.push_back(text); },
            [&](auto& current) { current = position; return available; }, [&]() { return session; }, (path / "sprays.xml").string());
    }

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
    CHECK(fixture.gps.ActiveDestination()->name == "Los Santos International");
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
    CHECK(fixture.notices.back().find("Fraktion oder Gang") != std::string::npos);
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
    CHECK(fixture.notices.back().find("Fraktion oder Gang") == std::string::npos);
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

void CustomPointsCaptureTheCurrentPositionAndPersistChanges() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Remove(14));
    CHECK(fixture.sprays.Catalog().Find(14) == nullptr);
    fixture.position = {111.25f, 222.5f, 33.75f, 0}; // Changed after opening the list.
    CHECK(fixture.sprays.AddCurrent("Mein eigener Punkt"));
    CHECK(fixture.sprays.Catalog().Entries().size() == 100);
    const unsigned int customId = fixture.sprays.Catalog().Entries().back().id;
    CHECK(customId > 100);
    const auto* added = fixture.sprays.Catalog().Find(customId);
    CHECK(added && added->name == "Mein eigener Punkt");
    CHECK(added->position.x == 111.25f && added->position.y == 222.5f && added->position.z == 33.75f && added->position.interior == 0);
    CHECK(fixture.sprays.Rows().front().id == customId && fixture.sprays.Rows().front().distance == 0.0f);
    fixture.ReloadSprays();
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Catalog().Find(14) == nullptr);
    CHECK(fixture.sprays.Catalog().Find(customId) && fixture.sprays.Catalog().Find(customId)->name == "Mein eigener Punkt");
    CHECK(fixture.sprays.Select(customId));
    CHECK(fixture.renderedPosition.x == 111.25f && fixture.renderedPosition.y == 222.5f && fixture.renderedPosition.z == 33.75f);
    CHECK(fixture.gps.ActiveDestination()->name == "Mein eigener Punkt");
    CHECK(fixture.sprays.Open());
    const std::string maximumName(64, 'A');
    CHECK(fixture.sprays.AddCurrent(maximumName));
    const unsigned int longNameId = fixture.sprays.Catalog().Entries().back().id;
    CHECK(fixture.sprays.Select(longNameId));
    CHECK(fixture.gps.ActiveDestination()->name == maximumName); // No extra prefix breaks the shared GPS limit.
}

void DynamicPagesReachTenAndClampAfterDeletingTheLastPage() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    for (unsigned int index = 0; index < 240; ++index) {
        fixture.position = {1000.0f + index, -1500.0f, 20.0f, 0};
        CHECK(fixture.sprays.AddCurrent("Eigener Punkt " + std::to_string(index + 1)));
    }
    CHECK(fixture.sprays.Catalog().Entries().size() == 340);
    CHECK(fixture.sprays.Rows().size() == 340 && fixture.sprays.PageCount() == 10);
    fixture.sprays.SetPage(9);
    CHECK(!fixture.sprays.AddCurrent("Ein Punkt zu viel"));
    CHECK(fixture.sprays.Catalog().Entries().size() == 340 && fixture.sprays.Page() == 9);
    for (int index = 0; index < 34; ++index) {
        const auto id = fixture.sprays.Catalog().Entries().back().id;
        CHECK(fixture.sprays.Remove(id));
    }
    CHECK(fixture.sprays.Catalog().Entries().size() == 306);
    CHECK(fixture.sprays.PageCount() == 9 && fixture.sprays.Page() == 8);
    fixture.ReloadSprays();
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Catalog().Entries().size() == 306 && fixture.sprays.PageCount() == 9);
}

void EmptyCatalogStaysEmptyAfterReloadAndCanBeFilledAgain() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    const auto original = fixture.sprays.Catalog().Entries();
    for (const auto& point : original) CHECK(fixture.sprays.Remove(point.id));
    CHECK(fixture.sprays.Catalog().Entries().empty() && fixture.sprays.Rows().empty());
    CHECK(fixture.sprays.PageCount() == 0);
    fixture.ReloadSprays();
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Catalog().Entries().empty());
    CHECK(fixture.sprays.AddCurrent("Neustart"));
    CHECK(fixture.sprays.Catalog().Entries().size() == 1 && fixture.sprays.PageCount() == 1);
    CHECK(fixture.sprays.Rows().front().place == "Neustart");
}

void CatalogMutationsRespectAccessAndOnlyStopTheirOwnRemovedRoute() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Select(14));
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Remove(1));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == (keybinder::GpsManager::kTemporaryIdMask | 14));
    CHECK(fixture.sprays.AddCurrent("Weiterer Punkt"));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == (keybinder::GpsManager::kTemporaryIdMask | 14));
    CHECK(fixture.sprays.Remove(14));
    CHECK(fixture.gps.ActiveDestination() == nullptr && fixture.renderedId == 0);
    CHECK(fixture.gps.Set("Privates GPS"));
    const auto privateId = fixture.gps.Entries().front().id;
    CHECK(fixture.gps.Navigate(privateId));
    CHECK(fixture.sprays.Remove(2));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == privateId);
    const auto entries = fixture.sprays.Catalog().Entries().size();
    fixture.available = false;
    CHECK(!fixture.sprays.AddCurrent("Ohne Position"));
    CHECK(!fixture.sprays.Remove(3));
    fixture.available = true;
    fixture.position.interior = 1;
    CHECK(!fixture.sprays.AddCurrent("Innenraum"));
    CHECK(fixture.notices.back().find("Innenr") != std::string::npos);
    fixture.position.interior = 0;
    CHECK(fixture.sprays.Open());
    fixture.session.clear(); // Dead/disconnected/role-revoked sessions are all denied by the caller.
    CHECK(!fixture.sprays.AddCurrent("Nicht erlaubt"));
    CHECK(!fixture.sprays.Remove(3));
    CHECK(fixture.sprays.Catalog().Entries().size() == entries);
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == privateId);
    fixture.session = "127.0.0.1:7777/OtherOfficer";
    CHECK(fixture.sprays.Open());
    fixture.position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!fixture.sprays.AddCurrent("Ung\xC3\xBC" "ltig"));
    CHECK(!fixture.sprays.Remove(3));
    CHECK(fixture.sprays.Catalog().Entries().size() == entries);
}

void FailedDiskWritesDoNotDeletePointsOrStopTheirRoute() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.AddCurrent("Datei erstellen"));
    CHECK(fixture.sprays.Select(14));
    CHECK(fixture.sprays.Open());
    const auto path = fixture.path / "sprays.xml";
    std::ifstream beforeFile(path, std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(beforeFile)), {});
    beforeFile.close();
    CHECK(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE);
    const auto count = fixture.sprays.Catalog().Entries().size();
    CHECK(!fixture.sprays.Remove(14));
    CHECK(fixture.sprays.Catalog().Find(14) != nullptr);
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == (keybinder::GpsManager::kTemporaryIdMask | 14));
    CHECK(!fixture.sprays.AddCurrent("Nicht gespeichert"));
    CHECK(fixture.sprays.Catalog().Entries().size() == count);
    std::ifstream afterFile(path, std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(afterFile)), {});
    CHECK(before == after);
    CHECK(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE);
}

void WindowCanResizeAndKeepsFooterInsideTheVisibleBody() {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    const float previousThemeScale = keybinder::ThemeManager::Instance().GetAppliedScale();
    for (const auto display : {ImVec2(640, 480), ImVec2(1920, 1080)}) {
        for (float fontScale : {1.0f, 1.3f, 2.0f}) {
            auto* context = ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = display;
            io.DeltaTime = 1.0f / 60.0f;
            io.FontGlobalScale = fontScale;
            unsigned char* pixels = nullptr;
            int width = 0, height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            keybinder::ThemeManager::Instance().ApplyImGuiStyle(1.0f);
            Fixture fixture;
            CHECK(fixture.sprays.Open());
            CHECK(fixture.sprays.Select(14));
            CHECK(fixture.sprays.Open()); // Include both Close and navigation stop controls.
            const ImVec2 requested = display.x < 1000 ? ImVec2(480, 400) : ImVec2(display.x - 20, display.y - 20);
            for (int frame = 0; frame < 3; ++frame) {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
                fixture.sprays.Render();
                ImGui::Render();
                if (frame == 0) ImGui::SetWindowSize("Graffiti-Navigation##NLBinderSprays", requested, ImGuiCond_Always);
            }
            const auto* window = ImGui::FindWindowByName("Graffiti-Navigation##NLBinderSprays");
            CHECK(window != nullptr);
            if (window) {
                CHECK(window->Size.x <= display.x - 20.0f + 0.5f && window->Size.y <= display.y - 20.0f + 0.5f);
                CHECK(window->ScrollMax.y <= 0.5f); // The body/table scrolls while the footer remains fixed.
                CHECK(window->DC.CursorMaxPos.y <= window->InnerClipRect.Max.y + 0.5f);
                CHECK(window->DC.CursorMaxPos.x <= window->InnerClipRect.Max.x + 0.5f);
                if (display.x > 1000) CHECK(window->Size.x > 680.0f && window->Size.y > 790.0f);
            }
            fixture.sprays.Shutdown();
            keybinder::ThemeManager::Instance().ApplyImGuiStyle(previousThemeScale);
            ImGui::DestroyContext(context);
            ImGui::SetCurrentContext(previous);
        }
    }
}

void ReplacedNavigationWithTheSamePointIdBelongsToTheNewOwner() {
    Fixture fixture;
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Select(14));
    const auto replacement = *fixture.gps.ActiveDestination();
    const auto previous = fixture.gps.ActiveNavigationGeneration();
    CHECK(fixture.gps.NavigateTemporary(replacement));
    const auto generation = fixture.gps.ActiveNavigationGeneration();
    CHECK(generation != previous);
    CHECK(fixture.sprays.Open());
    CHECK(fixture.sprays.Remove(14));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveNavigationGeneration() == generation);
    fixture.sprays.Shutdown();
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveNavigationGeneration() == generation);
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
    CustomPointsCaptureTheCurrentPositionAndPersistChanges();
    DynamicPagesReachTenAndClampAfterDeletingTheLastPage();
    EmptyCatalogStaysEmptyAfterReloadAndCanBeFilledAgain();
    CatalogMutationsRespectAccessAndOnlyStopTheirOwnRemovedRoute();
    FailedDiskWritesDoNotDeletePointsOrStopTheirRoute();
    WindowCanResizeAndKeepsFooterInsideTheVisibleBody();
    ReplacedNavigationWithTheSamePointIdBelongsToTheNewOwner();
    std::printf("SpraysNavigatorTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
