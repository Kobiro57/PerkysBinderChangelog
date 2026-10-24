/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    SettingsManager implementation - settings stored within keybinder.json
*/
#include "../include/SettingsManager.h"

#include <cstdlib>
#include <sstream>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include "../include/Logger.h"
#include "../include/SpeedometerManager.h"

namespace keybinder {

namespace {

std::string TrimWhitespace(std::string value);

float ClampSignedNormalizedOffset(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }

    return (std::max)(-1.0f, (std::min)(value, 1.0f));
}

float ClampNumberScale(float value) {
    if (!std::isfinite(value)) {
        return 1.0f;
    }

    return (std::max)(0.5f, (std::min)(value, 3.0f));
}

void NormalizeLegacyVitalStatsNumbersPosition(float& posX, float& posY) {
    if (posX > 0.5f && posY >= -0.1f && posY <= 0.35f) {
        posX = 0.0f;
        posY = 0.0f;
    }

    posX = ClampSignedNormalizedOffset(posX);
    posY = ClampSignedNormalizedOffset(posY);
}

void NormalizeVitalStatsNumbersPosition(InterfacePreset& preset) {
    NormalizeLegacyVitalStatsNumbersPosition(preset.vital_stats_numbers_pos_x, preset.vital_stats_numbers_pos_y);
    NormalizeLegacyVitalStatsNumbersPosition(preset.hp_number_pos_x, preset.hp_number_pos_y);
    NormalizeLegacyVitalStatsNumbersPosition(preset.armor_number_pos_x, preset.armor_number_pos_y);
    preset.hp_number_scale = ClampNumberScale(preset.hp_number_scale);
    preset.armor_number_scale = ClampNumberScale(preset.armor_number_scale);
}

std::string EscapeJsonString(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    size_t pos = 0;
    while ((pos = value.find('\\', pos)) != std::string::npos) {
        value.insert(pos, "\\");
        pos += 2;
    }

    pos = 0;
    while ((pos = value.find('"', pos)) != std::string::npos) {
        value.insert(pos, "\\");
        pos += 2;
    }

    pos = 0;
    while ((pos = value.find('\n', pos)) != std::string::npos) {
        value.replace(pos, 1, "\\n");
        pos += 2;
    }

    return value;
}

std::string UnescapeJsonString(std::string value) {
    std::string result;
    result.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[i + 1];
            switch (next) {
            case 'n':
                result.push_back('\n');
                ++i;
                continue;
            case 'r':
                result.push_back('\r');
                ++i;
                continue;
            case 't':
                result.push_back('\t');
                ++i;
                continue;
            case '\\':
                result.push_back('\\');
                ++i;
                continue;
            case '"':
                result.push_back('"');
                ++i;
                continue;
            default:
                result.push_back(next);
                ++i;
                continue;
            }
        }

        result.push_back(value[i]);
    }

    return result;
}

std::string NormalizeHitmarkerImagePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');

    const std::string prefix = "hitmarkers";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        const char separator = path[prefix.size()];
        if (separator != '\\') {
            path.insert(prefix.size(), "\\");
        }
    }

    return path;
}

std::string NormalizeCrosshairImagePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');

    const std::string prefix = "crosshairs";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        const char separator = path[prefix.size()];
        if (separator != '\\') {
            path.insert(prefix.size(), "\\");
        }
    }

    return path;
}

std::string NormalizeFistImagePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');

    const std::string prefix = "fists";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        const char separator = path[prefix.size()];
        if (separator != '\\') {
            path.insert(prefix.size(), "\\");
        }
    }

    return path;
}

std::string NormalizeHitsoundPath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');

    const std::string prefix = "hitsounds";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        const char separator = path[prefix.size()];
        if (separator != '\\') {
            path.insert(prefix.size(), "\\");
        }
    }

    return path;
}

std::string NormalizeRadarPack(std::string pack) {
    std::transform(pack.begin(), pack.end(), pack.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (pack == "radar2" || pack == "2" || pack == "set2") {
        return "radar2";
    }

    return "radar1";
}

float NormalizeUnitFloat(float value, float fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }

    return (std::max)(0.0f, (std::min)(1.0f, value));
}

std::string TrimWhitespace(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::string NormalizeUiOpenCommand(std::string command) {
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

constexpr const char* kDefaultThemeAccent = "";
constexpr const char* kDefaultThemeName = "Dark";
constexpr float kMaxUiTransparency = 0.70f;

bool IsHexColorDigit(char ch) {
    return (ch >= '0' && ch <= '9')
        || (ch >= 'a' && ch <= 'f')
        || (ch >= 'A' && ch <= 'F');
}

std::string NormalizeAccentHexColor(std::string color, const std::string& fallback) {
    color = TrimWhitespace(std::move(color));
    if (color.size() != 7 || color.front() != '#') {
        return fallback;
    }

    for (std::size_t i = 1; i < color.size(); ++i) {
        if (!IsHexColorDigit(color[i])) {
            return fallback;
        }

        color[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(color[i])));
    }

    return color;
}

float NormalizeUiTransparency(float transparency) {
    if (!std::isfinite(transparency)) {
        return 0.0f;
    }
    return (std::max)(0.0f, (std::min)(kMaxUiTransparency, transparency));
}

std::string NormalizeThemeName(std::string themeName) {
    themeName = TrimWhitespace(std::move(themeName));
    std::transform(themeName.begin(), themeName.end(), themeName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (themeName == "classic" || themeName == "dark") {
        return "Dark";
    }

    if (themeName == "nova") {
        return "Nova";
    }

    if (themeName == "ember") {
        return "Ember";
    }

    if (themeName == "ocean") {
        return "Ocean";
    }

    if (themeName == "forest") {
        return "Forest";
    }

    return kDefaultThemeName;
}


int NormalizeUiToggleHotkey(int hotkey) {
    switch (hotkey) {
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
            return VK_F2;
        default:
            return hotkey;
    }
}

int NormalizeQuickMapHotkey(int hotkey) {
    switch (hotkey) {
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
            return keybinder::VK_M;
        default:
            return hotkey;
    }
}

std::string NormalizeQuickMapZoomLevel(const std::string& zoomLevel) {
    std::string normalized = TrimWhitespace(zoomLevel);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "far"
        || normalized == "medium"
        || normalized == "close"
        || normalized == "very_close"
        || normalized == "ultra") {
        return normalized;
    }

    return "far";
}

RssNotificationMode NormalizeRssNotificationMode(const std::string& mode) {
    std::string normalized = TrimWhitespace(mode);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "chat") {
        return RssNotificationMode::Chat;
    }

    return RssNotificationMode::Toast;
}

const char* ToJsonString(RssNotificationMode mode) {
    return mode == RssNotificationMode::Chat ? "chat" : "toast";
}

bool ParseBoolField(const std::string& object, const char* key, bool& outValue) {
    const size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = object.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t valuePos = object.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valuePos == std::string::npos) {
        return false;
    }

    if (object.compare(valuePos, 4, "true") == 0) {
        outValue = true;
        return true;
    }

    if (object.compare(valuePos, 5, "false") == 0) {
        outValue = false;
        return true;
    }

    return false;
}

bool ParseIntField(const std::string& object, const char* key, int& outValue) {
    const size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = object.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t numStart = object.find_first_of("0123456789-", colonPos + 1);
    if (numStart == std::string::npos) {
        return false;
    }

    const size_t numEnd = object.find_first_not_of("0123456789-", numStart);
    outValue = std::atoi(object.substr(numStart, numEnd - numStart).c_str());
    return true;
}

bool ParseFloatField(const std::string& object, const char* key, float& outValue) {
    const size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = object.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t numStart = object.find_first_of("0123456789.-", colonPos + 1);
    if (numStart == std::string::npos) {
        return false;
    }

    const size_t numEnd = object.find_first_not_of("0123456789.-", numStart);
    outValue = static_cast<float>(std::atof(object.substr(numStart, numEnd - numStart).c_str()));
    return true;
}

bool ParseStringField(const std::string& object, const char* key, std::string& outValue) {
    const size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = object.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t firstQuote = object.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return false;
    }

    size_t secondQuote = firstQuote + 1;
    while (secondQuote < object.size()) {
        secondQuote = object.find('"', secondQuote);
        if (secondQuote == std::string::npos) {
            return false;
        }
            size_t backslashCount = 0;
        size_t checkPos = secondQuote - 1;
        while (checkPos >= firstQuote + 1 && object[checkPos] == '\\') {
            ++backslashCount;
            if (checkPos == 0) break;
            --checkPos;
        }
            if (backslashCount % 2 == 0) {
                break;
            }
            ++secondQuote;
        }

    if (secondQuote == std::string::npos || secondQuote >= object.size()) {
        return false;
    }

    outValue = UnescapeJsonString(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
    return true;
}

bool ParseTimeField(const std::string& object, const char* key, std::time_t& outValue) {
    const size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = object.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t numStart = object.find_first_of("0123456789-", colonPos + 1);
    if (numStart == std::string::npos) {
        return false;
    }

    const size_t numEnd = object.find_first_not_of("0123456789-", numStart);
    outValue = static_cast<std::time_t>(std::atoll(object.substr(numStart, numEnd - numStart).c_str()));
    return true;
}

bool IsEscapedJsonCharacter(const std::string& text, size_t pos) {
    if (pos == 0 || pos >= text.size()) {
        return false;
    }

    size_t backslashCount = 0;
    size_t checkPos = pos;
    while (checkPos > 0 && text[checkPos - 1] == '\\') {
        ++backslashCount;
        --checkPos;
    }

    return (backslashCount % 2) != 0;
}

size_t FindMatchingDelimiter(const std::string& text, size_t openPos, char openDelimiter, char closeDelimiter) {
    if (openPos == std::string::npos || openPos >= text.size() || text[openPos] != openDelimiter) {
        return std::string::npos;
    }

    size_t depth = 0;
    bool inString = false;
    for (size_t i = openPos; i < text.size(); ++i) {
        const char ch = text[i];

        if (ch == '"' && !IsEscapedJsonCharacter(text, i)) {
            inString = !inString;
            continue;
        }

        if (inString) {
            continue;
        }

        if (ch == openDelimiter) {
            ++depth;
        } else if (ch == closeDelimiter) {
            if (depth == 0) {
                return std::string::npos;
            }

            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

bool ExtractJsonArrayField(const std::string& object, const char* key, std::string& outValue) {
    const size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = object.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t arrayStart = object.find('[', colonPos + 1);
    if (arrayStart == std::string::npos) {
        return false;
    }

    const size_t arrayEnd = FindMatchingDelimiter(object, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos) {
        return false;
    }

    outValue = object.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
    return true;
}


void NormalizeSavedHitmarkerPresets(std::vector<HitmarkerPreset>& presets) {
    if (presets.size() > 5) {
        presets.resize(5);
    }

    while (presets.size() < 5) {
        presets.emplace_back();
    }
}

void NormalizeSavedInterfacePresets(std::vector<InterfacePreset>& presets) {
    constexpr std::size_t kPresetSlotCount = static_cast<std::size_t>(InterfaceEditor::PRESET_SLOT_COUNT);

    if (presets.size() > kPresetSlotCount) {
        presets.resize(kPresetSlotCount);
    }

    while (presets.size() < kPresetSlotCount) {
        presets.emplace_back();
    }
}

std::string NormalizeOnlineOverlayMode(std::string mode) {
    std::string normalized = TrimWhitespace(std::move(mode));
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "toggle" || normalized == "hold") {
        return normalized;
    }

    return "toggle";
}

std::string NormalizeOnlineOverlayAnchor(std::string anchor) {
    std::string normalized = TrimWhitespace(std::move(anchor));
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "top_left" || normalized == "top_right"
        || normalized == "bottom_left" || normalized == "bottom_right"
        || normalized == "center") {
        return normalized;
    }

    return "top_right";
}

std::string NormalizeOnlineOverlayStyle(std::string style) {
    std::string normalized = TrimWhitespace(std::move(style));
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (normalized == "card" || normalized == "minimal" || normalized == "dense") {
        return normalized;
    }

    return "card";
}

float NormalizeOnlineOverlayOpacity(float opacity) {
    if (!std::isfinite(opacity)) {
        return 0.85f;
    }

    return (std::max)(0.35f, (std::min)(1.0f, opacity));
}

float NormalizeOnlineOverlayScale(float scale) {
    if (!std::isfinite(scale)) {
        return 1.0f;
    }

    return (std::max)(0.75f, (std::min)(1.50f, scale));
}

float NormalizeOnlineOverlayColor(float component, float fallback) {
    if (!std::isfinite(component)) {
        return fallback;
    }

    return (std::max)(0.0f, (std::min)(1.0f, component));
}

std::vector<std::string> DedupeGangSelection(std::vector<std::string> gangs) {
    std::vector<std::string> result;
    result.reserve(gangs.size());
    for (auto& gang : gangs) {
        std::string trimmed = TrimWhitespace(std::move(gang));
        if (trimmed.empty()) continue;
        bool found = false;
        for (const auto& existing : result) {
            if (existing == trimmed) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.push_back(std::move(trimmed));
        }
    }
    return result;
}

std::vector<OnlineEventPlaceholder> MakeDefaultEventPlaceholders() {
    std::vector<OnlineEventPlaceholder> placeholders;
    {
        OnlineEventPlaceholder ph;
        ph.event_type = "gangwar";
        ph.patterns = {"gangwar"};
        placeholders.push_back(std::move(ph));
    }
    {
        OnlineEventPlaceholder ph;
        ph.event_type = "revierkampf";
        ph.patterns = {"revierkampf"};
        placeholders.push_back(std::move(ph));
    }
    {
        OnlineEventPlaceholder ph;
        ph.event_type = "takeover";
        ph.patterns = {"takeover"};
        placeholders.push_back(std::move(ph));
    }
    return placeholders;
}

}

SettingsManager::SettingsManager()
    : settings_{}
{
    NormalizeSavedHitmarkerPresets(settings_.hitmarker_saved_presets);
    NormalizeSavedInterfacePresets(settings_.interface_editor_saved_presets);
    settings_.online_event_placeholders = MakeDefaultEventPlaceholders();
}

float SettingsManager::ClampScale(float scale) {
    if (!std::isfinite(scale)) {
        return 1.0f;
    }

    if (scale < 0.5f) {
        return 0.5f;
    }

    if (scale > 4.0f) {
        return 4.0f;
    }

    return scale;
}

void SettingsManager::SetDpiScale(float scale) {
    settings_.dpi_scale = ClampScale(scale);
}

