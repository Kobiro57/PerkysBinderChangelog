#include "../include/CrosshairSwitcher.h"

#include "../include/Logger.h"
#include "../include/PersistentPaths.h"

#include <algorithm>
#include <cstring>

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwplcore.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwcore.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CHud.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CWeaponEffects.h"
#include "../include/plugin-sdk/stb/stb_image.h"
#include <windows.h>
#endif

namespace keybinder {

std::string CrosshairSwitcher::NormalizePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');

    const std::string prefix = "crosshairs";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        const char separator = path[prefix.size()];
        if (separator != '\\') {
            path.insert(prefix.size(), "\\");
        }
    }

    return path;
}

void CrosshairSwitcher::Initialize() {
#ifndef KEYBINDER_TESTING
    CaptureOriginalTexture();
#endif
    initialized_ = true;

    if (enabled_) {
        Settings settings;
        settings.crosshair_enabled = enabled_;
        settings.crosshair_texture_path = configuredPath_;
        ApplySettings(settings);
    }
}

void CrosshairSwitcher::Shutdown() {
    RestoreDefault();
#ifndef KEYBINDER_TESTING
    DestroyCustomTexture();
    DestroySuppressionTexture();
#endif
    initialized_ = false;
}

void CrosshairSwitcher::OnDeviceLost() {
    RestoreDefault();
#ifndef KEYBINDER_TESTING
    DestroyCustomTexture();
    DestroySuppressionTexture();
#endif
}

void CrosshairSwitcher::OnDeviceReset() {
    if (enabled_) {
        Settings settings;
        settings.crosshair_enabled = enabled_;
        settings.crosshair_texture_path = configuredPath_;
        ApplySettings(settings);
    }
}

bool CrosshairSwitcher::ApplySettings(const Settings& settings) {
    enabled_ = settings.crosshair_enabled;
    configuredPath_ = NormalizePath(settings.crosshair_texture_path);

    if (!enabled_ || configuredPath_.empty()) {
        RestoreDefault();
#ifndef KEYBINDER_TESTING
        DestroyCustomTexture();
        DestroySuppressionTexture();
#endif
        return true;
    }

    if (!initialized_) {
        return true;
    }

#ifdef KEYBINDER_TESTING
    customApplied_ = true;
    return true;
#else
    return LoadAndApplyTexture(configuredPath_);
#endif
}

void CrosshairSwitcher::RestoreDefault() {
#ifndef KEYBINDER_TESTING
    if (originalCaptured_ && originalTexture_ != nullptr) {
        gpCrossHairTex = static_cast<RwTexture*>(originalTexture_);
    }

    if (originalCaptured_ && CHud::Sprites != nullptr) {
        if (originalHudSiteM16Texture_ != nullptr) {
            CHud::Sprites[HUDSPRITE_SITEM16].m_pTexture = static_cast<RwTexture*>(originalHudSiteM16Texture_);
        }
        if (originalHudSiteRocketTexture_ != nullptr) {
            CHud::Sprites[HUDSPRITE_SITEROCKET].m_pTexture = static_cast<RwTexture*>(originalHudSiteRocketTexture_);
        }
    }
#endif
    customApplied_ = false;
    suppressionApplied_ = false;
}

#ifndef KEYBINDER_TESTING
bool CrosshairSwitcher::CaptureOriginalTexture() {
    if (originalCaptured_) {
        return true;
    }

    bool capturedAnything = false;

    if (gpCrossHairTex != nullptr) {
        originalTexture_ = gpCrossHairTex;
        capturedAnything = true;
    }

    if (CHud::Sprites != nullptr) {
        originalHudSiteM16Texture_ = CHud::Sprites[HUDSPRITE_SITEM16].m_pTexture;
        originalHudSiteRocketTexture_ = CHud::Sprites[HUDSPRITE_SITEROCKET].m_pTexture;
        if (originalHudSiteM16Texture_ != nullptr || originalHudSiteRocketTexture_ != nullptr) {
            capturedAnything = true;
        }
    }

    if (!capturedAnything) {
        Logger::Warning("[CrosshairSwitcher] Could not capture original crosshair textures yet: native HUD sprites and gpCrossHairTex are null");
        return false;
    }

    originalCaptured_ = true;
    Logger::Debug("[CrosshairSwitcher] Captured original crosshair textures");
    return true;
}

bool CrosshairSwitcher::ApplySuppressionTexture() {
    if (!CaptureOriginalTexture()) {
        return false;
    }

    if (suppressionTexture_ == nullptr) {
        suppressionTexture_ = CreateTransparentTexture();
        if (suppressionTexture_ == nullptr) {
            Logger::Warning("[CrosshairSwitcher] Failed to create suppression texture");
            return false;
        }
    }

    if (originalTexture_ != nullptr) {
        gpCrossHairTex = static_cast<RwTexture*>(suppressionTexture_);
    }

    if (CHud::Sprites != nullptr) {
        CHud::Sprites[HUDSPRITE_SITEM16].m_pTexture = static_cast<RwTexture*>(suppressionTexture_);
        CHud::Sprites[HUDSPRITE_SITEROCKET].m_pTexture = static_cast<RwTexture*>(suppressionTexture_);
    }

    CWeaponEffects::ClearCrossHairs();

    suppressionApplied_ = true;
    Logger::Debug("[CrosshairSwitcher] Applied native crosshair suppression");
    return true;
}

