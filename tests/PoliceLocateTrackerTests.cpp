#include "../include/PoliceLocateTracker.h"

#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("[FAIL] PoliceLocateTracker: %s at %d\n", #x, __LINE__); } } while (0)
using Tracker = keybinder::PoliceLocateTracker;
using Marker = keybinder::LocateCheckpoint;
constexpr std::uint32_t start = 100000;
const Marker targetMarker{1927.8734f, 168.8767f, 40.1172f, 4.0f};
const char* success = "Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.";
const char* locate = "Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden.";
const std::string expected = "/r [ORTUNG] Gesuchter Lesch wurde zuletzt in [Palomino Creek] geortet.";
std::string District(float, float, float) { return "Palomino Creek"; }

Tracker Authorized() {
    Tracker tracker;
    tracker.SetSession(true, "server:7777/Officer");
    return tracker;
}

void MarkerAndSuccessAreBothRequiredInEitherOrder() {
    auto tracker = Authorized();
    CHECK(!tracker.ObserveCheckpoint(targetMarker, start));
    CHECK(!tracker.ObserveChat(success, start));
    CHECK(tracker.TakeRadioMessage(start, District).empty());
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.HasPendingLocate());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 1000));
    CHECK(!tracker.ObserveChat("Detektiv: Du hast die Fähigkeit auf Stufe 191 verbessert.", start + 1000));
    CHECK(tracker.TakeRadioMessage(start + 1000, District).empty());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
    CHECK(tracker.TakeRadioMessage(start + 30000, District).empty());
    CHECK(tracker.ObserveChat(success, start + 30001));
    int calls = 0;
    const auto message = tracker.TakeRadioMessage(start + 30002, [&](float x, float y, float z) {
        ++calls;
        CHECK(x == targetMarker.x && y == targetMarker.y && z == targetMarker.z);
        return "Palomino Creek";
    });
    CHECK(message == expected);
    CHECK(calls == 1);
    CHECK(!tracker.HasPendingLocate());
    CHECK(tracker.TakeRadioMessage(start + 30003, District).empty());
    CHECK(!tracker.ObserveChat(success, start + 30003));
    CHECK(!tracker.ObserveCheckpoint(targetMarker, start + 30003));

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(tracker.TakeRadioMessage(start + 30000, District).empty());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30005));
    CHECK(tracker.TakeRadioMessage(start + 30006, District) == expected);
}

void OldMarkersAndUnrelatedChatNeverSupplyTheResult() {
    auto tracker = Authorized();
    CHECK(!tracker.ObserveCheckpoint(targetMarker, start - 10));
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 1000));
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(tracker.TakeRadioMessage(start + 30001, District).empty());
    CHECK(!tracker.ObserveChat("Detektiv: Du hast die Fähigkeit auf Stufe 191 verbessert.", start + 30002));
    CHECK(tracker.TakeRadioMessage(start + 30003, District).empty());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30004));
    CHECK(tracker.TakeRadioMessage(start + 30005, District) == expected);

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 28000));
    CHECK(tracker.ObserveChat(success, start + 39001));
    CHECK(tracker.TakeRadioMessage(start + 39002, District).empty());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 39003));
    CHECK(tracker.TakeRadioMessage(start + 39004, District) == expected);

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
    tracker.ObserveCheckpointDisabled();
    CHECK(tracker.ObserveChat(success, start + 30001));
    CHECK(tracker.TakeRadioMessage(start + 30002, District).empty());
}

void SessionAndRoleChangesDiscardEveryPendingResult() {
    for (int change = 0; change < 5; ++change) {
        auto tracker = Authorized();
        CHECK(tracker.ObserveChat(locate, start));
        CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
        if (change == 0) tracker.SetSession(false, "server:7777/Officer");
        if (change == 1) tracker.SetSession(true, "server:7777/OtherOfficer");
        if (change == 2) tracker.SetSession(true, "other-server:7777/Officer");
        if (change == 3) tracker.SetSession(true, "");
        if (change == 4) tracker.Reset();
        CHECK(!tracker.HasPendingLocate());
        CHECK(!tracker.ObserveChat(success, start + 30001));
        CHECK(tracker.TakeRadioMessage(start + 30002, District).empty());
        tracker.SetSession(true, "server:7777/Officer");
        CHECK(!tracker.ObserveCheckpoint(targetMarker, start + 30003));
        CHECK(!tracker.ObserveChat(success, start + 30004));
        CHECK(tracker.TakeRadioMessage(start + 30005, District).empty());
    }
    Tracker denied;
    CHECK(!denied.ObserveChat(locate, start));
    CHECK(!denied.ObserveCheckpoint(targetMarker, start + 30000));
    denied.SetSession(false, "server:7777/Civilian");
    CHECK(!denied.ObserveChat(locate, start));
    CHECK(!denied.ObserveChat(success, start + 30000));

    auto tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    tracker.SetSession(true, "server:7777/Officer");
    CHECK(tracker.HasPendingLocate());
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30001));
    CHECK(tracker.TakeRadioMessage(start + 30002, District) == expected);
}