bool SettingsManager::LoadFromJson(const std::string& json_content) {
    if (json_content.empty()) {
        Logger::Info("[SettingsManager] No settings in config; using defaults");
        return true;
    }

    float dpi_scale = settings_.dpi_scale;
    bool auto_detect_dpi = settings_.auto_detect_dpi;
    std::string ui_open_command_primary = settings_.ui_open_command_primary;
    std::string ui_open_command_secondary = settings_.ui_open_command_secondary;
    int ui_toggle_hotkey = settings_.ui_toggle_hotkey;
    float ui_window_width = settings_.ui_window_width;
    float ui_window_height = settings_.ui_window_height;
    bool kill_tracker_enabled = settings_.kill_tracker_enabled;
    bool kill_tracker_local_kill_message_enabled = settings_.kill_tracker_local_kill_message_enabled;
    bool kill_tracker_local_death_message_enabled = settings_.kill_tracker_local_death_message_enabled;
    bool kill_tracker_server_kill_message_enabled = settings_.kill_tracker_server_kill_message_enabled;
    bool kill_tracker_server_death_message_enabled = settings_.kill_tracker_server_death_message_enabled;
    bool kill_tracker_track_damage = settings_.kill_tracker_track_damage;
    std::string kill_tracker_local_kill_message_template = settings_.kill_tracker_local_kill_message_template;
    std::string kill_tracker_local_death_message_template = settings_.kill_tracker_local_death_message_template;
    std::string kill_tracker_server_kill_message_template = settings_.kill_tracker_server_kill_message_template;
    std::string kill_tracker_server_death_message_template = settings_.kill_tracker_server_death_message_template;
    bool kill_tracker_allow_manual_edits = settings_.kill_tracker_allow_manual_edits;
    bool auto_hudscalefix = settings_.auto_hudscalefix;
    bool auto_engine_start = settings_.auto_engine_start;
    bool damage_log_enabled = settings_.damage_log_enabled;
    bool quickmap_enabled = settings_.quickmap_enabled;
    int quickmap_hotkey = settings_.quickmap_hotkey;
    std::string quickmap_mode = settings_.quickmap_mode;
    std::string quickmap_map_variant = settings_.quickmap_map_variant;
    float quickmap_map_transparency = settings_.quickmap_map_transparency;
    bool quickmap_auto_zoom = settings_.quickmap_auto_zoom;
    bool quickmap_follow_player = settings_.quickmap_follow_player;
    bool quickmap_focus_marker = settings_.quickmap_focus_marker;
    std::string quickmap_zoom_level = settings_.quickmap_zoom_level;
    std::string quickmap_last_zoom_level = settings_.quickmap_last_zoom_level;
    float quickmap_gangzone_outline_thickness = settings_.quickmap_gangzone_outline_thickness;
    bool quickmap_gangzone_flash_enabled = settings_.quickmap_gangzone_flash_enabled;
    bool speedometer_enabled = settings_.speedometer_enabled;
    std::string speedometer_style = settings_.speedometer_style;
    bool hitmarker_enabled = settings_.hitmarker_enabled;
    int hitmarker_active_preset_index = settings_.hitmarker_active_preset_index;
    float hitmarker_size = settings_.hitmarker_size;
    float hitmarker_color_r = settings_.hitmarker_color_r;
    float hitmarker_color_g = settings_.hitmarker_color_g;
    float hitmarker_color_b = settings_.hitmarker_color_b;
    float hitmarker_color_a = settings_.hitmarker_color_a;
    float hitmarker_kill_color_r = settings_.hitmarker_kill_color_r;
    float hitmarker_kill_color_g = settings_.hitmarker_kill_color_g;
    float hitmarker_kill_color_b = settings_.hitmarker_kill_color_b;
    float hitmarker_kill_color_a = settings_.hitmarker_kill_color_a;
    float hitmarker_opacity = settings_.hitmarker_opacity;
    float hitmarker_duration = settings_.hitmarker_duration;
    bool hitmarker_fade_out = settings_.hitmarker_fade_out;
    int hitmarker_shape = settings_.hitmarker_shape;
    std::string hitmarker_custom_image_path = settings_.hitmarker_custom_image_path;
    int interface_editor_active_preset_index = settings_.interface_editor_active_preset_index;
    bool crosshair_enabled = settings_.crosshair_enabled;
    std::string crosshair_texture_path = settings_.crosshair_texture_path;
    bool squaremap_enabled = settings_.squaremap_enabled;
    std::string quickmap_radar_pack = settings_.quickmap_radar_pack;
    std::string fist_texture_path = settings_.fist_texture_path;
    bool hitsound_outgoing_enabled = settings_.hitsound_outgoing_enabled;
    std::string hitsound_outgoing_path = settings_.hitsound_outgoing_path;
    float hitsound_outgoing_volume = settings_.hitsound_outgoing_volume;
    bool hitsound_incoming_enabled = settings_.hitsound_incoming_enabled;
    std::string hitsound_incoming_path = settings_.hitsound_incoming_path;
    float hitsound_incoming_volume = settings_.hitsound_incoming_volume;
    std::string theme_name = kDefaultThemeName;
    std::string theme_custom_accent = settings_.theme_custom_accent;
    float ui_transparency = settings_.ui_transparency;
    bool found_theme_name = ParseStringField(json_content, "\"theme_name\"", theme_name);
    ParseStringField(json_content, "\"theme_custom_accent\"", theme_custom_accent);
    const bool found_legacy_theme_selected = json_content.find("\"theme_selected\"") != std::string::npos;
    const bool found_legacy_theme_accent_mode = json_content.find("\"theme_accent_mode\"") != std::string::npos;
    const bool found_legacy_theme_accent_preset = json_content.find("\"theme_accent_preset\"") != std::string::npos;
    bool rss_enabled = settings_.rss_enabled;
    std::string rss_notification_mode = ToJsonString(settings_.rss_notification_mode);

    // Online feature local variables
    bool online_enabled = settings_.online_enabled;
    bool online_quick_overlay_enabled = settings_.online_quick_overlay_enabled;
    int online_quick_overlay_hotkey = settings_.online_quick_overlay_hotkey;
    std::string online_quick_overlay_mode = settings_.online_quick_overlay_mode;
    bool online_quick_overlay_show_lspd = settings_.online_quick_overlay_show_lspd;
    bool online_quick_overlay_show_army = settings_.online_quick_overlay_show_army;
    bool online_quick_overlay_show_fire_department = settings_.online_quick_overlay_show_fire_department;
    bool online_quick_overlay_show_zivilisten = settings_.online_quick_overlay_show_zivilisten;
    bool online_quick_overlay_show_all_gangs = settings_.online_quick_overlay_show_all_gangs;
    std::vector<std::string> online_quick_overlay_selected_gangs;
    bool online_local_overlay_enabled = settings_.online_local_overlay_enabled;
    bool online_own_gang_overlay_enabled = settings_.online_own_gang_overlay_enabled;
    bool online_gangs_overlay_enabled = settings_.online_gangs_overlay_enabled;
    std::string online_overlay_anchor = settings_.online_overlay_anchor;
    std::string online_local_overlay_anchor = settings_.online_local_overlay_anchor;
    std::string online_own_gang_overlay_anchor = settings_.online_own_gang_overlay_anchor;
    std::string online_gangs_overlay_anchor = settings_.online_gangs_overlay_anchor;
    float online_overlay_offset_x = settings_.online_overlay_offset_x;
    float online_overlay_offset_y = settings_.online_overlay_offset_y;
    std::string online_overlay_style = settings_.online_overlay_style;
    float online_overlay_opacity = settings_.online_overlay_opacity;
    bool online_overlay_background_enabled = settings_.online_overlay_background_enabled;
    float online_overlay_scale = settings_.online_overlay_scale;
    bool online_overlay_custom_text_color_enabled = settings_.online_overlay_custom_text_color_enabled;
    float online_overlay_text_color_r = settings_.online_overlay_text_color_r;
    float online_overlay_text_color_g = settings_.online_overlay_text_color_g;
    float online_overlay_text_color_b = settings_.online_overlay_text_color_b;
    float online_overlay_text_color_a = settings_.online_overlay_text_color_a;
    float online_own_gang_overlay_offset_x = settings_.online_own_gang_overlay_offset_x;
    float online_own_gang_overlay_offset_y = settings_.online_own_gang_overlay_offset_y;
    std::string online_own_gang_overlay_style = settings_.online_own_gang_overlay_style;
    float online_own_gang_overlay_opacity = settings_.online_own_gang_overlay_opacity;
    bool online_own_gang_overlay_background_enabled = settings_.online_own_gang_overlay_background_enabled;
    float online_own_gang_overlay_scale = settings_.online_own_gang_overlay_scale;
    bool online_own_gang_overlay_custom_text_color_enabled = settings_.online_own_gang_overlay_custom_text_color_enabled;
    float online_own_gang_overlay_text_color_r = settings_.online_own_gang_overlay_text_color_r;
    float online_own_gang_overlay_text_color_g = settings_.online_own_gang_overlay_text_color_g;
    float online_own_gang_overlay_text_color_b = settings_.online_own_gang_overlay_text_color_b;
    float online_own_gang_overlay_text_color_a = settings_.online_own_gang_overlay_text_color_a;
    bool online_wanted_overlay_enabled = settings_.online_wanted_overlay_enabled;
    std::string online_wanted_overlay_anchor = settings_.online_wanted_overlay_anchor;
    float online_wanted_overlay_offset_x = settings_.online_wanted_overlay_offset_x;
    float online_wanted_overlay_offset_y = settings_.online_wanted_overlay_offset_y;
    std::string online_wanted_overlay_style = settings_.online_wanted_overlay_style;
    float online_wanted_overlay_opacity = settings_.online_wanted_overlay_opacity;
    bool online_wanted_overlay_background_enabled = settings_.online_wanted_overlay_background_enabled;
    float online_wanted_overlay_scale = settings_.online_wanted_overlay_scale;
    bool online_wanted_overlay_custom_text_color_enabled = settings_.online_wanted_overlay_custom_text_color_enabled;
    float online_wanted_overlay_text_color_r = settings_.online_wanted_overlay_text_color_r;
    float online_wanted_overlay_text_color_g = settings_.online_wanted_overlay_text_color_g;
    float online_wanted_overlay_text_color_b = settings_.online_wanted_overlay_text_color_b;
    float online_wanted_overlay_text_color_a = settings_.online_wanted_overlay_text_color_a;
    int online_wanted_overlay_min = settings_.online_wanted_overlay_min;
    int online_wanted_overlay_max = settings_.online_wanted_overlay_max;
    int online_event_detector_version = settings_.online_event_detector_version;

    bool found_dpi = false;
    bool found_auto = false;
    bool found_ui_open_command_primary = false;
    bool found_ui_open_command_secondary = false;
    bool found_ui_toggle_hotkey = false;
    bool found_ui_window_width = false;
    bool found_ui_window_height = false;
    bool found_kill_tracker = false;
    bool found_local_kill_message_enabled = false;
    bool found_local_death_message_enabled = false;
    bool found_server_kill_message_enabled = false;
    bool found_server_death_message_enabled = false;
    bool found_track_damage = false;
    bool found_local_kill_message_template = false;
    bool found_local_death_message_template = false;
    bool found_server_kill_message_template = false;
    bool found_server_death_message_template = false;
    bool found_manual_edits = false;
    bool found_auto_hudscalefix = false;
    bool found_auto_engine_start = false;
    bool found_damage_log_enabled = false;
    bool found_quickmap_enabled = false;
    bool found_quickmap_hotkey = false;
    bool found_quickmap_mode = false;
    bool found_quickmap_map_variant = false;
    bool found_quickmap_map_transparency = false;
    bool found_quickmap_auto_zoom = false;
    bool found_quickmap_follow_player = false;
    bool found_quickmap_focus_marker = false;
    bool found_quickmap_zoom_level = false;
    bool found_quickmap_last_zoom_level = false;
    bool found_quickmap_gangzone_outline_thickness = false;
    bool found_quickmap_gangzone_flash_enabled = false;
    bool found_speedometer_enabled = false;
    bool found_speedometer_style = false;
    bool found_hitmarker_enabled = false;
    bool found_hitmarker_active_preset_index = false;
    bool found_hitmarker_size = false;
    bool found_hitmarker_color_r = false;
    bool found_hitmarker_color_g = false;
    bool found_hitmarker_color_b = false;
    bool found_hitmarker_color_a = false;
    bool found_hitmarker_kill_color_r = false;
    bool found_hitmarker_kill_color_g = false;
    bool found_hitmarker_kill_color_b = false;
    bool found_hitmarker_kill_color_a = false;
    bool found_hitmarker_opacity = false;
    bool found_hitmarker_duration = false;
    bool found_hitmarker_fade_out = false;
    bool found_hitmarker_shape = false;
    bool found_hitmarker_custom_image_path = false;
    bool found_interface_editor_active_preset_index = false;
    bool found_interface_editor_saved_presets = false;
    bool found_crosshair_enabled = false;
    bool found_crosshair_texture_path = false;
    bool found_squaremap_enabled = false;
    bool found_quickmap_radar_pack = false;
    bool found_fist_texture_path = false;
    bool found_hitsound_outgoing_enabled = false;
    bool found_hitsound_outgoing_path = false;
    bool found_hitsound_outgoing_volume = false;
    bool found_hitsound_incoming_enabled = false;
    bool found_hitsound_incoming_path = false;
    bool found_hitsound_incoming_volume = false;
    bool found_ui_transparency = false;
    bool found_rss_enabled = false;
    bool found_rss_notification_mode = false;
    bool found_rss_feeds = false;

    // Online feature found_ flags
    bool found_online_enabled = false;
    bool found_online_quick_overlay_enabled = false;
    bool found_online_quick_overlay_hotkey = false;
    bool found_online_quick_overlay_mode = false;
    bool found_online_quick_overlay_show_lspd = false;
    bool found_online_quick_overlay_show_army = false;
    bool found_online_quick_overlay_show_fire_department = false;
    bool found_online_quick_overlay_show_zivilisten = false;
    bool found_online_quick_overlay_show_all_gangs = false;
    bool found_online_quick_overlay_selected_gangs = false;
    bool found_online_local_overlay_enabled = false;
    bool found_online_own_gang_overlay_enabled = false;
    bool found_online_gangs_overlay_enabled = false;
    bool found_online_overlay_anchor = false;
    bool found_online_local_overlay_anchor = false;
    bool found_online_own_gang_overlay_anchor = false;
    bool found_online_gangs_overlay_anchor = false;
    bool found_online_overlay_offset_x = false;
    bool found_online_overlay_offset_y = false;
    bool found_online_overlay_style = false;
    bool found_online_overlay_opacity = false;
    bool found_online_overlay_background_enabled = false;
    bool found_online_overlay_scale = false;
    bool found_online_overlay_custom_text_color_enabled = false;
    bool found_online_overlay_text_color_r = false;
    bool found_online_overlay_text_color_g = false;
    bool found_online_overlay_text_color_b = false;
    bool found_online_overlay_text_color_a = false;
    bool found_online_own_gang_overlay_offset_x = false;
    bool found_online_own_gang_overlay_offset_y = false;
    bool found_online_own_gang_overlay_style = false;
    bool found_online_own_gang_overlay_opacity = false;
    bool found_online_own_gang_overlay_background_enabled = false;
    bool found_online_own_gang_overlay_scale = false;
    bool found_online_own_gang_overlay_custom_text_color_enabled = false;
    bool found_online_own_gang_overlay_text_color_r = false;
    bool found_online_own_gang_overlay_text_color_g = false;
    bool found_online_own_gang_overlay_text_color_b = false;
    bool found_online_own_gang_overlay_text_color_a = false;
    bool found_online_wanted_overlay_enabled = false;
    bool found_online_wanted_overlay_anchor = false;
    bool found_online_wanted_overlay_offset_x = false;
    bool found_online_wanted_overlay_offset_y = false;
    bool found_online_wanted_overlay_style = false;
    bool found_online_wanted_overlay_opacity = false;
    bool found_online_wanted_overlay_background_enabled = false;
    bool found_online_wanted_overlay_scale = false;
    bool found_online_wanted_overlay_custom_text_color_enabled = false;
    bool found_online_wanted_overlay_text_color_r = false;
    bool found_online_wanted_overlay_text_color_g = false;
    bool found_online_wanted_overlay_text_color_b = false;
    bool found_online_wanted_overlay_text_color_a = false;
    bool found_online_wanted_overlay_min = false;
    bool found_online_wanted_overlay_max = false;
    bool found_online_event_detector_version = false;

    found_dpi = ParseFloatField(json_content, "\"dpi_scale\"", dpi_scale);
    found_auto = ParseBoolField(json_content, "\"auto_detect_dpi\"", auto_detect_dpi);

    found_ui_open_command_primary = ParseStringField(json_content, "\"ui_open_command_primary\"", ui_open_command_primary);
    found_ui_open_command_secondary = ParseStringField(json_content, "\"ui_open_command_secondary\"", ui_open_command_secondary);
    found_ui_toggle_hotkey = ParseIntField(json_content, "\"ui_toggle_hotkey\"", ui_toggle_hotkey);
    found_ui_window_width = ParseFloatField(json_content, "\"ui_window_width\"", ui_window_width);
    found_ui_window_height = ParseFloatField(json_content, "\"ui_window_height\"", ui_window_height);
    found_ui_transparency = ParseFloatField(json_content, "\"ui_transparency\"", ui_transparency);

    found_kill_tracker = ParseBoolField(json_content, "\"kill_tracker_enabled\"", kill_tracker_enabled);
    found_local_kill_message_enabled = ParseBoolField(json_content, "\"kill_tracker_local_kill_message_enabled\"", kill_tracker_local_kill_message_enabled);
    found_local_death_message_enabled = ParseBoolField(json_content, "\"kill_tracker_local_death_message_enabled\"", kill_tracker_local_death_message_enabled);
    if (!found_local_kill_message_enabled || !found_local_death_message_enabled) {
        bool legacyLocalMessageEnabled = kill_tracker_local_kill_message_enabled;
        if (ParseBoolField(json_content, "\"kill_tracker_local_message_enabled\"", legacyLocalMessageEnabled) || ParseBoolField(json_content, "\"kill_tracker_show_messages\"", legacyLocalMessageEnabled)) {
            if (!found_local_kill_message_enabled) {
                kill_tracker_local_kill_message_enabled = legacyLocalMessageEnabled;
                found_local_kill_message_enabled = true;
            }
            if (!found_local_death_message_enabled) {
                kill_tracker_local_death_message_enabled = legacyLocalMessageEnabled;
                found_local_death_message_enabled = true;
            }
        }
    }
    found_server_kill_message_enabled = ParseBoolField(json_content, "\"kill_tracker_server_kill_message_enabled\"", kill_tracker_server_kill_message_enabled);
    found_server_death_message_enabled = ParseBoolField(json_content, "\"kill_tracker_server_death_message_enabled\"", kill_tracker_server_death_message_enabled);
    if (!found_server_kill_message_enabled) {
        found_server_kill_message_enabled = ParseBoolField(json_content, "\"kill_tracker_server_message_enabled\"", kill_tracker_server_kill_message_enabled);
    }
    found_track_damage = ParseBoolField(json_content, "\"kill_tracker_track_damage\"", kill_tracker_track_damage);
    found_local_kill_message_template = ParseStringField(json_content, "\"kill_tracker_local_kill_message_template\"", kill_tracker_local_kill_message_template);
    if (!found_local_kill_message_template) {
        found_local_kill_message_template = ParseStringField(json_content, "\"kill_tracker_local_message_template\"", kill_tracker_local_kill_message_template);
    }
    if (!found_local_kill_message_template) {
        found_local_kill_message_template = ParseStringField(json_content, "\"kill_tracker_message_template\"", kill_tracker_local_kill_message_template);
    }
    found_local_death_message_template = ParseStringField(json_content, "\"kill_tracker_local_death_message_template\"", kill_tracker_local_death_message_template);
    if (!found_local_death_message_template) {
        found_local_death_message_template = ParseStringField(json_content, "\"kill_tracker_death_message_template\"", kill_tracker_local_death_message_template);
    }
    found_server_kill_message_template = ParseStringField(json_content, "\"kill_tracker_server_kill_message_template\"", kill_tracker_server_kill_message_template);
    if (!found_server_kill_message_template) {
        found_server_kill_message_template = ParseStringField(json_content, "\"kill_tracker_server_message_template\"", kill_tracker_server_kill_message_template);
    }
    found_server_death_message_template = ParseStringField(json_content, "\"kill_tracker_server_death_message_template\"", kill_tracker_server_death_message_template);
    found_manual_edits = ParseBoolField(json_content, "\"kill_tracker_allow_manual_edits\"", kill_tracker_allow_manual_edits);
    found_auto_hudscalefix = ParseBoolField(json_content, "\"auto_hudscalefix\"", auto_hudscalefix);
    found_auto_engine_start = ParseBoolField(json_content, "\"auto_engine_start\"", auto_engine_start);
    found_damage_log_enabled = ParseBoolField(json_content, "\"damage_log_enabled\"", damage_log_enabled);
    found_quickmap_enabled = ParseBoolField(json_content, "\"quickmap_enabled\"", quickmap_enabled);
    found_quickmap_hotkey = ParseIntField(json_content, "\"quickmap_hotkey\"", quickmap_hotkey);
    found_quickmap_mode = ParseStringField(json_content, "\"quickmap_mode\"", quickmap_mode);
    found_quickmap_map_variant = ParseStringField(json_content, "\"quickmap_map_variant\"", quickmap_map_variant);
    found_quickmap_map_transparency = ParseFloatField(json_content, "\"quickmap_map_transparency\"", quickmap_map_transparency);
    found_quickmap_auto_zoom = ParseBoolField(json_content, "\"quickmap_auto_zoom\"", quickmap_auto_zoom);
    found_quickmap_follow_player = ParseBoolField(json_content, "\"quickmap_follow_player\"", quickmap_follow_player);
    found_quickmap_focus_marker = ParseBoolField(json_content, "\"quickmap_focus_marker\"", quickmap_focus_marker);
    found_quickmap_zoom_level = ParseStringField(json_content, "\"quickmap_zoom_level\"", quickmap_zoom_level);
    found_quickmap_last_zoom_level = ParseStringField(json_content, "\"quickmap_last_zoom_level\"", quickmap_last_zoom_level);
    found_quickmap_gangzone_outline_thickness = ParseFloatField(json_content, "\"quickmap_gangzone_outline_thickness\"", quickmap_gangzone_outline_thickness);
    found_quickmap_gangzone_flash_enabled = ParseBoolField(json_content, "\"quickmap_gangzone_flash_enabled\"", quickmap_gangzone_flash_enabled);
    found_speedometer_enabled = ParseBoolField(json_content, "\"speedometer_enabled\"", speedometer_enabled);
    found_speedometer_style = ParseStringField(json_content, "\"speedometer_style\"", speedometer_style);
    found_hitmarker_enabled = ParseBoolField(json_content, "\"hitmarker_enabled\"", hitmarker_enabled);
    found_hitmarker_active_preset_index = ParseIntField(json_content, "\"hitmarker_active_preset_index\"", hitmarker_active_preset_index);
    found_hitmarker_size = ParseFloatField(json_content, "\"hitmarker_size\"", hitmarker_size);
    found_hitmarker_color_r = ParseFloatField(json_content, "\"hitmarker_color_r\"", hitmarker_color_r);
    found_hitmarker_color_g = ParseFloatField(json_content, "\"hitmarker_color_g\"", hitmarker_color_g);
    found_hitmarker_color_b = ParseFloatField(json_content, "\"hitmarker_color_b\"", hitmarker_color_b);
    found_hitmarker_color_a = ParseFloatField(json_content, "\"hitmarker_color_a\"", hitmarker_color_a);
    found_hitmarker_kill_color_r = ParseFloatField(json_content, "\"hitmarker_kill_color_r\"", hitmarker_kill_color_r);
    found_hitmarker_kill_color_g = ParseFloatField(json_content, "\"hitmarker_kill_color_g\"", hitmarker_kill_color_g);
    found_hitmarker_kill_color_b = ParseFloatField(json_content, "\"hitmarker_kill_color_b\"", hitmarker_kill_color_b);
    found_hitmarker_kill_color_a = ParseFloatField(json_content, "\"hitmarker_kill_color_a\"", hitmarker_kill_color_a);
    found_hitmarker_opacity = ParseFloatField(json_content, "\"hitmarker_opacity\"", hitmarker_opacity);
    found_hitmarker_duration = ParseFloatField(json_content, "\"hitmarker_duration\"", hitmarker_duration);
    found_hitmarker_fade_out = ParseBoolField(json_content, "\"hitmarker_fade_out\"", hitmarker_fade_out);
    found_hitmarker_shape = ParseIntField(json_content, "\"hitmarker_shape\"", hitmarker_shape);
    found_hitmarker_custom_image_path = ParseStringField(json_content, "\"hitmarker_custom_image_path\"", hitmarker_custom_image_path);
    found_interface_editor_active_preset_index = ParseIntField(json_content, "\"interface_editor_active_preset_index\"", interface_editor_active_preset_index);
    found_crosshair_enabled = ParseBoolField(json_content, "\"crosshair_enabled\"", crosshair_enabled);
    found_crosshair_texture_path = ParseStringField(json_content, "\"crosshair_texture_path\"", crosshair_texture_path);
    found_squaremap_enabled = ParseBoolField(json_content, "\"squaremap_enabled\"", squaremap_enabled);
    found_quickmap_radar_pack = ParseStringField(json_content, "\"quickmap_radar_pack\"", quickmap_radar_pack);
    found_fist_texture_path = ParseStringField(json_content, "\"fist_texture_path\"", fist_texture_path);
    found_hitsound_outgoing_enabled = ParseBoolField(json_content, "\"hitsound_outgoing_enabled\"", hitsound_outgoing_enabled);
    found_hitsound_outgoing_path = ParseStringField(json_content, "\"hitsound_outgoing_path\"", hitsound_outgoing_path);
    found_hitsound_outgoing_volume = ParseFloatField(json_content, "\"hitsound_outgoing_volume\"", hitsound_outgoing_volume);
    found_hitsound_incoming_enabled = ParseBoolField(json_content, "\"hitsound_incoming_enabled\"", hitsound_incoming_enabled);
    found_hitsound_incoming_path = ParseStringField(json_content, "\"hitsound_incoming_path\"", hitsound_incoming_path);
    found_hitsound_incoming_volume = ParseFloatField(json_content, "\"hitsound_incoming_volume\"", hitsound_incoming_volume);
    found_rss_enabled = ParseBoolField(json_content, "\"rss_enabled\"", rss_enabled);
    found_rss_notification_mode = ParseStringField(json_content, "\"rss_notification_mode\"", rss_notification_mode);

    // Online feature parsing
    found_online_enabled = ParseBoolField(json_content, "\"online_enabled\"", online_enabled);
    found_online_quick_overlay_enabled = ParseBoolField(json_content, "\"online_quick_overlay_enabled\"", online_quick_overlay_enabled);
    found_online_quick_overlay_hotkey = ParseIntField(json_content, "\"online_quick_overlay_hotkey\"", online_quick_overlay_hotkey);
    found_online_quick_overlay_mode = ParseStringField(json_content, "\"online_quick_overlay_mode\"", online_quick_overlay_mode);
    found_online_quick_overlay_show_lspd = ParseBoolField(json_content, "\"online_quick_overlay_show_lspd\"", online_quick_overlay_show_lspd);
    found_online_quick_overlay_show_army = ParseBoolField(json_content, "\"online_quick_overlay_show_army\"", online_quick_overlay_show_army);
    found_online_quick_overlay_show_fire_department = ParseBoolField(json_content, "\"online_quick_overlay_show_fire_department\"", online_quick_overlay_show_fire_department);
    found_online_quick_overlay_show_zivilisten = ParseBoolField(json_content, "\"online_quick_overlay_show_zivilisten\"", online_quick_overlay_show_zivilisten);
    found_online_quick_overlay_show_all_gangs = ParseBoolField(json_content, "\"online_quick_overlay_show_all_gangs\"", online_quick_overlay_show_all_gangs);
    found_online_local_overlay_enabled = ParseBoolField(json_content, "\"online_local_overlay_enabled\"", online_local_overlay_enabled);
    found_online_own_gang_overlay_enabled = ParseBoolField(json_content, "\"online_own_gang_overlay_enabled\"", online_own_gang_overlay_enabled);
    found_online_gangs_overlay_enabled = ParseBoolField(json_content, "\"online_gangs_overlay_enabled\"", online_gangs_overlay_enabled);
    found_online_overlay_anchor = ParseStringField(json_content, "\"online_overlay_anchor\"", online_overlay_anchor);
    found_online_local_overlay_anchor = ParseStringField(json_content, "\"online_local_overlay_anchor\"", online_local_overlay_anchor);
    found_online_own_gang_overlay_anchor = ParseStringField(json_content, "\"online_own_gang_overlay_anchor\"", online_own_gang_overlay_anchor);
    found_online_gangs_overlay_anchor = ParseStringField(json_content, "\"online_gangs_overlay_anchor\"", online_gangs_overlay_anchor);
    found_online_overlay_offset_x = ParseFloatField(json_content, "\"online_overlay_offset_x\"", online_overlay_offset_x);
    found_online_overlay_offset_y = ParseFloatField(json_content, "\"online_overlay_offset_y\"", online_overlay_offset_y);
    found_online_overlay_style = ParseStringField(json_content, "\"online_overlay_style\"", online_overlay_style);
    found_online_overlay_opacity = ParseFloatField(json_content, "\"online_overlay_opacity\"", online_overlay_opacity);
    found_online_overlay_background_enabled = ParseBoolField(json_content, "\"online_overlay_background_enabled\"", online_overlay_background_enabled);
    found_online_overlay_scale = ParseFloatField(json_content, "\"online_overlay_scale\"", online_overlay_scale);
    found_online_overlay_custom_text_color_enabled = ParseBoolField(json_content, "\"online_overlay_custom_text_color_enabled\"", online_overlay_custom_text_color_enabled);
    found_online_overlay_text_color_r = ParseFloatField(json_content, "\"online_overlay_text_color_r\"", online_overlay_text_color_r);
    found_online_overlay_text_color_g = ParseFloatField(json_content, "\"online_overlay_text_color_g\"", online_overlay_text_color_g);
    found_online_overlay_text_color_b = ParseFloatField(json_content, "\"online_overlay_text_color_b\"", online_overlay_text_color_b);
    found_online_overlay_text_color_a = ParseFloatField(json_content, "\"online_overlay_text_color_a\"", online_overlay_text_color_a);
    found_online_own_gang_overlay_offset_x = ParseFloatField(json_content, "\"online_own_gang_overlay_offset_x\"", online_own_gang_overlay_offset_x);
    found_online_own_gang_overlay_offset_y = ParseFloatField(json_content, "\"online_own_gang_overlay_offset_y\"", online_own_gang_overlay_offset_y);
    found_online_own_gang_overlay_style = ParseStringField(json_content, "\"online_own_gang_overlay_style\"", online_own_gang_overlay_style);
    found_online_own_gang_overlay_opacity = ParseFloatField(json_content, "\"online_own_gang_overlay_opacity\"", online_own_gang_overlay_opacity);
    found_online_own_gang_overlay_background_enabled = ParseBoolField(json_content, "\"online_own_gang_overlay_background_enabled\"", online_own_gang_overlay_background_enabled);
    found_online_own_gang_overlay_scale = ParseFloatField(json_content, "\"online_own_gang_overlay_scale\"", online_own_gang_overlay_scale);
    found_online_own_gang_overlay_custom_text_color_enabled = ParseBoolField(json_content, "\"online_own_gang_overlay_custom_text_color_enabled\"", online_own_gang_overlay_custom_text_color_enabled);
    found_online_own_gang_overlay_text_color_r = ParseFloatField(json_content, "\"online_own_gang_overlay_text_color_r\"", online_own_gang_overlay_text_color_r);
    found_online_own_gang_overlay_text_color_g = ParseFloatField(json_content, "\"online_own_gang_overlay_text_color_g\"", online_own_gang_overlay_text_color_g);
    found_online_own_gang_overlay_text_color_b = ParseFloatField(json_content, "\"online_own_gang_overlay_text_color_b\"", online_own_gang_overlay_text_color_b);
    found_online_own_gang_overlay_text_color_a = ParseFloatField(json_content, "\"online_own_gang_overlay_text_color_a\"", online_own_gang_overlay_text_color_a);
    found_online_wanted_overlay_enabled = ParseBoolField(json_content, "\"online_wanted_overlay_enabled\"", online_wanted_overlay_enabled);
    found_online_wanted_overlay_anchor = ParseStringField(json_content, "\"online_wanted_overlay_anchor\"", online_wanted_overlay_anchor);
    found_online_wanted_overlay_offset_x = ParseFloatField(json_content, "\"online_wanted_overlay_offset_x\"", online_wanted_overlay_offset_x);
    found_online_wanted_overlay_offset_y = ParseFloatField(json_content, "\"online_wanted_overlay_offset_y\"", online_wanted_overlay_offset_y);
    found_online_wanted_overlay_style = ParseStringField(json_content, "\"online_wanted_overlay_style\"", online_wanted_overlay_style);
    found_online_wanted_overlay_opacity = ParseFloatField(json_content, "\"online_wanted_overlay_opacity\"", online_wanted_overlay_opacity);
    found_online_wanted_overlay_background_enabled = ParseBoolField(json_content, "\"online_wanted_overlay_background_enabled\"", online_wanted_overlay_background_enabled);
    found_online_wanted_overlay_scale = ParseFloatField(json_content, "\"online_wanted_overlay_scale\"", online_wanted_overlay_scale);
    found_online_wanted_overlay_custom_text_color_enabled = ParseBoolField(json_content, "\"online_wanted_overlay_custom_text_color_enabled\"", online_wanted_overlay_custom_text_color_enabled);
    found_online_wanted_overlay_text_color_r = ParseFloatField(json_content, "\"online_wanted_overlay_text_color_r\"", online_wanted_overlay_text_color_r);
    found_online_wanted_overlay_text_color_g = ParseFloatField(json_content, "\"online_wanted_overlay_text_color_g\"", online_wanted_overlay_text_color_g);
    found_online_wanted_overlay_text_color_b = ParseFloatField(json_content, "\"online_wanted_overlay_text_color_b\"", online_wanted_overlay_text_color_b);
    found_online_wanted_overlay_text_color_a = ParseFloatField(json_content, "\"online_wanted_overlay_text_color_a\"", online_wanted_overlay_text_color_a);
    found_online_wanted_overlay_min = ParseIntField(json_content, "\"online_wanted_overlay_min\"", online_wanted_overlay_min);
    found_online_wanted_overlay_max = ParseIntField(json_content, "\"online_wanted_overlay_max\"", online_wanted_overlay_max);
    found_online_event_detector_version = ParseIntField(json_content, "\"online_event_detector_version\"", online_event_detector_version);

    settings_.dpi_scale = ClampScale(dpi_scale);
    settings_.auto_detect_dpi = auto_detect_dpi;
    settings_.ui_open_command_primary = NormalizeUiOpenCommand(ui_open_command_primary);
    if (settings_.ui_open_command_primary.empty()) {
        settings_.ui_open_command_primary = "kb";
    }

    settings_.ui_open_command_secondary = NormalizeUiOpenCommand(ui_open_command_secondary);
    if (settings_.ui_open_command_secondary == settings_.ui_open_command_primary) {
        settings_.ui_open_command_secondary.clear();
    }

    settings_.ui_toggle_hotkey = NormalizeUiToggleHotkey(ui_toggle_hotkey);
    settings_.ui_window_width = found_ui_window_width && std::isfinite(ui_window_width) && ui_window_width > 0.0f
        ? ui_window_width
        : 0.0f;
    settings_.ui_window_height = found_ui_window_height && std::isfinite(ui_window_height) && ui_window_height > 0.0f
        ? ui_window_height
        : 0.0f;
    settings_.kill_tracker_enabled = kill_tracker_enabled;
    ParseBoolField(json_content, "\"kill_tracker_count_kills\"", settings_.kill_tracker_count_kills);
    ParseBoolField(json_content, "\"kill_tracker_count_deaths\"", settings_.kill_tracker_count_deaths);
    settings_.kill_tracker_local_kill_message_enabled = kill_tracker_local_kill_message_enabled;
    settings_.kill_tracker_local_death_message_enabled = kill_tracker_local_death_message_enabled;
    settings_.kill_tracker_server_kill_message_enabled = kill_tracker_server_kill_message_enabled;
    settings_.kill_tracker_server_death_message_enabled = kill_tracker_server_death_message_enabled;
    settings_.kill_tracker_track_damage = kill_tracker_track_damage;
    settings_.kill_tracker_local_kill_message_template = kill_tracker_local_kill_message_template;
    settings_.kill_tracker_local_death_message_template = kill_tracker_local_death_message_template;
    settings_.kill_tracker_server_kill_message_template = kill_tracker_server_kill_message_template;
    settings_.kill_tracker_server_death_message_template = kill_tracker_server_death_message_template;
    settings_.kill_tracker_allow_manual_edits = kill_tracker_allow_manual_edits;
    settings_.auto_hudscalefix = auto_hudscalefix;
    settings_.auto_engine_start = auto_engine_start;
    settings_.damage_log_enabled = damage_log_enabled;
    settings_.quickmap_enabled = quickmap_enabled;
    settings_.quickmap_hotkey = NormalizeQuickMapHotkey(quickmap_hotkey);
    settings_.quickmap_mode = quickmap_mode;
    settings_.quickmap_map_variant = quickmap_map_variant == "satellite" ? "satellite" : "classic";
    settings_.quickmap_map_transparency = std::isfinite(quickmap_map_transparency)
        ? (std::max)(0.0f, (std::min)(1.0f, quickmap_map_transparency))
        : 1.0f;
    settings_.quickmap_auto_zoom = found_quickmap_auto_zoom ? quickmap_auto_zoom : settings_.quickmap_auto_zoom;
    settings_.quickmap_follow_player = found_quickmap_follow_player ? quickmap_follow_player : settings_.quickmap_follow_player;
    settings_.quickmap_focus_marker = found_quickmap_focus_marker ? quickmap_focus_marker : settings_.quickmap_focus_marker;
    const std::string normalizedQuickMapZoomLevel = found_quickmap_zoom_level
        ? NormalizeQuickMapZoomLevel(quickmap_zoom_level)
        : NormalizeQuickMapZoomLevel(settings_.quickmap_zoom_level);
    settings_.quickmap_zoom_level = normalizedQuickMapZoomLevel;
    settings_.quickmap_last_zoom_level = found_quickmap_last_zoom_level
        ? NormalizeQuickMapZoomLevel(quickmap_last_zoom_level)
        : normalizedQuickMapZoomLevel;
    settings_.quickmap_gangzone_outline_thickness = found_quickmap_gangzone_outline_thickness && std::isfinite(quickmap_gangzone_outline_thickness)
        ? (std::max)(1.0f, (std::min)(6.0f, quickmap_gangzone_outline_thickness))
        : settings_.quickmap_gangzone_outline_thickness;
    settings_.quickmap_gangzone_flash_enabled = found_quickmap_gangzone_flash_enabled ? quickmap_gangzone_flash_enabled : settings_.quickmap_gangzone_flash_enabled;
    settings_.speedometer_enabled = speedometer::kCustomTachoAvailable && (found_speedometer_enabled ? speedometer_enabled : settings_.speedometer_enabled);
    settings_.speedometer_style = found_speedometer_style
        ? speedometer::NormalizeSpeedometerStyleId(speedometer_style)
        : speedometer::NormalizeSpeedometerStyleId(settings_.speedometer_style);
    settings_.hitmarker_enabled = hitmarker_enabled;
    settings_.hitmarker_active_preset_index = hitmarker_active_preset_index;
    settings_.hitmarker_size = hitmarker_size;
    settings_.hitmarker_color_r = hitmarker_color_r;
    settings_.hitmarker_color_g = hitmarker_color_g;
    settings_.hitmarker_color_b = hitmarker_color_b;
    settings_.hitmarker_color_a = hitmarker_color_a;
    settings_.hitmarker_kill_color_r = hitmarker_kill_color_r;
    settings_.hitmarker_kill_color_g = hitmarker_kill_color_g;
    settings_.hitmarker_kill_color_b = hitmarker_kill_color_b;
    settings_.hitmarker_kill_color_a = hitmarker_kill_color_a;
    settings_.hitmarker_opacity = hitmarker_opacity;
    settings_.hitmarker_duration = hitmarker_duration;
    settings_.hitmarker_fade_out = hitmarker_fade_out;
    settings_.hitmarker_shape = hitmarker_shape;
    settings_.hitmarker_custom_image_path = NormalizeHitmarkerImagePath(hitmarker_custom_image_path);
    settings_.interface_editor_active_preset_index = interface_editor_active_preset_index;
    settings_.crosshair_enabled = crosshair_enabled;
    settings_.crosshair_texture_path = NormalizeCrosshairImagePath(crosshair_texture_path);
    settings_.squaremap_enabled = squaremap_enabled;
    settings_.quickmap_radar_pack = found_quickmap_radar_pack
        ? NormalizeRadarPack(quickmap_radar_pack)
        : NormalizeRadarPack(settings_.quickmap_radar_pack);
    settings_.fist_texture_path = NormalizeFistImagePath(fist_texture_path);
    settings_.hitsound_outgoing_enabled = hitsound_outgoing_enabled;
    settings_.hitsound_outgoing_path = NormalizeHitsoundPath(hitsound_outgoing_path);
    settings_.hitsound_outgoing_volume = found_hitsound_outgoing_volume
        ? NormalizeUnitFloat(hitsound_outgoing_volume, 0.08f)
        : 0.08f;
    settings_.hitsound_incoming_enabled = hitsound_incoming_enabled;
    settings_.hitsound_incoming_path = NormalizeHitsoundPath(hitsound_incoming_path);
    settings_.hitsound_incoming_volume = found_hitsound_incoming_volume
        ? NormalizeUnitFloat(hitsound_incoming_volume, 0.08f)
        : 0.08f;
    settings_.rss_enabled = rss_enabled;
    settings_.rss_notification_mode = NormalizeRssNotificationMode(rss_notification_mode);

    // Online feature assignments with validation
    settings_.online_enabled = online_enabled;
    settings_.online_quick_overlay_enabled = online_quick_overlay_enabled;
    settings_.online_quick_overlay_hotkey = online_quick_overlay_hotkey;
    settings_.online_quick_overlay_mode = NormalizeOnlineOverlayMode(online_quick_overlay_mode);
    settings_.online_quick_overlay_show_lspd = online_quick_overlay_show_lspd;
    settings_.online_quick_overlay_show_army = online_quick_overlay_show_army;
    settings_.online_quick_overlay_show_fire_department = online_quick_overlay_show_fire_department;
    settings_.online_quick_overlay_show_zivilisten = online_quick_overlay_show_zivilisten;
    settings_.online_quick_overlay_show_all_gangs = online_quick_overlay_show_all_gangs;
    settings_.online_local_overlay_enabled = online_local_overlay_enabled;
    settings_.online_own_gang_overlay_enabled = online_own_gang_overlay_enabled;
    settings_.online_gangs_overlay_enabled = online_gangs_overlay_enabled;
    settings_.online_overlay_anchor = NormalizeOnlineOverlayAnchor(online_overlay_anchor);
    settings_.online_local_overlay_anchor = NormalizeOnlineOverlayAnchor(
        found_online_local_overlay_anchor ? online_local_overlay_anchor : settings_.online_overlay_anchor);
    settings_.online_own_gang_overlay_anchor = NormalizeOnlineOverlayAnchor(
        found_online_own_gang_overlay_anchor ? online_own_gang_overlay_anchor : settings_.online_overlay_anchor);
    settings_.online_gangs_overlay_anchor = NormalizeOnlineOverlayAnchor(
        found_online_gangs_overlay_anchor ? online_gangs_overlay_anchor : settings_.online_overlay_anchor);
    settings_.online_overlay_offset_x = found_online_overlay_offset_x && std::isfinite(online_overlay_offset_x)
        ? online_overlay_offset_x
        : 12.0f;
    settings_.online_overlay_offset_y = found_online_overlay_offset_y && std::isfinite(online_overlay_offset_y)
        ? online_overlay_offset_y
        : 12.0f;
    settings_.online_overlay_style = NormalizeOnlineOverlayStyle(online_overlay_style);
    settings_.online_overlay_opacity = NormalizeOnlineOverlayOpacity(online_overlay_opacity);
    settings_.online_overlay_background_enabled = online_overlay_background_enabled;
    settings_.online_overlay_scale = NormalizeOnlineOverlayScale(online_overlay_scale);
    settings_.online_overlay_custom_text_color_enabled = online_overlay_custom_text_color_enabled;
    settings_.online_overlay_text_color_r = NormalizeOnlineOverlayColor(online_overlay_text_color_r, 1.0f);
    settings_.online_overlay_text_color_g = NormalizeOnlineOverlayColor(online_overlay_text_color_g, 1.0f);
    settings_.online_overlay_text_color_b = NormalizeOnlineOverlayColor(online_overlay_text_color_b, 1.0f);
    settings_.online_overlay_text_color_a = NormalizeOnlineOverlayColor(online_overlay_text_color_a, 1.0f);
    settings_.online_own_gang_overlay_offset_x = found_online_own_gang_overlay_offset_x && std::isfinite(online_own_gang_overlay_offset_x)
        ? online_own_gang_overlay_offset_x
        : 12.0f;
    settings_.online_own_gang_overlay_offset_y = found_online_own_gang_overlay_offset_y && std::isfinite(online_own_gang_overlay_offset_y)
        ? online_own_gang_overlay_offset_y
        : 12.0f;
    settings_.online_own_gang_overlay_style = NormalizeOnlineOverlayStyle(online_own_gang_overlay_style);
    settings_.online_own_gang_overlay_opacity = NormalizeOnlineOverlayOpacity(online_own_gang_overlay_opacity);
    settings_.online_own_gang_overlay_background_enabled = online_own_gang_overlay_background_enabled;
    settings_.online_own_gang_overlay_scale = NormalizeOnlineOverlayScale(online_own_gang_overlay_scale);
    settings_.online_own_gang_overlay_custom_text_color_enabled = online_own_gang_overlay_custom_text_color_enabled;
    settings_.online_own_gang_overlay_text_color_r = NormalizeOnlineOverlayColor(online_own_gang_overlay_text_color_r, 1.0f);
    settings_.online_own_gang_overlay_text_color_g = NormalizeOnlineOverlayColor(online_own_gang_overlay_text_color_g, 1.0f);
    settings_.online_own_gang_overlay_text_color_b = NormalizeOnlineOverlayColor(online_own_gang_overlay_text_color_b, 1.0f);
    settings_.online_own_gang_overlay_text_color_a = NormalizeOnlineOverlayColor(online_own_gang_overlay_text_color_a, 1.0f);
    settings_.online_wanted_overlay_enabled = online_wanted_overlay_enabled;
    settings_.online_wanted_overlay_anchor = NormalizeOnlineOverlayAnchor(
        found_online_wanted_overlay_anchor ? online_wanted_overlay_anchor : settings_.online_overlay_anchor);
    settings_.online_wanted_overlay_offset_x = found_online_wanted_overlay_offset_x && std::isfinite(online_wanted_overlay_offset_x)
        ? online_wanted_overlay_offset_x
        : 12.0f;
    settings_.online_wanted_overlay_offset_y = found_online_wanted_overlay_offset_y && std::isfinite(online_wanted_overlay_offset_y)
        ? online_wanted_overlay_offset_y
        : 12.0f;
    settings_.online_wanted_overlay_style = NormalizeOnlineOverlayStyle(online_wanted_overlay_style);
    settings_.online_wanted_overlay_opacity = NormalizeOnlineOverlayOpacity(online_wanted_overlay_opacity);
    settings_.online_wanted_overlay_background_enabled = online_wanted_overlay_background_enabled;
    settings_.online_wanted_overlay_scale = NormalizeOnlineOverlayScale(online_wanted_overlay_scale);
    settings_.online_wanted_overlay_custom_text_color_enabled = online_wanted_overlay_custom_text_color_enabled;
    settings_.online_wanted_overlay_text_color_r = NormalizeOnlineOverlayColor(online_wanted_overlay_text_color_r, 1.0f);
    settings_.online_wanted_overlay_text_color_g = NormalizeOnlineOverlayColor(online_wanted_overlay_text_color_g, 1.0f);
    settings_.online_wanted_overlay_text_color_b = NormalizeOnlineOverlayColor(online_wanted_overlay_text_color_b, 1.0f);
    settings_.online_wanted_overlay_text_color_a = NormalizeOnlineOverlayColor(online_wanted_overlay_text_color_a, 1.0f);
    // Clamp bounds to >= 0. max == 0 means "no upper limit"; only swap when
    // both bounds are positive and min > max (e.g. 200-100 → 100-200).
    if (online_wanted_overlay_min < 0) online_wanted_overlay_min = 0;
    if (online_wanted_overlay_max < 0) online_wanted_overlay_max = 0;
    if (online_wanted_overlay_max > 0 && online_wanted_overlay_min > online_wanted_overlay_max) {
        std::swap(online_wanted_overlay_min, online_wanted_overlay_max);
    }
    settings_.online_wanted_overlay_min = online_wanted_overlay_min;
    settings_.online_wanted_overlay_max = online_wanted_overlay_max;
    settings_.online_event_detector_version = online_event_detector_version;

    if (found_legacy_theme_selected || found_legacy_theme_accent_mode || found_legacy_theme_accent_preset) {
        Logger::Info("[SettingsManager] Ignoring deprecated legacy theme fields; using theme_name and theme_custom_accent only");
    }

    settings_.theme_name = NormalizeThemeName(found_theme_name ? theme_name : kDefaultThemeName);
    settings_.theme_custom_accent = NormalizeAccentHexColor(theme_custom_accent, kDefaultThemeAccent);
    settings_.ui_transparency = NormalizeUiTransparency(ui_transparency);

    settings_.hitmarker_saved_presets.clear();

    {
        size_t presetsPos = json_content.find("\"hitmarker_saved_presets\"");
        if (presetsPos != std::string::npos) {
            size_t arrayStart = json_content.find('[', presetsPos);
            size_t arrayEnd = json_content.find(']', arrayStart);
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                std::string arrayContent = json_content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                size_t pos = 0;
                while (pos < arrayContent.length()) {
                    size_t objStart = arrayContent.find('{', pos);
                    if (objStart == std::string::npos) break;
                    size_t objEnd = arrayContent.find('}', objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = arrayContent.substr(objStart, objEnd - objStart + 1);
                    HitmarkerPreset preset;
                    int shape = 0;
                    ParseStringField(obj, "\"name\"", preset.name);
                    ParseIntField(obj, "\"shape\"", shape);
                    preset.shape = static_cast<HitmarkerShape>(shape);
                    ParseFloatField(obj, "\"color_r\"", preset.color.r);
                    ParseFloatField(obj, "\"color_g\"", preset.color.g);
                    ParseFloatField(obj, "\"color_b\"", preset.color.b);
                    ParseFloatField(obj, "\"color_a\"", preset.color.a);
                    ParseFloatField(obj, "\"kill_color_r\"", preset.killConfirmedColor.r);
                    ParseFloatField(obj, "\"kill_color_g\"", preset.killConfirmedColor.g);
                    ParseFloatField(obj, "\"kill_color_b\"", preset.killConfirmedColor.b);
                    ParseFloatField(obj, "\"kill_color_a\"", preset.killConfirmedColor.a);
                    ParseFloatField(obj, "\"size\"", preset.size);
                    ParseFloatField(obj, "\"opacity\"", preset.opacity);
                    ParseFloatField(obj, "\"duration\"", preset.duration);
                    ParseBoolField(obj, "\"fade_out\"", preset.fadeOut);
                    ParseStringField(obj, "\"custom_image_path\"", preset.customImagePath);
                    preset.customImagePath = NormalizeHitmarkerImagePath(preset.customImagePath);
                    settings_.hitmarker_saved_presets.push_back(preset);

                    pos = objEnd + 1;
                }
            }
        }
    }

    NormalizeSavedHitmarkerPresets(settings_.hitmarker_saved_presets);

    settings_.interface_editor_saved_presets.clear();

    {
        size_t presetsPos = json_content.find("\"interface_editor_saved_presets\"");
        if (presetsPos != std::string::npos) {
            found_interface_editor_saved_presets = true;
            size_t arrayStart = json_content.find('[', presetsPos);
            size_t arrayEnd = json_content.find(']', arrayStart);
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                std::string arrayContent = json_content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                size_t pos = 0;
                while (pos < arrayContent.length()) {
                    size_t objStart = arrayContent.find('{', pos);
                    if (objStart == std::string::npos) break;
                    size_t objEnd = arrayContent.find('}', objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = arrayContent.substr(objStart, objEnd - objStart + 1);
                    InterfacePreset preset;
                    ParseStringField(obj, "\"name\"", preset.name);
                    ParseBoolField(obj, "\"show_master_hud\"", preset.show_master_hud);
                    ParseBoolField(obj, "\"show_radar\"", preset.show_radar);
                    ParseBoolField(obj, "\"show_area_name\"", preset.show_area_name);
                    ParseBoolField(obj, "\"show_vehicle_name\"", preset.show_vehicle_name);
                    ParseBoolField(obj, "\"show_clock\"", preset.show_clock);
                    ParseBoolField(obj, "\"show_vital_stats\"", preset.show_vital_stats);
                    ParseBoolField(obj, "\"show_3d_markers\"", preset.show_3d_markers);
                    ParseFloatField(obj, "\"hp_color_r\"", preset.hp_color_r);
                    ParseFloatField(obj, "\"hp_color_g\"", preset.hp_color_g);
                    ParseFloatField(obj, "\"hp_color_b\"", preset.hp_color_b);
                    ParseFloatField(obj, "\"hp_color_a\"", preset.hp_color_a);
                    ParseFloatField(obj, "\"armor_color_r\"", preset.armor_color_r);
                    ParseFloatField(obj, "\"armor_color_g\"", preset.armor_color_g);
                    ParseFloatField(obj, "\"armor_color_b\"", preset.armor_color_b);
                    ParseFloatField(obj, "\"armor_color_a\"", preset.armor_color_a);
                    ParseFloatField(obj, "\"money_color_r\"", preset.money_color_r);
                    ParseFloatField(obj, "\"money_color_g\"", preset.money_color_g);
                    ParseFloatField(obj, "\"money_color_b\"", preset.money_color_b);
                    ParseFloatField(obj, "\"money_color_a\"", preset.money_color_a);
                    ParseBoolField(obj, "\"show_hp_number\"", preset.show_hp_number);
                    ParseBoolField(obj, "\"show_armor_number\"", preset.show_armor_number);
                    const bool hasLegacyVitalStatsPosX = ParseFloatField(obj, "\"vital_stats_numbers_pos_x\"", preset.vital_stats_numbers_pos_x);
                    const bool hasLegacyVitalStatsPosY = ParseFloatField(obj, "\"vital_stats_numbers_pos_y\"", preset.vital_stats_numbers_pos_y);
                    const bool hasHpNumberPosX = ParseFloatField(obj, "\"hp_number_pos_x\"", preset.hp_number_pos_x);
                    const bool hasHpNumberPosY = ParseFloatField(obj, "\"hp_number_pos_y\"", preset.hp_number_pos_y);
                    const bool hasArmorNumberPosX = ParseFloatField(obj, "\"armor_number_pos_x\"", preset.armor_number_pos_x);
                    const bool hasArmorNumberPosY = ParseFloatField(obj, "\"armor_number_pos_y\"", preset.armor_number_pos_y);
                    ParseFloatField(obj, "\"hp_number_scale\"", preset.hp_number_scale);
                    ParseFloatField(obj, "\"armor_number_scale\"", preset.armor_number_scale);
                    if (hasLegacyVitalStatsPosX && hasLegacyVitalStatsPosY) {
                        if (!hasHpNumberPosX) preset.hp_number_pos_x = preset.vital_stats_numbers_pos_x;
                        if (!hasHpNumberPosY) preset.hp_number_pos_y = preset.vital_stats_numbers_pos_y;
                        if (!hasArmorNumberPosX) preset.armor_number_pos_x = preset.vital_stats_numbers_pos_x;
                        if (!hasArmorNumberPosY) preset.armor_number_pos_y = preset.vital_stats_numbers_pos_y;
                    }
                    NormalizeVitalStatsNumbersPosition(preset);
                    settings_.interface_editor_saved_presets.push_back(preset);

                    pos = objEnd + 1;
                }
            }
        }
    }

    NormalizeSavedInterfacePresets(settings_.interface_editor_saved_presets);

    settings_.rss_feeds.clear();

    {
        size_t feedsPos = json_content.find("\"rss_feeds\"");
        if (feedsPos != std::string::npos) {
            found_rss_feeds = true;
            size_t arrayStart = json_content.find('[', feedsPos);
            size_t arrayEnd = FindMatchingDelimiter(json_content, arrayStart, '[', ']');
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                std::string arrayContent = json_content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                size_t pos = 0;
                while (pos < arrayContent.length()) {
                    size_t objStart = arrayContent.find('{', pos);
                    if (objStart == std::string::npos) {
                        break;
                    }

                    size_t objEnd = FindMatchingDelimiter(arrayContent, objStart, '{', '}');
                    if (objEnd == std::string::npos) {
                        break;
                    }

                    std::string obj = arrayContent.substr(objStart, objEnd - objStart + 1);
                    RssFeedConfig feed;
                    ParseStringField(obj, "\"url\"", feed.url);
                    ParseStringField(obj, "\"label\"", feed.label);
                    if (!ParseStringField(obj, "\"last_modified\"", feed.lastModified)) {
                        ParseStringField(obj, "\"lastModified\"", feed.lastModified);
                    }
                    ParseStringField(obj, "\"etag\"", feed.etag);
                    if (!ParseIntField(obj, "\"poll_interval_seconds\"", feed.pollIntervalSeconds)) {
                        ParseIntField(obj, "\"pollIntervalSeconds\"", feed.pollIntervalSeconds);
                    }
                    ParseBoolField(obj, "\"enabled\"", feed.enabled);

                    std::string seenGuidsArray;
                    if (ExtractJsonArrayField(obj, "\"seen_guids\"", seenGuidsArray)) {
                        size_t seenPos = 0;
                        while (seenPos < seenGuidsArray.length()) {
                            size_t seenObjStart = seenGuidsArray.find('{', seenPos);
                            if (seenObjStart == std::string::npos) {
                                break;
                            }

                            size_t seenObjEnd = FindMatchingDelimiter(seenGuidsArray, seenObjStart, '{', '}');
                            if (seenObjEnd == std::string::npos) {
                                break;
                            }

                            std::string seenObj = seenGuidsArray.substr(seenObjStart, seenObjEnd - seenObjStart + 1);
                            RssSeenGuid seenGuid;
                            ParseStringField(seenObj, "\"guid\"", seenGuid.guid);
                            if (!ParseTimeField(seenObj, "\"seen_at\"", seenGuid.seenAt)) {
                                ParseTimeField(seenObj, "\"seenAt\"", seenGuid.seenAt);
                            }

                            if (!seenGuid.guid.empty()) {
                                feed.seen_guids.push_back(seenGuid);
                            }

                            seenPos = seenObjEnd + 1;
                        }
                    }

                    if (!feed.url.empty()) {
                        settings_.rss_feeds.push_back(feed);
                    }

                    pos = objEnd + 1;
                }
            }
        }
    }

    // Online: parse selected_gangs array
    {
        size_t gangsPos = json_content.find("\"online_quick_overlay_selected_gangs\"");
        if (gangsPos != std::string::npos) {
            found_online_quick_overlay_selected_gangs = true;
            std::string gangsArrayContent;
            if (ExtractJsonArrayField(json_content, "\"online_quick_overlay_selected_gangs\"", gangsArrayContent)) {
                size_t pos = 0;
                while (pos < gangsArrayContent.length()) {
                    size_t quoteStart = gangsArrayContent.find('"', pos);
                    if (quoteStart == std::string::npos) break;
                    size_t quoteEnd = quoteStart + 1;
                    while (quoteEnd < gangsArrayContent.size()) {
                        quoteEnd = gangsArrayContent.find('"', quoteEnd);
                        if (quoteEnd == std::string::npos) break;
                        size_t backslashCount = 0;
                        size_t checkPos = quoteEnd - 1;
                        while (checkPos >= quoteStart + 1 && gangsArrayContent[checkPos] == '\\') {
                            ++backslashCount;
                            if (checkPos == 0) break;
                            --checkPos;
                        }
                        if (backslashCount % 2 == 0) break;
                        ++quoteEnd;
                    }
                    if (quoteEnd == std::string::npos || quoteEnd >= gangsArrayContent.size()) break;
                    std::string gang = UnescapeJsonString(gangsArrayContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
                    if (!gang.empty()) {
                        online_quick_overlay_selected_gangs.push_back(std::move(gang));
                    }
                    pos = quoteEnd + 1;
                }
            }
        }
    }

    // Assign selected_gangs now that parsing is complete
    settings_.online_quick_overlay_selected_gangs = DedupeGangSelection(std::move(online_quick_overlay_selected_gangs));

    bool legacyMarkersEnabled = false;
    ParseBoolField(json_content, "\"online_markers_overlay_enabled\"", legacyMarkersEnabled);
    settings_.online_friends_overlay_enabled = legacyMarkersEnabled;
    settings_.online_enemies_overlay_enabled = legacyMarkersEnabled;
    ParseBoolField(json_content, "\"online_friends_overlay_enabled\"", settings_.online_friends_overlay_enabled);
    ParseBoolField(json_content, "\"online_enemies_overlay_enabled\"", settings_.online_enemies_overlay_enabled);
    ParseBoolField(json_content, "\"online_quick_overlay_show_rcb\"", settings_.online_quick_overlay_show_rcb);
    ParseBoolField(json_content, "\"reconnect_use_nova_launcher\"", settings_.reconnect_use_nova_launcher);
    ParseStringField(json_content, "\"reconnect_nova_launcher_path\"", settings_.reconnect_nova_launcher_path);
    ParseStringField(json_content, "\"reconnect_openmp_launcher_path\"", settings_.reconnect_openmp_launcher_path);
    ParseBoolField(json_content, "\"online_location_overlay_enabled\"", settings_.online_location_overlay_enabled);
    ParseBoolField(json_content, "\"online_transport_overlay_enabled\"", settings_.online_transport_overlay_enabled);
    ParseBoolField(json_content, "\"online_pedslots_overlay_enabled\"", settings_.online_pedslots_overlay_enabled);
    // Split the old shared radar position into two independently saved panels.
    // Keep friends at the old position and start enemies 120 px farther from
    // the same edge. Explicit new values below always take precedence.
    std::string legacyRadarAnchor = "top_left";
    float legacyRadarX = 12.0f;
    float legacyRadarY = 250.0f;
    if (ParseStringField(json_content, "\"online_markers_overlay_anchor\"", legacyRadarAnchor)) {
        settings_.online_friends_overlay_anchor = NormalizeOnlineOverlayAnchor(legacyRadarAnchor);
        settings_.online_enemies_overlay_anchor = settings_.online_friends_overlay_anchor;
    }
    if (ParseFloatField(json_content, "\"online_markers_overlay_offset_x\"", legacyRadarX)) {
        settings_.online_friends_overlay_offset_x = std::isfinite(legacyRadarX) ? legacyRadarX : 12.0f;
        settings_.online_enemies_overlay_offset_x = settings_.online_friends_overlay_offset_x;
    }
    if (ParseFloatField(json_content, "\"online_markers_overlay_offset_y\"", legacyRadarY)) {
        settings_.online_friends_overlay_offset_y = std::isfinite(legacyRadarY) ? legacyRadarY : 250.0f;
        settings_.online_enemies_overlay_offset_y = settings_.online_friends_overlay_offset_y + 120.0f;
    }
    ParseStringField(json_content, "\"online_friends_overlay_anchor\"", settings_.online_friends_overlay_anchor);
    settings_.online_friends_overlay_anchor = NormalizeOnlineOverlayAnchor(settings_.online_friends_overlay_anchor);
    ParseFloatField(json_content, "\"online_friends_overlay_offset_x\"", settings_.online_friends_overlay_offset_x);
    if (!std::isfinite(settings_.online_friends_overlay_offset_x)) settings_.online_friends_overlay_offset_x = 12.0f;
    ParseFloatField(json_content, "\"online_friends_overlay_offset_y\"", settings_.online_friends_overlay_offset_y);
    if (!std::isfinite(settings_.online_friends_overlay_offset_y)) settings_.online_friends_overlay_offset_y = 250.0f;
    ParseStringField(json_content, "\"online_enemies_overlay_anchor\"", settings_.online_enemies_overlay_anchor);
    settings_.online_enemies_overlay_anchor = NormalizeOnlineOverlayAnchor(settings_.online_enemies_overlay_anchor);
    ParseFloatField(json_content, "\"online_enemies_overlay_offset_x\"", settings_.online_enemies_overlay_offset_x);
    if (!std::isfinite(settings_.online_enemies_overlay_offset_x)) settings_.online_enemies_overlay_offset_x = 12.0f;
    ParseFloatField(json_content, "\"online_enemies_overlay_offset_y\"", settings_.online_enemies_overlay_offset_y);
    if (!std::isfinite(settings_.online_enemies_overlay_offset_y)) settings_.online_enemies_overlay_offset_y = 370.0f;
    ParseStringField(json_content, "\"online_location_overlay_anchor\"", settings_.online_location_overlay_anchor);
    settings_.online_location_overlay_anchor = NormalizeOnlineOverlayAnchor(settings_.online_location_overlay_anchor);
    ParseFloatField(json_content, "\"online_location_overlay_offset_x\"", settings_.online_location_overlay_offset_x);
    if (!std::isfinite(settings_.online_location_overlay_offset_x)) settings_.online_location_overlay_offset_x = 12.0f;
    ParseFloatField(json_content, "\"online_location_overlay_offset_y\"", settings_.online_location_overlay_offset_y);
    if (!std::isfinite(settings_.online_location_overlay_offset_y)) settings_.online_location_overlay_offset_y = 12.0f;
    ParseStringField(json_content, "\"online_transport_overlay_anchor\"", settings_.online_transport_overlay_anchor);
    settings_.online_transport_overlay_anchor = NormalizeOnlineOverlayAnchor(settings_.online_transport_overlay_anchor);
    ParseFloatField(json_content, "\"online_transport_overlay_offset_x\"", settings_.online_transport_overlay_offset_x);
    if (!std::isfinite(settings_.online_transport_overlay_offset_x)) settings_.online_transport_overlay_offset_x = 12.0f;
    ParseFloatField(json_content, "\"online_transport_overlay_offset_y\"", settings_.online_transport_overlay_offset_y);
    if (!std::isfinite(settings_.online_transport_overlay_offset_y)) settings_.online_transport_overlay_offset_y = 12.0f;
    ParseStringField(json_content, "\"online_pedslots_overlay_anchor\"", settings_.online_pedslots_overlay_anchor);
    settings_.online_pedslots_overlay_anchor = NormalizeOnlineOverlayAnchor(settings_.online_pedslots_overlay_anchor);
    ParseFloatField(json_content, "\"online_pedslots_overlay_offset_x\"", settings_.online_pedslots_overlay_offset_x);
    if (!std::isfinite(settings_.online_pedslots_overlay_offset_x)) settings_.online_pedslots_overlay_offset_x = 12.0f;
    ParseFloatField(json_content, "\"online_pedslots_overlay_offset_y\"", settings_.online_pedslots_overlay_offset_y);
    if (!std::isfinite(settings_.online_pedslots_overlay_offset_y)) settings_.online_pedslots_overlay_offset_y = 12.0f;

    settings_.online_player_markers.clear();
    std::string markersArray;
    if (ExtractJsonArrayField(json_content, "\"online_player_markers\"", markersArray)) {
        size_t pos = 0;
        while (settings_.online_player_markers.size() < 20) {
            const size_t start = markersArray.find('{', pos);
            if (start == std::string::npos) break;
            const size_t end = FindMatchingDelimiter(markersArray, start, '{', '}');
            if (end == std::string::npos) break;
            const std::string object = markersArray.substr(start, end - start + 1);
            OnlinePlayerMarker marker;
            ParseStringField(object, "\"name\"", marker.name);
            ParseStringField(object, "\"affiliation\"", marker.affiliation);
            ParseBoolField(object, "\"friendly\"", marker.friendly);
            marker.name = TrimWhitespace(marker.name).substr(0, 24);
            marker.affiliation = TrimWhitespace(marker.affiliation).substr(0, 48);
            const auto sameName = [&marker](const OnlinePlayerMarker& other) {
                if (marker.name.size() != other.name.size()) return false;
                return std::equal(marker.name.begin(), marker.name.end(), other.name.begin(),
                    [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
            };
            if (!marker.name.empty()
                && std::none_of(settings_.online_player_markers.begin(), settings_.online_player_markers.end(), sameName)) {
                settings_.online_player_markers.push_back(std::move(marker));
            }
            pos = end + 1;
        }
    }

    // Online: parse event_placeholders array
    {
        size_t phPos = json_content.find("\"online_event_placeholders\"");
        if (phPos != std::string::npos) {
            settings_.online_event_placeholders.clear();
            size_t arrayStart = json_content.find('[', phPos);
            size_t arrayEnd = FindMatchingDelimiter(json_content, arrayStart, '[', ']');
            if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                std::string arrayContent = json_content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                size_t pos = 0;
                while (pos < arrayContent.length()) {
                    size_t objStart = arrayContent.find('{', pos);
                    if (objStart == std::string::npos) break;
                    size_t objEnd = FindMatchingDelimiter(arrayContent, objStart, '{', '}');
                    if (objEnd == std::string::npos) break;

                    std::string obj = arrayContent.substr(objStart, objEnd - objStart + 1);
                    OnlineEventPlaceholder placeholder;
                    ParseStringField(obj, "\"event_type\"", placeholder.event_type);

                    std::string patternsArray;
                    if (ExtractJsonArrayField(obj, "\"patterns\"", patternsArray)) {
                        size_t patPos = 0;
                        while (patPos < patternsArray.length()) {
                            size_t quoteStart = patternsArray.find('"', patPos);
                            if (quoteStart == std::string::npos) break;
                            size_t quoteEnd = quoteStart + 1;
                            while (quoteEnd < patternsArray.size()) {
                                quoteEnd = patternsArray.find('"', quoteEnd);
                                if (quoteEnd == std::string::npos) break;
                                size_t backslashCount = 0;
                                size_t checkPos = quoteEnd - 1;
                                while (checkPos >= quoteStart + 1 && patternsArray[checkPos] == '\\') {
                                    ++backslashCount;
                                    if (checkPos == 0) break;
                                    --checkPos;
                                }
                                if (backslashCount % 2 == 0) break;
                                ++quoteEnd;
                            }
                            if (quoteEnd == std::string::npos || quoteEnd >= patternsArray.size()) break;
                            std::string pattern = UnescapeJsonString(patternsArray.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
                            if (!pattern.empty()) {
                                placeholder.patterns.push_back(std::move(pattern));
                            }
                            patPos = quoteEnd + 1;
                        }
                    }

                    if (!placeholder.event_type.empty()) {
                        settings_.online_event_placeholders.push_back(std::move(placeholder));
                    }

                    pos = objEnd + 1;
                }
            }
        }
    }

    // If no event placeholders were parsed from JSON, populate defaults
    if (settings_.online_event_placeholders.empty()) {
        settings_.online_event_placeholders = MakeDefaultEventPlaceholders();
    }

    settings_.kill_tracker_weapon_stats.clear();
    {
        size_t stats_pos = json_content.find("\"kill_tracker_weapon_stats\"");
        if (stats_pos != std::string::npos) {
            size_t array_start = json_content.find('[', stats_pos);
            size_t array_end = json_content.find(']', array_start);
            if (array_start != std::string::npos && array_end != std::string::npos) {
                std::string array_content = json_content.substr(array_start + 1, array_end - array_start - 1);
                size_t pos = 0;
                while (pos < array_content.length()) {
                    size_t obj_start = array_content.find('{', pos);
                    if (obj_start == std::string::npos) break;
                    size_t obj_end = array_content.find('}', obj_start);
                    if (obj_end == std::string::npos) break;
                    std::string obj = array_content.substr(obj_start, obj_end - obj_start + 1);

                    WeaponStatsData ws;
                    ws.weaponId = -1;
                    ws.totalDamage = 0.0f;
                    ws.killCount = 0;

                    size_t wid_pos = obj.find("\"weaponId\"");
                    if (wid_pos != std::string::npos) {
                        size_t colon = obj.find(':', wid_pos);
                        if (colon != std::string::npos) {
                            size_t num_start = obj.find_first_of("0123456789-", colon);
                            size_t num_end = obj.find_first_not_of("0123456789-", num_start);
                            ws.weaponId = std::atoi(obj.substr(num_start, num_end - num_start).c_str());
                        }
                    }
                    size_t dmg_pos = obj.find("\"totalDamage\"");
                    if (dmg_pos != std::string::npos) {
                        size_t colon = obj.find(':', dmg_pos);
                        if (colon != std::string::npos) {
                            size_t num_start = obj.find_first_of("0123456789.-", colon);
                            size_t num_end = obj.find_first_not_of("0123456789.-", num_start);
                            ws.totalDamage = static_cast<float>(std::atof(obj.substr(num_start, num_end - num_start).c_str()));
                        }
                    }
                    size_t kc_pos = obj.find("\"killCount\"");
                    if (kc_pos != std::string::npos) {
                        size_t colon = obj.find(':', kc_pos);
                        if (colon != std::string::npos) {
                            size_t num_start = obj.find_first_of("0123456789-", colon);
                            size_t num_end = obj.find_first_not_of("0123456789-", num_start);
                            ws.killCount = std::atoi(obj.substr(num_start, num_end - num_start).c_str());
                        }
                    }

                    if (ws.weaponId >= 0) {
                        settings_.kill_tracker_weapon_stats.push_back(ws);
                    }

                    pos = obj_end + 1;
                }
            }
        }
    }

    settings_.kill_tracker_recent_kills.clear();
    {
        size_t kills_pos = json_content.find("\"kill_tracker_recent_kills\"");
        if (kills_pos != std::string::npos) {
            size_t array_start = json_content.find('[', kills_pos);
            size_t array_end = json_content.find(']', array_start);
            if (array_start != std::string::npos && array_end != std::string::npos) {
                std::string array_content = json_content.substr(array_start + 1, array_end - array_start - 1);
                size_t pos = 0;
                while (pos < array_content.length()) {
                    size_t obj_start = array_content.find('{', pos);
                    if (obj_start == std::string::npos) break;
                    size_t obj_end = array_content.find('}', obj_start);
                    if (obj_end == std::string::npos) break;
                    std::string obj = array_content.substr(obj_start, obj_end - obj_start + 1);

                    KillEventData ke;
                    ke.weaponId = -1;
                    ke.timestamp = 0;
                    ke.damageDealt = 0.0f;

                    size_t v_pos = obj.find("\"victim\"");
                    if (v_pos != std::string::npos) {
                        size_t colon = obj.find(':', v_pos);
                        size_t q1 = obj.find('"', colon + 1);
                        size_t q2 = obj.find('"', q1 + 1);
                        if (q1 != std::string::npos && q2 != std::string::npos) {
                            ke.victim = obj.substr(q1 + 1, q2 - q1 - 1);
                        }
                    }
                    size_t wid_pos = obj.find("\"weaponId\"");
                    if (wid_pos != std::string::npos) {
                        size_t colon = obj.find(':', wid_pos);
                        size_t num_start = obj.find_first_of("0123456789-", colon);
                        size_t num_end = obj.find_first_not_of("0123456789-", num_start);
                        ke.weaponId = std::atoi(obj.substr(num_start, num_end - num_start).c_str());
                    }
                    size_t ts_pos = obj.find("\"timestamp\"");
                    if (ts_pos != std::string::npos) {
                        size_t colon = obj.find(':', ts_pos);
                        size_t num_start = obj.find_first_of("0123456789-", colon);
                        size_t num_end = obj.find_first_not_of("0123456789-", num_start);
                        ke.timestamp = static_cast<std::time_t>(std::atoll(obj.substr(num_start, num_end - num_start).c_str()));
                    }
                    size_t dmg_pos = obj.find("\"damageDealt\"");
                    if (dmg_pos != std::string::npos) {
                        size_t colon = obj.find(':', dmg_pos);
                        size_t num_start = obj.find_first_of("0123456789.-", colon);
                        size_t num_end = obj.find_first_not_of("0123456789.-", num_start);
                        ke.damageDealt = static_cast<float>(std::atof(obj.substr(num_start, num_end - num_start).c_str()));
                    }

                    if (!ke.victim.empty()) {
                        settings_.kill_tracker_recent_kills.push_back(ke);
                    }

                    pos = obj_end + 1;
                }
            }
        }
    }

    settings_.kill_tracker_kill_count = 0;
    bool found_kill_count = false;
    {
        size_t kc_pos = json_content.find("\"kill_tracker_kill_count\"");
        if (kc_pos != std::string::npos) {
            size_t colon = json_content.find(':', kc_pos);
            size_t num_start = json_content.find_first_of("0123456789", colon);
            size_t num_end = json_content.find_first_not_of("0123456789", num_start);
            if (num_start != std::string::npos) {
                settings_.kill_tracker_kill_count = std::atoi(json_content.substr(num_start, num_end - num_start).c_str());
                found_kill_count = true;
            }
        }
    }

    if (!found_kill_count) {
        int derivedKillCount = 0;
        for (const auto& ws : settings_.kill_tracker_weapon_stats) {
            derivedKillCount += ws.killCount;
        }
        settings_.kill_tracker_kill_count = derivedKillCount;
    }

    settings_.kill_tracker_death_count = 0;
    {
        size_t dc_pos = json_content.find("\"kill_tracker_death_count\"");
        if (dc_pos != std::string::npos) {
            size_t colon = json_content.find(':', dc_pos);
            size_t num_start = json_content.find_first_of("0123456789", colon);
            size_t num_end = json_content.find_first_not_of("0123456789", num_start);
            if (num_start != std::string::npos)
                settings_.kill_tracker_death_count = std::atoi(json_content.substr(num_start, num_end - num_start).c_str());
        }
    }

    settings_.kill_tracker_recent_deaths.clear();
    {
        size_t deaths_pos = json_content.find("\"kill_tracker_recent_deaths\"");
        if (deaths_pos != std::string::npos) {
            size_t array_start = json_content.find('[', deaths_pos);
            size_t array_end = json_content.find(']', array_start);
            if (array_start != std::string::npos && array_end != std::string::npos) {
                std::string array_content = json_content.substr(array_start + 1, array_end - array_start - 1);
                size_t pos = 0;
                while (pos < array_content.length()) {
                    size_t obj_start = array_content.find('{', pos);
                    if (obj_start == std::string::npos) break;
                    size_t obj_end = array_content.find('}', obj_start);
                    if (obj_end == std::string::npos) break;
                    std::string obj = array_content.substr(obj_start, obj_end - obj_start + 1);

                    DeathEventData de;
                    de.weaponId = -1;
                    de.timestamp = 0;

                    size_t k_pos = obj.find("\"killer\"");
                    if (k_pos != std::string::npos) {
                        size_t colon = obj.find(':', k_pos);
                        size_t q1 = obj.find('"', colon + 1);
                        size_t q2 = obj.find('"', q1 + 1);
                        if (q1 != std::string::npos && q2 != std::string::npos)
                            de.killer = obj.substr(q1 + 1, q2 - q1 - 1);
                    }
                    size_t wid_pos = obj.find("\"weaponId\"");
                    if (wid_pos != std::string::npos) {
                        size_t colon = obj.find(':', wid_pos);
                        size_t num_start = obj.find_first_of("0123456789-", colon);
                        size_t num_end = obj.find_first_not_of("0123456789-", num_start);
                        de.weaponId = std::atoi(obj.substr(num_start, num_end - num_start).c_str());
                    }
                    size_t ts_pos = obj.find("\"timestamp\"");
                    if (ts_pos != std::string::npos) {
                        size_t colon = obj.find(':', ts_pos);
                        size_t num_start = obj.find_first_of("0123456789-", colon);
                        size_t num_end = obj.find_first_not_of("0123456789-", num_start);
                        de.timestamp = static_cast<std::time_t>(std::atoll(obj.substr(num_start, num_end - num_start).c_str()));
                    }

                    settings_.kill_tracker_recent_deaths.push_back(de);
                    pos = obj_end + 1;
                }
            }
        }
    }

    if (found_dpi || found_auto || found_ui_open_command_primary || found_ui_open_command_secondary || found_ui_toggle_hotkey || found_ui_window_width || found_ui_window_height || found_ui_transparency || found_kill_tracker || found_local_kill_message_enabled || found_local_death_message_enabled || found_server_kill_message_enabled || found_server_death_message_enabled || found_track_damage || found_local_kill_message_template || found_local_death_message_template || found_server_kill_message_template || found_server_death_message_template || found_manual_edits || found_auto_hudscalefix || found_auto_engine_start || found_damage_log_enabled || found_quickmap_enabled || found_quickmap_hotkey || found_quickmap_mode || found_quickmap_map_variant || found_quickmap_map_transparency || found_quickmap_radar_pack || found_hitmarker_enabled || found_hitmarker_active_preset_index || found_hitmarker_size || found_hitmarker_color_r || found_hitmarker_color_g || found_hitmarker_color_b || found_hitmarker_color_a || found_hitmarker_kill_color_r || found_hitmarker_kill_color_g || found_hitmarker_kill_color_b || found_hitmarker_kill_color_a || found_hitmarker_opacity || found_hitmarker_duration || found_hitmarker_fade_out || found_hitmarker_shape || found_hitmarker_custom_image_path || found_interface_editor_active_preset_index || found_interface_editor_saved_presets || found_crosshair_enabled || found_crosshair_texture_path || found_squaremap_enabled || found_fist_texture_path || found_hitsound_outgoing_enabled || found_hitsound_outgoing_path || found_hitsound_incoming_enabled || found_hitsound_incoming_path || found_rss_enabled || found_rss_notification_mode || found_rss_feeds || found_theme_name || found_legacy_theme_selected || found_legacy_theme_accent_mode || found_legacy_theme_accent_preset) {
        Logger::Info("[SettingsManager] Loaded settings (ui_primary=/%s, ui_secondary=/%s, ui_hotkey=%d, kill_tracker_enabled=%s, local_kill=%s, local_death=%s, server_kill=%s, server_death=%s, kill_tracker_track_damage=%s, kill_tracker_allow_manual_edits=%s, auto_hudscalefix=%s, auto_engine_start=%s, hitmarker_enabled=%s)",
            settings_.ui_open_command_primary.c_str(),
            settings_.ui_open_command_secondary.c_str(),
            settings_.ui_toggle_hotkey,
            settings_.kill_tracker_enabled ? "true" : "false",
            settings_.kill_tracker_local_kill_message_enabled ? "true" : "false",
            settings_.kill_tracker_local_death_message_enabled ? "true" : "false",
            settings_.kill_tracker_server_kill_message_enabled ? "true" : "false",
            settings_.kill_tracker_server_death_message_enabled ? "true" : "false",
            settings_.kill_tracker_track_damage ? "true" : "false",
            settings_.kill_tracker_allow_manual_edits ? "true" : "false",
            settings_.auto_hudscalefix ? "true" : "false",
            settings_.auto_engine_start ? "true" : "false",
            settings_.hitmarker_enabled ? "true" : "false");
    } else {
        Logger::Info("[SettingsManager] No settings found in config; using defaults (ui_primary=/%s, ui_secondary=/%s, ui_hotkey=%d, kill_tracker_enabled=%s, local_kill=%s, local_death=%s, server_kill=%s, server_death=%s, kill_tracker_track_damage=%s, kill_tracker_allow_manual_edits=%s, auto_hudscalefix=%s, auto_engine_start=%s, hitmarker_enabled=%s)",
            settings_.ui_open_command_primary.c_str(),
            settings_.ui_open_command_secondary.c_str(),
            settings_.ui_toggle_hotkey,
            settings_.kill_tracker_enabled ? "true" : "false",
            settings_.kill_tracker_local_kill_message_enabled ? "true" : "false",
            settings_.kill_tracker_local_death_message_enabled ? "true" : "false",
            settings_.kill_tracker_server_kill_message_enabled ? "true" : "false",
            settings_.kill_tracker_server_death_message_enabled ? "true" : "false",
            settings_.kill_tracker_track_damage ? "true" : "false",
            settings_.kill_tracker_allow_manual_edits ? "true" : "false",
            settings_.auto_hudscalefix ? "true" : "false",
            settings_.auto_engine_start ? "true" : "false",
            settings_.hitmarker_enabled ? "true" : "false");
    }

    return true;
}

std::string SettingsManager::GetSettingsJson() const {
    std::ostringstream oss;
    oss << "  \"settings\": {\n";
    oss << "    \"version\": " << SettingsManager::SETTINGS_VERSION << ",\n";
    oss << "    \"ui_open_command_primary\": \"" << EscapeJsonString(NormalizeUiOpenCommand(settings_.ui_open_command_primary)) << "\",\n";
    oss << "    \"ui_open_command_secondary\": \"" << EscapeJsonString(NormalizeUiOpenCommand(settings_.ui_open_command_secondary)) << "\",\n";
    oss << "    \"ui_toggle_hotkey\": " << NormalizeUiToggleHotkey(settings_.ui_toggle_hotkey) << ",\n";
    if (settings_.ui_window_width > 0.0f && settings_.ui_window_height > 0.0f) {
        oss << "    \"ui_window_width\": " << settings_.ui_window_width << ",\n";
        oss << "    \"ui_window_height\": " << settings_.ui_window_height << ",\n";
    }
    oss << "    \"kill_tracker_count_kills\": " << (settings_.kill_tracker_count_kills ? "true" : "false") << ",\n";
    oss << "    \"kill_tracker_count_deaths\": " << (settings_.kill_tracker_count_deaths ? "true" : "false") << ",\n";
    oss << "    \"online_friends_overlay_enabled\": " << (settings_.online_friends_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_enemies_overlay_enabled\": " << (settings_.online_enemies_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_show_rcb\": " << (settings_.online_quick_overlay_show_rcb ? "true" : "false") << ",\n";
    oss << "    \"reconnect_use_nova_launcher\": " << (settings_.reconnect_use_nova_launcher ? "true" : "false") << ",\n";
    oss << "    \"reconnect_nova_launcher_path\": \"" << EscapeJsonString(settings_.reconnect_nova_launcher_path) << "\",\n";
    oss << "    \"reconnect_openmp_launcher_path\": \"" << EscapeJsonString(settings_.reconnect_openmp_launcher_path) << "\",\n";
    oss << "    \"online_location_overlay_enabled\": " << (settings_.online_location_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_transport_overlay_enabled\": " << (settings_.online_transport_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_pedslots_overlay_enabled\": " << (settings_.online_pedslots_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_friends_overlay_anchor\": \"" << EscapeJsonString(settings_.online_friends_overlay_anchor) << "\",\n";
    oss << "    \"online_friends_overlay_offset_x\": " << settings_.online_friends_overlay_offset_x << ",\n";
    oss << "    \"online_friends_overlay_offset_y\": " << settings_.online_friends_overlay_offset_y << ",\n";
    oss << "    \"online_enemies_overlay_anchor\": \"" << EscapeJsonString(settings_.online_enemies_overlay_anchor) << "\",\n";
    oss << "    \"online_enemies_overlay_offset_x\": " << settings_.online_enemies_overlay_offset_x << ",\n";
    oss << "    \"online_enemies_overlay_offset_y\": " << settings_.online_enemies_overlay_offset_y << ",\n";
    oss << "    \"online_location_overlay_anchor\": \"" << EscapeJsonString(settings_.online_location_overlay_anchor) << "\",\n";
    oss << "    \"online_location_overlay_offset_x\": " << settings_.online_location_overlay_offset_x << ",\n";
    oss << "    \"online_location_overlay_offset_y\": " << settings_.online_location_overlay_offset_y << ",\n";
    oss << "    \"online_transport_overlay_anchor\": \"" << EscapeJsonString(settings_.online_transport_overlay_anchor) << "\",\n";
    oss << "    \"online_transport_overlay_offset_x\": " << settings_.online_transport_overlay_offset_x << ",\n";
    oss << "    \"online_transport_overlay_offset_y\": " << settings_.online_transport_overlay_offset_y << ",\n";
    oss << "    \"online_pedslots_overlay_anchor\": \"" << EscapeJsonString(settings_.online_pedslots_overlay_anchor) << "\",\n";
    oss << "    \"online_pedslots_overlay_offset_x\": " << settings_.online_pedslots_overlay_offset_x << ",\n";
    oss << "    \"online_pedslots_overlay_offset_y\": " << settings_.online_pedslots_overlay_offset_y << ",\n";
    oss << "    \"online_player_markers\": [";
    bool firstMarker = true;
    for (const auto& marker : settings_.online_player_markers) {
        if (!firstMarker) oss << ", ";
        oss << "{\"name\":\"" << EscapeJsonString(marker.name)
            << "\",\"affiliation\":\"" << EscapeJsonString(marker.affiliation)
            << "\",\"friendly\":" << (marker.friendly ? "true" : "false") << "}";
        firstMarker = false;
    }
    oss << "],\n";
    oss << "    \"kill_tracker_enabled\": " << (settings_.kill_tracker_enabled ? "true" : "false") << ",\n";
    oss << "    \"kill_tracker_local_kill_message_enabled\": " << (settings_.kill_tracker_local_kill_message_enabled ? "true" : "false") << ",\n";
    oss << "    \"kill_tracker_local_death_message_enabled\": " << (settings_.kill_tracker_local_death_message_enabled ? "true" : "false") << ",\n";
    oss << "    \"kill_tracker_server_kill_message_enabled\": " << (settings_.kill_tracker_server_kill_message_enabled ? "true" : "false") << ",\n";
    oss << "    \"kill_tracker_server_death_message_enabled\": " << (settings_.kill_tracker_server_death_message_enabled ? "true" : "false") << ",\n";
    oss << "    \"kill_tracker_track_damage\": " << (settings_.kill_tracker_track_damage ? "true" : "false") << ",\n";
    size_t pos = 0;
    std::string escaped_template = EscapeJsonString(settings_.kill_tracker_local_kill_message_template);
    oss << "    \"kill_tracker_local_kill_message_template\": \"" << escaped_template << "\",\n";

    std::string escaped_death_template = EscapeJsonString(settings_.kill_tracker_local_death_message_template);
    oss << "    \"kill_tracker_local_death_message_template\": \"" << escaped_death_template << "\",\n";

    std::string escaped_server_template = EscapeJsonString(settings_.kill_tracker_server_kill_message_template);
    oss << "    \"kill_tracker_server_kill_message_template\": \"" << escaped_server_template << "\",\n";

    std::string escaped_server_death_template = EscapeJsonString(settings_.kill_tracker_server_death_message_template);
    oss << "    \"kill_tracker_server_death_message_template\": \"" << escaped_server_death_template << "\",\n";
    oss << "    \"kill_tracker_allow_manual_edits\": " << (settings_.kill_tracker_allow_manual_edits ? "true" : "false") << ",\n";
    oss << "    \"auto_hudscalefix\": " << (settings_.auto_hudscalefix ? "true" : "false") << ",\n";
    oss << "    \"auto_engine_start\": " << (settings_.auto_engine_start ? "true" : "false") << ",\n";
    oss << "    \"damage_log_enabled\": " << (settings_.damage_log_enabled ? "true" : "false") << ",\n";
    oss << "    \"quickmap_enabled\": " << (settings_.quickmap_enabled ? "true" : "false") << ",\n";
    oss << "    \"quickmap_hotkey\": " << NormalizeQuickMapHotkey(settings_.quickmap_hotkey) << ",\n";
    oss << "    \"quickmap_mode\": \"" << EscapeJsonString(settings_.quickmap_mode) << "\",\n";
    oss << "    \"quickmap_map_variant\": \"" << EscapeJsonString(settings_.quickmap_map_variant) << "\",\n";
    oss << "    \"quickmap_map_transparency\": " << settings_.quickmap_map_transparency << ",\n";
    oss << "    \"quickmap_auto_zoom\": " << (settings_.quickmap_auto_zoom ? "true" : "false") << ",\n";
    oss << "    \"quickmap_follow_player\": " << (settings_.quickmap_follow_player ? "true" : "false") << ",\n";
    oss << "    \"quickmap_focus_marker\": " << (settings_.quickmap_focus_marker ? "true" : "false") << ",\n";
    oss << "    \"quickmap_zoom_level\": \"" << EscapeJsonString(NormalizeQuickMapZoomLevel(settings_.quickmap_zoom_level)) << "\",\n";
    oss << "    \"quickmap_last_zoom_level\": \"" << EscapeJsonString(NormalizeQuickMapZoomLevel(settings_.quickmap_last_zoom_level)) << "\",\n";
    oss << "    \"quickmap_gangzone_outline_thickness\": " << settings_.quickmap_gangzone_outline_thickness << ",\n";
    oss << "    \"quickmap_gangzone_flash_enabled\": " << (settings_.quickmap_gangzone_flash_enabled ? "true" : "false") << ",\n";
    oss << "    \"speedometer_enabled\": " << (speedometer::kCustomTachoAvailable && settings_.speedometer_enabled ? "true" : "false") << ",\n";
    oss << "    \"speedometer_style\": \"" << EscapeJsonString(speedometer::NormalizeSpeedometerStyleId(settings_.speedometer_style)) << "\",\n";
    oss << "    \"hitmarker_enabled\": " << (settings_.hitmarker_enabled ? "true" : "false") << ",\n";
    oss << "    \"hitmarker_active_preset_index\": " << settings_.hitmarker_active_preset_index << ",\n";
    oss << "    \"hitmarker_size\": " << settings_.hitmarker_size << ",\n";
    oss << "    \"hitmarker_color_r\": " << settings_.hitmarker_color_r << ",\n";
    oss << "    \"hitmarker_color_g\": " << settings_.hitmarker_color_g << ",\n";
    oss << "    \"hitmarker_color_b\": " << settings_.hitmarker_color_b << ",\n";
    oss << "    \"hitmarker_color_a\": " << settings_.hitmarker_color_a << ",\n";
    oss << "    \"hitmarker_kill_color_r\": " << settings_.hitmarker_kill_color_r << ",\n";
    oss << "    \"hitmarker_kill_color_g\": " << settings_.hitmarker_kill_color_g << ",\n";
    oss << "    \"hitmarker_kill_color_b\": " << settings_.hitmarker_kill_color_b << ",\n";
    oss << "    \"hitmarker_kill_color_a\": " << settings_.hitmarker_kill_color_a << ",\n";
    oss << "    \"hitmarker_opacity\": " << settings_.hitmarker_opacity << ",\n";
    oss << "    \"hitmarker_duration\": " << settings_.hitmarker_duration << ",\n";
    oss << "    \"hitmarker_fade_out\": " << (settings_.hitmarker_fade_out ? "true" : "false") << ",\n";
    oss << "    \"hitmarker_shape\": " << settings_.hitmarker_shape << ",\n";
    std::string escaped_hitmarker_custom_image_path = EscapeJsonString(settings_.hitmarker_custom_image_path);
    oss << "    \"hitmarker_custom_image_path\": \"" << escaped_hitmarker_custom_image_path << "\",\n";
    oss << "    \"interface_editor_active_preset_index\": " << settings_.interface_editor_active_preset_index << ",\n";
    oss << "    \"crosshair_enabled\": " << (settings_.crosshair_enabled ? "true" : "false") << ",\n";
    oss << "    \"crosshair_texture_path\": \"" << EscapeJsonString(settings_.crosshair_texture_path) << "\",\n";
    oss << "    \"squaremap_enabled\": " << (settings_.squaremap_enabled ? "true" : "false") << ",\n";
    oss << "    \"quickmap_radar_pack\": \"" << EscapeJsonString(settings_.quickmap_radar_pack) << "\",\n";
    oss << "    \"fist_texture_path\": \"" << EscapeJsonString(settings_.fist_texture_path) << "\",\n";
    oss << "    \"hitsound_outgoing_enabled\": " << (settings_.hitsound_outgoing_enabled ? "true" : "false") << ",\n";
    oss << "    \"hitsound_outgoing_path\": \"" << EscapeJsonString(settings_.hitsound_outgoing_path) << "\",\n";
    oss << "    \"hitsound_outgoing_volume\": " << settings_.hitsound_outgoing_volume << ",\n";
    oss << "    \"hitsound_incoming_enabled\": " << (settings_.hitsound_incoming_enabled ? "true" : "false") << ",\n";
    oss << "    \"hitsound_incoming_path\": \"" << EscapeJsonString(settings_.hitsound_incoming_path) << "\",\n";
    oss << "    \"hitsound_incoming_volume\": " << settings_.hitsound_incoming_volume << ",\n";
    oss << "    \"theme_name\": \"" << EscapeJsonString(NormalizeThemeName(settings_.theme_name)) << "\",\n";
    oss << "    \"theme_custom_accent\": \"" << EscapeJsonString(settings_.theme_custom_accent) << "\",\n";
    oss << "    \"ui_transparency\": " << NormalizeUiTransparency(settings_.ui_transparency) << ",\n";
    oss << "    \"rss_enabled\": " << (settings_.rss_enabled ? "true" : "false") << ",\n";
    oss << "    \"rss_notification_mode\": \"" << ToJsonString(settings_.rss_notification_mode) << "\",\n";
    oss << "    \"rss_feeds\": [";
    bool rssFeedsFirst = true;
    for (const auto& feed : settings_.rss_feeds) {
        if (feed.url.empty()) {
            continue;
        }

        if (!rssFeedsFirst) {
            oss << ",";
        }

        oss << "\n      {"
            << "\"url\": \"" << EscapeJsonString(feed.url) << "\""
            << ", \"label\": \"" << EscapeJsonString(feed.label) << "\""
            << ", \"last_modified\": \"" << EscapeJsonString(feed.lastModified) << "\""
            << ", \"etag\": \"" << EscapeJsonString(feed.etag) << "\""
            << ", \"poll_interval_seconds\": " << feed.pollIntervalSeconds
            << ", \"enabled\": " << (feed.enabled ? "true" : "false")
            << ", \"seen_guids\": [";

        bool seenGuidsFirst = true;
        for (const auto& seenGuid : feed.seen_guids) {
            if (seenGuid.guid.empty()) {
                continue;
            }

            if (!seenGuidsFirst) {
                oss << ",";
            }

            oss << "\n        {\"guid\": \"" << EscapeJsonString(seenGuid.guid)
                << "\", \"seen_at\": " << static_cast<long long>(seenGuid.seenAt) << "}";
            seenGuidsFirst = false;
        }

        if (!seenGuidsFirst) {
            oss << "\n      ";
        }

        oss << "]}";
        rssFeedsFirst = false;
    }

    if (!rssFeedsFirst) {
        oss << "\n    ";
    }

    oss << "],\n";

    // Online feature settings emission
    oss << "    \"online_enabled\": " << (settings_.online_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_enabled\": " << (settings_.online_quick_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_hotkey\": " << settings_.online_quick_overlay_hotkey << ",\n";
    oss << "    \"online_quick_overlay_mode\": \"" << EscapeJsonString(settings_.online_quick_overlay_mode) << "\",\n";
    oss << "    \"online_quick_overlay_show_lspd\": " << (settings_.online_quick_overlay_show_lspd ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_show_army\": " << (settings_.online_quick_overlay_show_army ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_show_fire_department\": " << (settings_.online_quick_overlay_show_fire_department ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_show_zivilisten\": " << (settings_.online_quick_overlay_show_zivilisten ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_show_all_gangs\": " << (settings_.online_quick_overlay_show_all_gangs ? "true" : "false") << ",\n";
    oss << "    \"online_quick_overlay_selected_gangs\": [";
    {
        bool gangsFirst = true;
        for (const auto& gang : settings_.online_quick_overlay_selected_gangs) {
            if (gang.empty()) continue;
            if (!gangsFirst) oss << ", ";
            oss << "\"" << EscapeJsonString(gang) << "\"";
            gangsFirst = false;
        }
    }
    oss << "],\n";
    oss << "    \"online_local_overlay_enabled\": " << (settings_.online_local_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_own_gang_overlay_enabled\": " << (settings_.online_own_gang_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_gangs_overlay_enabled\": " << (settings_.online_gangs_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_overlay_anchor\": \"" << EscapeJsonString(settings_.online_overlay_anchor) << "\",\n";
    oss << "    \"online_local_overlay_anchor\": \"" << EscapeJsonString(settings_.online_local_overlay_anchor) << "\",\n";
    oss << "    \"online_own_gang_overlay_anchor\": \"" << EscapeJsonString(settings_.online_own_gang_overlay_anchor) << "\",\n";
    oss << "    \"online_gangs_overlay_anchor\": \"" << EscapeJsonString(settings_.online_gangs_overlay_anchor) << "\",\n";
    oss << "    \"online_overlay_offset_x\": " << settings_.online_overlay_offset_x << ",\n";
    oss << "    \"online_overlay_offset_y\": " << settings_.online_overlay_offset_y << ",\n";
    oss << "    \"online_overlay_style\": \"" << EscapeJsonString(settings_.online_overlay_style) << "\",\n";
    oss << "    \"online_overlay_opacity\": " << settings_.online_overlay_opacity << ",\n";
    oss << "    \"online_overlay_background_enabled\": " << (settings_.online_overlay_background_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_overlay_scale\": " << settings_.online_overlay_scale << ",\n";
    oss << "    \"online_overlay_custom_text_color_enabled\": " << (settings_.online_overlay_custom_text_color_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_overlay_text_color_r\": " << settings_.online_overlay_text_color_r << ",\n";
    oss << "    \"online_overlay_text_color_g\": " << settings_.online_overlay_text_color_g << ",\n";
    oss << "    \"online_overlay_text_color_b\": " << settings_.online_overlay_text_color_b << ",\n";
    oss << "    \"online_overlay_text_color_a\": " << settings_.online_overlay_text_color_a << ",\n";
    oss << "    \"online_own_gang_overlay_offset_x\": " << settings_.online_own_gang_overlay_offset_x << ",\n";
    oss << "    \"online_own_gang_overlay_offset_y\": " << settings_.online_own_gang_overlay_offset_y << ",\n";
    oss << "    \"online_own_gang_overlay_style\": \"" << EscapeJsonString(settings_.online_own_gang_overlay_style) << "\",\n";
    oss << "    \"online_own_gang_overlay_opacity\": " << settings_.online_own_gang_overlay_opacity << ",\n";
    oss << "    \"online_own_gang_overlay_background_enabled\": " << (settings_.online_own_gang_overlay_background_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_own_gang_overlay_scale\": " << settings_.online_own_gang_overlay_scale << ",\n";
    oss << "    \"online_own_gang_overlay_custom_text_color_enabled\": " << (settings_.online_own_gang_overlay_custom_text_color_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_own_gang_overlay_text_color_r\": " << settings_.online_own_gang_overlay_text_color_r << ",\n";
    oss << "    \"online_own_gang_overlay_text_color_g\": " << settings_.online_own_gang_overlay_text_color_g << ",\n";
    oss << "    \"online_own_gang_overlay_text_color_b\": " << settings_.online_own_gang_overlay_text_color_b << ",\n";
    oss << "    \"online_own_gang_overlay_text_color_a\": " << settings_.online_own_gang_overlay_text_color_a << ",\n";
    oss << "    \"online_wanted_overlay_enabled\": " << (settings_.online_wanted_overlay_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_wanted_overlay_anchor\": \"" << EscapeJsonString(settings_.online_wanted_overlay_anchor) << "\",\n";
    oss << "    \"online_wanted_overlay_offset_x\": " << settings_.online_wanted_overlay_offset_x << ",\n";
    oss << "    \"online_wanted_overlay_offset_y\": " << settings_.online_wanted_overlay_offset_y << ",\n";
    oss << "    \"online_wanted_overlay_style\": \"" << EscapeJsonString(settings_.online_wanted_overlay_style) << "\",\n";
    oss << "    \"online_wanted_overlay_opacity\": " << settings_.online_wanted_overlay_opacity << ",\n";
    oss << "    \"online_wanted_overlay_background_enabled\": " << (settings_.online_wanted_overlay_background_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_wanted_overlay_scale\": " << settings_.online_wanted_overlay_scale << ",\n";
    oss << "    \"online_wanted_overlay_custom_text_color_enabled\": " << (settings_.online_wanted_overlay_custom_text_color_enabled ? "true" : "false") << ",\n";
    oss << "    \"online_wanted_overlay_text_color_r\": " << settings_.online_wanted_overlay_text_color_r << ",\n";
    oss << "    \"online_wanted_overlay_text_color_g\": " << settings_.online_wanted_overlay_text_color_g << ",\n";
    oss << "    \"online_wanted_overlay_text_color_b\": " << settings_.online_wanted_overlay_text_color_b << ",\n";
    oss << "    \"online_wanted_overlay_text_color_a\": " << settings_.online_wanted_overlay_text_color_a << ",\n";
    oss << "    \"online_wanted_overlay_min\": " << settings_.online_wanted_overlay_min << ",\n";
    oss << "    \"online_wanted_overlay_max\": " << settings_.online_wanted_overlay_max << ",\n";
    oss << "    \"online_event_detector_version\": " << settings_.online_event_detector_version << ",\n";
    oss << "    \"online_event_placeholders\": [";
    {
        bool phFirst = true;
        for (const auto& ph : settings_.online_event_placeholders) {
            if (ph.event_type.empty()) continue;
            if (!phFirst) oss << ",";
            oss << "\n      {\"event_type\": \"" << EscapeJsonString(ph.event_type) << "\", \"patterns\": [";
            bool patFirst = true;
            for (const auto& pat : ph.patterns) {
                if (pat.empty()) continue;
                if (!patFirst) oss << ", ";
                oss << "\"" << EscapeJsonString(pat) << "\"";
                patFirst = false;
            }
            oss << "]}";
            phFirst = false;
        }
    }
    if (!settings_.online_event_placeholders.empty()) oss << "\n    ";
    oss << "],\n";

    oss << "    \"hitmarker_saved_presets\": [";
    bool presetsFirst = true;
    for (const auto& preset : settings_.hitmarker_saved_presets) {
        if (!presetsFirst) oss << ",";
        oss << "\n      {"
            << "\"name\": \"" << EscapeJsonString(preset.name) << "\""
            << ", \"shape\": " << static_cast<int>(preset.shape)
            << ", \"color_r\": " << preset.color.r
            << ", \"color_g\": " << preset.color.g
            << ", \"color_b\": " << preset.color.b
            << ", \"color_a\": " << preset.color.a
            << ", \"kill_color_r\": " << preset.killConfirmedColor.r
            << ", \"kill_color_g\": " << preset.killConfirmedColor.g
            << ", \"kill_color_b\": " << preset.killConfirmedColor.b
            << ", \"kill_color_a\": " << preset.killConfirmedColor.a
            << ", \"size\": " << preset.size
            << ", \"opacity\": " << preset.opacity
            << ", \"duration\": " << preset.duration
            << ", \"fade_out\": " << (preset.fadeOut ? "true" : "false")
            << ", \"custom_image_path\": \"" << EscapeJsonString(preset.customImagePath) << "\"}"
            ;
        presetsFirst = false;
    }
    if (!settings_.hitmarker_saved_presets.empty()) oss << "\n    ";
    oss << "],\n";

    oss << "    \"interface_editor_saved_presets\": [";
    bool interfacePresetsFirst = true;
    for (const auto& preset : settings_.interface_editor_saved_presets) {
        if (!interfacePresetsFirst) oss << ",";
        oss << "\n      {"
            << "\"name\": \"" << EscapeJsonString(preset.name) << "\""
            << ", \"show_master_hud\": " << (preset.show_master_hud ? "true" : "false")
            << ", \"show_radar\": " << (preset.show_radar ? "true" : "false")
            << ", \"show_area_name\": " << (preset.show_area_name ? "true" : "false")
            << ", \"show_vehicle_name\": " << (preset.show_vehicle_name ? "true" : "false")
            << ", \"show_clock\": " << (preset.show_clock ? "true" : "false")
            << ", \"show_vital_stats\": " << (preset.show_vital_stats ? "true" : "false")
            << ", \"show_3d_markers\": " << (preset.show_3d_markers ? "true" : "false")
            << ", \"hp_color_r\": " << preset.hp_color_r
            << ", \"hp_color_g\": " << preset.hp_color_g
            << ", \"hp_color_b\": " << preset.hp_color_b
            << ", \"hp_color_a\": " << preset.hp_color_a
            << ", \"armor_color_r\": " << preset.armor_color_r
            << ", \"armor_color_g\": " << preset.armor_color_g
            << ", \"armor_color_b\": " << preset.armor_color_b
            << ", \"armor_color_a\": " << preset.armor_color_a
            << ", \"money_color_r\": " << preset.money_color_r
            << ", \"money_color_g\": " << preset.money_color_g
            << ", \"money_color_b\": " << preset.money_color_b
            << ", \"money_color_a\": " << preset.money_color_a
            << ", \"show_hp_number\": " << (preset.show_hp_number ? "true" : "false")
            << ", \"show_armor_number\": " << (preset.show_armor_number ? "true" : "false")
            << ", \"hp_number_pos_x\": " << preset.hp_number_pos_x
            << ", \"hp_number_pos_y\": " << preset.hp_number_pos_y
            << ", \"armor_number_pos_x\": " << preset.armor_number_pos_x
            << ", \"armor_number_pos_y\": " << preset.armor_number_pos_y
            << ", \"hp_number_scale\": " << preset.hp_number_scale
            << ", \"armor_number_scale\": " << preset.armor_number_scale
            << "}";
        interfacePresetsFirst = false;
    }
    if (!settings_.interface_editor_saved_presets.empty()) oss << "\n    ";
    oss << "],\n";

    oss << "    \"kill_tracker_weapon_stats\": [";
    bool first = true;
    for (const auto& ws : settings_.kill_tracker_weapon_stats) {
        if (ws.totalDamage <= 0.0f && ws.killCount <= 0) continue;
        if (!first) oss << ",";
        oss << "\n      {\"weaponId\": " << ws.weaponId
            << ", \"totalDamage\": " << ws.totalDamage
            << ", \"killCount\": " << ws.killCount << "}";
        first = false;
    }
    if (!first) oss << "\n    ";
    oss << "],\n";

    oss << "    \"kill_tracker_recent_kills\": [";
    first = true;
    for (const auto& kill : settings_.kill_tracker_recent_kills) {
        if (kill.victim.empty()) continue;
        if (!first) oss << ",";
        std::string escaped_victim = kill.victim;
        size_t epos = 0;
        while ((epos = escaped_victim.find('"', epos)) != std::string::npos) {
            escaped_victim.insert(epos, "\\");
            epos += 2;
        }
        oss << "\n      {\"victim\": \"" << escaped_victim
            << "\", \"weaponId\": " << kill.weaponId
            << ", \"timestamp\": " << static_cast<long long>(kill.timestamp)
            << ", \"damageDealt\": " << kill.damageDealt << "}";
        first = false;
    }
    if (!first) oss << "\n    ";
    oss << "],\n";

    oss << "    \"kill_tracker_kill_count\": " << settings_.kill_tracker_kill_count << ",\n";

    oss << "    \"kill_tracker_death_count\": " << settings_.kill_tracker_death_count << ",\n";

    oss << "    \"kill_tracker_recent_deaths\": [";
    first = true;
    for (const auto& death : settings_.kill_tracker_recent_deaths) {
        if (!first) oss << ",";
        std::string escaped_killer = death.killer;
        size_t epos = 0;
        while ((epos = escaped_killer.find('"', epos)) != std::string::npos) {
            escaped_killer.insert(epos, "\\");
            epos += 2;
        }
        oss << "\n      {\"killer\": \"" << escaped_killer
            << "\", \"weaponId\": " << death.weaponId
            << ", \"timestamp\": " << static_cast<long long>(death.timestamp) << "}";
        first = false;
    }
    if (!first) oss << "\n    ";
    oss << "]\n";

    oss << "  }";
    return oss.str();
}

}
