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

ImVec4 LoadColor(float r, float g, float b, float a) {
    return ImVec4(r, g, b, a);
}

void StoreColor(InterfacePreset& preset, const ImVec4& color, float InterfacePreset::*r, float InterfacePreset::*g, float InterfacePreset::*b, float InterfacePreset::*a) {
    preset.*r = color.x;
    preset.*g = color.y;
    preset.*b = color.z;
    preset.*a = color.w;
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
    if (settingsManager_ == nullptr || interfaceEditor_ == nullptr) {
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

    Settings& settings = settingsManager_->GetMutable();
    EnsureSavedPresetSlots(settings);
    SyncInterfaceEditorFromSettings();

    bool gtaPresetChanged = false;

    UIComponents::BeginCard("interface_editor_presets", 0.0f);
    UIComponents::CardHeader("Preset-Verwaltung", "Slots speichern Farben und Zonentext");

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

    UIComponents::Separator("HUD-Farben");
    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::CardGap)));

    if (ImGui::BeginTable("interface_editor_color_table", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImVec4 hpColor = LoadColor(currentPreset.hp_color_r, currentPreset.hp_color_g, currentPreset.hp_color_b, currentPreset.hp_color_a);
        if (UIComponents::ColorPicker("HP", &hpColor)) {
            StoreColor(currentPreset, hpColor, &InterfacePreset::hp_color_r, &InterfacePreset::hp_color_g, &InterfacePreset::hp_color_b, &InterfacePreset::hp_color_a);
            gtaPresetChanged = true;
        }

        ImGui::TableNextColumn();
        ImVec4 armorColor = LoadColor(currentPreset.armor_color_r, currentPreset.armor_color_g, currentPreset.armor_color_b, currentPreset.armor_color_a);
        if (UIComponents::ColorPicker("Armor", &armorColor)) {
            StoreColor(currentPreset, armorColor, &InterfacePreset::armor_color_r, &InterfacePreset::armor_color_g, &InterfacePreset::armor_color_b, &InterfacePreset::armor_color_a);
            gtaPresetChanged = true;
        }

        ImGui::TableNextColumn();
        ImVec4 moneyColor = LoadColor(currentPreset.money_color_r, currentPreset.money_color_g, currentPreset.money_color_b, currentPreset.money_color_a);
        if (UIComponents::ColorPicker("Geld", &moneyColor)) {
            StoreColor(currentPreset, moneyColor, &InterfacePreset::money_color_r, &InterfacePreset::money_color_g, &InterfacePreset::money_color_b, &InterfacePreset::money_color_a);
            gtaPresetChanged = true;
        }

        ImGui::EndTable();
    }

    if (gtaPresetChanged) {
        settings.interface_editor_active_preset_index = ClampPresetIndex(settings.interface_editor_active_preset_index);
        SyncInterfaceEditorFromSettings();

        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
}

} // namespace keybinder
