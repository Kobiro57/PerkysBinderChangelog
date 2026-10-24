#ifndef KEYBINDER_TESTING
#define KEYBINDER_TESTING
#endif

#include "../include/OnlineOverlayController.h"
#include "../include/OnlineOverlayRenderer.h"
#include "../include/OnlineDataManager.h"
#include "../include/OnlinePlayerInfoService.h"
#include "../include/SettingsManager.h"

#include "../include/imgui-1.92.7/imgui.h"
#include "../include/imgui-1.92.7/imgui_internal.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace keybinder;

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

struct ImGuiScope {
    ImGuiScope() {
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui::DestroyContext();
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1920.0f, 1080.0f);
        io.DeltaTime = 1.0f / 60.0f;
        io.FontGlobalScale = 1.0f;

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }

    ~ImGuiScope() {
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui::DestroyContext();
        }
    }

    void BeginFrame(float width = 1920.0f, float height = 1080.0f) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(width, height);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
    }

    void EndFrame() {
        ImGui::Render();
    }
};

OnlinePlayerInfo BuildRawPlayer(const std::string& id,
                                const std::string& name,
                                const std::string& faction = std::string(),
                                const std::string& gang = std::string(),
                                const std::string& teamRank = std::string()) {
    OnlinePlayerInfo player;
    player.id = id;
    player.name = name;
    player.level = 10;
    player.skinId = 7;

    if (!faction.empty()) {
        player.hasFaction = true;
        player.faction = faction;
    }

    if (!gang.empty()) {
        player.hasGang = true;
        player.gang = gang;
    }

    if (!teamRank.empty()) {
        player.hasTeamRank = true;
        player.teamRank = teamRank;
    }

    return player;
}

void PopulateUsableSnapshot(OnlineDataManager& manager) {
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "Finn", "LSPD", std::string(), "Officer"));
    players.push_back(BuildRawPlayer("2", "Alpha", "LSPD"));
    players.push_back(BuildRawPlayer("3", "Berta", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("4", "GroveOne", std::string(), "Grove"));
    players.push_back(BuildRawPlayer("5", "Civvy"));
    manager.UpdateFromPlayers(players, 12345, "finn");
}

ImGuiWindow* FindWindow(const char* name) {
    return ImGui::FindWindowByName(name);
}

bool TestParseModeSupportsHoldAndFallback() {
    ASSERT_EQ(OnlineOverlayRenderer::ParseMode("hold"), OnlineQuickOverlayMode::Hold);
    ASSERT_EQ(OnlineOverlayRenderer::ParseMode("toggle"), OnlineQuickOverlayMode::Toggle);
    ASSERT_EQ(OnlineOverlayRenderer::ParseMode("unexpected"), OnlineQuickOverlayMode::Toggle);
    return true;
}

bool TestShowHideAndToggleRespectSettingsAndCallbacks() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_quick_overlay_enabled = true;
    settings.GetMutable().online_quick_overlay_hotkey = 'O';
    settings.GetMutable().online_quick_overlay_mode = "toggle";
    renderer.Initialize(&settings, &manager);

    renderer.Show();
    ASSERT_TRUE(renderer.IsVisible());

    renderer.Hide();
    ASSERT_TRUE(!renderer.IsVisible());

    renderer.Toggle();
    ASSERT_TRUE(renderer.IsVisible());

    renderer.Hide();
    renderer.SetQuickMapVisibleCallback([]() { return true; });
    renderer.Show();
    ASSERT_TRUE(!renderer.IsVisible());

    renderer.SetQuickMapVisibleCallback([]() { return false; });
    renderer.SetOverlayVisibleCallback([]() { return true; });
    renderer.Show();
    ASSERT_TRUE(!renderer.IsVisible());

    renderer.SetOverlayVisibleCallback([]() { return false; });
    settings.GetMutable().online_quick_overlay_enabled = false;
    renderer.Update();
    renderer.Show();
    ASSERT_TRUE(!renderer.IsVisible());

    settings.GetMutable().online_quick_overlay_enabled = true;
    settings.GetMutable().online_quick_overlay_hotkey = 0;
    renderer.Update();
    renderer.Show();
    ASSERT_TRUE(!renderer.IsVisible());
    return true;
}

bool TestUpdateSuppressesQuickOverlayHotkeyWhileInputGated() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayRenderer renderer;
    bool hotkeyDown = false;
    bool inputGated = false;

    settings.GetMutable().online_quick_overlay_enabled = true;
    settings.GetMutable().online_quick_overlay_hotkey = 'Y';
    settings.GetMutable().online_quick_overlay_mode = "toggle";
    renderer.Initialize(&settings, &manager);
    renderer.SetTestingHooks({
        [&](int key) { return key == 'Y' && hotkeyDown; },
        [&]() { return inputGated; },
    });

    inputGated = true;
    renderer.Show();
    ASSERT_TRUE(!renderer.IsVisible());

    hotkeyDown = true;
    renderer.Update();
    ASSERT_TRUE(!renderer.IsVisible());

    inputGated = false;
    renderer.Update();
    ASSERT_TRUE(!renderer.IsVisible());

    hotkeyDown = false;
    renderer.Update();
    ASSERT_TRUE(!renderer.IsVisible());

    hotkeyDown = true;
    renderer.Update();
    ASSERT_TRUE(renderer.IsVisible());
    return true;
}

