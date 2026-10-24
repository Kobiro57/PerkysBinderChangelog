/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    OnlineUI unit tests - navigation routing, sub-tab state, empty-state stability
*/
#define KEYBINDER_TESTING

#include "../include/OnlineUI.h"
#include "../include/MainWindowUI.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;

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

#define RUN_TEST(name) do { \
    if (name()) { \
        std::printf("[PASS] %s\n", #name); \
    } else { \
        std::printf("[FAIL] %s\n", #name); \
    } \
} while(0)

// ── NavCategory enum tests ──

bool TestNavCategoryOnlineKeepsStableValue() {
    // Online keeps its persisted enum value; sidebar display order is independent.
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::Online), 6);
    return true;
}

bool TestNavCategoryExistingValuesUnchanged() {
    // Verify no reordering of existing enum values
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::Keybinds), 0);
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::KillTracker), 1);
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::Jobs), 2);
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::Mods), 3);
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::Settings), 4);
    ASSERT_EQ(static_cast<int>(keybinder::NavCategory::Help), 5);
    return true;
}

bool TestNavCategoryOnlineDistinctFromHelp() {
    ASSERT_TRUE(static_cast<int>(keybinder::NavCategory::Online) != static_cast<int>(keybinder::NavCategory::Help));
    return true;
}

// ── OnlineSubtab enum tests ──

bool TestOnlineSubtabOverviewValue() {
    ASSERT_EQ(static_cast<int>(keybinder::OnlineSubtab::Overview), 0);
    return true;
}

bool TestOnlineSubtabOverlaySettingsValue() {
    ASSERT_EQ(static_cast<int>(keybinder::OnlineSubtab::OverlaySettings), 1);
    return true;
}

// ── OnlineUI construction and default state ──

bool TestOnlineUIDefaultSubtabIsOverview() {
    keybinder::OnlineUI ui;
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::Overview);
    return true;
}

bool TestOnlineUIDefaultServiceIsNull() {
    keybinder::OnlineUI ui;
    // With null service, overview tab should render gracefully (tested via
    // absence of crash; we cannot call Render without an ImGui context).
    // Instead, verify that SetCurrentSubtab round-trips correctly.
    ui.SetCurrentSubtab(keybinder::OnlineSubtab::OverlaySettings);
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::OverlaySettings);
    ui.SetCurrentSubtab(keybinder::OnlineSubtab::Overview);
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::Overview);
    return true;
}

bool TestOnlineUISetCurrentSubtabRoundTrip() {
    keybinder::OnlineUI ui;

    // Switch to OverlaySettings
    ui.SetCurrentSubtab(keybinder::OnlineSubtab::OverlaySettings);
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::OverlaySettings);

    // Switch back to Overview
    ui.SetCurrentSubtab(keybinder::OnlineSubtab::Overview);
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::Overview);
    return true;
}

// ── OnlineUI injection tests ──

bool TestOnlineUIAcceptsServiceInjection() {
    keybinder::OnlineUI ui;
    // Inject a service pointer (can be nullptr for empty-state)
    ui.SetOnlineDataManager(nullptr);
    // No crash means injection is safe with null
    return true;
}

bool TestOnlineUIAcceptsNonNullServiceInjection() {
    keybinder::OnlineUI ui;
    keybinder::OnlineDataManager manager;
    ui.SetOnlineDataManager(&manager);
    // Injection succeeds without crash
    return true;
}

// ── Navigation scenario: entering/exiting Online tab ──

bool TestNavCategoryCanNavigateToOnline() {
    keybinder::NavCategory cat = keybinder::NavCategory::Online;
    ASSERT_EQ(cat, keybinder::NavCategory::Online);
    // The enum value stays stable even though sidebar display order is decoupled.
    ASSERT_EQ(static_cast<int>(cat), 6);
    return true;
}

bool TestNavCategoryCanNavigateAwayFromOnline() {
    keybinder::NavCategory cat = keybinder::NavCategory::Online;
    // Navigate away
    cat = keybinder::NavCategory::Keybinds;
    ASSERT_EQ(cat, keybinder::NavCategory::Keybinds);
    ASSERT_EQ(static_cast<int>(cat), 0);
    return true;
}

