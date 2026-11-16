#include "../include/NearbyWantedLabels.h"
#include "../include/OnlineDataManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/shared/plugin.h"
#include "../include/FactionAccess.h"
#include "../include/SAMPBridge.h"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CPlayerPool.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "CCamera.h"
#include "CMenuManager.h"
#include "CSprite.h"
#include "CTimer.h"
#include "CWorld.h"
#include "ePedBones.h"
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

#ifndef KEYBINDER_TESTING
bool IsAlivePed(const ::CPed* ped) {
    return ped && std::isfinite(ped->m_fHealth) && ped->m_fHealth > 0.0f
        && ped->m_ePedState != PEDSTATE_DIE && ped->m_ePedState != PEDSTATE_DEAD
        && ped->m_ePedState != PEDSTATE_DIE_BY_STEALTH;
}

bool IsFinitePosition(const ::CVector& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

bool HasUnobstructedView(const ::CVector& origin, const ::CVector& target) {
    // Do not use the server's optional through-wall nametag behavior. Buildings,
    // vehicles and objects always block this annotation. Peds are excluded so
    // the target itself cannot block the ray that ends at its head.
    return CWorld::GetIsLineOfSightClear(origin, target, true, true, false,
        true, true, false, false);
}
#endif

} // namespace

bool ShouldShowNearbyWantedLabel(const NearbyWantedLabelState& s) {
    if (!s.overlayEnabled || !s.policeMember || !s.connected || !s.localActive || !s.localAlive
        || !s.streamed || !s.targetAlive || !s.targetVisible || !s.sameInterior
        || !s.serverAllowsLabels || !s.remoteAllowsLabels || !s.cameraLineOfSight
        || !s.localLineOfSight || !s.projectedOnScreen || s.wanted <= 0
        || s.localPlayerId < 0 || s.localPlayerId >= 1004 || s.playerId < 0 || s.playerId >= 1004
        || s.localPlayerId == s.playerId) return false;
    if (!std::isfinite(s.distanceSquared) || s.distanceSquared < 0.0f
        || !std::isfinite(s.serverDrawDistance) || s.serverDrawDistance <= 0.0f) return false;
    const float allowedDistance = (std::min)(kNearbyWantedLabelMaximumDistance, s.serverDrawDistance);
    return s.distanceSquared <= allowedDistance * allowedDistance;
}

std::optional<int> FindLiveWantedForName(const OnlineSnapshot& snapshot, const std::string& name) {
    if (!snapshot.IsLive()) return std::nullopt;
    const std::string normalizedName = NormalizeWantedName(name);
    if (normalizedName.empty()) return std::nullopt;
    std::optional<int> found;
    bool matched = false;
    for (const auto& player : snapshot.players) {
        if (NormalizeWantedName(player.name) != normalizedName) continue;
        // Ambiguous API rows must not assign somebody else's wanted level.
        if (matched) return std::nullopt;
        matched = true;
        if (player.wanted > 0) found = player.wanted;
    }
    return found;
}

std::string BuildNearbyWantedLabel(int wanted) {
    return wanted > 0 ? "[GESUCHT | " + std::to_string(wanted) + " Wanteds]" : std::string{};
}

