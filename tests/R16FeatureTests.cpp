#define KEYBINDER_TESTING
#include "../include/StaminaGauge.h"
#include "../include/GpsManager.h"
#include "../include/JobParser.h"
#include "../include/SettingsManager.h"
#include "../include/ForumBrowserProtocol.h"
#include <cstdio>
#include <filesystem>
#include <limits>

namespace {
int checks = 0, failures = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { ++failures; std::printf("[FAIL] R16: %s, line %d\n", #condition, __LINE__); } } while (0)

void StaminaUsesTheRealReserveAndMeasuredRecovery() {
    keybinder::StaminaGauge gauge;
    auto state = gauge.Observe({true, false, 1000, 1000}, 0.1f);
    CHECK(state.available && state.percent == 100 && state.status == "Voll");
    state = gauge.Observe({true, false, 425, 1000}, 0.1f);
    CHECK(state.percent == 50 && state.status == "Verbrauch");
    state = gauge.Observe({true, false, -150, 1000}, 0.1f);
    CHECK(state.percent == 0);
    state = gauge.Observe({true, false, -100, 1000}, 0.1f);
    CHECK(state.percent > 0 && state.status == "Erholt sich");
    state = gauge.Observe({true, false, -100, 1000}, 0.1f);
    CHECK(state.status == "Erholt sich");
    state = gauge.Observe({true, false, -100, 1000}, 0.8f);
    CHECK(state.status == "Pause");
    CHECK(gauge.Observe({true, true, -100, 1000}, 0.1f).percent == 100);
    CHECK(!gauge.Observe({true, false, std::numeric_limits<float>::quiet_NaN(), 1000}, 0.1f).available);
    CHECK(!gauge.Observe({true, false, 10, 0}, 0.1f).available);
    CHECK(!gauge.Observe({}, 0.1f).available);
}

void EveryRegisteredJobRecognizesAkkordWithoutDoubleEarnings() {
    jobs::JobRegistry registry; jobs::JobRegistry::Snapshot snapshot;
    CHECK(registry.LoadEmbedded(snapshot));
    jobs::JobParser parser;
    for (const auto& job : snapshot.jobs) {
        jobs::JobChatEvent message;
        message.jobPrefix = job.name;
        message.normalizedLine = job.name + ": Dein Akkord-Anteil von 2.896$ wird dir zum n\xC3\xA4" "chsten Zahltag ausgezahlt";
        message.arrivalTime = 123456;
        auto events = parser.Parse(message, snapshot);
        CHECK(events.size() == 1);
        if (!events.empty()) {
            CHECK(events[0].jobId == job.id && events[0].type == "earning");
            CHECK(events[0].amount == 2896 && events[0].source == "chat_auto");
            CHECK(events[0].timestamp == 123456);
        }
    }
    jobs::JobChatEvent invalid;
    invalid.jobPrefix = snapshot.jobs.front().name;
    for (const auto* amount : {"2.89", "2147483648", "0", "-50", "garbage"}) {
        invalid.normalizedLine = invalid.jobPrefix + ": Dein Akkord-Anteil von " + amount + "$ wird dir zum n\xC3\xA4" "chsten Zahltag ausgezahlt";
        CHECK(parser.Parse(invalid, snapshot).empty());
    }
}

void GpsPersistsTwentyIndependentTargetsAndReportsArrivalOnce() {
    const auto base = std::filesystem::path("build/r16-gps-tests") / std::to_string(GetTickCount64());
    std::filesystem::create_directories(base);
    const auto path = (base / "gps.xml").string();
    keybinder::GpsPosition position{100, 200, 30, 0};
    std::vector<std::string> notices;
    keybinder::GpsManager gps;
    gps.Initialize(path, [&](const auto& text) { notices.push_back(text); }, [&](auto& result) { result = position; return true; });
    for (int i = 0; i < 20; ++i) { position.x = 100.0f + i * 10; CHECK(gps.Set("M\xC3\xBC" "lltonne " + std::to_string(i))); }
    CHECK(gps.Entries().size() == 20);
    CHECK(!gps.Set("Kein Platz"));
    keybinder::GpsManager loaded;
    loaded.Initialize(path, [&](const auto& text) { notices.push_back(text); }, [&](auto& result) { result = position; return true; });
    CHECK(loaded.Entries().size() == 20);
    CHECK(loaded.Entries()[0].name == "M\xC3\xBC" "lltonne 0");
    CHECK(loaded.ActiveDestination() == nullptr);
    const auto destination = loaded.Entries()[0];
    CHECK(loaded.Navigate(destination.id));
    position = destination.position; position.interior = 1;
    loaded.Update(); CHECK(loaded.ActiveDestination() != nullptr);
    position.interior = 0; position.z += 20;
    loaded.Update(); CHECK(loaded.ActiveDestination() != nullptr);
    position = destination.position;
    const auto before = notices.size(); loaded.Update(); loaded.Update();
    CHECK(notices.size() == before + 1);
    CHECK(loaded.ActiveDestination() == nullptr);
    CHECK(loaded.Navigate(loaded.Entries()[1].id));
    CHECK(loaded.Remove(loaded.Entries()[1].id));
    CHECK(loaded.ActiveDestination() == nullptr && loaded.Entries().size() == 19);
    CHECK(loaded.RemoveAll() && loaded.Entries().empty());
    keybinder::GpsManager empty;
    empty.Initialize(path, {}, {}); CHECK(empty.Entries().empty());
    keybinder::GpsManager failed;
    failed.Initialize((base / "missing/parent/gps.xml").string(), {}, [&](auto& result) { result = position; return true; });
    CHECK(!failed.Set("Unwriteable") && failed.Entries().empty());
}

void SettingsPreserveStaminaAndForumWindowGeometry() {
    const auto base = std::filesystem::path("build/r16-settings-tests") / std::to_string(GetTickCount64());
    std::filesystem::create_directories(base);
    keybinder::SettingsManager manager;
    auto& settings = manager.GetMutable();
    CHECK(!settings.online_stamina_overlay_enabled);
    settings.online_stamina_overlay_enabled = true;
    settings.online_stamina_overlay_anchor = "bottom_right";
    settings.online_stamina_overlay_offset_x = 42.5f;
    settings.online_stamina_overlay_offset_y = 83;
    settings.forum_window_x = 100; settings.forum_window_y = 200;
    settings.forum_window_width = 1200; settings.forum_window_height = 700;
    settings.forum_window_fullscreen = true;
    const auto serialized = manager.GetSettingsJson();
    keybinder::SettingsManager read; CHECK(read.LoadFromJson(serialized));
    CHECK(read.Get().online_stamina_overlay_enabled && read.Get().online_stamina_overlay_anchor == "bottom_right");
    CHECK(read.Get().online_stamina_overlay_offset_x == 42.5f && read.Get().online_stamina_overlay_offset_y == 83);
    CHECK(read.Get().forum_window_x == 100 && read.Get().forum_window_y == 200);
    CHECK(read.Get().forum_window_width == 1200 && read.Get().forum_window_height == 700 && read.Get().forum_window_fullscreen);
    CHECK(sizeof(keybinder::forum::Command) == 28 && offsetof(keybinder::forum::SharedState, parentWindow) == 64);
}
} // namespace
int RunR16FeatureTests() {
    checks = failures = 0;
    StaminaUsesTheRealReserveAndMeasuredRecovery();
    EveryRegisteredJobRecognizesAkkordWithoutDoubleEarnings();
    GpsPersistsTwentyIndependentTargetsAndReportsArrivalOnce();
    SettingsPreserveStaminaAndForumWindowGeometry();
    std::printf("R16FeatureTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
