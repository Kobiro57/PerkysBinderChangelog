/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Job Adjustment Tests - Summary adjustments persistence, API, and clamp behavior
*/
#define KEYBINDER_TESTING

#include "../include/JobEventStore.h"
#include "../include/JobTracker.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
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
        "%sJobAdjustment_%s_%lu_%llu.json",
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

std::string ReadWholeFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool TestStoreAdjustmentsDefaultToZero() {
    const std::string filepath = MakeTempFilePath("default_adj");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    const jobs::JobSummaryAdjustment adj = store.GetJobSummaryAdjustments("landwirt");
    ASSERT_EQ(adj.jobId, std::string("landwirt"));
    ASSERT_EQ(adj.earningsOffset, 0);
    ASSERT_EQ(adj.skillOffset, 0);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreSetAndGetAdjustments() {
    const std::string filepath = MakeTempFilePath("set_get_adj");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = 500;
    adj.skillOffset = 3;
    store.SetJobSummaryAdjustments("landwirt", adj);

    const jobs::JobSummaryAdjustment retrieved = store.GetJobSummaryAdjustments("landwirt");
    ASSERT_EQ(retrieved.jobId, std::string("landwirt"));
    ASSERT_EQ(retrieved.earningsOffset, 500);
    ASSERT_EQ(retrieved.skillOffset, 3);

    const jobs::JobSummaryAdjustment other = store.GetJobSummaryAdjustments("busfahrer");
    ASSERT_EQ(other.earningsOffset, 0);
    ASSERT_EQ(other.skillOffset, 0);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreAdjustmentRoundTrip() {
    const std::string filepath = MakeTempFilePath("adj_roundtrip");
    RemoveIfExists(filepath);

    {
        jobs::JobEventStore store(filepath);
        ASSERT_TRUE(store.Load());

        jobs::JobEvent earning;
        earning.jobId = "landwirt";
        earning.type = "earning";
        earning.amount = 1000;
        earning.level = 0;
        earning.source = "chat_auto";
        earning.timestamp = MakeLocalTimestamp(2026, 4, 13, 10, 0, 0);
        ASSERT_TRUE(store.RecordEvent(earning));

        jobs::JobEvent skill;
        skill.jobId = "landwirt";
        skill.type = "skill_level_absolute";
        skill.amount = 0;
        skill.level = 5;
        skill.source = "chat_auto";
        skill.timestamp = MakeLocalTimestamp(2026, 4, 13, 11, 0, 0);
        ASSERT_TRUE(store.RecordEvent(skill));

        jobs::JobSummaryAdjustment adj;
        adj.jobId = "landwirt";
        adj.earningsOffset = 250;
        adj.skillOffset = 2;
        store.SetJobSummaryAdjustments("landwirt", adj);

        jobs::JobSummaryAdjustment adj2;
        adj2.jobId = "busfahrer";
        adj2.earningsOffset = -100;
        adj2.skillOffset = -1;
        store.SetJobSummaryAdjustments("busfahrer", adj2);

        ASSERT_TRUE(store.Save());
    }

    {
        jobs::JobEventStore reloaded(filepath);
        ASSERT_TRUE(reloaded.Load());

        const jobs::JobSummaryAdjustment adj = reloaded.GetJobSummaryAdjustments("landwirt");
        ASSERT_EQ(adj.earningsOffset, 250);
        ASSERT_EQ(adj.skillOffset, 2);

        const jobs::JobSummaryAdjustment adj2 = reloaded.GetJobSummaryAdjustments("busfahrer");
        ASSERT_EQ(adj2.earningsOffset, -100);
        ASSERT_EQ(adj2.skillOffset, -1);

        ASSERT_EQ(reloaded.GetEvents().size(), static_cast<std::size_t>(2));

        jobs::JobStats stats;
        ASSERT_TRUE(reloaded.GetJobStats("landwirt", stats));
        ASSERT_EQ(stats.totalAmount, 1000);
        ASSERT_EQ(stats.highestLevel, 5);
    }

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreAdjustedLifetimeEarnings() {
    const std::string filepath = MakeTempFilePath("adj_lifetime");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("landwirt"), 0);

    jobs::JobEvent e1;
    e1.jobId = "landwirt";
    e1.type = "earning";
    e1.amount = 1000;
    e1.level = 0;
    e1.source = "chat_auto";
    e1.timestamp = MakeLocalTimestamp(2026, 4, 13, 10, 0, 0);
    ASSERT_TRUE(store.RecordEvent(e1));

    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("landwirt"), 1000);

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = 500;
    store.SetJobSummaryAdjustments("landwirt", adj);
    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("landwirt"), 1500);

    adj.earningsOffset = -200;
    store.SetJobSummaryAdjustments("landwirt", adj);
    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("landwirt"), 800);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreAdjustedEarningsClampedToZero() {
    const std::string filepath = MakeTempFilePath("adj_clamp");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobEvent e1;
    e1.jobId = "landwirt";
    e1.type = "earning";
    e1.amount = 100;
    e1.level = 0;
    e1.source = "chat_auto";
    e1.timestamp = MakeLocalTimestamp(2026, 4, 13, 10, 0, 0);
    ASSERT_TRUE(store.RecordEvent(e1));

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = -500;
    store.SetJobSummaryAdjustments("landwirt", adj);

    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("landwirt"), 0);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreAdjustedSkillClampedToZero() {
    const std::string filepath = MakeTempFilePath("skill_clamp");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    ASSERT_EQ(store.GetAdjustedHighestSkill("landwirt"), 0);

    jobs::JobEvent e1;
    e1.jobId = "landwirt";
    e1.type = "skill_level_absolute";
    e1.amount = 0;
    e1.level = 3;
    e1.source = "chat_auto";
    e1.timestamp = MakeLocalTimestamp(2026, 4, 13, 10, 0, 0);
    ASSERT_TRUE(store.RecordEvent(e1));

    ASSERT_EQ(store.GetAdjustedHighestSkill("landwirt"), 3);

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.skillOffset = 2;
    store.SetJobSummaryAdjustments("landwirt", adj);
    ASSERT_EQ(store.GetAdjustedHighestSkill("landwirt"), 5);

    adj.skillOffset = -10;
    store.SetJobSummaryAdjustments("landwirt", adj);
    ASSERT_EQ(store.GetAdjustedHighestSkill("landwirt"), 0);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreDayWindowsUnaffectedByAdjustments() {
    const std::string filepath = MakeTempFilePath("day_windows");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    const std::time_t now = MakeLocalTimestamp(2026, 4, 13, 18, 0, 0);

    jobs::JobEvent e1;
    e1.jobId = "landwirt";
    e1.type = "earning";
    e1.amount = 500;
    e1.level = 0;
    e1.source = "chat_auto";
    e1.timestamp = now;
    ASSERT_TRUE(store.RecordEvent(e1));

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = 9999;
    store.SetJobSummaryAdjustments("landwirt", adj);

    const jobs::JobWindowAggregates agg = store.GetAggregates("landwirt", now);

    ASSERT_EQ(agg.today, 500);
    ASSERT_EQ(agg.last7Days, 500);
    ASSERT_EQ(agg.last30Days, 500);
    ASSERT_EQ(agg.bestDay, 500);
    ASSERT_EQ(agg.lifetime, 10499);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreGetAllAdjustments() {
    const std::string filepath = MakeTempFilePath("all_adj");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobSummaryAdjustment adj1;
    adj1.jobId = "landwirt";
    adj1.earningsOffset = 100;
    store.SetJobSummaryAdjustments("landwirt", adj1);

    jobs::JobSummaryAdjustment adj2;
    adj2.jobId = "busfahrer";
    adj2.earningsOffset = 200;
    store.SetJobSummaryAdjustments("busfahrer", adj2);

    const auto& all = store.GetAllSummaryAdjustments();
    ASSERT_EQ(all.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(all.find("landwirt") != all.end());
    ASSERT_TRUE(all.find("busfahrer") != all.end());
    ASSERT_EQ(all.at("landwirt").earningsOffset, 100);
    ASSERT_EQ(all.at("busfahrer").earningsOffset, 200);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreAdjustmentsInSerializedJson() {
    const std::string filepath = MakeTempFilePath("adj_json");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = 250;
    adj.skillOffset = 3;
    store.SetJobSummaryAdjustments("landwirt", adj);

    ASSERT_TRUE(store.Save());

    const std::string json = ReadWholeFile(filepath);
    ASSERT_TRUE(json.find("\"summaryAdjustments\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"earningsOffset\": 250") != std::string::npos);
    ASSERT_TRUE(json.find("\"skillOffset\": 3") != std::string::npos);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreNegativeOffsetNoEarningsClampsToZero() {
    const std::string filepath = MakeTempFilePath("neg_no_base");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "busfahrer";
    adj.earningsOffset = -50;
    adj.skillOffset = -3;
    store.SetJobSummaryAdjustments("busfahrer", adj);

    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("busfahrer"), 0);
    ASSERT_EQ(store.GetAdjustedHighestSkill("busfahrer"), 0);

    RemoveIfExists(filepath);
    return true;
}

bool TestStoreOverwriteAdjustments() {
    const std::string filepath = MakeTempFilePath("overwrite_adj");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = 100;
    adj.skillOffset = 2;
    store.SetJobSummaryAdjustments("landwirt", adj);

    ASSERT_EQ(store.GetJobSummaryAdjustments("landwirt").earningsOffset, 100);
    ASSERT_EQ(store.GetJobSummaryAdjustments("landwirt").skillOffset, 2);

    adj.earningsOffset = 500;
    adj.skillOffset = -1;
    store.SetJobSummaryAdjustments("landwirt", adj);

    ASSERT_EQ(store.GetJobSummaryAdjustments("landwirt").earningsOffset, 500);
    ASSERT_EQ(store.GetJobSummaryAdjustments("landwirt").skillOffset, -1);

    RemoveIfExists(filepath);
    return true;
}

bool TestTrackerGetJobEditSnapshot() {
    const std::string filepath = MakeTempFilePath("edit_snapshot");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    tracker.AddManualEvent("landwirt", "manual_earning", 1000, 0, ts, "");
    tracker.AddManualEvent("landwirt", "manual_skill_level", 0, 5, ts, "");

    jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
    ASSERT_EQ(snap.jobId, std::string("landwirt"));
    ASSERT_EQ(snap.baseTotalEarnings, 1000);
    ASSERT_EQ(snap.baseHighestSkill, 5);
    ASSERT_EQ(snap.earningsOffset, 0);
    ASSERT_EQ(snap.skillOffset, 0);
    ASSERT_EQ(snap.adjustedTotalEarnings, 1000);
    ASSERT_EQ(snap.adjustedHighestSkill, 5);

    tracker.ApplyJobSummaryAdjustments("landwirt", 250, 2);

    snap = tracker.GetJobEditSnapshot("landwirt");
    ASSERT_EQ(snap.baseTotalEarnings, 1000);
    ASSERT_EQ(snap.baseHighestSkill, 5);
    ASSERT_EQ(snap.earningsOffset, 250);
    ASSERT_EQ(snap.skillOffset, 2);
    ASSERT_EQ(snap.adjustedTotalEarnings, 1250);
    ASSERT_EQ(snap.adjustedHighestSkill, 7);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestTrackerApplyAdjustmentsWithClamping() {
    const std::string filepath = MakeTempFilePath("adj_clamp_tracker");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    tracker.AddManualEvent("landwirt", "manual_earning", 100, 0, ts, "");

    tracker.ApplyJobSummaryAdjustments("landwirt", -500, -10);

    const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
    ASSERT_EQ(snap.baseTotalEarnings, 100);
    ASSERT_EQ(snap.earningsOffset, -500);
    ASSERT_EQ(snap.skillOffset, -10);
    ASSERT_EQ(snap.adjustedTotalEarnings, 0);
    ASSERT_EQ(snap.adjustedHighestSkill, 0);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestTrackerEditSnapshotForUnknownJob() {
    const std::string filepath = MakeTempFilePath("unknown_snap");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("nonexistent");
    ASSERT_EQ(snap.jobId, std::string("nonexistent"));
    ASSERT_EQ(snap.baseTotalEarnings, 0);
    ASSERT_EQ(snap.baseHighestSkill, 0);
    ASSERT_EQ(snap.earningsOffset, 0);
    ASSERT_EQ(snap.skillOffset, 0);
    ASSERT_EQ(snap.adjustedTotalEarnings, 0);
    ASSERT_EQ(snap.adjustedHighestSkill, 0);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestTrackerAdjustmentsPersistAcrossShutdown() {
    const std::string filepath = MakeTempFilePath("adj_persist");
    RemoveIfExists(filepath);

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());

        const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
        tracker.AddManualEvent("landwirt", "manual_earning", 500, 0, ts, "");
        tracker.ApplyJobSummaryAdjustments("landwirt", 200, 1);

        tracker.Shutdown();
    }

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());

        const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
        ASSERT_EQ(snap.baseTotalEarnings, 500);
        ASSERT_EQ(snap.earningsOffset, 200);
        ASSERT_EQ(snap.skillOffset, 1);
        ASSERT_EQ(snap.adjustedTotalEarnings, 700);
        ASSERT_EQ(snap.adjustedHighestSkill, 1);

        tracker.Shutdown();
    }

    RemoveIfExists(filepath);
    return true;
}

} // namespace

int RunJobAdjustmentTests() {
    g_state = {};

    std::printf("\n=== Job Adjustment Tests ===\n\n");

    RUN_TEST(TestStoreAdjustmentsDefaultToZero);
    RUN_TEST(TestStoreSetAndGetAdjustments);
    RUN_TEST(TestStoreAdjustmentRoundTrip);
    RUN_TEST(TestStoreAdjustedLifetimeEarnings);
    RUN_TEST(TestStoreAdjustedEarningsClampedToZero);
    RUN_TEST(TestStoreAdjustedSkillClampedToZero);
    RUN_TEST(TestStoreDayWindowsUnaffectedByAdjustments);
    RUN_TEST(TestStoreGetAllAdjustments);
    RUN_TEST(TestStoreAdjustmentsInSerializedJson);
    RUN_TEST(TestStoreNegativeOffsetNoEarningsClampsToZero);
    RUN_TEST(TestStoreOverwriteAdjustments);
    RUN_TEST(TestTrackerGetJobEditSnapshot);
    RUN_TEST(TestTrackerApplyAdjustmentsWithClamping);
    RUN_TEST(TestTrackerEditSnapshotForUnknownJob);
    RUN_TEST(TestTrackerAdjustmentsPersistAcrossShutdown);

    std::printf("\n=== Results: %d passed, %d failed ===\n\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
