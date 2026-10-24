#include "../include/InterfaceEditor.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>

class CSprite2d;

class CHud {
public:
    static bool& bScriptDontDisplayAreaName;
    static bool& bScriptDontDisplayVehicleName;
    static bool& bScriptForceDisplayWithCounters;
    static bool& bScriptDontDisplayRadar;
    static bool& bDrawClock;
    static bool& m_bDraw3dMarkers;
    static bool& m_Wants_To_Draw_Hud;
    static bool& bDrawingVitalStats;
    static CSprite2d* Sprites;
};

namespace sampapi {
namespace v03dl {

class CChat {
public:
    enum DisplayMode {
        DISPLAY_MODE_OFF,
        DISPLAY_MODE_NOSHADOW,
        DISPLAY_MODE_NORMAL
    };

    enum { MAX_MESSAGES = 100 };

    unsigned int m_nPageSize;
    char* m_szLastMessage;
    int m_nMode;
    bool m_bTimestamps;

    void UpdateScrollbar();
    void SetPageSize(int nValue);
};

class CScoreboard {
public:
    int m_bIsEnabled;

    void Recalc();
    void Close(bool bHideCursor);
    void SendNotification();
    void UpdateList();
    void Enable();
};

CChat*& RefChat();
CScoreboard*& RefScoreboard();

} // namespace v03dl
} // namespace sampapi

