/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Jobs Correction Tests - Manual add, delete, validation, aggregate refresh
*/
#define KEYBINDER_TESTING

#include "../include/JobTracker.h"
#include "../include/JobsUI.h"
#include "../include/JobsTypes.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

TestState g_state;

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

std::string MakeTempFilePath(const char* label) {
    char tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH) {
        return std::string(label) + ".json";
    }

    char filename[MAX_PATH] = {};
    std::snprintf(filename,
        sizeof(filename),
        "%sJobsCorrection_%s_%lu_%llu.json",
        tempPath,
        label,
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long long>(GetTickCount64()));
    return filename;
}

void RemoveIfExists(const std::string& filepath) {
    DeleteFileA(filepath.c_str());
}

std::time_t MakeLocalTimestamp(int year, int month, int day, int hour = 12, int minute = 0, int second = 0) {
    std::tm localTime = {};
    localTime.tm_year = year - 1900;
    localTime.tm_mon = month - 1;
    localTime.tm_mday = day;
    localTime.tm_hour = hour;
    localTime.tm_min = minute;
    localTime.tm_sec = second;
    localTime.tm_isdst = -1;
    return std::mktime(&localTime);
}

// ── AddManualEvent: valid earning ───────────────────────────────────────

bool TestAddManualEventEarning() {
    const std::string filepath = MakeTempFilePath("add_earning");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 30, 0);
    const std::string eventId = tracker.AddManualEvent("landwirt", "manual_earning", 250, 0, ts, "Nachtrag");

    ASSERT_TRUE(!eventId.empty());

    const auto& events = tracker.GetEvents();
    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].type, "manual_earning");
    ASSERT_EQ(events[0].amount, 250);
    ASSERT_EQ(events[0].level, 0);
    ASSERT_EQ(events[0].source, "manual_add");
    ASSERT_EQ(events[0].note, "Nachtrag");
    ASSERT_TRUE(!events[0].deleted);

    // Aggregates should reflect +250
    const jobs::JobWindowAggregates agg = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(agg.today, 250);
    ASSERT_EQ(agg.lifetime, 250);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── AddManualEvent: valid skill level ───────────────────────────────────

bool TestAddManualEventSkillLevel() {
    const std::string filepath = MakeTempFilePath("add_skill");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 15, 0, 0);
    const std::string eventId = tracker.AddManualEvent("landwirt", "manual_skill_level", 0, 5, ts, "Level-Korrektur");

    ASSERT_TRUE(!eventId.empty());

    const auto& events = tracker.GetEvents();
    ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(events[0].type, "manual_skill_level");
    ASSERT_EQ(events[0].amount, 0);
    ASSERT_EQ(events[0].level, 5);
    ASSERT_EQ(events[0].note, "Level-Korrektur");

    // Stats should reflect level
    jobs::JobStats stats;
    ASSERT_TRUE(tracker.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.highestLevel, 5);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── AddManualEvent: negative earning rejected ───────────────────────────