bool TestRenderQuickOverlayCreatesWindowForUsableSnapshot() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayRenderer renderer;
    PopulateUsableSnapshot(manager);

    settings.GetMutable().online_quick_overlay_enabled = true;
    settings.GetMutable().online_quick_overlay_hotkey = 'O';
    settings.GetMutable().online_quick_overlay_mode = "toggle";
    renderer.Initialize(&settings, &manager);
    renderer.Show();

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.Render();

    ImGuiWindow* window = FindWindow("##OnlineQuickOverlay");
    ASSERT_TRUE(window != nullptr);
    ASSERT_TRUE(window->Size.x > 0.0f);
    ASSERT_TRUE(window->Size.y > 0.0f);

    imgui.EndFrame();
    return true;
}

bool TestRenderQuickOverlaySkipsOfflineEmptySnapshot() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_quick_overlay_enabled = true;
    settings.GetMutable().online_quick_overlay_hotkey = 'O';
    renderer.Initialize(&settings, &manager);
    renderer.Show();

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.Render();
    ASSERT_TRUE(FindWindow("##OnlineQuickOverlay") == nullptr);
    imgui.EndFrame();
    return true;
}

bool TestPersistentOverlaysDefaultOffWithUsableSnapshots() {
    SettingsManager settings;
    settings.GetMutable().online_enabled = true;

    OnlineDataManager factionManager;
    PopulateUsableSnapshot(factionManager);
    OnlineOverlayController factionController;
    factionController.Initialize(&settings, &factionManager);
    factionController.Update(0.0f);

    ASSERT_TRUE(!factionController.GetLocalOverlayData().visible);
    ASSERT_TRUE(!factionController.GetGangsOverlayData().visible);

    OnlineDataManager gangManager;
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "Berta", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("2", "Other", std::string(), "Grove"));
    gangManager.UpdateFromPlayers(players, 12345, "Berta");

    OnlineOverlayController gangController;
    gangController.Initialize(&settings, &gangManager);
    gangController.Update(0.0f);

    ASSERT_TRUE(!gangController.GetOwnGangOverlayData().visible);
    ASSERT_TRUE(!gangController.GetGangsOverlayData().visible);
    return true;
}

bool TestRenderPersistentOverlaysCreatesLocalAndGangSummaryWindows() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayController controller;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_local_overlay_enabled = true;
    settings.GetMutable().online_gangs_overlay_enabled = true;
    settings.GetMutable().online_overlay_anchor = "top_right";
    settings.GetMutable().online_overlay_style = "card";
    settings.GetMutable().online_overlay_opacity = 0.9f;

    PopulateUsableSnapshot(manager);
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    const PersistentOverlayData& gangsData = controller.GetGangsOverlayData();
    ASSERT_TRUE(gangsData.visible);
    ASSERT_EQ(gangsData.title, std::string("Gangs Online"));
    ASSERT_EQ(gangsData.playerCount, 2);
    ASSERT_EQ(gangsData.countRows.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(gangsData.countRows[0].label, std::string("Ballas"));
    ASSERT_EQ(gangsData.countRows[0].count, 1);
    ASSERT_EQ(gangsData.countRows[1].label, std::string("Grove"));
    ASSERT_EQ(gangsData.countRows[1].count, 1);

    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.RenderPersistentOverlays();

    ImGuiWindow* localWindow = FindWindow("##PersistentOverlay_LSPD");
    ImGuiWindow* gangsWindow = FindWindow("##PersistentOverlay_Gangs Online");
    ASSERT_TRUE(localWindow != nullptr);
    ASSERT_TRUE(gangsWindow != nullptr);
    ASSERT_TRUE(gangsWindow->Pos.y > localWindow->Pos.y);
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Ballas") == nullptr);

    imgui.EndFrame();
    return true;
}

bool TestRenderPersistentOverlaysRespectsOnlineEnabledGate() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayController controller;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_enabled = false;
    settings.GetMutable().online_local_overlay_enabled = true;
    settings.GetMutable().online_gangs_overlay_enabled = true;
    PopulateUsableSnapshot(manager);
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.RenderPersistentOverlays();
    ASSERT_TRUE(FindWindow("##PersistentOverlay_LSPD") == nullptr);
    imgui.EndFrame();
    return true;
}

