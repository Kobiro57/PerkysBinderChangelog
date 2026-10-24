#define KEYBINDER_TESTING

#include "../include/JobEventStore.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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
        "%sJobEventStore_%s_%lu_%llu.json",
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

std::time_t ShiftLocalDays(std::time_t base, int daysAgo) {
    std::tm localTime = {};
    if (localtime_s(&localTime, &base) != 0) {
        return base;
    }

    localTime.tm_mday -= daysAgo;
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

bool TestRoundTripPersistsEventsAndJobState() {
    const std::string filepath = MakeTempFilePath("roundtrip");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());
    ASSERT_TRUE(store.GetEvents().empty());

    jobs::JobEvent earning;
    earning.jobId = "landwirt";
    earning.type = "earning";
    earning.amount = 1250;
    earning.level = 0;
    earning.source = "chat_auto";
    earning.timestamp = MakeLocalTimestamp(2026, 4, 13, 10, 30, 0);
    ASSERT_TRUE(store.RecordEvent(earning));

    jobs::JobEvent skill;
    skill.jobId = "landwirt";
    skill.type = "skill_level_absolute";
    skill.amount = 0;
    skill.level = 4;
    skill.source = "chat_auto";
    skill.timestamp = MakeLocalTimestamp(2026, 4, 13, 11, 45, 0);
    ASSERT_TRUE(store.RecordEvent(skill));

    ASSERT_TRUE(store.Save());

    const std::string savedJson = ReadWholeFile(filepath);
    ASSERT_TRUE(savedJson.find("\"version\": 1") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"jobState\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"events\"") != std::string::npos);

    jobs::JobEventStore reloaded(filepath);
    ASSERT_TRUE(reloaded.Load());
    ASSERT_EQ(reloaded.GetEvents().size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(!reloaded.GetEvents()[0].id.empty());
    ASSERT_EQ(reloaded.GetEvents()[0].dayKey, jobs::JobEventStore::BuildDayKey(earning.timestamp));
    ASSERT_EQ(reloaded.GetEvents()[1].dayKey, jobs::JobEventStore::BuildDayKey(skill.timestamp));

    jobs::JobStats stats;
    ASSERT_TRUE(reloaded.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.jobId, std::string("landwirt"));
    ASSERT_EQ(stats.totalEvents, 2);
    ASSERT_EQ(stats.totalAmount, 1250);
    ASSERT_EQ(stats.highestLevel, 4);
    ASSERT_EQ(stats.lastEventTimestamp, skill.timestamp);
    ASSERT_EQ(stats.lastDayKey, jobs::JobEventStore::BuildDayKey(skill.timestamp));

    const jobs::JobWindowAggregates aggregates = reloaded.GetAggregates("landwirt", MakeLocalTimestamp(2026, 4, 13, 18, 0, 0));
    ASSERT_EQ(aggregates.today, 1250);
    ASSERT_EQ(aggregates.last7Days, 1250);
    ASSERT_EQ(aggregates.last30Days, 1250);
    ASSERT_EQ(aggregates.bestDay, 1250);
    ASSERT_EQ(aggregates.lifetime, 1250);

    RemoveIfExists(filepath);
    return true;
}

bool TestRollingWindowsAggregateCorrectly() {
    const std::string filepath = MakeTempFilePath("windows");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    const std::time_t now = MakeLocalTimestamp(2026, 4, 13, 20, 0, 0);

    struct EarningSeed {
        int daysAgo;
        int amount;
        const char* type;
        const char* source;
    };

    const EarningSeed earnings[] = {
        {0, 100, "earning", "chat_auto"},
        {0, 20, "manual_earning", "manual_add"},
        {1, 50, "earning", "chat_auto"},
        {6, 25, "earning", "chat_auto"},
        {7, 200, "earning", "chat_auto"},
        {15, 75, "manual_earning", "manual_add"},
        {20, 500, "earning", "chat_auto"},
        {29, 300, "earning", "chat_auto"},
        {30, 400, "earning", "chat_auto"},
        {44, 600, "earning", "chat_auto"}
    };

    for (const EarningSeed& seed : earnings) {
        jobs::JobEvent event;
        event.jobId = "landwirt";
        event.type = seed.type;
        event.amount = seed.amount;
        event.level = 0;
        event.source = seed.source;
        event.timestamp = ShiftLocalDays(now, seed.daysAgo);
        ASSERT_TRUE(store.RecordEvent(event));
    }

    jobs::JobEvent skill;
    skill.jobId = "landwirt";
    skill.type = "skill_level_absolute";
    skill.amount = 0;
    skill.level = 4;
    skill.source = "chat_auto";
    skill.timestamp = ShiftLocalDays(now, 10);
    ASSERT_TRUE(store.RecordEvent(skill));

    jobs::JobEvent manualSkill;
    manualSkill.jobId = "landwirt";
    manualSkill.type = "manual_skill_level";
    manualSkill.amount = 0;
    manualSkill.level = 6;
    manualSkill.source = "manual_add";
    manualSkill.timestamp = ShiftLocalDays(now, 2);
    ASSERT_TRUE(store.RecordEvent(manualSkill));

    const jobs::JobWindowAggregates aggregates = store.GetAggregates("landwirt", now);
    ASSERT_EQ(aggregates.today, 120);
    ASSERT_EQ(aggregates.last7Days, 195);
    ASSERT_EQ(aggregates.last30Days, 1270);
    ASSERT_EQ(aggregates.bestDay, 600);
    ASSERT_EQ(aggregates.lifetime, 2270);

    jobs::JobStats stats;
    ASSERT_TRUE(store.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.totalEvents, 12);
    ASSERT_EQ(stats.totalAmount, 2270);
    ASSERT_EQ(stats.highestLevel, 6);

    RemoveIfExists(filepath);
    return true;
}

