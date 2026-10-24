/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    ThemeManager - centralized theme tokens and built-in themes
*/
#pragma once

#include <array>
#include <string>
#include <vector>

#include "imgui-1.92.7/imgui.h"

namespace keybinder {

class SettingsManager;

enum class ThemeColorRole {
    Background = 0,
    Surface,
    SurfaceAlt,
    SurfaceElevated,
    Accent,
    AccentHover,
    AccentActive,
    TextPrimary,
    TextSecondary,
    TextMuted,
    TextDisabled,
    Border,
    BorderStrong,
    Hover,
    Active,
    Disabled,
    Success,
    Warning,
    Danger,
    DangerHover,
    Info,
    Selection,
    Count
};

enum class ThemeSpacingRole {
    WindowPadding = 0,
    FramePaddingX,
    FramePaddingY,
    ItemSpacingX,
    ItemSpacingY,
    InnerSpacingX,
    InnerSpacingY,
    SectionGap,
    CardGap,
    Count
};

enum class ThemeRadiusRole {
    Window = 0,
    Child,
    Popup,
    Frame,
    Grab,
    Tab,
    Count
};

enum class ThemeBorderRole {
    Window = 0,
    Child,
    Popup,
    Frame,
    Tab,
    Count
};

enum class ThemeControlHeightRole {
    Button = 0,
    Input,
    Toggle,
    Tab,
    Count
};

enum class ThemeTypographyRole {
    Header = 0,
    Body,
    Caption,
    Monospace,
    Count
};

enum class ThemeMotionRole {
    Fast = 0,
    Normal,
    Slow,
    Glow,
    Count
};

enum class ThemeMotionEasing {
    Linear = 0,
    EaseOutCubic,
    EaseInOutQuad
};

struct ThemeBorderToken {
    float thickness = 0.0f;
    ImVec4 color{};
};

struct ThemeTypographyToken {
    float size = 0.0f;
};

struct ThemeMotionToken {
    float durationSeconds = 0.0f;
    ThemeMotionEasing easing = ThemeMotionEasing::Linear;
};

struct ThemeDefinition {
    std::string id;
    std::string displayName;
    std::string description;
    std::array<ImVec4, static_cast<std::size_t>(ThemeColorRole::Count)> colors{};
    std::array<float, static_cast<std::size_t>(ThemeSpacingRole::Count)> spacing{};
    std::array<float, static_cast<std::size_t>(ThemeRadiusRole::Count)> radii{};
    std::array<ThemeBorderToken, static_cast<std::size_t>(ThemeBorderRole::Count)> borders{};
    std::array<float, static_cast<std::size_t>(ThemeControlHeightRole::Count)> controlHeights{};
    std::array<ThemeTypographyToken, static_cast<std::size_t>(ThemeTypographyRole::Count)> typography{};
    std::array<ThemeMotionToken, static_cast<std::size_t>(ThemeMotionRole::Count)> motion{};
};

class ThemeManager {
public:
    static constexpr float kMinTextContrast = 4.5f;
    static constexpr float kMinAccentContrast = 3.0f;

    static ThemeManager& Instance();

    const ThemeDefinition& GetActiveTheme() const;

    const ImVec4& GetColor(ThemeColorRole role) const;
    float GetSpacing(ThemeSpacingRole role) const;
    float GetRadius(ThemeRadiusRole role) const;
    const ThemeBorderToken& GetBorder(ThemeBorderRole role) const;
    float GetControlHeight(ThemeControlHeightRole role) const;
    const ThemeTypographyToken& GetTypography(ThemeTypographyRole role) const;
    const ThemeMotionToken& GetMotion(ThemeMotionRole role) const;
    float GetAppliedScale() const;
    float GetAppliedTransparency() const;

    void ApplyImGuiStyle(float scale = 1.0f);
    void ApplyCustomAccent(const std::string& hexColor);
    void ApplyConfiguredTheme(const SettingsManager* settingsManager, float scale = 1.0f);
    static const char* GetMotionEasingName(ThemeMotionEasing easing);

    // Built-in theme registry
    bool HasTheme(const std::string& id) const;
    bool SetTheme(const std::string& id);
    const std::string& GetActiveThemeId() const;
    const std::string& GetActiveThemeName() const;
    std::vector<std::string> GetRegisteredThemeIds() const;
    const ThemeDefinition& GetBuiltInTheme(const std::string& id) const;

    static ImVec4 ClampColor(const ImVec4& color);
    static float RelativeLuminance(const ImVec4& color);
    static float ContrastRatio(const ImVec4& a, const ImVec4& b);
    static bool MeetsContrastRatio(const ImVec4& a, const ImVec4& b, float minRatio = kMinTextContrast);

    static ImVec4 SanitizeAccentColor(const ImVec4& accent, const ImVec4& background = DefaultBackground());
    static ImVec4 DeriveHoverColor(const ImVec4& accent, const ImVec4& background = DefaultBackground());
    static ImVec4 DeriveActiveColor(const ImVec4& accent, const ImVec4& background = DefaultBackground());
    static ImVec4 DeriveTextColor(const ImVec4& accent, const ImVec4& background);

    static ImVec4 DefaultBackground();

private:
    ThemeManager();

    void RegisterBuiltInThemes();

    std::vector<ThemeDefinition> builtInThemes_;
    std::string activeThemeId_;
    ThemeDefinition baseTheme_{};
    ThemeDefinition activeTheme_{};
    float appliedScale_ = 1.0f;
    float appliedTransparency_ = 0.0f;
};

} // namespace keybinder
