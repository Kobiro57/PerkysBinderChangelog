/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    ThemeManager - centralized theme tokens and built-in themes
*/
#include "../include/ThemeManager.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../include/SettingsManager.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

namespace keybinder {

namespace {

template <typename TEnum>
constexpr std::size_t EnumIndex(TEnum value) {
    return static_cast<std::size_t>(value);
}

ImVec4 MakeColor(int r, int g, int b, int a = 255) {
    return ImVec4(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        static_cast<float>(a) / 255.0f);
}

float ClampUnit(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

float NormalizeUiTransparency(float transparency) {
    if (!std::isfinite(transparency)) {
        return 0.0f;
    }
    return (std::max)(0.0f, (std::min)(0.70f, transparency));
}

void ScaleAlpha(ImVec4& color, float opacityScale) {
    color.w = ClampUnit(color.w * opacityScale);
}

void ApplyUiTransparency(ThemeDefinition& theme, float transparency) {
    const float opacityScale = 1.0f - NormalizeUiTransparency(transparency);
    const ThemeColorRole transparentRoles[] = {
        ThemeColorRole::Background,
        ThemeColorRole::Surface,
        ThemeColorRole::SurfaceAlt,
        ThemeColorRole::SurfaceElevated,
        ThemeColorRole::Border,
        ThemeColorRole::BorderStrong,
        ThemeColorRole::Hover,
        ThemeColorRole::Active,
        ThemeColorRole::Disabled,
        ThemeColorRole::Selection
    };

    for (ThemeColorRole role : transparentRoles) {
        ScaleAlpha(theme.colors[EnumIndex(role)], opacityScale);
    }

    for (ThemeBorderToken& border : theme.borders) {
        ScaleAlpha(border.color, opacityScale);
    }
}

void SetBaseTokens(ThemeDefinition& theme) {
    theme.spacing[EnumIndex(ThemeSpacingRole::WindowPadding)] = 16.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::FramePaddingX)] = 14.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::FramePaddingY)] = 10.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::ItemSpacingX)] = 10.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::ItemSpacingY)] = 10.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::InnerSpacingX)] = 10.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::InnerSpacingY)] = 10.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::SectionGap)] = 20.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::CardGap)] = 20.0f;

    theme.radii[EnumIndex(ThemeRadiusRole::Window)] = 14.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Child)] = 13.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Popup)] = 13.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Frame)] = 12.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Grab)] = 11.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Tab)] = 12.0f;

    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Button)] = 44.0f;
    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Input)] = 42.0f;
    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Toggle)] = 26.0f;
    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Tab)] = 42.0f;

    theme.typography[EnumIndex(ThemeTypographyRole::Header)] = {24.0f};
    theme.typography[EnumIndex(ThemeTypographyRole::Body)] = {18.0f};
    theme.typography[EnumIndex(ThemeTypographyRole::Caption)] = {13.0f};
    theme.typography[EnumIndex(ThemeTypographyRole::Monospace)] = {14.0f};

    theme.motion[EnumIndex(ThemeMotionRole::Fast)] = {0.10f, ThemeMotionEasing::EaseOutCubic};
    theme.motion[EnumIndex(ThemeMotionRole::Normal)] = {0.14f, ThemeMotionEasing::EaseInOutQuad};
    theme.motion[EnumIndex(ThemeMotionRole::Slow)] = {0.16f, ThemeMotionEasing::EaseInOutQuad};
    theme.motion[EnumIndex(ThemeMotionRole::Glow)] = {0.22f, ThemeMotionEasing::EaseOutCubic};
}

void SetBorderToken(ThemeDefinition& theme, ThemeBorderRole role, float thickness, const ImVec4& color) {
    theme.borders[EnumIndex(role)] = {thickness, color};
}

void SetColorToken(ThemeDefinition& theme, ThemeColorRole role, const ImVec4& color) {
    theme.colors[EnumIndex(role)] = color;
}

