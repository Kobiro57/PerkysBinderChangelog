#include "../include/GpsManager.h"
#include "../include/SAMPBridge.h"
#ifndef KEYBINDER_TESTING
#include "C3dMarkers.h"
#include "C3dMarker.h"
#include "CRadar.h"
#include "CPed.h"
#include "CVector.h"
#endif

namespace keybinder {
namespace {
constexpr unsigned int kGpsMarkerId = 0x4E4C4753;
#ifndef KEYBINDER_TESTING
int gpsBlip = -1;
unsigned int gpsBlipEntry = 0;
GpsPosition gpsBlipPosition;
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
#ifndef KEYBINDER_TESTING
    if (gpsBlip != -1) { CRadar::ClearBlip(gpsBlip); gpsBlip = -1; gpsBlipEntry = 0; }
    for (unsigned int i = 0; i < MAX_NUM_3DMARKERS; ++i) {
        auto& marker = C3dMarkers::m_aMarkerArray[i];
        if (static_cast<unsigned int>(marker.m_nIdentifier) == kGpsMarkerId && marker.m_bIsUsed) marker.DeleteMarkerObject();
    }
#endif
}
void RenderGpsDestination(const GpsEntry* destination) {
#ifndef KEYBINDER_TESTING
    if (!destination) { ClearGpsDestinationMarker(); return; }
    CVector target(destination->position.x, destination->position.y, destination->position.z - 0.9f);
    if (gpsBlip == -1 || gpsBlipEntry != destination->id || gpsBlipPosition.x != destination->position.x
        || gpsBlipPosition.y != destination->position.y || gpsBlipPosition.z != destination->position.z) {
        if (gpsBlip != -1) CRadar::ClearBlip(gpsBlip);
        gpsBlip = CRadar::SetCoordBlip(BLIP_COORD, target, 0, BLIP_DISPLAY_BLIP_ONLY, nullptr);
        if (gpsBlip != -1) CRadar::ChangeBlipColour(gpsBlip, BLIP_COLOUR_RED);
        gpsBlipEntry = destination->id; gpsBlipPosition = destination->position;
    }
    C3dMarkers::PlaceMarker(kGpsMarkerId, MARKER3D_CYLINDER, target, 3.0f,
        255, 40, 40, 170, 1024, 0.15f, 0, 0.0f, 0.0f, 1.0f, false);
#endif
}
} // namespace keybinder
