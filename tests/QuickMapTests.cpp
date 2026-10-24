#define KEYBINDER_TESTING

#include "../include/QuickMap.h"
#include "../include/SettingsManager.h"
#include "../include/RadarSpriteResources.h"
#include "../include/ResourceIds.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CRadar.h"

#include <cmath>
#include <cstdio>
#include <string>

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

bool NearlyEqual(float actual, float expected, float epsilon = 0.01f) {
    return std::fabs(actual - expected) <= epsilon;
}

bool AssertVec2Near(const ImVec2& actual, const ImVec2& expected, float epsilon = 0.01f) {
    if (!NearlyEqual(actual.x, expected.x, epsilon) || !NearlyEqual(actual.y, expected.y, epsilon)) {
        g_state.error = "Vector mismatch";
        return false;
    }

    g_state.passed += 2;
    return true;
}

bool TestRadarSpriteResourceMapping() {
    const RadarSpriteResource* waypoint = FindRadarSpriteResource(RADAR_SPRITE_WAYPOINT);
    ASSERT_TRUE(waypoint != nullptr);
    ASSERT_EQ(waypoint->radar1ResourceId, IDR_RADAR_WAYPOINT);
    ASSERT_EQ(waypoint->radar2ResourceId, IDR_RADAR2_WAYPOINT);

    const RadarSpriteResource* centre = FindRadarSpriteResource(RADAR_SPRITE_CENTRE);
    ASSERT_TRUE(centre != nullptr);
    ASSERT_EQ(centre->radar1ResourceId, IDR_RADAR_CENTRE);
    ASSERT_EQ(centre->radar2ResourceId, IDR_RADAR_CENTRE);

    const RadarSpriteResource* mapHere = FindRadarSpriteResource(RADAR_SPRITE_MAP_HERE);
    ASSERT_TRUE(mapHere != nullptr);
    ASSERT_EQ(mapHere->radar1ResourceId, IDR_RADAR_MAP_HERE);
    ASSERT_EQ(mapHere->radar2ResourceId, IDR_RADAR2_MAP_HERE);
    ASSERT_TRUE(!mapHere->allowTextureNameMatch);

    ASSERT_TRUE(FindRadarSpriteResource(RADAR_SPRITE_WHITE) == nullptr);
    ASSERT_TRUE(FindRadarSpriteResource(RADAR_SPRITE_NONE) == nullptr);
    return true;
}

bool TestRadarSpriteTextureNameLookup() {
    const RadarSpriteResource* burger = FindRadarSpriteResourceByTextureName("hud:radar_burgerShot");
    ASSERT_TRUE(burger != nullptr);
    ASSERT_EQ(burger->radarSprite, RADAR_SPRITE_BURGERSHOT);
    ASSERT_EQ(burger->radar2ResourceId, IDR_RADAR2_BURGERSHOT);

    const RadarSpriteResource* hostpital = FindRadarSpriteResourceByTextureName("RADAR_HOSTPITAL.PNG");
    ASSERT_TRUE(hostpital != nullptr);
    ASSERT_EQ(hostpital->radarSprite, RADAR_SPRITE_HOSTPITAL);

    ASSERT_TRUE(FindRadarSpriteResourceByTextureName("LD_SPAC:white") == nullptr);
    return true;
}

bool TestRadarSpriteActivePackResolution() {
    Settings settings;
    ASSERT_EQ(ResolveActiveRadarSpritePack(nullptr), RadarSpritePack::Radar1);

    settings.quickmap_radar_pack = "radar2";
    ASSERT_EQ(ResolveActiveRadarSpritePack(&settings), RadarSpritePack::Radar2);

    settings.quickmap_radar_pack = "unexpected";
    ASSERT_EQ(ResolveActiveRadarSpritePack(&settings), RadarSpritePack::Radar1);
    ASSERT_EQ(NormalizeRadarSpritePack("RADAR2"), RadarSpritePack::Radar2);
    return true;
}

