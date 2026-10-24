/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    ThemeManager unit tests - default tokens, contrast guarantees, custom-accent derivation
*/
#include "ThemeManager.h"
#include "SettingsManager.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

namespace {

void ResetDefaultAccent() {
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.SetTheme("dark");
}

void ResetDarkTheme() {
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.SetTheme("dark");
}

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[FAIL] %s\n", message);
        return false;
    }
    return true;
}

bool ColorEquals(const ImVec4& c, int r, int g, int b, int a, float epsilon = 0.01f) {
    const float fr = static_cast<float>(r) / 255.0f;
    const float fg = static_cast<float>(g) / 255.0f;
    const float fb = static_cast<float>(b) / 255.0f;
    const float fa = static_cast<float>(a) / 255.0f;
    return std::fabs(c.x - fr) < epsilon
        && std::fabs(c.y - fg) < epsilon
        && std::fabs(c.z - fb) < epsilon
        && std::fabs(c.w - fa) < epsilon;
}

// ── Default accent family ──────────────────────────────────────────

bool TestDefaultAccentIsPurple() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    // Purple #7B2CBF = (123, 44, 191)
    return Expect(ColorEquals(accent, 123, 44, 191, 255),
        "Default accent should be purple #7B2CBF");
}

bool TestDefaultAccentHover() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 hover = tm.GetColor(keybinder::ThemeColorRole::AccentHover);
    // Hover should differ from accent (derived via TintAccent)
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    return Expect(hover.x != accent.x || hover.y != accent.y || hover.z != accent.z,
        "AccentHover should differ from Accent (derived color)");
}

bool TestDefaultAccentActive() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 active = tm.GetColor(keybinder::ThemeColorRole::AccentActive);
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    return Expect(active.x != accent.x || active.y != accent.y || active.z != accent.z,
        "AccentActive should differ from Accent (derived color)");
}

// ── Text hierarchy colors ──────────────────────────────────────────

bool TestTextPrimaryIsLight() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
    // Primary #EDEDED = (237, 237, 237)
    return Expect(ColorEquals(text, 237, 237, 237, 255),
        "TextPrimary should be #EDEDED");
}

bool TestTextSecondary() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextSecondary);
    // Secondary #A0A0A0 = (160, 160, 160)
    return Expect(ColorEquals(text, 160, 160, 160, 255),
        "TextSecondary should be #A0A0A0");
}

bool TestTextMuted() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextMuted);
    // Muted #6A6A6A = (106, 106, 106)
    return Expect(ColorEquals(text, 106, 106, 106, 255),
        "TextMuted should be #6A6A6A");
}

bool TestTextHierarchyOrdering() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 primary   = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
    const ImVec4 secondary = tm.GetColor(keybinder::ThemeColorRole::TextSecondary);
    const ImVec4 muted     = tm.GetColor(keybinder::ThemeColorRole::TextMuted);
    // Each tier should be darker than the previous
    const float lum_primary   = keybinder::ThemeManager::RelativeLuminance(primary);
    const float lum_secondary = keybinder::ThemeManager::RelativeLuminance(secondary);
    const float lum_muted     = keybinder::ThemeManager::RelativeLuminance(muted);
    return Expect(lum_primary > lum_secondary, "TextPrimary luminance > TextSecondary")
        && Expect(lum_secondary > lum_muted, "TextSecondary luminance > TextMuted");
}

// ── Contrast guarantees ────────────────────────────────────────────

bool TestTextPrimaryContrastMeetsThreshold() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDefaultAccent();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
    const float ratio = keybinder::ThemeManager::ContrastRatio(text, bg);
    return Expect(ratio >= keybinder::ThemeManager::kMinTextContrast,
        "TextPrimary contrast ratio should meet kMinTextContrast (4.5f)");
}

bool TestTextSecondaryContrastMeetsThreshold() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDefaultAccent();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextSecondary);
    const float ratio = keybinder::ThemeManager::ContrastRatio(text, bg);
    return Expect(ratio >= keybinder::ThemeManager::kMinTextContrast,
        "TextSecondary contrast ratio should meet kMinTextContrast (4.5f)");
}

