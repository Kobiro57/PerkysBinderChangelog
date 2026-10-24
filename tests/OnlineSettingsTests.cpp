//
//   NLBinder - Online settings roundtrip and normalization tests
//
#ifndef KEYBINDER_TESTING
#define KEYBINDER_TESTING
#endif

#include "../include/SettingsManager.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace keybinder;

namespace {

struct TestState { int passed = 0; int failed = 0; std::string error; };
TestState g_state;

#define ASSERT_TRUE(condition) do { if (!(condition)) { g_state.error = "Assertion failed: " #condition; return false; } ++g_state.passed; } while (0)
#define ASSERT_EQ(actual, expected) do { if (!((actual) == (expected))) { g_state.error = "Assertion failed: " #actual " != " #expected; return false; } ++g_state.passed; } while (0)
#define ASSERT_NEAR(actual, expected, epsilon) do { if (std::fabs((actual) - (expected)) > (epsilon)) { g_state.error = "Assertion failed: " #actual " ~= " #expected; return false; } ++g_state.passed; } while (0)
#define RUN_TEST(test_name) do { g_state.error.clear(); if (test_name()) { std::printf("[PASS] %s\n", #test_name); } else { ++g_state.failed; std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); } } while (0)

std::string BuildOnlineSettingsJson(
    bool enabled = true,
    bool quick_enabled = true,
    int hotkey = 0,
    const std::string& mode = "toggle",
    bool show_lspd = true,
    bool show_army = true,
    bool show_fire = true,
    bool show_ziv = true,
    bool show_all_gangs = true,
    const std::string& gangs = "[]",
    bool local_enabled = false,
    bool gangs_enabled = false,
    const std::string& anchor = "top_right",
    float offset_x = 12.0f,
    float offset_y = 12.0f,
    const std::string& style = "card",
    float opacity = 0.85f,
    int timeout = 180,
    int detector_ver = 1,
    const std::string& placeholders = "",
    bool own_gang_enabled = false
) {
    std::string json = "{\"online_enabled\":" + std::string(enabled ? "true" : "false");
    json += ",\"online_quick_overlay_enabled\":" + std::string(quick_enabled ? "true" : "false");
    json += ",\"online_quick_overlay_hotkey\":" + std::to_string(hotkey);
    json += ",\"online_quick_overlay_mode\":\"" + mode + "\"";
    json += ",\"online_quick_overlay_show_lspd\":" + std::string(show_lspd ? "true" : "false");
    json += ",\"online_quick_overlay_show_army\":" + std::string(show_army ? "true" : "false");
    json += ",\"online_quick_overlay_show_fire_department\":" + std::string(show_fire ? "true" : "false");
    json += ",\"online_quick_overlay_show_zivilisten\":" + std::string(show_ziv ? "true" : "false");
    json += ",\"online_quick_overlay_show_all_gangs\":" + std::string(show_all_gangs ? "true" : "false");
    json += ",\"online_quick_overlay_selected_gangs\":" + gangs + "";
    json += ",\"online_local_overlay_enabled\":" + std::string(local_enabled ? "true" : "false");
    json += ",\"online_gangs_overlay_enabled\":" + std::string(gangs_enabled ? "true" : "false");
    json += ",\"online_own_gang_overlay_enabled\":" + std::string(own_gang_enabled ? "true" : "false");
    json += ",\"online_overlay_anchor\":\"" + anchor + "\"";
    json += ",\"online_overlay_offset_x\":" + std::to_string(offset_x);
    json += ",\"online_overlay_offset_y\":" + std::to_string(offset_y);
    json += ",\"online_overlay_style\":\"" + style + "\"";
    json += ",\"online_overlay_opacity\":" + std::to_string(opacity);
    json += ",\"online_event_detector_version\":" + std::to_string(detector_ver);
    (void)timeout;
    if (!placeholders.empty()) {
        json += ",\"online_event_placeholders\":" + placeholders;
    }
    json += "}";
    return json;
}

} // namespace

