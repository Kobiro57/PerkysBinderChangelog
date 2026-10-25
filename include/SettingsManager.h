#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>
#include "KillTrackerTypes.h"
#include "Hitmarker.h"
#include "InterfaceEditor.h"
#include "RssTypes.h"
#include "ServerStatsSnapshot.h"

namespace keybinder {

constexpr int VK_M = 'M';

struct WeaponStatsData {
    int weaponId;
    float totalDamage;
    int killCount;
};

struct KillEventData {
    std::string victim;
    int weaponId;
    std::time_t timestamp;
    float damageDealt;
};

struct DeathEventData {
    std::string killer;
    int weaponId;
    std::time_t timestamp;
};

struct OnlineEventPlaceholder {
    std::string event_type;
    std::vector<std::string> patterns;
};

struct OnlinePlayerMarker {
    std::string name;
    std::string affiliation;
    bool friendly = false;
};

struct WeaponIconSelection {
    int weaponId = -1;
    std::string path;
    bool enabled = true;
};

struct Settings {
    float dpi_scale = 1.0f;
    bool auto_detect_dpi = true;
    std::string ui_open_command_primary = "kb";
    std::string ui_open_command_secondary = "keybinder";
    int ui_toggle_hotkey = VK_F2;
    float ui_window_width = 0.0f;
    float ui_window_height = 0.0f;
    bool kill_tracker_enabled = false;
    bool kill_tracker_count_kills = true;
    bool kill_tracker_count_deaths = true;
    bool kill_tracker_local_kill_message_enabled = true;
    bool kill_tracker_local_death_message_enabled = true;
    bool kill_tracker_server_kill_message_enabled = false;
    bool kill_tracker_server_death_message_enabled = false;
    bool kill_tracker_track_damage = true;
    std::string kill_tracker_local_kill_message_template = "{33CCFF}[NL-BINDER]{FFFFFF} Kill in {city}, {bezirk}. Meine Kills: {total_kills}";
    std::string kill_tracker_local_death_message_template = "{33CCFF}[NL-BINDER]{FFFFFF} Du bist gestorben. Deine aktuelle Tode: {total_deaths}";
    std::string kill_tracker_server_kill_message_template = "Kill in {city}, {bezirk}. Meine Kills: {total_kills}";
    std::string kill_tracker_server_death_message_template = "Tode: {total_deaths}";
    bool kill_tracker_allow_manual_edits = false;
    bool auto_hudscalefix = true;
    bool auto_engine_start = false;
    bool damage_log_enabled = false;
    bool reconnect_use_nova_launcher = true;
    std::string reconnect_nova_launcher_path;
    std::string reconnect_openmp_launcher_path;

    bool quickmap_enabled = true;
    int quickmap_hotkey = VK_M;
    std::string quickmap_mode = "toggle";
    std::string quickmap_map_variant = "classic";
    float quickmap_map_transparency = 1.0f;
    bool quickmap_auto_zoom = false;
    bool quickmap_follow_player = true;
    bool quickmap_focus_marker = true;
    std::string quickmap_zoom_level = "far";
    std::string quickmap_last_zoom_level = "far";
    float quickmap_gangzone_outline_thickness = 2.5f;
    bool quickmap_gangzone_flash_enabled = true;

    bool speedometer_enabled = false;
    std::string speedometer_style = "analog_arc";

    bool hitmarker_enabled = false;
    int hitmarker_active_preset_index = 0;
    float hitmarker_size = 20.0f;
    float hitmarker_color_r = 1.0f;
    float hitmarker_color_g = 1.0f;
    float hitmarker_color_b = 1.0f;
    float hitmarker_color_a = 1.0f;
    float hitmarker_kill_color_r = 1.0f;
    float hitmarker_kill_color_g = 0.0f;
    float hitmarker_kill_color_b = 0.0f;
    float hitmarker_kill_color_a = 1.0f;
    float hitmarker_opacity = 1.0f;
    float hitmarker_duration = 0.5f;
    bool hitmarker_fade_out = true;
    int hitmarker_shape = 0;
    std::string hitmarker_custom_image_path = "";
    std::vector<HitmarkerPreset> hitmarker_saved_presets;

