#include "../include/InterfaceEditor.h"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/ThemeManager.h"
#include "../include/imgui-1.92.7/imgui.h"

#ifndef KEYBINDER_TESTING
#include "plugin.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CHud.h"
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

unsigned char ClampColorComponent(float value) {
    const float clamped = (std::max)(0.0f, (std::min)(value, 1.0f));
    return static_cast<unsigned char>(std::lround(clamped * 255.0f));
}

int ClampPresetIndex(int index) {
    return (std::max)(0, (std::min)(index, InterfaceEditor::PRESET_SLOT_COUNT - 1));
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
    // NativeHudEditor owns all native draw hooks. This class owns presets and
    // the move interaction, so no second hook of CHud::Draw is necessary.
    return true;
}

void InterfaceEditor::Shutdown() {
    FinishElementMove();
}

void InterfaceEditor::ApplyCurrentPreset() {
    if (!original_values_captured_) {
        original_show_master_hud_ = CHud::m_Wants_To_Draw_Hud;
        original_show_radar_ = !CHud::bScriptDontDisplayRadar;
        original_show_area_name_ = !CHud::bScriptDontDisplayAreaName;
        original_show_vehicle_name_ = !CHud::bScriptDontDisplayVehicleName;
        original_show_clock_ = CHud::bDrawClock;
        original_show_vital_stats_ = CHud::bDrawingVitalStats;
        original_show_3d_markers_ = CHud::m_bDraw3dMarkers;
        original_values_captured_ = true;
    }

    const InterfacePreset* preset = GetPreset(GetActivePreset());
    if (preset == nullptr) return;

    // Colors are applied only inside the corresponding native draw call.
    // Keeping the shared GTA palette unchanged avoids recoloring other HUD text.
    CHud::m_Wants_To_Draw_Hud = preset->show_master_hud;
    CHud::bScriptDontDisplayRadar = !preset->show_radar;
    CHud::bScriptDontDisplayAreaName = !preset->show_area_name;
    CHud::bScriptDontDisplayVehicleName = !preset->show_vehicle_name;
    CHud::bDrawClock = preset->show_clock;
    CHud::bDrawingVitalStats = preset->show_vital_stats;
    CHud::m_bDraw3dMarkers = preset->show_3d_markers;
}

void InterfaceEditor::RestoreOriginalValues() {
    if (!original_values_captured_) return;
    CHud::m_Wants_To_Draw_Hud = original_show_master_hud_;
    CHud::bScriptDontDisplayRadar = !original_show_radar_;
    CHud::bScriptDontDisplayAreaName = !original_show_area_name_;
    CHud::bScriptDontDisplayVehicleName = !original_show_vehicle_name_;
    CHud::bDrawClock = original_show_clock_;
    CHud::bDrawingVitalStats = original_show_vital_stats_;
    CHud::m_bDraw3dMarkers = original_show_3d_markers_;
    original_values_captured_ = false;
}

void InterfaceEditor::ReadSampLiveState() {
    // SA-MP live state remains runtime-only. The Interface Editor reads these
    // values directly from CChat / CScoreboard in the UI layer when available.
}

void InterfaceEditor::SetElementBoundsProvider(std::function<bool(HudElement, float&, float&, float&, float&)> provider) {
    element_bounds_provider_ = std::move(provider);
}

bool InterfaceEditor::StartElementMove(HudElement element) {
    if (static_cast<std::size_t>(element) >= kHudElementCount) return false;
    if (IsElementMoveModeActive()) FinishElementMove();
    moving_element_ = element;
    moving_preset_ = active_preset_;
    hp_number_move_mode_ = armor_number_move_mode_ = false;
    hp_number_dragging_ = armor_number_dragging_ = false;
    InterfacePreset* preset = GetPreset(moving_preset_);
    preset->layout[static_cast<std::size_t>(element)] = SanitizeHudElementStyle(preset->layout[static_cast<std::size_t>(element)]);
    return true;
}

bool InterfaceEditor::MoveElementBy(float normalizedDeltaX, float normalizedDeltaY) {
    if (!IsElementMoveModeActive() || !std::isfinite(normalizedDeltaX) || !std::isfinite(normalizedDeltaY)) return false;
    InterfacePreset* preset = GetPreset(moving_preset_);
    HudElementStyle& style = preset->layout[static_cast<std::size_t>(moving_element_)];
    style.offset_x += normalizedDeltaX;
    style.offset_y += normalizedDeltaY;
    style = SanitizeHudElementStyle(style);
    return true;
}

void InterfaceEditor::FinishElementMove() {
    if (!IsElementMoveModeActive()) return;
    const int completedSlot = moving_preset_;
    moving_element_ = HudElement::Count;
    moving_preset_ = -1;
    if (settings_manager_ != nullptr) {
        Settings& settings = settings_manager_->GetMutable();
        if (settings.interface_editor_saved_presets.size() < PRESET_SLOT_COUNT) {
            settings.interface_editor_saved_presets.resize(PRESET_SLOT_COUNT);
        }
        settings.interface_editor_saved_presets[static_cast<std::size_t>(completedSlot)] = *GetPreset(completedSlot);
    }
    if (runtime_preset_changed_callback_) runtime_preset_changed_callback_();
}