bool TestOnlineDefaultsWhenEmpty() {
    SettingsManager mgr;
    mgr.LoadFromJson("{}");
    const auto& s = mgr.Get();
    ASSERT_TRUE(s.online_enabled);
    ASSERT_TRUE(s.online_quick_overlay_enabled);
    ASSERT_EQ(s.online_quick_overlay_hotkey, 0);
    ASSERT_EQ(s.online_quick_overlay_mode, "toggle");
    ASSERT_TRUE(s.online_quick_overlay_show_lspd);
    ASSERT_TRUE(s.online_quick_overlay_show_army);
    ASSERT_TRUE(s.online_quick_overlay_show_fire_department);
    ASSERT_TRUE(s.online_quick_overlay_show_zivilisten);
    ASSERT_TRUE(s.online_quick_overlay_show_all_gangs);
    ASSERT_TRUE(s.online_quick_overlay_selected_gangs.empty());
    ASSERT_TRUE(!s.online_local_overlay_enabled);
    ASSERT_TRUE(!s.online_own_gang_overlay_enabled);
    ASSERT_TRUE(!s.online_gangs_overlay_enabled);
    ASSERT_EQ(s.online_overlay_anchor, "top_right");
    ASSERT_EQ(s.online_overlay_offset_x, 12.0f);
    ASSERT_EQ(s.online_overlay_offset_y, 12.0f);
    ASSERT_EQ(s.online_overlay_style, "card");
    ASSERT_EQ(s.online_overlay_opacity, 0.85f);
    ASSERT_TRUE(s.online_overlay_background_enabled);
    ASSERT_NEAR(s.online_overlay_scale, 1.0f, 0.001f);
    ASSERT_TRUE(!s.online_overlay_custom_text_color_enabled);
    ASSERT_NEAR(s.online_overlay_text_color_r, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_g, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_b, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_a, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_offset_x, 12.0f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_offset_y, 12.0f, 0.001f);
    ASSERT_EQ(s.online_own_gang_overlay_style, "card");
    ASSERT_NEAR(s.online_own_gang_overlay_opacity, 0.85f, 0.001f);
    ASSERT_TRUE(s.online_own_gang_overlay_background_enabled);
    ASSERT_NEAR(s.online_own_gang_overlay_scale, 1.0f, 0.001f);
    ASSERT_TRUE(!s.online_own_gang_overlay_custom_text_color_enabled);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_r, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_g, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_b, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_a, 1.0f, 0.001f);
    ASSERT_EQ(s.online_event_detector_version, 1);
    ASSERT_EQ(s.online_event_placeholders.size(), static_cast<size_t>(3));
    return true;
}

bool TestOnlineRoundtripHappyPath() {
    SettingsManager mgr;
    std::string json = BuildOnlineSettingsJson(
        false, false, 5, "hold", false, false, false, false, false,
        R"(["Grove","Ballas"])", false, false, "bottom_left",
        20.0f, 30.0f, "minimal", 0.5f, 300, 2,
        R"([{"event_type":"gangwar","patterns":["test"]}])");
    mgr.LoadFromJson(json);
    std::string saved = mgr.GetSettingsJson();
    SettingsManager mgr2;
    mgr2.LoadFromJson(saved);
    const auto& s = mgr2.Get();
    ASSERT_TRUE(!s.online_enabled);
    ASSERT_TRUE(!s.online_quick_overlay_enabled);
    ASSERT_EQ(s.online_quick_overlay_hotkey, 5);
    ASSERT_EQ(s.online_quick_overlay_mode, "hold");
    ASSERT_TRUE(!s.online_quick_overlay_show_lspd);
    ASSERT_EQ(s.online_overlay_anchor, "bottom_left");
    ASSERT_EQ(s.online_overlay_offset_x, 20.0f);
    ASSERT_EQ(s.online_overlay_offset_y, 30.0f);
    ASSERT_EQ(s.online_overlay_style, "minimal");
    ASSERT_EQ(s.online_overlay_opacity, 0.5f);
    ASSERT_EQ(s.online_event_detector_version, 2);
    return true;
}

