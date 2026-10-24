#include "../include/MessageVariablesUI.h"
#include "../include/MainWindowUI.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/Logger.h"
#include "../include/Version.h"
#include "../include/SAMPBridge.h"
#include "../include/Hitmarker.h"
#include "../include/HitsoundPlayer.h"
#include "../include/InterfaceEditor.h"
#include "../include/KeyInputUtils.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/ResourceIds.h"
#include "../include/CrosshairUI.h"
#include "../include/CrosshairSwitcher.h"
#include "../include/SquareMapUI.h"
#include "../include/HudSpriteManager.h"
#include "../include/HitsoundUI.h"
#include "../include/InterfaceEditorUI.h"
#include "../include/JobTracker.h"
#include "../include/PersistentPaths.h"
#include "../include/RssFeedManager.h"
#include "../include/TextureManager.h"
#include "../include/JobsUI.h"
#include "../include/OnlineDataManager.h"
#include "../include/SpeedometerManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <windows.h>

namespace keybinder {

static inline float GetLayoutScale() {
    return ThemeManager::Instance().GetAppliedScale();
}

static inline float Scale(float value) {
    return value * GetLayoutScale();
}

static ImVec4 WithAlpha(const ImVec4& color, float alpha) {
    ImVec4 adjusted = color;
    adjusted.w = alpha;
    return adjusted;
}

static ImVec4 BlendColor(const ImVec4& from, const ImVec4& to, float t) {
    return ImVec4(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t,
        from.w + (to.w - from.w) * t);
}

static void DrawMainWindowGlassBackdrop() {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 boundsMin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 boundsMax(boundsMin.x + size.x, boundsMin.y + size.y);
    const float radius = theme.GetRadius(ThemeRadiusRole::Window);
    const float opacityScale = 1.0f - theme.GetAppliedTransparency();
    const ImVec4 surface = theme.GetColor(ThemeColorRole::Surface);
    const ImVec4 elevated = theme.GetColor(ThemeColorRole::SurfaceElevated);
    const ImVec4 accent = theme.GetColor(ThemeColorRole::Accent);
    const ImVec4 text = theme.GetColor(ThemeColorRole::TextPrimary);

    // Honor theme alpha rather than clobbering it: average the two glass surface
    // tokens so Nova reads airy and Dark reads dense.
    ImVec4 shell = BlendColor(surface, elevated, 0.35f);
    shell.w = ((surface.w + elevated.w) * 0.5f) * opacityScale;
    drawList->AddRectFilled(boundsMin, boundsMax, ImGui::GetColorU32(shell), radius);

    // Top sheen as a true vertical gradient instead of a solid rect band.
    const float sheenHeight = (std::min)(size.y * 0.32f, 220.0f);
    const ImU32 sheenTop = ImGui::GetColorU32(WithAlpha(BlendColor(elevated, text, 0.05f), 0.22f * opacityScale));
    const ImU32 sheenBottom = ImGui::GetColorU32(WithAlpha(elevated, 0.0f));
    drawList->AddRectFilledMultiColor(
        boundsMin,
        ImVec2(boundsMax.x, boundsMin.y + sheenHeight),
        sheenTop,
        sheenTop,
        sheenBottom,
        sheenBottom);

    drawList->AddRect(boundsMin, boundsMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.42f * opacityScale)), radius, 0, 1.2f);
    drawList->AddRect(
        ImVec2(boundsMin.x + 1.0f, boundsMin.y + 1.0f),
        ImVec2(boundsMax.x - 1.0f, boundsMax.y - 1.0f),
        ImGui::GetColorU32(WithAlpha(accent, 0.08f * opacityScale)),
        (std::max)(radius - 1.0f, 0.0f),
        0,
        1.0f);
    drawList->AddLine(
        ImVec2(boundsMin.x + radius, boundsMin.y + 1.0f),
        ImVec2(boundsMax.x - radius, boundsMin.y + 1.0f),
        ImGui::GetColorU32(WithAlpha(text, 0.13f)),
        1.0f);
}

namespace {

constexpr char kMainWindowTitle[] = "NLBinder Keybinder";
constexpr float kMainWindowBaseWidth = 1140.0f;
constexpr float kMainWindowBaseHeight = 760.0f;
constexpr float kScaleChangeEpsilon = 0.001f;
constexpr float kMainWindowViewportMargin = 24.0f;
constexpr float kKillfeedReserveTop = 16.0f;
constexpr float kKillfeedReserveRight = 16.0f;
constexpr float kKillfeedReserveWidth = 380.0f;
constexpr float kKillfeedReserveHeight = 220.0f;
constexpr float kKillfeedAvoidanceGap = 20.0f;
float g_lastMainWindowScale = 0.0f;
ImVec2 g_lastMainWindowSize(0.0f, 0.0f);

bool IsMeaningfulWindowSize(const ImVec2& size) {
    return std::isfinite(size.x) && std::isfinite(size.y) && size.x > 0.0f && size.y > 0.0f;
}

bool HasMeaningfulScaleChange(float previousScale, float currentScale) {
    return std::fabs(previousScale - currentScale) > kScaleChangeEpsilon;
}

ImVec2 GetScaledMainWindowSize(float scale) {
    return ImVec2(kMainWindowBaseWidth * scale, kMainWindowBaseHeight * scale);
}

ImVec2 GetStoredMainWindowSize(const SettingsManager* settingsManager, float scale) {
    if (settingsManager == nullptr || !std::isfinite(scale) || scale <= 0.0f) {
        return ImVec2(0.0f, 0.0f);
    }

    const Settings& settings = settingsManager->Get();
    if (!std::isfinite(settings.ui_window_width) || !std::isfinite(settings.ui_window_height)
        || settings.ui_window_width <= 0.0f || settings.ui_window_height <= 0.0f) {
        return ImVec2(0.0f, 0.0f);
    }

    return ImVec2(
        (std::max)(kMainWindowBaseWidth, settings.ui_window_width) * scale,
        (std::max)(kMainWindowBaseHeight, settings.ui_window_height) * scale);
}

ImVec2 GetResponsiveMainWindowSize(ImVec2 previousWindowSize, float previousScale, float currentScale) {
    const ImVec2 minimumSize = GetScaledMainWindowSize(currentScale);
    if (previousWindowSize.x <= 0.0f || previousWindowSize.y <= 0.0f
        || previousScale <= 0.0f
        || !std::isfinite(previousScale)
        || !std::isfinite(currentScale)) {
        return minimumSize;
    }

    const float resizeRatio = currentScale / previousScale;
    const ImVec2 scaledExistingSize(previousWindowSize.x * resizeRatio, previousWindowSize.y * resizeRatio);
    return ImVec2(
        (std::max)(minimumSize.x, scaledExistingSize.x),
        (std::max)(minimumSize.y, scaledExistingSize.y));
}

void RememberMainWindowSize(SettingsManager* settingsManager, const ImVec2& windowSize, float currentScale) {
    if (settingsManager == nullptr || !IsMeaningfulWindowSize(windowSize)
        || !std::isfinite(currentScale) || currentScale <= 0.0f) {
        return;
    }

    Settings& settings = settingsManager->GetMutable();
    settings.ui_window_width = (std::max)(kMainWindowBaseWidth, windowSize.x / currentScale);
    settings.ui_window_height = (std::max)(kMainWindowBaseHeight, windowSize.y / currentScale);
}

bool RectanglesOverlap(const ImVec2& aMin, const ImVec2& aMax, const ImVec2& bMin, const ImVec2& bMax) {
    return aMin.x < bMax.x && aMax.x > bMin.x && aMin.y < bMax.y && aMax.y > bMin.y;
}

float ClampFloat(float value, float minValue, float maxValue) {
    return (std::max)(minValue, (std::min)(value, maxValue));
}

ImVec2 ClampWindowPositionToViewport(const ImVec2& position, const ImVec2& windowSize, const ImGuiViewport* viewport, float scale) {
    if (viewport == nullptr) {
        return position;
    }

    const float margin = kMainWindowViewportMargin * scale;
    const float minX = viewport->Pos.x + margin;
    const float minY = viewport->Pos.y + margin;
    const float maxX = viewport->Pos.x + viewport->Size.x - windowSize.x - margin;
    const float maxY = viewport->Pos.y + viewport->Size.y - windowSize.y - margin;

    ImVec2 clamped = position;
    clamped.x = (maxX >= minX) ? ClampFloat(clamped.x, minX, maxX) : minX;
    clamped.y = (maxY >= minY) ? ClampFloat(clamped.y, minY, maxY) : minY;
    return clamped;
}

ImVec2 GetSafeMainWindowPosition(const ImVec2& windowSize, float scale) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        return ImVec2(0.0f, 0.0f);
    }

    const float margin = kMainWindowViewportMargin * scale;
    const float reserveTop = kKillfeedReserveTop * scale;
    const float reserveRight = kKillfeedReserveRight * scale;
    const float reserveWidth = kKillfeedReserveWidth * scale;
    const float reserveHeight = kKillfeedReserveHeight * scale;
    const float avoidanceGap = kKillfeedAvoidanceGap * scale;

    ImVec2 position(
        viewport->Pos.x + (viewport->Size.x - windowSize.x) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - windowSize.y) * 0.5f);

    const ImVec2 killfeedMin(
        viewport->Pos.x + viewport->Size.x - reserveRight - reserveWidth,
        viewport->Pos.y + reserveTop);
    const ImVec2 killfeedMax(
        viewport->Pos.x + viewport->Size.x - reserveRight,
        killfeedMin.y + reserveHeight);

    const ImVec2 windowMin = position;
    const ImVec2 windowMax(position.x + windowSize.x, position.y + windowSize.y);
    if (RectanglesOverlap(windowMin, windowMax, killfeedMin, killfeedMax)) {
        const float leftAlignedX = killfeedMin.x - windowSize.x - avoidanceGap;
        if (leftAlignedX >= viewport->Pos.x + margin) {
            position.x = leftAlignedX;
        } else {
            position.y = killfeedMax.y + avoidanceGap;
        }
    }

    return ClampWindowPositionToViewport(position, windowSize, viewport, scale);
}

