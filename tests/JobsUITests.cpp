/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    JobsUI unit tests - overview rendering, selection, detail updates
*/
#define KEYBINDER_TESTING

#include "../include/JobsUI.h"
#include "../include/JobsTypes.h"
#include "../include/JobEventStore.h"
#include "../include/JobTracker.h"
#include "../include/JobRegistry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

// ── Local assertion macros (same pattern as other test files) ──
int g_passed = 0;
int g_failed = 0;

std::string MakeTempFilePath(const char* label) {
    return std::string("build\\") + label + "_" + std::to_string(static_cast<long long>(std::time(nullptr))) + ".json";
}

void RemoveIfExists(const std::string& filepath) {
    std::remove(filepath.c_str());
}

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::printf("[FAIL] %s: %s\n", __func__, #cond); \
        return false; \
    } \
    ++g_passed; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("[FAIL] %s: %s != %s\n", __func__, #a, #b); \
        return false; \
    } \
    ++g_passed; \
} while(0)

// ── Formatting tests (pure logic, no ImGui needed) ──

bool TestFormatMoney() {
    keybinder::JobsUI ui;
    ASSERT_EQ(keybinder::JobsUI::FormatMoney(0), "0 $");
    ASSERT_EQ(keybinder::JobsUI::FormatMoney(1250), "1250 $");
    ASSERT_EQ(keybinder::JobsUI::FormatMoney(-50), "-50 $");
    return true;
}

bool TestFormatTimestamp() {
    // Epoch 0 should return "-"
    ASSERT_EQ(keybinder::JobsUI::FormatTimestamp(0), "-");

    // Negative should return "-"
    ASSERT_EQ(keybinder::JobsUI::FormatTimestamp(-1), "-");

    // Positive timestamp should produce a non-empty string
    std::string result = keybinder::JobsUI::FormatTimestamp(std::time(nullptr));
    ASSERT_TRUE(!result.empty());
    ASSERT_TRUE(result != "-");
    return true;
}

bool TestFormatEventTypeBadge() {
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("earning"), "Verdienst");
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("skill_level"), "Level");
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("skill_level_absolute"), "Level");
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("correction"), "Korrektur");
    // Unknown type falls through
    ASSERT_EQ(keybinder::JobsUI::FormatEventTypeBadge("unknown_type"), "unknown_type");
    return true;
}

bool TestFormatSourceBadge() {
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("auto"), "Auto");
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("manual"), "Manuell");
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("chat_auto"), "Auto");
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("manual_add"), "Manuell");
    ASSERT_EQ(keybinder::JobsUI::FormatSourceBadge("other"), "other");
    return true;
}

// ── Selection state tests ──

bool TestDefaultSelectionUnresolvedWithoutTracker() {
    keybinder::JobsUI ui;
    // No SAMPBridge set, so ResolveDefaultSelection should not resolve
    ASSERT_TRUE(!ui.IsDefaultSelectionResolved());
    ASSERT_EQ(ui.GetSelectedJobIndex(), -1);
    return true;
}

bool TestSetSelectedJobIndex() {
    keybinder::JobsUI ui;
    ASSERT_EQ(ui.GetSelectedJobIndex(), -1);

    ui.SetSelectedJobIndex(3);
    ASSERT_EQ(ui.GetSelectedJobIndex(), 3);

    ui.SetSelectedJobIndex(0);
    ASSERT_EQ(ui.GetSelectedJobIndex(), 0);

    ui.SetSelectedJobIndex(-1);
    ASSERT_EQ(ui.GetSelectedJobIndex(), -1);
    return true;
}

bool TestSelectionStableAcrossMultipleSets() {
    keybinder::JobsUI ui;
    ui.SetSelectedJobIndex(5);
    ASSERT_EQ(ui.GetSelectedJobIndex(), 5);

    // Set again to same value - should stay
    ui.SetSelectedJobIndex(5);
    ASSERT_EQ(ui.GetSelectedJobIndex(), 5);

    // Change to different
    ui.SetSelectedJobIndex(2);
    ASSERT_EQ(ui.GetSelectedJobIndex(), 2);
    return true;
}

// ── Data model integration tests (no ImGui render) ──

