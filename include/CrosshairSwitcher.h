#pragma once

#include <string>

#include "SettingsManager.h"

namespace keybinder {

// Runtime crosshair feature: suppresses GTA native crosshair textures and
// swaps in a selected PNG texture at runtime.
class CrosshairSwitcher {
public:
    CrosshairSwitcher() = default;
    ~CrosshairSwitcher() = default;

    void Initialize();
    void Shutdown();
    void OnDeviceLost();
    void OnDeviceReset();

    bool ApplySettings(const Settings& settings);
    void RestoreDefault();

    bool IsEnabled() const { return enabled_; }
    bool IsCustomCrosshairActive() const { return customApplied_; }
    const std::string& GetConfiguredPath() const { return configuredPath_; }

private:
    static std::string NormalizePath(std::string path);

#ifndef KEYBINDER_TESTING
    bool CaptureOriginalTexture();
    bool LoadAndApplyTexture(const std::string& path);
    void* CreateTextureFromPng(const std::string& path);
    void DestroyCustomTexture();

    // Native crosshair suppression via transparent texture replacement
    bool ApplySuppressionTexture();
    void* CreateTransparentTexture(int width = 4, int height = 4);
    void DestroySuppressionTexture();

    void* originalTexture_ = nullptr;
    void* originalHudSiteM16Texture_ = nullptr;
    void* originalHudSiteRocketTexture_ = nullptr;
    void* customTexture_ = nullptr;
    void* suppressionTexture_ = nullptr;
#endif

    bool initialized_ = false;
    bool originalCaptured_ = false;
    bool enabled_ = false;
    bool customApplied_ = false;
    bool suppressionApplied_ = false;
    std::string configuredPath_;
};

} // namespace keybinder