    int interface_editor_active_preset_index = 0;
    std::vector<InterfacePreset> interface_editor_saved_presets;

    bool crosshair_enabled = false;
    std::string crosshair_texture_path = "";

    bool weapon_icons_enabled = false;
    std::vector<WeaponIconSelection> weapon_icon_selections;

    bool squaremap_enabled = false;
    std::string quickmap_radar_pack = "radar1";
    std::string fist_texture_path = "";

    bool hitsound_outgoing_enabled = true;
    std::string hitsound_outgoing_path = "";
    float hitsound_outgoing_volume = 0.08f;
    bool hitsound_incoming_enabled = true;
    std::string hitsound_incoming_path = "";
    float hitsound_incoming_volume = 0.08f;

    std::string theme_name = "Dark";
    std::string theme_custom_accent = "";
    float ui_transparency = 0.0f;
    bool rss_enabled = false;
    RssNotificationMode rss_notification_mode = RssNotificationMode::Toast;
    std::vector<RssFeedConfig> rss_feeds;

    // Online feature settings
    std::vector<OnlinePlayerMarker> online_player_markers;
    bool online_friends_overlay_enabled = false;
    bool online_enemies_overlay_enabled = false;
    std::string online_friends_overlay_anchor = "top_left";
    float online_friends_overlay_offset_x = 12.0f;
    float online_friends_overlay_offset_y = 250.0f;
    std::string online_enemies_overlay_anchor = "top_left";
    float online_enemies_overlay_offset_x = 12.0f;
    float online_enemies_overlay_offset_y = 370.0f;
    bool online_location_overlay_enabled = false;
    std::string online_location_overlay_anchor = "top_left";
    float online_location_overlay_offset_x = 12.0f;
    float online_location_overlay_offset_y = 45.0f;
    bool online_transport_overlay_enabled = false;
    std::string online_transport_overlay_anchor = "top_left";
    float online_transport_overlay_offset_x = 12.0f;
    float online_transport_overlay_offset_y = 115.0f;
    bool online_pedslots_overlay_enabled = false;
    std::string online_pedslots_overlay_anchor = "top_left";
    float online_pedslots_overlay_offset_x = 12.0f;
    float online_pedslots_overlay_offset_y = 185.0f;
    bool online_enabled = true;
    bool online_quick_overlay_enabled = true;
    int online_quick_overlay_hotkey = 0;
    std::string online_quick_overlay_mode = "toggle";
    bool online_quick_overlay_show_lspd = true;
    bool online_quick_overlay_show_army = true;
    bool online_quick_overlay_show_fire_department = true;
    bool online_quick_overlay_show_rcb = true;
    bool online_quick_overlay_show_zivilisten = true;
    bool online_quick_overlay_show_all_gangs = true;
    std::vector<std::string> online_quick_overlay_selected_gangs;
    bool online_local_overlay_enabled = false;
    bool online_own_gang_overlay_enabled = false;
    bool online_gangs_overlay_enabled = false;
    std::string online_overlay_anchor = "top_right";
    // Per-overlay anchors (auto-updated when dragged in move-mode based on the
    // viewport quadrant the panel is dropped in). Fall back to
    // `online_overlay_anchor` if missing from the persisted JSON so legacy
    // settings keep their global behavior.
    std::string online_local_overlay_anchor = "top_right";
    std::string online_own_gang_overlay_anchor = "top_right";
    std::string online_gangs_overlay_anchor = "top_right";
    float online_overlay_offset_x = 12.0f;
    float online_overlay_offset_y = 12.0f;
    std::string online_overlay_style = "card";
    float online_overlay_opacity = 0.85f;
    bool online_overlay_background_enabled = true;
    float online_overlay_scale = 1.0f;
    bool online_overlay_custom_text_color_enabled = false;
    float online_overlay_text_color_r = 1.0f;
    float online_overlay_text_color_g = 1.0f;
    float online_overlay_text_color_b = 1.0f;
    float online_overlay_text_color_a = 1.0f;
    float online_own_gang_overlay_offset_x = 12.0f;
    float online_own_gang_overlay_offset_y = 12.0f;
    std::string online_own_gang_overlay_style = "card";
    float online_own_gang_overlay_opacity = 0.85f;
    bool online_own_gang_overlay_background_enabled = true;
    float online_own_gang_overlay_scale = 1.0f;
    bool online_own_gang_overlay_custom_text_color_enabled = false;
    float online_own_gang_overlay_text_color_r = 1.0f;
    float online_own_gang_overlay_text_color_g = 1.0f;
    float online_own_gang_overlay_text_color_b = 1.0f;
    float online_own_gang_overlay_text_color_a = 1.0f;
    int online_event_detector_version = 1;
    std::vector<OnlineEventPlaceholder> online_event_placeholders;