bool TestAddManualEventRejectsNegativeEarning() {
    const std::string filepath = MakeTempFilePath("neg_earning");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    const std::string eventId = tracker.AddManualEvent("landwirt", "manual_earning", -50, 0, ts, "");

    ASSERT_TRUE(eventId.empty());
    ASSERT_TRUE(tracker.GetEvents().empty());

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── AddManualEvent: negative level rejected ─────────────────────────────

bool TestAddManualEventRejectsNegativeLevel() {
    const std::string filepath = MakeTempFilePath("neg_level");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    const std::string eventId = tracker.AddManualEvent("landwirt", "manual_skill_level", 0, -3, ts, "");

    ASSERT_TRUE(eventId.empty());
    ASSERT_TRUE(tracker.GetEvents().empty());

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── AddManualEvent: empty job ID rejected ───────────────────────────────

bool TestAddManualEventRejectsEmptyJobId() {
    const std::string filepath = MakeTempFilePath("empty_job");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    const std::string eventId = tracker.AddManualEvent("", "manual_earning", 100, 0, ts, "");

    ASSERT_TRUE(eventId.empty());

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── AddManualEvent: unknown job ID rejected ────────────────────────────

bool TestAddManualEventRejectsUnknownJobId() {
    const std::string filepath = MakeTempFilePath("unknown_job");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    const std::string eventId = tracker.AddManualEvent("nonexistent_job", "manual_earning", 100, 0, ts, "");

    ASSERT_TRUE(eventId.empty());

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── DeleteEvent: soft-deletes and refreshes aggregates ──────────────────

bool TestDeleteEventSoftDeletesAndRefreshesAggregates() {
    const std::string filepath = MakeTempFilePath("delete");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    const std::string eventId = tracker.AddManualEvent("landwirt", "manual_earning", 300, 0, ts, "");
    ASSERT_TRUE(!eventId.empty());

    // Verify aggregate is +300
    jobs::JobWindowAggregates agg = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(agg.today, 300);
    ASSERT_EQ(agg.lifetime, 300);

    // Delete the event
    ASSERT_TRUE(tracker.DeleteEvent(eventId));

    // Event should be soft-deleted (excluded from non-deleted queries)
    std::vector<jobs::JobEvent> activeEvents = tracker.GetEventsForJob("landwirt", false);
    ASSERT_TRUE(activeEvents.empty());

    // Including deleted should still find it
    std::vector<jobs::JobEvent> allEvents = tracker.GetEventsForJob("landwirt", true);
    ASSERT_EQ(allEvents.size(), static_cast<std::size_t>(1));
    ASSERT_TRUE(allEvents[0].deleted);

    // Aggregates should now be 0
    agg = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(agg.today, 0);
    ASSERT_EQ(agg.lifetime, 0);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── DeleteEvent: non-existent event ID returns false ────────────────────

bool TestDeleteEventNonExistentReturnsFalse() {
    const std::string filepath = MakeTempFilePath("delete_nonexist");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    ASSERT_TRUE(!tracker.DeleteEvent("nonexistent_event_id"));

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── Manual earning updates global summaries ─────────────────────────────

bool TestManualEarningUpdatesGlobalSummaries() {
    const std::string filepath = MakeTempFilePath("global_summary");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    tracker.AddManualEvent("landwirt", "manual_earning", 250, 0, ts, "");

    // Aggregate for landwirt
    const jobs::JobWindowAggregates agg = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(agg.today, 250);

    // Global summary should include this
    int globalToday = 0;
    const auto& jobs = tracker.GetJobs();
    for (const auto& job : jobs) {
        globalToday += tracker.GetAggregates(job.id, MakeLocalTimestamp(2026, 4, 13, 18, 0, 0)).today;
    }
    ASSERT_EQ(globalToday, 250);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── Multiple corrections accumulate correctly ──────────────────────────

bool TestMultipleCorrectionsAccumulate() {
    const std::string filepath = MakeTempFilePath("multi_correct");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts1 = MakeLocalTimestamp(2026, 4, 13, 10, 0, 0);
    const std::time_t ts2 = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);

    tracker.AddManualEvent("landwirt", "manual_earning", 100, 0, ts1, "first");
    tracker.AddManualEvent("landwirt", "manual_earning", 200, 0, ts2, "second");

    const jobs::JobWindowAggregates agg = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(agg.today, 300);
    ASSERT_EQ(agg.lifetime, 300);

    const auto& events = tracker.GetEvents();
    ASSERT_EQ(events.size(), static_cast<std::size_t>(2));

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

// ── ValidateCorrection: UI validation logic ────────────────────────────

bool TestValidateCorrectionRejectsEmptyJob() {
    std::string error = keybinder::JobsUI::ValidateCorrection(0, 100, "");
    ASSERT_TRUE(!error.empty());

    error = keybinder::JobsUI::ValidateCorrection(1, 5, "");
    ASSERT_TRUE(!error.empty());
    return true;
}

bool TestValidateCorrectionRejectsNegativeEarning() {
    const std::string error = keybinder::JobsUI::ValidateCorrection(0, -50, "landwirt");
    ASSERT_TRUE(!error.empty());
    return true;
}

bool TestValidateCorrectionRejectsNegativeLevel() {
    const std::string error = keybinder::JobsUI::ValidateCorrection(1, -3, "landwirt");
    ASSERT_TRUE(!error.empty());
    return true;
}

bool TestValidateCorrectionAcceptsValidEarning() {
    const std::string error = keybinder::JobsUI::ValidateCorrection(0, 100, "landwirt");
    ASSERT_TRUE(error.empty());
    return true;
}

bool TestValidateCorrectionAcceptsValidLevel() {
    const std::string error = keybinder::JobsUI::ValidateCorrection(1, 5, "landwirt");
    ASSERT_TRUE(error.empty());
    return true;
}

bool TestValidateCorrectionAcceptsZeroValues() {
    // Zero earning is valid
    std::string error = keybinder::JobsUI::ValidateCorrection(0, 0, "landwirt");
    ASSERT_TRUE(error.empty());

    // Zero level is valid
    error = keybinder::JobsUI::ValidateCorrection(1, 0, "landwirt");
    ASSERT_TRUE(error.empty());
    return true;
}

// ── Format badge tests for manual types ────────────────────────────────

bool TestFormatBadgeForManualEarning() {
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("manual_earning"), "Verdienst");
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("manual_add"), "Manuell");
    return true;
}

bool TestFormatBadgeForManualSkillLevel() {
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("manual_skill_level"), "Level");
    return true;
}

bool TestFormatBadgeForChatAuto() {
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("chat_auto"), "Auto");
    return true;
}

// ── Note persistence round-trip ─────────────────────────────────────────

bool TestNotePersistenceRoundTrip() {
    const std::string filepath = MakeTempFilePath("note_persist");
    RemoveIfExists(filepath);

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());

        const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
        tracker.AddManualEvent("landwirt", "manual_earning", 500, 0, ts, "Korrektur für gestern");

        tracker.Shutdown();
    }

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());

        const auto& events = tracker.GetEvents();
        ASSERT_EQ(events.size(), static_cast<std::size_t>(1));
        ASSERT_EQ(events[0].note, "Korrektur für gestern");
        ASSERT_EQ(events[0].amount, 500);
        ASSERT_EQ(events[0].source, "manual_add");

        tracker.Shutdown();
    }

    RemoveIfExists(filepath);
    return true;
}

} // namespace