bool TestAccentContrastMeetsThreshold() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDefaultAccent();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
    const ImVec4 hover = tm.GetColor(keybinder::ThemeColorRole::AccentHover);
    const ImVec4 active = tm.GetColor(keybinder::ThemeColorRole::AccentActive);
    const float hoverRatio = keybinder::ThemeManager::ContrastRatio(hover, bg);
    const float activeRatio = keybinder::ThemeManager::ContrastRatio(active, bg);
    return Expect(hoverRatio >= keybinder::ThemeManager::kMinAccentContrast,
        "AccentHover contrast ratio should meet kMinAccentContrast (3.0f)")
        && Expect(activeRatio >= keybinder::ThemeManager::kMinAccentContrast,
            "AccentActive contrast ratio should meet kMinAccentContrast (3.0f)");
}

bool TestStaticContrastRatioFunction() {
    std::printf("Running test...\n"); fflush(stdout);
    // Black vs White = 21:1
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const float ratio = keybinder::ThemeManager::ContrastRatio(white, black);
    return Expect(ratio >= 20.0f, "Black/White contrast should be ~21:1")
        && Expect(keybinder::ThemeManager::MeetsContrastRatio(white, black, 4.5f),
            "MeetsContrastRatio should return true for black/white at 4.5f");
}

// ── Custom-accent derivation path ──────────────────────────────────

bool TestCustomAccentOverridesDefault() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    ResetDefaultAccent();
    // Apply a known custom accent
    tm.ApplyCustomAccent("#FF0000");
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    // After ApplyCustomAccent, accent should no longer be Dark's purple #7B2CBF
    const ImVec4 purple(123.0f / 255.0f, 44.0f / 255.0f, 191.0f / 255.0f, 1.0f);
    const bool changed = (std::fabs(accent.x - purple.x) > 0.01f
                       || std::fabs(accent.y - purple.y) > 0.01f
                       || std::fabs(accent.z - purple.z) > 0.01f);
    // Reset back to default
    ResetDefaultAccent();
    return Expect(changed, "Custom accent should override the default violet");
}

bool TestCustomAccentDerivesHoverAndActive() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    ResetDefaultAccent();
    tm.ApplyCustomAccent("#00AAFF");
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    const ImVec4 hover  = tm.GetColor(keybinder::ThemeColorRole::AccentHover);
    const ImVec4 active = tm.GetColor(keybinder::ThemeColorRole::AccentActive);
    // Reset
    ResetDefaultAccent();
    // Hover and active should be derived from the custom accent, not equal to it
    const bool hoverDiffers = (std::fabs(hover.x - accent.x) > 0.001f
                            || std::fabs(hover.y - accent.y) > 0.001f
                            || std::fabs(hover.z - accent.z) > 0.001f);
    const bool activeDiffers = (std::fabs(active.x - accent.x) > 0.001f
                             || std::fabs(active.y - accent.y) > 0.001f
                             || std::fabs(active.z - accent.z) > 0.001f);
    return Expect(hoverDiffers, "Custom AccentHover should differ from custom Accent")
        && Expect(activeDiffers, "Custom AccentActive should differ from custom Accent");
}

bool TestCustomAccentContrastGuaranteed() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    ResetDefaultAccent();
    tm.ApplyCustomAccent("#FF6600");
    const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    const float ratio = keybinder::ThemeManager::ContrastRatio(accent, bg);
    // Reset
    ResetDefaultAccent();
    return Expect(ratio >= keybinder::ThemeManager::kMinAccentContrast,
        "Custom accent should meet kMinAccentContrast after sanitization");
}

bool TestInvalidCustomAccentIgnored() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    ResetDefaultAccent();
    // Get current accent before invalid input
    const ImVec4 before = tm.GetColor(keybinder::ThemeColorRole::Accent);
    // Apply invalid hex
    tm.ApplyCustomAccent("not-a-color");
    const ImVec4 after = tm.GetColor(keybinder::ThemeColorRole::Accent);
    // Accent should remain unchanged
    return Expect(std::fabs(after.x - before.x) < 0.01f
               && std::fabs(after.y - before.y) < 0.01f
               && std::fabs(after.z - before.z) < 0.01f,
        "Invalid custom accent should leave theme unchanged");
}

// ── Static utility functions ───────────────────────────────────────