bool TestProjectMarkerPreservesRadarSpriteMetadata() {
    QuickMapMarker marker;
    marker.type = QuickMapMarkerType::MapIcon;
    marker.worldX = 125.0f;
    marker.worldY = -250.0f;
    marker.radarSprite = RADAR_SPRITE_POLICE;
    marker.blipType = BLIP_COORD;

    const MapMarker projected = QuickMap::ProjectMarker(marker, ImVec2(10.0f, 20.0f), ImVec2(600.0f, 600.0f));
    ASSERT_EQ(projected.type, QuickMapMarkerType::MapIcon);
    ASSERT_EQ(projected.radarSprite, static_cast<unsigned char>(RADAR_SPRITE_POLICE));
    ASSERT_EQ(projected.blipType, static_cast<unsigned char>(BLIP_COORD));
    return true;
}

bool TestWorldToMapReferencePoints() {
    const ImVec2 origin(100.0f, 50.0f);
    const ImVec2 size(1600.0f, 1457.0f);

    if (!AssertVec2Near(QuickMap::WorldToMap(-3000.0f, 3000.0f, origin, size), ImVec2(100.0f, 50.0f))) {
        return false;
    }

    if (!AssertVec2Near(QuickMap::WorldToMap(0.0f, 0.0f, origin, size), ImVec2(900.0f, 778.5f))) {
        return false;
    }

    if (!AssertVec2Near(QuickMap::WorldToMap(3000.0f, -3000.0f, origin, size), ImVec2(1700.0f, 1507.0f))) {
        return false;
    }

    return true;
}

bool TestWorldToMapClampsOutsideBounds() {
    const ImVec2 origin(10.0f, 20.0f);
    const ImVec2 size(200.0f, 300.0f);
    return AssertVec2Near(QuickMap::WorldToMap(9999.0f, 9999.0f, origin, size), ImVec2(210.0f, 20.0f));
}

bool TestToggleModeStateTransitionsWithoutD3D() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_hotkey = 'Q';
    settings.GetMutable().quickmap_mode = "toggle";

    QuickMap map;
    bool hotkeyDown = false;
    bool inputGated = false;
    map.Initialize(&settings, nullptr);
    map.SetTestingHooks({
        [&](int key) { return key == 'Q' && hotkeyDown; },
        [&]() { return inputGated; },
    });

    map.Update();
    ASSERT_TRUE(!map.IsVisible());
    ASSERT_EQ(map.GetMode(), QuickMapMode::Toggle);
    ASSERT_EQ(map.GetHotkey(), 'Q');

    hotkeyDown = true;
    map.Update();
    ASSERT_TRUE(map.IsVisible());

    map.Update();
    ASSERT_TRUE(map.IsVisible());

    hotkeyDown = false;
    map.Update();
    ASSERT_TRUE(map.IsVisible());

    inputGated = true;
    hotkeyDown = true;
    map.Update();
    ASSERT_TRUE(map.IsVisible());

    hotkeyDown = false;
    map.Update();
    ASSERT_TRUE(map.IsVisible());

    inputGated = false;
    hotkeyDown = true;
    map.Update();
    ASSERT_TRUE(!map.IsVisible());
    return true;
}

bool TestHoldModeStateTransitionsWithoutD3D() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_hotkey = 'H';
    settings.GetMutable().quickmap_mode = "hold";

    QuickMap map;
    bool hotkeyDown = false;
    bool inputGated = false;
    map.Initialize(&settings, nullptr);
    map.SetTestingHooks({
        [&](int key) { return key == 'H' && hotkeyDown; },
        [&]() { return inputGated; },
    });

    map.Update();
    ASSERT_TRUE(!map.IsVisible());
    ASSERT_EQ(map.GetMode(), QuickMapMode::Hold);

    hotkeyDown = true;
    map.Update();
    ASSERT_TRUE(map.IsVisible());

    inputGated = true;
    map.Update();
    ASSERT_TRUE(!map.IsVisible());

    inputGated = false;
    map.Update();
    ASSERT_TRUE(map.IsVisible());

    hotkeyDown = false;
    map.Update();
    ASSERT_TRUE(!map.IsVisible());
    return true;
}

