#pragma once

#include "UIComponents.h"
#include "ThemeManager.h"
#include "MessageVariables.h"
#include "imgui-1.92.7/imgui.h"

namespace keybinder {

// Shared variable help for text bindings and automatic combat messages.
inline void RenderMessageVariablesModal(const char* popupId, bool* open, bool combatMessage) {
    if (!*open) return;

    auto& theme = ThemeManager::Instance();
    const float scale = theme.GetAppliedScale();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(580.0f * scale, 500.0f * scale), ImGuiCond_Appearing);
    ImGui::OpenPopup(popupId);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, theme.GetColor(ThemeColorRole::SurfaceElevated));
    if (ImGui::BeginPopupModal(popupId, open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("Variablen immer mit geschweiften Klammern schreiben, z.B. {total_kills}. Die Werte stammen direkt aus dem Spiel oder dem gespeicherten Binderstand.");
        if (!combatMessage) ImGui::TextWrapped("{input} fragt beim Ausl\xC3\xB6sen des Binds einmal nach deinem Text. Enter best\xC3\xA4tigt, Esc bricht ab.");
        ImGui::Spacing();
        ImGui::BeginChild("VariableListScroll", ImVec2(0.0f, -50.0f * scale));
        UIComponents::BeginCard("VariableList", 0.0f, false);
        auto renderVar = [&theme, scale](const char* tag, const char* description) {
            ImGui::TextColored(theme.GetColor(ThemeColorRole::Accent), "{%s}", tag);
            ImGui::SameLine(150.0f * scale);
            ImGui::TextColored(theme.GetColor(ThemeColorRole::TextSecondary), "%s", description);
        };
        if (combatMessage) {
            for (const auto& variable : kCombatMessageVariables) renderVar(variable.tag, variable.description);
        } else {
            for (const auto& variable : kTextBindingVariables) renderVar(variable.tag, variable.description);
        }
        UIComponents::EndCard();
        ImGui::EndChild();
        if (UIComponents::PrimaryButton("Schließen", ImVec2(120.0f * scale, 0.0f))) {
            *open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace keybinder
