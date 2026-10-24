#pragma once

#include "SettingsManager.h"

namespace keybinder {

class SquareMap {
public:
    SquareMap() = default;
    ~SquareMap() = default;

    void Initialize();
    void Shutdown();
    void OnDeviceLost();
    void OnDeviceReset();

    bool ApplySettings(const Settings& settings);
    void RestoreDefault();

    bool IsEnabled() const { return enabled_; }
    bool IsApplied() const { return applied_; }

#ifdef KEYBINDER_TESTING
    bool IsInitializedForTesting() const { return initialized_; }
#endif

private:
#ifndef KEYBINDER_TESTING
    bool CaptureOriginalTexture();
    bool LoadAndApplyTexture();
    void* CreateTextureFromEmbeddedPng();
    void DestroyCustomTexture();

    bool ApplyPatches();
    void RestorePatches();

    void* originalRadarDiscTexture_ = nullptr;
    void* customTexture_ = nullptr;
    bool originalCaptured_ = false;
    bool patchesCaptured_ = false;
    bool patchesApplied_ = false;
    unsigned char originalLimitRadarPointBytes_[5] = {};
    unsigned char originalDrawRadarMaskBytes_[5] = {};
#endif

    bool initialized_ = false;
    bool enabled_ = false;
    bool applied_ = false;
};

} // namespace keybinder