bool TestDisabledQuickMapCannotBeShown() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.Show();
    ASSERT_TRUE(!map.IsVisible());

    settings.GetMutable().quickmap_enabled = true;
    map.Show();
    ASSERT_TRUE(map.IsVisible());
    return true;
}

bool TestConfiguredMapAspectFallsBackToVariantDefaults() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    ASSERT_TRUE(NearlyEqual(map.GetActiveMapAspectRatio(), QuickMap::kClassicMapImageWidth / QuickMap::kClassicMapImageHeight));

    settings.GetMutable().quickmap_map_variant = "satellite";
    ASSERT_TRUE(NearlyEqual(map.GetActiveMapAspectRatio(), QuickMap::kSatelliteMapImageWidth / QuickMap::kSatelliteMapImageHeight));
    return true;
}

bool TestDefaultZoomLevelIsFar() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Far);
    ASSERT_EQ(std::string(map.GetZoomLevelLabel()), std::string("Far"));
    return true;
}

bool TestOnMouseWheelStepsFromFarToMedium() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Medium);
    return true;
}

bool TestOnMouseWheelAtFarStaysClamped() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.OnMouseWheel(-1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Far);
    return true;
}

bool TestOnMouseWheelStepsThroughAllLevels() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Medium);
    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Close);
    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::VeryClose);
    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Ultra);
    map.OnMouseWheel(-1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::VeryClose);
    return true;
}

bool TestOnMouseWheelAtUltraStaysClamped() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.SetZoomLevel(QuickMapZoomLevel::Ultra);
    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Ultra);
    return true;
}

bool TestWantsMouseWheelCaptureReturnsFalseWhenHidden() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.SetTestingHooks({
        [](int) { return false; },
        []() { return false; },
    });

    ASSERT_TRUE(!map.WantsMouseWheelCapture());
    return true;
}

bool TestWantsMouseWheelCaptureReturnsTrueWhenVisibleAndNotInputGated() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.SetTestingHooks({
        [](int) { return false; },
        []() { return false; },
    });

    map.Show();
    ASSERT_TRUE(map.WantsMouseWheelCapture());
    return true;
}

bool TestGetZoomLevelLabelReturnsCorrectStrings() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.SetZoomLevel(QuickMapZoomLevel::Far);
    ASSERT_EQ(std::string(map.GetZoomLevelLabel()), std::string("Far"));
    map.SetZoomLevel(QuickMapZoomLevel::Medium);
    ASSERT_EQ(std::string(map.GetZoomLevelLabel()), std::string("Medium"));
    map.SetZoomLevel(QuickMapZoomLevel::Close);
    ASSERT_EQ(std::string(map.GetZoomLevelLabel()), std::string("Close"));
    map.SetZoomLevel(QuickMapZoomLevel::VeryClose);
    ASSERT_EQ(std::string(map.GetZoomLevelLabel()), std::string("Very Close"));
    map.SetZoomLevel(QuickMapZoomLevel::Ultra);
    ASSERT_EQ(std::string(map.GetZoomLevelLabel()), std::string("Ultra"));
    return true;
}

bool TestEffectiveZoomFactorIncludesManualMultiplier() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 100.0f;
    renderData.localPlayer.worldY = 200.0f;
    map.SetRenderData(renderData);

    map.SetZoomLevel(QuickMapZoomLevel::Close);
    map.Show();
    ASSERT_TRUE(NearlyEqual(map.GetEffectiveZoomFactorForTesting(), 1.45f, 0.02f));

    return true;
}

bool TestSetZoomLevelChangesTheLevel() {
    SettingsManager settings;
    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.SetZoomLevel(QuickMapZoomLevel::VeryClose);
    ASSERT_EQ(map.GetZoomLevel(), QuickMapZoomLevel::VeryClose);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::VeryClose);
    return true;
}

bool TestManualZoomPersistsLastZoomLevelWithoutChangingAutoZoomSetting() {
    SettingsManager settings;
    settings.GetMutable().quickmap_zoom_level = "far";
    settings.GetMutable().quickmap_last_zoom_level = "far";

    QuickMap map;
    map.Initialize(&settings, nullptr);

    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Medium);
    ASSERT_EQ(settings.Get().quickmap_last_zoom_level, std::string("medium"));
    ASSERT_EQ(settings.Get().quickmap_zoom_level, std::string("far"));
    return true;
}

