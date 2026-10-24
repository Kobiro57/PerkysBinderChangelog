#include "../include/InterfaceEditor.h"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/imgui-1.92.7/imgui.h"

#ifndef KEYBINDER_TESTING
#include "plugin.h"
#include "common.h"
#include "safetyhook.hpp"

#include "../include/Logger.h"

#include "../include/plugin-sdk/plugin_sa/game_sa/CHud.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CHudColours.h"
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>

#ifdef KEYBINDER_TESTING
class CSprite2d;

class CHud {
public:
    static bool& bScriptDontDisplayAreaName;
    static bool& bScriptDontDisplayVehicleName;
    static bool& bScriptDontDisplayRadar;
    static bool& bDrawClock;
    static bool& bDrawingVitalStats;
    static bool& m_bDraw3dMarkers;
    static bool& m_Wants_To_Draw_Hud;
    static CSprite2d* Sprites;
};
#endif

namespace keybinder {

namespace {

#ifndef KEYBINDER_TESTING
constexpr unsigned char kHealthPaletteSlot = HUD_COLOUR_RED;
constexpr unsigned char kArmorPaletteSlot = HUD_COLOUR_GREY;
constexpr unsigned char kMoneyPaletteSlot = HUD_COLOUR_GREEN;
InterfaceEditor* g_active_interface_editor = nullptr;
safetyhook::InlineHook* g_draw_hud_hook = nullptr;
bool g_logged_draw_hook_execution = false;

unsigned char ClampToByte(float value) {
    const float clamped = (std::max)(0.0f, (std::min)(value, 1.0f));
    return static_cast<unsigned char>(std::lround(clamped * 255.0f));
}

void __cdecl HookedHudDraw() {
    InterfaceEditor* editor = g_active_interface_editor;
    if (editor == nullptr || g_draw_hud_hook == nullptr) {
        return;
    }

    const InterfacePreset* preset = editor->GetPreset(editor->GetActivePreset());
    if (preset == nullptr) {
        g_draw_hud_hook->ccall<void>();
        return;
    }

    g_draw_hud_hook->ccall<void>();
    if (!g_logged_draw_hook_execution) {
        keybinder::Logger::Debug("[InterfaceEditor] HUD draw hook executed");
        g_logged_draw_hook_execution = true;
    }
}

bool InstallHook(safetyhook::InlineHook*& hook, void* target, void* destination) {
    if (hook != nullptr) {
        return true;
    }

    auto createdHook = safetyhook::InlineHook::create(target, destination);
    if (!createdHook) {
        return false;
    }

    hook = new safetyhook::InlineHook(std::move(*createdHook));
    return true;
}

void RemoveHook(safetyhook::InlineHook*& hook) {
    if (hook != nullptr) {
        delete hook;
        hook = nullptr;
    }
}
#endif

unsigned char ClampColorComponent(float value) {
    const float clamped = (std::max)(0.0f, (std::min)(value, 1.0f));
    return static_cast<unsigned char>(std::lround(clamped * 255.0f));
}

int ClampPresetIndex(int index) {
    return (std::max)(0, (std::min)(index, InterfaceEditor::PRESET_SLOT_COUNT - 1));
}

float ClampNormalizedPosition(float value) {
    return (std::max)(0.0f, (std::min)(value, 1.0f));
}

float ClampSignedNormalizedOffset(float value) {
    return (std::max)(-1.0f, (std::min)(value, 1.0f));
}

float ClampNumberScale(float value) {
    if (!std::isfinite(value)) {
        return 1.0f;
    }

    return (std::max)(0.5f, (std::min)(value, 3.0f));
}

float ScaleHudX(float viewportWidth, float hudUnits) {
    return viewportWidth * hudUnits / 640.0f;
}

float ScaleHudY(float viewportHeight, float hudUnits) {
    return viewportHeight * hudUnits / 448.0f;
}

bool TryReadPlayerVitalStats(const ::SAMPBridge* bridge, int& healthValue, int& armorValue) {
#ifndef KEYBINDER_TESTING
    if (bridge == nullptr) {
        return false;
    }

    sampapi::v03dl::CLocalPlayer* localPlayer = bridge->GetLocalPlayer();
    if (localPlayer == nullptr) {
        return false;
    }

    auto clampVisibleStat = [](int value) -> int {
        return (std::max)(0, (std::min)(value, 100));
    };

    auto clampPedFallback = [](float value) -> int {
        if (!std::isfinite(value)) {
            return 0;
        }

        return (std::max)(0, (std::min)(static_cast<int>(std::lround(value)), 100));
    };

    if (localPlayer->m_incarData.m_nDriverHealth > 0 || localPlayer->m_incarData.m_nDriverArmor > 0) {
        healthValue = clampVisibleStat(static_cast<int>(localPlayer->m_incarData.m_nDriverHealth));
        armorValue = clampVisibleStat(static_cast<int>(localPlayer->m_incarData.m_nDriverArmor));
        return true;
    }

    if (localPlayer->m_onfootData.m_nHealth > 0 || localPlayer->m_onfootData.m_nArmor > 0) {
        healthValue = clampVisibleStat(static_cast<int>(localPlayer->m_onfootData.m_nHealth));
        armorValue = clampVisibleStat(static_cast<int>(localPlayer->m_onfootData.m_nArmor));
        return true;
    }

    if (localPlayer->m_pPed != nullptr) {
        const float health = localPlayer->m_pPed->GetHealth();
        const float armor = localPlayer->m_pPed->GetArmour();
        healthValue = clampPedFallback(health);
        armorValue = clampPedFallback(armor);
        return true;
    }

    return false;
#else
    healthValue = 0;
    armorValue = 0;
    return false;
#endif
}

} // namespace

bool InterfaceEditor::Initialize() {
#ifndef KEYBINDER_TESTING
    g_active_interface_editor = this;
    if (!InstallHook(g_draw_hud_hook, reinterpret_cast<void*>(0x58FAE0), reinterpret_cast<void*>(&HookedHudDraw))) {
        keybinder::Logger::Error("[InterfaceEditor] Failed to install CHud::Draw hook");
        return false;
    }
    keybinder::Logger::Debug("[InterfaceEditor] Installed CHud::Draw hook");
    g_logged_draw_hook_execution = false;
#endif

    return true;
}

void InterfaceEditor::Shutdown() {
#ifndef KEYBINDER_TESTING
    RemoveHook(g_draw_hud_hook);
    if (g_active_interface_editor == this) {
        g_active_interface_editor = nullptr;
    }
#endif
}

void InterfaceEditor::ApplyCurrentPreset() {
    if (!original_values_captured_) {
#ifndef KEYBINDER_TESTING
        original_show_area_name_ = !CHud::bScriptDontDisplayAreaName;
        const CRGBA originalHealth = HudColour.GetRGBA(kHealthPaletteSlot);
        original_health_palette_[0] = originalHealth.r;
        original_health_palette_[1] = originalHealth.g;
        original_health_palette_[2] = originalHealth.b;
        original_health_palette_[3] = originalHealth.a;

        const CRGBA originalArmor = HudColour.GetRGBA(kArmorPaletteSlot);
        original_armor_palette_[0] = originalArmor.r;
        original_armor_palette_[1] = originalArmor.g;
        original_armor_palette_[2] = originalArmor.b;
        original_armor_palette_[3] = originalArmor.a;

        const CRGBA originalMoney = HudColour.GetRGBA(kMoneyPaletteSlot);
        original_money_palette_[0] = originalMoney.r;
        original_money_palette_[1] = originalMoney.g;
        original_money_palette_[2] = originalMoney.b;
        original_money_palette_[3] = originalMoney.a;
#else
        original_show_master_hud_ = CHud::m_Wants_To_Draw_Hud;
        original_show_radar_ = !CHud::bScriptDontDisplayRadar;
        original_show_area_name_ = !CHud::bScriptDontDisplayAreaName;
        original_show_vehicle_name_ = !CHud::bScriptDontDisplayVehicleName;
        original_show_clock_ = CHud::bDrawClock;
        original_show_vital_stats_ = CHud::bDrawingVitalStats;
        original_show_3d_markers_ = CHud::m_bDraw3dMarkers;
#endif
        original_values_captured_ = true;
    }

    InterfacePreset* preset = GetPreset(GetActivePreset());
    if (preset == nullptr) {
        return;
    }

#ifndef KEYBINDER_TESTING
    HudColour.SetRGBAValue(
        kHealthPaletteSlot,
        ClampToByte(preset->hp_color_r),
        ClampToByte(preset->hp_color_g),
        ClampToByte(preset->hp_color_b),
        ClampToByte(preset->hp_color_a));
    HudColour.SetRGBAValue(
        kArmorPaletteSlot,
        ClampToByte(preset->armor_color_r),
        ClampToByte(preset->armor_color_g),
        ClampToByte(preset->armor_color_b),
        ClampToByte(preset->armor_color_a));
    HudColour.SetRGBAValue(
        kMoneyPaletteSlot,
        ClampToByte(preset->money_color_r),
        ClampToByte(preset->money_color_g),
        ClampToByte(preset->money_color_b),
        ClampToByte(preset->money_color_a));
    CHud::m_Wants_To_Draw_Hud = preset->show_master_hud;
    CHud::bScriptDontDisplayRadar = !preset->show_radar;
    CHud::bScriptDontDisplayAreaName = !preset->show_area_name;
    CHud::bScriptDontDisplayVehicleName = !preset->show_vehicle_name;
    CHud::bDrawClock = preset->show_clock;
    CHud::bDrawingVitalStats = preset->show_vital_stats;
    CHud::m_bDraw3dMarkers = preset->show_3d_markers;
#else
    CHud::m_Wants_To_Draw_Hud = preset->show_master_hud;
    CHud::bScriptDontDisplayRadar = !preset->show_radar;
    CHud::bScriptDontDisplayAreaName = !preset->show_area_name;
    CHud::bScriptDontDisplayVehicleName = !preset->show_vehicle_name;
    CHud::bDrawClock = preset->show_clock;
    CHud::bDrawingVitalStats = preset->show_vital_stats;
    CHud::m_bDraw3dMarkers = preset->show_3d_markers;
#endif
}

void InterfaceEditor::RestoreOriginalValues() {
    if (!original_values_captured_) {
        return;
    }

#ifndef KEYBINDER_TESTING
    HudColour.SetRGBAValue(
        kHealthPaletteSlot,
        original_health_palette_[0],
        original_health_palette_[1],
        original_health_palette_[2],
        original_health_palette_[3]);
    HudColour.SetRGBAValue(
        kArmorPaletteSlot,
        original_armor_palette_[0],
        original_armor_palette_[1],
        original_armor_palette_[2],
        original_armor_palette_[3]);
    HudColour.SetRGBAValue(
        kMoneyPaletteSlot,
        original_money_palette_[0],
        original_money_palette_[1],
        original_money_palette_[2],
        original_money_palette_[3]);
    CHud::m_Wants_To_Draw_Hud = original_show_master_hud_;
    CHud::bScriptDontDisplayRadar = !original_show_radar_;
    CHud::bScriptDontDisplayAreaName = !original_show_area_name_;
    CHud::bScriptDontDisplayVehicleName = !original_show_vehicle_name_;
    CHud::bDrawClock = original_show_clock_;
    CHud::bDrawingVitalStats = original_show_vital_stats_;
    CHud::m_bDraw3dMarkers = original_show_3d_markers_;
#else
    CHud::m_Wants_To_Draw_Hud = original_show_master_hud_;
    CHud::bScriptDontDisplayRadar = !original_show_radar_;
    CHud::bScriptDontDisplayAreaName = !original_show_area_name_;
    CHud::bScriptDontDisplayVehicleName = !original_show_vehicle_name_;
    CHud::bDrawClock = original_show_clock_;
    CHud::bDrawingVitalStats = original_show_vital_stats_;
    CHud::m_bDraw3dMarkers = original_show_3d_markers_;
#endif
    original_values_captured_ = false;
}

void InterfaceEditor::ReadSampLiveState() {
    // SA-MP live state remains runtime-only. The Interface Editor reads these
    // values directly from CChat / CScoreboard in the UI layer when available.
}

void InterfaceEditor::RenderVitalStatsNumbers(ImDrawList* drawList, bool overlayVisible, float uiScale) {
    if (drawList == nullptr) {
        return;
    }

    InterfacePreset* preset = GetPreset(GetActivePreset());
    if (preset == nullptr) {
        hp_number_dragging_ = false;
        armor_number_dragging_ = false;
        return;
    }

    const bool hpMoveModeActive = hp_number_move_mode_ && overlayVisible;
    const bool armorMoveModeActive = armor_number_move_mode_ && overlayVisible;
    int healthValue = 0;
    int armorValue = 0;
    const bool hasLiveStats = TryReadPlayerVitalStats(samp_bridge_, healthValue, armorValue);

    if (!hasLiveStats && !hpMoveModeActive && !armorMoveModeActive) {
        hp_number_dragging_ = false;
        armor_number_dragging_ = false;
        return;
    }

    if (!hasLiveStats) {
        healthValue = 100;
        armorValue = 100;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        return;
    }

    preset->hp_number_pos_x = ClampSignedNormalizedOffset(preset->hp_number_pos_x);
    preset->hp_number_pos_y = ClampSignedNormalizedOffset(preset->hp_number_pos_y);
    preset->armor_number_pos_x = ClampSignedNormalizedOffset(preset->armor_number_pos_x);
    preset->armor_number_pos_y = ClampSignedNormalizedOffset(preset->armor_number_pos_y);
    preset->hp_number_scale = ClampNumberScale(preset->hp_number_scale);
    preset->armor_number_scale = ClampNumberScale(preset->armor_number_scale);

    const float renderScale = (std::max)(uiScale, 1.0f);
    const float fontSize = 18.0f * renderScale;
    const float padding = 10.0f * renderScale;
    ImFont* font = ImGui::GetFont();
    if (font == nullptr) {
        return;
    }

    const std::string healthText = std::to_string(healthValue);
    const std::string armorText = std::to_string(armorValue);
    const float healthBarWidth = ScaleHudX(viewport->Size.x, 109.0f);
    const float armorBarWidth = ScaleHudX(viewport->Size.x, 62.0f);
    const float barHeight = ScaleHudY(viewport->Size.y, 8.0f);
    const float verticalSpacing = ScaleHudY(viewport->Size.y, 6.0f);
    const float barCenterX = viewport->Pos.x + viewport->Size.x - ScaleHudX(viewport->Size.x, 141.0f) - healthBarWidth * 0.5f;
    const float healthCenterY = viewport->Pos.y + ScaleHudY(viewport->Size.y, 52.5f);
    const float armorCenterY = healthCenterY + barHeight + verticalSpacing;

    const ImU32 shadowColor = IM_COL32(0, 0, 0, 220);
    const ImU32 healthColor = IM_COL32(
        ClampColorComponent(preset->hp_color_r),
        ClampColorComponent(preset->hp_color_g),
        ClampColorComponent(preset->hp_color_b),
        ClampColorComponent(preset->hp_color_a));
    const ImU32 armorColor = IM_COL32(
        ClampColorComponent(preset->armor_color_r),
        ClampColorComponent(preset->armor_color_g),
        ClampColorComponent(preset->armor_color_b),
        ClampColorComponent(preset->armor_color_a));
    const ImVec2 shadowOffset(2.0f * renderScale, 2.0f * renderScale);

    auto persistPreset = [&]() {
        if (settings_manager_ != nullptr) {
            Settings& settings = settings_manager_->GetMutable();
            if (active_preset_ >= 0 && active_preset_ < static_cast<int>(settings.interface_editor_saved_presets.size())) {
                settings.interface_editor_active_preset_index = active_preset_;
                settings.interface_editor_saved_presets[static_cast<std::size_t>(active_preset_)] = *preset;
            }
        }
    };

    auto renderNumber = [&](const char* windowId,
                            const char* handleId,
                            const char* moveLabel,
                            const std::string& text,
                            bool visible,
                            bool moveMode,
                            bool& dragging,
                            float anchorCenterX,
                            float anchorCenterY,
                            float& offsetX,
                            float& offsetY,
                            float scale,
                            ImU32 textColor) {
        const float localFontSize = fontSize * ClampNumberScale(scale);
        const ImVec2 minimumTextSize = font->CalcTextSizeA(localFontSize, FLT_MAX, 0.0f, "999");
        const ImVec2 textSize = font->CalcTextSizeA(localFontSize, FLT_MAX, 0.0f, text.c_str());
        const ImVec2 drawSize(
            (std::max)(minimumTextSize.x, textSize.x),
            (std::max)(minimumTextSize.y, textSize.y));
        const ImVec2 position(
            anchorCenterX - textSize.x * 0.5f + viewport->Size.x * offsetX,
            anchorCenterY - textSize.y * 0.5f + viewport->Size.y * offsetY);

        if (moveMode) {
            const ImVec2 handleMin(position.x - padding, position.y - padding);
            const ImVec2 handleSize(drawSize.x + padding * 2.0f, drawSize.y + padding * 2.0f);

            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowBgAlpha(0.0f);
            const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            if (ImGui::Begin(windowId, nullptr, flags)) {
                ImGui::SetCursorScreenPos(handleMin);
                ImGui::InvisibleButton(handleId, handleSize);

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    ImVec2 nextPos(position.x + delta.x, position.y + delta.y);
                    const float minX = viewport->Pos.x + padding;
                    const float minY = viewport->Pos.y + padding;
                    const float maxX = viewport->Pos.x + viewport->Size.x - textSize.x - padding;
                    const float maxY = viewport->Pos.y + viewport->Size.y - textSize.y - padding;

                    nextPos.x = (std::max)(minX, (std::min)(nextPos.x, maxX));
                    nextPos.y = (std::max)(minY, (std::min)(nextPos.y, maxY));

                    offsetX = ClampSignedNormalizedOffset(((nextPos.x + textSize.x * 0.5f) - anchorCenterX) / viewport->Size.x);
                    offsetY = ClampSignedNormalizedOffset(((nextPos.y + textSize.y * 0.5f) - anchorCenterY) / viewport->Size.y);
                    persistPreset();
                    dragging = true;
                }

                if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    dragging = false;
                    if (runtime_preset_changed_callback_) {
                        runtime_preset_changed_callback_();
                    }
                }
            }
            ImGui::End();

            drawList->AddRect(handleMin, ImVec2(handleMin.x + handleSize.x, handleMin.y + handleSize.y), IM_COL32(123, 44, 191, 220), 6.0f, 0, 2.0f);
            drawList->AddRectFilled(ImVec2(handleMin.x, handleMin.y - 28.0f * renderScale), ImVec2(handleMin.x + 205.0f * renderScale, handleMin.y - 6.0f * renderScale), IM_COL32(10, 10, 14, 200), 6.0f);
            drawList->AddText(font, 16.0f * renderScale, ImVec2(handleMin.x + 10.0f * renderScale, handleMin.y - 24.0f * renderScale), IM_COL32(255, 255, 255, 255), moveLabel);
        } else if (dragging) {
            dragging = false;
            if (runtime_preset_changed_callback_) {
                runtime_preset_changed_callback_();
            }
        }