bool TestOnlineRoundtripPreservesAllFields() {
    SettingsManager mgr;
    Settings& settings = mgr.GetMutable();
    settings.online_enabled = false;
    settings.online_quick_overlay_enabled = false;
    settings.online_quick_overlay_hotkey = 'O';
    settings.online_quick_overlay_mode = "hold";
    settings.online_quick_overlay_show_lspd = false;
    settings.online_quick_overlay_show_army = false;
    settings.online_quick_overlay_show_fire_department = false;
    settings.online_quick_overlay_show_zivilisten = false;
    settings.online_quick_overlay_show_all_gangs = false;
    settings.online_quick_overlay_selected_gangs = {"Grove Street Families", "Ballas"};
    settings.online_local_overlay_enabled = false;
    settings.online_own_gang_overlay_enabled = false;
    settings.online_gangs_overlay_enabled = false;
    settings.online_overlay_anchor = "bottom_left";
    settings.online_overlay_offset_x = 18.5f;
    settings.online_overlay_offset_y = -7.25f;
    settings.online_overlay_style = "dense";
    settings.online_overlay_opacity = 0.65f;
    settings.online_overlay_background_enabled = false;
    settings.online_overlay_scale = 1.25f;
    settings.online_overlay_custom_text_color_enabled = true;
    settings.online_overlay_text_color_r = 0.2f;
    settings.online_overlay_text_color_g = 0.4f;
    settings.online_overlay_text_color_b = 0.6f;
    settings.online_overlay_text_color_a = 0.8f;
    settings.online_own_gang_overlay_offset_x = 41.0f;
    settings.online_own_gang_overlay_offset_y = 52.0f;
    settings.online_own_gang_overlay_style = "minimal";
    settings.online_own_gang_overlay_opacity = 0.55f;
    settings.online_own_gang_overlay_background_enabled = false;
    settings.online_own_gang_overlay_scale = 1.30f;
    settings.online_own_gang_overlay_custom_text_color_enabled = true;
    settings.online_own_gang_overlay_text_color_r = 0.1f;
    settings.online_own_gang_overlay_text_color_g = 0.2f;
    settings.online_own_gang_overlay_text_color_b = 0.3f;
    settings.online_own_gang_overlay_text_color_a = 0.4f;
    settings.online_event_detector_version = 4;
    settings.online_event_placeholders = {
        {"activation", {"Gangwar gestartet", "Takeover \"Alpha\""}},
        {"refresh", {"Ballas \\\\ halten", "weiter!"}},
        {"deactivation", {"Angriff beendet"}},
    };

    const std::string saved = mgr.GetSettingsJson();

    SettingsManager reloaded;
    reloaded.LoadFromJson(saved);
    const Settings& s = reloaded.Get();

    ASSERT_TRUE(!s.online_enabled);
    ASSERT_TRUE(!s.online_quick_overlay_enabled);
    ASSERT_EQ(s.online_quick_overlay_hotkey, static_cast<int>('O'));
    ASSERT_EQ(s.online_quick_overlay_mode, "hold");
    ASSERT_TRUE(!s.online_quick_overlay_show_lspd);
    ASSERT_TRUE(!s.online_quick_overlay_show_army);
    ASSERT_TRUE(!s.online_quick_overlay_show_fire_department);
    ASSERT_TRUE(!s.online_quick_overlay_show_zivilisten);
    ASSERT_TRUE(!s.online_quick_overlay_show_all_gangs);
    ASSERT_EQ(s.online_quick_overlay_selected_gangs.size(), static_cast<size_t>(2));
    ASSERT_EQ(s.online_quick_overlay_selected_gangs[0], "Grove Street Families");
    ASSERT_EQ(s.online_quick_overlay_selected_gangs[1], "Ballas");
    ASSERT_TRUE(!s.online_local_overlay_enabled);
    ASSERT_TRUE(!s.online_own_gang_overlay_enabled);
    ASSERT_TRUE(!s.online_gangs_overlay_enabled);
    ASSERT_EQ(s.online_overlay_anchor, "bottom_left");
    ASSERT_NEAR(s.online_overlay_offset_x, 18.5f, 0.001f);
    ASSERT_NEAR(s.online_overlay_offset_y, -7.25f, 0.001f);
    ASSERT_EQ(s.online_overlay_style, "dense");
    ASSERT_NEAR(s.online_overlay_opacity, 0.65f, 0.001f);
    ASSERT_TRUE(!s.online_overlay_background_enabled);
    ASSERT_NEAR(s.online_overlay_scale, 1.25f, 0.001f);
    ASSERT_TRUE(s.online_overlay_custom_text_color_enabled);
    ASSERT_NEAR(s.online_overlay_text_color_r, 0.2f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_g, 0.4f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_b, 0.6f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_a, 0.8f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_offset_x, 41.0f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_offset_y, 52.0f, 0.001f);
    ASSERT_EQ(s.online_own_gang_overlay_style, "minimal");
    ASSERT_NEAR(s.online_own_gang_overlay_opacity, 0.55f, 0.001f);
    ASSERT_TRUE(!s.online_own_gang_overlay_background_enabled);
    ASSERT_NEAR(s.online_own_gang_overlay_scale, 1.30f, 0.001f);
    ASSERT_TRUE(s.online_own_gang_overlay_custom_text_color_enabled);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_r, 0.1f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_g, 0.2f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_b, 0.3f, 0.001f);
    ASSERT_NEAR(s.online_own_gang_overlay_text_color_a, 0.4f, 0.001f);
    ASSERT_EQ(s.online_event_detector_version, 4);
    ASSERT_EQ(s.online_event_placeholders.size(), static_cast<size_t>(3));
    ASSERT_EQ(s.online_event_placeholders[0].event_type, "activation");
    ASSERT_EQ(s.online_event_placeholders[0].patterns.size(), static_cast<size_t>(2));
    ASSERT_EQ(s.online_event_placeholders[0].patterns[0], "Gangwar gestartet");
    ASSERT_EQ(s.online_event_placeholders[0].patterns[1], "Takeover \"Alpha\"");
    ASSERT_EQ(s.online_event_placeholders[1].event_type, "refresh");
    ASSERT_EQ(s.online_event_placeholders[1].patterns.size(), static_cast<size_t>(2));
    ASSERT_EQ(s.online_event_placeholders[1].patterns[0], "Ballas \\\\ halten");
    ASSERT_EQ(s.online_event_placeholders[1].patterns[1], "weiter!");
    ASSERT_EQ(s.online_event_placeholders[2].event_type, "deactivation");
    ASSERT_EQ(s.online_event_placeholders[2].patterns.size(), static_cast<size_t>(1));
    ASSERT_EQ(s.online_event_placeholders[2].patterns[0], "Angriff beendet");
    return true;
}