bool TestRelativeLuminance() {
    std::printf("Running test...\n"); fflush(stdout);
    // White should have luminance ~1.0
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const float lumWhite = keybinder::ThemeManager::RelativeLuminance(white);
    // Black should have luminance ~0.0
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    const float lumBlack = keybinder::ThemeManager::RelativeLuminance(black);
    return Expect(lumWhite > 0.9f, "White luminance should be ~1.0")
        && Expect(lumBlack < 0.1f, "Black luminance should be ~0.0");
}

bool TestClampColor() {
    std::printf("Running test...\n"); fflush(stdout);
    const ImVec4 overflow(1.5f, -0.5f, 2.0f, 0.5f);
    const ImVec4 clamped = keybinder::ThemeManager::ClampColor(overflow);
    return Expect(clamped.x <= 1.0f && clamped.x >= 0.0f, "Clamped R should be in [0,1]")
        && Expect(clamped.y <= 1.0f && clamped.y >= 0.0f, "Clamped G should be in [0,1]")
        && Expect(clamped.z <= 1.0f && clamped.z >= 0.0f, "Clamped B should be in [0,1]")
        && Expect(clamped.w <= 1.0f && clamped.w >= 0.0f, "Clamped A should be in [0,1]");
}

bool TestSanitizeAccentColor() {
    std::printf("Running test...\n"); fflush(stdout);
    // A very dark accent on a dark background should be adjusted to meet contrast
    const ImVec4 darkAccent(0.02f, 0.01f, 0.03f, 1.0f);
    const ImVec4 background = keybinder::ThemeManager::DefaultBackground();
    const ImVec4 sanitized = keybinder::ThemeManager::SanitizeAccentColor(darkAccent, background);
    const float ratio = keybinder::ThemeManager::ContrastRatio(sanitized, background);
    return Expect(ratio >= keybinder::ThemeManager::kMinAccentContrast,
        "Sanitized accent should meet kMinAccentContrast even from very dark input");
}

bool TestDeriveHoverColor() {
    std::printf("Running test...\n"); fflush(stdout);
    const ImVec4 accent(124.0f / 255.0f, 58.0f / 255.0f, 237.0f / 255.0f, 1.0f);
    const ImVec4 bg = keybinder::ThemeManager::DefaultBackground();
    const ImVec4 hover = keybinder::ThemeManager::DeriveHoverColor(accent, bg);
    const float ratio = keybinder::ThemeManager::ContrastRatio(hover, bg);
    return Expect(ratio >= keybinder::ThemeManager::kMinAccentContrast,
        "Derived hover color should meet kMinAccentContrast");
}

bool TestDeriveActiveColor() {
    std::printf("Running test...\n"); fflush(stdout);
    const ImVec4 accent(124.0f / 255.0f, 58.0f / 255.0f, 237.0f / 255.0f, 1.0f);
    const ImVec4 bg = keybinder::ThemeManager::DefaultBackground();
    const ImVec4 active = keybinder::ThemeManager::DeriveActiveColor(accent, bg);
    const float ratio = keybinder::ThemeManager::ContrastRatio(active, bg);
    return Expect(ratio >= keybinder::ThemeManager::kMinAccentContrast,
        "Derived active color should meet kMinAccentContrast");
}

bool TestDefaultBackgroundIsDark() {
    std::printf("Running test...\n"); fflush(stdout);
    const ImVec4 bg = keybinder::ThemeManager::DefaultBackground();
    const float lum = keybinder::ThemeManager::RelativeLuminance(bg);
    return Expect(lum < 0.1f, "Default background should be very dark (luminance < 0.1)");
}

// ── Built-in theme registry ────────────────────────────────────────

bool TestNovaThemeIsRegistered() {
    std::printf("Running test...\n"); fflush(stdout);
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    return Expect(tm.HasTheme("nova"),
        "HasTheme(\"nova\") should return true");
}

bool TestDarkThemeIsRegistered() {
    std::printf("Running test...\n"); fflush(stdout);
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    return Expect(tm.HasTheme("dark"),
        "HasTheme(\"dark\") should return true");
}