bool TestSkillLevelEventsUseLatestAbsoluteValue() {
    const std::string filepath = MakeTempFilePath("skill_absolute");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);

    jobs::JobEvent newerHigh;
    newerHigh.jobId = "landwirt";
    newerHigh.type = "skill_level_absolute";
    newerHigh.source = "chat_auto";
    newerHigh.level = 25;
    newerHigh.timestamp = MakeLocalTimestamp(2026, 4, 14, 12, 0, 0);
    ASSERT_TRUE(store.RecordEvent(newerHigh));

    jobs::JobEvent latestLower;
    latestLower.jobId = "landwirt";
    latestLower.type = "skill_level_absolute";
    latestLower.source = "chat_auto";
    latestLower.level = 23;
    latestLower.timestamp = MakeLocalTimestamp(2026, 4, 14, 12, 5, 0);
    ASSERT_TRUE(store.RecordEvent(latestLower));

    jobs::JobStats stats;
    ASSERT_TRUE(store.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.highestLevel, 23);

    ASSERT_TRUE(store.Save());

    jobs::JobEventStore reloaded(filepath);
    ASSERT_TRUE(reloaded.Load());
    ASSERT_TRUE(reloaded.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.highestLevel, 23);

    RemoveIfExists(filepath);
    return true;
}