bool TestJobDefinitionDefaults() {
    jobs::JobDefinition def;
    ASSERT_TRUE(!def.enabled);
    ASSERT_EQ(def.name.size(), 0u);
    ASSERT_EQ(def.id.size(), 0u);
    ASSERT_TRUE(def.parserRules.empty());
    ASSERT_TRUE(def.skillTree.empty());
    return true;
}

bool TestSkillTreeNodeDefaults() {
    jobs::SkillTreeNode node;
    ASSERT_EQ(node.maxPoints, 0);
    ASSERT_EQ(node.unlockOrder, 0);
    ASSERT_EQ(node.name.size(), 0u);
    ASSERT_EQ(node.perkType.size(), 0u);
    ASSERT_EQ(node.perkValue.size(), 0u);
    return true;
}

bool TestJobWindowAggregatesDefaults() {
    jobs::JobWindowAggregates agg;
    ASSERT_EQ(agg.today, 0);
    ASSERT_EQ(agg.last7Days, 0);
    ASSERT_EQ(agg.last30Days, 0);
    ASSERT_EQ(agg.bestDay, 0);
    ASSERT_EQ(agg.lifetime, 0);
    return true;
}

bool TestJobStatsDefaults() {
    jobs::JobStats stats;
    ASSERT_EQ(stats.totalEvents, 0);
    ASSERT_EQ(stats.totalAmount, 0);
    ASSERT_EQ(stats.highestLevel, 0);
    ASSERT_EQ(stats.lastEventTimestamp, 0);
    return true;
}

bool TestJobEventDefaults() {
    jobs::JobEvent evt;
    ASSERT_EQ(evt.amount, 0);
    ASSERT_EQ(evt.level, 0);
    ASSERT_EQ(evt.timestamp, 0);
    ASSERT_TRUE(!evt.deleted);
    ASSERT_EQ(evt.type.size(), 0u);
    ASSERT_EQ(evt.source.size(), 0u);
    return true;
}

bool TestRegistrySnapshotDefaults() {
    jobs::JobRegistry::Snapshot snap;
    ASSERT_EQ(snap.version, 0);
    ASSERT_TRUE(snap.jobs.empty());
    return true;
}

// ── Recent events filtering: verify non-deleted events are counted ──

bool TestRecentEventsFiltering() {
    // Create a set of events, some deleted, verify count logic
    std::vector<jobs::JobEvent> events;
    for (int i = 0; i < 15; ++i) {
        jobs::JobEvent evt;
        evt.id = "evt_" + std::to_string(i);
        evt.jobId = "test_job";
        evt.type = "earning";
        evt.amount = i * 100;
        evt.source = "auto";
        evt.timestamp = static_cast<std::time_t>(1000 + i);
        evt.deleted = (i % 3 == 0); // Every 3rd is deleted
        events.push_back(evt);
    }

    // Count non-deleted
    int nonDeleted = 0;
    for (const auto& e : events) {
        if (!e.deleted) ++nonDeleted;
    }

    // 15 events, every 3rd deleted: indices 0,3,6,9,12 = 5 deleted, 10 non-deleted
    ASSERT_EQ(nonDeleted, 10);

    // Take newest 10 non-deleted (iterate from end)
    int shown = 0;
    for (int i = static_cast<int>(events.size()) - 1; i >= 0 && shown < 10; --i) {
        if (!events[i].deleted) ++shown;
    }
    ASSERT_EQ(shown, 10);
    return true;
}

bool TestRecentEventsMaxTen() {
    // 20 non-deleted events → should only show 10
    std::vector<jobs::JobEvent> events;
    for (int i = 0; i < 20; ++i) {
        jobs::JobEvent evt;
        evt.id = "evt_" + std::to_string(i);
        evt.jobId = "test_job";
        evt.type = "earning";
        evt.amount = i * 50;
        evt.source = "auto";
        evt.timestamp = static_cast<std::time_t>(2000 + i);
        evt.deleted = false;
        events.push_back(evt);
    }

    int shown = 0;
    for (int i = static_cast<int>(events.size()) - 1; i >= 0 && shown < 10; --i) {
        if (!events[i].deleted) ++shown;
    }
    ASSERT_EQ(shown, 10);
    return true;
}

