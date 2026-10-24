#include "../include/SquareMap.h"

#include "../include/Logger.h"
#include "../include/ResourceIds.h"
#include "../include/EmbeddedRwTexture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <string>

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwplcore.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwcore.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CHud.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CMenuManager.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CVector2D.h"
#include "../include/plugin-sdk/shared/Patch.h"
#endif

namespace keybinder {

#ifndef KEYBINDER_TESTING
namespace {


constexpr uintptr_t kLimitRadarPointAddress = 0x5832F0;
constexpr uintptr_t kDrawRadarMaskAddress = 0x585700;
constexpr std::size_t kPatchSize = 5;

std::string FormatPatchBytes(const unsigned char* bytes) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%02X %02X %02X %02X %02X",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
    return buffer;
}

bool MatchesPatchSignature(const unsigned char* bytes, const unsigned char (&expected)[kPatchSize]) {
    return std::memcmp(bytes, expected, kPatchSize) == 0;
}

bool IsRelativeJumpPatch(const unsigned char* bytes) {
    return bytes[0] == 0xE9;
}

bool MatchesLimitRadarPointSignature(const unsigned char* bytes) {
    // Some SA-MP/mod stacks pre-hook this helper with a near JMP before our ASI
    // runs. Replacing and later restoring that JMP is reversible and avoids
    // corrupting the hook target/code cave.
    if (IsRelativeJumpPatch(bytes)) {
        return true;
    }

    static constexpr unsigned char kOptimizedPrologue[kPatchSize] = {0x56, 0x8B, 0x74, 0x24, 0x08};
    static constexpr unsigned char kFramePointerPrologueA[kPatchSize] = {0x55, 0x8B, 0xEC, 0x56, 0x8B};
    static constexpr unsigned char kFramePointerPrologueB[kPatchSize] = {0x55, 0x8B, 0xEC, 0x83, 0xEC};
    return MatchesPatchSignature(bytes, kOptimizedPrologue)
        || MatchesPatchSignature(bytes, kFramePointerPrologueA)
        || MatchesPatchSignature(bytes, kFramePointerPrologueB);
}

bool MatchesDrawRadarMaskSignature(const unsigned char* bytes) {
    // The mask may also be pre-hooked by HUD/radar compatibility mods; treat a
    // near JMP at the expected function entry as patchable because restoration
    // writes the original JMP bytes back verbatim.
    if (IsRelativeJumpPatch(bytes)) {
        return true;
    }

    static constexpr unsigned char kStackPrologueA[kPatchSize] = {0x83, 0xEC, 0x60, 0x56, 0x57};
    static constexpr unsigned char kStackPrologueB[kPatchSize] = {0x81, 0xEC, 0x80, 0x00, 0x00};
    static constexpr unsigned char kFramePointerPrologueA[kPatchSize] = {0x55, 0x8B, 0xEC, 0x83, 0xEC};
    static constexpr unsigned char kFramePointerPrologueB[kPatchSize] = {0x55, 0x8B, 0xEC, 0x83, 0xE4};
    return MatchesPatchSignature(bytes, kStackPrologueA)
        || MatchesPatchSignature(bytes, kStackPrologueB)
        || MatchesPatchSignature(bytes, kFramePointerPrologueA)
        || MatchesPatchSignature(bytes, kFramePointerPrologueB);
}


float __cdecl SquareLimitRadarPoint(CVector2D& point) {
    const float distance = std::sqrt(point.x * point.x + point.y * point.y);

    if (FrontEndMenuManager.m_bMenuActive) {
        return distance;
    }

    const float maxAbs = (std::max)(std::fabs(point.x), std::fabs(point.y));
    if (maxAbs > 1.0f) {
        point.x /= maxAbs;
        point.y /= maxAbs;
    }

    return distance;
}

void __cdecl SquareDrawRadarMask() {
    // No-op: the native function draws the circular mask outside the radar.
}

} // namespace
#endif

void SquareMap::Initialize() {
#ifndef KEYBINDER_TESTING
    CaptureOriginalTexture();
#endif
    initialized_ = true;

    if (enabled_) {
        Settings settings;
        settings.squaremap_enabled = true;
        ApplySettings(settings);
    }
}

void SquareMap::Shutdown() {
    RestoreDefault();
#ifndef KEYBINDER_TESTING
    DestroyCustomTexture();
#endif
    initialized_ = false;
    enabled_ = false;
}

void SquareMap::OnDeviceLost() {
    RestoreDefault();
#ifndef KEYBINDER_TESTING
    DestroyCustomTexture();
#endif
}

void SquareMap::OnDeviceReset() {
    if (enabled_) {
        Settings settings;
        settings.squaremap_enabled = true;
        ApplySettings(settings);
    }
}