bool TestAutoZoomKeepsSavedSettingStableWhileWheelZoomDoesNotPersist() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_auto_zoom = true;
    settings.GetMutable().quickmap_follow_player = false;
    settings.GetMutable().quickmap_zoom_level = "close";
    settings.GetMutable().quickmap_last_zoom_level = "medium";

    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.Show();

    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Close);

    map.OnMouseWheel(+1);
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::VeryClose);
    ASSERT_EQ(settings.Get().quickmap_zoom_level, std::string("close"));
    ASSERT_EQ(settings.Get().quickmap_last_zoom_level, std::string("medium"));

    map.Hide();
    map.Show();
    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Close);
    return true;
}

bool TestAutoZoomUsesConfiguredStageWhenOpeningWithFollowPlayerEnabled() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_auto_zoom = true;
    settings.GetMutable().quickmap_follow_player = true;
    settings.GetMutable().quickmap_focus_marker = true;
    settings.GetMutable().quickmap_zoom_level = "close";
    settings.GetMutable().quickmap_last_zoom_level = "far";

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 120.0f;
    renderData.localPlayer.worldY = -80.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();

    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::Close);
    ASSERT_TRUE(NearlyEqual(map.GetCurrentZoomFactorForTesting(), 1.35f * 1.45f, 0.02f));
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(120.0f, -80.0f)));
    return true;
}

bool TestShowResumesLastManualZoomWhenAutoZoomIsDisabled() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_auto_zoom = false;
    settings.GetMutable().quickmap_zoom_level = "far";
    settings.GetMutable().quickmap_last_zoom_level = "very_close";

    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.Show();

    ASSERT_EQ(map.GetZoomLevelForTesting(), QuickMapZoomLevel::VeryClose);
    return true;
}

bool TestZoomStageMatchesDisplayAspectRatio() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = false;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.Show();
    map.SetDisplaySizeForTesting(ImVec2(1600.0f, 900.0f));
    map.SetZoomLevel(QuickMapZoomLevel::Medium);
    map.Update();

    const float viewSpanX = map.GetCurrentViewSpanXForTesting();
    const float viewSpanY = map.GetCurrentViewSpanYForTesting();
    ASSERT_TRUE(NearlyEqual(viewSpanX / viewSpanY, 1600.0f / 900.0f, 0.02f));
    ASSERT_TRUE(NearlyEqual(viewSpanX, 6000.0f / 1.2f, 0.02f));
    return true;
}

bool TestZoomStageIsResolutionIndependentForMatchingAspectRatios() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = false;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.Show();
    map.SetZoomLevel(QuickMapZoomLevel::Close);

    map.SetDisplaySizeForTesting(ImVec2(1280.0f, 720.0f));
    map.Update();
    const float spanX720 = map.GetCurrentViewSpanXForTesting();
    const float spanY720 = map.GetCurrentViewSpanYForTesting();

    map.SetDisplaySizeForTesting(ImVec2(2560.0f, 1440.0f));
    map.Update();
    const float spanX1440 = map.GetCurrentViewSpanXForTesting();
    const float spanY1440 = map.GetCurrentViewSpanYForTesting();

    ASSERT_TRUE(NearlyEqual(spanX720, spanX1440, 0.02f));
    ASSERT_TRUE(NearlyEqual(spanY720, spanY1440, 0.02f));
    return true;
}

bool TestFarZoomKeepsFullWorldSpanOnAtLeastOneAxis() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = false;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.Show();
    map.SetDisplaySizeForTesting(ImVec2(1920.0f, 1080.0f));
    map.SetZoomLevel(QuickMapZoomLevel::Far);
    map.Update();

    const float longestSpan = (std::max)(map.GetCurrentViewSpanXForTesting(), map.GetCurrentViewSpanYForTesting());
    ASSERT_TRUE(NearlyEqual(longestSpan, QuickMap::kWorldSpan, 0.02f));
    return true;
}

