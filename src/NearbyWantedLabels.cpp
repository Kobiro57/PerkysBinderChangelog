#include "../include/NearbyWantedLabels.h"
#include "../include/OnlineDataManager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <utility>

#ifndef KEYBINDER_TESTING
#include <d3d9.h>
#include "../include/plugin-sdk/shared/plugin.h"
#include "../include/FactionAccess.h"
#include "../include/SAMPBridge.h"
#include "../include/Logger.h"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CPlayerTags.h"
#include "CMenuManager.h"
#include "CTimer.h"
#include "CSprite.h"
#include "safetyhook.hpp"
extern std::atomic<int> g_shutdown_phase;
#endif

namespace keybinder {
namespace {
std::string NormalizeWantedName(const std::string& name) {
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = name.find_last_not_of(" \t\r\n");
    std::string result = name.substr(first, last - first + 1);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string VisibleNativeText(const std::string& text) {
    if (text.size() > 192) return {};
    std::string plain;
    for (std::size_t at = 0; at < text.size();) {
        if (text[at] == '{') {
            const auto end = text.find('}', at + 1);
            if (end != std::string::npos && (end - at == 7 || end - at == 9)
                && std::all_of(text.begin() + at + 1, text.begin() + end,
                    [](unsigned char c) { return std::isxdigit(c) != 0; })) { at = end + 1; continue; }
        }
        const unsigned char ch = static_cast<unsigned char>(text[at++]);
        if (ch == '\0' || ch == '\n' || ch == '\r') return {};
        plain.push_back(static_cast<char>(ch));
    }
    return NormalizeWantedName(plain);
}

#ifndef KEYBINDER_TESTING
namespace samp = sampapi::v03dl;
safetyhook::InlineHook* nameTagHook = nullptr;
SAMPBridge* currentBridge = nullptr;
const OnlineDataManager* currentOnline = nullptr;
std::function<bool()> enabledProvider;
bool fallbackEnabled = false;
bool hookFailureLogged = false;
unsigned int cachedFrame = 0;
bool cacheValid = false;
bool cachedPolice = false;
OnlineSnapshot cachedSnapshot;
thread_local bool drawingWanted = false;

bool EnabledNow() {
    if (g_shutdown_phase.load() != 0) return false;
    try { return enabledProvider ? enabledProvider() : fallbackEnabled; }
    catch (...) { return false; }
}

// Exact SA-MP 0.3.DL-1 ABI and address from the bundled CPlayerTags.cpp.
// The native name pass has already applied the streaming, distance and
// visibility rules, including occupants of vehicles.
void __fastcall HookedDrawLabel(samp::CPlayerTags* self, void*, sampapi::CVector* position,
    const char* text, D3DCOLOR color, float distance, bool statusVisible, int status) {
    auto* hook = nameTagHook;
    if (!hook) return;
    const sampapi::CVector originalPosition = position ? *position : sampapi::CVector();
    hook->thiscall<void>(self, position, text, color, distance, statusVisible, status);
    if (drawingWanted || !currentBridge || !currentOnline || !EnabledNow()
        || !position || !text || !std::isfinite(distance) || distance < 0.0f
        || !std::isfinite(originalPosition.x) || !std::isfinite(originalPosition.y)
        || !std::isfinite(originalPosition.z) || FrontEndMenuManager.m_bMenuActive
        || CTimer::m_UserPause || CTimer::m_CodePause) return;
    try {
        auto* net = currentBridge->GetNetGame();
        if (!net || net->m_nGameState != samp::CNetGame::GAME_MODE_CONNECTED
            || !net->m_pSettings || !net->m_pSettings->m_bNameTags) return;
        auto* pool = net->GetPlayerPool();
        if (!pool || pool->m_nLocalPlayerId >= samp::CPlayerPool::MAX_PLAYERS
            || !pool->pLocalPlayer || !pool->pLocalPlayer->m_bIsActive
            || pool->pLocalPlayer->m_bIsWasted) return;
        if (!cacheValid || cachedFrame != CTimer::m_FrameCounter) {
            cachedFrame = CTimer::m_FrameCounter;
            cachedPolice = IsPoliceMember(currentBridge->GetLocalFaction());
            cachedSnapshot = cachedPolice ? currentOnline->GetSnapshotCopy() : OnlineSnapshot{};
            cacheValid = true;
        }
        if (!cachedPolice || !cachedSnapshot.IsLive()) return;
        // A draw invocation can still be clipped inside the native function.
        // Require the original name anchor onscreen before shifting our row.
        RwV3d anchor{originalPosition.x, originalPosition.y, originalPosition.z};
        RwV3d screen{};
        float screenWidth = 0.0f, screenHeight = 0.0f;
        D3DVIEWPORT9 viewport{};
        if (!self || !self->m_pDevice || FAILED(self->m_pDevice->GetViewport(&viewport))
            || !viewport.Width || !viewport.Height
            || !CSprite::CalcScreenCoors(anchor, &screen, &screenWidth, &screenHeight, true, true)
            || !std::isfinite(screen.x) || !std::isfinite(screen.y) || !std::isfinite(screen.z)
            || screen.z <= 0.0f || screen.x < viewport.X || screen.y < viewport.Y
            || screen.x >= static_cast<double>(viewport.X) + viewport.Width
            || screen.y >= static_cast<double>(viewport.Y) + viewport.Height) return;
        const std::size_t length = strnlen(text, 193);
        if (length > 192) return;
        const std::string nativeText(text, length);
        int matchedId = -1;
        std::string label;
        for (int id = 0; id < samp::CPlayerPool::MAX_PLAYERS; ++id) {
            if (id == pool->m_nLocalPlayerId || !pool->m_bNotEmpty[id]) continue;
            const auto* info = pool->m_pObject[id];
            if (!info || info->m_szNick.empty() || !MatchesNativePlayerNameTag(nativeText, info->m_szNick, id)) continue;
            if (matchedId >= 0) return; // Ambiguous modified tags do not annotate another player.
            matchedId = id;
            const auto* remote = info->m_pPlayer;
            if (!remote || remote->m_nId != id || !remote->m_bDrawLabels || !remote->m_pPed) continue;
            const bool aliveState = remote->m_nState == samp::CRemotePlayer::PLAYER_STATE_ONFOOT
                || remote->m_nState == samp::CRemotePlayer::PLAYER_STATE_DRIVER
                || remote->m_nState == samp::CRemotePlayer::PLAYER_STATE_PASSENGER;
            NearbyWantedLabelState state;
            state.overlayEnabled = state.policeMember = state.connected = true;
            state.localActive = state.localAlive = true;
            state.streamed = remote->m_pPed->m_pGamePed != nullptr;
            state.targetAlive = aliveState;
            state.serverAllowsLabels = state.remoteAllowsLabels = state.nativeNameTagVisible = true;
            state.localPlayerId = pool->m_nLocalPlayerId;
            state.playerId = id;
            label = BuildWantedLabelForNativeDraw(state, cachedSnapshot, nativeText, info->m_szNick);
        }
        if (label.empty()) return;
        sampapi::CVector wantedPosition = originalPosition;
        wantedPosition.z += 0.4f;
        struct DrawingGuard { DrawingGuard() { drawingWanted = true; } ~DrawingGuard() { drawingWanted = false; } } guard;
        // Keep the original alpha and native distance/fade behavior. The
        // trampoline prevents observing our additional row recursively.
        const D3DCOLOR red = (color & 0xFF000000u) | 0x00FF3030u;
        hook->thiscall<void>(self, &wantedPosition, label.c_str(), red, distance, false, 0);
    } catch (...) {
        // The optional annotation must not interrupt the native name pass.
    }
}
#endif
} // namespace

bool ShouldShowNearbyWantedLabel(const NearbyWantedLabelState& s) {
    return s.overlayEnabled && s.policeMember && s.connected && s.localActive && s.localAlive
        && s.streamed && s.targetAlive && s.serverAllowsLabels && s.remoteAllowsLabels
        && s.nativeNameTagVisible && s.wanted > 0 && s.localPlayerId >= 0 && s.localPlayerId < 1004
        && s.playerId >= 0 && s.playerId < 1004 && s.localPlayerId != s.playerId;
}

std::optional<int> FindLiveWantedForName(const OnlineSnapshot& snapshot, const std::string& name) {
    if (!snapshot.IsLive()) return std::nullopt;
    const auto normalized = NormalizeWantedName(name);
    if (normalized.empty()) return std::nullopt;
    std::optional<int> found;
    bool matched = false;
    for (const auto& player : snapshot.players) {
        if (NormalizeWantedName(player.name) != normalized) continue;
        if (matched) return std::nullopt;
        matched = true;
        if (player.wanted > 0) found = player.wanted;
    }
    return found;
}

std::string BuildNearbyWantedLabel(int wanted) {
    return wanted > 0 ? "[GESUCHT | " + std::to_string(wanted) + " Wanteds]" : std::string{};
}

bool MatchesNativePlayerNameTag(const std::string& text, const std::string& name, int id) {
    if (id < 0 || id >= 1004 || name.empty() || name.size() > 24) return false;
    const auto plain = VisibleNativeText(text);
    const auto nick = NormalizeWantedName(name);
    if (plain.empty() || nick.empty()) return false;
    const std::string suffix = "(" + std::to_string(id) + ")";
    return plain == nick || plain == nick + suffix || plain == nick + " " + suffix;
}

std::string BuildWantedLabelForNativeDraw(NearbyWantedLabelState state, const OnlineSnapshot& snapshot,
    const std::string& nativeText, const std::string& playerName) {
    if (!MatchesNativePlayerNameTag(nativeText, playerName, state.playerId)) return {};
    const auto wanted = FindLiveWantedForName(snapshot, playerName);
    if (!wanted) return {};
    state.wanted = *wanted;
    return ShouldShowNearbyWantedLabel(state) ? BuildNearbyWantedLabel(*wanted) : std::string{};
}

void NearbyWantedLabels::Render(SAMPBridge& bridge, const OnlineDataManager& online, bool enabled) {
#ifndef KEYBINDER_TESTING
    currentBridge = &bridge;
    currentOnline = &online;
    fallbackEnabled = enabled;
    if (!enabled) cacheValid = false;
    if (nameTagHook || bridge.GetVersion() != SAMPBridge::Version::V03DLR1 || !sampapi::GetBase()) return;
    auto hook = safetyhook::InlineHook::create(reinterpret_cast<void*>(sampapi::GetBase() + 0x6C7E0),
                                              reinterpret_cast<void*>(&HookedDrawLabel));
    if (hook) {
        nameTagHook = new safetyhook::InlineHook(std::move(*hook));
        Logger::Info("[WantedLabels] Native CPlayerTags::DrawLabel observer installed");
    } else if (!hookFailureLogged) {
        hookFailureLogged = true;
        Logger::Warning("[WantedLabels] Native name-draw observer could not be installed");
    }
#else
    (void)bridge; (void)online; (void)enabled;
#endif
}

void NearbyWantedLabels::SetEnabledProvider(std::function<bool()> provider) {
#ifndef KEYBINDER_TESTING
    enabledProvider = std::move(provider);
    cacheValid = false;
#else
    (void)provider;
#endif
}

void NearbyWantedLabels::Shutdown(bool removeHook) {
#ifndef KEYBINDER_TESTING
    fallbackEnabled = false;
    enabledProvider = {};
    currentBridge = nullptr;
    currentOnline = nullptr;
    cacheValid = false;
    if (removeHook) { delete nameTagHook; nameTagHook = nullptr; }
#else
    (void)removeHook;
#endif
}
} // namespace keybinder
