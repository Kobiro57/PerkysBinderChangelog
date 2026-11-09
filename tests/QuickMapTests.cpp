#define KEYBINDER_TESTING

#include "../include/QuickMap.h"
#include "../include/ClaimSpots.h"
#include "../include/TextureManager.h"
#include "../include/SettingsManager.h"
#include "../include/RadarSpriteResources.h"
#include "../include/ResourceIds.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CRadar.h"

#include <cmath>
#include <cstdint>
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

bool TestClaimSpotsMatchAllTwentyEightUserSaves() {
    const float expected[][3] = {
        {1583.9396f,33.5867f,26.9868f}, {1927.8734f,168.8767f,40.1172f},
        {2160.4473f,-107.1974f,2.7599f}, {2239.8594f,-86.8115f,27.8548f},
        {2311.3462f,-17.4462f,32.5313f}, {2330.9189f,80.9126f,32.0073f},
        {2547.1592f,93.7197f,29.8577f}, {1297.6686f,182.1735f,20.4670f},
        {1297.8884f,308.8936f,27.5555f}, {1287.1136f,270.3256f,23.7625f},
        {1247.1160f,367.3232f,22.7848f}, {1488.7997f,361.4571f,21.9852f},
        {1371.5627f,471.9797f,23.1484f}, {760.6830f,381.3502f,26.0898f},
        {752.6452f,259.9708f,29.7391f}, {863.5613f,-23.6717f,66.2610f},
        {645.8249f,-519.8103f,21.1673f}, {743.5359f,-588.8473f,20.5990f},
        {803.7162f,-564.1726f,21.3363f}, {1072.5286f,-291.1361f,76.8663f},
        {252.7985f,-292.9166f,1.5836f}, {193.9769f,-229.6732f,7.0782f},
        {206.3966f,-179.7932f,5.0786f}, {172.4167f,-161.3612f,6.7786f},
        {183.5547f,-108.0306f,2.0234f}, {355.0618f,-76.3290f,1.3705f},
        {-437.4864f,-59.2712f,62.0269f}, {-549.7010f,-202.3125f,80.4654f},
    };
    ASSERT_EQ(kRcbClaimSpots.size(), static_cast<std::size_t>(28));
    for (std::size_t i = 0; i < kRcbClaimSpots.size(); ++i) {
        ASSERT_EQ(kRcbClaimSpots[i].x, expected[i][0]);
        ASSERT_EQ(kRcbClaimSpots[i].y, expected[i][1]);
        ASSERT_EQ(kRcbClaimSpots[i].z, expected[i][2]);
        ASSERT_TRUE(kRcbClaimSpots[i].name != nullptr && kRcbClaimSpots[i].name[0] != '\0');
    }
    return true;
}

bool TestClaimsRequireExplicitToggleAndPreserveServerMarkers() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    QuickMap map;
    map.Initialize(&settings, nullptr);
    QuickMapRenderData data;
    data.hasCheckpoint = true;
    data.checkpoint.worldX = 1000.0f;
    data.checkpoint.worldY = 2000.0f;
    data.mapIcons.push_back({});
    data.mapIcons[0].worldX = -200.0f;
    data.mapIcons[0].radarSprite = RADAR_SPRITE_POLICE;
    map.SetRenderData(data);
    ASSERT_TRUE(!map.AreClaimsVisible());
    ASSERT_TRUE(!map.ToggleClaims());
    ASSERT_TRUE(map.ProjectClaimsForTesting(ImVec2(), ImVec2(600,600)).empty());
    map.SetClaimsAllowed(true);
    ASSERT_TRUE(!map.AreClaimsVisible());
    ASSERT_TRUE(map.ToggleClaims());
    ASSERT_TRUE(map.AreClaimsVisible());
    ASSERT_TRUE(!map.IsVisible()); // /claims does not itself open the map.
    ASSERT_EQ(map.GetRenderData().mapIcons.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(map.GetRenderData().mapIcons[0].radarSprite, static_cast<unsigned char>(RADAR_SPRITE_POLICE));
    ASSERT_EQ(map.GetRenderData().mapIcons[0].worldX, -200.0f);
    ASSERT_TRUE(map.GetRenderData().hasCheckpoint);
    ASSERT_EQ(map.GetRenderData().checkpoint.worldX, 1000.0f);
    map.Show();
    map.Hide();
    ASSERT_TRUE(map.AreClaimsVisible());
    map.OnDeviceLost();
    map.OnDeviceReset(nullptr);
    ASSERT_TRUE(map.AreClaimsVisible());
    ASSERT_TRUE(map.ToggleClaims());
    ASSERT_TRUE(!map.AreClaimsVisible());
    ASSERT_EQ(map.GetRenderData().mapIcons.size(), static_cast<std::size_t>(1));
    ASSERT_TRUE(map.ToggleClaims());
    map.SetClaimsAllowed(false);
    ASSERT_TRUE(!map.AreClaimsVisible());
    ASSERT_TRUE(!map.ToggleClaims());
    map.SetClaimsAllowed(true);
    ASSERT_TRUE(!map.AreClaimsVisible()); // Regaining membership never auto-enables.
    ASSERT_TRUE(map.ToggleClaims());
    map.Shutdown();
    ASSERT_TRUE(!map.AreClaimsVisible());
    ASSERT_TRUE(!map.ToggleClaims());
    return true;
}