constexpr const char* kIconKeyboard = "\xEE\x8C\x92";
constexpr const char* kIconJobs = "\xEE\xA8\xA3";
constexpr const char* kIconSettings = "\xEE\xA2\xB8";
constexpr const char* kIconHelp = "\xEE\xA3\xBD";
constexpr const char* kIconRss = "\xEE\xA7\x89";
constexpr const char* kIconAppearance = "\xEE\x90\x8A";
constexpr const char* kIconOnline = "\xEE\x98\xBE";
constexpr const char* kIconSelected = "\xEE\x97\x8A";

struct SidebarNavItem {
    NavCategory category;
    const char* label;
    const char* icon;
};

constexpr std::array<SidebarNavItem, 6> kSidebarNavItems = {{
    { NavCategory::Keybinds, "Tastenbelegung", kIconKeyboard },
    { NavCategory::Jobs, "Jobs", kIconJobs },
    { NavCategory::Mods, "Mods", kIconAppearance },
    { NavCategory::Online, "Online", kIconOnline },
    { NavCategory::Settings, "Einstellungen", kIconSettings },
    { NavCategory::Help, "Hilfe", kIconHelp },
}};

constexpr const char* kIconUnselected = "\xEE\xBD\x8A";

constexpr const char* kThemeNames[] = {"Dark", "Nova", "Ember", "Ocean", "Forest"};
constexpr int kThemeNameCount = static_cast<int>(sizeof(kThemeNames) / sizeof(kThemeNames[0]));

int FindThemeNameIndex(std::string themeName) {
    std::transform(themeName.begin(), themeName.end(), themeName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (themeName == "classic") {
        return 0;
    }

    for (int index = 0; index < kThemeNameCount; ++index) {
        std::string candidate = kThemeNames[index];
        std::transform(candidate.begin(), candidate.end(), candidate.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (themeName == candidate) {
            return index;
        }
    }

    return 0;
}

std::string TrimWhitespace(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::string NormalizeUiCommand(std::string command) {
    command = TrimWhitespace(std::move(command));
    while (!command.empty() && command.front() == '/') {
        command.erase(command.begin());
    }

    const std::size_t separator = command.find_first_of(" \t\n\r");
    if (separator != std::string::npos) {
        command.resize(separator);
    }

    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return command;
}

std::string FormatCommandForInput(const std::string& command) {
    if (command.empty()) {
        return {};
    }

    return "/" + command;
}

std::string FormatCommandForDisplay(const std::string& command) {
    return command.empty() ? std::string("deaktiviert") : "/" + command;
}

std::string FormatHexColor(const ImVec4& color) {
    const ImVec4 clamped = ThemeManager::ClampColor(color);
    char buffer[16] = {0};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "#%02X%02X%02X",
        static_cast<int>(clamped.x * 255.0f),
        static_cast<int>(clamped.y * 255.0f),
        static_cast<int>(clamped.z * 255.0f));
    return buffer;
}

ImVec4 ParseHexColor(const std::string& value, const ImVec4& fallback) {
    if (value.size() < 7 || value.front() != '#') {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long rgb = std::strtoul(value.c_str() + 1, &end, 16);
    if (end == nullptr || *end != '\0' || value.size() != 7) {
        return fallback;
    }

    return ImVec4(
        static_cast<float>((rgb >> 16) & 0xFFUL) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFFUL) / 255.0f,
        static_cast<float>(rgb & 0xFFUL) / 255.0f,
        1.0f);
}

void CopyStringToBuffer(const std::string& value, char* buffer, std::size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }

    std::memset(buffer, 0, bufferSize);
    if (!value.empty()) {
        std::strncpy(buffer, value.c_str(), bufferSize - 1);
    }
}

std::string FormatTextDrawIdList(const std::vector<int>& ids) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << ids[i];
    }
    return oss.str();
}

struct ParsedTextDrawIds {
    std::vector<int> validIds;
    std::vector<std::string> invalidTokens;
};

ParsedTextDrawIds ParseTextDrawIdInput(const char* input) {
    constexpr int kMaxTextDrawId = 2048 + 256 - 1;
    ParsedTextDrawIds parsed;
    const std::string value = input != nullptr ? input : "";

    std::size_t pos = 0;
    while (pos < value.size()) {
        const std::size_t commaPos = value.find(',', pos);
        const std::size_t tokenEnd = commaPos == std::string::npos ? value.size() : commaPos;
        std::string token = TrimWhitespace(value.substr(pos, tokenEnd - pos));

        if (!token.empty()) {
            char* end = nullptr;
            const long id = std::strtol(token.c_str(), &end, 10);
            while (end != nullptr && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) {
                ++end;
            }

            if (end == token.c_str() || end == nullptr || *end != '\0' || id < 0 || id > kMaxTextDrawId) {
                parsed.invalidTokens.push_back(std::move(token));
            } else {
                parsed.validIds.push_back(static_cast<int>(id));
            }
        }

        if (commaPos == std::string::npos) {
            break;
        }
        pos = commaPos + 1;
    }

    return parsed;
}

std::string JoinTokens(const std::vector<std::string>& tokens) {
    std::string result;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += tokens[i];
    }
    return result;
}

bool InputTextBuffer(const char* label, char* buffer, std::size_t bufferSize, const char* placeholder = nullptr) {
    if (buffer == nullptr || bufferSize == 0) {
        return false;
    }

    std::string value(buffer);
    if (!UIComponents::InputText(label, &value, placeholder)) {
        return false;
    }

    CopyStringToBuffer(value, buffer, bufferSize);
    return true;
}

bool IsSupportedUiToggleHotkey(int key) {
    switch (key) {
        case 0:
        case VK_ESCAPE:
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
            return false;
        default:
            return true;
    }
}

std::string GetUiToggleHotkeyLabel(int key) {
    return GetLocalizedVirtualKeyName(key);
}

bool IsSupportedQuickMapHotkey(int key) {
    return IsSupportedUiToggleHotkey(key);
}

std::string GetQuickMapHotkeyLabel(int key) {
    return GetLocalizedVirtualKeyName(key);
}

} // namespace

MainWindowUI::MainWindowUI() {
    listUI_.OnCreateClicked = [this]() {
        HandleCreateKeybind();
    };

    listUI_.OnKeybindSelected = [this](const Keybind& keybind) {
        HandleEditKeybind(keybind);
    };

    editorUI_.OnSave([this](const Keybind& keybind) {
        HandleSaveKeybind(keybind);
    });

    editorUI_.OnCancel([this]() {
        editorOpen_ = false;
        creatingNew_ = false;
    });

    editorUI_.OnDelete([this](const std::string& name) {
        HandleDeleteKeybind(name);
    });

    killTrackerUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ == nullptr) {
            return;
        }

        const KillTrackerSettings& killTrackerSettings = killTrackerUI_.GetSettings();
        Settings& settings = settingsManager_->GetMutable();
        settings.kill_tracker_enabled = killTrackerSettings.enabled;
        settings.kill_tracker_count_kills = killTrackerSettings.countKills;
        settings.kill_tracker_count_deaths = killTrackerSettings.countDeaths;
        settings.kill_tracker_local_kill_message_enabled = killTrackerSettings.showLocalKillMessage;
        settings.kill_tracker_local_death_message_enabled = killTrackerSettings.showLocalDeathMessage;
        settings.kill_tracker_server_kill_message_enabled = killTrackerSettings.sendServerKillMessage;
        settings.kill_tracker_server_death_message_enabled = killTrackerSettings.sendServerDeathMessage;
        settings.kill_tracker_track_damage = killTrackerSettings.trackDamage;
        settings.kill_tracker_local_kill_message_template = killTrackerSettings.customLocalKillMessage;
        settings.kill_tracker_local_death_message_template = killTrackerSettings.customLocalDeathMessage;
        settings.kill_tracker_server_kill_message_template = killTrackerSettings.customServerKillMessage;
        settings.kill_tracker_server_death_message_template = killTrackerSettings.customServerDeathMessage;
        settings.kill_tracker_allow_manual_edits = killTrackerSettings.allowManualEdits;

        if (bridge_ != nullptr) {
            bridge_->SetSettingsManager(settingsManager_);
        }

        if (keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    hitmarkerUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    crosshairUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    squareMapUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    hudSpriteUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    hitsoundUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    interfaceEditorUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    rssTrackerUI_.OnSettingsChanged = [this]() {
        if (settingsManager_ != nullptr && keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }

        if (rssFeedManager_ != nullptr) {
            rssFeedManager_->UpdateSettings();
        }
    };
}

void MainWindowUI::SetKeybindManager(KeybindManager* manager) {
    keybindManager_ = manager;
    onlineUI_.SetKeybindManager(manager);
    RefreshKeybindList();
}

void MainWindowUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
    onlineUI_.SetSettingsManager(manager);
    SyncUiOpenSettingsBuffers();
    interfaceEditorUI_.SetSettingsManager(manager);

    if (settingsManager_ != nullptr) {
        SyncKillTrackerSettings();
        hitmarkerUI_.SetSettingsManager(manager);
        crosshairUI_.SetSettingsManager(manager);
        squareMapUI_.SetSettingsManager(manager);
        hudSpriteUI_.SetSettingsManager(manager);
        hitsoundUI_.SetSettingsManager(manager);
        rssTrackerUI_.SetSettingsManager(manager);
    }
}