bool TestSoftDeleteExcludesEventsButKeepsThemPersisted() {
    const std::string filepath = MakeTempFilePath("softdelete");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    const std::time_t now = MakeLocalTimestamp(2026, 4, 13, 19, 0, 0);

    jobs::JobEvent event;
    event.jobId = "landwirt";
    event.type = "manual_earning";
    event.amount = 250;
    event.level = 0;
    event.source = "manual_add";
    event.timestamp = now;
    ASSERT_TRUE(store.RecordEvent(event));

    jobs::JobEvent untouched;
    untouched.jobId = "landwirt";
    untouched.type = "earning";
    untouched.amount = 50;
    untouched.level = 0;
    untouched.source = "chat_auto";
    untouched.timestamp = ShiftLocalDays(now, 1);
    ASSERT_TRUE(store.RecordEvent(untouched));

    ASSERT_TRUE(store.Save());
    ASSERT_EQ(store.GetAggregates("landwirt", now).lifetime, 300);

    const std::vector<jobs::JobEvent> eventsBeforeDelete = store.GetEventsForJob("landwirt", true);
    ASSERT_EQ(eventsBeforeDelete.size(), static_cast<std::size_t>(2));

    ASSERT_TRUE(store.SoftDeleteEvent(eventsBeforeDelete[0].id));
    ASSERT_TRUE(store.Save());

    const jobs::JobWindowAggregates aggregates = store.GetAggregates("landwirt", now);
    ASSERT_EQ(aggregates.today, 0);
    ASSERT_EQ(aggregates.last7Days, 50);
    ASSERT_EQ(aggregates.last30Days, 50);
    ASSERT_EQ(aggregates.bestDay, 50);
    ASSERT_EQ(aggregates.lifetime, 50);

    const std::vector<jobs::JobEvent> visibleEvents = store.GetEventsForJob("landwirt", false);
    ASSERT_EQ(visibleEvents.size(), static_cast<std::size_t>(1));

    const std::vector<jobs::JobEvent> allEvents = store.GetEventsForJob("landwirt", true);
    ASSERT_EQ(allEvents.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(allEvents[0].deleted || allEvents[1].deleted);

    const std::string savedJson = ReadWholeFile(filepath);
    ASSERT_TRUE(savedJson.find("\"deleted\": true") != std::string::npos);

    jobs::JobEventStore reloaded(filepath);
    ASSERT_TRUE(reloaded.Load());
    ASSERT_EQ(reloaded.GetAggregates("landwirt", now).lifetime, 50);
    ASSERT_EQ(reloaded.GetEventsForJob("landwirt", true).size(), static_cast<std::size_t>(2));
    ASSERT_EQ(reloaded.GetEventsForJob("landwirt", false).size(), static_cast<std::size_t>(1));

    RemoveIfExists(filepath);
    return true;
}

bool TestSummaryAdjustmentsPersistWithEvents() {
    const std::string filepath = MakeTempFilePath("adjustments_events");
    RemoveIfExists(filepath);

    {
        jobs::JobEventStore store(filepath);
        ASSERT_TRUE(store.Load());

        jobs::JobEvent earning;
        earning.jobId = "landwirt";
        earning.type = "earning";
        earning.amount = 600;
        earning.level = 0;
        earning.source = "chat_auto";
        earning.timestamp = MakeLocalTimestamp(2026, 4, 13, 10, 0, 0);
        ASSERT_TRUE(store.RecordEvent(earning));

        jobs::JobSummaryAdjustment adj;
        adj.jobId = "landwirt";
        adj.earningsOffset = 150;
        adj.skillOffset = 2;
        store.SetJobSummaryAdjustments("landwirt", adj);

        ASSERT_TRUE(store.Save());
    }

    jobs::JobEventStore reloaded(filepath);
    ASSERT_TRUE(reloaded.Load());

    const jobs::JobSummaryAdjustment loadedAdj = reloaded.GetJobSummaryAdjustments("landwirt");
    ASSERT_EQ(loadedAdj.earningsOffset, 150);
    ASSERT_EQ(loadedAdj.skillOffset, 2);
    ASSERT_EQ(reloaded.GetAdjustedLifetimeEarnings("landwirt"), 750);

    jobs::JobStats stats;
    ASSERT_TRUE(reloaded.GetJobStats("landwirt", stats));
    ASSERT_EQ(stats.totalAmount, 600);
    ASSERT_EQ(stats.highestLevel, 0);

    RemoveIfExists(filepath);
    return true;
}

bool TestAggregatesLifetimeIncludesAdjustmentsWithoutEarningsBuckets() {
    const std::string filepath = MakeTempFilePath("adjustments_no_earnings_buckets");
    RemoveIfExists(filepath);

    jobs::JobEventStore store(filepath);
    ASSERT_TRUE(store.Load());

    jobs::JobEvent skill;
    skill.jobId = "landwirt";
    skill.type = "manual_skill_level";
    skill.amount = 0;
    skill.level = 4;
    skill.source = "manual_add";
    skill.timestamp = MakeLocalTimestamp(2026, 4, 13, 12, 0, 0);
    ASSERT_TRUE(store.RecordEvent(skill));

    jobs::JobSummaryAdjustment adj;
    adj.jobId = "landwirt";
    adj.earningsOffset = 500;
    store.SetJobSummaryAdjustments("landwirt", adj);

    const std::time_t now = MakeLocalTimestamp(2026, 4, 13, 18, 0, 0);
    const jobs::JobWindowAggregates aggregates = store.GetAggregates("landwirt", now);

    ASSERT_EQ(aggregates.today, 0);
    ASSERT_EQ(aggregates.last7Days, 0);
    ASSERT_EQ(aggregates.last30Days, 0);
    ASSERT_EQ(aggregates.bestDay, 0);
    ASSERT_EQ(aggregates.lifetime, 500);
    ASSERT_EQ(store.GetAdjustedLifetimeEarnings("landwirt"), 500);

    RemoveIfExists(filepath);
    return true;
}

} // namespace

int RunJobEventStoreTests() {
    g_state = {};

    RUN_TEST(TestRoundTripPersistsEventsAndJobState);
    RUN_TEST(TestRollingWindowsAggregateCorrectly);
    RUN_TEST(TestSkillLevelEventsUseLatestAbsoluteValue);
    RUN_TEST(TestSoftDeleteExcludesEventsButKeepsThemPersisted);
    RUN_TEST(TestSummaryAdjustmentsPersistWithEvents);
    RUN_TEST(TestAggregatesLifetimeIncludesAdjustmentsWithoutEarningsBuckets);

    std::printf("JobEventStoreTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
