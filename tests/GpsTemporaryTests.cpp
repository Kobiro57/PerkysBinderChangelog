#include "../include/GpsManager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] GpsTemporary: %s (line %d)\n", #value, __LINE__); } } while (0)

unsigned int nextFixture = 0;
struct Fixture {
    std::filesystem::path directory, file;
    keybinder::GpsPosition position{100, 200, 30, 0};
    bool available = true;
    std::vector<std::string> messages;
    std::optional<keybinder::GpsEntry> rendered;
    keybinder::GpsManager gps;

    Fixture() {
        directory = std::filesystem::path("build/gps-temporary-tests") /
            (std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(++nextFixture));
        std::filesystem::create_directories(directory);
        file = directory / "gps.xml";
        gps.Initialize(file.string(), [&](const auto& message) { messages.push_back(message); },
            [&](auto& out) { out = position; return available; },
            [&](const keybinder::GpsEntry* destination) {
                if (destination) rendered = *destination;
                else rendered.reset();
            });
    }
    ~Fixture() { gps.Shutdown(); }
    std::string Xml() const {
        std::ifstream input(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    std::size_t Arrivals() const {
        return std::count_if(messages.begin(), messages.end(), [](const auto& message) {
            return message.find("Du hast dein Ziel erreicht:") != std::string::npos;
        });
    }
};

keybinder::GpsEntry Temporary(unsigned int id = 1) {
    return {keybinder::GpsManager::kTemporaryIdMask | id,
        "Spray 001 - Vinewood", {944.3f, -985.8f, 39.3f, 0}};
}

void TemporaryRouteWorksWithTwentyBookmarksWithoutWritingXml() {
    Fixture fixture;
    for (int index = 0; index < 20; ++index) {
        fixture.position.x = static_cast<float>(index * 10);
        CHECK(fixture.gps.Set("Saved " + std::to_string(index + 1)));
    }
    CHECK(fixture.gps.Entries().size() == 20);
    const auto before = fixture.Xml();
    CHECK(!before.empty());
    const auto target = Temporary();
    CHECK(fixture.gps.NavigateTemporary(target));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == target.id);
    CHECK(fixture.rendered && fixture.rendered->position.x == target.position.x);
    CHECK(fixture.gps.Entries().size() == 20);
    CHECK(fixture.Xml() == before);
    CHECK(!fixture.gps.Set("Twenty-first bookmark"));
    CHECK(fixture.Xml() == before);
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == target.id);
    fixture.gps.StopNavigation();
    CHECK(!fixture.gps.ActiveDestination() && !fixture.rendered);
    CHECK(fixture.Xml() == before);

    keybinder::GpsManager loaded;
    loaded.Initialize(fixture.file.string(), {}, {});
    CHECK(loaded.Entries().size() == 20);
    CHECK(!loaded.ActiveDestination());
    for (const auto& entry : loaded.Entries()) CHECK((entry.id & keybinder::GpsManager::kTemporaryIdMask) == 0);
    loaded.Shutdown();
}

void SavedAndTemporaryRoutesSwitchAndDeletionDoesNotAffectTheOtherOwner() {
    Fixture fixture;
    CHECK(fixture.gps.Set("Saved A"));
    const auto savedA = fixture.gps.Entries().back().id;
    fixture.position.x += 20;
    CHECK(fixture.gps.Set("Saved B"));
    const auto savedB = fixture.gps.Entries().back().id;
    CHECK(fixture.gps.Navigate(savedA));
    CHECK(fixture.rendered && fixture.rendered->id == savedA);
    CHECK(fixture.gps.NavigateTemporary(Temporary(1)));
    CHECK(fixture.rendered && fixture.rendered->id == Temporary(1).id);
    CHECK(fixture.gps.Remove(savedA));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == Temporary(1).id);
    CHECK(!fixture.gps.Remove(Temporary(1).id));
    CHECK(fixture.gps.RemoveAll());
    CHECK(fixture.gps.Entries().empty());
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == Temporary(1).id);
    CHECK(!fixture.gps.Navigate(savedB));
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == Temporary(1).id);
    CHECK(fixture.gps.Set("Saved C"));
    const auto savedC = fixture.gps.Entries().back().id;
    CHECK(fixture.gps.Navigate(savedC));
    CHECK(fixture.rendered && fixture.rendered->id == savedC);
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == savedC);
    CHECK(fixture.gps.Remove(savedC));
    CHECK(!fixture.gps.ActiveDestination() && !fixture.rendered);

    auto second = Temporary(2);
    second.name = "Spray 002 - Temple";
    second.position = {1072.9f, -1012.8f, 35.5f, 0};
    CHECK(fixture.gps.NavigateTemporary(Temporary(1)));
    CHECK(fixture.gps.NavigateTemporary(second));
    CHECK(fixture.rendered && fixture.rendered->id == second.id);
    CHECK(fixture.rendered && fixture.rendered->position.x == second.position.x);
    second.name = "Caller mutated its entry";
    second.position.x = 0;
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->name == "Spray 002 - Temple");
    CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->position.x == 1072.9f);
}