bool TestPersistentOverlayStyleSettingsPropagateToController() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayController controller;

    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_local_overlay_enabled = true;
    settings.GetMutable().online_overlay_background_enabled = false;
    settings.GetMutable().online_overlay_scale = 1.35f;
    settings.GetMutable().online_overlay_custom_text_color_enabled = true;
    settings.GetMutable().online_overlay_text_color_r = 0.25f;
    settings.GetMutable().online_overlay_text_color_g = 0.50f;
    settings.GetMutable().online_overlay_text_color_b = 0.75f;
    settings.GetMutable().online_overlay_text_color_a = 0.80f;
    PopulateUsableSnapshot(manager);

    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    const PersistentOverlayData& localData = controller.GetLocalOverlayData();
    ASSERT_TRUE(localData.visible);
    ASSERT_TRUE(!localData.backgroundEnabled);
    ASSERT_EQ(localData.scale, 1.35f);
    ASSERT_TRUE(localData.customTextColorEnabled);
    ASSERT_EQ(localData.textColorR, 0.25f);
    ASSERT_EQ(localData.textColorG, 0.50f);
    ASSERT_EQ(localData.textColorB, 0.75f);
    ASSERT_EQ(localData.textColorA, 0.80f);
    return true;
}

bool TestOwnGangOverlayIsSeparateFromSummaryAndUsesOwnStyle() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayController controller;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_local_overlay_enabled = true;
    settings.GetMutable().online_own_gang_overlay_enabled = true;
    settings.GetMutable().online_gangs_overlay_enabled = true;
    settings.GetMutable().online_own_gang_overlay_style = "minimal";
    settings.GetMutable().online_own_gang_overlay_opacity = 0.45f;
    settings.GetMutable().online_own_gang_overlay_background_enabled = false;
    settings.GetMutable().online_own_gang_overlay_scale = 1.25f;
    settings.GetMutable().online_own_gang_overlay_custom_text_color_enabled = true;
    settings.GetMutable().online_own_gang_overlay_text_color_r = 0.2f;

    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "Berta", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("2", "Other", std::string(), "Grove"));
    manager.UpdateFromPlayers(players, 12345, "Berta");

    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    ASSERT_TRUE(!controller.GetLocalOverlayData().visible);
    const PersistentOverlayData& ownGangData = controller.GetOwnGangOverlayData();
    ASSERT_TRUE(ownGangData.visible);
    ASSERT_EQ(ownGangData.title, std::string("Ballas"));
    ASSERT_EQ(ownGangData.style, OnlineOverlayStyle::Minimal);
    ASSERT_EQ(ownGangData.opacity, 0.45f);
    ASSERT_TRUE(!ownGangData.backgroundEnabled);
    ASSERT_EQ(ownGangData.scale, 1.25f);
    ASSERT_TRUE(ownGangData.customTextColorEnabled);
    ASSERT_EQ(ownGangData.textColorR, 0.2f);
    ASSERT_TRUE(controller.GetGangsOverlayData().visible);

    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.RenderPersistentOverlays();

    ImGuiWindow* ownGangWindow = FindWindow("##PersistentOverlay_Ballas");
    ImGuiWindow* gangsWindow = FindWindow("##PersistentOverlay_Gangs Online");
    ASSERT_TRUE(ownGangWindow != nullptr);
    ASSERT_TRUE(gangsWindow != nullptr);
    ASSERT_TRUE(gangsWindow->Pos.y > ownGangWindow->Pos.y);

    imgui.EndFrame();
    return true;
}

bool TestPersistentOverlayMoveModeMakesWindowsInteractive() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayController controller;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_local_overlay_enabled = true;
    settings.GetMutable().online_gangs_overlay_enabled = false;
    PopulateUsableSnapshot(manager);
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);
    renderer.SetPersistentOverlayMoveMode(true);
    ASSERT_TRUE(renderer.IsPersistentOverlayMoveModeActive());

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.RenderPersistentOverlays();

    ImGuiWindow* localWindow = FindWindow("##PersistentOverlay_LSPD");
    ASSERT_TRUE(localWindow != nullptr);
    ASSERT_TRUE((localWindow->Flags & ImGuiWindowFlags_NoMouseInputs) == 0);
    ASSERT_TRUE((localWindow->Flags & ImGuiWindowFlags_NoMove) == 0);

    imgui.EndFrame();
    renderer.SetPersistentOverlayMoveMode(false);
    ASSERT_TRUE(!renderer.IsPersistentOverlayMoveModeActive());
    return true;
}

bool TestPersistentOverlayLongGangNameKeepsSummaryWindowFixed() {
    SettingsManager settings;
    OnlineDataManager manager;
    OnlineOverlayController controller;
    OnlineOverlayRenderer renderer;

    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_local_overlay_enabled = false;
    settings.GetMutable().online_gangs_overlay_enabled = true;

    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "LongGangMember", std::string(), "Extremely Long Gang Name That Would Otherwise Overlap The Count"));
    manager.UpdateFromPlayers(players, 12345, "LongGangMember");

    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);
    ASSERT_TRUE(controller.GetGangsOverlayData().visible);
    ASSERT_EQ(controller.GetGangsOverlayData().countRows[0].label, std::string("Extremely Long Gang Name That Would Otherwise Overlap The Count"));

    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);

    ImGuiScope imgui;
    imgui.BeginFrame();
    renderer.RenderPersistentOverlays();

    ImGuiWindow* gangsWindow = FindWindow("##PersistentOverlay_Gangs Online");
    ASSERT_TRUE(gangsWindow != nullptr);
    ASSERT_TRUE(gangsWindow->Size.x <= 260.0f);

    imgui.EndFrame();
    return true;
}

