#define KEYBINDER_TESTING

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
        "%sJobTracker_%s_%lu_%llu.json",
        tempPath,
        label,
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long long>(GetTickCount64()));
    return filename;
}

void RemoveIfExists(const std::string& filepath) {
    DeleteFileA(filepath.c_str());
}

bool FileExists(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    return file.good();
}

std::string ReadWholeFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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

bool LoadInvalidRegistry(jobs::JobRegistry::Snapshot& snapshot) {
    snapshot = {};
    snapshot.version = 1;
    return false;
}

bool TestInitAndShutdownAreIdempotentAndOnlySaveWhenDirty() {
    const std::string filepath = MakeTempFilePath("idempotent");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());
    ASSERT_TRUE(tracker.Initialize());
    ASSERT_TRUE(tracker.IsInitialized());
    ASSERT_TRUE(!tracker.IsDirty());
    ASSERT_EQ(tracker.GetJobs().size(), jobs::kExpectedJobDefinitionCount);
    ASSERT_TRUE(!FileExists(filepath));

    tracker.Shutdown();
    tracker.Shutdown();
    ASSERT_TRUE(!FileExists(filepath));

    ASSERT_TRUE(tracker.Initialize());
    ASSERT_TRUE(tracker.AddManualEarningCorrection("landwirt", 1250, MakeLocalTimestamp(2026, 4, 13, 13, 15, 0)));
    ASSERT_TRUE(tracker.IsDirty());

    tracker.Shutdown();
    ASSERT_TRUE(FileExists(filepath));

    const std::string json = ReadWholeFile(filepath);
    ASSERT_TRUE(json.find("manual_earning") != std::string::npos);
    ASSERT_TRUE(json.find("landwirt") != std::string::npos);

    RemoveIfExists(filepath);
    return true;
}

bool TestMissingFilesAndInvalidRegistryFallbackToEmptyState() {
    const std::string filepath = MakeTempFilePath("invalid_registry");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(&LoadInvalidRegistry, filepath);
    ASSERT_TRUE(tracker.Initialize());
    ASSERT_TRUE(tracker.IsInitialized());
    ASSERT_TRUE(tracker.GetJobs().empty());
    ASSERT_TRUE(tracker.GetEvents().empty());
    ASSERT_TRUE(tracker.GetAllJobStats().empty());

    const jobs::JobWindowAggregates aggregates = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(aggregates.today, 0);
    ASSERT_EQ(aggregates.last7Days, 0);
    ASSERT_EQ(aggregates.last30Days, 0);
    ASSERT_EQ(aggregates.bestDay, 0);
    ASSERT_EQ(aggregates.lifetime, 0);
    ASSERT_TRUE(!tracker.AddManualEarningCorrection("landwirt", 50));

    tracker.Shutdown();
    ASSERT_TRUE(!FileExists(filepath));

    RemoveIfExists(filepath);
    return true;
}

bool TestChatPipelineStoresParsedEventsAndUpdatesAggregates() {
    const std::string filepath = MakeTempFilePath("pipeline");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    jobs::JobEvent event;
    event.jobId = "landwirt";
    event.type = "earning";
    event.amount = 1234;
    event.level = 0;
    event.source = "chat_auto";
    event.timestamp = MakeLocalTimestamp(2026, 4, 13, 18, 28, 35);

    ASSERT_TRUE(tracker.RecordEvent(event));

    ASSERT_EQ(tracker.GetEvents().size(), static_cast<std::size_t>(1));
    jobs::JobStats stats;
    ASSERT_TRUE(tracker.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.totalEvents, 1);
    ASSERT_EQ(stats.totalAmount, 1234);

    const jobs::JobWindowAggregates aggregates = tracker.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 23, 0, 0));
    ASSERT_EQ(aggregates.today, 1234);
    ASSERT_EQ(aggregates.last7Days, 1234);
    ASSERT_EQ(aggregates.last30Days, 1234);
    ASSERT_EQ(aggregates.bestDay, 1234);
    ASSERT_EQ(aggregates.lifetime, 1234);

    tracker.Shutdown();
    ASSERT_TRUE(FileExists(filepath));

    RemoveIfExists(filepath);
    return true;
}