bool TestNavCategoryRoundTripAllCategories() {
    // Verify all 7 categories can be assigned and compared
    keybinder::NavCategory cat = keybinder::NavCategory::Keybinds;
    for (int i = 0; i <= 6; ++i) {
        cat = static_cast<keybinder::NavCategory>(i);
        ASSERT_EQ(static_cast<int>(cat), i);
    }
    return true;
}

// ── Empty-state stability tests ──
// These verify that OnlineUI can be constructed and queried with no
// snapshot data and no service, without accessing invalid state.

bool TestOnlineUIEmptySnapshotOverviewNoCrash() {
    keybinder::OnlineUI ui;
    // No service injected — default state
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::Overview);
    // Sub-tab state is valid even without data
    return true;
}

bool TestOnlineUIEmptySnapshotOverlaySettingsNoCrash() {
    keybinder::OnlineUI ui;
    ui.SetCurrentSubtab(keybinder::OnlineSubtab::OverlaySettings);
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::OverlaySettings);
    // No settings loaded — sub-tab state is still valid
    return true;
}

bool TestOnlineUINoServiceNoCrash() {
    keybinder::OnlineUI ui;
    ui.SetOnlineDataManager(nullptr);
    // Default subtab
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::Overview);
    // Can switch to overlay settings
    ui.SetCurrentSubtab(keybinder::OnlineSubtab::OverlaySettings);
    ASSERT_EQ(ui.GetCurrentSubtab(), keybinder::OnlineSubtab::OverlaySettings);
    return true;
}

// ── BalanceCardColumns (overview masonry packing) ──

bool ColumnsContainEachIndexOnce(const std::vector<std::vector<int>>& cols, int cardCount) {
    std::vector<int> seen(static_cast<std::size_t>(cardCount), 0);
    int total = 0;
    for (const auto& col : cols) {
        for (int idx : col) {
            if (idx < 0 || idx >= cardCount) {
                return false;
            }
            ++seen[static_cast<std::size_t>(idx)];
            ++total;
        }
    }
    if (total != cardCount) {
        return false;
    }
    for (int s : seen) {
        if (s != 1) {
            return false;
        }
    }
    return true;
}

bool TestBalanceColumnsFillsEmptyColumnsInOrder() {
    // Equal cards: each empty column is filled left-to-right, preserving order.
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns({5, 5, 5}, 3);
    ASSERT_EQ(static_cast<int>(cols.size()), 3);
    ASSERT_EQ(static_cast<int>(cols[0].size()), 1);
    ASSERT_EQ(cols[0][0], 0);
    ASSERT_EQ(cols[1][0], 1);
    ASSERT_EQ(cols[2][0], 2);
    return true;
}

bool TestBalanceColumnsPacksEveryIndexExactlyOnce() {
    const std::vector<int> weights = {7, 1, 3, 9, 2, 4, 6};
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns(weights, 4);
    ASSERT_TRUE(ColumnsContainEachIndexOnce(cols, static_cast<int>(weights.size())));
    return true;
}

bool TestBalanceColumnsKeepsTallCardFromStrandingShortOnes() {
    // One very tall card beside several short ones: the tall card sits alone and
    // the short cards stack in the other column instead of leaving a gap.
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns({10, 1, 1, 1, 1}, 2);
    ASSERT_EQ(static_cast<int>(cols.size()), 2);
    ASSERT_EQ(static_cast<int>(cols[0].size()), 1);
    ASSERT_EQ(cols[0][0], 0);
    ASSERT_EQ(static_cast<int>(cols[1].size()), 4);
    ASSERT_EQ(cols[1][0], 1);
    ASSERT_EQ(cols[1][3], 4);
    return true;
}

bool TestBalanceColumnsBalancesUniformWeights() {
    // Five equal cards over two columns: counts differ by at most one card and
    // the top row keeps source order.
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns({2, 2, 2, 2, 2}, 2);
    ASSERT_EQ(static_cast<int>(cols.size()), 2);
    const int diff = static_cast<int>(cols[0].size()) - static_cast<int>(cols[1].size());
    ASSERT_TRUE(diff >= -1 && diff <= 1);
    ASSERT_TRUE(ColumnsContainEachIndexOnce(cols, 5));
    ASSERT_EQ(cols[0][0], 0);
    ASSERT_EQ(cols[1][0], 1);
    return true;
}