bool TestWantedOverlayFiltersByRangeAndBuildsCountRows() {
    SettingsManager settings;
    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_wanted_overlay_enabled = true;
    // Range 100-200 should include the 103 and 158 players, exclude 0 and 59.
    settings.GetMutable().online_wanted_overlay_min = 100;
    settings.GetMutable().online_wanted_overlay_max = 200;

    OnlineDataManager manager;
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "ZeroWanted", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("2", "LowWanted", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("3", "MidWanted", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("4", "HighWanted", std::string(), "Ballas"));
    players[0].wanted = 0;
    players[1].wanted = 59;
    players[2].wanted = 103;
    players[3].wanted = 158;
    manager.UpdateFromPlayers(players, 12345, "ZeroWanted");

    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    const PersistentOverlayData& wanted = controller.GetWantedOverlayData();
    ASSERT_TRUE(wanted.visible);
    ASSERT_EQ(wanted.title, std::string("Wanted"));
    ASSERT_EQ(wanted.playerCount, 2);
    ASSERT_EQ(wanted.countRows.size(), static_cast<std::size_t>(2));
    // Sorted by wanted descending: 158 first, then 103.
    ASSERT_EQ(wanted.countRows[0].label, std::string("HighWanted"));
    ASSERT_EQ(wanted.countRows[0].count, 158);
    ASSERT_EQ(wanted.countRows[1].label, std::string("MidWanted"));
    ASSERT_EQ(wanted.countRows[1].count, 103);
    return true;
}

bool TestWantedOverlayMaxZeroMeansNoUpperLimit() {
    SettingsManager settings;
    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_wanted_overlay_enabled = true;
    // min=1, max=0 → no upper limit; all players with wanted >= 1 show.
    settings.GetMutable().online_wanted_overlay_min = 1;
    settings.GetMutable().online_wanted_overlay_max = 0;

    OnlineDataManager manager;
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "Clean", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("2", "LowWanted", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("3", "HighWanted", std::string(), "Ballas"));
    players[0].wanted = 0;
    players[1].wanted = 5;
    players[2].wanted = 999;
    manager.UpdateFromPlayers(players, 12345, "Clean");

    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    const PersistentOverlayData& wanted = controller.GetWantedOverlayData();
    ASSERT_TRUE(wanted.visible);
    ASSERT_EQ(wanted.playerCount, 2);
    return true;
}

bool TestWantedOverlaySwapsInvertedRange() {
    SettingsManager settings;
    settings.GetMutable().online_enabled = true;
    settings.GetMutable().online_wanted_overlay_enabled = true;
    // Inverted range 200-100 should normalize to 100-200, not show nothing.
    settings.GetMutable().online_wanted_overlay_min = 200;
    settings.GetMutable().online_wanted_overlay_max = 100;

    OnlineDataManager manager;
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "In", std::string(), "Ballas"));
    players.push_back(BuildRawPlayer("2", "Out", std::string(), "Ballas"));
    players[0].wanted = 150;
    players[1].wanted = 50;
    manager.UpdateFromPlayers(players, 12345, "In");

    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    const PersistentOverlayData& wanted = controller.GetWantedOverlayData();
    ASSERT_TRUE(wanted.visible);
    ASSERT_EQ(wanted.playerCount, 1);
    ASSERT_EQ(wanted.countRows[0].label, std::string("In"));
    ASSERT_EQ(wanted.countRows[0].count, 150);
    return true;
}

bool TestWantedOverlayDisabledByDefault() {
    SettingsManager settings;
    settings.GetMutable().online_enabled = true;
    // online_wanted_overlay_enabled defaults to false.

    OnlineDataManager manager;
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "Wanted", std::string(), "Ballas"));
    players[0].wanted = 100;
    manager.UpdateFromPlayers(players, 12345, "Wanted");

    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);

    ASSERT_TRUE(!controller.GetWantedOverlayData().visible);
    return true;
}

