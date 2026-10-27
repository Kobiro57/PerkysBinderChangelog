#include "../include/InterfaceEditorUI.h"

#include "../include/UIComponents.h"
#include "../include/ThemeManager.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace keybinder {

namespace {

ThemeManager& Theme() {
    return ThemeManager::Instance();
}

int ClampPresetIndex(int index) {
    return (std::max)(0, (std::min)(index, InterfaceEditor::PRESET_SLOT_COUNT - 1));
}

void EnsureSavedPresetSlots(Settings& settings) {
    constexpr std::size_t kPresetSlotCount = static_cast<std::size_t>(InterfaceEditor::PRESET_SLOT_COUNT);

    if (settings.interface_editor_saved_presets.size() > kPresetSlotCount) {
        settings.interface_editor_saved_presets.resize(kPresetSlotCount);
    }

    while (settings.interface_editor_saved_presets.size() < kPresetSlotCount) {
        settings.interface_editor_saved_presets.emplace_back();
    }
}

std::string GetPresetSlotLabel(const InterfacePreset& preset, int index) {
    const std::string slotLabel = "Slot " + std::to_string(index + 1);
    if (preset.name.empty()) {
        return slotLabel;
    }

    return slotLabel + " - " + preset.name;
}

void SyncLegacyMoneyColor(InterfacePreset& preset, HudElement element) {
    const HudElementStyle& style = preset.layout[static_cast<std::size_t>(element)];
    const InterfacePreset defaults;
    if (element == HudElement::Money) {
        preset.money_color_r = style.custom_color ? style.color[0] : defaults.money_color_r;
        preset.money_color_g = style.custom_color ? style.color[1] : defaults.money_color_g;
        preset.money_color_b = style.custom_color ? style.color[2] : defaults.money_color_b;
        preset.money_color_a = style.custom_color ? style.color[3] : defaults.money_color_a;
    }
}

} // namespace

void InterfaceEditorUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
    SyncInterfaceEditorFromSettings();
}

void InterfaceEditorUI::SetInterfaceEditor(InterfaceEditor* editor) {
    interfaceEditor_ = editor;
    SyncInterfaceEditorFromSettings();
}