void NearbyWantedLabels::Render(SAMPBridge& bridge, const OnlineDataManager& online, bool wantedOverlayEnabled) {
#ifndef KEYBINDER_TESTING
    if (!wantedOverlayEnabled || !ImGui::GetCurrentContext()
        || bridge.GetVersion() != SAMPBridge::Version::V03DLR1
        || !IsPoliceMember(bridge.GetLocalFaction()) || FrontEndMenuManager.m_bMenuActive
        || CTimer::m_UserPause || CTimer::m_CodePause) return;
    auto* netGame = bridge.GetNetGame();
    if (!netGame || netGame->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        || !netGame->m_pSettings || !netGame->m_pSettings->m_bNameTags) return;
    const float serverDistance = netGame->m_pSettings->m_fNameTagsDrawDist;
    if (!std::isfinite(serverDistance) || serverDistance <= 0.0f) return;
    auto* pool = netGame->GetPlayerPool();
    if (!pool || pool->m_nLocalPlayerId >= sampapi::v03dl::CPlayerPool::MAX_PLAYERS
        || !pool->pLocalPlayer || !pool->pLocalPlayer->m_bIsActive
        || !pool->pLocalPlayer->m_pPed) return;
    auto* localPed = pool->pLocalPlayer->m_pPed->m_pGamePed;
    if (!IsAlivePed(localPed)) return;
    const ::CVector localPosition = localPed->GetPosition();
    if (!IsFinitePosition(localPosition)) return;
    const OnlineSnapshot snapshot = online.GetSnapshotCopy();
    if (!snapshot.IsLive()) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (!std::isfinite(display.x) || !std::isfinite(display.y) || display.x <= 0 || display.y <= 0) return;
    const ::CVector cameraPosition = TheCamera.m_mCameraMatrix.pos;
    if (!IsFinitePosition(cameraPosition)) return;
    ::CVector localEye = localPosition;
    localEye.z += 0.65f;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    for (int id = 0; id < sampapi::v03dl::CPlayerPool::MAX_PLAYERS; ++id) {
        if (id == pool->m_nLocalPlayerId || !pool->m_bNotEmpty[id]) continue;
        auto* info = pool->m_pObject[id];
        if (!info || info->m_szNick.empty()) continue;
        auto* remote = info->m_pPlayer;
        if (!remote || remote->m_nId != id || !remote->m_bDrawLabels || !remote->m_pPed) continue;
        if (remote->m_nState != sampapi::v03dl::CRemotePlayer::PLAYER_STATE_ONFOOT
            && remote->m_nState != sampapi::v03dl::CRemotePlayer::PLAYER_STATE_DRIVER
            && remote->m_nState != sampapi::v03dl::CRemotePlayer::PLAYER_STATE_PASSENGER) continue;
        auto* ped = remote->m_pPed->m_pGamePed;
        if (!IsAlivePed(ped) || !ped->m_pRwObject || !ped->bIsVisible || ped->bOffscreen
            || ped->bRemoveFromWorld || ped->m_nAreaCode != localPed->m_nAreaCode) continue;
        const ::CVector position = ped->GetPosition();
        if (!IsFinitePosition(position)) continue;
        const double dx = static_cast<double>(position.x) - localPosition.x;
        const double dy = static_cast<double>(position.y) - localPosition.y;
        const double dz = static_cast<double>(position.z) - localPosition.z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;
        const float limit = (std::min)(kNearbyWantedLabelMaximumDistance, serverDistance);
        if (!std::isfinite(distanceSquared) || distanceSquared > static_cast<double>(limit) * limit) continue;
        const auto wanted = FindLiveWantedForName(snapshot, info->m_szNick);
        if (!wanted) continue;
        RwV3d head{};
        ped->GetBonePosition(head, BONE_HEAD, false);
        ::CVector headPosition(head.x, head.y, head.z);
        if (!IsFinitePosition(headPosition)) continue;
        if (!HasUnobstructedView(cameraPosition, headPosition) || !HasUnobstructedView(localEye, headPosition)) continue;
        head.z += 0.5f;
        RwV3d screen{};
        float screenW = 0.0f, screenH = 0.0f;
        if (!CSprite::CalcScreenCoors(head, &screen, &screenW, &screenH, true, true)
            || !std::isfinite(screen.x) || !std::isfinite(screen.y) || !std::isfinite(screen.z)
            || screen.z <= 0.0f) continue;

        const std::string label = BuildNearbyWantedLabel(*wanted);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 textPosition(screen.x - textSize.x * 0.5f, screen.y - textSize.y);
        NearbyWantedLabelState s;
        s.overlayEnabled = s.policeMember = s.connected = s.localActive = s.localAlive = true;
        s.streamed = s.targetAlive = s.targetVisible = s.sameInterior = true;
        s.serverAllowsLabels = s.remoteAllowsLabels = s.cameraLineOfSight = s.localLineOfSight = true;
        s.projectedOnScreen = textPosition.x >= 0.0f && textPosition.y >= 0.0f
            && textPosition.x + textSize.x <= display.x && textPosition.y + textSize.y <= display.y;
        s.localPlayerId = pool->m_nLocalPlayerId;
        s.playerId = id;
        s.wanted = *wanted;
        s.distanceSquared = static_cast<float>(distanceSquared);
        s.serverDrawDistance = serverDistance;
        if (!ShouldShowNearbyWantedLabel(s)) continue;
        drawList->AddText(ImVec2(textPosition.x + 1.0f, textPosition.y + 1.0f), IM_COL32(0, 0, 0, 230), label.c_str());
        drawList->AddText(textPosition, IM_COL32(255, 48, 48, 255), label.c_str());
    }
#else
    (void)bridge; (void)online; (void)wantedOverlayEnabled;
#endif
}

} // namespace keybinder