bool InterfaceEditor::IsElementMoveModeActive() const {
    return moving_preset_ >= 0 && moving_preset_ < PRESET_SLOT_COUNT && static_cast<std::size_t>(moving_element_) < kHudElementCount;
}

HudElement InterfaceEditor::GetMovingElement() const {
    return IsElementMoveModeActive() ? moving_element_ : HudElement::Count;
}

void InterfaceEditor::RenderElementMoveMode(ImDrawList* drawList, float uiScale) {
    if (!IsElementMoveModeActive()) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        FinishElementMove();
        return;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (drawList == nullptr || viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) return;

    const std::size_t elementIndex = static_cast<std::size_t>(moving_element_);
    const HudElementStyle& style = GetPreset(moving_preset_)->layout[elementIndex];
    // Fallback handles also allow moving elements which are currently invisible
    // (for example armour without a vest). Observed native bounds take priority.
    constexpr std::array<std::array<float, 4>, kHudElementCount> defaultRects{{
        {{499, 77, 109, 10}}, {{546, 48, 62, 10}}, {{546, 62, 62, 10}},
        {{480, 89, 128, 24}}, {{548, 22, 60, 24}}, {{546, 20, 62, 62}},
        {{540, 64, 68, 20}}, {{490, 114, 118, 24}}, {{400, 344, 208, 32}},
        {{400, 344, 208, 32}}, {{40, 344, 94, 76}}
    }};
    const auto& fallback = defaultRects[elementIndex];
    float left = viewport->Size.x * (fallback[0] / 640.0f + style.offset_x);
    float top = viewport->Size.y * (fallback[1] / 448.0f + style.offset_y);
    float right = left + viewport->Size.x * fallback[2] / 640.0f;
    float bottom = top + viewport->Size.y * fallback[3] / 448.0f;
    if (element_bounds_provider_) {
        float observedLeft = 0.0f, observedTop = 0.0f, observedWidth = 0.0f, observedHeight = 0.0f;
        if (element_bounds_provider_(moving_element_, observedLeft, observedTop, observedWidth, observedHeight)
            && std::isfinite(observedLeft) && std::isfinite(observedTop) && std::isfinite(observedWidth) && std::isfinite(observedHeight)
            && observedWidth > 0.0f && observedHeight > 0.0f) {
            left = observedLeft;
            top = observedTop;
            right = observedLeft + observedWidth;
            bottom = observedTop + observedHeight;
        }
    }

    const float scale = (std::max)(0.5f, uiScale);
    const float pad = 6.0f * scale;
    const ImVec2 handleMin(viewport->Pos.x + left - pad, viewport->Pos.y + top - pad);
    const ImVec2 handleSize((std::max)(32.0f * scale, right - left + 2 * pad), (std::max)(28.0f * scale, bottom - top + 2 * pad));
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##NativeInterfaceMoveLayer", nullptr, flags)) {
        ImGui::SetCursorScreenPos(handleMin);
        ImGui::InvisibleButton("##NativeInterfaceDragHandle", handleSize);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            const float width = right - left, height = bottom - top;
            const float nextLeft = (std::max)(0.0f, (std::min)(left + delta.x, (std::max)(0.0f, viewport->Size.x - width)));
            const float nextTop = (std::max)(0.0f, (std::min)(top + delta.y, (std::max)(0.0f, viewport->Size.y - height)));
            MoveElementBy((nextLeft - left) / viewport->Size.x, (nextTop - top) / viewport->Size.y);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    const auto& theme = ThemeManager::Instance();
    const ImU32 accent = ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Accent));
    drawList->AddRect(handleMin, ImVec2(handleMin.x + handleSize.x, handleMin.y + handleSize.y), accent, 5.0f * scale, 0, 2.0f * scale);
    const std::string hint = std::string(kHudElements[elementIndex].label) + ": mit der Maus verschieben | Esc speichert";
    const ImVec2 hintSize = ImGui::CalcTextSize(hint.c_str());
    const ImVec2 hintMin(viewport->Pos.x + (viewport->Size.x - hintSize.x) * 0.5f - 10.0f * scale, viewport->Pos.y + 18.0f * scale);
    const ImVec2 hintMax(hintMin.x + hintSize.x + 20.0f * scale, hintMin.y + hintSize.y + 16.0f * scale);
    ImVec4 background = theme.GetColor(ThemeColorRole::Surface);
    background.w = 0.93f;
    drawList->AddRectFilled(hintMin, hintMax, ImGui::GetColorU32(background), 6.0f * scale);
    drawList->AddRect(hintMin, hintMax, accent, 6.0f * scale);
    drawList->AddText(ImVec2(hintMin.x + 10.0f * scale, hintMin.y + 8.0f * scale), ImGui::GetColorU32(theme.GetColor(ThemeColorRole::TextPrimary)), hint.c_str());
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
    const int nextPreset = ClampPresetIndex(index);
    if (nextPreset != active_preset_) FinishElementMove();
    active_preset_ = nextPreset;
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