bool TestChatPipelineIgnoresInfoAndStartLines() {
    const std::string filepath = MakeTempFilePath("non_financial");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    jobs::JobChatEvent infoEvent;
    infoEvent.rawLine = "INFO: Du kannst deine Tätigkeit mit /stopjob vorzeitig beenden...";
    infoEvent.timestampFreeLine = infoEvent.rawLine;
    infoEvent.normalizedLine = infoEvent.rawLine;
    infoEvent.jobPrefix.clear();
    infoEvent.arrivalTime = MakeLocalTimestamp(2026, 4, 13, 18, 28, 35);

    jobs::JobChatEvent startEvent;
    startEvent.rawLine = "Landwirt: Die Kühe müssen gemolken werden...";
    startEvent.timestampFreeLine = startEvent.rawLine;
    startEvent.normalizedLine = startEvent.rawLine;
    startEvent.jobPrefix = "Landwirt";
    startEvent.arrivalTime = MakeLocalTimestamp(2026, 4, 13, 18, 29, 0);

    tracker.HandleChatEvent(infoEvent);
    tracker.HandleChatEvent(startEvent);

    ASSERT_TRUE(tracker.GetEvents().empty());

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestGetJobEditSnapshotReflectsAdjustments() {
    const std::string filepath = MakeTempFilePath("edit_snapshot");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
    ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_earning", 1000, 0, ts, "").empty());
    ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_skill_level", 0, 5, ts, "").empty());

    tracker.ApplyJobSummaryAdjustments("landwirt", 250, 2);

    const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
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

bool TestApplyJobSummaryAdjustmentsPersistAcrossReload() {
    const std::string filepath = MakeTempFilePath("adjustments_reload");
    RemoveIfExists(filepath);

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());

        const std::time_t ts = MakeLocalTimestamp(2026, 4, 13, 14, 0, 0);
        ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_earning", 500, 0, ts, "").empty());
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

bool TestNewJobSkillPointsPersistWithSkillTrees() {
    const std::string filepath = MakeTempFilePath("new_job_skill_trees");
    RemoveIfExists(filepath);

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());

        for (const char* jobId : {"holzfaeller", "bauarbeiter"}) {
            const std::string prefix = std::string(jobId) == "holzfaeller" ? "Holzfäller" : "Bauarbeiter";
            jobs::JobChatEvent event;
            event.rawLine = prefix + ": Du hast deinen Job-Skill auf 749 verbessert.";
            event.timestampFreeLine = event.rawLine;
            event.normalizedLine = event.rawLine;
            event.jobPrefix = prefix;
            event.arrivalTime = MakeLocalTimestamp(2026, 10, 24, 12, 0, 0);
            ASSERT_EQ(tracker.HandleChatEvent(event).size(), static_cast<std::size_t>(1));
            ASSERT_EQ(tracker.GetAdjustedHighestSkill(jobId), 749);
        }
        tracker.ApplyJobSummaryAdjustments("bauarbeiter", 0, 1);
        ASSERT_EQ(tracker.GetAdjustedHighestSkill("bauarbeiter"), 750);
        ASSERT_TRUE(tracker.Flush());
        tracker.Shutdown();
    }

    {
        jobs::JobTracker tracker(filepath);
        ASSERT_TRUE(tracker.Initialize());
        ASSERT_EQ(tracker.GetAdjustedHighestSkill("holzfaeller"), 749);
        ASSERT_EQ(tracker.GetAdjustedHighestSkill("bauarbeiter"), 750);
        ASSERT_EQ(tracker.GetJobEditSnapshot("bauarbeiter").skillOffset, 1);

        for (const jobs::JobDefinition& job : tracker.GetJobs()) {
            if (job.id != "holzfaeller" && job.id != "bauarbeiter") continue;
            ASSERT_EQ(job.skillTree.size(), static_cast<std::size_t>(6));
            jobs::JobStats stats;
            ASSERT_TRUE(tracker.GetJobStats(job.id, stats));
            ASSERT_EQ(stats.highestLevel, 749);
        }
        tracker.Shutdown();
    }

    RemoveIfExists(filepath);
    return true;
}

} // namespace

int RunJobTrackerLifecycleTests() {
    g_state = {};

    RUN_TEST(TestInitAndShutdownAreIdempotentAndOnlySaveWhenDirty);
    RUN_TEST(TestMissingFilesAndInvalidRegistryFallbackToEmptyState);
    RUN_TEST(TestChatPipelineStoresParsedEventsAndUpdatesAggregates);
    RUN_TEST(TestChatPipelineIgnoresInfoAndStartLines);
    RUN_TEST(TestGetJobEditSnapshotReflectsAdjustments);
    RUN_TEST(TestApplyJobSummaryAdjustmentsPersistAcrossReload);
    RUN_TEST(TestNewJobSkillPointsPersistWithSkillTrees);

    std::printf("JobTrackerLifecycleTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
