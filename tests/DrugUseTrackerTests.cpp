#include "../include/DrugUseTracker.h"
#include <chrono>
#include <cstdio>

namespace {
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("[FAIL] DrugUseTracker: %s at %d\n", #x, __LINE__); } } while (0)
using Tracker = keybinder::DrugUseTracker;
using namespace std::chrono;
const Tracker::TimePoint start = Tracker::TimePoint{} + hours(10);

void OnlyExactOwnConsumptionStartsTimers() {
    Tracker tracker;
    CHECK(!tracker.ObserveChat("Other nimmt Kokain zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("NotLocal nimmt Kokain zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("LocalOther nimmt Kokain zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("Local: Local nimmt Kokain zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("Other: Local nimmt Kokain zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("[f] Local nimmt Kokain zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("Local nimmt Kokain zu sich. irgendwas", "Local", start));
    CHECK(!tracker.ObserveChat("Local nimmt unbekannt zu sich.", "Local", start));
    CHECK(!tracker.ObserveChat("Local nimmt Kokain zu sich.", "", start));
    CHECK(tracker.Snapshot(start).empty());
    CHECK(tracker.ObserveChat("Local nimmt Kokain zu sich.", "Local", start));
    CHECK(tracker.Snapshot(start).size() == 1);
    CHECK(tracker.Snapshot(start)[0].displayName == "Kokain");
    CHECK(tracker.Snapshot(start)[0].cooldownSeconds == 120);
    CHECK(tracker.Snapshot(start)[0].aftereffectSeconds == 120);
    CHECK(!tracker.ObserveChat("Local nimmt Kokain zu sich.", "Different_Name", start + seconds(2)));
    tracker.Reset();
    CHECK(tracker.ObserveChat("Different_Name nimmt Heroin zu sich.", "Different_Name", start));
    CHECK(tracker.Snapshot(start)[0].displayName == "Heroin");
    tracker.Reset();
    CHECK(tracker.ObserveChat("[22:13:03] {33CCFF}Different_Name{FFFFFF} nimmt Heroin zu sich.\r\n", "Different_Name", start));
    CHECK(!tracker.ObserveChat("{NOHEXX}Different_Name nimmt Heroin zu sich.", "Different_Name", start));
}

void EveryConfiguredDrugHasExactParallelDurations() {
    struct Expected { const char* name; int cooldown; int aftereffect; };
    for (const Expected expected : {
        Expected{"Kokain",120,120}, {"Heroin",120,60}, {"Speed",180,180},
        {"Ecstasy",180,60}, {"Cannabis",0,40}, {"Adrenalin",45,0}, {"Morphium",45,45}}) {
        Tracker tracker;
        CHECK(tracker.ObserveChat(std::string("Player nimmt ") + expected.name + " zu sich.", "Player", start));
        auto snapshot = tracker.Snapshot(start);
        CHECK(snapshot.size() == 1);
        CHECK(snapshot[0].displayName == expected.name);
        CHECK(snapshot[0].cooldownSeconds == expected.cooldown);
        CHECK(snapshot[0].aftereffectSeconds == expected.aftereffect);
        CHECK(snapshot[0].cooldownDuration == expected.cooldown);
        CHECK(snapshot[0].aftereffectDuration == expected.aftereffect);
        snapshot = tracker.Snapshot(start + milliseconds(1500));
        CHECK(snapshot[0].cooldownSeconds == (expected.cooldown ? expected.cooldown - 1 : 0));
        CHECK(snapshot[0].aftereffectSeconds == (expected.aftereffect ? expected.aftereffect - 1 : 0));
        CHECK(tracker.TakeReadyDrugs(start + seconds(500)).size() == (expected.cooldown ? 1u : 0u));
        CHECK(tracker.TakeReadyDrugs(start + seconds(501)).empty());
        CHECK(tracker.Snapshot(start + seconds(500)).empty());
    }
    Tracker alias;
    CHECK(alias.ObserveChat("Player nimmt Ectasy zu sich.", "Player", start));
    CHECK(alias.Snapshot(start)[0].displayName == "Ecstasy");
    CHECK(alias.Snapshot(start)[0].cooldownSeconds == 180);
    CHECK(alias.Snapshot(start)[0].aftereffectSeconds == 60);
}

void DeadlinesAreRoundedUpAndCompletionIsNotConsumedByRendering() {
    Tracker tracker;
    CHECK(tracker.ObserveChat("Player nimmt Heroin zu sich.", "Player", start));
    CHECK(tracker.Snapshot(start + milliseconds(1))[0].cooldownSeconds == 120);
    CHECK(tracker.Snapshot(start + milliseconds(999))[0].cooldownSeconds == 120);
    CHECK(tracker.Snapshot(start + seconds(1))[0].cooldownSeconds == 119);
    CHECK(tracker.Snapshot(start + seconds(60))[0].aftereffectSeconds == 0);
    CHECK(tracker.Snapshot(start + seconds(60))[0].cooldownSeconds == 60);
    CHECK(tracker.Snapshot(start + milliseconds(119999))[0].cooldownSeconds == 1);
    CHECK(tracker.TakeReadyDrugs(start + milliseconds(119999)).empty());
    CHECK(tracker.Snapshot(start + seconds(120)).empty());
    CHECK(tracker.Snapshot(start + seconds(121)).empty());
    auto ready = tracker.TakeReadyDrugs(start + seconds(121));
    CHECK(ready.size() == 1 && ready[0] == "Heroin");
    CHECK(tracker.TakeReadyDrugs(start + seconds(121)).empty());
    CHECK(tracker.TakeReadyDrugs(start + hours(10)).empty());
    tracker.Reset();
    CHECK(tracker.ObserveChat("Player nimmt Kokain zu sich.", "Player", start));
    CHECK(tracker.Snapshot(start - seconds(1))[0].cooldownSeconds == 120);
    CHECK(tracker.TakeReadyDrugs(start - seconds(1)).empty());
}

void DuplicateHooksAndSeparateDrugsDoNotResetEachOther() {
    Tracker tracker;
    CHECK(tracker.ObserveChat("Player nimmt Speed zu sich.", "Player", start));
    CHECK(!tracker.ObserveChat("Player nimmt Speed zu sich.", "Player", start));
    CHECK(!tracker.ObserveChat("{FFFFFF}Player nimmt Speed zu sich.", "Player", start + milliseconds(999)));
    CHECK(tracker.ObserveChat("Player nimmt Morphium zu sich.", "Player", start + seconds(30)));
    CHECK(tracker.Snapshot(start + seconds(30)).size() == 2);
    CHECK(tracker.Snapshot(start + seconds(30))[0].cooldownSeconds == 150);
    CHECK(tracker.Snapshot(start + seconds(30))[1].cooldownSeconds == 45);
    CHECK(tracker.ObserveChat("Player nimmt Speed zu sich.", "Player", start + seconds(60)));
    CHECK(tracker.Snapshot(start + seconds(60))[0].cooldownSeconds == 180);
    CHECK(tracker.Snapshot(start + seconds(60))[1].cooldownSeconds == 15);
    auto ready = tracker.TakeReadyDrugs(start + seconds(75));
    CHECK(ready.size() == 1 && ready[0] == "Morphium");
    CHECK(tracker.Snapshot(start + seconds(75)).size() == 1);
    ready = tracker.TakeReadyDrugs(start + seconds(240));
    CHECK(ready.size() == 1 && ready[0] == "Speed");
    CHECK(tracker.TakeReadyDrugs(start + seconds(240)).empty());
    CHECK(tracker.ObserveChat("Player nimmt Adrenalin zu sich.", "Player", start + seconds(250)));
    tracker.Reset();
    CHECK(tracker.Snapshot(start + seconds(251)).empty());
    CHECK(tracker.TakeReadyDrugs(start + seconds(500)).empty());
}
}

int RunDrugUseTrackerTests() {
    checks = failures = 0;
    OnlyExactOwnConsumptionStartsTimers();
    EveryConfiguredDrugHasExactParallelDurations();
    DeadlinesAreRoundedUpAndCompletionIsNotConsumedByRendering();
    DuplicateHooksAndSeparateDrugsDoNotResetEachOther();
    std::printf("DrugUseTrackerTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
