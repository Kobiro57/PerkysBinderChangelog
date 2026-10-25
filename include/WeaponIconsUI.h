#pragma once

#include <d3d9.h>
#include <functional>
#include <string>
#include <utility>

namespace keybinder {
class SettingsManager;
class WeaponIconManager;
class TextureManager;

class WeaponIconsUI {
public:
    void Render();
    void SetSettingsManager(SettingsManager* manager) { settingsManager_ = manager; }
    void SetWeaponIconManager(WeaponIconManager* manager) { weaponIconManager_ = manager; }
    void SetTextureManager(TextureManager* manager) { textureManager_ = manager; }
    void SetDeviceGetter(std::function<IDirect3DDevice9*()> getter) { deviceGetter_ = std::move(getter); }
    std::function<void()> OnSettingsChanged;

private:
    void PersistAndApply();
    bool EnsureThumbnailLoaded(const std::string& path);
    SettingsManager* settingsManager_ = nullptr;
    WeaponIconManager* weaponIconManager_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    std::function<IDirect3DDevice9*()> deviceGetter_;
    int selectedWeaponId_ = 24;
};
} // namespace keybinder