ThemeDefinition MakeDarkTheme() {
    ThemeDefinition theme;
    theme.id = "dark";
    theme.displayName = "Dark";
    theme.description = "Deep-black glass with purple accent.";
    SetBaseTokens(theme);

    // Deep-black glass surfaces with purple undertone
    SetColorToken(theme, ThemeColorRole::Background, MakeColor(8, 10, 14, 236));        // Deep void
    SetColorToken(theme, ThemeColorRole::Surface, MakeColor(10, 10, 15, 214));          // Glass base
    SetColorToken(theme, ThemeColorRole::SurfaceAlt, MakeColor(14, 14, 20, 204));       // Elevated glass
    SetColorToken(theme, ThemeColorRole::SurfaceElevated, MakeColor(20, 20, 28, 224)); // Highest surface
    
    // Purple accent (#7B2CBF) with derived hover/active
    SetColorToken(theme, ThemeColorRole::Accent, MakeColor(123, 44, 191));              // #7B2CBF
    SetColorToken(theme, ThemeColorRole::AccentHover, ThemeManager::DeriveHoverColor(MakeColor(123, 44, 191), MakeColor(8, 10, 14, 236)));
    SetColorToken(theme, ThemeColorRole::AccentActive, ThemeManager::DeriveActiveColor(MakeColor(123, 44, 191), MakeColor(8, 10, 14, 236)));
    
    // Neutral text hierarchy
    SetColorToken(theme, ThemeColorRole::TextPrimary, MakeColor(237, 237, 237));   // #EDEDED
    SetColorToken(theme, ThemeColorRole::TextSecondary, MakeColor(160, 160, 160)); // #A0A0A0
    SetColorToken(theme, ThemeColorRole::TextMuted, MakeColor(106, 106, 106));     // #6A6A6A
    SetColorToken(theme, ThemeColorRole::TextDisabled, MakeColor(122, 127, 135));  // Readable but restrained disabled / hint text
    
    // Slightly stronger outlines for readability on deep dark surfaces
    SetColorToken(theme, ThemeColorRole::Border, MakeColor(255, 255, 255, 30));       // Readable base outline
    SetColorToken(theme, ThemeColorRole::BorderStrong, MakeColor(255, 255, 255, 54)); // Clear interactive outline
    
    // Interaction states
    SetColorToken(theme, ThemeColorRole::Hover, MakeColor(255, 255, 255, 15));   // Subtle white lift
    SetColorToken(theme, ThemeColorRole::Active, MakeColor(255, 255, 255, 26));  // Slightly stronger
    SetColorToken(theme, ThemeColorRole::Disabled, MakeColor(24, 26, 33));       // Soft off state surface
    
    // Semantic colors remain vibrant but not oversaturated
    SetColorToken(theme, ThemeColorRole::Success, MakeColor(72, 187, 120));
    SetColorToken(theme, ThemeColorRole::Warning, MakeColor(246, 173, 85));
    SetColorToken(theme, ThemeColorRole::Danger, MakeColor(252, 99, 107));
    SetColorToken(theme, ThemeColorRole::DangerHover, MakeColor(255, 128, 136));
    SetColorToken(theme, ThemeColorRole::Info, MakeColor(99, 179, 237));
    
    // Selection with translucent purple accent
    SetColorToken(theme, ThemeColorRole::Selection, MakeColor(123, 44, 191, 48));

    SetBorderToken(theme, ThemeBorderRole::Window, 1.2f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Child, 1.15f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Popup, 1.2f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Frame, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Tab, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    return theme;
}

ThemeDefinition MakeNovaTheme() {
    ThemeDefinition theme;
    theme.id = "nova";
    theme.displayName = "Nova";
    theme.description = "Layered translucent glass with violet accents.";
    SetBaseTokens(theme);

    // Larger radii and tighter chrome make Nova read as a glass shell, not a flat recolor.
    theme.spacing[EnumIndex(ThemeSpacingRole::WindowPadding)] = 18.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::ItemSpacingX)] = 12.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::SectionGap)] = 22.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::CardGap)] = 18.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Window)] = 16.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Child)] = 14.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Popup)] = 14.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Frame)] = 11.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Grab)] = 10.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Tab)] = 12.0f;
    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Button)] = 42.0f;
    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Input)] = 40.0f;
    theme.controlHeights[EnumIndex(ThemeControlHeightRole::Tab)] = 42.0f;

    // Translucent midnight glass surfaces.
    SetColorToken(theme, ThemeColorRole::Background, MakeColor(7, 8, 16, 218));
    SetColorToken(theme, ThemeColorRole::Surface, MakeColor(16, 17, 30, 158));
    SetColorToken(theme, ThemeColorRole::SurfaceAlt, MakeColor(28, 27, 44, 164));
    SetColorToken(theme, ThemeColorRole::SurfaceElevated, MakeColor(42, 39, 64, 190));

    // Violet accent family (#7C3AED).
    SetColorToken(theme, ThemeColorRole::Accent, MakeColor(124, 58, 237));
    SetColorToken(theme, ThemeColorRole::AccentHover, MakeColor(151, 103, 255));
    SetColorToken(theme, ThemeColorRole::AccentActive, MakeColor(139, 92, 246));

    // Text hierarchy tuned for glass overlays.
    SetColorToken(theme, ThemeColorRole::TextPrimary, MakeColor(244, 241, 255));
    SetColorToken(theme, ThemeColorRole::TextSecondary, MakeColor(190, 186, 205));
    SetColorToken(theme, ThemeColorRole::TextMuted, MakeColor(132, 127, 148));
    SetColorToken(theme, ThemeColorRole::TextDisabled, MakeColor(94, 90, 110));

    // Fine glass outlines and active affordances.
    SetColorToken(theme, ThemeColorRole::Border, MakeColor(255, 255, 255, 46));
    SetColorToken(theme, ThemeColorRole::BorderStrong, MakeColor(255, 255, 255, 86));

    SetColorToken(theme, ThemeColorRole::Hover, MakeColor(255, 255, 255, 18));
    SetColorToken(theme, ThemeColorRole::Active, MakeColor(255, 255, 255, 30));
    SetColorToken(theme, ThemeColorRole::Disabled, MakeColor(27, 27, 43, 144));

    SetColorToken(theme, ThemeColorRole::Success, MakeColor(45, 211, 111));
    SetColorToken(theme, ThemeColorRole::Warning, MakeColor(245, 165, 36));
    SetColorToken(theme, ThemeColorRole::Danger, MakeColor(255, 92, 122));
    SetColorToken(theme, ThemeColorRole::DangerHover, MakeColor(255, 112, 112));
    SetColorToken(theme, ThemeColorRole::Info, MakeColor(96, 165, 250));

    SetColorToken(theme, ThemeColorRole::Selection, MakeColor(124, 58, 237, 74));

    SetBorderToken(theme, ThemeBorderRole::Window, 1.25f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Child, 1.2f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Popup, 1.25f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Frame, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Tab, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    return theme;
}

