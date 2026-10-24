#pragma once

#include <functional>

#include "SettingsManager.h"

namespace keybinder {

class SquareMap;

class SquareMapUI {
public:
    void Render();
    void SetSettingsManager(SettingsManager* manager);
    void SetSquareMap(SquareMap* squareMap);

    std::function<void()> OnSettingsChanged;

private:
    SettingsManager* settingsManager_ = nullptr;
    SquareMap* squareMap_ = nullptr;
};

} // namespace keybinder