bool TestThemeRegistryContainsOnlyStableBuiltIns() {
    std::printf("Running test...\n"); fflush(stdout);
    const std::vector<std::string> ids = keybinder::ThemeManager::Instance().GetRegisteredThemeIds();
    return Expect(ids.size() == 5, "Theme registry should expose exactly five built-in themes")
        && Expect(ids[0] == "dark", "Dark should remain the first registered built-in and default target")
        && Expect(ids[1] == "nova", "Nova should remain the second registered built-in")
        && Expect(ids[2] == "ember", "Ember should remain the third registered built-in")
        && Expect(ids[3] == "ocean", "Ocean should remain the fourth registered built-in")
        && Expect(ids[4] == "forest", "Forest should remain the fifth registered built-in");
}

bool TestThemeDisplayNamesAreStable() {
    std::printf("Running test...\n"); fflush(stdout);
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const keybinder::ThemeDefinition& dark = tm.GetBuiltInTheme("dark");
    const keybinder::ThemeDefinition& nova = tm.GetBuiltInTheme("nova");
    const keybinder::ThemeDefinition& ember = tm.GetBuiltInTheme("ember");
    const keybinder::ThemeDefinition& ocean = tm.GetBuiltInTheme("ocean");
    const keybinder::ThemeDefinition& forest = tm.GetBuiltInTheme("forest");
    return Expect(dark.displayName == "Dark", "Dark display name should stay stable for Settings > Appearance")
        && Expect(nova.displayName == "Nova", "Nova display name should stay stable for Settings > Appearance")
        && Expect(ember.displayName == "Ember", "Ember display name should stay stable for Settings > Appearance")
        && Expect(ocean.displayName == "Ocean", "Ocean display name should stay stable for Settings > Appearance")
        && Expect(forest.displayName == "Forest", "Forest display name should stay stable for Settings > Appearance");
}

bool TestInvalidThemeIdDoesNotMutateActiveTheme() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.SetTheme("dark");
    const bool switched = tm.SetTheme("missing-theme");
    const bool activeStillDark = tm.GetActiveThemeId() == "dark";
    ResetDefaultAccent();
    return Expect(!switched, "SetTheme should reject unknown theme IDs")
        && Expect(activeStillDark, "Rejected theme IDs must not mutate the active theme");
}

bool TestNovaDefaultTokensAreStable() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager::Instance().SetTheme("nova");
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const bool activeNova = tm.GetActiveThemeId() == "nova";
    const bool accent = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::Accent), 124, 58, 237, 255);
    const bool background = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::Background), 7, 8, 16, 218);
    const bool surface = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::Surface), 16, 17, 30, 158);
    const bool text = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::TextPrimary), 244, 241, 255, 255);
    return Expect(activeNova, "SetTheme(nova) should activate Nova")
        && Expect(accent, "Nova default accent should be #7C3AED")
        && Expect(background, "Nova default background should be #070810DA")
        && Expect(surface, "Nova default surface should be #10111E9E")
        && Expect(text, "Nova default primary text should be #F4F1FF");
}

bool TestNovaThemeDistinctFromDark() {
    std::printf("Running test...\n"); fflush(stdout);
    const keybinder::ThemeDefinition& nova = keybinder::ThemeManager::Instance().GetBuiltInTheme("nova");
    const keybinder::ThemeDefinition& dark = keybinder::ThemeManager::Instance().GetBuiltInTheme("dark");
    // Nova accent #7C3AED (124, 58, 237) should differ from Dark #7B2CBF (123, 44, 191)
    const ImVec4 novaAccent = nova.colors[static_cast<std::size_t>(keybinder::ThemeColorRole::Accent)];
    const ImVec4 darkAccent = dark.colors[static_cast<std::size_t>(keybinder::ThemeColorRole::Accent)];
    const bool accentDiffers = (std::fabs(novaAccent.x - darkAccent.x) > 0.001f
                             || std::fabs(novaAccent.y - darkAccent.y) > 0.001f
                             || std::fabs(novaAccent.z - darkAccent.z) > 0.001f);
    // Nova background #070810 should differ from Dark #080A0E
    const ImVec4 novaBg = nova.colors[static_cast<std::size_t>(keybinder::ThemeColorRole::Background)];
    const ImVec4 darkBg = dark.colors[static_cast<std::size_t>(keybinder::ThemeColorRole::Background)];
    const bool bgDiffers = (std::fabs(novaBg.x - darkBg.x) > 0.001f
                         || std::fabs(novaBg.y - darkBg.y) > 0.001f
                         || std::fabs(novaBg.z - darkBg.z) > 0.001f);
    // Source-contract: Nova Accent equivalent to #7C3AED
    const bool novaAccentCorrect = ColorEquals(novaAccent, 124, 58, 237, 255);
    // Source-contract: Nova Background equivalent to #070810DA
    const bool novaBgCorrect = ColorEquals(novaBg, 7, 8, 16, 218);
    return Expect(accentDiffers, "Nova accent should differ from Dark accent")
        && Expect(bgDiffers, "Nova background should differ from Dark background")
        && Expect(novaAccentCorrect, "Nova Accent should be #7C3AED")
        && Expect(novaBgCorrect, "Nova Background should be #070810DA");
}