ThemeDefinition MakeEmberTheme() {
    ThemeDefinition theme;
    theme.id = "ember";
    theme.displayName = "Ember";
    theme.description = "Smoky graphite glass with warm orange highlights.";
    SetBaseTokens(theme);

    theme.spacing[EnumIndex(ThemeSpacingRole::WindowPadding)] = 17.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::SectionGap)] = 22.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Window)] = 15.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Child)] = 14.0f;

    SetColorToken(theme, ThemeColorRole::Background, MakeColor(13, 10, 8, 232));
    SetColorToken(theme, ThemeColorRole::Surface, MakeColor(28, 20, 16, 198));
    SetColorToken(theme, ThemeColorRole::SurfaceAlt, MakeColor(42, 29, 20, 204));
    SetColorToken(theme, ThemeColorRole::SurfaceElevated, MakeColor(58, 38, 24, 220));

    SetColorToken(theme, ThemeColorRole::Accent, MakeColor(249, 115, 22));
    SetColorToken(theme, ThemeColorRole::AccentHover, ThemeManager::DeriveHoverColor(MakeColor(249, 115, 22), MakeColor(13, 10, 8, 232)));
    SetColorToken(theme, ThemeColorRole::AccentActive, ThemeManager::DeriveActiveColor(MakeColor(249, 115, 22), MakeColor(13, 10, 8, 232)));

    SetColorToken(theme, ThemeColorRole::TextPrimary, MakeColor(255, 244, 230));
    SetColorToken(theme, ThemeColorRole::TextSecondary, MakeColor(218, 190, 166));
    SetColorToken(theme, ThemeColorRole::TextMuted, MakeColor(151, 122, 100));
    SetColorToken(theme, ThemeColorRole::TextDisabled, MakeColor(124, 102, 86));

    SetColorToken(theme, ThemeColorRole::Border, MakeColor(255, 255, 255, 34));
    SetColorToken(theme, ThemeColorRole::BorderStrong, MakeColor(255, 255, 255, 66));
    SetColorToken(theme, ThemeColorRole::Hover, MakeColor(255, 189, 130, 20));
    SetColorToken(theme, ThemeColorRole::Active, MakeColor(255, 189, 130, 34));
    SetColorToken(theme, ThemeColorRole::Disabled, MakeColor(41, 31, 27, 156));

    SetColorToken(theme, ThemeColorRole::Success, MakeColor(74, 222, 128));
    SetColorToken(theme, ThemeColorRole::Warning, MakeColor(251, 191, 36));
    SetColorToken(theme, ThemeColorRole::Danger, MakeColor(248, 113, 113));
    SetColorToken(theme, ThemeColorRole::DangerHover, MakeColor(252, 165, 165));
    SetColorToken(theme, ThemeColorRole::Info, MakeColor(125, 211, 252));
    SetColorToken(theme, ThemeColorRole::Selection, MakeColor(249, 115, 22, 72));

    SetBorderToken(theme, ThemeBorderRole::Window, 1.2f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Child, 1.15f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Popup, 1.25f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Frame, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Tab, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    return theme;
}