void* CrosshairSwitcher::CreateTransparentTexture(int width, int height) {
    RwRaster* raster = RwRasterCreate(width, height, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    if (raster == nullptr) {
        return nullptr;
    }

    auto* pixels = reinterpret_cast<RwUInt32*>(RwRasterLock(raster, 0, rwRASTERLOCKWRITE));
    if (pixels == nullptr) {
        RwRasterDestroy(raster);
        return nullptr;
    }

    std::memset(pixels, 0, static_cast<std::size_t>(width * height * 4));
    RwRasterUnlock(raster);

    RwTexture* texture = RwTextureCreate(raster);
    if (texture == nullptr) {
        RwRasterDestroy(raster);
        return nullptr;
    }

    RwTextureSetFilterMode(texture, rwFILTERLINEAR);
    return texture;
}

void CrosshairSwitcher::DestroySuppressionTexture() {
    if (suppressionTexture_ != nullptr) {
        RwTextureDestroy(static_cast<RwTexture*>(suppressionTexture_));
        suppressionTexture_ = nullptr;
    }
}

bool CrosshairSwitcher::LoadAndApplyTexture(const std::string& path) {
    if (!CaptureOriginalTexture()) {
        return false;
    }

    const std::string resolvedPath = PersistentPaths::ResolveAssetPath(path);
    if (resolvedPath.empty()) {
        Logger::Warning("[CrosshairSwitcher] Failed to resolve crosshair path: %s", path.c_str());
        DestroyCustomTexture();
        RestoreDefault();
        return false;
    }

    const DWORD attrs = GetFileAttributesA(resolvedPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        Logger::Warning("[CrosshairSwitcher] Crosshair file not found: %s", path.c_str());
        DestroyCustomTexture();
        RestoreDefault();
        return false;
    }

    if (customTexture_ != nullptr) {
        DestroyCustomTexture();
    }

    customTexture_ = CreateTextureFromPng(resolvedPath);
    if (customTexture_ == nullptr) {
        Logger::Warning("[CrosshairSwitcher] Failed to create native crosshair texture from %s", path.c_str());
        RestoreDefault();
        return false;
    }

    if (originalTexture_ != nullptr) {
        gpCrossHairTex = static_cast<RwTexture*>(customTexture_);
    }

    if (CHud::Sprites != nullptr) {
        CHud::Sprites[HUDSPRITE_SITEM16].m_pTexture = static_cast<RwTexture*>(customTexture_);
        CHud::Sprites[HUDSPRITE_SITEROCKET].m_pTexture = static_cast<RwTexture*>(customTexture_);
    }

    suppressionApplied_ = false;
    customApplied_ = true;
    Logger::Debug("[CrosshairSwitcher] Applied custom crosshair to HUD sprites: %s", path.c_str());
    return true;
}

void* CrosshairSwitcher::CreateTextureFromPng(const std::string& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (data == nullptr) {
        return nullptr;
    }

    constexpr int kMaxTextureSize = 256;
    if (width <= 0 || height <= 0 || width > kMaxTextureSize || height > kMaxTextureSize) {
        stbi_image_free(data);
        Logger::Warning("[CrosshairSwitcher] Invalid crosshair size %dx%d for %s", width, height, path.c_str());
        return nullptr;
    }

    RwRaster* raster = RwRasterCreate(width, height, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    if (raster == nullptr) {
        stbi_image_free(data);
        return nullptr;
    }

    auto* pixels = reinterpret_cast<RwUInt32*>(RwRasterLock(raster, 0, rwRASTERLOCKWRITE));
    if (pixels == nullptr) {
        RwRasterDestroy(raster);
        stbi_image_free(data);
        return nullptr;
    }

    for (int i = 0; i < width * height * 4; i += 4) {
        const unsigned char r = data[i + 0];
        const unsigned char g = data[i + 1];
        const unsigned char b = data[i + 2];
        const unsigned char a = data[i + 3];
        data[i + 0] = b;
        data[i + 1] = g;
        data[i + 2] = r;
        data[i + 3] = a;
    }

    std::memcpy(pixels, data, static_cast<std::size_t>(width * height * 4));
    RwRasterUnlock(raster);
    stbi_image_free(data);

    RwTexture* texture = RwTextureCreate(raster);
    if (texture == nullptr) {
        RwRasterDestroy(raster);
        return nullptr;
    }

    RwTextureSetFilterMode(texture, rwFILTERLINEAR);
    return texture;
}

void CrosshairSwitcher::DestroyCustomTexture() {
    if (customTexture_ != nullptr) {
        RwTextureDestroy(static_cast<RwTexture*>(customTexture_));
        customTexture_ = nullptr;
    }
}
#endif

} // namespace keybinder
