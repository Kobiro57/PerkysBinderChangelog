#pragma once

#include "InterfaceLayout.h"

#include <array>
#include <functional>
#include <string>

struct ImDrawList;
class SAMPBridge;

namespace keybinder {

class SettingsManager;

struct InterfacePreset {
    std::string name;
    HudLayout layout{};
    bool show_master_hud = true;
    bool show_radar = true;
    bool show_area_name = true;
    bool show_vehicle_name = true;
    bool show_clock = true;
    bool show_vital_stats = true;
    bool show_3d_markers = true;
    float hp_color_r = 0.705882f;
    float hp_color_g = 0.098039f;
    float hp_color_b = 0.113725f;
    float hp_color_a = 1.0f;
    float armor_color_r = 1.0f;
    float armor_color_g = 1.0f;
    float armor_color_b = 1.0f;
    float armor_color_a = 1.0f;
    float money_color_r = 0.211764f;
    float money_color_g = 0.407843f;
    float money_color_b = 0.172549f;
    float money_color_a = 1.0f;
    bool show_hp_number = false;
    bool show_armor_number = false;
    float hp_number_pos_x = 0.0f;
    float hp_number_pos_y = 0.0f;
    float armor_number_pos_x = 0.0f;
    float armor_number_pos_y = 0.0f;
    float hp_number_scale = 1.0f;
    float armor_number_scale = 1.0f;
    // Legacy shared offset kept for config migration.
    float vital_stats_numbers_pos_x = 0.0f;
    float vital_stats_numbers_pos_y = 0.0f;
};

class InterfaceEditor {
public:
    static constexpr int PRESET_SLOT_COUNT = 5;

    InterfaceEditor() = default;
    ~InterfaceEditor() = default;

    bool Initialize();
    void Shutdown();

    void ApplyCurrentPreset();
    void RestoreOriginalValues();
    void ReadSampLiveState();
    void RenderVitalStatsNumbers(ImDrawList* drawList, bool overlayVisible, float uiScale);
    // Bounds provider returns x, y, width, height in viewport pixels, including
    // the current layout displacement.
    void SetElementBoundsProvider(std::function<bool(HudElement, float&, float&, float&, float&)> provider);
    bool StartElementMove(HudElement element);
    bool MoveElementBy(float normalizedDeltaX, float normalizedDeltaY);
    void FinishElementMove();
    bool IsElementMoveModeActive() const;
    HudElement GetMovingElement() const;
    void RenderElementMoveMode(ImDrawList* drawList, float uiScale);

    void SetActivePreset(int index);
    int GetActivePreset() const;
    void SetSettingsManager(SettingsManager* manager);
    void SetSAMPBridge(SAMPBridge* bridge);
    void SetOnRuntimePresetChanged(std::function<void()> callback);
    void SetHpNumberMoveMode(bool enabled);
    bool IsHpNumberMoveMode() const;
    void SetArmorNumberMoveMode(bool enabled);
    bool IsArmorNumberMoveMode() const;

    std::array<InterfacePreset, PRESET_SLOT_COUNT>& GetPresets() { return presets_; }
    const std::array<InterfacePreset, PRESET_SLOT_COUNT>& GetPresets() const { return presets_; }
    InterfacePreset* GetPreset(int index);
    const InterfacePreset* GetPreset(int index) const;

    static constexpr unsigned char GetHealthPaletteSlotForTesting() { return kHealthPaletteSlot; }
    static constexpr unsigned char GetArmorPaletteSlotForTesting() { return kArmorPaletteSlot; }
    static constexpr unsigned char GetMoneyPaletteSlotForTesting() { return kMoneyPaletteSlot; }

private:
    // GTA SA HUD color IDs: health=red(0), armor=white(4), money=green(1).
    static constexpr unsigned char kHealthPaletteSlot = 0;
    static constexpr unsigned char kArmorPaletteSlot = 4;
    static constexpr unsigned char kMoneyPaletteSlot = 1;

    std::array<InterfacePreset, PRESET_SLOT_COUNT> presets_{};
    int active_preset_ = 0;

    bool original_values_captured_ = false;
    bool original_show_master_hud_ = true;
    bool original_show_radar_ = true;
    bool original_show_area_name_ = true;
    bool original_show_vehicle_name_ = true;
    bool original_show_clock_ = true;
    bool original_show_vital_stats_ = true;
    bool original_show_3d_markers_ = true;
    SettingsManager* settings_manager_ = nullptr;
    ::SAMPBridge* samp_bridge_ = nullptr;
    std::function<void()> runtime_preset_changed_callback_;
    bool hp_number_move_mode_ = false;
    bool hp_number_dragging_ = false;
    bool armor_number_move_mode_ = false;
    bool armor_number_dragging_ = false;
    HudElement moving_element_ = HudElement::Count;
    int moving_preset_ = -1;
    std::function<bool(HudElement, float&, float&, float&, float&)> element_bounds_provider_;
};

} // namespace keybinder