bool TestFollowPlayerCentersOnPlayerPosition() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = true;
    settings.GetMutable().quickmap_focus_marker = true;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 100.0f;
    renderData.localPlayer.worldY = 200.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();
    ASSERT_TRUE(NearlyEqual(map.GetCurrentZoomFactorForTesting(), 1.0f));
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(100.0f, 200.0f)));
    return true;
}

bool TestFollowPlayerDisabledShowsFullMap() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = false;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 320.0f;
    renderData.localPlayer.worldY = -440.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();
    ASSERT_TRUE(NearlyEqual(map.GetCurrentZoomFactorForTesting(), 1.0f));
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(320.0f, -440.0f)));
    return true;
}

bool TestFocusMarkerCentersBetweenPlayerAndCheckpoint() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = true;
    settings.GetMutable().quickmap_focus_marker = true;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 100.0f;
    renderData.localPlayer.worldY = 200.0f;
    renderData.hasCheckpoint = true;
    renderData.checkpoint.worldX = 1100.0f;
    renderData.checkpoint.worldY = 200.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();
    ASSERT_TRUE(map.GetCurrentZoomFactorForTesting() > 1.0f);
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(600.0f, 200.0f)));
    return true;
}

bool TestFocusMarkerDisabledDoesNotAdjustForMarker() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = true;
    settings.GetMutable().quickmap_focus_marker = false;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 50.0f;
    renderData.localPlayer.worldY = 75.0f;
    renderData.hasCheckpoint = true;
    renderData.checkpoint.worldX = 850.0f;
    renderData.checkpoint.worldY = 75.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();
    ASSERT_TRUE(NearlyEqual(map.GetCurrentZoomFactorForTesting(), 1.0f));
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(50.0f, 75.0f)));
    return true;
}

bool TestFocusMarkerWithMultipleMarkersFallsBackToFollow() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = true;
    settings.GetMutable().quickmap_focus_marker = true;
    settings.GetMutable().quickmap_auto_zoom = false;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = 320.0f;
    renderData.localPlayer.worldY = -440.0f;
    renderData.hasCheckpoint = true;
    renderData.checkpoint.worldX = 900.0f;
    renderData.checkpoint.worldY = 100.0f;
    renderData.hasRaceCheckpoint = true;
    renderData.raceCheckpoint.worldX = -1200.0f;
    renderData.raceCheckpoint.worldY = 650.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();
    ASSERT_TRUE(NearlyEqual(map.GetCurrentZoomFactorForTesting(), 1.0f));
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(320.0f, -440.0f)));
    return true;
}

bool TestAutoZoomWithoutFollowPlayer() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_follow_player = false;
    settings.GetMutable().quickmap_focus_marker = true;
    settings.GetMutable().quickmap_auto_zoom = true;

    QuickMap map;
    map.Initialize(&settings, nullptr);

    QuickMapRenderData renderData;
    renderData.hasLocalPlayer = true;
    renderData.localPlayer.worldX = -250.0f;
    renderData.localPlayer.worldY = 90.0f;
    renderData.hasCheckpoint = true;
    renderData.checkpoint.worldX = 650.0f;
    renderData.checkpoint.worldY = 90.0f;
    map.SetRenderData(renderData);

    map.Show();
    map.Update();
    ASSERT_TRUE(NearlyEqual(map.GetCurrentZoomFactorForTesting(), 1.35f, 0.02f));
    ASSERT_TRUE(AssertVec2Near(map.GetCurrentFocusWorldForTesting(), ImVec2(-250.0f, 90.0f)));
    return true;
}

bool TestGangZoneFlashDisabledAlwaysReturnsColor() {
    const ImU32 color = IM_COL32(255, 0, 0, 200);
    const ImU32 alt = IM_COL32(0, 0, 255, 200);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 0.0, false) == color);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 0.75, false) == color);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 12345.49, false) == color);
    return true;
}

bool TestGangZoneFlashWithoutAltColorReturnsColor() {
    const ImU32 color = IM_COL32(255, 0, 0, 200);
    // alpha == 0 means: not flashing (matches CGangZonePool defaults)
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, 0, 0.75, true) == color);
    const ImU32 altZeroAlpha = IM_COL32(0, 0, 255, 0);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, altZeroAlpha, 0.75, true) == color);
    return true;
}