int RunJobsCorrectionTests() {
    g_state = {};

    std::printf("\n=== Jobs Correction Tests ===\n\n");

    RUN_TEST(TestAddManualEventEarning);
    RUN_TEST(TestAddManualEventSkillLevel);
    RUN_TEST(TestAddManualEventRejectsNegativeEarning);
    RUN_TEST(TestAddManualEventRejectsNegativeLevel);
    RUN_TEST(TestAddManualEventRejectsEmptyJobId);
    RUN_TEST(TestAddManualEventRejectsUnknownJobId);
    RUN_TEST(TestDeleteEventSoftDeletesAndRefreshesAggregates);
    RUN_TEST(TestDeleteEventNonExistentReturnsFalse);
    RUN_TEST(TestManualEarningUpdatesGlobalSummaries);
    RUN_TEST(TestMultipleCorrectionsAccumulate);
    RUN_TEST(TestValidateCorrectionRejectsEmptyJob);
    RUN_TEST(TestValidateCorrectionRejectsNegativeEarning);
    RUN_TEST(TestValidateCorrectionRejectsNegativeLevel);
    RUN_TEST(TestValidateCorrectionAcceptsValidEarning);
    RUN_TEST(TestValidateCorrectionAcceptsValidLevel);
    RUN_TEST(TestValidateCorrectionAcceptsZeroValues);
    RUN_TEST(TestFormatBadgeForManualEarning);
    RUN_TEST(TestFormatBadgeForManualSkillLevel);
    RUN_TEST(TestFormatBadgeForChatAuto);
    RUN_TEST(TestNotePersistenceRoundTrip);

    std::printf("\n=== Results: %d passed, %d failed ===\n\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
