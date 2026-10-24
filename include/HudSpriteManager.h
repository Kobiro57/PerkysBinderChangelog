#pragma once

#include "SettingsManager.h"

#include <string>
#include <cstddef>

namespace keybinder {

class HudSpriteManager {
public:
    HudSpriteManager() = default;
    ~HudSpriteManager() = default;

    void Initialize();
    void Shutdown();
    void OnDeviceLost();
    void OnDeviceReset();

    bool ApplySettings(const Settings& settings);
    void RestoreDefault();

    bool IsApplied() const { return applied_; }
    bool IsFistCustomApplied() const { return fistCustomApplied_; }

#ifdef KEYBINDER_TESTING
    bool IsInitializedForTesting() const { return initialized_; }
#endif

private:
#ifndef KEYBINDER_TESTING
    bool ApplyCustomFistTexture(const std::string& path);
    void RestoreCustomFistTexture();
    void DestroyCustomFistTexture();
#endif

    bool initialized_ = false;
    bool enabled_ = false;
    bool applied_ = false;
    bool fistCustomApplied_ = false;
    std::string configuredFistPath_;

};

} // namespace keybinder
