#include "../include/HudSpriteManager.h"

#include "../include/EmbeddedRwTexture.h"
#include "../include/Logger.h"
#include "../include/PersistentPaths.h"

#include "../include/plugin-sdk/plugin_sa/game_sa/CHud.h"
#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwcore.h"
#include <windows.h>
#endif

#include <algorithm>
#include <string>

namespace keybinder {

namespace {

std::string NormalizeFistPath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');

    const std::string prefix = "fists";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        const char separator = path[prefix.size()];
        if (separator != '\\') {
            path.insert(prefix.size(), "\\");
        }
    }

    return path;
}

#ifndef KEYBINDER_TESTING
void* g_originalFistTexture = nullptr;
void* g_customFistTexture = nullptr;
bool g_originalFistCaptured = false;
std::string g_customFistPath;

void** ResolveFistTextureSlot() {
    if (CHud::Sprites == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void**>(&CHud::Sprites[HUDSPRITE_FIST].m_pTexture);
}
#endif

} // namespace

void HudSpriteManager::Initialize() {
    initialized_ = true;

    if (!configuredFistPath_.empty()) {
        Settings settings;
        settings.fist_texture_path = configuredFistPath_;
        ApplySettings(settings);
    }
}

void HudSpriteManager::Shutdown() {
    RestoreDefault();
#ifndef KEYBINDER_TESTING
    DestroyCustomFistTexture();
    g_originalFistTexture = nullptr;
    g_originalFistCaptured = false;
#endif
    initialized_ = false;
    enabled_ = false;
    configuredFistPath_.clear();
}

void HudSpriteManager::OnDeviceLost() {
    RestoreDefault();
#ifndef KEYBINDER_TESTING
    DestroyCustomFistTexture();
#endif
}

void HudSpriteManager::OnDeviceReset() {
    if (!configuredFistPath_.empty()) {
        Settings settings;
        settings.fist_texture_path = configuredFistPath_;
        ApplySettings(settings);
    }
}

bool HudSpriteManager::ApplySettings(const Settings& settings) {
    enabled_ = false;
    configuredFistPath_ = NormalizeFistPath(settings.fist_texture_path);

    if (!initialized_) {
        applied_ = false;
        fistCustomApplied_ = false;
        return true;
    }

#ifdef KEYBINDER_TESTING
    fistCustomApplied_ = !configuredFistPath_.empty();
    applied_ = fistCustomApplied_;
    return true;
#else
    if (!configuredFistPath_.empty()) {
        applied_ = ApplyCustomFistTexture(configuredFistPath_);
    } else {
        RestoreCustomFistTexture();
        DestroyCustomFistTexture();
        applied_ = false;
    }

    return true;
#endif
}

void HudSpriteManager::RestoreDefault() {
#ifndef KEYBINDER_TESTING
    RestoreCustomFistTexture();
#endif
    applied_ = false;
    fistCustomApplied_ = false;
}

#ifndef KEYBINDER_TESTING
bool HudSpriteManager::ApplyCustomFistTexture(const std::string& path) {
    void** slot = ResolveFistTextureSlot();
    if (slot == nullptr) {
        Logger::Warning("[HudSpriteManager] Native fist sprite slot unavailable");
        RestoreCustomFistTexture();
        return false;
    }

    if (!g_originalFistCaptured) {
        g_originalFistTexture = *slot;
        g_originalFistCaptured = true;
    }

    const std::string resolvedPath = PersistentPaths::ResolveAssetPath(path);
    if (resolvedPath.empty()) {
        Logger::Warning("[HudSpriteManager] Failed to resolve fist texture path: %s", path.c_str());
        RestoreCustomFistTexture();
        DestroyCustomFistTexture();
        return false;
    }

    const DWORD attrs = GetFileAttributesA(resolvedPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        Logger::Warning("[HudSpriteManager] Fist texture file not found: %s", path.c_str());
        RestoreCustomFistTexture();
        DestroyCustomFistTexture();
        return false;
    }

    if (g_customFistTexture == nullptr || g_customFistPath != path) {
        DestroyCustomFistTexture();
        g_customFistTexture = CreateRwTextureFromPngFile(resolvedPath, "custom fist", 512);
        if (g_customFistTexture == nullptr) {
            RestoreCustomFistTexture();
            return false;
        }
        g_customFistPath = path;
    }

    *slot = g_customFistTexture;
    fistCustomApplied_ = true;
    Logger::Debug("[HudSpriteManager] Applied custom fist texture: %s", path.c_str());
    return true;
}

void HudSpriteManager::RestoreCustomFistTexture() {
    if (!g_originalFistCaptured) {
        fistCustomApplied_ = false;
        return;
    }

    void** slot = ResolveFistTextureSlot();
    if (slot != nullptr) {
        *slot = g_originalFistTexture;
    }

    fistCustomApplied_ = false;
}

void HudSpriteManager::DestroyCustomFistTexture() {
    if (g_customFistTexture != nullptr) {
        RwTextureDestroy(static_cast<RwTexture*>(g_customFistTexture));
        g_customFistTexture = nullptr;
    }
    g_customFistPath.clear();
}
#endif

} // namespace keybinder