bool TestOnlineRoundtripDefaults() {
    SettingsManager mgr;
    std::string json = BuildOnlineSettingsJson();
    mgr.LoadFromJson(json);
    std::string saved = mgr.GetSettingsJson();
    SettingsManager mgr2;
    mgr2.LoadFromJson(saved);
    const auto& s = mgr2.Get();
    ASSERT_TRUE(s.online_enabled);
    ASSERT_EQ(s.online_quick_overlay_mode, "toggle");
    ASSERT_EQ(s.online_overlay_anchor, "top_right");
    ASSERT_EQ(s.online_overlay_style, "card");
    ASSERT_EQ(s.online_overlay_opacity, 0.85f);
    ASSERT_TRUE(s.online_overlay_background_enabled);
    ASSERT_NEAR(s.online_overlay_scale, 1.0f, 0.001f);
    ASSERT_TRUE(!s.online_overlay_custom_text_color_enabled);
    ASSERT_TRUE(!s.online_local_overlay_enabled);
    ASSERT_TRUE(!s.online_own_gang_overlay_enabled);
    ASSERT_TRUE(!s.online_gangs_overlay_enabled);
    ASSERT_EQ(s.online_own_gang_overlay_style, "card");
    ASSERT_NEAR(s.online_own_gang_overlay_opacity, 0.85f, 0.001f);
    return true;
}

bool TestNormalizeInvalidMode() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"invalid_mode"));
    ASSERT_EQ(mgr.Get().online_quick_overlay_mode, "toggle");
    return true;
}

bool TestNormalizeInvalidAnchor() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                            true,true,"mid_center"));
    ASSERT_EQ(mgr.Get().online_overlay_anchor, "top_right");
    return true;
}

bool TestNormalizeInvalidStyle() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                            true,true,"top_right",12.0f,12.0f,"fancy"));
    ASSERT_EQ(mgr.Get().online_overlay_style, "card");
    return true;
}

bool TestNormalizeOpacityLow() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                            true,true,"top_right",12.0f,12.0f,"card",0.1f));
    ASSERT_EQ(mgr.Get().online_overlay_opacity, 0.35f);
    return true;
}

bool TestNormalizeOpacityHigh() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                            true,true,"top_right",12.0f,12.0f,"card",1.5f));
    ASSERT_EQ(mgr.Get().online_overlay_opacity, 1.0f);
    return true;
}

bool TestNormalizeValidHoldMode() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"hold"));
    ASSERT_EQ(mgr.Get().online_quick_overlay_mode, "hold");
    return true;
}

bool TestNormalizeAllValidAnchors() {
    const char* anchors[] = {"top_left", "top_right", "bottom_left", "bottom_right", "center"};
    for (const char* a : anchors) {
        SettingsManager mgr;
        mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                                true,true,a));
        ASSERT_EQ(mgr.Get().online_overlay_anchor, std::string(a));
    }
    return true;
}

bool TestNormalizeAllValidStyles() {
    const char* styles[] = {"card", "minimal", "dense"};
    for (const char* st : styles) {
        SettingsManager mgr;
        mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                                true,true,"top_right",12.0f,12.0f,st));
        ASSERT_EQ(mgr.Get().online_overlay_style, std::string(st));
    }
    return true;
}

bool TestNormalizePersistentOverlayStyling() {
    SettingsManager mgr;
    mgr.LoadFromJson(R"({"online_overlay_background_enabled":false,"online_overlay_scale":2.0,"online_overlay_custom_text_color_enabled":true,"online_overlay_text_color_r":-0.5,"online_overlay_text_color_g":0.25,"online_overlay_text_color_b":1.5,"online_overlay_text_color_a":0.75})");
    const Settings& s = mgr.Get();
    ASSERT_TRUE(!s.online_overlay_background_enabled);
    ASSERT_NEAR(s.online_overlay_scale, 1.50f, 0.001f);
    ASSERT_TRUE(s.online_overlay_custom_text_color_enabled);
    ASSERT_NEAR(s.online_overlay_text_color_r, 0.0f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_g, 0.25f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_b, 1.0f, 0.001f);
    ASSERT_NEAR(s.online_overlay_text_color_a, 0.75f, 0.001f);
    mgr.LoadFromJson(R"({"online_own_gang_overlay_scale":0.1,"online_own_gang_overlay_opacity":2.0,"online_own_gang_overlay_text_color_r":2.0,"online_own_gang_overlay_text_color_g":-1.0})");
    const Settings& own = mgr.Get();
    ASSERT_NEAR(own.online_own_gang_overlay_scale, 0.75f, 0.001f);
    ASSERT_NEAR(own.online_own_gang_overlay_opacity, 1.0f, 0.001f);
    ASSERT_NEAR(own.online_own_gang_overlay_text_color_r, 1.0f, 0.001f);
    ASSERT_NEAR(own.online_own_gang_overlay_text_color_g, 0.0f, 0.001f);
    return true;
}