void DeadlinesHandleStallsDuplicatesAndCounterWrap() {
    auto tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(!tracker.ObserveChat(locate, start + 999));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(!tracker.ObserveChat(success, start + 30001));
    CHECK(tracker.TakeRadioMessage(start + 30002, District) == expected);

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(!tracker.ObserveCheckpoint(targetMarker, start + Tracker::kOperationTimeoutMs + 1));
    CHECK(!tracker.HasPendingLocate());
    CHECK(tracker.TakeRadioMessage(start + Tracker::kOperationTimeoutMs + 1, District).empty());

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(!tracker.ObserveCheckpoint(targetMarker, start + 30000 + Tracker::kCompletionWindowMs + 1));
    CHECK(!tracker.HasPendingLocate());

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
    CHECK(tracker.ObserveChat(success, start + 35000));
    CHECK(tracker.TakeRadioMessage(start + 35001, District) == expected);

    tracker = Authorized();
    const std::uint32_t wrapStart = std::numeric_limits<std::uint32_t>::max() - 1000;
    CHECK(tracker.ObserveChat(locate, wrapStart));
    CHECK(tracker.ObserveChat(success, wrapStart + 30000));
    CHECK(tracker.ObserveCheckpoint(targetMarker, wrapStart + 30001));
    CHECK(tracker.TakeRadioMessage(wrapStart + 30002, District) == expected);
}

void NamesColorCodesAndExactServerEnvelopesAreValidated() {
    for (const char* invalid : {
        "Other: Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden.",
        "[r] Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden.",
        "Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 0 Sekunden.",
        "Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 121 Sekunden.",
        "Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 3x Sekunden.",
        "Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden. Zusatz",
        "Detektiv: Das Handy und die Ortung von Bad Name wurde gestartet und dauert 30 Sekunden.",
        "Detektiv: Das Handy und die Ortung von Evil\n/r injected wurde gestartet und dauert 30 Sekunden.",
        "Detektiv: Das Handy und die Ortung von wurde gestartet und dauert 30 Sekunden.",
        "Detektiv: Das Handy und die Ortung von AAAAAAAAAAAAAAAAAAAAAAAAA wurde gestartet und dauert 30 Sekunden."}) {
        auto tracker = Authorized();
        CHECK(!tracker.ObserveChat(invalid, start));
        CHECK(!tracker.HasPendingLocate());
    }
    auto tracker = Authorized();
    CHECK(tracker.ObserveChat("[13:20:30] {33CCFF}Detektiv: {FFFFFF}Das Handy und die Ortung von [RCB]Lesch wurde gestartet und dauert 30 Sekunden.\r\n", start));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
    CHECK(!tracker.ObserveChat("Other: Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.", start + 30001));
    CHECK(tracker.TakeRadioMessage(start + 30001, District).empty());
    CHECK(tracker.ObserveChat("{33CCFF}Detektiv: {FFFFFF}Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.", start + 30002));
    CHECK(tracker.TakeRadioMessage(start + 30003, District)
        == "/r [ORTUNG] Gesuchter [RCB]Lesch wurde zuletzt in [Palomino Creek] geortet.");

    tracker = Authorized();
    CHECK(tracker.ObserveChat("Detektiv: Das Handy und die Ortung von New_Target wurde gestartet und dauert 10 Sekunden.", start));
    CHECK(tracker.ObserveChat(success, start + 10000));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 10001));
    CHECK(tracker.TakeRadioMessage(start + 10002, District)
        == "/r [ORTUNG] Gesuchter New_Target wurde zuletzt in [Palomino Creek] geortet.");
}