void InvalidTemporaryTargetsPreserveTheCurrentRoute() {
    Fixture fixture;
    const auto original = Temporary();
    CHECK(fixture.gps.NavigateTemporary(original));
    const auto baseline = fixture.messages.size();
    auto noNamespace = Temporary(2); noNamespace.id = 2;
    auto empty = Temporary(2); empty.name = " \t ";
    auto control = Temporary(2); control.name = "Target\n/r injected";
    auto tooLong = Temporary(2); tooLong.name = std::string(65, 'a');
    auto nan = Temporary(2); nan.position.x = std::numeric_limits<float>::quiet_NaN();
    auto infinity = Temporary(2); infinity.position.y = std::numeric_limits<float>::infinity();
    auto excessive = Temporary(2); excessive.position.z = 1000001.0f;
    auto negativeInterior = Temporary(2); negativeInterior.position.interior = -1;
    auto excessiveInterior = Temporary(2); excessiveInterior.position.interior = 256;
    for (const auto& invalid : {noNamespace, empty, control, tooLong, nan, infinity, excessive, negativeInterior, excessiveInterior}) {
        CHECK(!fixture.gps.NavigateTemporary(invalid));
        CHECK(fixture.gps.ActiveDestination() && fixture.gps.ActiveDestination()->id == original.id);
        CHECK(fixture.rendered && fixture.rendered->id == original.id);
    }
    CHECK(fixture.messages.size() == baseline);
}

void TemporaryArrivalUsesTheExistingCheckpointLifecycleExactlyOnce() {
    Fixture fixture;
    const auto target = Temporary();
    CHECK(fixture.gps.NavigateTemporary(target));
    fixture.gps.Update();
    CHECK(fixture.Arrivals() == 0);
    fixture.available = false;
    fixture.gps.Update();
    CHECK(!fixture.rendered && fixture.gps.ActiveDestination());
    fixture.available = true;
    fixture.position = target.position;
    fixture.position.interior = 1;
    fixture.gps.Update();
    CHECK(!fixture.rendered && fixture.gps.ActiveDestination());
    CHECK(fixture.Arrivals() == 0);
    fixture.position.interior = 0;
    fixture.gps.Update();
    CHECK(!fixture.rendered && !fixture.gps.ActiveDestination());
    CHECK(fixture.Arrivals() == 1);
    for (int index = 0; index < 10; ++index) fixture.gps.Update();
    CHECK(fixture.Arrivals() == 1);

    // Selecting a spray while already standing at it follows the GPS rule:
    // display its marker until the player leaves and comes back.
    CHECK(fixture.gps.NavigateTemporary(target));
    for (int index = 0; index < 5; ++index) fixture.gps.Update();
    CHECK(fixture.gps.ActiveDestination() && fixture.rendered);
    CHECK(fixture.Arrivals() == 1);
    fixture.position.x += 5;
    fixture.gps.Update();
    fixture.position = target.position;
    fixture.gps.Update();
    CHECK(!fixture.gps.ActiveDestination() && !fixture.rendered);
    CHECK(fixture.Arrivals() == 2);
    fixture.gps.Update();
    CHECK(fixture.Arrivals() == 2);
}

void ShutdownAndReinitializationDiscardTemporaryTargets() {
    Fixture fixture;
    CHECK(fixture.gps.NavigateTemporary(Temporary()));
    fixture.gps.Shutdown();
    CHECK(!fixture.gps.ActiveDestination() && !fixture.rendered);
    CHECK(fixture.gps.NavigateTemporary(Temporary()));
    fixture.gps.Initialize(fixture.file.string(), {}, {});
    CHECK(!fixture.gps.ActiveDestination() && !fixture.rendered);
    CHECK(fixture.gps.Entries().empty());
}
} // namespace

int RunGpsTemporaryTests() {
    checks = failures = 0;
    TemporaryRouteWorksWithTwentyBookmarksWithoutWritingXml();
    SavedAndTemporaryRoutesSwitchAndDeletionDoesNotAffectTheOtherOwner();
    InvalidTemporaryTargetsPreserveTheCurrentRoute();
    TemporaryArrivalUsesTheExistingCheckpointLifecycleExactlyOnce();
    ShutdownAndReinitializationDiscardTemporaryTargets();
    std::printf("GpsTemporaryTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