ThemeDefinition MakeOceanTheme() {
    ThemeDefinition theme;
    theme.id = "ocean";
    theme.displayName = "Ocean";
    theme.description = "Deep navy glass with cyan signal accents.";
    SetBaseTokens(theme);

    theme.spacing[EnumIndex(ThemeSpacingRole::WindowPadding)] = 18.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::CardGap)] = 18.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Window)] = 16.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Frame)] = 10.0f;

    SetColorToken(theme, ThemeColorRole::Background, MakeColor(5, 13, 22, 230));
    SetColorToken(theme, ThemeColorRole::Surface, MakeColor(9, 29, 45, 190));
    SetColorToken(theme, ThemeColorRole::SurfaceAlt, MakeColor(13, 43, 64, 198));
    SetColorToken(theme, ThemeColorRole::SurfaceElevated, MakeColor(18, 57, 83, 216));

    SetColorToken(theme, ThemeColorRole::Accent, MakeColor(6, 182, 212));
    SetColorToken(theme, ThemeColorRole::AccentHover, ThemeManager::DeriveHoverColor(MakeColor(6, 182, 212), MakeColor(5, 13, 22, 230)));
    SetColorToken(theme, ThemeColorRole::AccentActive, ThemeManager::DeriveActiveColor(MakeColor(6, 182, 212), MakeColor(5, 13, 22, 230)));

    SetColorToken(theme, ThemeColorRole::TextPrimary, MakeColor(235, 250, 255));
    SetColorToken(theme, ThemeColorRole::TextSecondary, MakeColor(177, 218, 230));
    SetColorToken(theme, ThemeColorRole::TextMuted, MakeColor(109, 151, 168));
    SetColorToken(theme, ThemeColorRole::TextDisabled, MakeColor(87, 116, 130));

    SetColorToken(theme, ThemeColorRole::Border, MakeColor(255, 255, 255, 34));
    SetColorToken(theme, ThemeColorRole::BorderStrong, MakeColor(255, 255, 255, 68));
    SetColorToken(theme, ThemeColorRole::Hover, MakeColor(125, 211, 252, 18));
    SetColorToken(theme, ThemeColorRole::Active, MakeColor(125, 211, 252, 32));
    SetColorToken(theme, ThemeColorRole::Disabled, MakeColor(17, 34, 48, 152));

    SetColorToken(theme, ThemeColorRole::Success, MakeColor(52, 211, 153));
    SetColorToken(theme, ThemeColorRole::Warning, MakeColor(250, 204, 21));
    SetColorToken(theme, ThemeColorRole::Danger, MakeColor(251, 113, 133));
    SetColorToken(theme, ThemeColorRole::DangerHover, MakeColor(253, 164, 175));
    SetColorToken(theme, ThemeColorRole::Info, MakeColor(56, 189, 248));
    SetColorToken(theme, ThemeColorRole::Selection, MakeColor(6, 182, 212, 70));

    SetBorderToken(theme, ThemeBorderRole::Window, 1.2f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Child, 1.15f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Popup, 1.25f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Frame, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Tab, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    return theme;
}

ThemeDefinition MakeForestTheme() {
    ThemeDefinition theme;
    theme.id = "forest";
    theme.displayName = "Forest";
    theme.description = "Charcoal green glass with emerald accents.";
    SetBaseTokens(theme);

    theme.spacing[EnumIndex(ThemeSpacingRole::FramePaddingY)] = 9.0f;
    theme.spacing[EnumIndex(ThemeSpacingRole::SectionGap)] = 21.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Window)] = 13.0f;
    theme.radii[EnumIndex(ThemeRadiusRole::Child)] = 12.0f;

    SetColorToken(theme, ThemeColorRole::Background, MakeColor(6, 16, 12, 232));
    SetColorToken(theme, ThemeColorRole::Surface, MakeColor(12, 32, 24, 196));
    SetColorToken(theme, ThemeColorRole::SurfaceAlt, MakeColor(18, 47, 34, 204));
    SetColorToken(theme, ThemeColorRole::SurfaceElevated, MakeColor(24, 64, 46, 220));

    SetColorToken(theme, ThemeColorRole::Accent, MakeColor(34, 197, 94));
    SetColorToken(theme, ThemeColorRole::AccentHover, ThemeManager::DeriveHoverColor(MakeColor(34, 197, 94), MakeColor(6, 16, 12, 232)));
    SetColorToken(theme, ThemeColorRole::AccentActive, ThemeManager::DeriveActiveColor(MakeColor(34, 197, 94), MakeColor(6, 16, 12, 232)));

    SetColorToken(theme, ThemeColorRole::TextPrimary, MakeColor(238, 255, 244));
    SetColorToken(theme, ThemeColorRole::TextSecondary, MakeColor(190, 225, 201));
    SetColorToken(theme, ThemeColorRole::TextMuted, MakeColor(119, 155, 132));
    SetColorToken(theme, ThemeColorRole::TextDisabled, MakeColor(92, 122, 104));

    SetColorToken(theme, ThemeColorRole::Border, MakeColor(255, 255, 255, 32));
    SetColorToken(theme, ThemeColorRole::BorderStrong, MakeColor(255, 255, 255, 64));
    SetColorToken(theme, ThemeColorRole::Hover, MakeColor(134, 239, 172, 16));
    SetColorToken(theme, ThemeColorRole::Active, MakeColor(134, 239, 172, 30));
    SetColorToken(theme, ThemeColorRole::Disabled, MakeColor(21, 38, 30, 152));

    SetColorToken(theme, ThemeColorRole::Success, MakeColor(74, 222, 128));
    SetColorToken(theme, ThemeColorRole::Warning, MakeColor(234, 179, 8));
    SetColorToken(theme, ThemeColorRole::Danger, MakeColor(248, 113, 113));
    SetColorToken(theme, ThemeColorRole::DangerHover, MakeColor(252, 165, 165));
    SetColorToken(theme, ThemeColorRole::Info, MakeColor(45, 212, 191));
    SetColorToken(theme, ThemeColorRole::Selection, MakeColor(34, 197, 94, 68));

    SetBorderToken(theme, ThemeBorderRole::Window, 1.2f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Child, 1.15f, theme.colors[EnumIndex(ThemeColorRole::Border)]);
    SetBorderToken(theme, ThemeBorderRole::Popup, 1.25f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Frame, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    SetBorderToken(theme, ThemeBorderRole::Tab, 1.15f, theme.colors[EnumIndex(ThemeColorRole::BorderStrong)]);
    return theme;
}