bool TestGangZoneFlashAlternatesBetweenColors() {
    const ImU32 color = IM_COL32(255, 0, 0, 200);
    const ImU32 alt = IM_COL32(0, 0, 255, 200);
    // First half-second of every second: primary color.
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 0.0, true) == color);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 0.25, true) == color);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 12.49, true) == color);
    // Second half-second: alt color.
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 0.5, true) == alt);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 0.75, true) == alt);
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, alt, 12.99, true) == alt);
    return true;
}

bool TestGangZoneFlashIdenticalColorsReturnsColor() {
    const ImU32 color = IM_COL32(128, 64, 32, 200);
    // Same color (incl. alpha) means "no flash" even if both are non-zero.
    ASSERT_TRUE(QuickMap::ResolveGangZoneFlashColor(color, color, 0.75, true) == color);
    return true;
}

bool TestGangZoneOutlineColorBoostsDimZone() {
    // Dim red (max channel 0.2) gets boosted to 0.85 brightness.
    const ImU32 dimRed = IM_COL32(51, 0, 0, 128);
    const ImU32 outline = QuickMap::MakeGangZoneOutlineColor(dimRed);
    const ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(outline);
    ASSERT_TRUE(rgba.x >= 0.84f && rgba.x <= 0.86f);
    ASSERT_TRUE(rgba.y <= 0.01f);
    ASSERT_TRUE(rgba.z <= 0.01f);
    ASSERT_TRUE(rgba.w >= 0.94f);
    return true;
}

bool TestGangZoneOutlineColorPreservesBrightZone() {
    // Already-bright color is not over-saturated, only the alpha is forced opaque.
    const ImU32 brightGreen = IM_COL32(0, 230, 0, 80);
    const ImU32 outline = QuickMap::MakeGangZoneOutlineColor(brightGreen);
    const ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(outline);
    ASSERT_TRUE(rgba.x <= 0.01f);
    ASSERT_TRUE(rgba.y >= 0.89f);
    ASSERT_TRUE(rgba.z <= 0.01f);
    ASSERT_TRUE(rgba.w >= 0.94f);
    return true;
}

bool TestGangZoneOutlineColorBlackFillFallsBackToWhite() {
    const ImU32 outline = QuickMap::MakeGangZoneOutlineColor(IM_COL32(0, 0, 0, 0));
    const ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(outline);
    ASSERT_TRUE(rgba.x >= 0.99f);
    ASSERT_TRUE(rgba.y >= 0.99f);
    ASSERT_TRUE(rgba.z >= 0.99f);
    ASSERT_TRUE(rgba.w >= 0.89f);
    return true;
}

bool TestGangZoneOutlineThicknessSettingClampsAndPersists() {
    SettingsManager settings;
    settings.GetMutable().quickmap_gangzone_outline_thickness = 4.0f;
    settings.GetMutable().quickmap_gangzone_flash_enabled = false;

    const std::string json = settings.GetSettingsJson();
    ASSERT_TRUE(json.find("\"quickmap_gangzone_outline_thickness\":") != std::string::npos);
    ASSERT_TRUE(json.find("\"quickmap_gangzone_flash_enabled\": false") != std::string::npos);

    SettingsManager reloaded;
    ASSERT_TRUE(reloaded.LoadFromJson("{" + json + "}"));
    ASSERT_TRUE(NearlyEqual(reloaded.Get().quickmap_gangzone_outline_thickness, 4.0f));
    ASSERT_TRUE(reloaded.Get().quickmap_gangzone_flash_enabled == false);

    SettingsManager outOfRange;
    ASSERT_TRUE(outOfRange.LoadFromJson(
        "{\"settings\":{\"quickmap_gangzone_outline_thickness\": 99.0, \"quickmap_gangzone_flash_enabled\": true}}"));
    ASSERT_TRUE(NearlyEqual(outOfRange.Get().quickmap_gangzone_outline_thickness, 6.0f));
    ASSERT_TRUE(outOfRange.Get().quickmap_gangzone_flash_enabled == true);

    SettingsManager belowRange;
    ASSERT_TRUE(belowRange.LoadFromJson(
        "{\"settings\":{\"quickmap_gangzone_outline_thickness\": 0.1}}"));
    ASSERT_TRUE(NearlyEqual(belowRange.Get().quickmap_gangzone_outline_thickness, 1.0f));
    return true;
}

} // namespace