bool TestGameHudUsesLiveStateAndOriginalAppearance() {
    SettingsManager settings;
    Settings& s = settings.GetMutable();
    s.online_location_overlay_enabled = true;
    s.online_transport_overlay_enabled = true;
    s.online_pedslots_overlay_enabled = true;
    s.online_friends_overlay_enabled = true;
    s.online_enemies_overlay_enabled = true;
    s.online_overlay_style = "dense";
    s.online_overlay_scale = 1.2f;
    s.online_player_markers.push_back({"Alpha", "LSPD", true});
    OnlineGameOverlayState game;
    game.connected = true;
    game.location = "Los Santos | Idlewood";
    game.transport = "Sultan";
    game.streamedPlayers = 7;
    game.players.push_back({42, "Alpha"});
    OnlineDataManager manager;
    PopulateUsableSnapshot(manager);
    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.SetGameStateProvider([&]() { return game; });
    controller.Update(0.0f);
    const auto& overlays = controller.GetGameOverlays();
    ASSERT_EQ(overlays.size(), std::size_t(5));
    ASSERT_EQ(overlays[0].kind, PersistentOverlayKind::Location);
    ASSERT_EQ(overlays[0].playerNames[0], game.location);
    ASSERT_TRUE(!overlays[0].showPlayerCount);
    ASSERT_EQ(overlays[1].playerNames[0], std::string("Sultan"));
    ASSERT_EQ(overlays[2].playerNames[0], std::string("7 Spieler im Stream"));
    ASSERT_EQ(overlays[3].playerCount, 1);
    ASSERT_EQ(overlays[3].playerNames[0], std::string("Freund: Alpha [LSPD] | ID 42"));
    ASSERT_EQ(overlays[3].kind, PersistentOverlayKind::FriendsRadar);
    ASSERT_EQ(overlays[4].kind, PersistentOverlayKind::EnemiesRadar);
    ASSERT_EQ(overlays[4].playerCount, 0);
    ASSERT_EQ(overlays[0].style, OnlineOverlayStyle::Dense);
    ASSERT_EQ(overlays[0].scale, 1.2f);

    OnlineOverlayRenderer renderer;
    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);
    ImGuiScope scope;
    scope.BeginFrame();
    renderer.RenderPersistentOverlays();
    scope.EndFrame();
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Ort") != nullptr);
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Fortbewegungsmittel") != nullptr);
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Pedslots") != nullptr);
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Freunde-Radar") != nullptr);
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Feinde-Radar") != nullptr);
    ASSERT_TRUE(FindWindow("##PersistentOverlay_Spieler-Radar") == nullptr);
    return true;
}

bool TestPlayerListsRespectIndependentFriendAndEnemyVisibility() {
    SettingsManager settings;
    Settings& s = settings.GetMutable();
    s.online_player_markers.push_back({"Enemy", "Ballas", false});
    s.online_player_markers.push_back({"Friend", "LSPD", true});
    OnlineGameOverlayState game;
    game.connected = true;
    game.players.push_back({9, "fRiEnD"});
    game.players.push_back({15, "enemy"});
    OnlineDataManager manager;
    manager.UpdateFromPlayers({BuildRawPlayer("f", "Friend", "LSPD"), BuildRawPlayer("e", "Enemy", "", "Ballas")}, 0);
    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.SetGameStateProvider([&]() { return game; });
    controller.Update(0.0f);
    ASSERT_TRUE(controller.GetGameOverlays().empty());
    ASSERT_EQ(controller.GetMarkerStates().size(), std::size_t(2));
    ASSERT_TRUE(controller.GetMarkerStates()[0].online);
    ASSERT_TRUE(controller.GetMarkerStates()[1].online);

    s.online_friends_overlay_enabled = true;
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetGameOverlays().size(), std::size_t(1));
    ASSERT_EQ(controller.GetGameOverlays()[0].playerCount, 1);
    ASSERT_EQ(controller.GetGameOverlays()[0].kind, PersistentOverlayKind::FriendsRadar);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Freund: fRiEnD [LSPD] | ID 9"));

    s.online_friends_overlay_enabled = false;
    s.online_enemies_overlay_enabled = true;
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerCount, 1);
    ASSERT_EQ(controller.GetGameOverlays()[0].kind, PersistentOverlayKind::EnemiesRadar);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Feind: enemy [Ballas] | ID 15"));

    s.online_friends_overlay_enabled = true;
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetGameOverlays().size(), std::size_t(2));
    ASSERT_EQ(controller.GetGameOverlays()[0].kind, PersistentOverlayKind::FriendsRadar);
    ASSERT_EQ(controller.GetGameOverlays()[1].kind, PersistentOverlayKind::EnemiesRadar);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerCount, 1);
    ASSERT_EQ(controller.GetGameOverlays()[1].playerCount, 1);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Freund: fRiEnD [LSPD] | ID 9"));
    ASSERT_EQ(controller.GetGameOverlays()[1].playerNames[0], std::string("Feind: enemy [Ballas] | ID 15"));
    return true;
}