bool TestClaimsUseTheCurrentZoomAndDoNotClampOffscreenFlags() {
    SettingsManager settings;
    settings.GetMutable().quickmap_enabled = true;
    settings.GetMutable().quickmap_auto_zoom = false;
    settings.GetMutable().quickmap_follow_player = true;
    settings.GetMutable().quickmap_focus_marker = false;
    QuickMap map;
    map.Initialize(&settings, nullptr);
    map.SetDisplaySizeForTesting(ImVec2(600,600));
    map.Show();
    map.SetZoomLevel(QuickMapZoomLevel::Far);
    map.SetClaimsAllowed(true);
    ASSERT_TRUE(map.ToggleClaims());
    auto claims = map.ProjectClaimsForTesting(ImVec2(10,20), ImVec2(600,600));
    ASSERT_EQ(claims.size(), static_cast<std::size_t>(28));
    for (const auto& claim : claims) {
        ASSERT_TRUE(claim.visible);
        ASSERT_EQ(claim.radarSprite, static_cast<unsigned char>(RADAR_SPRITE_ENEMYATTACK));
        ASSERT_TRUE(!claim.emphasize);
    }
    ASSERT_TRUE(AssertVec2Near(claims[0].position, ImVec2(468.39396f,316.64133f)));
    QuickMapRenderData data;
    data.hasLocalPlayer = true;
    data.localPlayer.worldX = 2500.0f;
    data.localPlayer.worldY = -2500.0f;
    map.SetRenderData(data);
    map.SetZoomLevel(QuickMapZoomLevel::Ultra);
    claims = map.ProjectClaimsForTesting(ImVec2(10,20), ImVec2(600,600));
    ASSERT_TRUE(!claims[0].visible); // Montgomery is outside this zoomed view.
    ASSERT_TRUE(claims[17].visible); // Dillimore remains at its actual position.
    ASSERT_TRUE(!claims[27].visible); // Left-of-view flags do not pile at the edge.
    ASSERT_TRUE(claims[17].position.x >= 10.0f && claims[17].position.x <= 610.0f);
    ASSERT_TRUE(claims[17].position.y >= 20.0f && claims[17].position.y <= 620.0f);
    data.localPlayer.worldX = -500.0f;
    data.localPlayer.worldY = -200.0f;
    map.SetRenderData(data);
    claims = map.ProjectClaimsForTesting(ImVec2(10,20), ImVec2(600,600));
    ASSERT_TRUE(claims[27].visible);
    ASSERT_TRUE(!claims[0].visible);
    map.SetClaimsAllowed(false);
    ASSERT_TRUE(map.ProjectClaimsForTesting(ImVec2(10,20), ImVec2(600,600)).empty());
    return true;
}

struct ClaimDrawContext {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    ClaimDrawContext() {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1000,1000);
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(static_cast<ImTextureID>(1));
    }
    ~ClaimDrawContext() {
        ImGui::DestroyContext(context);
        ImGui::SetCurrentContext(previous);
    }
};