namespace {

using keybinder::InterfaceEditor;
using keybinder::InterfacePreset;

struct InterfaceEditorTestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

InterfaceEditorTestState g_state;

#define IFACE_EXPECT_TRUE(condition) do { \
    if (!(condition)) { \
        g_state.error = "Assertion failed: " #condition; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define IFACE_EXPECT_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        g_state.error = "Equality failed: " #actual " != " #expected; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define IFACE_RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

struct HudSnapshot {
    bool show_master_hud = true;
    bool show_radar = true;
    bool show_area_name = true;
    bool show_vehicle_name = true;
    bool show_clock = true;
    bool show_vital_stats = true;
    bool show_3d_markers = true;
};

struct SampCounters {
    int update_scrollbar_calls = 0;
    int set_page_size_calls = 0;
    int scoreboard_enable_calls = 0;
    int scoreboard_update_calls = 0;
    int scoreboard_close_calls = 0;
    int scoreboard_recalc_calls = 0;
    int scoreboard_notify_calls = 0;
    bool scoreboard_close_hide_cursor = false;
};

SampCounters g_samp_counters;
sampapi::v03dl::CChat g_chat{};
sampapi::v03dl::CScoreboard g_scoreboard{};

bool PresetsEqual(const InterfacePreset& lhs, const InterfacePreset& rhs) {
    return lhs.name == rhs.name &&
           lhs.show_master_hud == rhs.show_master_hud &&
           lhs.show_radar == rhs.show_radar &&
           lhs.show_area_name == rhs.show_area_name &&
           lhs.show_vehicle_name == rhs.show_vehicle_name &&
           lhs.show_clock == rhs.show_clock &&
           lhs.show_vital_stats == rhs.show_vital_stats &&
           lhs.show_3d_markers == rhs.show_3d_markers &&
           lhs.hp_color_r == rhs.hp_color_r &&
           lhs.hp_color_g == rhs.hp_color_g &&
           lhs.hp_color_b == rhs.hp_color_b &&
           lhs.hp_color_a == rhs.hp_color_a &&
           lhs.armor_color_r == rhs.armor_color_r &&
           lhs.armor_color_g == rhs.armor_color_g &&
           lhs.armor_color_b == rhs.armor_color_b &&
           lhs.armor_color_a == rhs.armor_color_a &&
           lhs.money_color_r == rhs.money_color_r &&
           lhs.money_color_g == rhs.money_color_g &&
           lhs.money_color_b == rhs.money_color_b &&
            lhs.money_color_a == rhs.money_color_a &&
            lhs.show_hp_number == rhs.show_hp_number &&
           lhs.show_armor_number == rhs.show_armor_number &&
           lhs.hp_number_pos_x == rhs.hp_number_pos_x &&
           lhs.hp_number_pos_y == rhs.hp_number_pos_y &&
           lhs.armor_number_pos_x == rhs.armor_number_pos_x &&
           lhs.armor_number_pos_y == rhs.armor_number_pos_y &&
           lhs.hp_number_scale == rhs.hp_number_scale &&
           lhs.armor_number_scale == rhs.armor_number_scale &&
           lhs.vital_stats_numbers_pos_x == rhs.vital_stats_numbers_pos_x &&
            lhs.vital_stats_numbers_pos_y == rhs.vital_stats_numbers_pos_y;
}

bool PresetArraysEqual(
    const std::array<InterfacePreset, InterfaceEditor::PRESET_SLOT_COUNT>& lhs,
    const std::array<InterfacePreset, InterfaceEditor::PRESET_SLOT_COUNT>& rhs) {
    for (int i = 0; i < InterfaceEditor::PRESET_SLOT_COUNT; ++i) {
        if (!PresetsEqual(lhs[static_cast<std::size_t>(i)], rhs[static_cast<std::size_t>(i)])) {
            return false;
        }
    }

    return true;
}

InterfacePreset MakePreset(
    const char* name,
    bool show_master_hud,
    bool show_radar,
    bool show_area_name,
    bool show_vehicle_name,
    bool show_clock,
    bool show_vital_stats,
    bool show_3d_markers) {
    InterfacePreset preset;
    preset.name = name;
    preset.show_master_hud = show_master_hud;
    preset.show_radar = show_radar;
    preset.show_area_name = show_area_name;
    preset.show_vehicle_name = show_vehicle_name;
    preset.show_clock = show_clock;
    preset.show_vital_stats = show_vital_stats;
    preset.show_3d_markers = show_3d_markers;
    return preset;
}

HudSnapshot CaptureHud() {
    return HudSnapshot{
        CHud::m_Wants_To_Draw_Hud,
        !CHud::bScriptDontDisplayRadar,
        !CHud::bScriptDontDisplayAreaName,
        !CHud::bScriptDontDisplayVehicleName,
        CHud::bDrawClock,
        CHud::bDrawingVitalStats,
        CHud::m_bDraw3dMarkers,
    };
}

void ApplyHudSnapshot(const HudSnapshot& snapshot) {
    CHud::m_Wants_To_Draw_Hud = snapshot.show_master_hud;
    CHud::bScriptDontDisplayRadar = !snapshot.show_radar;
    CHud::bScriptDontDisplayAreaName = !snapshot.show_area_name;
    CHud::bScriptDontDisplayVehicleName = !snapshot.show_vehicle_name;
    CHud::bDrawClock = snapshot.show_clock;
    CHud::bDrawingVitalStats = snapshot.show_vital_stats;
    CHud::m_bDraw3dMarkers = snapshot.show_3d_markers;
}

bool ExpectHudMatchesPreset(const InterfacePreset& preset) {
    const HudSnapshot snapshot = CaptureHud();
    IFACE_EXPECT_EQ(snapshot.show_master_hud, preset.show_master_hud);
    IFACE_EXPECT_EQ(snapshot.show_radar, preset.show_radar);
    IFACE_EXPECT_EQ(snapshot.show_area_name, preset.show_area_name);
    IFACE_EXPECT_EQ(snapshot.show_vehicle_name, preset.show_vehicle_name);
    IFACE_EXPECT_EQ(snapshot.show_clock, preset.show_clock);
    IFACE_EXPECT_EQ(snapshot.show_vital_stats, preset.show_vital_stats);
    IFACE_EXPECT_EQ(snapshot.show_3d_markers, preset.show_3d_markers);
    return true;
}

bool ExpectHudMatchesSnapshot(const HudSnapshot& snapshot) {
    const HudSnapshot current = CaptureHud();
    IFACE_EXPECT_EQ(current.show_master_hud, snapshot.show_master_hud);
    IFACE_EXPECT_EQ(current.show_radar, snapshot.show_radar);
    IFACE_EXPECT_EQ(current.show_area_name, snapshot.show_area_name);
    IFACE_EXPECT_EQ(current.show_vehicle_name, snapshot.show_vehicle_name);
    IFACE_EXPECT_EQ(current.show_clock, snapshot.show_clock);
    IFACE_EXPECT_EQ(current.show_vital_stats, snapshot.show_vital_stats);
    IFACE_EXPECT_EQ(current.show_3d_markers, snapshot.show_3d_markers);
    return true;
}

void ResetSampLiveObjects() {
    g_samp_counters = {};
    std::memset(&g_chat, 0, sizeof(g_chat));
    std::memset(&g_scoreboard, 0, sizeof(g_scoreboard));
    sampapi::v03dl::RefChat() = nullptr;
    sampapi::v03dl::RefScoreboard() = nullptr;
}

void AttachSampLiveObjects() {
    ResetSampLiveObjects();
    sampapi::v03dl::RefChat() = &g_chat;
    sampapi::v03dl::RefScoreboard() = &g_scoreboard;
}

bool Test_DefaultPresetSlotsNormalizedToFive() {
    InterfaceEditor editor;

    IFACE_EXPECT_EQ(editor.GetPresets().size(), static_cast<std::size_t>(InterfaceEditor::PRESET_SLOT_COUNT));

    for (int i = 0; i < InterfaceEditor::PRESET_SLOT_COUNT; ++i) {
        const InterfacePreset* preset = editor.GetPreset(i);
        IFACE_EXPECT_TRUE(preset != nullptr);
        IFACE_EXPECT_TRUE(preset->name.empty());
        IFACE_EXPECT_EQ(preset->show_master_hud, true);
        IFACE_EXPECT_EQ(preset->show_radar, true);
        IFACE_EXPECT_EQ(preset->show_area_name, true);
        IFACE_EXPECT_EQ(preset->show_vehicle_name, true);
        IFACE_EXPECT_EQ(preset->show_clock, true);
        IFACE_EXPECT_EQ(preset->show_vital_stats, true);
        IFACE_EXPECT_EQ(preset->show_3d_markers, true);
        IFACE_EXPECT_EQ(preset->hp_color_r, 0.705882f);
        IFACE_EXPECT_EQ(preset->hp_color_g, 0.098039f);
        IFACE_EXPECT_EQ(preset->hp_color_b, 0.113725f);
        IFACE_EXPECT_EQ(preset->hp_color_a, 1.0f);
        IFACE_EXPECT_EQ(preset->armor_color_r, 1.0f);
        IFACE_EXPECT_EQ(preset->armor_color_g, 1.0f);
        IFACE_EXPECT_EQ(preset->armor_color_b, 1.0f);
        IFACE_EXPECT_EQ(preset->armor_color_a, 1.0f);
        IFACE_EXPECT_EQ(preset->money_color_r, 0.211764f);
        IFACE_EXPECT_EQ(preset->money_color_g, 0.407843f);
        IFACE_EXPECT_EQ(preset->money_color_b, 0.172549f);
        IFACE_EXPECT_EQ(preset->money_color_a, 1.0f);
        IFACE_EXPECT_EQ(preset->show_hp_number, false);
        IFACE_EXPECT_EQ(preset->show_armor_number, false);
        IFACE_EXPECT_EQ(preset->hp_number_pos_x, 0.0f);
        IFACE_EXPECT_EQ(preset->hp_number_pos_y, 0.0f);
        IFACE_EXPECT_EQ(preset->armor_number_pos_x, 0.0f);
        IFACE_EXPECT_EQ(preset->armor_number_pos_y, 0.0f);
        IFACE_EXPECT_EQ(preset->hp_number_scale, 1.0f);
        IFACE_EXPECT_EQ(preset->armor_number_scale, 1.0f);
        IFACE_EXPECT_EQ(preset->vital_stats_numbers_pos_x, 0.0f);
        IFACE_EXPECT_EQ(preset->vital_stats_numbers_pos_y, 0.0f);
    }

    return true;
}

bool Test_PaletteSlotsMatchGtaHudBars() {
    IFACE_EXPECT_EQ(InterfaceEditor::GetHealthPaletteSlotForTesting(), static_cast<unsigned char>(0));
    IFACE_EXPECT_EQ(InterfaceEditor::GetArmorPaletteSlotForTesting(), static_cast<unsigned char>(4));
    IFACE_EXPECT_EQ(InterfaceEditor::GetMoneyPaletteSlotForTesting(), static_cast<unsigned char>(1));
    return true;
}

bool Test_PresetSelectionEditingAndIndexValidation() {
    InterfaceEditor editor;
    const InterfaceEditor& const_editor = editor;

    editor.SetActivePreset(3);
    IFACE_EXPECT_EQ(editor.GetActivePreset(), 3);

    InterfacePreset* preset = editor.GetPreset(editor.GetActivePreset());
    IFACE_EXPECT_TRUE(preset != nullptr);

    *preset = MakePreset("Minimal HUD", false, false, true, false, false, true, false);
    preset->hp_color_r = 0.10f;
    preset->hp_color_g = 0.20f;
    preset->hp_color_b = 0.30f;
    preset->armor_color_r = 0.40f;
    preset->armor_color_g = 0.50f;
    preset->armor_color_b = 0.60f;
    preset->money_color_r = 0.70f;
    preset->money_color_g = 0.80f;
    preset->money_color_b = 0.90f;
    preset->show_hp_number = true;
    preset->show_armor_number = false;
    preset->hp_number_pos_x = -0.08f;
    preset->hp_number_pos_y = 0.04f;
    preset->armor_number_pos_x = 0.03f;
    preset->armor_number_pos_y = -0.02f;
    preset->hp_number_scale = 1.15f;
    preset->armor_number_scale = 0.9f;
    preset->vital_stats_numbers_pos_x = -0.08f;
    preset->vital_stats_numbers_pos_y = 0.04f;

    const InterfacePreset* stored = const_editor.GetPreset(3);
    IFACE_EXPECT_TRUE(stored != nullptr);
    IFACE_EXPECT_EQ(stored->name, std::string("Minimal HUD"));
    IFACE_EXPECT_EQ(stored->show_master_hud, false);
    IFACE_EXPECT_EQ(stored->show_radar, false);
    IFACE_EXPECT_EQ(stored->show_area_name, true);
    IFACE_EXPECT_EQ(stored->show_vehicle_name, false);
    IFACE_EXPECT_EQ(stored->show_clock, false);
    IFACE_EXPECT_EQ(stored->show_vital_stats, true);
    IFACE_EXPECT_EQ(stored->show_3d_markers, false);
    IFACE_EXPECT_EQ(stored->hp_color_r, 0.10f);
    IFACE_EXPECT_EQ(stored->hp_color_g, 0.20f);
    IFACE_EXPECT_EQ(stored->hp_color_b, 0.30f);
    IFACE_EXPECT_EQ(stored->armor_color_r, 0.40f);
    IFACE_EXPECT_EQ(stored->armor_color_g, 0.50f);
    IFACE_EXPECT_EQ(stored->armor_color_b, 0.60f);
    IFACE_EXPECT_EQ(stored->money_color_r, 0.70f);
    IFACE_EXPECT_EQ(stored->money_color_g, 0.80f);
    IFACE_EXPECT_EQ(stored->money_color_b, 0.90f);
    IFACE_EXPECT_EQ(stored->show_hp_number, true);
    IFACE_EXPECT_EQ(stored->show_armor_number, false);
    IFACE_EXPECT_EQ(stored->hp_number_pos_x, -0.08f);
    IFACE_EXPECT_EQ(stored->hp_number_pos_y, 0.04f);
    IFACE_EXPECT_EQ(stored->armor_number_pos_x, 0.03f);
    IFACE_EXPECT_EQ(stored->armor_number_pos_y, -0.02f);
    IFACE_EXPECT_EQ(stored->hp_number_scale, 1.15f);
    IFACE_EXPECT_EQ(stored->armor_number_scale, 0.9f);
    IFACE_EXPECT_EQ(stored->vital_stats_numbers_pos_x, -0.08f);
    IFACE_EXPECT_EQ(stored->vital_stats_numbers_pos_y, 0.04f);

    editor.SetActivePreset(-99);
    IFACE_EXPECT_EQ(editor.GetActivePreset(), 0);

    editor.SetActivePreset(99);
    IFACE_EXPECT_EQ(editor.GetActivePreset(), InterfaceEditor::PRESET_SLOT_COUNT - 1);

    IFACE_EXPECT_TRUE(editor.GetPreset(-1) == nullptr);
    IFACE_EXPECT_TRUE(editor.GetPreset(InterfaceEditor::PRESET_SLOT_COUNT) == nullptr);
    IFACE_EXPECT_TRUE(const_editor.GetPreset(-1) == nullptr);
    IFACE_EXPECT_TRUE(const_editor.GetPreset(InterfaceEditor::PRESET_SLOT_COUNT) == nullptr);

    return true;
}

bool Test_ApplyCurrentPresetAppliesFlagsAndCapturesOriginalsOnce() {
    const HudSnapshot original{false, true, false, true, false, true, false};
    ApplyHudSnapshot(original);

    InterfaceEditor editor;
    *editor.GetPreset(0) = MakePreset("Preset A", true, false, true, false, true, false, true);
    *editor.GetPreset(1) = MakePreset("Preset B", false, true, true, true, false, false, false);

    editor.SetActivePreset(0);
    editor.ApplyCurrentPreset();
    IFACE_EXPECT_TRUE(ExpectHudMatchesPreset(*editor.GetPreset(0)));

    editor.SetActivePreset(1);
    editor.ApplyCurrentPreset();
    IFACE_EXPECT_TRUE(ExpectHudMatchesPreset(*editor.GetPreset(1)));

    editor.RestoreOriginalValues();
    IFACE_EXPECT_TRUE(ExpectHudMatchesSnapshot(original));
    return true;
}

bool Test_RestoreOriginalValuesRestoresCapturedStateAndResetsCapture() {
    InterfaceEditor editor;
    *editor.GetPreset(0) = MakePreset("Hidden", false, false, false, false, false, false, false);
    editor.SetActivePreset(0);

    const HudSnapshot initial{true, false, true, false, true, false, true};
    ApplyHudSnapshot(initial);
    editor.RestoreOriginalValues();
    IFACE_EXPECT_TRUE(ExpectHudMatchesSnapshot(initial));

    editor.ApplyCurrentPreset();
    IFACE_EXPECT_TRUE(ExpectHudMatchesPreset(*editor.GetPreset(0)));

    editor.RestoreOriginalValues();
    IFACE_EXPECT_TRUE(ExpectHudMatchesSnapshot(initial));

    const HudSnapshot recaptured_original{false, true, true, false, false, true, true};
    ApplyHudSnapshot(recaptured_original);
    editor.RestoreOriginalValues();
    IFACE_EXPECT_TRUE(ExpectHudMatchesSnapshot(recaptured_original));

    editor.ApplyCurrentPreset();
    IFACE_EXPECT_TRUE(ExpectHudMatchesPreset(*editor.GetPreset(0)));

    editor.RestoreOriginalValues();
    IFACE_EXPECT_TRUE(ExpectHudMatchesSnapshot(recaptured_original));
    return true;
}

} // namespace

namespace sampapi {
namespace v03dl {

CChat*& RefChat() {
    static CChat* chat = nullptr;
    return chat;
}

CScoreboard*& RefScoreboard() {
    static CScoreboard* scoreboard = nullptr;
    return scoreboard;
}

void CChat::UpdateScrollbar() {
    ++g_samp_counters.update_scrollbar_calls;
}

void CChat::SetPageSize(int nValue) {
    m_nPageSize = static_cast<unsigned int>(nValue);
    ++g_samp_counters.set_page_size_calls;
}

void CScoreboard::Recalc() {
    ++g_samp_counters.scoreboard_recalc_calls;
}

void CScoreboard::Close(bool bHideCursor) {
    ++g_samp_counters.scoreboard_close_calls;
    g_samp_counters.scoreboard_close_hide_cursor = bHideCursor;
    m_bIsEnabled = 0;
}

void CScoreboard::SendNotification() {
    ++g_samp_counters.scoreboard_notify_calls;
}

void CScoreboard::UpdateList() {
    ++g_samp_counters.scoreboard_update_calls;
}

void CScoreboard::Enable() {
    ++g_samp_counters.scoreboard_enable_calls;
    m_bIsEnabled = 1;
}

} // namespace v03dl
} // namespace sampapi

int RunInterfaceEditorTests() {
    std::printf("\n=== InterfaceEditor Tests ===\n\n");

    IFACE_RUN_TEST(Test_DefaultPresetSlotsNormalizedToFive);
    IFACE_RUN_TEST(Test_PaletteSlotsMatchGtaHudBars);
    IFACE_RUN_TEST(Test_PresetSelectionEditingAndIndexValidation);
    IFACE_RUN_TEST(Test_ApplyCurrentPresetAppliesFlagsAndCapturesOriginalsOnce);
    IFACE_RUN_TEST(Test_RestoreOriginalValuesRestoresCapturedStateAndResetsCapture);
    std::printf("\n=== InterfaceEditor Results: %d passed, %d failed ===\n\n",
                g_state.passed, g_state.failed);

    return g_state.failed;
}