void InterfaceEditorUI::SyncInterfaceEditorFromSettings() {
    if (settingsManager_ == nullptr || interfaceEditor_ == nullptr || interfaceEditor_->IsElementMoveModeActive()) {
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    EnsureSavedPresetSlots(settings);

    auto& presets = interfaceEditor_->GetPresets();
    for (int i = 0; i < InterfaceEditor::PRESET_SLOT_COUNT; ++i) {
        presets[static_cast<std::size_t>(i)] = settings.interface_editor_saved_presets[static_cast<std::size_t>(i)];
    }

    interfaceEditor_->SetActivePreset(ClampPresetIndex(settings.interface_editor_active_preset_index));
}

void InterfaceEditorUI::UpdateEditingPresetName(int selectedSlot) {
    if (settingsManager_ == nullptr) {
        return;
    }

    // Only update the editing buffer when the slot changes or on first render
    if (selectedSlot != lastSelectedSlot_) {
        Settings& settings = settingsManager_->GetMutable();
        EnsureSavedPresetSlots(settings);
        InterfacePreset& currentPreset = settings.interface_editor_saved_presets[static_cast<std::size_t>(selectedSlot)];
        editingPresetName_ = currentPreset.name;
        lastSelectedSlot_ = selectedSlot;
    }
}

void InterfaceEditorUI::Render() {
    if (settingsManager_ == nullptr || interfaceEditor_ == nullptr) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::Danger), "Interface Editor UI nicht initialisiert");
        return;
    }

    if (interfaceEditor_->IsElementMoveModeActive()) {
        UIComponents::BeginCard("interface_move_active");
        UIComponents::CardHeader("Interface wird verschoben", "Mit der Maus verschieben, Esc speichert die Position.");
        if (UIComponents::PrimaryButton("Position speichern")) interfaceEditor_->FinishElementMove();
        UIComponents::EndCard();
        return;
    }
    Settings& settings = settingsManager_->GetMutable();
    EnsureSavedPresetSlots(settings);
    SyncInterfaceEditorFromSettings();

    bool gtaPresetChanged = false;
    HudElement requestedMove = HudElement::Count;

    UIComponents::BeginCard("interface_editor_presets", 0.0f);
    UIComponents::CardHeader("Preset-Verwaltung", "Positionen und Farben für jedes GTA-HUD-Element");

    int selectedSlot = ClampPresetIndex(settings.interface_editor_active_preset_index);
    std::array<std::string, InterfaceEditor::PRESET_SLOT_COUNT> slotLabels;
    std::array<const char*, InterfaceEditor::PRESET_SLOT_COUNT> slotLabelPtrs{};

    for (int i = 0; i < InterfaceEditor::PRESET_SLOT_COUNT; ++i) {
        slotLabels[static_cast<std::size_t>(i)] = GetPresetSlotLabel(settings.interface_editor_saved_presets[static_cast<std::size_t>(i)], i);
        slotLabelPtrs[static_cast<std::size_t>(i)] = slotLabels[static_cast<std::size_t>(i)].c_str();
    }

    if (UIComponents::Combo("Preset-Slot", &selectedSlot, slotLabelPtrs.data(), static_cast<int>(slotLabelPtrs.size()))) {
        settings.interface_editor_active_preset_index = selectedSlot;
        gtaPresetChanged = true;
    }

    InterfacePreset& currentPreset = settings.interface_editor_saved_presets[static_cast<std::size_t>(selectedSlot)];

    // Update the editing buffer when slot changes or on first render
    UpdateEditingPresetName(selectedSlot);

    // Use persistent string to maintain ImGui focus state across frames
    if (UIComponents::InputText("Preset-Name", &editingPresetName_, "Name eingeben...")) {
        currentPreset.name = editingPresetName_;
        gtaPresetChanged = true;
    }

    UIComponents::InfoRow("Aktiver Slot", std::to_string(selectedSlot + 1).c_str());
    UIComponents::InfoRow("Preset-Name", currentPreset.name.empty() ? "(unbenannt)" : currentPreset.name.c_str());

    ImGui::Spacing();
    if (UIComponents::SecondaryButton("Auf Standard zurücksetzen")) {
        currentPreset = InterfacePreset{};
        editingPresetName_ = currentPreset.name;
        gtaPresetChanged = true;
    }

    ImGui::SameLine();
    if (UIComponents::DangerButton("Preset löschen")) {
        currentPreset = InterfacePreset{};
        editingPresetName_ = currentPreset.name;
        gtaPresetChanged = true;
    }

    if (currentPreset.name.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Dieser Slot ist noch unbenannt. Alle Änderungen darunter werden direkt diesem Preset zugeordnet.");
        ImGui::PopStyleColor();
    }
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap)));

    UIComponents::SectionHeader("Interface anpassen", "Jedes Element einzeln verschieben oder färben. Esc speichert im Verschiebemodus.");
    ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextMuted));
    ImGui::TextWrapped("Die Einstellungen gelten für das GTA-HUD. Servereigene Textanzeigen und Menüs werden vom Server gestaltet.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap)));

    for (const HudElementDefinition& definition : kHudElements) {
        const std::size_t index = static_cast<std::size_t>(definition.element);
        HudElementStyle& style = currentPreset.layout[index];
        ImGui::PushID(definition.key);
        UIComponents::BeginCard("interface_element");
        UIComponents::CardHeader(definition.label);
        if (ImGui::BeginTable("interface_element_controls", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            UIComponents::Separator("Position");
            if (UIComponents::PrimaryButton("Mit der Maus verschieben")) requestedMove = definition.element;
            float horizontal = style.offset_x * 100.0f;
            float vertical = style.offset_y * 100.0f;
            bool positionChanged = UIComponents::InputFloat("Horizontal (%)", &horizontal);
            positionChanged = UIComponents::InputFloat("Vertikal (%)", &vertical) || positionChanged;
            if (positionChanged) {
                style.offset_x = horizontal / 100.0f;
                style.offset_y = vertical / 100.0f;
                style = SanitizeHudElementStyle(style);
                gtaPresetChanged = true;
            }
            if (UIComponents::SecondaryButton("Position zurücksetzen")) {
                style.offset_x = style.offset_y = 0.0f;
                gtaPresetChanged = true;
            }
            ImGui::TableNextColumn();
            UIComponents::Separator("Farbe");
            if (UIComponents::Toggle("Eigene Farbe", &style.custom_color)) {
                SyncLegacyMoneyColor(currentPreset, definition.element);
                gtaPresetChanged = true;
            }
            if (style.custom_color) {
                ImVec4 color(style.color[0], style.color[1], style.color[2], style.color[3]);
                if (UIComponents::ColorPicker("Farbe auswählen", &color)) {
                    style.color = {{color.x, color.y, color.z, color.w}};
                    style = SanitizeHudElementStyle(style);
                    SyncLegacyMoneyColor(currentPreset, definition.element);
                    gtaPresetChanged = true;
                }
            } else {
                UIComponents::InfoRow("Farbe", "Originalfarbe");
            }
            if (UIComponents::SecondaryButton("Originalfarbe wiederherstellen")) {
                style.custom_color = false;
                SyncLegacyMoneyColor(currentPreset, definition.element);
                gtaPresetChanged = true;
            }
            ImGui::EndTable();
        }
        UIComponents::EndCard();
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap)));
    }

    if (gtaPresetChanged) {
        settings.interface_editor_active_preset_index = ClampPresetIndex(settings.interface_editor_active_preset_index);
        SyncInterfaceEditorFromSettings();

        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
    if (requestedMove != HudElement::Count) {
        SyncInterfaceEditorFromSettings();
        if (interfaceEditor_->StartElementMove(requestedMove) && OnRequestMoveMode) OnRequestMoveMode();
    }
}

} // namespace keybinder
