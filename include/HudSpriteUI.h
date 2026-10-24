#pragma once

#include "SettingsManager.h"
#include "TextureManager.h"

#include <d3d9.h>
#include <functional>
#include <string>
#include <vector>

namespace keybinder {

class HudSpriteManager;

class HudSpriteUI {
public:
    void RenderFistLibrary();
    void SetSettingsManager(SettingsManager* manager);
    void SetHudSpriteManager(HudSpriteManager* manager);
    void SetTextureManager(TextureManager* manager);
    void SetDeviceGetter(std::function<IDirect3DDevice9*()> getter);

    std::function<void()> OnSettingsChanged;

private:
    void RefreshAvailableFists();
    bool EnsureThumbnailLoaded(const std::string& path);
    SettingsManager* settingsManager_ = nullptr;
    HudSpriteManager* hudSpriteManager_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    std::function<IDirect3DDevice9*()> deviceGetter_;
    std::vector<std::string> availableFistPngs_;
};

} // namespace keybinder