void MainWindowUI::SyncKillTrackerSettings() {
    if (settingsManager_ == nullptr) return;
    KillTrackerSettings settings = killTrackerUI_.GetSettings();
    settings.enabled = settingsManager_->Get().kill_tracker_enabled;
    settings.countKills = settingsManager_->Get().kill_tracker_count_kills;
    settings.countDeaths = settingsManager_->Get().kill_tracker_count_deaths;
    settings.showLocalKillMessage = settingsManager_->Get().kill_tracker_local_kill_message_enabled;
    settings.showLocalDeathMessage = settingsManager_->Get().kill_tracker_local_death_message_enabled;
    settings.sendServerKillMessage = settingsManager_->Get().kill_tracker_server_kill_message_enabled;
    settings.sendServerDeathMessage = settingsManager_->Get().kill_tracker_server_death_message_enabled;
    settings.trackDamage = settingsManager_->Get().kill_tracker_track_damage;
    settings.customLocalKillMessage = settingsManager_->Get().kill_tracker_local_kill_message_template;
    settings.customLocalDeathMessage = settingsManager_->Get().kill_tracker_local_death_message_template;
    settings.customServerKillMessage = settingsManager_->Get().kill_tracker_server_kill_message_template;
    settings.customServerDeathMessage = settingsManager_->Get().kill_tracker_server_death_message_template;
    settings.allowManualEdits = settingsManager_->Get().kill_tracker_allow_manual_edits;
    killTrackerUI_.SetSettings(settings);
}

void MainWindowUI::SetInterfaceEditor(InterfaceEditor* editor) {
    interfaceEditorUI_.SetInterfaceEditor(editor);
}

void MainWindowUI::SyncUiOpenSettingsBuffers() {
    if (settingsManager_ == nullptr) {
        CopyStringToBuffer({}, uiOpenCommandPrimaryBuffer_.data(), uiOpenCommandPrimaryBuffer_.size());
        CopyStringToBuffer({}, uiOpenCommandSecondaryBuffer_.data(), uiOpenCommandSecondaryBuffer_.size());
        uiOpenSettingsDirty_ = false;
        uiHotkeyCaptureActive_ = false;
        quickMapHotkeyCaptureActive_ = false;
        return;
    }

    const Settings& settings = settingsManager_->Get();
    CopyStringToBuffer(FormatCommandForInput(settings.ui_open_command_primary), uiOpenCommandPrimaryBuffer_.data(), uiOpenCommandPrimaryBuffer_.size());
    CopyStringToBuffer(FormatCommandForInput(settings.ui_open_command_secondary), uiOpenCommandSecondaryBuffer_.data(), uiOpenCommandSecondaryBuffer_.size());
    uiOpenSettingsDirty_ = false;
    uiHotkeyCaptureActive_ = false;
    quickMapHotkeyCaptureActive_ = false;
}