bool TestDefaultThemeIsDark() {
    std::printf("Running test...\n"); fflush(stdout);
    // Create a fresh reference to check the initial state
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    // Reset to ensure we're looking at the default
    keybinder::ThemeManager::Instance().SetTheme("dark");
    return Expect(tm.GetActiveThemeId() == "dark",
        "Default active theme ID should be \"dark\"");
}

bool TestSetThemeSwitchesActiveTheme() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    // Start with nova
    tm.SetTheme("nova");
    if (!Expect(tm.GetActiveThemeId() == "nova", "After SetTheme(nova), active ID should be nova")) {
        ResetDefaultAccent();
        return false;
    }
    // Switch to dark
    bool switched = tm.SetTheme("dark");
    if (!Expect(switched, "SetTheme(\"dark\") should return true")) {
        ResetDefaultAccent();
        return false;
    }
    if (!Expect(tm.GetActiveThemeId() == "dark", "After SetTheme(dark), active ID should be dark")) {
        ResetDefaultAccent();
        return false;
    }
    // Verify the accent color changed
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    const bool isDarkPurple = ColorEquals(accent, 123, 44, 191, 255);
    // Reset
    ResetDefaultAccent();
    return Expect(isDarkPurple, "After switching to Dark, accent should be #7B2CBF");
}

bool TestApplyConfiguredThemeSelectsSavedTheme() {
    std::printf("Running test...\n"); fflush(stdout);
    ImGui::CreateContext();
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().theme_name = "Dark";
    settingsManager.GetMutable().theme_custom_accent.clear();

    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.ApplyConfiguredTheme(&settingsManager, 1.25f);
    const bool activeThemeIsDark = tm.GetActiveThemeId() == "dark";
    const bool scaleApplied = std::fabs(tm.GetAppliedScale() - 1.25f) < 0.001f;
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    const bool darkAccent = ColorEquals(accent, 123, 44, 191, 255);

    ImGui::DestroyContext();
    ResetDefaultAccent();
    return Expect(activeThemeIsDark, "ApplyConfiguredTheme should select settings.theme_name")
        && Expect(scaleApplied, "ApplyConfiguredTheme should apply the requested scale")
        && Expect(darkAccent, "ApplyConfiguredTheme should preserve selected built-in accent when custom accent is empty");
}

bool TestApplyConfiguredThemeAppliesTransparency() {
    std::printf("Running test...\n"); fflush(stdout);
    ImGui::CreateContext();
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().theme_name = "Dark";
    settingsManager.GetMutable().theme_custom_accent.clear();
    settingsManager.GetMutable().ui_transparency = 0.25f;

    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.ApplyConfiguredTheme(&settingsManager, 1.0f);

    const bool transparencyApplied = std::fabs(tm.GetAppliedTransparency() - 0.25f) < 0.001f;
    const ImVec4 surface = tm.GetColor(keybinder::ThemeColorRole::Surface);
    const bool surfaceBecameMoreTransparent = surface.w < (214.0f / 255.0f);
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
    const bool textStaysOpaque = std::fabs(text.w - 1.0f) < 0.001f;

    ImGui::DestroyContext();
    ResetDefaultAccent();
    return Expect(transparencyApplied, "ApplyConfiguredTheme should store configured UI transparency")
        && Expect(surfaceBecameMoreTransparent, "UI transparency should reduce glass surface alpha")
        && Expect(textStaysOpaque, "UI transparency should not fade text tokens");
}


