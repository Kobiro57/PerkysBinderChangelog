#include "../include/WeaponIconManager.h"
#include "../include/EmbeddedRwTexture.h"
#include "../include/Logger.h"
#include "../include/PersistentPaths.h"

#include <algorithm>
#include <utility>
#include <windows.h>
#ifndef KEYBINDER_TESTING
#include <atomic>
extern std::atomic<int> g_shutdown_phase;
#endif

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/CPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CPlayerPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/common.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/rw/rwcore.h"
#include "../include/plugin-sdk/safetyhook/safetyhook.hpp"
#endif

namespace keybinder {
namespace {
bool IsOrdinaryAsset(const std::string& path, bool directory) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        && static_cast<bool>(attributes & FILE_ATTRIBUTE_DIRECTORY) == directory;
}

#ifndef KEYBINDER_TESTING
WeaponIconManager* activeManager = nullptr;
safetyhook::InlineHook* drawWeaponHook = nullptr;
safetyhook::InlineHook* findTextureHook = nullptr;
thread_local RwTexture* scopedWeaponTexture = nullptr;

RwTexture* __cdecl FindWeaponHudTexture(RwTexDictionary* dictionary, unsigned int hash) {
    if (scopedWeaponTexture) return scopedWeaponTexture;
    return findTextureHook ? findTextureHook->ccall<RwTexture*>(dictionary, hash) : nullptr;
}

void __cdecl DrawWeaponHudIcon(CPed* ped, int x, int y, float alpha) {
    // The original DrawWeaponIcon keeps its positioning, aspect ratio, alpha
    // handling and modded call arguments. Only its texture lookup is scoped.
    // Native path: plugin-sdk CHud.cpp (0x58D7D0), common.cpp (0x734E50);
    // verified against gta-reversed source/game_sa/Hud.cpp DrawWeaponIcon.
    const auto previous = scopedWeaponTexture;
    scopedWeaponTexture = nullptr;
    if (activeManager && ped && ped == FindPlayerPed(0) && ped->m_nSelectedWepSlot < 13) {
        const int weaponId = static_cast<int>(ped->m_aWeapons[ped->m_nSelectedWepSlot].m_eWeaponType);
        scopedWeaponTexture = static_cast<RwTexture*>(activeManager->GetTextureForWeapon(weaponId));
    }
    if (drawWeaponHook) drawWeaponHook->ccall<void>(ped, x, y, alpha);
    scopedWeaponTexture = previous;
}
#endif
} // namespace

WeaponIconManager::~WeaponIconManager() {
#ifndef KEYBINDER_TESTING
    // Main explicitly releases hooks/textures while RenderWare is alive.
    // During an emergency process exit native render state may be gone.
    if (initialized_ && g_shutdown_phase.load() != 0) return;
#endif
    Shutdown();
}

void WeaponIconManager::Initialize() {
    if (initialized_) return;
    initialized_ = true;
    deviceReady_ = true;
    RefreshLibrary();
}

void WeaponIconManager::Shutdown() {
    RemoveNativeHooks();
    ReleaseTextures();
    initialized_ = deviceReady_ = enabled_ = false;
    selections_.clear();
    library_.clear();
    lastError_.clear();
}

void WeaponIconManager::OnDeviceLost() {
    deviceReady_ = false;
    ReleaseTextures();
}

void WeaponIconManager::OnDeviceReset() {
    if (!initialized_) return;
    deviceReady_ = true;
    RebuildTextures(true);
}

bool WeaponIconManager::ApplySettings(const Settings& settings) {
    lastError_.clear();
    enabled_ = settings.weapon_icons_enabled;
    selections_ = settings.weapon_icon_selections;
    return RebuildTextures(false);
}

bool WeaponIconManager::RefreshLibrary() {
    lastError_.clear();
    const std::string root = PersistentPaths::EnsureLayout();
    if (root.empty()) {
        lastError_ = "Der NLBinder-Ordner konnte nicht angelegt werden.";
        return false;
    }
    const auto weaponsRoot = PersistentPaths::WeaponIconsDir();
    if (!IsOrdinaryAsset(weaponsRoot, true)) {
        lastError_ = "Der Waffenicons-Ordner ist nicht verfügbar.";
        return false;
    }
    library_.clear();
    bool complete = true;
    for (const auto& definition : kWeaponIconCatalog) {
        const auto folder = weaponsRoot + "\\" + definition.folder;
        if (!IsOrdinaryAsset(folder, true)) {
            if (!CreateDirectoryA(folder.c_str(), nullptr) || !IsOrdinaryAsset(folder, true)) {
                complete = false;
                continue;
            }
        }
        WIN32_FIND_DATAA data{};
        HANDLE search = FindFirstFileA((folder + "\\*").c_str(), &data);
        if (search == INVALID_HANDLE_VALUE) continue;
        auto& entries = library_[definition.weaponId];
        do {
            if ((data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) continue;
            const std::string candidate = NormalizeWeaponIconPath(definition.weaponId,
                std::string("Waffenicons\\") + definition.folder + "\\" + data.cFileName);
            if (!candidate.empty()) entries.push_back(candidate);
        } while (FindNextFileA(search, &data));
        FindClose(search);
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            const auto a = LowerWeaponIconPath(left), b = LowerWeaponIconPath(right);
            return a == b ? left < right : a < b;
        });
    }
    if (!complete) lastError_ = "Einige Waffenordner konnten nicht angelegt oder gelesen werden.";
    const bool loaded = RebuildTextures(true);
    return complete && loaded;
}