int RunQuickMapTests() {
    g_state = {};

    RUN_TEST(TestRadarSpriteResourceMapping);
    RUN_TEST(TestRadarSpriteTextureNameLookup);
    RUN_TEST(TestRadarSpriteActivePackResolution);
    RUN_TEST(TestProjectMarkerPreservesRadarSpriteMetadata);
    RUN_TEST(TestWorldToMapReferencePoints);
    RUN_TEST(TestWorldToMapClampsOutsideBounds);
    RUN_TEST(TestToggleModeStateTransitionsWithoutD3D);
    RUN_TEST(TestHoldModeStateTransitionsWithoutD3D);
    RUN_TEST(TestDisabledQuickMapCannotBeShown);
    RUN_TEST(TestConfiguredMapAspectFallsBackToVariantDefaults);
    RUN_TEST(TestDefaultZoomLevelIsFar);
    RUN_TEST(TestOnMouseWheelStepsFromFarToMedium);
    RUN_TEST(TestOnMouseWheelAtFarStaysClamped);
    RUN_TEST(TestOnMouseWheelStepsThroughAllLevels);
    RUN_TEST(TestOnMouseWheelAtUltraStaysClamped);
    RUN_TEST(TestWantsMouseWheelCaptureReturnsFalseWhenHidden);
    RUN_TEST(TestWantsMouseWheelCaptureReturnsTrueWhenVisibleAndNotInputGated);
    RUN_TEST(TestGetZoomLevelLabelReturnsCorrectStrings);
    RUN_TEST(TestEffectiveZoomFactorIncludesManualMultiplier);
    RUN_TEST(TestSetZoomLevelChangesTheLevel);
    RUN_TEST(TestManualZoomPersistsLastZoomLevelWithoutChangingAutoZoomSetting);
    RUN_TEST(TestAutoZoomKeepsSavedSettingStableWhileWheelZoomDoesNotPersist);
    RUN_TEST(TestAutoZoomUsesConfiguredStageWhenOpeningWithFollowPlayerEnabled);
    RUN_TEST(TestShowResumesLastManualZoomWhenAutoZoomIsDisabled);
    RUN_TEST(TestZoomStageMatchesDisplayAspectRatio);
    RUN_TEST(TestZoomStageIsResolutionIndependentForMatchingAspectRatios);
    RUN_TEST(TestFarZoomKeepsFullWorldSpanOnAtLeastOneAxis);
    RUN_TEST(TestFollowPlayerCentersOnPlayerPosition);
    RUN_TEST(TestFollowPlayerDisabledShowsFullMap);
    RUN_TEST(TestFocusMarkerCentersBetweenPlayerAndCheckpoint);
    RUN_TEST(TestFocusMarkerDisabledDoesNotAdjustForMarker);
    RUN_TEST(TestFocusMarkerWithMultipleMarkersFallsBackToFollow);
    RUN_TEST(TestAutoZoomWithoutFollowPlayer);
    RUN_TEST(TestGangZoneFlashDisabledAlwaysReturnsColor);
    RUN_TEST(TestGangZoneFlashWithoutAltColorReturnsColor);
    RUN_TEST(TestGangZoneFlashAlternatesBetweenColors);
    RUN_TEST(TestGangZoneFlashIdenticalColorsReturnsColor);
    RUN_TEST(TestGangZoneOutlineColorBoostsDimZone);
    RUN_TEST(TestGangZoneOutlineColorPreservesBrightZone);
    RUN_TEST(TestGangZoneOutlineColorBlackFillFallsBackToWhite);
    RUN_TEST(TestGangZoneOutlineThicknessSettingClampsAndPersists);

    std::printf("QuickMapTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