bool SquareMap::ApplySettings(const Settings& settings) {
    enabled_ = settings.squaremap_enabled;

    if (!enabled_) {
        RestoreDefault();
#ifndef KEYBINDER_TESTING
        DestroyCustomTexture();
#endif
        return true;
    }

    if (!initialized_) {
        return true;
    }

#ifdef KEYBINDER_TESTING
    applied_ = true;
    return true;
#else
    if (!LoadAndApplyTexture()) {
        RestoreDefault();
        DestroyCustomTexture();
        enabled_ = false;
        return false;
    }

    applied_ = true;
    return true;
#endif
}

void SquareMap::RestoreDefault() {
#ifndef KEYBINDER_TESTING
    if (originalCaptured_ && CHud::Sprites != nullptr) {
        CHud::Sprites[HUDSPRITE_RADARDISC].m_pTexture = static_cast<RwTexture*>(originalRadarDiscTexture_);
    }
    RestorePatches();
#endif
    applied_ = false;
}

#ifndef KEYBINDER_TESTING
bool SquareMap::CaptureOriginalTexture() {
    if (originalCaptured_) {
        return true;
    }

    if (CHud::Sprites == nullptr) {
        Logger::Warning("[SquareMap] Could not capture original radar disc texture yet: CHud::Sprites is null");
        return false;
    }

    originalRadarDiscTexture_ = CHud::Sprites[HUDSPRITE_RADARDISC].m_pTexture;
    originalCaptured_ = true;
    Logger::Debug("[SquareMap] Captured original radar disc texture");
    return true;
}

bool SquareMap::LoadAndApplyTexture() {
    if (!CaptureOriginalTexture()) {
        return false;
    }

    if (!ApplyPatches()) {
        Logger::Warning("[SquareMap] Native radar patch verification failed; Squaremap remains disabled");
        return false;
    }

    if (customTexture_ == nullptr) {
        customTexture_ = CreateTextureFromEmbeddedPng();
        if (customTexture_ == nullptr) {
            Logger::Warning("[SquareMap] Failed to create radardisc texture from embedded resource");
            return false;
        }
    }

    if (CHud::Sprites != nullptr) {
        CHud::Sprites[HUDSPRITE_RADARDISC].m_pTexture = static_cast<RwTexture*>(customTexture_);
    }

    Logger::Debug("[SquareMap] Applied square radar disc and native radar patches");
    return true;
}

void* SquareMap::CreateTextureFromEmbeddedPng() {
    return CreateRwTextureFromEmbeddedPng(IDR_RADARDISC, "radardisc");
}

void SquareMap::DestroyCustomTexture() {
    if (customTexture_ != nullptr) {
        RwTextureDestroy(static_cast<RwTexture*>(customTexture_));
        customTexture_ = nullptr;
    }
}

bool SquareMap::ApplyPatches() {
    if (patchesApplied_) {
        return true;
    }

    unsigned char limitBytes[kPatchSize] = {};
    unsigned char maskBytes[kPatchSize] = {};
    plugin::patch::GetRaw(kLimitRadarPointAddress, limitBytes, kPatchSize);
    plugin::patch::GetRaw(kDrawRadarMaskAddress, maskBytes, kPatchSize);

    if (!MatchesLimitRadarPointSignature(limitBytes)) {
        Logger::Warning("[SquareMap] CRadar::LimitRadarPoint signature mismatch at 0x%X: %s",
            static_cast<unsigned int>(kLimitRadarPointAddress),
            FormatPatchBytes(limitBytes).c_str());
        return false;
    }

    if (!MatchesDrawRadarMaskSignature(maskBytes)) {
        Logger::Warning("[SquareMap] CRadar::DrawRadarMask signature mismatch at 0x%X: %s",
            static_cast<unsigned int>(kDrawRadarMaskAddress),
            FormatPatchBytes(maskBytes).c_str());
        return false;
    }

    if (!patchesCaptured_) {
        std::memcpy(originalLimitRadarPointBytes_, limitBytes, kPatchSize);
        std::memcpy(originalDrawRadarMaskBytes_, maskBytes, kPatchSize);
        patchesCaptured_ = true;
    }

    plugin::patch::ReplaceFunction(kLimitRadarPointAddress, reinterpret_cast<void*>(&SquareLimitRadarPoint));
    plugin::patch::ReplaceFunction(kDrawRadarMaskAddress, reinterpret_cast<void*>(&SquareDrawRadarMask));
    patchesApplied_ = true;
    return true;
}

void SquareMap::RestorePatches() {
    if (!patchesApplied_ || !patchesCaptured_) {
        return;
    }

    plugin::patch::SetRaw(kLimitRadarPointAddress, originalLimitRadarPointBytes_, kPatchSize);
    plugin::patch::SetRaw(kDrawRadarMaskAddress, originalDrawRadarMaskBytes_, kPatchSize);
    patchesApplied_ = false;
}
#endif

} // namespace keybinder