std::vector<std::string> WeaponIconManager::GetAvailableIcons(int weaponId) const {
    const auto found = library_.find(weaponId);
    return found == library_.end() ? std::vector<std::string>{} : found->second;
}

std::string WeaponIconManager::ResolveConfiguredPath(int weaponId) const {
    const auto files = library_.find(weaponId);
    if (files == library_.end() || files->second.empty()) return {};
    std::string desired;
    for (const auto& selected : selections_) {
        if (selected.weaponId != weaponId) continue;
        if (!selected.enabled) return {};
        if (!selected.path.empty()) {
            desired = NormalizeWeaponIconPath(weaponId, selected.path);
            if (desired.empty()) return {};
        }
        break;
    }
    if (desired.empty()) return files->second.front();
    for (const auto& path : files->second)
        if (LowerWeaponIconPath(path) == LowerWeaponIconPath(desired)) return path;
    return {}; // A missing explicit selection must not silently choose another icon.
}

std::string WeaponIconManager::GetConfiguredIcon(int weaponId) const { return ResolveConfiguredPath(weaponId); }

void* WeaponIconManager::GetTextureForWeapon(int weaponId) const {
    if (!enabled_ || !initialized_ || !deviceReady_) return nullptr;
    const auto found = textures_.find(weaponId);
    return found == textures_.end() ? nullptr : found->second.texture;
}

bool WeaponIconManager::IsAppliedForWeapon(int weaponId) const { return GetTextureForWeapon(weaponId) != nullptr; }

void WeaponIconManager::ReleaseTextures() {
    for (auto& entry : textures_) {
        if (!entry.second.texture) continue;
#ifdef KEYBINDER_TESTING
        if (testingHooks_.destroyTexture) testingHooks_.destroyTexture(entry.second.texture);
#else
        RwTextureDestroy(static_cast<RwTexture*>(entry.second.texture));
#endif
    }
    textures_.clear();
}

bool WeaponIconManager::RebuildTextures(bool forceReload) {
    if (!initialized_ || !deviceReady_) return true;
    if (!enabled_) {
        RemoveNativeHooks();
        ReleaseTextures();
        return true;
    }
    // A refresh reloads every asset. Release this manager's old textures first
    // so a full pack never doubles its GPU/32-bit process memory temporarily.
    if (forceReload) ReleaseTextures();
    bool loaded = true;
    std::unordered_map<int, LoadedIcon> next;
    for (const auto& definition : kWeaponIconCatalog) {
        const auto path = ResolveConfiguredPath(definition.weaponId);
        if (path.empty()) continue;
        auto old = textures_.find(definition.weaponId);
        if (!forceReload && old != textures_.end() && old->second.path == path) {
            next.emplace(definition.weaponId, std::move(old->second));
            textures_.erase(old);
            continue;
        }
        const auto resolved = PersistentPaths::ResolveAssetPath(path);
        if (resolved.empty() || !IsOrdinaryAsset(resolved, false)) continue;
        void* texture = nullptr;
#ifdef KEYBINDER_TESTING
        if (testingHooks_.loadTexture) texture = testingHooks_.loadTexture(resolved);
#else
        texture = CreateRwTextureFromPngFile(resolved, definition.name, 512);
#endif
        if (texture) next.emplace(definition.weaponId, LoadedIcon{path, texture});
        else {
            loaded = false;
            lastError_ = std::string("PNG konnte nicht geladen werden: ") + path;
        }
    }
    ReleaseTextures();
    textures_ = std::move(next);
    if (!textures_.empty() && !InstallNativeHooks()) {
        lastError_ = "Die native Waffenicon-Anzeige konnte nicht eingebunden werden.";
        ReleaseTextures();
        return false;
    }
    if (textures_.empty()) RemoveNativeHooks();
    return loaded;
}

bool WeaponIconManager::InstallNativeHooks() {
#ifdef KEYBINDER_TESTING
    return true;
#else
    if (drawWeaponHook && findTextureHook && activeManager == this) return true;
    if (activeManager && activeManager != this) return false;
    auto texture = safetyhook::InlineHook::create(reinterpret_cast<void*>(0x734E50), reinterpret_cast<void*>(&FindWeaponHudTexture));
    if (!texture) return false;
    findTextureHook = new safetyhook::InlineHook(std::move(*texture));
    auto draw = safetyhook::InlineHook::create(reinterpret_cast<void*>(0x58D7D0), reinterpret_cast<void*>(&DrawWeaponHudIcon));
    if (!draw) {
        delete findTextureHook;
        findTextureHook = nullptr;
        return false;
    }
    drawWeaponHook = new safetyhook::InlineHook(std::move(*draw));
    activeManager = this;
    return true;
#endif
}

void WeaponIconManager::RemoveNativeHooks() {
#ifndef KEYBINDER_TESTING
    if (activeManager != this) return;
    activeManager = nullptr;
    delete drawWeaponHook;
    drawWeaponHook = nullptr;
    delete findTextureHook;
    findTextureHook = nullptr;
    scopedWeaponTexture = nullptr;
#endif
}

} // namespace keybinder