struct DerivedAccentTokens {
    ImVec4 accent{};
    ImVec4 hover{};
    ImVec4 active{};
    ImVec4 selection{};
};

float Clamp01(float value);

DerivedAccentTokens BuildDerivedAccentTokens(const ImVec4& accent, const ImVec4& background) {
    DerivedAccentTokens tokens;
    tokens.accent = ThemeManager::SanitizeAccentColor(accent, background);
    tokens.hover = ThemeManager::DeriveHoverColor(tokens.accent, background);
    tokens.active = ThemeManager::DeriveActiveColor(tokens.accent, background);
    tokens.selection = tokens.accent;
    tokens.selection.w = Clamp01((std::max)(tokens.accent.w * 0.30f, 0.30f));
    return tokens;
}

void ApplyDerivedAccentTokens(ThemeDefinition& theme, const DerivedAccentTokens& tokens) {
    SetColorToken(theme, ThemeColorRole::Accent, tokens.accent);
    SetColorToken(theme, ThemeColorRole::AccentHover, tokens.hover);
    SetColorToken(theme, ThemeColorRole::AccentActive, tokens.active);
    SetColorToken(theme, ThemeColorRole::Selection, tokens.selection);
}

bool TryParseHexColor(const std::string& hexColor, ImVec4& outColor) {
    if (hexColor.length() < 7 || hexColor[0] != '#') {
        return false;
    }

    int r = 0;
    int g = 0;
    int b = 0;
    if (sscanf_s(hexColor.c_str() + 1, "%02x%02x%02x", &r, &g, &b) != 3) {
        return false;
    }

    outColor = MakeColor(r, g, b);
    return true;
}

} // namespace

ThemeManager& ThemeManager::Instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager()
    : builtInThemes_()
    , activeThemeId_("dark") {
    RegisterBuiltInThemes();
    baseTheme_ = GetBuiltInTheme(activeThemeId_);
    activeTheme_ = baseTheme_;
}

void ThemeManager::RegisterBuiltInThemes() {
    builtInThemes_.push_back(MakeDarkTheme());
    builtInThemes_.push_back(MakeNovaTheme());
    builtInThemes_.push_back(MakeEmberTheme());
    builtInThemes_.push_back(MakeOceanTheme());
    builtInThemes_.push_back(MakeForestTheme());
}

const ThemeDefinition& ThemeManager::GetActiveTheme() const {
    return activeTheme_;
}

const ImVec4& ThemeManager::GetColor(ThemeColorRole role) const {
    return GetActiveTheme().colors[EnumIndex(role)];
}

float ThemeManager::GetSpacing(ThemeSpacingRole role) const {
    return GetActiveTheme().spacing[EnumIndex(role)];
}

float ThemeManager::GetRadius(ThemeRadiusRole role) const {
    return GetActiveTheme().radii[EnumIndex(role)];
}

const ThemeBorderToken& ThemeManager::GetBorder(ThemeBorderRole role) const {
    return GetActiveTheme().borders[EnumIndex(role)];
}

float ThemeManager::GetControlHeight(ThemeControlHeightRole role) const {
    return GetActiveTheme().controlHeights[EnumIndex(role)];
}

const ThemeTypographyToken& ThemeManager::GetTypography(ThemeTypographyRole role) const {
    return GetActiveTheme().typography[EnumIndex(role)];
}

const ThemeMotionToken& ThemeManager::GetMotion(ThemeMotionRole role) const {
    return GetActiveTheme().motion[EnumIndex(role)];
}

float ThemeManager::GetAppliedScale() const {
    return appliedScale_;
}