bool TestPlayerListsRefreshPresenceAndClearOnDisconnectOrDisable() {
    SettingsManager settings;
    settings.GetMutable().online_enemies_overlay_enabled = true;
    settings.GetMutable().online_player_markers.push_back({"Enemy", "", false});
    OnlineDataManager manager;
    OnlineOverlayController controller;
    OnlineGameOverlayState game;
    game.connected = true;
    controller.Initialize(&settings, &manager);
    int captures = 0;
    controller.SetGameStateProvider([&]() { ++captures; return game; });
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerCount, 0);
    ASSERT_TRUE(!controller.GetMarkerStates()[0].online);

    game.players.push_back({3, "Enemy"});
    controller.Update(0.6f);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Feind: Enemy [Unbekannt] | ID 3"));
    ASSERT_TRUE(controller.GetMarkerStates()[0].online);
    game.players.clear();
    controller.Update(0.6f);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerCount, 0);
    ASSERT_TRUE(!controller.GetMarkerStates()[0].online);

    game = {};
    controller.Update(0.6f);
    ASSERT_TRUE(controller.GetGameOverlays().empty());
    ASSERT_TRUE(controller.GetMarkerStates().empty());
    game.connected = true;
    game.players.push_back({4, "Enemy"});
    controller.Update(0.6f);
    ASSERT_TRUE(controller.GetMarkerStates()[0].online);
    const int capturesBeforeDisable = captures;
    settings.GetMutable().online_enabled = false;
    controller.Update(2.1f);
    ASSERT_TRUE(controller.GetGameOverlays().empty());
    ASSERT_TRUE(controller.GetGameState().players.empty());
    ASSERT_EQ(captures, capturesBeforeDisable);
    return true;
}
bool TestNewGameHudRemainsInsideSmallViewportWithTwentyMarkers() {
    SettingsManager settings;
    Settings& s = settings.GetMutable();
    s.online_location_overlay_enabled = true;
    s.online_transport_overlay_enabled = true;
    s.online_pedslots_overlay_enabled = true;
    s.online_friends_overlay_enabled = true;
    s.online_enemies_overlay_enabled = true;
    s.online_overlay_scale = 1.5f;
    s.online_location_overlay_offset_x = -500.0f;
    s.online_transport_overlay_offset_y = 5000.0f;
    OnlineGameOverlayState game;
    game.connected = true;
    game.location = "Los Santos | Idlewood";
    game.transport = "Sultan";
    for (int i = 0; i < 20; ++i) {
        const std::string name = "Player" + std::to_string(i);
        s.online_player_markers.push_back({name, "Ballas", false});
        game.players.push_back({i, name});
    }
    OnlineDataManager manager;
    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.SetGameStateProvider([&]() { return game; });
    controller.Update(0.0f);
    OnlineOverlayRenderer renderer;
    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);
    ImGuiScope scope;
    const ImVec2 dimensions[] = {ImVec2(1280.0f, 720.0f), ImVec2(640.0f, 360.0f)};
    for (const ImVec2& dimensionsForFrame : dimensions) {
        scope.BeginFrame(dimensionsForFrame.x, dimensionsForFrame.y);
        renderer.RenderPersistentOverlays();
        scope.EndFrame();
        for (const char* name : {"##PersistentOverlay_Ort", "##PersistentOverlay_Fortbewegungsmittel", "##PersistentOverlay_Pedslots", "##PersistentOverlay_Freunde-Radar", "##PersistentOverlay_Feinde-Radar"}) {
            const ImGuiWindow* window = FindWindow(name);
            ASSERT_TRUE(window != nullptr);
            ASSERT_TRUE(window->Pos.x >= 0.0f && window->Pos.y >= 0.0f);
            ASSERT_TRUE(window->Pos.x + window->Size.x <= dimensionsForFrame.x + 0.5f);
            ASSERT_TRUE(window->Pos.y + window->Size.y <= dimensionsForFrame.y + 0.5f);
        }
    }
    return true;
}

bool TestPlayerAffiliationAlwaysUsesCurrentSnapshotRatherThanSavedNotes() {
    SettingsManager settings;
    settings.GetMutable().online_friends_overlay_enabled = true;
    settings.GetMutable().online_player_markers.push_back({"Friend", "Old manual note", true});
    OnlineDataManager manager;
    manager.UpdateFromPlayers({BuildRawPlayer("account", "fRiEnD", "Police Department")}, 0);
    OnlineOverlayController controller;
    OnlineGameOverlayState game;
    game.connected = true;
    game.players.push_back({12, "FRIEND"});
    controller.Initialize(&settings, &manager);
    controller.SetGameStateProvider([&]() { return game; });
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Freund: FRIEND [LSPD] | ID 12"));

    manager.UpdateFromPlayers({BuildRawPlayer("account", "Friend", "", "Ballas")}, 0);
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetMarkerStates()[0].affiliation, std::string("Ballas"));
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Freund: FRIEND [Ballas] | ID 12"));

    manager.UpdateFromPlayers({BuildRawPlayer("account", "Friend")}, 0);
    game.players[0].id = 9;
    controller.Update(0.6f);
    ASSERT_EQ(controller.GetGameOverlays()[0].playerNames[0], std::string("Freund: FRIEND [Zivilist] | ID 9"));

    manager.UpdateFromPlayers({BuildRawPlayer("account", "Friend", "Red County Brotherhood")}, 0);
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetMarkerStates()[0].affiliation, std::string("Red County Brotherhood"));
    OnlineSnapshot snapshot = manager.GetSnapshot();
    snapshot.fetchStatus = OnlineFetchStatus::Offline;
    manager.SetSnapshotForTesting(snapshot);
    controller.Update(0.0f);
    ASSERT_EQ(controller.GetMarkerStates()[0].affiliation, std::string("Unbekannt"));
    ASSERT_EQ(OnlineOverlayController::ResolveMarkerAffiliation(OnlineSnapshot{}, "missing"), std::string("Unbekannt"));
    return true;
}

