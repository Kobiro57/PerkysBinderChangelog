#pragma once

#include <d3d9.h>

#include "SettingsManager.h"
#include <functional>
#include <string>
#include <vector>

namespace keybinder {

class Hitmarker;
class TextureManager;

class HitmarkerUI {
public:
    void Render();
    void SetSettingsManager(SettingsManager* manager);
    void SetHitmarker(Hitmarker* hitmarker);
    void SetTextureManager(TextureManager* manager);
    void SetDeviceGetter(std::function<IDirect3DDevice9*()> getter);
    std::function<void()> OnSettingsChanged;

private:
    void RefreshAvailablePngs();
    bool EnsureThumbnailLoaded(const std::string& path);

    SettingsManager* settingsManager_ = nullptr;
    Hitmarker* hitmarker_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    std::function<IDirect3DDevice9*()> deviceGetter_;
    std::vector<std::string> availablePngs_;
};

} // namespace keybinder
