#include "../include/GpsManager.h"
#include "../include/GpsRadarOwnership.h"
#include "../include/SAMPBridge.h"
#include "../include/Logger.h"
#include <cmath>
#if !defined(KEYBINDER_TESTING) || defined(NLBINDER_NATIVE_GPS_FIXTURE)
#include "C3dMarkers.h"
#include "C3dMarker.h"
#include "CRadar.h"
#include "CPed.h"
#include "CVector.h"
#include "CWorld.h"
#endif

namespace keybinder {
namespace {
constexpr unsigned int kGpsMarkerId = 0x4E4C4753;
#if !defined(KEYBINDER_TESTING) || defined(NLBINDER_NATIVE_GPS_FIXTURE)
int gpsBlip = -1;
unsigned int gpsBlipEntry = 0;
GpsPosition gpsBlipPosition;
CVector checkpointPosition;
unsigned int checkpointEntry = 0;
bool OwnsGpsBlip() {
    if (gpsBlip == -1) return false;
    const auto index = static_cast<unsigned int>(gpsBlip) & 0xffff;
    if (index >= 175 || !CRadar::ms_RadarTrace) return false;
    const auto& trace = CRadar::ms_RadarTrace[index];
    return OwnsGpsRadarTrace(gpsBlip, gpsBlipPosition, {trace.m_bInUse != 0, trace.m_nCounter,
        trace.m_nBlipType, {trace.m_vecPos.x, trace.m_vecPos.y, trace.m_vecPos.z, 0}});
}
#endif
}
bool ReadGpsPlayerPosition(::SAMPBridge* bridge, GpsPosition& position) {
#ifdef KEYBINDER_TESTING
    return false;
#else
    if (!bridge) return false;
    const auto* network = bridge->GetNetGame();
    const auto* player = bridge->GetLocalPlayer();
    if (!network || network->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        || !player || !player->m_bIsActive || player->m_bDoesSpectating || !player->m_pPed || !player->m_pPed->m_pGamePed) return false;
    const auto& p = player->m_pPed->m_pGamePed->GetPosition();
    position = {p.x, p.y, p.z, static_cast<unsigned char>(player->m_nCurrentInterior)};
    return GpsManager::IsValidPosition(position);
#endif
}
void ClearGpsDestinationMarker() {
#if !defined(KEYBINDER_TESTING) || defined(NLBINDER_NATIVE_GPS_FIXTURE)
    if (OwnsGpsBlip()) CRadar::ClearBlip(gpsBlip);
    gpsBlip = -1; gpsBlipEntry = 0;
    checkpointEntry = 0;
    for (unsigned int i = 0; i < MAX_NUM_3DMARKERS; ++i) {
        auto& marker = C3dMarkers::m_aMarkerArray[i];
        if (static_cast<unsigned int>(marker.m_nIdentifier) == kGpsMarkerId && marker.m_pAtomic) {
            marker.DeleteMarkerObject(); marker.m_bIsUsed = false; marker.m_bMustBeRenderedThisFrame = false;
        }
    }
#endif
}
void RenderGpsDestination(const GpsEntry* destination) {
#if !defined(KEYBINDER_TESTING) || defined(NLBINDER_NATIVE_GPS_FIXTURE)
    if (!destination) { ClearGpsDestinationMarker(); return; }
    CVector target(destination->position.x, destination->position.y, destination->position.z - 0.9f);
    if (!OwnsGpsBlip()) gpsBlip = -1; // The server/game can recycle a radar slot.
    if (gpsBlip == -1 || gpsBlipEntry != destination->id || gpsBlipPosition.x != destination->position.x
        || gpsBlipPosition.y != destination->position.y || gpsBlipPosition.z != destination->position.z) {
        if (gpsBlip != -1) CRadar::ClearBlip(gpsBlip);
        gpsBlip = CRadar::SetCoordBlip(BLIP_COORD, target, 0, BLIP_DISPLAY_BOTH, nullptr);
        if (gpsBlip != -1) CRadar::ChangeBlipColour(gpsBlip, BLIP_COLOUR_RED);
        gpsBlipEntry = destination->id; gpsBlipPosition = destination->position;
    }
    if (OwnsGpsBlip()) {
        CRadar::ChangeBlipDisplay(gpsBlip, BLIP_DISPLAY_BOTH);
        CRadar::ChangeBlipColour(gpsBlip, BLIP_COLOUR_RED);
        CRadar::ChangeBlipScale(gpsBlip, 3);
        CRadar::ChangeBlipBrightness(gpsBlip, 1);
        CRadar::SetCoordBlipAppearance(gpsBlip, 2);
        CRadar::SetBlipAlwaysDisplayInZoom(gpsBlip, true);
        CRadar::ms_RadarTrace[static_cast<unsigned int>(gpsBlip) & 0xffff].m_bShortRange = false;
    }
    const bool checkpointChanged = checkpointEntry != destination->id;
    if (checkpointChanged) {
        checkpointPosition = target;
        bool found = false;
        const float ground = CWorld::FindGroundZFor3DCoord(target.x, target.y, destination->position.z + 3.0f, &found, nullptr);
        if (found && std::isfinite(ground) && std::fabs(ground - target.z) < 6.0f) checkpointPosition.z = ground + 0.1f;
        checkpointEntry = destination->id;
        Logger::Info("[GPS] Navigation checkpoint activated");
    }
    // A normal script checkpoint uses this native marker set and a zero
    // surface normal. Re-submit it before effects/HUD instead of relying
    // on the replaceable game-processing hook.
    bool prepared = false;
    for (unsigned int i = 0; i < MAX_NUM_3DMARKERS; ++i) {
        const auto& marker = C3dMarkers::m_aMarkerArray[i];
        if (static_cast<unsigned int>(marker.m_nIdentifier) == kGpsMarkerId && marker.m_pAtomic
            && marker.m_bMustBeRenderedThisFrame) { prepared = true; break; }
    }
    if (checkpointChanged) {
        for (unsigned int i = 0; i < MAX_NUM_3DMARKERS; ++i) {
            auto& marker = C3dMarkers::m_aMarkerArray[i];
            if (static_cast<unsigned int>(marker.m_nIdentifier) == kGpsMarkerId) {
                marker.m_bIsUsed = false; marker.m_bMustBeRenderedThisFrame = false;
            }
        }
    }
    if (!prepared || checkpointChanged) C3dMarkers::PlaceMarkerSet(kGpsMarkerId, MARKER3D_CYLINDER, checkpointPosition, 3.0f,
        255, 0, 0, 228, 2048, 0.1f, 0);
    // PlaceMarker marks the object as used, but GTA normally sets the active
    // render flag in C3dMarkers::Update. Our HUD/scene calls run after that
    // update, so explicitly submit only our own checkpoint for this pass.
    for (unsigned int i = 0; i < MAX_NUM_3DMARKERS; ++i) {
        auto& marker = C3dMarkers::m_aMarkerArray[i];
        if (static_cast<unsigned int>(marker.m_nIdentifier) == kGpsMarkerId && marker.m_pAtomic
            && marker.m_nType == MARKER3D_CYLINDER) marker.m_bMustBeRenderedThisFrame = true;
    }
#endif
}
} // namespace keybinder