void NewOperationAndInvalidDistrictsNeverRecycleOldData() {
    auto tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 30000));
    CHECK(tracker.ObserveChat("Detektiv: Das Handy und die Ortung von Next_Target wurde gestartet und dauert 30 Sekunden.", start + 30001));
    CHECK(!tracker.ObserveChat("Detektiv: Du hast die Fähigkeit auf Stufe 191 verbessert.", start + 30002));
    CHECK(tracker.TakeRadioMessage(start + 30003, District).empty());
    CHECK(tracker.ObserveChat(success, start + 60001));
    CHECK(tracker.TakeRadioMessage(start + 60002, District).empty());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 60003));
    CHECK(tracker.TakeRadioMessage(start + 60004, {}).empty());
    for (const std::string badDistrict : {"", "Unknown", "unknown", "\r\n", "Market\n/r injected", "{FF0000}Market", "[Market]"}) {
        CHECK(tracker.TakeRadioMessage(start + 60004, [&](float, float, float) { return badDistrict; }).empty());
    }
    CHECK(tracker.TakeRadioMessage(start + 60005, [](float, float, float) -> std::string { throw std::runtime_error("test"); }).empty());
    CHECK(tracker.TakeRadioMessage(start + 60006, [](float, float, float) { return " Market "; })
        == "/r [ORTUNG] Gesuchter Next_Target wurde zuletzt in [Market] geortet.");

    for (const Marker invalid : {
        Marker{std::numeric_limits<float>::quiet_NaN(), 1, 2, 3},
        Marker{1, std::numeric_limits<float>::infinity(), 2, 3},
        Marker{1, 2, -std::numeric_limits<float>::infinity(), 3},
        Marker{1, 2, 3, std::numeric_limits<float>::quiet_NaN()},
        Marker{1, 2, 3, 0}, Marker{1, 2, 3, -1}, Marker{1, 2, 3, 1001}, Marker{20001, 2, 3, 4}}) {
        tracker = Authorized();
        CHECK(tracker.ObserveChat(locate, start));
        CHECK(!tracker.ObserveCheckpoint(invalid, start + 30000));
        CHECK(tracker.ObserveChat(success, start + 30001));
        CHECK(tracker.TakeRadioMessage(start + 30002, District).empty());
    }
}

void SuccessIsAuthoritativeAndCurrentCheckpointWaitIsBounded() {
    auto tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(!tracker.ReadyForCurrentCheckpoint(start + 1000));
    CHECK(tracker.ObserveChat(success, start + 1000)); // Actual success, even before announced 30 seconds.
    CHECK(!tracker.ReadyForCurrentCheckpoint(start + 1149));
    CHECK(tracker.ReadyForCurrentCheckpoint(start + 1150));
    CHECK(tracker.TakeRadioMessage(start + 1150, District).empty());
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 1150)); // Native memory, no RPC requirement.
    CHECK(tracker.TakeRadioMessage(start + 1151, District) == expected);
    CHECK(!tracker.ReadyForCurrentCheckpoint(start + 1152));
    CHECK(!tracker.ObserveChat(success, start + 1200));

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(tracker.ReadyForCurrentCheckpoint(start + 38000)); // Late marker is still useful.
    CHECK(tracker.ObserveCheckpoint(targetMarker, start + 38000));
    CHECK(tracker.TakeRadioMessage(start + 38001, District) == expected);

    tracker = Authorized();
    CHECK(tracker.ObserveChat(locate, start));
    CHECK(tracker.ObserveChat(success, start + 30000));
    CHECK(!tracker.ReadyForCurrentCheckpoint(start + 30000 + Tracker::kCompletionWindowMs + 1));
    CHECK(!tracker.HasPendingLocate());
}

} // namespace

int RunPoliceLocateTrackerTests() {
    checks = failures = 0;
    MarkerAndSuccessAreBothRequiredInEitherOrder();
    OldMarkersAndUnrelatedChatNeverSupplyTheResult();
    SessionAndRoleChangesDiscardEveryPendingResult();
    DeadlinesHandleStallsDuplicatesAndCounterWrap();
    NamesColorCodesAndExactServerEnvelopesAreValidated();
    NewOperationAndInvalidDistrictsNeverRecycleOldData();
    SuccessIsAuthoritativeAndCurrentCheckpointWaitIsBounded();
    std::printf("PoliceLocateTrackerTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