void MainWindowUI::SaveUiOpenSettings() {
    if (settingsManager_ == nullptr) {
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    settings.ui_open_command_primary = NormalizeUiCommand(uiOpenCommandPrimaryBuffer_.data());
    if (settings.ui_open_command_primary.empty()) {
        settings.ui_open_command_primary = "kb";
    }

    settings.ui_open_command_secondary = NormalizeUiCommand(uiOpenCommandSecondaryBuffer_.data());
    if (settings.ui_open_command_secondary == settings.ui_open_command_primary) {
        settings.ui_open_command_secondary.clear();
    }

    SyncUiOpenSettingsBuffers();

    if (keybindManager_ != nullptr) {
        keybindManager_->Save(settingsManager_);
    }
}


void MainWindowUI::SaveSpeedometerSettings() {
    if (settingsManager_ == nullptr) {
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    if (!speedometer::kCustomTachoAvailable) {
        settings.speedometer_enabled = false;
    }
    if (keybindManager_ != nullptr) {
        keybindManager_->Save(settingsManager_);
    }

    if (speedometerManager_ != nullptr) {
        speedometerManager_->OnSettingsChanged(settings.speedometer_enabled);
    }
}

void MainWindowUI::StartUiHotkeyCapture() {
    uiHotkeyCaptureActive_ = true;
    quickMapHotkeyCaptureActive_ = false;
    uiHotkeyCaptureFrame_ = ImGui::GetFrameCount();
    CollectPressedVirtualKeys(uiHotkeyIgnoreKeys_);
}

void MainWindowUI::RenderUiHotkeyCaptureUI() {
    Settings& settings = settingsManager_->GetMutable();

    if (uiHotkeyCaptureActive_) {
        UIComponents::SecondaryButton("Taste drücken... (ESC zum Abbrechen)", ImVec2(Scale(280.0f), 0.0f), true);

        if (ImGui::GetFrameCount() > uiHotkeyCaptureFrame_ + 1) {
            const int vk = FindPressedVirtualKey(uiHotkeyIgnoreKeys_);
            if (vk == VK_ESCAPE) {
                uiHotkeyCaptureActive_ = false;
            } else if (IsSupportedUiToggleHotkey(vk)) {
                settings.ui_toggle_hotkey = vk;
                uiHotkeyCaptureActive_ = false;
                if (keybindManager_ != nullptr) {
                    keybindManager_->Save(settingsManager_);
                }
            }
        }
        return;
    }

    const std::string hotkeyLabel = GetUiToggleHotkeyLabel(settings.ui_toggle_hotkey);
    if (UIComponents::SecondaryButton(hotkeyLabel.c_str(), ImVec2(Scale(220.0f), 0.0f))) {
        StartUiHotkeyCapture();
    }
}

void MainWindowUI::StartQuickMapHotkeyCapture() {
    quickMapHotkeyCaptureActive_ = true;
    uiHotkeyCaptureActive_ = false;
    quickMapHotkeyCaptureFrame_ = ImGui::GetFrameCount();
    CollectPressedVirtualKeys(quickMapHotkeyIgnoreKeys_);
}

void MainWindowUI::RenderQuickMapHotkeyCaptureUI() {
    Settings& settings = settingsManager_->GetMutable();

    if (quickMapHotkeyCaptureActive_) {
        UIComponents::SecondaryButton("Taste drücken... (ESC zum Abbrechen)", ImVec2(Scale(280.0f), 0.0f), true);

        if (ImGui::GetFrameCount() > quickMapHotkeyCaptureFrame_ + 1) {
            const int vk = FindPressedVirtualKey(quickMapHotkeyIgnoreKeys_);
            if (vk == VK_ESCAPE) {
                quickMapHotkeyCaptureActive_ = false;
            } else if (IsSupportedQuickMapHotkey(vk)) {
                settings.quickmap_hotkey = vk;
                quickMapHotkeyCaptureActive_ = false;
                if (keybindManager_ != nullptr) {
                    keybindManager_->Save(settingsManager_);
                }
            }
        }
        return;
    }

    const std::string hotkeyLabel = GetQuickMapHotkeyLabel(settings.quickmap_hotkey);
    if (UIComponents::SecondaryButton(hotkeyLabel.c_str(), ImVec2(Scale(220.0f), 0.0f))) {
        StartQuickMapHotkeyCapture();
    }
}

void MainWindowUI::SetHitmarker(Hitmarker* hitmarker) {
    hitmarkerUI_.SetHitmarker(hitmarker);
}

void MainWindowUI::SetHitsoundPlayer(HitsoundPlayer* player) {
    hitsoundUI_.SetHitsoundPlayer(player);
}

void MainWindowUI::SetSAMPBridge(SAMPBridge* bridge) {
    bridge_ = bridge;
    killTrackerUI_.SetSAMPBridge(bridge);
    jobsUI_.SetSAMPBridge(bridge);

    if (bridge != nullptr && settingsManager_ != nullptr) {
        const Settings& settings = settingsManager_->Get();
        bridge->SetKillTrackerEnabled(settings.kill_tracker_enabled);
        bridge->SetKillTrackerLocalKillMessageEnabled(settings.kill_tracker_local_kill_message_enabled);
        bridge->SetKillTrackerLocalDeathMessageEnabled(settings.kill_tracker_local_death_message_enabled);
        bridge->SetKillTrackerServerKillMessageEnabled(settings.kill_tracker_server_kill_message_enabled);
        bridge->SetKillTrackerServerDeathMessageEnabled(settings.kill_tracker_server_death_message_enabled);
        bridge->SetKillTrackerTrackDamage(settings.kill_tracker_track_damage);
        bridge->SetKillTrackerLocalKillMessageTemplate(settings.kill_tracker_local_kill_message_template);
        bridge->SetKillTrackerLocalDeathMessageTemplate(settings.kill_tracker_local_death_message_template);
        bridge->SetKillTrackerServerKillMessageTemplate(settings.kill_tracker_server_kill_message_template);
        bridge->SetKillTrackerServerDeathMessageTemplate(settings.kill_tracker_server_death_message_template);
    }
}

void MainWindowUI::SetRssFeedManager(RssFeedManager* manager) {
    rssFeedManager_ = manager;
    rssTrackerUI_.SetRssFeedManager(manager);
}

void MainWindowUI::SetSpeedometerManager(SpeedometerManager* manager) {
    speedometerManager_ = manager;
    if (settingsManager_ != nullptr && speedometerManager_ != nullptr) {
        const Settings& settings = settingsManager_->Get();
        speedometerManager_->OnSettingsChanged(settings.speedometer_enabled);
    }
}

jobs::JobTracker* MainWindowUI::GetJobTracker() const {
    return bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
}

void MainWindowUI::OnKeybindsChanged(std::function<void()> callback) {
    onKeybindsChanged_ = std::move(callback);
}

void MainWindowUI::SetCategory(NavCategory category) {
    currentCategory_ = category == NavCategory::KillTracker ? NavCategory::Keybinds : category;
}

void MainWindowUI::PersistWindowState() {
    if (settingsManager_ == nullptr || keybindManager_ == nullptr) {
        return;
    }

    keybindManager_->Save(settingsManager_);
}

void MainWindowUI::Render() {
    ApplyCurrentTheme();

    const float currentScale = GetLayoutScale();
    const bool scaleChanged = HasMeaningfulScaleChange(g_lastMainWindowScale, currentScale);
    const ImVec2 defaultWindowSize = GetScaledMainWindowSize(currentScale);
    const ImVec2 storedWindowSize = GetStoredMainWindowSize(settingsManager_, currentScale);

    ImVec2 targetWindowSize = defaultWindowSize;
    if (IsMeaningfulWindowSize(storedWindowSize)) {
        targetWindowSize = storedWindowSize;
    } else if (scaleChanged) {
        targetWindowSize = GetResponsiveMainWindowSize(g_lastMainWindowSize, g_lastMainWindowScale, currentScale);
    } else if (IsMeaningfulWindowSize(g_lastMainWindowSize)) {
        targetWindowSize = ImVec2(
            (std::max)(defaultWindowSize.x, g_lastMainWindowSize.x),
            (std::max)(defaultWindowSize.y, g_lastMainWindowSize.y));
    }

    ImGui::SetNextWindowSize(targetWindowSize, scaleChanged ? ImGuiCond_Always : ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(GetSafeMainWindowPosition(targetWindowSize, currentScale), ImGuiCond_Appearing);
    g_lastMainWindowScale = currentScale;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;

    bool open = true;
    if (!ImGui::Begin(kMainWindowTitle, &open, flags)) {
        ImGui::End();
        if (!open) {
            closeRequested_ = true;
        }
        return;
    }

    DrawMainWindowGlassBackdrop();

    RenderSidebar();
    ImGui::SameLine(0.0f, ThemeManager::Instance().GetSpacing(ThemeSpacingRole::CardGap));
    RenderContent();

    g_lastMainWindowSize = ImGui::GetWindowSize();
    RememberMainWindowSize(settingsManager_, g_lastMainWindowSize, currentScale);
    ImGui::End();
    
    if (!open) {
        closeRequested_ = true;
    }
}

void MainWindowUI::RenderSidebar() {
    auto& theme = ThemeManager::Instance();
    UIComponents::BeginCardFillHeight("SidebarShell", Scale(sidebarWidth_), true);

    // Keep the sidebar pinned to the top. Keyboard navigation can otherwise
    // scroll this child window to the focused nav item, which makes the logo
    // appear to disappear off-screen even though the texture is still loaded.
    if (ImGui::GetScrollY() != 0.0f) {
        ImGui::SetScrollY(0.0f);
    }
    
    RenderSidebarLogo();
    
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::TextUnformatted("NAVIGATION");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.4f));

    for (int i = 0; i < static_cast<int>(kSidebarNavItems.size()); ++i) {
        const SidebarNavItem& item = kSidebarNavItems[static_cast<std::size_t>(i)];
        bool isSelected = (currentCategory_ == item.category);
        std::string id = "nav_" + std::to_string(static_cast<int>(item.category));

        if (UIComponents::SelectableItem(id.c_str(), item.label, isSelected, item.icon)) {
            currentCategory_ = item.category;
        }
    }

    {
        const float cursorY = ImGui::GetCursorPosY();
        const float contentMaxY = ImGui::GetWindowContentRegionMax().y;
        const float smallFontScale = 0.75f;
        const float textHeight = ImGui::GetTextLineHeightWithSpacing() * smallFontScale;
        const float paddingY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.5f;
        const float targetY = contentMaxY - textHeight - paddingY;
        if (targetY > cursorY) {
            ImGui::SetCursorPosY(targetY);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::SetWindowFontScale(smallFontScale);
        ImGui::TextUnformatted("NLBinder v." NLBINDER_VERSION " by KOBiRO");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    UIComponents::EndCard();
}

bool MainWindowUI::EnsureSidebarLogoLoaded() {
    if (textureManager_ == nullptr || deviceGetter_ == nullptr) {
        return false;
    }

    const std::string sidebarLogoTextureKey = TextureManager::GetResourceTextureKey(IDR_IMAGE_NL_LOGO);
    if (textureManager_->HasTexture(sidebarLogoTextureKey)) {
        return true;
    }

    sidebarLogoLoadAttempted_ = true;
    IDirect3DDevice9* device = deviceGetter_();
    if (device == nullptr) {
        Logger::Warning("[MainWindowUI] Sidebar logo skipped: no D3D9 device available");
        return false;
    }

    return textureManager_->LoadTextureFromResource(IDR_IMAGE_NL_LOGO, device);
}

void MainWindowUI::RenderSidebarLogo() {
    if (!EnsureSidebarLogoLoaded() || textureManager_ == nullptr) {
        return;
    }

    const std::string sidebarLogoTextureKey = TextureManager::GetResourceTextureKey(IDR_IMAGE_NL_LOGO);
    TextureInfo* logo = textureManager_->GetTexture(sidebarLogoTextureKey);
    if (logo == nullptr || logo->texture == nullptr || logo->width <= 0 || logo->height <= 0) {
        return;
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float maxWidth = availableWidth - Scale(12.0f);
    const float maxHeight = Scale(173.0f);
    const float aspectRatio = static_cast<float>(logo->width) / static_cast<float>(logo->height);

    float drawWidth = maxWidth;
    float drawHeight = drawWidth / aspectRatio;
    if (drawHeight > maxHeight) {
        drawHeight = maxHeight;
        drawWidth = drawHeight * aspectRatio;
    }

    const float indent = (std::max)(0.0f, (availableWidth - drawWidth) * 0.5f);
    if (indent > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    }

    ImGui::Image(reinterpret_cast<ImTextureID>(logo->texture), ImVec2(drawWidth, drawHeight));
    ImGui::Dummy(ImVec2(0.0f, ThemeManager::Instance().GetSpacing(ThemeSpacingRole::InnerSpacingY) * 0.8f));
}

void MainWindowUI::RenderContent() {
    const float contentWidth = ImGui::GetContentRegionAvail().x;
    UIComponents::BeginCardFillHeight("ContentShell", contentWidth, false);

    switch (currentCategory_) {
        case NavCategory::Keybinds:
            RenderKeybindsView();
            break;
        case NavCategory::KillTracker:
            currentCategory_ = NavCategory::Keybinds;
            RenderKeybindsView();
            break;
        case NavCategory::Jobs:
            RenderJobsView();
            break;
        case NavCategory::Mods:
            RenderModsView();
            break;
        case NavCategory::Settings:
            RenderSettingsView();
            break;
        case NavCategory::Help:
            RenderHelpView();
            break;
        case NavCategory::Online:
            RenderOnlineView();
            break;
    }

    UIComponents::EndCard();
}

void MainWindowUI::RenderKeybindsView() {
    auto& theme = ThemeManager::Instance();

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float paneGap = theme.GetSpacing(ThemeSpacingRole::CardGap);
    const float preferredLeftPaneWidth = Scale(430.0f);
    const float minLeftPaneWidth = Scale(320.0f);
    const float minRightPaneWidth = Scale(360.0f);

    float scaledLeftPaneWidth = preferredLeftPaneWidth;
    const float maxLeftPaneWidth = availableWidth - minRightPaneWidth - paneGap;
    if (maxLeftPaneWidth > minLeftPaneWidth) {
        scaledLeftPaneWidth = (std::min)((std::max)(preferredLeftPaneWidth, minLeftPaneWidth), maxLeftPaneWidth);
    } else {
        scaledLeftPaneWidth = (std::max)((availableWidth - paneGap) * 0.42f, Scale(280.0f));
    }

    // Right pane takes remaining space; never exceed available width
    // (previous std::max with Scale(280) could create a child window wider than
    // the parent, causing text wrapped at the child's WorkRect to be clipped)
    const float rightPaneWidth = availableWidth - scaledLeftPaneWidth - paneGap;

    UIComponents::BeginCardFillHeight("KeybindListPane", scaledLeftPaneWidth, false);
    listUI_.Render();
    UIComponents::EndCard();

    ImGui::SameLine(0.0f, paneGap);
    
    UIComponents::BeginCardFillHeight("EditorPane", rightPaneWidth, editorOpen_);

    if (!editorOpen_) {
        UIComponents::CardHeader("Details", "Wähle links eine Belegung aus oder lege direkt eine neue an.");
    }

    if (editorOpen_) {
        editorUI_.RenderAsPanel();
    } else {
        ImGui::Spacing();
        const float createButtonSize = theme.GetControlHeight(ThemeControlHeightRole::Button);
        if (UIComponents::PrimaryButton("+", ImVec2(createButtonSize, 0.0f))) {
            HandleCreateKeybind();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Neue Tastenbelegung erstellen");
        }
    }

    UIComponents::EndCard();
}

void MainWindowUI::RenderJobsView() {
    jobsUI_.Render();
}

void MainWindowUI::RenderSettingsView() {
    auto& theme = ThemeManager::Instance();

    if (settingsManager_ == nullptr) {
        UIComponents::BeginCard("SettingsUnavailable", 0.0f, true);
        UIComponents::InfoRow("Status", "Einstellungen nicht verfügbar");
        UIComponents::EndCard();
        return;
    }

    RenderSettingsTabs();

    switch (currentSettingsSubtab_) {
        case SettingsSubtab::General:
            RenderSettingsGeneralTab();
            break;
        case SettingsSubtab::Appearance:
            RenderSettingsAppearanceTab();
            break;
        case SettingsSubtab::QuickMap:
            RenderSettingsQuickMapTab();
            break;
        case SettingsSubtab::RssTracker:
            RenderSettingsRssTrackerTab();
            break;
    }

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
    ImGui::TextWrapped("Einstellungen werden automatisch gespeichert, sobald du Änderungen bestätigst oder das Overlay schließt.");
    ImGui::PopStyleColor();
}

void MainWindowUI::RenderModsView() {
    RenderModsTabs();

    switch (currentModsSubtab_) {
        case ModsSubtab::Hitmarker:
            hitmarkerUI_.Render();
            break;
        case ModsSubtab::Hitsound:
            hitsoundUI_.Render();
            break;
        case ModsSubtab::Hud:
            RenderHudModsTab();
            break;
    }
}

void MainWindowUI::RenderHudModsTab() {
    squareMapUI_.Render();
    ImGui::Spacing();
    interfaceEditorUI_.Render();
    ImGui::Spacing();
    UIComponents::BeginCard("hud-sprite-library", 0.0f);
    RenderHudSpriteLibraryTabs();
    ImGui::Spacing();

    switch (currentHudSpriteLibrarySubtab_) {
        case HudSpriteLibrarySubtab::Crosshair:
            crosshairUI_.Render();
            break;
        case HudSpriteLibrarySubtab::HudIcons:
            hudSpriteUI_.RenderFistLibrary();
            break;
    }

    UIComponents::EndCard();
}

void MainWindowUI::RenderHudSpriteLibraryTabs() {
    UIComponents::BeginSubTabs("HudSpriteLibraryTabs");

    if (UIComponents::SubTabItem("Crosshair", currentHudSpriteLibrarySubtab_ == HudSpriteLibrarySubtab::Crosshair)) {
        currentHudSpriteLibrarySubtab_ = HudSpriteLibrarySubtab::Crosshair;
    }
    if (UIComponents::SubTabItem("Fist", currentHudSpriteLibrarySubtab_ == HudSpriteLibrarySubtab::HudIcons)) {
        currentHudSpriteLibrarySubtab_ = HudSpriteLibrarySubtab::HudIcons;
    }

    UIComponents::EndSubTabs();
}

void MainWindowUI::RenderModsTabs() {
    UIComponents::BeginSubTabs("ModsTabs");

    if (UIComponents::SubTabItem("Hitmarker", currentModsSubtab_ == ModsSubtab::Hitmarker)) {
        currentModsSubtab_ = ModsSubtab::Hitmarker;
    }
    if (UIComponents::SubTabItem("Hitsounds", currentModsSubtab_ == ModsSubtab::Hitsound)) {
        currentModsSubtab_ = ModsSubtab::Hitsound;
    }
    if (UIComponents::SubTabItem("Hud", currentModsSubtab_ == ModsSubtab::Hud)) {
        currentModsSubtab_ = ModsSubtab::Hud;
    }

    UIComponents::EndSubTabs();
}

void MainWindowUI::RenderSettingsTabs() {
    UIComponents::BeginSubTabs("SettingsTabs");

    if (UIComponents::SubTabItem("General", currentSettingsSubtab_ == SettingsSubtab::General)) {
        currentSettingsSubtab_ = SettingsSubtab::General;
    }
    if (UIComponents::SubTabItem("Appearance", currentSettingsSubtab_ == SettingsSubtab::Appearance)) {
        currentSettingsSubtab_ = SettingsSubtab::Appearance;
    }
    if (UIComponents::SubTabItem("QuickMap", currentSettingsSubtab_ == SettingsSubtab::QuickMap)) {
        currentSettingsSubtab_ = SettingsSubtab::QuickMap;
    }
    if (UIComponents::SubTabItem("RSS Tracker", currentSettingsSubtab_ == SettingsSubtab::RssTracker)) {
        currentSettingsSubtab_ = SettingsSubtab::RssTracker;
    }

    UIComponents::EndSubTabs();
}

void MainWindowUI::RenderSettingsGeneralTab() {
    auto& theme = ThemeManager::Instance();
    Settings& settings = settingsManager_->GetMutable();

    auto persistSettings = [this]() {
        if (keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    UIComponents::BeginCard("GameplaySettingsCard", 0.0f, true);
    UIComponents::CardHeader("Gameplay", "Optionen für SA-MP-spezifisches Verhalten");

    if (UIComponents::Toggle("Automatisches HUD Scale Fix", &settings.auto_hudscalefix)) {
        persistSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Führt automatisch /hudscalefix beim Start von SA-MP aus.\nBehebt HUD-Skalierungsprobleme auf hochauflösenden Bildschirmen.");
    }

    if (UIComponents::Toggle("Automatischer Motorstart", &settings.auto_engine_start)) {
        persistSettings();
        if (bridge_ != nullptr) {
            bridge_->SetAutoEngineStart(settings.auto_engine_start);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drückt automatisch die Z-Taste nach dem Einsteigen als Fahrer,\num den Motor sofort zu starten.");
    }

    if (UIComponents::Toggle("Damagelog", &settings.damage_log_enabled)) {
        persistSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zeigt erhaltenen Trefferschaden (HP und Weste) nur in deinem Chat.");
    }

    UIComponents::EndCard();

    UIComponents::BeginCard("ReconnectSettingsCard", 0.0f, true);
    UIComponents::CardHeader("Reconnect", "Spiel schließen und mit /reconnect neu verbinden");
    if (UIComponents::Toggle("Benutzt du den Nova Launcher?", &settings.reconnect_use_nova_launcher)) {
        persistSettings();
    }
    ImGui::TextWrapped(settings.reconnect_use_nova_launcher
        ? "Nova Launcher wird geöffnet. Der Binder versucht dort den normalen Spielstart."
        : "OpenMP startet das Spiel mit dem aktuellen Server, Spielernamen und Spielordner.");
    std::string& launcherPath = settings.reconnect_use_nova_launcher
        ? settings.reconnect_nova_launcher_path : settings.reconnect_openmp_launcher_path;
    UIComponents::InputText("Launcher-Pfad (optional)", &launcherPath, "Leer lassen: automatisch erkennen");
    if (ImGui::IsItemDeactivatedAfterEdit()) persistSettings();
    ImGui::TextWrapped("Der nächste Start wird vorbereitet, bevor das Spiel beendet wird. Bei Nova können Anmeldung oder Launcher-Rückfragen einen eigenen Klick erfordern.");
    UIComponents::EndCard();

    bool saveUiOpenSettings = false;

    UIComponents::BeginCard("UiOpenSettingsCard", 0.0f, true);
    UIComponents::CardHeader("GUI öffnen", "Slash-Befehle und Hotkey für die globale Navigation");
    if (InputTextBuffer("Befehl 1", uiOpenCommandPrimaryBuffer_.data(), uiOpenCommandPrimaryBuffer_.size(), "/kb")) {
        uiOpenSettingsDirty_ = true;
    }
    if (uiOpenSettingsDirty_ && ImGui::IsItemDeactivatedAfterEdit()) {
        saveUiOpenSettings = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Mit oder ohne führenden Schrägstrich eingeben. Leer = Standard /kb.");
    }

    if (InputTextBuffer("Befehl 2", uiOpenCommandSecondaryBuffer_.data(), uiOpenCommandSecondaryBuffer_.size(), "/keybinder")) {
        uiOpenSettingsDirty_ = true;
    }
    if (uiOpenSettingsDirty_ && ImGui::IsItemDeactivatedAfterEdit()) {
        saveUiOpenSettings = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Optionaler zweiter Alias. Leer lassen, um ihn zu deaktivieren.");
    }

    UIComponents::Separator();
    UIComponents::InfoRow("Hotkey", GetUiToggleHotkeyLabel(settings.ui_toggle_hotkey).c_str());
    RenderUiHotkeyCaptureUI();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Klicke auf den Button und drücke dann die gewünschte Taste. ESC bricht ab.");
    }

    ImGui::SameLine();
    if (UIComponents::GhostButton("Standard", ImVec2(Scale(120.0f), 0.0f))) {
        CopyStringToBuffer("/kb", uiOpenCommandPrimaryBuffer_.data(), uiOpenCommandPrimaryBuffer_.size());
        CopyStringToBuffer("/keybinder", uiOpenCommandSecondaryBuffer_.data(), uiOpenCommandSecondaryBuffer_.size());
        settings.ui_toggle_hotkey = VK_F2;
        uiHotkeyCaptureActive_ = false;
        quickMapHotkeyCaptureActive_ = false;
        SaveUiOpenSettings();
        persistSettings();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::TextWrapped("Aktiv: %s, %s und %s",
        FormatCommandForDisplay(settings.ui_open_command_primary).c_str(),
        FormatCommandForDisplay(settings.ui_open_command_secondary).c_str(),
        GetUiToggleHotkeyLabel(settings.ui_toggle_hotkey).c_str());
    ImGui::PopStyleColor();

    if (saveUiOpenSettings) {
        SaveUiOpenSettings();
    }
    UIComponents::EndCard();

    RenderSettingsKillCounterCard();
    RenderSettingsTachoCard();
}

void MainWindowUI::RenderSettingsAppearanceTab() {
    Settings& settings = settingsManager_->GetMutable();

    auto persistSettings = [this]() {
        if (keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    auto applyAppearanceSettings = [this]() {
        ApplyCurrentTheme();
    };

    auto setAccentHex = [&](const std::string& accentHex) {
        if (settings.theme_custom_accent == accentHex) {
            return;
        }

        settings.theme_custom_accent = accentHex;
        applyAppearanceSettings();
        persistSettings();
    };

    auto setCustomAccent = [&](const ImVec4& color) {
        settings.theme_custom_accent = FormatHexColor(color);
        applyAppearanceSettings();
        persistSettings();
    };

    auto& theme = ThemeManager::Instance();

    UIComponents::BeginCard("AppearanceSettingsCard", 0.0f, true);
    UIComponents::CardHeader("Appearance", "Feste Basis mit benutzerdefiniertem Akzent");

    int selectedThemeIndex = FindThemeNameIndex(settings.theme_name);
    if (UIComponents::Combo("Theme", &selectedThemeIndex, kThemeNames, IM_ARRAYSIZE(kThemeNames))) {
        settings.theme_name = kThemeNames[selectedThemeIndex];
        applyAppearanceSettings();
        persistSettings();
    }

    UIComponents::Separator("Transparenz");
    float uiTransparencyPercent = std::clamp(settings.ui_transparency, 0.0f, 0.70f) * 100.0f;
    if (UIComponents::SliderFloat("UI-Transparenz", &uiTransparencyPercent, 0.0f, 70.0f, "%.0f%%")) {
        settings.ui_transparency = std::clamp(uiTransparencyPercent / 100.0f, 0.0f, 0.70f);
        applyAppearanceSettings();
        persistSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Regelt die Glas-Transparenz der NLBinder-Oberflaeche. 0%% entspricht der Standard-Deckkraft.");
    }
    UIComponents::Separator("Akzentfarbe");

    ImVec4 customAccent = ParseHexColor(settings.theme_custom_accent, theme.GetColor(ThemeColorRole::Accent));
    if (UIComponents::ColorPicker("Akzentfarbe wählen", &customAccent, "Klicke, um die Akzentfarbe zu ändern")) {
        setCustomAccent(customAccent);
    }

    const ImVec4 background = theme.GetColor(ThemeColorRole::Background);
    const float contrast = ThemeManager::ContrastRatio(customAccent, background);
    const bool hasLowContrast = contrast < ThemeManager::kMinAccentContrast;
    const ImVec4 contrastColor = hasLowContrast
        ? theme.GetColor(ThemeColorRole::Warning)
        : theme.GetColor(ThemeColorRole::Success);
    const char* contrastLabel = hasLowContrast ? "Warnung" : "Kontrast ok";
    ImGui::TextColored(contrastColor, "%s: %.1f:1 gegen Hintergrund", contrastLabel, contrast);

    UIComponents::Separator();
    if (UIComponents::GhostButton("Akzent zurücksetzen", ImVec2(Scale(180.0f), 0.0f))) {
        settings.theme_custom_accent.clear();
        applyAppearanceSettings();
        persistSettings();
    }

    UIComponents::EndCard();
}

void MainWindowUI::RenderSettingsQuickMapTab() {
    Settings& settings = settingsManager_->GetMutable();

    auto persistQuickMapSettings = [this]() {
        if (keybindManager_ != nullptr) {
            keybindManager_->Save(settingsManager_);
        }
    };

    UIComponents::BeginCard("QuickMapSettingsCard", 0.0f, true);
    UIComponents::CardHeader("QuickMap", "Schnellzugriff auf die GTA:SA Weltkarte und Darstellungsoptionen");

    if (UIComponents::Toggle("QuickMap aktivieren", &settings.quickmap_enabled)) {
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Aktiviert oder deaktiviert die QuickMap-Funktion global.");
    }

    UIComponents::Separator();
    UIComponents::InfoRow("Hotkey", GetQuickMapHotkeyLabel(settings.quickmap_hotkey).c_str());
    RenderQuickMapHotkeyCaptureUI();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Klicke auf den Button und drücke dann die gewünschte Taste. ESC bricht ab.");
    }

    const char* quickMapModeItems[] = { "Umschalten", "Halten" };
    int quickMapModeIndex = settings.quickmap_mode == "hold" ? 1 : 0;
    if (UIComponents::Combo("Modus", &quickMapModeIndex, quickMapModeItems, IM_ARRAYSIZE(quickMapModeItems))) {
        settings.quickmap_mode = quickMapModeIndex == 1 ? "hold" : "toggle";
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Umschalten = einmal drücken zum Ein-/Ausblenden. Halten = Karte nur anzeigen, solange die Taste gedrückt bleibt.");
    }

    if (UIComponents::Toggle("Spieler folgen", &settings.quickmap_follow_player)) {
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zentriert die QuickMap immer auf der Spielerposition und folgt der Bewegung.");
    }

    if (UIComponents::Toggle("Marker fokussieren", &settings.quickmap_focus_marker)) {
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Wenn genau ein roter Marker auf der Karte ist, wird zwischen Spieler und Marker fokussiert und gezoomt, sodass beide sichtbar sind.");
    }

    const char* quickMapVariantItems[] = { "Klassisch", "Satellit" };
    int quickMapVariantIndex = settings.quickmap_map_variant == "satellite" ? 1 : 0;
    if (UIComponents::Combo("Kartenstil", &quickMapVariantIndex, quickMapVariantItems, IM_ARRAYSIZE(quickMapVariantItems))) {
        settings.quickmap_map_variant = quickMapVariantIndex == 1 ? "satellite" : "classic";
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Wechselt zwischen der klassischen GTA:SA-Karte und der Satellitenansicht.");
    }

    const char* quickMapRadarPackItems[] = { "RADAR1", "RADAR2" };
    int quickMapRadarPackIndex = settings.quickmap_radar_pack == "radar2" ? 1 : 0;
    if (UIComponents::Combo("QuickMap Radar-PNGs", &quickMapRadarPackIndex, quickMapRadarPackItems, IM_ARRAYSIZE(quickMapRadarPackItems))) {
        settings.quickmap_radar_pack = quickMapRadarPackIndex == 1 ? "radar2" : "radar1";
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Wählt, welches eingebettete Radar-PNG-Set QuickMap für Blips und Marker nutzt.");
    }

    if (UIComponents::Toggle("Auto Zoom", &settings.quickmap_auto_zoom)) {
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Aktiviert die gespeicherte Auto-Zoom-Stufe. Aus = Start mit der zuletzt manuell genutzten Stufe.");
    }

    const char* quickMapZoomLevelItems[] = { "Weit", "Mittel", "Nah", "Sehr nah", "Extrem" };
    int quickMapZoomLevelIndex = 0;
    const std::string zoomLevelStr = settings.quickmap_zoom_level;
    if (zoomLevelStr == "medium") quickMapZoomLevelIndex = 1;
    else if (zoomLevelStr == "close") quickMapZoomLevelIndex = 2;
    else if (zoomLevelStr == "very_close") quickMapZoomLevelIndex = 3;
    else if (zoomLevelStr == "ultra") quickMapZoomLevelIndex = 4;

    if (!settings.quickmap_auto_zoom) {
        ImGui::BeginDisabled();
    }

    if (UIComponents::Combo("Zoom-Stufe", &quickMapZoomLevelIndex, quickMapZoomLevelItems, IM_ARRAYSIZE(quickMapZoomLevelItems))) {
        const char* zoomLevelValues[] = { "far", "medium", "close", "very_close", "ultra" };
        settings.quickmap_zoom_level = zoomLevelValues[quickMapZoomLevelIndex];
        persistQuickMapSettings();
    }
    if (settings.quickmap_auto_zoom) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Legt die gespeicherte Auto-Zoom-Stufe fest. Mausrad speichert separat die letzte manuelle Stufe.");
        }
    } else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Nur nutzbar, wenn Auto Zoom aktiviert ist.");
    }

    if (!settings.quickmap_auto_zoom) {
        ImGui::EndDisabled();
    }

    float transparencyPercent = settings.quickmap_map_transparency * 100.0f;
    if (ImGui::SliderFloat("Kartentransparenz", &transparencyPercent, 0.0f, 100.0f, "%.0f%%")) {
        settings.quickmap_map_transparency = (std::max)(0.0f, (std::min)(1.0f, transparencyPercent / 100.0f));
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Regelt die Sichtbarkeit der eingeblendeten Kartenstruktur, damit Marker leichter lesbar bleiben.");
    }

    UIComponents::Separator("Gangzonen");

    if (UIComponents::Toggle("Flash bei Gangwar", &settings.quickmap_gangzone_flash_enabled)) {
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Laesst Gangzonen mit Flash-Farbe (z.B. waehrend eines laufenden Gangwars) zwischen beiden Farben blinken, wie auf der normalen Karte.");
    }

    float gangzoneOutlineThickness = settings.quickmap_gangzone_outline_thickness;
    if (ImGui::SliderFloat("Gangzonen-Umrandung", &gangzoneOutlineThickness, 1.0f, 6.0f, "%.1f px")) {
        settings.quickmap_gangzone_outline_thickness = (std::max)(1.0f, (std::min)(6.0f, gangzoneOutlineThickness));
        persistQuickMapSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Verstaerkt die Umrandung von Gangzonen, damit Grenzen auch bei vielen Markern klar lesbar bleiben.");
    }

    UIComponents::EndCard();
}

void MainWindowUI::SaveKillCounterSettings() {
    if (settingsManager_ == nullptr) return;
    const Settings& settings = settingsManager_->Get();
    if (bridge_ != nullptr) {
        bridge_->SetSettingsManager(settingsManager_);
        bridge_->SetKillTrackerEnabled(settings.kill_tracker_enabled);
        bridge_->SetKillTrackerServerKillMessageEnabled(settings.kill_tracker_server_kill_message_enabled);
        bridge_->SetKillTrackerServerKillMessageTemplate(settings.kill_tracker_server_kill_message_template);
    }
    SyncKillTrackerSettings();
    if (keybindManager_ != nullptr) {
        keybindManager_->Save(settingsManager_);
    }
}

void MainWindowUI::RenderSettingsKillCounterCard() {
    Settings& settings = settingsManager_->GetMutable();
    UIComponents::BeginCard("KillCounterSettingsCard", 0.0f, true);
    UIComponents::CardHeader("Kill- & Todeszähler", "Kills und Tode unabhängig erfassen");

    bool killsEnabled = settings.kill_tracker_enabled && settings.kill_tracker_count_kills;
    bool deathsEnabled = settings.kill_tracker_enabled && settings.kill_tracker_count_deaths;
    bool changed = UIComponents::Toggle("Killzähler aktivieren", &killsEnabled);
    changed |= UIComponents::Toggle("Todeszähler aktivieren", &deathsEnabled);
    if (changed) {
        settings.kill_tracker_count_kills = killsEnabled;
        settings.kill_tracker_count_deaths = deathsEnabled;
        settings.kill_tracker_enabled = killsEnabled || deathsEnabled;
        SaveKillCounterSettings();
    }

    ImGui::TextWrapped("Der Todeszähler meldet jeden Tod nur lokal im Chat. Echte Gesamtstände werden aus empfangenen Serverstatistiken übernommen.");
    UIComponents::Separator();
    if (UIComponents::Toggle("Kill-Nachricht an Server senden", &settings.kill_tracker_server_kill_message_enabled)) {
        SaveKillCounterSettings();
    }
    ImGui::TextUnformatted("Nachricht bei einem Kill");
    const float buttonWidth = Scale(110.0f);
    const float inputWidth = (std::max)(Scale(120.0f),
        ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
    if (UIComponents::InputText("##GeneralKillMessage", &settings.kill_tracker_server_kill_message_template,
        "/f Ich habe einen Kill in {zone}. Meine Kills: {total_kills}", inputWidth)) {
        if (bridge_ != nullptr) {
            bridge_->SetKillTrackerServerKillMessageTemplate(settings.kill_tracker_server_kill_message_template);
        }
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SaveKillCounterSettings();
    }
    ImGui::SameLine();
    if (UIComponents::GhostButton("Variablen##GeneralKillMessage", ImVec2(buttonWidth, 0.0f))) {
        showKillMessageVariables_ = true;
    }
    ImGui::TextColored(ThemeManager::Instance().GetColor(ThemeColorRole::TextMuted),
        "%d / 144 Zeichen vor Ersetzung", static_cast<int>(settings.kill_tracker_server_kill_message_template.size()));
    ImGui::TextWrapped("Beispiel: /f Ich habe einen Kill in {zone}. Meine Kills: {total_kills}");
    UIComponents::EndCard();
    RenderMessageVariablesModal("Verfügbare Variablen##GeneralKillVariables", &showKillMessageVariables_, true);
}

void MainWindowUI::RenderSettingsTachoCard() {
    auto& theme = ThemeManager::Instance();
    Settings& settings = settingsManager_->GetMutable();

    UIComponents::BeginCard("TachoSettingsCard", 0.0f, true);
    UIComponents::CardHeader("Tacho");

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Warning));
    ImGui::TextWrapped("Wichtig! Das Tacho blendet nur TachoV2 korrekt aus. Wähle TachoV2 unter /settings aus. Ansonsten ist das Tacho nicht voll funktionsfähig!");
    ImGui::PopStyleColor();

    UIComponents::Separator();
    if (UIComponents::Toggle("Tacho aktivieren", &settings.speedometer_enabled)) {
        SaveSpeedometerSettings();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Zeigt das Custom Tacho nur als Fahrer eines Fahrzeugs an.");
    }

    const speedometer::SpeedometerStyle speedometerStyle = speedometer::ParseSpeedometerStyle(settings.speedometer_style);
    const speedometer::SpeedometerStyle speedometerStyleValues[] = {
        speedometer::SpeedometerStyle::TachometerArc,
        speedometer::SpeedometerStyle::AnalogArc,
        speedometer::SpeedometerStyle::MinimalDigital,
    };
    const char* speedometerStyleItems[] = {
        speedometer::SpeedometerStyleLabel(speedometerStyleValues[0]),
        speedometer::SpeedometerStyleLabel(speedometerStyleValues[1]),
        speedometer::SpeedometerStyleLabel(speedometerStyleValues[2]),
    };
    int speedometerStyleIndex = 0;
    for (int i = 0; i < IM_ARRAYSIZE(speedometerStyleValues); ++i) {
        if (speedometerStyle == speedometerStyleValues[i]) {
            speedometerStyleIndex = i;
            break;
        }
    }
    if (UIComponents::Combo("Tacho-Stil", &speedometerStyleIndex, speedometerStyleItems, IM_ARRAYSIZE(speedometerStyleItems))) {
        settings.speedometer_style = speedometer::SpeedometerStyleId(speedometerStyleValues[speedometerStyleIndex]);
        SaveSpeedometerSettings();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Tachometer Arc zeigt geschätzte RPM; Analog Arc zeigt km/h als Rundinstrument; Minimal Tachometer zeigt km/h, Fuel und Motorzustand als schlanke HUD-Leiste.");
    }

    UIComponents::EndCard();
}

void MainWindowUI::RenderHelpView() {
    auto& theme = ThemeManager::Instance();

    if (settingsManager_ == nullptr) {
        UIComponents::BeginCard("HelpUnavailable", 0.0f, true);
        UIComponents::CardHeader("Hilfe nicht verfügbar", "Die Einstellungen konnten noch nicht geladen werden.");
        UIComponents::InfoRow("Status", "Keine Settings verbunden", &theme.GetColor(ThemeColorRole::Warning));
        UIComponents::EndCard();
        return;
    }

    const Settings& settings = settingsManager_->Get();
    const ImVec4& accentColor = theme.GetColor(ThemeColorRole::Accent);
    const ImVec4& successColor = theme.GetColor(ThemeColorRole::Success);
    const ImVec4& warningColor = theme.GetColor(ThemeColorRole::Warning);
    const ImVec4& mutedColor = theme.GetColor(ThemeColorRole::TextMuted);
    const ImVec4& textSecondaryColor = theme.GetColor(ThemeColorRole::TextSecondary);

    const std::string primaryCommand = FormatCommandForDisplay(settings.ui_open_command_primary);
    const std::string secondaryCommand = FormatCommandForDisplay(settings.ui_open_command_secondary);
    const std::string hotkeyLabel = GetUiToggleHotkeyLabel(settings.ui_toggle_hotkey);
    const std::string quickMapHotkeyLabel = GetQuickMapHotkeyLabel(settings.quickmap_hotkey);

    const std::size_t keybindCount = keybindManager_ != nullptr ? keybindManager_->GetAll().size() : 0;
    const std::size_t rssFeedCount = settings.rss_feeds.size();

    jobs::JobTracker* jobTracker = GetJobTracker();
    const bool jobTrackerAttached = jobTracker != nullptr;
    const bool jobTrackerReady = jobTrackerAttached && jobTracker->IsInitialized();
    const bool jobTrackerHasRegistry = jobTrackerReady && jobTracker->HasRegistryData();

    auto boolStatusText = [](bool enabled) -> const char* {
        return enabled ? "Aktiv" : "Inaktiv";
    };

    auto pathOrUnavailable = [](const std::string& path) -> std::string {
        return path.empty() ? std::string("Nicht verfügbar") : path;
    };

    auto countLabel = [](std::size_t count, const char* singular, const char* plural) -> std::string {
        return std::to_string(count) + " " + (count == 1 ? singular : plural);
    };

    auto renderWrappedText = [&](const char* text, const ImVec4& color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    };

    auto renderPathEntry = [&](const char* label, const std::string& path, const char* hint = nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, textSecondaryColor);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, path == "Nicht verfügbar" ? warningColor : theme.GetColor(ThemeColorRole::TextPrimary));
        ImGui::TextWrapped("%s", path.c_str());
        ImGui::PopStyleColor();

        if (hint != nullptr) {
            ImGui::PushStyleColor(ImGuiCol_Text, mutedColor);
            ImGui::TextWrapped("%s", hint);
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.35f));
    };

    std::string keybindCountLabel = keybindManager_ != nullptr
        ? countLabel(keybindCount, "Eintrag", "Einträge")
        : std::string("Nicht geladen");
    std::string rssCountLabel = rssFeedCount == 0
        ? std::string("Keine Feeds")
        : countLabel(rssFeedCount, "Feed aktiv", "Feeds aktiv");
    std::string jobsStatus = "Nicht verbunden";
    const ImVec4* jobsStatusColor = &warningColor;
    if (jobTrackerAttached && !jobTrackerReady) {
        jobsStatus = "Initialisierung läuft";
    } else if (jobTrackerReady && !jobTrackerHasRegistry) {
        jobsStatus = "Aktiv, aber noch ohne Registry";
    } else if (jobTrackerReady) {
        jobsStatus = "Bereit";
        jobsStatusColor = &successColor;
    }

    const std::string rootDir = pathOrUnavailable(PersistentPaths::RootDir());
    const std::string configPath = pathOrUnavailable(PersistentPaths::ConfigFile());
    const std::string jobsDataPath = pathOrUnavailable(
        jobTracker != nullptr && !jobTracker->GetDataFilePath().empty()
            ? jobTracker->GetDataFilePath()
            : PersistentPaths::JobsDataFile());
    const std::string imguiIniPath = pathOrUnavailable(PersistentPaths::ImGuiIniFile());
    const std::string logPath = pathOrUnavailable(PersistentPaths::LogFile());
    const std::string crashLogPath = pathOrUnavailable(PersistentPaths::CrashLogFile());
    const std::string crosshairPath = pathOrUnavailable(PersistentPaths::CrosshairsDir());
    const std::string hitmarkerPath = pathOrUnavailable(PersistentPaths::HitmarkersDir());

    UIComponents::BeginCard("HelpQuickStart", 0.0f, true);
    UIComponents::CardHeader("Schnellstart & Aktueller Status", "Hier siehst du sofort, wie du NLBinder öffnest und den aktuellen Status deiner Funktionen.");
    UIComponents::InfoRow("Primärer Befehl", primaryCommand.c_str(), &accentColor);
    UIComponents::InfoRow(
        "Sekundärer Befehl",
        secondaryCommand.c_str(),
        settings.ui_open_command_secondary.empty() ? &mutedColor : &textSecondaryColor);
    UIComponents::InfoRow(
        "Overlay-Hotkey",
        hotkeyLabel.c_str(),
        settings.ui_toggle_hotkey == 0 ? &warningColor : &successColor);
    UIComponents::InfoRow(
        "QuickMap-Hotkey",
        quickMapHotkeyLabel.c_str(),
        settings.quickmap_enabled ? &successColor : &mutedColor);
    UIComponents::Separator("Status");
    UIComponents::InfoRow("Tastenbelegungen", keybindCountLabel.c_str(), keybindManager_ != nullptr ? &accentColor : &warningColor);
    UIComponents::InfoRow("Hitmarker", boolStatusText(settings.hitmarker_enabled), settings.hitmarker_enabled ? &successColor : &mutedColor);
    UIComponents::InfoRow("Crosshair", boolStatusText(settings.crosshair_enabled), settings.crosshair_enabled ? &successColor : &mutedColor);
    UIComponents::InfoRow("Hitsounds", boolStatusText(settings.hitsound_outgoing_enabled || settings.hitsound_incoming_enabled), (settings.hitsound_outgoing_enabled || settings.hitsound_incoming_enabled) ? &successColor : &mutedColor);
    UIComponents::InfoRow("QuickMap", boolStatusText(settings.quickmap_enabled), settings.quickmap_enabled ? &successColor : &mutedColor);
    UIComponents::InfoRow("RSS Tracker", rssCountLabel.c_str(), rssFeedCount > 0 ? &accentColor : &mutedColor);
    UIComponents::InfoRow("Job-Tracker", jobsStatus.c_str(), jobsStatusColor);
    UIComponents::Separator("Hinweis");
    if (keybindCount == 0) {
        renderWrappedText("Du hast aktuell noch keine Tastenbelegung gespeichert. Starte am besten im Bereich 'Tastenbelegung' und lege dort deinen ersten Shortcut an.", textSecondaryColor);
    } else if (!settings.hitmarker_enabled && !settings.crosshair_enabled) {
        renderWrappedText("Deine Basisfunktionen sind vorhanden, aber unter 'Mods' ist noch kein Hitmarker oder Crosshair aktiv. Dort kannst du dein Overlay sichtbar erweitern.", textSecondaryColor);
    } else {
        renderWrappedText("Die Kernbereiche sind erreichbar. Weiter unten findest du die exakten Speicherorte für Dateien, Logs und PNG-Assets.", textSecondaryColor);
    }
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::CardGap)));

    UIComponents::BeginCard("HelpAreas", 0.0f, false);
    UIComponents::CardHeader("Bereiche im Überblick", "Jeder Button bringt dich direkt in den passenden Teil der UI.");

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    ImGui::TextUnformatted("Tastenbelegung");
    ImGui::PopStyleColor();
    renderWrappedText("Erstelle Shortcuts, kombiniere mehrere Aktionen und bearbeite vorhandene Einträge.", textSecondaryColor);
    if (UIComponents::AccentOutlineButton("Neue Tastenbelegung anlegen", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Keybinds;
        HandleCreateKeybind();
    }

    UIComponents::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    ImGui::TextUnformatted("Mods");
    ImGui::PopStyleColor();
    renderWrappedText("Hier konfigurierst du Hitmarker, Hitsounds und HUD-Mods wie Crosshair, Squaremap und Fist.", textSecondaryColor);
    if (UIComponents::AccentOutlineButton("Hitmarker konfigurieren", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Mods;
        currentModsSubtab_ = ModsSubtab::Hitmarker;
    }
    if (UIComponents::AccentOutlineButton("Hitsounds", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Mods;
        currentModsSubtab_ = ModsSubtab::Hitsound;
    }
    if (UIComponents::AccentOutlineButton("Hud konfigurieren", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Mods;
        currentModsSubtab_ = ModsSubtab::Hud;
        currentHudSpriteLibrarySubtab_ = HudSpriteLibrarySubtab::Crosshair;
    }

    UIComponents::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    ImGui::TextUnformatted("Einstellungen");
    ImGui::PopStyleColor();
    renderWrappedText("Passe Controls, Gameplay, QuickMap, Theme und RSS-Benachrichtigungen an.", textSecondaryColor);
    if (UIComponents::AccentOutlineButton("Einstellungen öffnen", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Settings;
        currentSettingsSubtab_ = SettingsSubtab::General;
    }
    if (UIComponents::AccentOutlineButton("QuickMap öffnen", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Settings;
        currentSettingsSubtab_ = SettingsSubtab::QuickMap;
    }
    if (UIComponents::AccentOutlineButton("RSS Tracker öffnen", ImVec2(-1.0f, 0.0f))) {
        currentCategory_ = NavCategory::Settings;
        currentSettingsSubtab_ = SettingsSubtab::RssTracker;
    }
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::CardGap)));

    UIComponents::BeginCard("HelpPaths", 0.0f, true);
    UIComponents::CardHeader("Dateien und Ordner", "Alle persistenten Daten liegen im Benutzerordner von GTA San Andreas.");
    renderPathEntry("NLBinder-Root", rootDir, "Hier liegen Konfigurationen, Daten und Logs.");
    renderPathEntry("Einstellungen", configPath, "Globale Plugin- und UI-Einstellungen.");
    renderPathEntry("ImGui-Layout", imguiIniPath, "Fensterpositionen und UI-Layout werden hier statt im Spielverzeichnis gespeichert.");
    renderPathEntry("Job-Daten", jobsDataPath, "Persistierte Events und Auswertungen für den Job-Tracker.");
    renderPathEntry("Crosshairs", crosshairPath, "PNG-Dateien hier ablegen und danach im Crosshair-Tab neu scannen.");
    renderPathEntry("Hitmarker", hitmarkerPath, "PNG-Dateien hier ablegen und danach im Hitmarker-Tab neu scannen.");
    renderPathEntry("Hitsounds", pathOrUnavailable(PersistentPaths::HitsoundsDir()), "WAV-Dateien hier ablegen und danach im Hitsounds-Tab neu scannen.");
    renderPathEntry("Log", logPath, "Hilfreich für allgemeine Fehleranalyse.");
    renderPathEntry("Crash-Log", crashLogPath, "Relevant, wenn das Plugin oder Overlay unerwartet beendet wurde.");
    ImGui::PushStyleColor(ImGuiCol_Text, mutedColor);
    ImGui::TextWrapped("Empfohlen für Assets: PNG und möglichst kompakte Abmessungen, damit Vorschau und Rendern sauber bleiben.");
    ImGui::PopStyleColor();
    UIComponents::EndCard();


}

void MainWindowUI::RenderKillTrackerView() {
    killTrackerUI_.Render();
}

void MainWindowUI::RenderOnlineView() {
    onlineUI_.Render();
}

void MainWindowUI::RenderSettingsRssTrackerTab() {
    rssTrackerUI_.Render();
}

void MainWindowUI::HandleCreateKeybind() {
    editorOpen_ = true;
    creatingNew_ = true;
    editorUI_.OpenForCreate();
}

void MainWindowUI::HandleEditKeybind(const Keybind& keybind) {
    editorOpen_ = true;
    creatingNew_ = false;
    editorUI_.OpenForEdit(keybind);
}

void MainWindowUI::HandleSaveKeybind(const Keybind& keybind) {
    if (!keybindManager_) {
        Logger::Error("[MainWindowUI] HandleSaveKeybind: keybindManager is null");
        return;
    }

    auto& keybinds = keybindManager_->GetAll();

    if (creatingNew_) {
        keybindManager_->Add(keybind);
        Logger::Info("[MainWindowUI] Created new keybind '%s' (key=%d, actions=%zu)",
            keybind.name.c_str(), keybind.key, keybind.actions.size());
    } else {
        bool found = false;
        for (auto& kb : keybinds) {
            if (kb.name == keybind.name) {
                kb = keybind;
                found = true;
                break;
            }
        }
        if (found) {
            Logger::Info("[MainWindowUI] Updated keybind '%s' (key=%d, actions=%zu)",
                keybind.name.c_str(), keybind.key, keybind.actions.size());
        } else {
            Logger::Warning("[MainWindowUI] Could not find keybind '%s' to update, adding as new",
                keybind.name.c_str());
            keybindManager_->Add(keybind);
        }
    }

    if (!keybindManager_->Save(settingsManager_)) {
        Logger::Error("[MainWindowUI] Failed to save keybinds to disk");
    }

    RefreshKeybindList();

    if (onKeybindsChanged_) {
        onKeybindsChanged_();
    }

    editorOpen_ = false;
    creatingNew_ = false;
}

void MainWindowUI::HandleDeleteKeybind(const std::string& name) {
    if (!keybindManager_) {
        Logger::Error("[MainWindowUI] HandleDeleteKeybind: keybindManager is null");
        return;
    }

    auto& keybinds = keybindManager_->GetAll();
    bool found = false;
    for (size_t i = 0; i < keybinds.size(); ++i) {
        if (keybinds[i].name == name) {
            keybindManager_->Remove(i);
            found = true;
            break;
        }
    }

    if (found) {
        Logger::Info("[MainWindowUI] Deleted keybind '%s'", name.c_str());
    } else {
        Logger::Warning("[MainWindowUI] Could not find keybind '%s' to delete", name.c_str());
    }

    if (!keybindManager_->Save(settingsManager_)) {
        Logger::Error("[MainWindowUI] Failed to save keybinds after deletion");
    }

    RefreshKeybindList();

    if (onKeybindsChanged_) {
        onKeybindsChanged_();
    }

    editorOpen_ = false;
    creatingNew_ = false;
}

void MainWindowUI::RefreshKeybindList() {
    if (keybindManager_) {
        listUI_.SetKeybinds(keybindManager_->GetAll());
    }
}

bool MainWindowUI::IsCapturingKey() const {
    return editorUI_.IsCapturingKey() || uiHotkeyCaptureActive_ || quickMapHotkeyCaptureActive_;
}

bool MainWindowUI::WasCloseRequested() const {
    return closeRequested_;
}

void MainWindowUI::ResetCloseRequest() {
    closeRequested_ = false;
}

void MainWindowUI::ApplyCurrentTheme() {
    if (settingsManager_ == nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ThemeManager::Instance().ApplyConfiguredTheme(settingsManager_, GetLayoutScale());
}


void MainWindowUI::SetCrosshairSwitcher(CrosshairSwitcher* switcher) {
    crosshairUI_.SetCrosshairSwitcher(switcher);
}

void MainWindowUI::SetSquareMap(SquareMap* squareMap) {
    squareMapUI_.SetSquareMap(squareMap);
}

void MainWindowUI::SetHudSpriteManager(HudSpriteManager* manager) {
    hudSpriteUI_.SetHudSpriteManager(manager);
}

void MainWindowUI::SetTextureManager(TextureManager* manager) {
    textureManager_ = manager;
    sidebarLogoLoadAttempted_ = false;
    hitmarkerUI_.SetTextureManager(manager);
    crosshairUI_.SetTextureManager(manager);
    hudSpriteUI_.SetTextureManager(manager);
}

void MainWindowUI::SetDeviceGetter(std::function<IDirect3DDevice9*()> getter) {
    deviceGetter_ = getter;
    sidebarLogoLoadAttempted_ = false;
    hitmarkerUI_.SetDeviceGetter(getter);
    crosshairUI_.SetDeviceGetter(getter);
    hudSpriteUI_.SetDeviceGetter(getter);
}

void MainWindowUI::SetOnlineDataManager(OnlineDataManager* manager) {
    onlineUI_.SetOnlineDataManager(manager);
}

void MainWindowUI::SetOnlineOverlayController(OnlineOverlayController* controller) {
    onlineOverlayController_ = controller;
    onlineUI_.SetOnlineOverlayController(controller);
}

void MainWindowUI::SetOnlineOverlayRenderer(OnlineOverlayRenderer* renderer) {
    onlineUI_.SetOnlineOverlayRenderer(renderer);
}

void MainWindowUI::SetOnlineEventDetector(OnlineEventDetector* detector) {
    onlineEventDetector_ = detector;
    onlineUI_.SetOnlineEventDetector(detector);
}

void MainWindowUI::SetOnlineLocalPlayerName(const std::string& localPlayerName) {
    onlineUI_.SetLocalPlayerName(localPlayerName);
}

} // namespace keybinder
