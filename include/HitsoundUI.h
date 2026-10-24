#pragma once

#include "SettingsManager.h"

#include <functional>
#include <string>
#include <vector>

namespace keybinder {

class HitsoundPlayer;

class HitsoundUI {
public:
    void Render();
    void SetSettingsManager(SettingsManager* manager);
    void SetHitsoundPlayer(HitsoundPlayer* player);
    std::function<void()> OnSettingsChanged;

private:
    void RefreshAvailableWavs();

    SettingsManager* settingsManager_ = nullptr;
    HitsoundPlayer* hitsoundPlayer_ = nullptr;
    std::vector<std::string> availableWavs_;
};

} // namespace keybinder
