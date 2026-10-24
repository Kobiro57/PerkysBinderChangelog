#include "../include/SquareMapUI.h"

#include "../include/SquareMap.h"
#include "../include/UIComponents.h"

#include "../include/imgui-1.92.7/imgui.h"

namespace keybinder {

void SquareMapUI::SetSettingsManager(SettingsManager* manager) {
    settingsManager_ = manager;
}

void SquareMapUI::SetSquareMap(SquareMap* squareMap) {
    squareMap_ = squareMap;
}

void SquareMapUI::Render() {
    if (settingsManager_ == nullptr || squareMap_ == nullptr) {
        ImGui::TextUnformatted("Squaremap UI nicht initialisiert");
        return;
    }

    Settings& settings = settingsManager_->GetMutable();
    bool changed = false;

    UIComponents::BeginCard("squaremap-options", 0.0f);
    if (UIComponents::Toggle("Squaremap aktivieren", &settings.squaremap_enabled)) {
        changed = true;
    }

    UIComponents::EndCard();

    if (changed) {
        if (!squareMap_->ApplySettings(settings)) {
            settings.squaremap_enabled = false;
        }

        if (OnSettingsChanged) {
            OnSettingsChanged();
        }
    }
}

} // namespace keybinder
