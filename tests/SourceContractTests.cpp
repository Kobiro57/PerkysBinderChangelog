/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Source contract tests for theme architecture and renamed UI component surface
*/
#ifndef KEYBINDER_TESTING
#define KEYBINDER_TESTING
#endif

#include "KeybindTests.h"

#include <fstream>
#include <string>
#include <vector>

namespace {

std::string ReadTextFile(const char* path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool FileExists(const char* path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    return file.good();
}

int CountOccurrences(const std::string& text, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }

    int count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

bool ContainsAny(const std::string& text, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool TestRenamedUIComponentsSurface() {
    const std::string oldComponentName = "PremiumUIComponents";
    const std::string oldHeaderPath = std::string("include/") + oldComponentName + ".h";
    const std::string oldSourcePath = std::string("src/") + oldComponentName + ".cpp";

    ASSERT_TRUE(FileExists("include/UIComponents.h"));
    ASSERT_TRUE(FileExists("src/UIComponents.cpp"));
    ASSERT_TRUE(!FileExists(oldHeaderPath.c_str()));
    ASSERT_TRUE(!FileExists(oldSourcePath.c_str()));

    const std::string runTests = ReadTextFile("tools/run_tests.bat");
    const std::string runner = ReadTextFile("tests/KeybindTests.cpp");
    ASSERT_TRUE(runTests.find("UIComponentsTests.cpp") != std::string::npos);
    ASSERT_TRUE(runner.find("RunUIComponentsTests") != std::string::npos);
    return true;
}

bool TestNoLegacyUiNamingInFirstPartyContracts() {
    const std::vector<const char*> paths = {
        "include/UIComponents.h",
        "src/UIComponents.cpp",
        "src/MainWindowUI.cpp",
        "include/MainWindowUI.h",
        "tests/KeybindTests.cpp",
        "tests/UIComponentsTests.cpp",
        "tools/run_tests.bat",
        "Keybinder.vcxproj"
    };
    const std::vector<std::string> legacyTerms = {
        "PremiumUIComponents",
        "PremiumUI"
    };

    for (const char* path : paths) {
        const std::string content = ReadTextFile(path);
        ASSERT_TRUE(!content.empty());
        ASSERT_TRUE(!ContainsAny(content, legacyTerms));
    }
    return true;
}

bool TestCanonicalThemeApplyPathSourceContract() {
    const std::string themeHeader = ReadTextFile("include/ThemeManager.h");
    const std::string themeSource = ReadTextFile("src/ThemeManager.cpp");
    const std::string mainSource = ReadTextFile("src/Main.cpp");
    const std::string mainWindowSource = ReadTextFile("src/MainWindowUI.cpp");
    const std::string overlaySource = ReadTextFile("src/ImGuiOverlay.cpp");

    ASSERT_TRUE(themeHeader.find("ApplyConfiguredTheme") != std::string::npos);
    ASSERT_TRUE(themeSource.find("settings.theme_name") != std::string::npos);
    ASSERT_TRUE(themeSource.find("SetTheme(") != std::string::npos);
    ASSERT_TRUE(themeSource.find("ApplyCustomAccent(") != std::string::npos);
    ASSERT_TRUE(themeSource.find("ApplyImGuiStyle(scale)") != std::string::npos);

    ASSERT_TRUE(mainWindowSource.find("ApplyConfiguredTheme(settingsManager_, GetLayoutScale())") != std::string::npos);
    ASSERT_TRUE(overlaySource.find("ApplyConfiguredTheme(settings_manager_, current_scale_)") != std::string::npos);
    ASSERT_TRUE(mainSource.find("ApplyConfiguredAppearanceStyle") == std::string::npos);
    ASSERT_TRUE(mainSource.find("ApplyCustomAccent(") == std::string::npos);
    ASSERT_TRUE(mainSource.find("ApplyImGuiStyle(") == std::string::npos);

    const std::string callers = mainSource + mainWindowSource + overlaySource;
    ASSERT_EQ(CountOccurrences(callers, "ApplyCustomAccent("), 0);
    ASSERT_EQ(CountOccurrences(callers, "ApplyImGuiStyle("), 0);
    return true;
}

bool TestAppearanceSelectorSourceContract() {
    const std::string header = ReadTextFile("include/MainWindowUI.h");
    const std::string source = ReadTextFile("src/MainWindowUI.cpp");

    ASSERT_TRUE(header.find("Appearance = 1") != std::string::npos);
    ASSERT_TRUE(header.find("RenderSettingsAppearanceTab") != std::string::npos);
    ASSERT_TRUE(source.find("SubTabItem(\"Appearance\"") != std::string::npos);
    ASSERT_TRUE(source.find("RenderSettingsAppearanceTab()") != std::string::npos);
    ASSERT_TRUE(source.find("UIComponents::Combo(\"Theme\"") != std::string::npos);
    ASSERT_TRUE(source.find("\"Dark\", \"Nova\", \"Ember\", \"Ocean\", \"Forest\"") != std::string::npos);
    ASSERT_TRUE(source.find("settings.theme_name = kThemeNames[selectedThemeIndex]") != std::string::npos);
    ASSERT_TRUE(source.find("ApplyCurrentTheme()") != std::string::npos);
    return true;
}

bool TestNoRawNovaPaletteOutsideThemeManagerContracts() {
    const std::vector<const char*> paths = {
        "src/Main.cpp",
        "src/MainWindowUI.cpp",
        "src/ImGuiOverlay.cpp",
        "src/UIComponents.cpp",
        "src/KillTrackerUI.cpp",
        "src/HitmarkerUI.cpp",
        "src/HitmarkerRenderer.cpp",
        "src/SpeedometerUI.cpp",
        "src/OnlineOverlayRenderer.cpp"
    };
    const std::vector<std::string> forbidden = {
        std::string("0x7C") + "3AED",
        std::string("0x090") + "A12",
        std::string("0x161") + "724",
        std::string("0x1E1") + "E2E",
        std::string("0x262") + "63A",
        std::string("124, 58") + ", 237",
        std::string("9, 10") + ", 18",
        std::string("22, 23") + ", 36",
        std::string("30, 30") + ", 46",
        std::string("38, 38") + ", 58"
    };

    for (const char* path : paths) {
        const std::string content = ReadTextFile(path);
        ASSERT_TRUE(!content.empty());
        ASSERT_TRUE(!ContainsAny(content, forbidden));
    }
    return true;
}

bool TestSettingsSubtabEnumContract() {
    const std::string header = ReadTextFile("include/MainWindowUI.h");
    ASSERT_TRUE(!header.empty());
    // Settings subtabs: General=0, Appearance=1, QuickMap=2, RssTracker=3
    ASSERT_TRUE(header.find("RssTracker = 3") != std::string::npos);
    // Tacho settings are rendered inside General rather than as a separate subtab.
    ASSERT_TRUE(header.find("SettingsSubtab") != std::string::npos);
    return true;
}

bool TestSubTabMigrationContract() {
    const std::string mainWindow = ReadTextFile("src/MainWindowUI.cpp");
    const std::string uiComponents = ReadTextFile("src/UIComponents.cpp");
    ASSERT_TRUE(!mainWindow.empty());
    ASSERT_TRUE(!uiComponents.empty());
    // After migration, screen-level tabs use SubTabItem, not TabItem.
    ASSERT_TRUE(mainWindow.find("BeginSubTabs(") != std::string::npos);
    ASSERT_TRUE(mainWindow.find("SubTabItem(") != std::string::npos);
    ASSERT_TRUE(mainWindow.find("EndSubTabs(") != std::string::npos);
    // The subtab primitive is a real glass navigation strip, not a per-button recolor.
    ASSERT_TRUE(uiComponents.find("glass navigation strip") != std::string::npos);
    ASSERT_TRUE(uiComponents.find("GetSubTabBarPaddingY") != std::string::npos);
    ASSERT_TRUE(uiComponents.find("AddRectFilledMultiColor") != std::string::npos);
    return true;
}

bool TestHudModsSubtabSourceContract() {
    const std::string header = ReadTextFile("include/MainWindowUI.h");
    const std::string source = ReadTextFile("src/MainWindowUI.cpp");
    const std::string hudSpriteUi = ReadTextFile("src/HudSpriteUI.cpp");
    ASSERT_TRUE(!header.empty());
    ASSERT_TRUE(!source.empty());
    ASSERT_TRUE(!hudSpriteUi.empty());

    ASSERT_TRUE(header.find("Hud = 2") != std::string::npos);
    ASSERT_TRUE(header.find("HudSpriteLibrarySubtab") != std::string::npos);
    ASSERT_TRUE(source.find("SubTabItem(\"Hud\"") != std::string::npos);
    ASSERT_TRUE(source.find("SubTabItem(\"Crosshair\"") != std::string::npos);
    ASSERT_TRUE(source.find("SubTabItem(\"Fist\"") != std::string::npos);
    ASSERT_TRUE(source.find("SubTabItem(\"Squaremap\"") == std::string::npos);
    ASSERT_TRUE(source.find("SubTabItem(\"Interface\"") == std::string::npos);
    ASSERT_TRUE(source.find("interfaceEditorUI_.Render()") != std::string::npos);
    ASSERT_TRUE(source.find("HD Blips") == std::string::npos);
    ASSERT_TRUE(source.find("QuickMap Radar-PNGs") != std::string::npos);
    ASSERT_TRUE(hudSpriteUi.find("Native GTA-Fist") != std::string::npos);
    ASSERT_TRUE(hudSpriteUi.find("fist_texture_path") != std::string::npos);
    ASSERT_TRUE(hudSpriteUi.find("HD Blip Set") == std::string::npos);
    ASSERT_TRUE(hudSpriteUi.find("Blip-Größe") == std::string::npos);
    const std::string hudSpriteManager = ReadTextFile("src/HudSpriteManager.cpp");
    const std::string radarResources = ReadTextFile("include/RadarSpriteResources.h");
    ASSERT_TRUE(!hudSpriteManager.empty());
    ASSERT_TRUE(!radarResources.empty());
    ASSERT_TRUE(radarResources.find("RADAR_SPRITE_CENTRE, IDR_RADAR_CENTRE, IDR_RADAR_CENTRE") != std::string::npos);
    ASSERT_TRUE(radarResources.find("RADAR_SPRITE_MAP_HERE, IDR_RADAR_MAP_HERE, IDR_RADAR2_MAP_HERE") != std::string::npos);
    ASSERT_TRUE(radarResources.find("radar map-here arrow\", false") != std::string::npos);
    ASSERT_TRUE(radarResources.find("resource.allowTextureNameMatch") != std::string::npos);
    ASSERT_TRUE(radarResources.find("IDR_RADAR2_NORTH") != std::string::npos);
    ASSERT_TRUE(radarResources.find("ResolveActiveRadarSpritePack") != std::string::npos);
    ASSERT_TRUE(radarResources.find("quickmap_radar_pack") != std::string::npos);
    ASSERT_TRUE(radarResources.find("ResolveRadarSpriteResourceId") != std::string::npos);
    ASSERT_TRUE(radarResources.find("FindRadarSpriteResourceByTextureName") != std::string::npos);
    ASSERT_TRUE(radarResources.find("radar_burgerShot") != std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("../include/RadarSpriteResources.h") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("kRadarSpriteResources") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("CRadar::LoadTextures") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("configuredRadarSpriteScale_") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("replacement.radarSprite == RADAR_SPRITE_CENTRE") == std::string::npos);
    const std::string embeddedTexture = ReadTextFile("src/EmbeddedRwTexture.cpp");
    ASSERT_TRUE(!embeddedTexture.empty());
    ASSERT_TRUE(embeddedTexture.find("SampleAreaFilteredRgba") != std::string::npos);
    ASSERT_TRUE(embeddedTexture.find("weightedAlpha") != std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("CTextDraw::Draw") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("CSprite2d::SetTexture") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("HookedSampTextDrawDraw") == std::string::npos);
    ASSERT_TRUE(hudSpriteManager.find("RetireRadarTexture") == std::string::npos);
    ASSERT_TRUE(radarResources.find("RADAR_SPRITE_TORENO,") == std::string::npos);
    return true;
}

bool TestDarkTokenContractInThemeManager() {
    const std::string themeSource = ReadTextFile("src/ThemeManager.cpp");
    ASSERT_TRUE(!themeSource.empty());
    // Dark accent token: #7B2CBF = (123, 44, 191)
    ASSERT_TRUE(themeSource.find("123, 44, 191") != std::string::npos);
    ASSERT_TRUE(themeSource.find("7B2CBF") != std::string::npos);
    // Nova accent token: #7C3AED = (124, 58, 237)
    ASSERT_TRUE(themeSource.find("124, 58, 237") != std::string::npos);
    ASSERT_TRUE(themeSource.find("7C3AED") != std::string::npos);
    return true;
}

bool TestDeterministicFixturePresence() {
    ASSERT_TRUE(FileExists("tools/qa/fixtures/nova-theme.json"));
    ASSERT_TRUE(FileExists("tools/qa/fixtures/dark-theme.json"));
    return true;
}

bool TestNovaFixtureContract() {
    const std::string fixture = ReadTextFile("tools/qa/fixtures/nova-theme.json");
    ASSERT_TRUE(!fixture.empty());
    ASSERT_TRUE(fixture.find("\"theme_name\": \"Nova\"") != std::string::npos);
    ASSERT_TRUE(fixture.find("\"theme_custom_accent\": \"\"") != std::string::npos);
    ASSERT_TRUE(fixture.find("\"ui_toggle_hotkey\"") != std::string::npos);
    ASSERT_TRUE(fixture.find("\"keybinds\"") != std::string::npos);
    return true;
}

bool TestDarkFixtureContract() {
    const std::string fixture = ReadTextFile("tools/qa/fixtures/dark-theme.json");
    ASSERT_TRUE(!fixture.empty());
    ASSERT_TRUE(fixture.find("\"theme_name\": \"Dark\"") != std::string::npos);
    ASSERT_TRUE(fixture.find("\"theme_custom_accent\": \"\"") != std::string::npos);
    ASSERT_TRUE(fixture.find("\"ui_toggle_hotkey\"") != std::string::npos);
    ASSERT_TRUE(fixture.find("\"keybinds\"") != std::string::npos);
    return true;
}

bool TestReusableGlowHelperSourceContract() {
    const std::string header = ReadTextFile("include/UIComponents.h");
    const std::string source = ReadTextFile("src/UIComponents.cpp");
    const std::string speedometer = ReadTextFile("src/SpeedometerUI.cpp");
    const std::string quickMap = ReadTextFile("src/QuickMap.cpp");
    ASSERT_TRUE(!header.empty());
    ASSERT_TRUE(!source.empty());
    ASSERT_TRUE(!speedometer.empty());
    ASSERT_TRUE(!quickMap.empty());

    ASSERT_TRUE(header.find("AddSoftGlowRect") != std::string::npos);
    ASSERT_TRUE(header.find("AddSoftGlowArc") != std::string::npos);
    ASSERT_TRUE(source.find("PushBoundedEffectClipRect") != std::string::npos);
    ASSERT_TRUE(source.find("GetClipRectMin") != std::string::npos);
    ASSERT_TRUE(source.find("GetClipRectMax") != std::string::npos);
    ASSERT_TRUE(source.find("PushClipRectFullScreen") == std::string::npos);
    ASSERT_TRUE(source.find("ImDrawListFlags_AntiAliasedFill") != std::string::npos);
    ASSERT_TRUE(source.find("ImDrawListFlags_AntiAliasedLines") != std::string::npos);
    ASSERT_TRUE(speedometer.find("#include \"../include/UIComponents.h\"") != std::string::npos);
    ASSERT_TRUE(speedometer.find("UIComponents::AddSoftGlowArc") != std::string::npos);
    ASSERT_TRUE(CountOccurrences(speedometer, "UIComponents::AddSoftGlowRect") >= 2);
    ASSERT_TRUE(quickMap.find("#include \"../include/UIComponents.h\"") != std::string::npos);
    ASSERT_TRUE(quickMap.find("UIComponents::AddSoftGlowRect") != std::string::npos);
    ASSERT_TRUE(quickMap.find("ImGui::GetTime()") != std::string::npos);
    ASSERT_TRUE(CountOccurrences(quickMap, "AddQuickMapRedMarkerGlow(drawList") >= 2);
    return true;
}

bool TestKeyboardLayoutAwareHotkeyCaptureContract() {
    const std::string helper = ReadTextFile("include/KeyInputUtils.h");
    const std::string editor = ReadTextFile("src/KeybindEditorUI.cpp");
    const std::string list = ReadTextFile("src/KeybindListUI.cpp");
    const std::string mainWindow = ReadTextFile("src/MainWindowUI.cpp");
    const std::string online = ReadTextFile("src/OnlineUI.cpp");
    ASSERT_TRUE(!helper.empty());
    ASSERT_TRUE(!editor.empty());
    ASSERT_TRUE(!list.empty());
    ASSERT_TRUE(!mainWindow.empty());
    ASSERT_TRUE(!online.empty());

    ASSERT_TRUE(helper.find("GetAsyncKeyState") != std::string::npos);
    ASSERT_TRUE(helper.find("MapVirtualKeyExA") != std::string::npos);
    ASSERT_TRUE(helper.find("GetKeyboardLayout(0)") != std::string::npos);
    ASSERT_TRUE(editor.find("ImGuiKeyToVK") == std::string::npos);
    ASSERT_TRUE(mainWindow.find("ImGuiKeyToVK") == std::string::npos);
    ASSERT_TRUE(online.find("ImGuiKeyToVK") == std::string::npos);
    ASSERT_TRUE(editor.find("FindPressedVirtualKey") != std::string::npos);
    ASSERT_TRUE(mainWindow.find("FindPressedVirtualKey") != std::string::npos);
    ASSERT_TRUE(online.find("FindPressedVirtualKey") != std::string::npos);
    ASSERT_TRUE(list.find("GetLocalizedVirtualKeyName") != std::string::npos);
    return true;
}

bool TestOverlayMenuPreservesGameplayKeyboardContract() {
    const std::string inputHeader = ReadTextFile("include/InputManager.h");
    const std::string inputSource = ReadTextFile("src/InputManager.cpp");
    const std::string overlaySource = ReadTextFile("src/ImGuiOverlay.cpp");

    ASSERT_TRUE(!inputHeader.empty());
    ASSERT_TRUE(!inputSource.empty());
    ASSERT_TRUE(!overlaySource.empty());

    ASSERT_TRUE(inputHeader.find("suppressOverlayKeyboardInput") != std::string::npos);
    ASSERT_TRUE(inputSource.find("if (IsImGuiCapturingKeyboard())") != std::string::npos);
    ASSERT_TRUE(overlaySource.find("IsKeyboardCaptureActive()") != std::string::npos);
    ASSERT_TRUE(overlaySource.find("return io.WantCaptureKeyboard || io.WantTextInput;") != std::string::npos);
    ASSERT_TRUE(overlaySource.find("deviceType == DI8DEVTYPE_KEYBOARD && active_instance_->IsKeyboardCaptureActive()") != std::string::npos);
    ASSERT_TRUE(overlaySource.find("IsRawInputMouseEvent") != std::string::npos);
    ASSERT_TRUE(overlaySource.find("Raw keyboard") != std::string::npos);
    return true;
}

} // namespace

int RunSourceContractTests() {
    printf("\n=== Source Contract Tests ===\n\n");
    g_results = TestResult();

    RUN_TEST(TestRenamedUIComponentsSurface);
    RUN_TEST(TestNoLegacyUiNamingInFirstPartyContracts);
    RUN_TEST(TestCanonicalThemeApplyPathSourceContract);
    RUN_TEST(TestAppearanceSelectorSourceContract);
    RUN_TEST(TestNoRawNovaPaletteOutsideThemeManagerContracts);
    RUN_TEST(TestSettingsSubtabEnumContract);
    RUN_TEST(TestSubTabMigrationContract);
    RUN_TEST(TestHudModsSubtabSourceContract);
    RUN_TEST(TestDarkTokenContractInThemeManager);
    RUN_TEST(TestDeterministicFixturePresence);
    RUN_TEST(TestNovaFixtureContract);
    RUN_TEST(TestDarkFixtureContract);
    RUN_TEST(TestReusableGlowHelperSourceContract);
    RUN_TEST(TestKeyboardLayoutAwareHotkeyCaptureContract);
    RUN_TEST(TestOverlayMenuPreservesGameplayKeyboardContract);

    printf("\n=== Results: %d passed, %d failed ===\n\n", g_results.passed, g_results.failed);
    return g_results.failed;
}