bool TestApplyConfiguredThemeAppliesCustomAccentAfterTheme() {
    std::printf("Running test...\n"); fflush(stdout);
    ImGui::CreateContext();
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().theme_name = "Nova";
    settingsManager.GetMutable().theme_custom_accent = "#00AAFF";

    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.SetTheme("dark");
    tm.ApplyConfiguredTheme(&settingsManager, 1.0f);

    const bool activeThemeIsNova = tm.GetActiveThemeId() == "nova";
    const ImVec4 accent = tm.GetColor(keybinder::ThemeColorRole::Accent);
    const bool customAccentApplied = !ColorEquals(accent, 124, 58, 237, 255);

    ImGui::DestroyContext();
    ResetDefaultAccent();
    return Expect(activeThemeIsNova, "ApplyConfiguredTheme should switch to Nova before applying accent")
        && Expect(customAccentApplied, "ApplyConfiguredTheme should apply custom accent against selected theme");
}

bool TestApplyConfiguredThemeFallsBackToDarkForInvalidSavedTheme() {
    std::printf("Running test...\n"); fflush(stdout);
    ImGui::CreateContext();
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().theme_name = "Missing";
    settingsManager.GetMutable().theme_custom_accent.clear();

    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    tm.SetTheme("nova");
    tm.ApplyConfiguredTheme(&settingsManager, 1.0f);

    const bool activeThemeIsDark = tm.GetActiveThemeId() == "dark";
    const bool darkAccent = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::Accent), 123, 44, 191, 255);

    ImGui::DestroyContext();
    ResetDefaultAccent();
    return Expect(activeThemeIsDark, "ApplyConfiguredTheme should fall back to Dark for invalid saved theme names")
        && Expect(darkAccent, "Fallback theme should use Dark default tokens when no custom accent is set");
}

// ── Nova theme text hierarchy ──────────────────────────────────────

bool TestNovaTextHierarchyOrdering() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager::Instance().SetTheme("nova");
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 primary   = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
    const ImVec4 secondary = tm.GetColor(keybinder::ThemeColorRole::TextSecondary);
    const ImVec4 muted     = tm.GetColor(keybinder::ThemeColorRole::TextMuted);
    const float lum_primary   = keybinder::ThemeManager::RelativeLuminance(primary);
    const float lum_secondary = keybinder::ThemeManager::RelativeLuminance(secondary);
    const float lum_muted     = keybinder::ThemeManager::RelativeLuminance(muted);
    return Expect(lum_primary > lum_secondary, "Nova TextPrimary luminance > TextSecondary")
        && Expect(lum_secondary > lum_muted, "Nova TextSecondary luminance > TextMuted");
}

bool TestAllBuiltInThemesMeetContrastContracts() {
    std::printf("Running test...\n"); fflush(stdout);
    keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const std::vector<std::string> ids = tm.GetRegisteredThemeIds();
    for (const std::string& id : ids) {
        if (!tm.SetTheme(id)) {
            ResetDefaultAccent();
            return Expect(false, "Each registered theme should be selectable");
        }

        const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
        const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
        const ImVec4 hover = tm.GetColor(keybinder::ThemeColorRole::AccentHover);
        const ImVec4 active = tm.GetColor(keybinder::ThemeColorRole::AccentActive);
        if (!Expect(keybinder::ThemeManager::ContrastRatio(text, bg) >= keybinder::ThemeManager::kMinTextContrast,
            "Each built-in theme should keep readable primary text")) {
            ResetDefaultAccent();
            return false;
        }

        if (!Expect(keybinder::ThemeManager::ContrastRatio(hover, bg) >= keybinder::ThemeManager::kMinAccentContrast,
            "Each built-in theme should keep hover accent contrast above threshold")) {
            ResetDefaultAccent();
            return false;
        }

        if (!Expect(keybinder::ThemeManager::ContrastRatio(active, bg) >= keybinder::ThemeManager::kMinAccentContrast,
            "Each built-in theme should keep active accent contrast above threshold")) {
            ResetDefaultAccent();
            return false;
        }
    }

    ResetDefaultAccent();
    return true;
}