bool TestGangDeduplication() {
    SettingsManager mgr;
    // Build the JSON with gangs array inline to avoid helper issues
    std::string json = R"({"online_enabled":true,"online_quick_overlay_enabled":true,"online_quick_overlay_hotkey":0,"online_quick_overlay_mode":"toggle","online_quick_overlay_show_lspd":true,"online_quick_overlay_show_army":true,"online_quick_overlay_show_fire_department":true,"online_quick_overlay_show_zivilisten":true,"online_quick_overlay_show_all_gangs":true,"online_quick_overlay_selected_gangs":["Grove","Ballas","Grove",""],"online_local_overlay_enabled":true,"online_gangs_overlay_enabled":true,"online_overlay_anchor":"top_right","online_overlay_offset_x":12.0,"online_overlay_offset_y":12.0,"online_overlay_style":"card","online_overlay_opacity":0.85,"online_event_detector_version":1})";
    mgr.LoadFromJson(json);
    const auto& gangs = mgr.Get().online_quick_overlay_selected_gangs;
    ASSERT_EQ(gangs.size(), static_cast<size_t>(2));
    ASSERT_EQ(gangs[0], "Grove");
    ASSERT_EQ(gangs[1], "Ballas");
    return true;
}

bool TestGangsOverlaySettingRoundtrips() {
    SettingsManager mgr;
    mgr.LoadFromJson(BuildOnlineSettingsJson(true,true,0,"toggle",true,true,true,true,true,"[]",
                                            true,false));
    ASSERT_TRUE(!mgr.Get().online_gangs_overlay_enabled);
    ASSERT_TRUE(mgr.GetSettingsJson().find("\"online_gangs_overlay_enabled\": false") != std::string::npos);
    return true;
}

bool TestEventPlaceholdersPreserved() {
    SettingsManager mgr;
    const auto& defaults = mgr.Get().online_event_placeholders;
    ASSERT_EQ(defaults.size(), static_cast<size_t>(3));
    ASSERT_EQ(defaults[0].event_type, "gangwar");
    ASSERT_EQ(defaults[1].event_type, "revierkampf");
    ASSERT_EQ(defaults[2].event_type, "takeover");
    return true;
}

bool TestEmptyEventPlaceholderArrayFallsBackToDefaults() {
    SettingsManager mgr;
    mgr.LoadFromJson(R"({"online_event_placeholders":[]})");
    const auto& placeholders = mgr.Get().online_event_placeholders;
    ASSERT_EQ(placeholders.size(), static_cast<size_t>(3));
    ASSERT_EQ(placeholders[0].event_type, "gangwar");
    ASSERT_EQ(placeholders[1].event_type, "revierkampf");
    ASSERT_EQ(placeholders[2].event_type, "takeover");
    return true;
}

bool TestWantedOverlaySettingsRoundtripAndNormalizeRange() {
    SettingsManager mgr;
    Settings& s = mgr.GetMutable();
    s.online_wanted_overlay_enabled = true;
    s.online_wanted_overlay_anchor = "bottom_left";
    s.online_wanted_overlay_style = "dense";
    s.online_wanted_overlay_opacity = 0.7f;
    s.online_wanted_overlay_background_enabled = false;
    s.online_wanted_overlay_scale = 1.2f;
    s.online_wanted_overlay_custom_text_color_enabled = true;
    s.online_wanted_overlay_text_color_r = 0.3f;
    s.online_wanted_overlay_text_color_g = 0.4f;
    s.online_wanted_overlay_text_color_b = 0.5f;
    s.online_wanted_overlay_text_color_a = 0.6f;
    s.online_wanted_overlay_offset_x = 25.0f;
    s.online_wanted_overlay_offset_y = 35.0f;
    s.online_wanted_overlay_min = 100;
    s.online_wanted_overlay_max = 200;

    const std::string saved = mgr.GetSettingsJson();

    // Verify JSON contains the new fields.
    ASSERT_TRUE(saved.find("\"online_wanted_overlay_enabled\": true") != std::string::npos);
    ASSERT_TRUE(saved.find("\"online_wanted_overlay_min\": 100") != std::string::npos);
    ASSERT_TRUE(saved.find("\"online_wanted_overlay_max\": 200") != std::string::npos);

    SettingsManager reloaded;
    reloaded.LoadFromJson(saved);
    const Settings& r = reloaded.Get();
    ASSERT_TRUE(r.online_wanted_overlay_enabled);
    ASSERT_EQ(r.online_wanted_overlay_anchor, std::string("bottom_left"));
    ASSERT_EQ(r.online_wanted_overlay_style, std::string("dense"));
    ASSERT_NEAR(r.online_wanted_overlay_opacity, 0.7f, 0.001f);
    ASSERT_TRUE(!r.online_wanted_overlay_background_enabled);
    ASSERT_NEAR(r.online_wanted_overlay_scale, 1.2f, 0.001f);
    ASSERT_TRUE(r.online_wanted_overlay_custom_text_color_enabled);
    ASSERT_NEAR(r.online_wanted_overlay_text_color_r, 0.3f, 0.001f);
    ASSERT_NEAR(r.online_wanted_overlay_min, 100, 0);
    ASSERT_NEAR(r.online_wanted_overlay_max, 200, 0);
    return true;
}