        if (!visible && !moveMode) {
            return;
        }

        drawList->AddText(font, localFontSize, ImVec2(position.x + shadowOffset.x, position.y + shadowOffset.y), shadowColor, text.c_str());
        drawList->AddText(font, localFontSize, position, textColor, text.c_str());
    };

    renderNumber("##HpNumberMoveLayer",
                 "##HpNumberDragHandle",
                 "Move mode: drag HP",
                 healthText,
                 preset->show_hp_number,
                 hpMoveModeActive,
                 hp_number_dragging_,
                 barCenterX,
                 healthCenterY,
                 preset->hp_number_pos_x,
                 preset->hp_number_pos_y,
                 preset->hp_number_scale,
                 healthColor);

    renderNumber("##ArmorNumberMoveLayer",
                 "##ArmorNumberDragHandle",
                 "Move mode: drag Armor",
                 armorText,
                 preset->show_armor_number,
                 armorMoveModeActive,
                 armor_number_dragging_,
                 barCenterX,
                 armorCenterY,
                 preset->armor_number_pos_x,
                 preset->armor_number_pos_y,
                 preset->armor_number_scale,
                 armorColor);
}

void InterfaceEditor::SetActivePreset(int index) {
    active_preset_ = ClampPresetIndex(index);
}

int InterfaceEditor::GetActivePreset() const {
    return active_preset_;
}