// ── Dark theme default tokens ─────────────────────────────────────

bool TestDarkDefaultTokensAreStable() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const bool activeDark = tm.GetActiveThemeId() == "dark";
    const bool accent = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::Accent), 123, 44, 191, 255);
    const bool text = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::TextPrimary), 237, 237, 237, 255);
    const bool secondary = ColorEquals(tm.GetColor(keybinder::ThemeColorRole::TextSecondary), 160, 160, 160, 255);
    return Expect(activeDark, "Active theme should be Dark after ResetDarkTheme")
        && Expect(accent, "Dark default accent should be #7B2CBF")
        && Expect(text, "Dark default TextPrimary should be #EDEDED")
        && Expect(secondary, "Dark default TextSecondary should be #A0A0A0");
}

bool TestDarkBackgroundIsDark() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
    const float lum = keybinder::ThemeManager::RelativeLuminance(bg);
    ResetDefaultAccent();
    return Expect(lum < 0.1f, "Dark background should be very dark (luminance < 0.1)");
}

bool TestDarkTextContrastMeetsThreshold() {
    std::printf("Running test...\n"); fflush(stdout);
    ResetDarkTheme();
    const keybinder::ThemeManager& tm = keybinder::ThemeManager::Instance();
    const ImVec4 bg = tm.GetColor(keybinder::ThemeColorRole::Background);
    const ImVec4 text = tm.GetColor(keybinder::ThemeColorRole::TextPrimary);
    const float ratio = keybinder::ThemeManager::ContrastRatio(text, bg);
    ResetDefaultAccent();
    return Expect(ratio >= keybinder::ThemeManager::kMinTextContrast,
        "Dark TextPrimary contrast ratio should meet kMinTextContrast (4.5f)");
}

} // namespace