float ThemeManager::GetAppliedTransparency() const {
    return appliedTransparency_;
}

bool ThemeManager::HasTheme(const std::string& id) const {
    for (const auto& theme : builtInThemes_) {
        if (theme.id == id) {
            return true;
        }
    }
    return false;
}

bool ThemeManager::SetTheme(const std::string& id) {
    if (!HasTheme(id)) {
        return false;
    }
    activeThemeId_ = id;
    baseTheme_ = GetBuiltInTheme(id);
    activeTheme_ = baseTheme_;
    return true;
}

const std::string& ThemeManager::GetActiveThemeId() const {
    return activeThemeId_;
}

const std::string& ThemeManager::GetActiveThemeName() const {
    for (const auto& theme : builtInThemes_) {
        if (theme.id == activeThemeId_) {
            return theme.displayName;
        }
    }
    static const std::string kUnknown = "Unknown";
    return kUnknown;
}

std::vector<std::string> ThemeManager::GetRegisteredThemeIds() const {
    std::vector<std::string> ids;
    ids.reserve(builtInThemes_.size());
    for (const auto& theme : builtInThemes_) {
        ids.push_back(theme.id);
    }
    return ids;
}

const ThemeDefinition& ThemeManager::GetBuiltInTheme(const std::string& id) const {
    for (const auto& theme : builtInThemes_) {
        if (theme.id == id) {
            return theme;
        }
    }
    // Fallback to first registered theme
    if (!builtInThemes_.empty()) {
        return builtInThemes_.front();
    }
    static ThemeDefinition kFallback{};
    return kFallback;
}