bool TestFriendAndEnemyRadarMoveAndPersistIndependently() {
    SettingsManager settings;
    auto& s = settings.GetMutable();
    s.online_friends_overlay_enabled = true;
    s.online_enemies_overlay_enabled = true;
    s.online_friends_overlay_anchor = "top_left";
    s.online_friends_overlay_offset_x = 40.0f;
    s.online_friends_overlay_offset_y = 80.0f;
    s.online_enemies_overlay_anchor = "bottom_right";
    s.online_enemies_overlay_offset_x = 25.0f;
    s.online_enemies_overlay_offset_y = 35.0f;
    s.online_player_markers.push_back({"Alpha", "", true});
    s.online_player_markers.push_back({"Berta", "", false});
    OnlineDataManager manager;
    PopulateUsableSnapshot(manager);
    OnlineGameOverlayState game;
    game.connected = true;
    game.players = {{5, "Alpha"}, {6, "Berta"}};
    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.SetGameStateProvider([&]() { return game; });
    controller.Update(0.0f);
    OnlineOverlayRenderer renderer;
    renderer.Initialize(&settings, &manager);
    renderer.SetOverlayController(&controller);

    ImGuiScope scope;
    scope.BeginFrame();
    renderer.RenderPersistentOverlays();
    scope.EndFrame();
    const ImGuiWindow* friends = FindWindow("##PersistentOverlay_Freunde-Radar");
    const ImGuiWindow* enemies = FindWindow("##PersistentOverlay_Feinde-Radar");
    ASSERT_TRUE(friends != nullptr && enemies != nullptr && friends != enemies);
    ASSERT_EQ(friends->Pos.x, 40.0f);
    ASSERT_EQ(friends->Pos.y, 80.0f);
    ASSERT_EQ(enemies->Pos.x, 1920.0f - enemies->Size.x - 25.0f);
    ASSERT_EQ(enemies->Pos.y, 1080.0f - enemies->Size.y - 35.0f);
    const ImVec2 friendSize = friends->Size;
    const ImVec2 enemySize = enemies->Size;

    renderer.SetPersistentOverlayMoveMode(true);
    scope.BeginFrame();
    renderer.RenderPersistentOverlays();
    ASSERT_TRUE((friends->Flags & ImGuiWindowFlags_NoMove) == 0);
    ASSERT_TRUE((enemies->Flags & ImGuiWindowFlags_NoMove) == 0);
    renderer.PersistMovedOverlayPositionForTesting(controller.GetGameOverlays()[0], ImVec2(1300.0f, 800.0f), friendSize);
    ASSERT_EQ(s.online_friends_overlay_anchor, std::string("bottom_right"));
    ASSERT_EQ(s.online_friends_overlay_offset_x, 620.0f - friendSize.x);
    ASSERT_EQ(s.online_friends_overlay_offset_y, 280.0f - friendSize.y);
    ASSERT_EQ(s.online_enemies_overlay_anchor, std::string("bottom_right"));
    ASSERT_EQ(s.online_enemies_overlay_offset_x, 25.0f);
    ASSERT_EQ(s.online_enemies_overlay_offset_y, 35.0f);
    renderer.PersistMovedOverlayPositionForTesting(controller.GetGameOverlays()[1], ImVec2(100.0f, 160.0f), enemySize);
    ASSERT_EQ(s.online_enemies_overlay_anchor, std::string("top_left"));
    ASSERT_EQ(s.online_enemies_overlay_offset_x, 100.0f);
    ASSERT_EQ(s.online_enemies_overlay_offset_y, 160.0f);
    ASSERT_EQ(s.online_friends_overlay_offset_x, 620.0f - friendSize.x);
    ASSERT_EQ(s.online_friends_overlay_offset_y, 280.0f - friendSize.y);
    scope.EndFrame();

    SettingsManager reloaded;
    ASSERT_TRUE(reloaded.LoadFromJson(settings.GetSettingsJson()));
    ASSERT_EQ(reloaded.Get().online_friends_overlay_anchor, std::string("bottom_right"));
    ASSERT_EQ(reloaded.Get().online_enemies_overlay_anchor, std::string("top_left"));
    ASSERT_EQ(reloaded.Get().online_friends_overlay_offset_x, s.online_friends_overlay_offset_x);
    ASSERT_EQ(reloaded.Get().online_enemies_overlay_offset_x, 100.0f);
    renderer.SetPersistentOverlayMoveMode(false);
    controller.Update(0.0f);
    scope.BeginFrame();
    renderer.RenderPersistentOverlays();
    scope.EndFrame();
    ASSERT_EQ(friends->Pos.x, 1300.0f);
    ASSERT_EQ(friends->Pos.y, 800.0f);
    ASSERT_EQ(enemies->Pos.x, 100.0f);
    ASSERT_EQ(enemies->Pos.y, 160.0f);
    return true;
}