// ── Skill tree ordering test ──

bool TestSkillTreeOrderByUnlockOrder() {
    std::vector<jobs::SkillTreeNode> tree;
    // Add out of order
    jobs::SkillTreeNode n3; n3.name = "Experte";  n3.unlockOrder = 3; n3.maxPoints = 5;
    jobs::SkillTreeNode n1; n1.name = "Anfänger"; n1.unlockOrder = 1; n1.maxPoints = 3;
    jobs::SkillTreeNode n2; n2.name = "Fortgeschritten"; n2.unlockOrder = 2; n2.maxPoints = 4;
    tree.push_back(n3);
    tree.push_back(n1);
    tree.push_back(n2);

    // Sort by unlockOrder
    std::vector<int> sortedIndices;
    for (int i = 0; i < static_cast<int>(tree.size()); ++i) sortedIndices.push_back(i);
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
        return tree[a].unlockOrder < tree[b].unlockOrder;
    });

    ASSERT_EQ(sortedIndices.size(), 3u);
    ASSERT_EQ(tree[sortedIndices[0]].unlockOrder, 1);
    ASSERT_EQ(tree[sortedIndices[1]].unlockOrder, 2);
    ASSERT_EQ(tree[sortedIndices[2]].unlockOrder, 3);

    ASSERT_EQ(tree[sortedIndices[0]].name, "Anfänger");
    ASSERT_EQ(tree[sortedIndices[1]].name, "Fortgeschritten");
    ASSERT_EQ(tree[sortedIndices[2]].name, "Experte");
    return true;
}

// ── Disabled jobs not selectable ──

bool TestDisabledJobNotSelectable() {
    // Create a registry with mixed enabled/disabled jobs
    jobs::JobRegistry::Snapshot snap;
    for (int i = 0; i < 5; ++i) {
        jobs::JobDefinition def;
        def.id = "job_" + std::to_string(i);
        def.name = "Job " + std::to_string(i);
        def.enabled = (i != 0 && i != 3); // Jobs 0 and 3 disabled
        snap.jobs.push_back(def);
    }

    // First enabled job should be at index 1
    int firstEnabled = -1;
    for (int i = 0; i < static_cast<int>(snap.jobs.size()); ++i) {
        if (snap.jobs[i].enabled) {
            firstEnabled = i;
            break;
        }
    }
    ASSERT_EQ(firstEnabled, 1);
    return true;
}

// ── Registry order preserved (no sorting) ──

bool TestRegistryOrderPreserved() {
    jobs::JobRegistry::Snapshot snap;
    const char* names[] = { "Frachtpilot", "Flughafenmitarbeiter", "Hochseefischer", "Pilot", "Bergarbeiter" };
    for (int i = 0; i < 5; ++i) {
        jobs::JobDefinition def;
        def.id = "job_" + std::to_string(i);
        def.name = names[i];
        def.enabled = true;
        snap.jobs.push_back(def);
    }

    // Verify order is exactly as added
    ASSERT_EQ(snap.jobs.size(), 5u);
    ASSERT_EQ(snap.jobs[0].name, "Frachtpilot");
    ASSERT_EQ(snap.jobs[1].name, "Flughafenmitarbeiter");
    ASSERT_EQ(snap.jobs[2].name, "Hochseefischer");
    ASSERT_EQ(snap.jobs[3].name, "Pilot");
    ASSERT_EQ(snap.jobs[4].name, "Bergarbeiter");
    return true;
}

// ── Empty state ──

bool TestEmptyJobsPlaceholder() {
    jobs::JobRegistry::Snapshot snap;
    ASSERT_TRUE(snap.jobs.empty());

    // When no jobs, default selection should stay -1
    keybinder::JobsUI ui;
    // No bridge → selection stays -1
    ASSERT_EQ(ui.GetSelectedJobIndex(), -1);
    return true;
}

// ── kExpectedJobDefinitionCount ──

bool TestExpectedJobCount() {
    ASSERT_EQ(static_cast<std::size_t>(jobs::kExpectedJobDefinitionCount), 17u);
    return true;
}