    // Wanted overlay — shows players whose wanted level falls in [min, max].
    bool online_wanted_overlay_enabled = false;
    std::string online_wanted_overlay_anchor = "top_right";
    float online_wanted_overlay_offset_x = 12.0f;
    float online_wanted_overlay_offset_y = 12.0f;
    std::string online_wanted_overlay_style = "card";
    float online_wanted_overlay_opacity = 0.85f;
    bool online_wanted_overlay_background_enabled = true;
    float online_wanted_overlay_scale = 1.0f;
    bool online_wanted_overlay_custom_text_color_enabled = false;
    float online_wanted_overlay_text_color_r = 1.0f;
    float online_wanted_overlay_text_color_g = 1.0f;
    float online_wanted_overlay_text_color_b = 1.0f;
    float online_wanted_overlay_text_color_a = 1.0f;
    int online_wanted_overlay_min = 1;   // inclusive lower bound
    int online_wanted_overlay_max = 0;   // inclusive upper bound; 0 means "no upper limit"
    std::vector<WeaponStatsData> kill_tracker_weapon_stats;
    std::vector<KillEventData> kill_tracker_recent_kills;
    std::vector<DeathEventData> kill_tracker_recent_deaths;
    int kill_tracker_kill_count = 0;
    int kill_tracker_death_count = 0;
    ServerStatsSnapshot synchronized_player_stats;
};

class SettingsManager {
public:
    static constexpr int SETTINGS_VERSION = 1;

    SettingsManager();

    bool LoadFromJson(const std::string& json_content);

    std::string GetSettingsJson() const;

    const Settings& Get() const { return settings_; }
    Settings& GetMutable() { return settings_; }

    float GetDpiScale() const { return settings_.dpi_scale; }
    void SetDpiScale(float scale);

    static float ClampScale(float scale);

    void SetWeaponStats(const std::vector<WeaponStatsData>& stats) { settings_.kill_tracker_weapon_stats = stats; }
    const std::vector<WeaponStatsData>& GetWeaponStats() const { return settings_.kill_tracker_weapon_stats; }

    void SetRecentKills(const std::vector<KillEventData>& kills) { settings_.kill_tracker_recent_kills = kills; }
    const std::vector<KillEventData>& GetRecentKills() const { return settings_.kill_tracker_recent_kills; }

    void SetRecentDeaths(const std::vector<DeathEventData>& deaths) { settings_.kill_tracker_recent_deaths = deaths; }
    const std::vector<DeathEventData>& GetRecentDeaths() const { return settings_.kill_tracker_recent_deaths; }
    void SetKillCount(int count) { settings_.kill_tracker_kill_count = count; }
    int GetKillCount() const { return settings_.kill_tracker_kill_count; }
    void SetDeathCount(int count) { settings_.kill_tracker_death_count = count; }
    int GetDeathCount() const { return settings_.kill_tracker_death_count; }

    void SetRssFeeds(const std::vector<RssFeedConfig>& feeds) { settings_.rss_feeds = feeds; }
    const std::vector<RssFeedConfig>& GetRssFeeds() const { return settings_.rss_feeds; }
private:
    Settings settings_;
};

} // namespace keybinder