void ThemeManager::ApplyConfiguredTheme(const SettingsManager* settingsManager, float scale) {
    appliedTransparency_ = 0.0f;
    if (settingsManager != nullptr) {
        const Settings& settings = settingsManager->Get();
        std::string themeId = settings.theme_name;
        std::transform(themeId.begin(), themeId.end(), themeId.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (themeId == "classic") {
            themeId = "dark";
        }

        if (!SetTheme(themeId)) {
            SetTheme("dark");
        }

        if (!settings.theme_custom_accent.empty()) {
            ApplyCustomAccent(settings.theme_custom_accent);
        }

        appliedTransparency_ = NormalizeUiTransparency(settings.ui_transparency);
        ApplyUiTransparency(activeTheme_, appliedTransparency_);
    }

    ApplyImGuiStyle(scale);
}

void ThemeManager::ApplyImGuiStyle(float scale) {
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }

    appliedScale_ = scale;

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(GetSpacing(ThemeSpacingRole::WindowPadding) * scale, GetSpacing(ThemeSpacingRole::WindowPadding) * scale);
    style.FramePadding = ImVec2(GetSpacing(ThemeSpacingRole::FramePaddingX) * scale, GetSpacing(ThemeSpacingRole::FramePaddingY) * scale);
    style.ItemSpacing = ImVec2(GetSpacing(ThemeSpacingRole::ItemSpacingX) * scale, GetSpacing(ThemeSpacingRole::ItemSpacingY) * scale);
    style.ItemInnerSpacing = ImVec2(GetSpacing(ThemeSpacingRole::InnerSpacingX) * scale, GetSpacing(ThemeSpacingRole::InnerSpacingY) * scale);

    style.WindowRounding = GetRadius(ThemeRadiusRole::Window) * scale;
    style.ChildRounding = GetRadius(ThemeRadiusRole::Child) * scale;
    style.PopupRounding = GetRadius(ThemeRadiusRole::Popup) * scale;
    style.FrameRounding = GetRadius(ThemeRadiusRole::Frame) * scale;
    style.GrabRounding = GetRadius(ThemeRadiusRole::Grab) * scale;
    style.TabRounding = GetRadius(ThemeRadiusRole::Tab) * scale;

    style.WindowBorderSize = GetBorder(ThemeBorderRole::Window).thickness;
    style.ChildBorderSize = GetBorder(ThemeBorderRole::Child).thickness;
    style.PopupBorderSize = GetBorder(ThemeBorderRole::Popup).thickness;
    style.FrameBorderSize = GetBorder(ThemeBorderRole::Frame).thickness;
    style.TabBorderSize = GetBorder(ThemeBorderRole::Tab).thickness;
    style.DisabledAlpha = 0.72f;

    style.Colors[ImGuiCol_Text] = GetColor(ThemeColorRole::TextPrimary);
    style.Colors[ImGuiCol_TextDisabled] = GetColor(ThemeColorRole::TextDisabled);
    style.Colors[ImGuiCol_WindowBg] = GetColor(ThemeColorRole::Background);
    style.Colors[ImGuiCol_ChildBg] = GetColor(ThemeColorRole::Surface);
    style.Colors[ImGuiCol_PopupBg] = GetColor(ThemeColorRole::SurfaceElevated);
    style.Colors[ImGuiCol_Border] = GetColor(ThemeColorRole::Border);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = GetColor(ThemeColorRole::SurfaceAlt);
    style.Colors[ImGuiCol_FrameBgHovered] = GetColor(ThemeColorRole::SurfaceElevated);
    style.Colors[ImGuiCol_FrameBgActive] = GetColor(ThemeColorRole::SurfaceElevated);
    style.Colors[ImGuiCol_TitleBg] = GetColor(ThemeColorRole::SurfaceAlt);
    style.Colors[ImGuiCol_TitleBgActive] = GetColor(ThemeColorRole::SurfaceElevated);
    style.Colors[ImGuiCol_TitleBgCollapsed] = GetColor(ThemeColorRole::Surface);
    style.Colors[ImGuiCol_MenuBarBg] = GetColor(ThemeColorRole::SurfaceAlt);
    style.Colors[ImGuiCol_ScrollbarBg] = GetColor(ThemeColorRole::Surface);
    style.Colors[ImGuiCol_ScrollbarGrab] = GetColor(ThemeColorRole::Hover);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = GetColor(ThemeColorRole::AccentHover);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = GetColor(ThemeColorRole::AccentActive);
    style.Colors[ImGuiCol_CheckMark] = GetColor(ThemeColorRole::Accent);
    style.Colors[ImGuiCol_SliderGrab] = GetColor(ThemeColorRole::Accent);
    style.Colors[ImGuiCol_SliderGrabActive] = GetColor(ThemeColorRole::AccentActive);
    style.Colors[ImGuiCol_Button] = GetColor(ThemeColorRole::Accent);
    style.Colors[ImGuiCol_ButtonHovered] = GetColor(ThemeColorRole::AccentHover);
    style.Colors[ImGuiCol_ButtonActive] = GetColor(ThemeColorRole::AccentActive);
    style.Colors[ImGuiCol_Header] = GetColor(ThemeColorRole::SurfaceAlt);
    style.Colors[ImGuiCol_HeaderHovered] = GetColor(ThemeColorRole::Hover);
    style.Colors[ImGuiCol_HeaderActive] = GetColor(ThemeColorRole::Active);
    style.Colors[ImGuiCol_Separator] = GetColor(ThemeColorRole::Border);
    style.Colors[ImGuiCol_SeparatorHovered] = GetColor(ThemeColorRole::BorderStrong);
    style.Colors[ImGuiCol_SeparatorActive] = GetColor(ThemeColorRole::Accent);
    style.Colors[ImGuiCol_ResizeGrip] = GetColor(ThemeColorRole::Accent);
    style.Colors[ImGuiCol_ResizeGripHovered] = GetColor(ThemeColorRole::AccentHover);
    style.Colors[ImGuiCol_ResizeGripActive] = GetColor(ThemeColorRole::AccentActive);
    style.Colors[ImGuiCol_Tab] = GetColor(ThemeColorRole::SurfaceAlt);
    style.Colors[ImGuiCol_TabHovered] = GetColor(ThemeColorRole::AccentHover);
    style.Colors[ImGuiCol_TabSelected] = GetColor(ThemeColorRole::Accent);
    style.Colors[ImGuiCol_TabDimmed] = GetColor(ThemeColorRole::Surface);
    style.Colors[ImGuiCol_TabDimmedSelected] = GetColor(ThemeColorRole::SurfaceElevated);
    style.Colors[ImGuiCol_TableHeaderBg] = GetColor(ThemeColorRole::SurfaceElevated);
    style.Colors[ImGuiCol_TableBorderStrong] = GetColor(ThemeColorRole::BorderStrong);
    style.Colors[ImGuiCol_TableBorderLight] = GetColor(ThemeColorRole::Border);
    style.Colors[ImGuiCol_TableRowBgAlt] = GetColor(ThemeColorRole::SurfaceAlt);
    style.Colors[ImGuiCol_TextSelectedBg] = GetColor(ThemeColorRole::Selection);
    style.Colors[ImGuiCol_NavCursor] = GetColor(ThemeColorRole::Accent);
}

const char* ThemeManager::GetMotionEasingName(ThemeMotionEasing easing) {
    switch (easing) {
    case ThemeMotionEasing::Linear:
        return "linear";
    case ThemeMotionEasing::EaseOutCubic:
        return "ease-out-cubic";
    case ThemeMotionEasing::EaseInOutQuad:
        return "ease-in-out-quad";
    default:
        return "linear";
    }
}