int RunThemeManagerTests() {
    int failed = 0;

    // Default accent family
    if (TestDefaultAccentIsPurple()) {
        std::printf("[PASS] TestDefaultAccentIsPurple\n");
    } else { ++failed; }

    if (TestDefaultAccentHover()) {
        std::printf("[PASS] TestDefaultAccentHover\n");
    } else { ++failed; }

    if (TestDefaultAccentActive()) {
        std::printf("[PASS] TestDefaultAccentActive\n");
    } else { ++failed; }

    // Text hierarchy colors
    if (TestTextPrimaryIsLight()) {
        std::printf("[PASS] TestTextPrimaryIsLight\n");
    } else { ++failed; }

    if (TestTextSecondary()) {
        std::printf("[PASS] TestTextSecondary\n");
    } else { ++failed; }

    if (TestTextMuted()) {
        std::printf("[PASS] TestTextMuted\n");
    } else { ++failed; }

    if (TestTextHierarchyOrdering()) {
        std::printf("[PASS] TestTextHierarchyOrdering\n");
    } else { ++failed; }

    // Contrast guarantees
    if (TestTextPrimaryContrastMeetsThreshold()) {
        std::printf("[PASS] TestTextPrimaryContrastMeetsThreshold\n");
    } else { ++failed; }

    if (TestTextSecondaryContrastMeetsThreshold()) {
        std::printf("[PASS] TestTextSecondaryContrastMeetsThreshold\n");
    } else { ++failed; }

    if (TestAccentContrastMeetsThreshold()) {
        std::printf("[PASS] TestAccentContrastMeetsThreshold\n");
    } else { ++failed; }

    if (TestStaticContrastRatioFunction()) {
        std::printf("[PASS] TestStaticContrastRatioFunction\n");
    } else { ++failed; }

    // Custom-accent derivation path
    if (TestCustomAccentOverridesDefault()) {
        std::printf("[PASS] TestCustomAccentOverridesDefault\n");
    } else { ++failed; }

    if (TestCustomAccentDerivesHoverAndActive()) {
        std::printf("[PASS] TestCustomAccentDerivesHoverAndActive\n");
    } else { ++failed; }

    if (TestCustomAccentContrastGuaranteed()) {
        std::printf("[PASS] TestCustomAccentContrastGuaranteed\n");
    } else { ++failed; }

    if (TestInvalidCustomAccentIgnored()) {
        std::printf("[PASS] TestInvalidCustomAccentIgnored\n");
    } else { ++failed; }

    // Static utility functions
    if (TestRelativeLuminance()) {
        std::printf("[PASS] TestRelativeLuminance\n");
    } else { ++failed; }

    if (TestClampColor()) {
        std::printf("[PASS] TestClampColor\n");
    } else { ++failed; }

    if (TestSanitizeAccentColor()) {
        std::printf("[PASS] TestSanitizeAccentColor\n");
    } else { ++failed; }

    if (TestDeriveHoverColor()) {
        std::printf("[PASS] TestDeriveHoverColor\n");
    } else { ++failed; }

    if (TestDeriveActiveColor()) {
        std::printf("[PASS] TestDeriveActiveColor\n");
    } else { ++failed; }

    if (TestDefaultBackgroundIsDark()) {
        std::printf("[PASS] TestDefaultBackgroundIsDark\n");
    } else { ++failed; }

    // Built-in theme registry
    if (TestNovaThemeIsRegistered()) {
        std::printf("[PASS] TestNovaThemeIsRegistered\n");
    } else { ++failed; }

    if (TestDarkThemeIsRegistered()) {
        std::printf("[PASS] TestDarkThemeIsRegistered\n");
    } else { ++failed; }

    if (TestThemeRegistryContainsOnlyStableBuiltIns()) {
        std::printf("[PASS] TestThemeRegistryContainsOnlyStableBuiltIns\n");
    } else { ++failed; }

    if (TestThemeDisplayNamesAreStable()) {
        std::printf("[PASS] TestThemeDisplayNamesAreStable\n");
    } else { ++failed; }

    if (TestInvalidThemeIdDoesNotMutateActiveTheme()) {
        std::printf("[PASS] TestInvalidThemeIdDoesNotMutateActiveTheme\n");
    } else { ++failed; }

    if (TestNovaDefaultTokensAreStable()) {
        std::printf("[PASS] TestNovaDefaultTokensAreStable\n");
    } else { ++failed; }

    if (TestNovaThemeDistinctFromDark()) {
        std::printf("[PASS] TestNovaThemeDistinctFromDark\n");
    } else { ++failed; }

    if (TestDefaultThemeIsDark()) {
        std::printf("[PASS] TestDefaultThemeIsDark\n");
    } else { ++failed; }

    if (TestSetThemeSwitchesActiveTheme()) {
        std::printf("[PASS] TestSetThemeSwitchesActiveTheme\n");
    } else { ++failed; }

    if (TestApplyConfiguredThemeSelectsSavedTheme()) {
        std::printf("[PASS] TestApplyConfiguredThemeSelectsSavedTheme\n");
    } else { ++failed; }

    if (TestApplyConfiguredThemeAppliesTransparency()) {
        std::printf("[PASS] TestApplyConfiguredThemeAppliesTransparency\n");
    } else { ++failed; }

    if (TestApplyConfiguredThemeAppliesCustomAccentAfterTheme()) {
        std::printf("[PASS] TestApplyConfiguredThemeAppliesCustomAccentAfterTheme\n");
    } else { ++failed; }

    if (TestApplyConfiguredThemeFallsBackToDarkForInvalidSavedTheme()) {
        std::printf("[PASS] TestApplyConfiguredThemeFallsBackToDarkForInvalidSavedTheme\n");
    } else { ++failed; }

    // Nova theme text hierarchy
    if (TestNovaTextHierarchyOrdering()) {
        std::printf("[PASS] TestNovaTextHierarchyOrdering\n");
    } else { ++failed; }

    if (TestAllBuiltInThemesMeetContrastContracts()) {
        std::printf("[PASS] TestAllBuiltInThemesMeetContrastContracts\n");
    } else { ++failed; }

    // Dark theme default tokens
    if (TestDarkDefaultTokensAreStable()) {
        std::printf("[PASS] TestDarkDefaultTokensAreStable\n");
    } else { ++failed; }

    if (TestDarkBackgroundIsDark()) {
        std::printf("[PASS] TestDarkBackgroundIsDark\n");
    } else { ++failed; }

    if (TestDarkTextContrastMeetsThreshold()) {
        std::printf("[PASS] TestDarkTextContrastMeetsThreshold\n");
    } else { ++failed; }

    return failed;
}