bool TestWantedOverlayInvertedRangeNormalizesToSwapped() {
    SettingsManager mgr;
    mgr.LoadFromJson(R"({"online_wanted_overlay_min":200,"online_wanted_overlay_max":100})");
    const Settings& s = mgr.Get();
    // 200-100 should normalize to 100-200, not show nothing.
    ASSERT_EQ(s.online_wanted_overlay_min, 100);
    ASSERT_EQ(s.online_wanted_overlay_max, 200);
    return true;
}

bool TestWantedOverlayMaxZeroPreservedAsNoLimit() {
    SettingsManager mgr;
    mgr.LoadFromJson(R"({"online_wanted_overlay_min":1,"online_wanted_overlay_max":0})");
    const Settings& s = mgr.Get();
    // max=0 means no upper limit; must NOT be swapped to min=0.
    ASSERT_EQ(s.online_wanted_overlay_min, 1);
    ASSERT_EQ(s.online_wanted_overlay_max, 0);
    return true;
}

bool TestWantedOverlayDefaultsWhenAbsent() {
    SettingsManager mgr;
    mgr.LoadFromJson("{}");
    const Settings& s = mgr.Get();
    ASSERT_TRUE(!s.online_wanted_overlay_enabled);
    ASSERT_EQ(s.online_wanted_overlay_anchor, std::string("top_right"));
    ASSERT_EQ(s.online_wanted_overlay_style, std::string("card"));
    ASSERT_NEAR(s.online_wanted_overlay_opacity, 0.85f, 0.001f);
    ASSERT_TRUE(s.online_wanted_overlay_background_enabled);
    ASSERT_NEAR(s.online_wanted_overlay_scale, 1.0f, 0.001f);
    ASSERT_EQ(s.online_wanted_overlay_min, 1);
    ASSERT_EQ(s.online_wanted_overlay_max, 0);
    return true;
}

bool TestPlayerRadarAndGameHudSettingsRoundtrip() {
    SettingsManager manager;
    Settings& s = manager.GetMutable();
    s.online_friends_overlay_enabled = true;
    s.online_enemies_overlay_enabled = false;
    s.online_quick_overlay_show_rcb = false;
    s.online_friends_overlay_anchor = "bottom_left";
    s.online_friends_overlay_offset_x = 123.0f;
    s.online_friends_overlay_offset_y = 234.0f;
    s.online_enemies_overlay_anchor = "top_right";
    s.online_enemies_overlay_offset_x = 345.0f;
    s.online_enemies_overlay_offset_y = 456.0f;
    s.online_location_overlay_enabled = true;
    s.online_location_overlay_anchor = "bottom_right";
    s.online_location_overlay_offset_x = 17.0f;
    s.online_location_overlay_offset_y = 28.0f;
    s.online_transport_overlay_enabled = true;
    s.online_transport_overlay_anchor = "top_right";
    s.online_transport_overlay_offset_x = 39.0f;
    s.online_transport_overlay_offset_y = 40.0f;
    s.online_pedslots_overlay_enabled = true;
    s.online_pedslots_overlay_anchor = "bottom_left";
    s.online_pedslots_overlay_offset_x = 51.0f;
    s.online_pedslots_overlay_offset_y = 62.0f;
    s.online_player_markers.push_back({"Some_Player", "Gang \"Blue\"", true});
    SettingsManager reloaded;
    ASSERT_TRUE(reloaded.LoadFromJson(manager.GetSettingsJson()));
    const Settings& r = reloaded.Get();
    ASSERT_TRUE(r.online_friends_overlay_enabled);
    ASSERT_TRUE(!r.online_enemies_overlay_enabled);
    ASSERT_TRUE(!r.online_quick_overlay_show_rcb);
    ASSERT_EQ(r.online_friends_overlay_anchor, std::string("bottom_left"));
    ASSERT_EQ(r.online_friends_overlay_offset_x, 123.0f);
    ASSERT_EQ(r.online_friends_overlay_offset_y, 234.0f);
    ASSERT_EQ(r.online_enemies_overlay_anchor, std::string("top_right"));
    ASSERT_EQ(r.online_enemies_overlay_offset_x, 345.0f);
    ASSERT_EQ(r.online_enemies_overlay_offset_y, 456.0f);
    ASSERT_TRUE(r.online_location_overlay_enabled && r.online_transport_overlay_enabled && r.online_pedslots_overlay_enabled);
    ASSERT_EQ(r.online_location_overlay_anchor, std::string("bottom_right"));
    ASSERT_EQ(r.online_transport_overlay_anchor, std::string("top_right"));
    ASSERT_EQ(r.online_pedslots_overlay_anchor, std::string("bottom_left"));
    ASSERT_EQ(r.online_location_overlay_offset_x, 17.0f);
    ASSERT_EQ(r.online_location_overlay_offset_y, 28.0f);
    ASSERT_EQ(r.online_transport_overlay_offset_x, 39.0f);
    ASSERT_EQ(r.online_transport_overlay_offset_y, 40.0f);
    ASSERT_EQ(r.online_pedslots_overlay_offset_x, 51.0f);
    ASSERT_EQ(r.online_pedslots_overlay_offset_y, 62.0f);
    ASSERT_EQ(r.online_player_markers.size(), std::size_t(1));
    ASSERT_EQ(r.online_player_markers[0].name, std::string("Some_Player"));
    ASSERT_EQ(r.online_player_markers[0].affiliation, std::string("Gang \"Blue\""));
    ASSERT_TRUE(r.online_player_markers[0].friendly);
    return true;
}