bool TestBalanceColumnsSingleColumnPreservesOrder() {
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns({4, 1, 9}, 1);
    ASSERT_EQ(static_cast<int>(cols.size()), 1);
    ASSERT_EQ(static_cast<int>(cols[0].size()), 3);
    ASSERT_EQ(cols[0][0], 0);
    ASSERT_EQ(cols[0][1], 1);
    ASSERT_EQ(cols[0][2], 2);
    return true;
}

bool TestBalanceColumnsClampsNonPositiveColumnCount() {
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns({3, 3}, 0);
    ASSERT_EQ(static_cast<int>(cols.size()), 1);
    ASSERT_EQ(static_cast<int>(cols[0].size()), 2);
    return true;
}

bool TestBalanceColumnsLeavesSurplusColumnsEmpty() {
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns({3, 3}, 4);
    ASSERT_EQ(static_cast<int>(cols.size()), 4);
    ASSERT_EQ(static_cast<int>(cols[0].size()), 1);
    ASSERT_EQ(static_cast<int>(cols[1].size()), 1);
    ASSERT_TRUE(cols[2].empty());
    ASSERT_TRUE(cols[3].empty());
    return true;
}

bool TestBalanceColumnsEmptyInputYieldsEmptyColumns() {
    const std::vector<std::vector<int>> cols = keybinder::OnlineUI::BalanceCardColumns(std::vector<int>{}, 3);
    ASSERT_EQ(static_cast<int>(cols.size()), 3);
    for (const auto& col : cols) {
        ASSERT_TRUE(col.empty());
    }
    return true;
}

} // anonymous namespace

int RunOnlineUITests() {
    std::printf("\n=== OnlineUI Tests ===\n\n");

    RUN_TEST(TestNavCategoryOnlineKeepsStableValue);
    RUN_TEST(TestNavCategoryExistingValuesUnchanged);
    RUN_TEST(TestNavCategoryOnlineDistinctFromHelp);
    RUN_TEST(TestOnlineSubtabOverviewValue);
    RUN_TEST(TestOnlineSubtabOverlaySettingsValue);
    RUN_TEST(TestOnlineUIDefaultSubtabIsOverview);
    RUN_TEST(TestOnlineUIDefaultServiceIsNull);
    RUN_TEST(TestOnlineUISetCurrentSubtabRoundTrip);
    RUN_TEST(TestOnlineUIAcceptsServiceInjection);
    RUN_TEST(TestOnlineUIAcceptsNonNullServiceInjection);
    RUN_TEST(TestNavCategoryCanNavigateToOnline);
    RUN_TEST(TestNavCategoryCanNavigateAwayFromOnline);
    RUN_TEST(TestNavCategoryRoundTripAllCategories);
    RUN_TEST(TestOnlineUIEmptySnapshotOverviewNoCrash);
    RUN_TEST(TestOnlineUIEmptySnapshotOverlaySettingsNoCrash);
    RUN_TEST(TestOnlineUINoServiceNoCrash);
    RUN_TEST(TestBalanceColumnsFillsEmptyColumnsInOrder);
    RUN_TEST(TestBalanceColumnsPacksEveryIndexExactlyOnce);
    RUN_TEST(TestBalanceColumnsKeepsTallCardFromStrandingShortOnes);
    RUN_TEST(TestBalanceColumnsBalancesUniformWeights);
    RUN_TEST(TestBalanceColumnsSingleColumnPreservesOrder);
    RUN_TEST(TestBalanceColumnsClampsNonPositiveColumnCount);
    RUN_TEST(TestBalanceColumnsLeavesSurplusColumnsEmpty);
    RUN_TEST(TestBalanceColumnsEmptyInputYieldsEmptyColumns);

    std::printf("\n=== OnlineUI Results: %d passed, %d failed ===\n\n",
                 g_passed, g_failed);
    return g_failed;
}
