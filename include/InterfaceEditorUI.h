#pragma once

#include "SettingsManager.h"
#include "InterfaceEditor.h"

#include <functional>
#include <string>

namespace keybinder {

class InterfaceEditorUI {
public:
    void Render();
    void SetSettingsManager(SettingsManager* manager);
    void SetInterfaceEditor(InterfaceEditor* editor);
    std::function<void()> OnSettingsChanged;
    std::function<void()> OnRequestMoveMode;

private:
    void SyncInterfaceEditorFromSettings();
    void UpdateEditingPresetName(int selectedSlot);

    SettingsManager* settingsManager_ = nullptr;
    InterfaceEditor* interfaceEditor_ = nullptr;

    // Persistent storage for the preset name being edited to maintain ImGui focus
    std::string editingPresetName_;
    int lastSelectedSlot_ = -1;
};

} // namespace keybinder
