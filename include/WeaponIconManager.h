#pragma once

#include "SettingsManager.h"
#include "WeaponIconCatalog.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keybinder {

class WeaponIconManager {
public:
    ~WeaponIconManager();
    void Initialize();
    void Shutdown();
    void OnDeviceLost();
    void OnDeviceReset();
    bool ApplySettings(const Settings& settings);
    bool RefreshLibrary();
    std::vector<std::string> GetAvailableIcons(int weaponId) const;
    std::string GetConfiguredIcon(int weaponId) const;
    bool IsEnabled() const { return enabled_; }
    bool IsAppliedForWeapon(int weaponId) const;
    const std::string& GetLastError() const { return lastError_; }

    // The native draw hook calls the original geometry/rendering path and
    // substitutes a texture only during that local HUD weapon-icon draw.
    void* GetTextureForWeapon(int weaponId) const;
#ifdef KEYBINDER_TESTING
    struct TestingHooks {
        std::function<void*(const std::string&)> loadTexture;
        std::function<void(void*)> destroyTexture;
    };
    void SetTestingHooks(TestingHooks hooks) { testingHooks_ = std::move(hooks); }
    bool IsDeviceReadyForTesting() const { return deviceReady_; }
#endif

private:
    struct LoadedIcon { std::string path; void* texture = nullptr; };
    bool RebuildTextures(bool forceReload);
    void ReleaseTextures();
    bool InstallNativeHooks();
    void RemoveNativeHooks();
    std::string ResolveConfiguredPath(int weaponId) const;
    bool initialized_ = false;
    bool deviceReady_ = false;
    bool enabled_ = false;
    std::vector<WeaponIconSelection> selections_;
    std::unordered_map<int, std::vector<std::string>> library_;
    std::unordered_map<int, LoadedIcon> textures_;
    std::string lastError_;
#ifdef KEYBINDER_TESTING
    TestingHooks testingHooks_;
#endif
};

} // namespace keybinder