void InterfaceEditor::SetSettingsManager(SettingsManager* manager) {
    settings_manager_ = manager;
}

void InterfaceEditor::SetSAMPBridge(::SAMPBridge* bridge) {
    samp_bridge_ = bridge;
}

void InterfaceEditor::SetOnRuntimePresetChanged(std::function<void()> callback) {
    runtime_preset_changed_callback_ = std::move(callback);
}

void InterfaceEditor::SetHpNumberMoveMode(bool enabled) {
    hp_number_move_mode_ = enabled;
    if (enabled) {
        armor_number_move_mode_ = false;
        armor_number_dragging_ = false;
    } else {
        hp_number_dragging_ = false;
    }
}

bool InterfaceEditor::IsHpNumberMoveMode() const {
    return hp_number_move_mode_;
}

void InterfaceEditor::SetArmorNumberMoveMode(bool enabled) {
    armor_number_move_mode_ = enabled;
    if (enabled) {
        hp_number_move_mode_ = false;
        hp_number_dragging_ = false;
    } else {
        armor_number_dragging_ = false;
    }
}

bool InterfaceEditor::IsArmorNumberMoveMode() const {
    return armor_number_move_mode_;
}

InterfacePreset* InterfaceEditor::GetPreset(int index) {
    if (index < 0 || index >= PRESET_SLOT_COUNT) {
        return nullptr;
    }

    return &presets_[static_cast<std::size_t>(index)];
}

const InterfacePreset* InterfaceEditor::GetPreset(int index) const {
    if (index < 0 || index >= PRESET_SLOT_COUNT) {
        return nullptr;
    }

    return &presets_[static_cast<std::size_t>(index)];
}

} // namespace keybinder