bool TestRedCountyBrotherhoodMembershipAndQuickFilter() {
    SettingsManager settings;
    Settings& s = settings.GetMutable();
    s.online_local_overlay_enabled = true;
    s.online_own_gang_overlay_enabled = true;
    s.online_quick_overlay_show_lspd = false;
    s.online_quick_overlay_show_army = false;
    s.online_quick_overlay_show_fire_department = false;
    s.online_quick_overlay_show_rcb = false;
    s.online_quick_overlay_show_zivilisten = false;
    s.online_quick_overlay_show_all_gangs = false;
    OnlineDataManager manager;
    std::vector<OnlinePlayerInfo> players;
    players.push_back(BuildRawPlayer("1", "Brother", "RCB", "Ballas"));
    players.push_back(BuildRawPlayer("2", "Sister", "Red County Brotherhood"));
    players.push_back(BuildRawPlayer("3", "Officer", "LSPD"));
    players.push_back(BuildRawPlayer("4", "Civilian"));
    manager.UpdateFromPlayers(players, 12345, "brother");
    const OnlineSnapshot snapshot = manager.GetSnapshot();
    ASSERT_TRUE(snapshot.localAffiliation.isFaction);
    ASSERT_EQ(snapshot.localAffiliation.name, std::string("Red County Brotherhood"));
    ASSERT_TRUE(snapshot.players[0].isFaction);
    ASSERT_TRUE(!snapshot.players[0].isGang);

    OnlineOverlayController controller;
    controller.Initialize(&settings, &manager);
    controller.Update(0.0f);
    ASSERT_TRUE(controller.GetLocalOverlayData().visible);
    ASSERT_EQ(controller.GetLocalOverlayData().title, std::string("Red County Brotherhood"));
    ASSERT_EQ(controller.GetLocalOverlayData().playerCount, 2);
    ASSERT_TRUE(!controller.GetOwnGangOverlayData().visible);
    OnlineOverlayRenderer renderer;
    renderer.Initialize(&settings, &manager);
    ASSERT_EQ(renderer.CountFilteredPlayersForTesting(snapshot), 0);
    s.online_quick_overlay_show_rcb = true;
    renderer.Update();
    ASSERT_EQ(renderer.CountFilteredPlayersForTesting(snapshot), 2);
    return true;
}

} // namespace

int RunOnlineOverlayRendererTests() {
    g_state = {};

    RUN_TEST(TestParseModeSupportsHoldAndFallback);
    RUN_TEST(TestShowHideAndToggleRespectSettingsAndCallbacks);
    RUN_TEST(TestUpdateSuppressesQuickOverlayHotkeyWhileInputGated);
    RUN_TEST(TestRenderQuickOverlayCreatesWindowForUsableSnapshot);
    RUN_TEST(TestRenderQuickOverlaySkipsOfflineEmptySnapshot);
    RUN_TEST(TestPersistentOverlaysDefaultOffWithUsableSnapshots);
    RUN_TEST(TestRenderPersistentOverlaysCreatesLocalAndGangSummaryWindows);
    RUN_TEST(TestRenderPersistentOverlaysRespectsOnlineEnabledGate);
    RUN_TEST(TestPersistentOverlayStyleSettingsPropagateToController);
    RUN_TEST(TestPersistentOverlayMoveModeMakesWindowsInteractive);
    RUN_TEST(TestPersistentOverlayLongGangNameKeepsSummaryWindowFixed);
    RUN_TEST(TestOwnGangOverlayIsSeparateFromSummaryAndUsesOwnStyle);
    RUN_TEST(TestWantedOverlayFiltersByRangeAndBuildsCountRows);
    RUN_TEST(TestWantedOverlayMaxZeroMeansNoUpperLimit);
    RUN_TEST(TestWantedOverlaySwapsInvertedRange);
    RUN_TEST(TestWantedOverlayDisabledByDefault);
    RUN_TEST(TestGameHudUsesLiveStateAndOriginalAppearance);
    RUN_TEST(TestPlayerListsRespectIndependentFriendAndEnemyVisibility);
    RUN_TEST(TestPlayerListsRefreshPresenceAndClearOnDisconnectOrDisable);
    RUN_TEST(TestNewGameHudRemainsInsideSmallViewportWithTwentyMarkers);
    RUN_TEST(TestPlayerAffiliationAlwaysUsesCurrentSnapshotRatherThanSavedNotes);
    RUN_TEST(TestFriendAndEnemyRadarMoveAndPersistIndependently);
    RUN_TEST(TestRedCountyBrotherhoodMembershipAndQuickFilter);
    std::printf("OnlineOverlayRendererTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
