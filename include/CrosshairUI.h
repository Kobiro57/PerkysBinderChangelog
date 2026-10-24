#pragma once

#include <d3d9.h>

#include <functional>
#include <string>
#include <vector>

#include "SettingsManager.h"

namespace keybinder {

class CrosshairSwitcher;
class TextureManager;

class CrosshairUI {
public:
    void Render();
    void SetSettingsManager(SettingsManager* manager);
    void SetCrosshairSwitcher(CrosshairSwitcher* switcher);
    void SetTextureManager(TextureManager* manager);
    void SetDeviceGetter(std::function<IDirect3DDevice9*()> getter);
    std::function<void()> OnSettingsChanged;

private:
    void RefreshAvailablePngs();
    bool EnsureThumbnailLoaded(const std::string& path);

    SettingsManager* settingsManager_ = nullptr;
    CrosshairSwitcher* crosshairSwitcher_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    std::function<IDirect3DDevice9*()> deviceGetter_;
    std::vector<std::string> availablePngs_;
};

} // namespace keybinder