bool TestPlayerMarkersNormalizeDeduplicateAndLimitEntries() {
    SettingsManager manager;
    std::string json = R"({"online_player_markers":[{"name":" Alpha ","affiliation":" LSPD ","friendly":true},{"name":"alpha"},{"name":"  "},{"name":"LongPlayerNameThatExceedsTwentyFourCharacters","affiliation":"01234567890123456789012345678901234567890123456789012345"})";
    for (int i = 0; i < 30; ++i) json += ",{\"name\":\"Player" + std::to_string(i) + "\"}";
    json += "]}";
    ASSERT_TRUE(manager.LoadFromJson(json));
    const Settings& s = manager.Get();
    ASSERT_EQ(s.online_player_markers.size(), std::size_t(20));
    ASSERT_EQ(s.online_player_markers[0].name, std::string("Alpha"));
    ASSERT_EQ(s.online_player_markers[0].affiliation, std::string("LSPD"));
    ASSERT_TRUE(s.online_player_markers[0].friendly);
    ASSERT_EQ(s.online_player_markers[1].name.size(), std::size_t(24));
    ASSERT_EQ(s.online_player_markers[1].affiliation.size(), std::size_t(48));
    ASSERT_TRUE(manager.LoadFromJson(R"({"online_player_markers":[]})"));
    ASSERT_TRUE(manager.Get().online_player_markers.empty());
    return true;
}

bool TestLegacyRadarSettingsOnlyMigrateDisplaySelections() {
    SettingsManager manager;
    ASSERT_TRUE(manager.LoadFromJson(R"({"online_markers_overlay_enabled":true,"online_marker_watcher_enabled":true,"online_marker_chat_alert_enabled":true,"online_marker_sound_alert_enabled":true,"online_marker_refresh_seconds":2})"));
    ASSERT_TRUE(manager.Get().online_friends_overlay_enabled);
    ASSERT_TRUE(manager.Get().online_enemies_overlay_enabled);
    const std::string saved = manager.GetSettingsJson();
    ASSERT_TRUE(saved.find("online_markers_overlay_enabled") == std::string::npos);
    ASSERT_TRUE(saved.find("online_marker_watcher_enabled") == std::string::npos);
    ASSERT_TRUE(saved.find("online_marker_chat_alert_enabled") == std::string::npos);
    ASSERT_TRUE(saved.find("online_marker_sound_alert_enabled") == std::string::npos);
    ASSERT_TRUE(saved.find("online_marker_refresh_seconds") == std::string::npos);

    SettingsManager explicitlyDisabled;
    ASSERT_TRUE(explicitlyDisabled.LoadFromJson(R"({"online_markers_overlay_enabled":true,"online_friends_overlay_enabled":false,"online_enemies_overlay_enabled":false})"));
    ASSERT_TRUE(!explicitlyDisabled.Get().online_friends_overlay_enabled);
    ASSERT_TRUE(!explicitlyDisabled.Get().online_enemies_overlay_enabled);
    SettingsManager onlyRemovedOptions;
    ASSERT_TRUE(onlyRemovedOptions.LoadFromJson(R"({"online_marker_watcher_enabled":true,"online_marker_chat_alert_enabled":true,"online_marker_sound_alert_enabled":true})"));
    ASSERT_TRUE(!onlyRemovedOptions.Get().online_friends_overlay_enabled);
    ASSERT_TRUE(!onlyRemovedOptions.Get().online_enemies_overlay_enabled);
    ASSERT_TRUE(onlyRemovedOptions.Get().online_quick_overlay_show_rcb);
    return true;
}