namespace {

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

ImVec4 LerpColor(const ImVec4& from, const ImVec4& to, float t) {
    return ImVec4(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t,
        from.w + (to.w - from.w) * t);
}

ImVec4 AdjustForContrast(const ImVec4& color, const ImVec4& background, float minRatio) {
    ImVec4 sanitized = ThemeManager::ClampColor(color);
    if (ThemeManager::ContrastRatio(sanitized, background) >= minRatio) {
        return sanitized;
    }

    const float backgroundLuminance = ThemeManager::RelativeLuminance(background);
    const ImVec4 target = backgroundLuminance < 0.5f
        ? ImVec4(1.0f, 1.0f, 1.0f, sanitized.w)
        : ImVec4(0.0f, 0.0f, 0.0f, sanitized.w);

    if (ThemeManager::ContrastRatio(target, background) < minRatio) {
        return target;
    }

    ImVec4 best = target;
    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 16; ++i) {
        const float mid = (low + high) * 0.5f;
        const ImVec4 candidate = ThemeManager::ClampColor(LerpColor(sanitized, target, mid));
        if (ThemeManager::ContrastRatio(candidate, background) >= minRatio) {
            best = candidate;
            high = mid;
        } else {
            low = mid;
        }
    }

    return ThemeManager::ClampColor(best);
}

ImVec4 TintAccent(const ImVec4& accent, const ImVec4& background, float amountToTarget, float minRatio) {
    const float backgroundLuminance = ThemeManager::RelativeLuminance(background);
    const ImVec4 target = backgroundLuminance < 0.5f
        ? ImVec4(1.0f, 1.0f, 1.0f, accent.w)
        : ImVec4(0.0f, 0.0f, 0.0f, accent.w);

    return AdjustForContrast(LerpColor(accent, target, amountToTarget), background, minRatio);
}

} // namespace

ImVec4 ThemeManager::ClampColor(const ImVec4& color) {
    return ImVec4(
        Clamp01(std::isfinite(color.x) ? color.x : 0.0f),
        Clamp01(std::isfinite(color.y) ? color.y : 0.0f),
        Clamp01(std::isfinite(color.z) ? color.z : 0.0f),
        Clamp01(std::isfinite(color.w) ? color.w : 1.0f));
}

float ThemeManager::RelativeLuminance(const ImVec4& color) {
    const ImVec4 clamped = ClampColor(color);
    const auto toLinear = [](float channel) -> float {
        if (channel <= 0.04045f) {
            return channel / 12.92f;
        }
        return std::pow((channel + 0.055f) / 1.055f, 2.4f);
    };

    const float r = toLinear(clamped.x);
    const float g = toLinear(clamped.y);
    const float b = toLinear(clamped.z);
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float ThemeManager::ContrastRatio(const ImVec4& a, const ImVec4& b) {
    const float la = RelativeLuminance(a);
    const float lb = RelativeLuminance(b);
    const float brighter = la > lb ? la : lb;
    const float darker = la < lb ? la : lb;
    return (brighter + 0.05f) / (darker + 0.05f);
}

bool ThemeManager::MeetsContrastRatio(const ImVec4& a, const ImVec4& b, float minRatio) {
    return ContrastRatio(a, b) >= minRatio;
}

ImVec4 ThemeManager::SanitizeAccentColor(const ImVec4& accent, const ImVec4& background) {
    return AdjustForContrast(accent, background, kMinAccentContrast);
}

ImVec4 ThemeManager::DeriveHoverColor(const ImVec4& accent, const ImVec4& background) {
    return TintAccent(accent, background, 0.16f, kMinAccentContrast);
}

ImVec4 ThemeManager::DeriveActiveColor(const ImVec4& accent, const ImVec4& background) {
    return TintAccent(accent, background, 0.22f, kMinAccentContrast);
}



ImVec4 ThemeManager::DeriveTextColor(const ImVec4& accent, const ImVec4& background) {
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 base = ContrastRatio(white, background) >= ContrastRatio(black, background) ? white : black;
    const ImVec4 tinted = LerpColor(base, ClampColor(accent), 0.18f);
    return AdjustForContrast(tinted, background, kMinTextContrast);
}

ImVec4 ThemeManager::DefaultBackground() {
    return Instance().GetActiveTheme().colors[EnumIndex(ThemeColorRole::Background)];
}

void ThemeManager::ApplyCustomAccent(const std::string& hexColor) {
    ImVec4 color{};
    if (!TryParseHexColor(hexColor, color)) {
        return;
    }

    const ImVec4 defaultAccent = baseTheme_.colors[EnumIndex(ThemeColorRole::Accent)];
    const bool isDefaultAccent =
        std::fabs(color.x - defaultAccent.x) < 0.0001f &&
        std::fabs(color.y - defaultAccent.y) < 0.0001f &&
        std::fabs(color.z - defaultAccent.z) < 0.0001f;

    activeTheme_ = baseTheme_;
    if (isDefaultAccent) {
        return;
    }

    ApplyDerivedAccentTokens(activeTheme_, BuildDerivedAccentTokens(color, baseTheme_.colors[EnumIndex(ThemeColorRole::Background)]));
}
} // namespace keybinder