bool TestClaimRenderingUsesBothExistingFlagPacksAndDrawsPlayerLast() {
    for (const auto pack : {RadarSpritePack::Radar1, RadarSpritePack::Radar2}) {
        ClaimDrawContext context;
        SettingsManager settings;
        auto& configured = settings.GetMutable();
        configured.quickmap_enabled = true;
        configured.quickmap_follow_player = false;
        configured.quickmap_focus_marker = false;
        configured.quickmap_auto_zoom = false;
        configured.quickmap_radar_pack = pack == RadarSpritePack::Radar1 ? "radar1" : "radar2";
        QuickMap map;
        map.Initialize(&settings, nullptr);
        std::vector<WORD> requested;
        const WORD flagId = pack == RadarSpritePack::Radar1 ? IDR_RADAR_ENEMYATTACK : IDR_RADAR2_ENEMYATTACK;
        TextureInfo flag;
        flag.texture = reinterpret_cast<IDirect3DTexture9*>(static_cast<std::uintptr_t>(0x10000 + flagId));
        flag.width = flag.height = 32;
        QuickMap::TestingHooks hooks;
        hooks.getRadarSpriteTextureByResource = [&](WORD resource) -> TextureInfo* {
            requested.push_back(resource);
            return resource == flagId ? &flag : nullptr;
        };
        map.SetTestingHooks(hooks);
        QuickMapRenderData data;
        data.hasLocalPlayer = true;
        data.localPlayer.type = QuickMapMarkerType::LocalPlayer;
        data.localPlayer.worldX = -2000.0f;
        data.localPlayer.worldY = 2200.0f;
        QuickMapMarker serverIcon;
        serverIcon.worldX = -2300.0f;
        serverIcon.worldY = 2000.0f;
        serverIcon.radarSprite = RADAR_SPRITE_POLICE;
        data.mapIcons.push_back(serverIcon);
        map.SetRenderData(data);
        map.Show();
        map.SetZoomLevel(QuickMapZoomLevel::Far);
        map.SetClaimsAllowed(true);
        ASSERT_TRUE(map.ToggleClaims());
        ImGui::NewFrame();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        map.Render(drawList, ImVec2(100,100), ImVec2(600,600));
        unsigned int flagIndices = 0;
        for (const ImDrawCmd& command : drawList->CmdBuffer) {
            if (command.GetTexID() == reinterpret_cast<ImTextureID>(flag.texture)) {
                flagIndices += command.ElemCount;
                ASSERT_EQ(command.ClipRect.x, 100.0f);
                ASSERT_EQ(command.ClipRect.y, 100.0f);
                ASSERT_EQ(command.ClipRect.z, 700.0f);
                ASSERT_EQ(command.ClipRect.w, 700.0f);
            }
        }
        ASSERT_EQ(flagIndices, static_cast<unsigned int>(28 * 6));
        ASSERT_EQ(requested.size(), static_cast<std::size_t>(30));
        ASSERT_EQ(requested.front(), static_cast<WORD>(ResolveRadarSpriteResourceId(*FindRadarSpriteResource(RADAR_SPRITE_POLICE), pack)));
        ASSERT_EQ(requested.back(), static_cast<WORD>(IDR_RADAR_CENTRE));
        for (std::size_t i = 1; i <= 28; ++i) ASSERT_EQ(requested[i], flagId);
        ImGui::Render();
        ASSERT_TRUE(map.ToggleClaims());
        requested.clear();
        ImGui::NewFrame();
        drawList = ImGui::GetForegroundDrawList();
        map.Render(drawList, ImVec2(100,100), ImVec2(600,600));
        for (const ImDrawCmd& command : drawList->CmdBuffer) {
            ASSERT_TRUE(command.GetTexID() != reinterpret_cast<ImTextureID>(flag.texture));
        }
        ASSERT_EQ(requested.size(), static_cast<std::size_t>(2));
        ASSERT_EQ(map.GetRenderData().mapIcons.size(), static_cast<std::size_t>(1));
        ImGui::Render();
    }
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
    RUN_TEST(TestClaimSpotsMatchAllTwentyEightUserSaves);
    RUN_TEST(TestClaimsRequireExplicitToggleAndPreserveServerMarkers);
    RUN_TEST(TestClaimsUseTheCurrentZoomAndDoNotClampOffscreenFlags);
    RUN_TEST(TestClaimRenderingUsesBothExistingFlagPacksAndDrawsPlayerLast);

    std::printf("QuickMapTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