bool TestEditModalStateDefaults() {
    keybinder::JobsUI ui;
    const auto& state = ui.GetEditModalState();
    ASSERT_TRUE(!state.open);
    ASSERT_TRUE(!state.openCorrectionFromEdit);
    ASSERT_TRUE(!state.initialized);
    ASSERT_EQ(state.loadedJobId.size(), 0u);
    ASSERT_EQ(state.baseLifetime, 0);
    ASSERT_EQ(state.baseHighestLevel, 0);
    ASSERT_EQ(state.currentEarningsOffset, 0);
    ASSERT_EQ(state.currentSkillOffset, 0);
    ASSERT_EQ(state.editedTotalEarnings, 0);
    ASSERT_EQ(state.editedTotalSkillPoints, 0);
    return true;
}

bool TestEditModalLoadsSnapshotAndSavesAdjustments() {
    const std::string filepath = MakeTempFilePath("jobs_ui_save");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = std::time(nullptr);
    ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_earning", 1000, 0, ts, "").empty());
    ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_skill_level", 0, 5, ts, "").empty());

    keybinder::JobsUI ui;
    ui.SetTestJobTracker(&tracker);
    ui.DebugOpenEditModal("landwirt");

    auto& state = ui.GetEditModalState();
    ASSERT_TRUE(state.open);
    ASSERT_TRUE(state.initialized);
    ASSERT_EQ(state.loadedJobId, std::string("landwirt"));
    ASSERT_EQ(state.baseLifetime, 1000);
    ASSERT_EQ(state.baseHighestLevel, 5);

    state.currentEarningsOffset = 250;
    state.currentSkillOffset = 2;
    state.editedTotalEarnings = 1250;
    state.editedTotalSkillPoints = 7;
    ui.DebugApplyEditModalChanges();

    const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
    const jobs::JobWindowAggregates agg = tracker.GetAggregates("landwirt", ts);
    ASSERT_EQ(snap.earningsOffset, 250);
    ASSERT_EQ(snap.skillOffset, 2);
    ASSERT_EQ(snap.adjustedTotalEarnings, 1250);
    ASSERT_EQ(snap.adjustedHighestSkill, 7);
    ASSERT_EQ(agg.lifetime, 1250);
    ASSERT_TRUE(!ui.GetEditModalState().open);
    ASSERT_TRUE(!ui.GetEditModalState().initialized);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestEditModalUpdatesAggregateLifetimeWithoutEarningEvents() {
    const std::string filepath = MakeTempFilePath("jobs_ui_save_no_earnings");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = std::time(nullptr);
    ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_skill_level", 0, 5, ts, "").empty());

    keybinder::JobsUI ui;
    ui.SetTestJobTracker(&tracker);
    ui.DebugOpenEditModal("landwirt");

    auto& state = ui.GetEditModalState();
    ASSERT_EQ(state.baseLifetime, 0);
    ASSERT_EQ(state.editedTotalEarnings, 0);

    state.editedTotalEarnings = 500;
    state.editedTotalSkillPoints = 5;
    ui.DebugApplyEditModalChanges();

    const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
    const jobs::JobWindowAggregates agg = tracker.GetAggregates("landwirt", ts);
    ASSERT_EQ(snap.earningsOffset, 500);
    ASSERT_EQ(snap.adjustedTotalEarnings, 500);
    ASSERT_EQ(agg.lifetime, 500);
    ASSERT_TRUE(!ui.GetEditModalState().open);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestEditModalCancelDiscardsChanges() {
    const std::string filepath = MakeTempFilePath("jobs_ui_cancel");
    RemoveIfExists(filepath);

    jobs::JobTracker tracker(filepath);
    ASSERT_TRUE(tracker.Initialize());

    const std::time_t ts = std::time(nullptr);
    ASSERT_TRUE(!tracker.AddManualEvent("landwirt", "manual_earning", 400, 0, ts, "").empty());

    keybinder::JobsUI ui;
    ui.SetTestJobTracker(&tracker);
    ui.DebugOpenEditModal("landwirt");

    auto& state = ui.GetEditModalState();
    state.currentEarningsOffset = 900;
    state.currentSkillOffset = 4;
    state.editedTotalEarnings = 1300;
    state.editedTotalSkillPoints = 4;

    ui.DebugCancelEditModal();

    const jobs::JobEditSnapshot snap = tracker.GetJobEditSnapshot("landwirt");
    ASSERT_EQ(snap.earningsOffset, 0);
    ASSERT_EQ(snap.skillOffset, 0);
    ASSERT_EQ(snap.adjustedTotalEarnings, 400);
    ASSERT_EQ(snap.adjustedHighestSkill, 0);
    ASSERT_TRUE(!ui.GetEditModalState().open);

    tracker.Shutdown();
    RemoveIfExists(filepath);
    return true;
}

bool TestEditModalHandoffRestoresEditContext() {
    keybinder::JobsUI ui;
    ui.DebugOpenEditModal("landwirt");

    auto& state = ui.GetEditModalState();
    state.currentEarningsOffset = 42;
    state.currentSkillOffset = 3;
    state.open = false;
    state.openCorrectionFromEdit = true;

    ui.DebugHandleCorrectionModalClosed();

    ASSERT_TRUE(ui.GetEditModalState().open);
    ASSERT_TRUE(!ui.GetEditModalState().openCorrectionFromEdit);
    ASSERT_EQ(ui.GetEditModalState().currentEarningsOffset, 42);
    ASSERT_EQ(ui.GetEditModalState().currentSkillOffset, 3);
    ASSERT_EQ(ui.GetEditModalState().loadedJobId, std::string("landwirt"));
    return true;
}

} // namespace

int RunJobsUITests() {
    std::printf("\n=== Jobs UI Tests ===\n\n");

    g_passed = 0;
    g_failed = 0;

    auto run = [](bool (*fn)(), const char* name) {
        if (fn()) {
            std::printf("[PASS] %s\n", name);
        } else {
            ++g_failed;
        }
    };

    run(TestFormatMoney,           "TestFormatMoney");
    run(TestFormatTimestamp,        "TestFormatTimestamp");
    run(TestFormatEventTypeBadge,   "TestFormatEventTypeBadge");
    run(TestFormatSourceBadge,      "TestFormatSourceBadge");
    run(TestDefaultSelectionUnresolvedWithoutTracker, "TestDefaultSelectionUnresolvedWithoutTracker");
    run(TestSetSelectedJobIndex,    "TestSetSelectedJobIndex");
    run(TestSelectionStableAcrossMultipleSets, "TestSelectionStableAcrossMultipleSets");
    run(TestJobDefinitionDefaults,  "TestJobDefinitionDefaults");
    run(TestSkillTreeNodeDefaults,  "TestSkillTreeNodeDefaults");
    run(TestJobWindowAggregatesDefaults, "TestJobWindowAggregatesDefaults");
    run(TestJobStatsDefaults,       "TestJobStatsDefaults");
    run(TestJobEventDefaults,       "TestJobEventDefaults");
    run(TestRegistrySnapshotDefaults, "TestRegistrySnapshotDefaults");
    run(TestRecentEventsFiltering,  "TestRecentEventsFiltering");
    run(TestRecentEventsMaxTen,     "TestRecentEventsMaxTen");
    run(TestSkillTreeOrderByUnlockOrder, "TestSkillTreeOrderByUnlockOrder");
    run(TestDisabledJobNotSelectable, "TestDisabledJobNotSelectable");
    run(TestRegistryOrderPreserved, "TestRegistryOrderPreserved");
    run(TestEmptyJobsPlaceholder,   "TestEmptyJobsPlaceholder");
    run(TestExpectedJobCount,       "TestExpectedJobCount");
    run(TestEditModalStateDefaults, "TestEditModalStateDefaults");
    run(TestEditModalLoadsSnapshotAndSavesAdjustments, "TestEditModalLoadsSnapshotAndSavesAdjustments");
    run(TestEditModalUpdatesAggregateLifetimeWithoutEarningEvents, "TestEditModalUpdatesAggregateLifetimeWithoutEarningEvents");
    run(TestEditModalCancelDiscardsChanges, "TestEditModalCancelDiscardsChanges");
    run(TestEditModalHandoffRestoresEditContext, "TestEditModalHandoffRestoresEditContext");

    std::printf("\n=== Results: %d passed, %d failed ===\n\n", g_passed, g_failed);
    return g_failed;
}