bool TestSeparateRadarPositionsMigrateLegacyValuesAndRespectExplicitPlacements() {
    SettingsManager defaults;
    ASSERT_EQ(defaults.Get().online_friends_overlay_anchor, std::string("top_left"));
    ASSERT_EQ(defaults.Get().online_enemies_overlay_anchor, std::string("top_left"));
    ASSERT_EQ(defaults.Get().online_friends_overlay_offset_y, 250.0f);
    ASSERT_EQ(defaults.Get().online_enemies_overlay_offset_y, 370.0f);

    SettingsManager legacy;
    ASSERT_TRUE(legacy.LoadFromJson(R"({"online_markers_overlay_anchor":"bottom_left","online_markers_overlay_offset_x":42,"online_markers_overlay_offset_y":83})"));
    ASSERT_EQ(legacy.Get().online_friends_overlay_anchor, std::string("bottom_left"));
    ASSERT_EQ(legacy.Get().online_enemies_overlay_anchor, std::string("bottom_left"));
    ASSERT_EQ(legacy.Get().online_friends_overlay_offset_x, 42.0f);
    ASSERT_EQ(legacy.Get().online_enemies_overlay_offset_x, 42.0f);
    ASSERT_EQ(legacy.Get().online_friends_overlay_offset_y, 83.0f);
    ASSERT_EQ(legacy.Get().online_enemies_overlay_offset_y, 203.0f);
    const std::string saved = legacy.GetSettingsJson();
    ASSERT_TRUE(saved.find("online_markers_overlay_anchor") == std::string::npos);
    ASSERT_TRUE(saved.find("online_markers_overlay_offset_") == std::string::npos);

    SettingsManager mixed;
    ASSERT_TRUE(mixed.LoadFromJson(R"({"online_markers_overlay_anchor":"bottom_left","online_markers_overlay_offset_x":42,"online_markers_overlay_offset_y":83,"online_friends_overlay_anchor":"top_right","online_friends_overlay_offset_x":100,"online_friends_overlay_offset_y":101,"online_enemies_overlay_anchor":"bottom_right","online_enemies_overlay_offset_x":200,"online_enemies_overlay_offset_y":201})"));
    ASSERT_EQ(mixed.Get().online_friends_overlay_anchor, std::string("top_right"));
    ASSERT_EQ(mixed.Get().online_friends_overlay_offset_x, 100.0f);
    ASSERT_EQ(mixed.Get().online_friends_overlay_offset_y, 101.0f);
    ASSERT_EQ(mixed.Get().online_enemies_overlay_anchor, std::string("bottom_right"));
    ASSERT_EQ(mixed.Get().online_enemies_overlay_offset_x, 200.0f);
    ASSERT_EQ(mixed.Get().online_enemies_overlay_offset_y, 201.0f);

    SettingsManager partial;
    ASSERT_TRUE(partial.LoadFromJson(R"({"online_markers_overlay_offset_x":42,"online_markers_overlay_offset_y":83,"online_friends_overlay_offset_x":111,"online_enemies_overlay_offset_y":222})"));
    ASSERT_EQ(partial.Get().online_friends_overlay_offset_x, 111.0f);
    ASSERT_EQ(partial.Get().online_friends_overlay_offset_y, 83.0f);
    ASSERT_EQ(partial.Get().online_enemies_overlay_offset_x, 42.0f);
    ASSERT_EQ(partial.Get().online_enemies_overlay_offset_y, 222.0f);
    return true;
}

int RunOnlineSettingsTests() {
    std::printf("\n=== Online Settings Tests ===\n");
    RUN_TEST(TestOnlineDefaultsWhenEmpty);
    RUN_TEST(TestOnlineRoundtripHappyPath);
    RUN_TEST(TestOnlineRoundtripPreservesAllFields);
    RUN_TEST(TestOnlineRoundtripDefaults);
    RUN_TEST(TestNormalizeInvalidMode);
    RUN_TEST(TestNormalizeInvalidAnchor);
    RUN_TEST(TestNormalizeInvalidStyle);
    RUN_TEST(TestNormalizeOpacityLow);
    RUN_TEST(TestNormalizeOpacityHigh);
    RUN_TEST(TestNormalizeValidHoldMode);
    RUN_TEST(TestNormalizeAllValidAnchors);
    RUN_TEST(TestNormalizeAllValidStyles);
    RUN_TEST(TestGangDeduplication);
    RUN_TEST(TestNormalizePersistentOverlayStyling);
    RUN_TEST(TestGangsOverlaySettingRoundtrips);
    RUN_TEST(TestEmptyEventPlaceholderArrayFallsBackToDefaults);
    RUN_TEST(TestWantedOverlaySettingsRoundtripAndNormalizeRange);
    RUN_TEST(TestWantedOverlayInvertedRangeNormalizesToSwapped);
    RUN_TEST(TestWantedOverlayMaxZeroPreservedAsNoLimit);
    RUN_TEST(TestWantedOverlayDefaultsWhenAbsent);
    RUN_TEST(TestPlayerRadarAndGameHudSettingsRoundtrip);
    RUN_TEST(TestPlayerMarkersNormalizeDeduplicateAndLimitEntries);
    RUN_TEST(TestLegacyRadarSettingsOnlyMigrateDisplaySelections);
    RUN_TEST(TestSeparateRadarPositionsMigrateLegacyValuesAndRespectExplicitPlacements);
    std::printf("Online Settings: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}

#ifndef ONLINE_SETTINGS_TEST_MAIN
// When compiled as part of KeybindTests.exe, main() is provided by KeybindTests.cpp
#else
int main() {
    return RunOnlineSettingsTests();
}
#endif
