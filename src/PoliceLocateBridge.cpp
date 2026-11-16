#include "../include/SAMPBridge.h"
#include "../include/FactionAccess.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#ifndef KEYBINDER_TESTING
#include "CTheZones.h"
#endif

namespace {
unsigned long PoliceTick() {
#ifdef KEYBINDER_TESTING
    const auto& hooks = SAMPBridge::GetTestingHooks();
    if (hooks.getTickCount) return hooks.getTickCount();
#endif
    return GetTickCount();
}

#ifndef KEYBINDER_TESTING
std::string ZoneTextToUtf8(const char* text) {
    if (!text || !*text) return {};
    const int length = static_cast<int>(strnlen(text, 256));
    if (length == 256) return {};
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, nullptr, 0) > 0)
        return std::string(text, length);
    const int wideLength = MultiByteToWideChar(CP_ACP, 0, text, length, nullptr, 0);
    if (wideLength <= 0) return {};
    std::wstring wide(wideLength, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, length, wide.data(), wideLength);
    const int count = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, result.data(), count, nullptr, nullptr);
    return result;
}
#endif
} // namespace

void SAMPBridge::InitializePoliceLocate() {
    if (!policeCheckpointAdapter_)
        policeCheckpointAdapter_ = std::make_unique<keybinder::ServerCheckpointRPCAdapter>();
    policeCheckpointAdapter_->SetCallback([this](const keybinder::ServerCheckpointEvent& event) {
        CapturePoliceCheckpoint(event);
    });
    policeCheckpointAdapter_->Initialize();
}

void SAMPBridge::ShutdownPoliceLocate() {
    {
        std::lock_guard<std::mutex> lock(policeLocateMutex_);
        policeLocateCaptureActive_ = false;
        policeLocateQueueOverflow_ = false;
        pendingPoliceLocateEvents_.clear();
    }
    if (policeCheckpointAdapter_) policeCheckpointAdapter_->Shutdown();
    policeCheckpointAdapter_.reset();
    policeLocateTracker_.Reset();
    policeLocateSession_.clear();
}

void SAMPBridge::CapturePoliceLocateChat(const std::string& text) {
    if (text.empty() || text.size() > 1024) return;
    std::lock_guard<std::mutex> lock(policeLocateMutex_);
    if (!policeLocateCaptureActive_) return;
    if (pendingPoliceLocateEvents_.size() >= 64) {
        pendingPoliceLocateEvents_.clear();
        policeLocateCaptureActive_ = false;
        policeLocateQueueOverflow_ = true;
        return;
    }
    pendingPoliceLocateEvents_.push_back({false, text, {}, PoliceTick()});
}

void SAMPBridge::CapturePoliceCheckpoint(const keybinder::ServerCheckpointEvent& event) {
    std::lock_guard<std::mutex> lock(policeLocateMutex_);
    if (!policeLocateCaptureActive_) return;
    if (pendingPoliceLocateEvents_.size() >= 64) {
        pendingPoliceLocateEvents_.clear();
        policeLocateCaptureActive_ = false;
        policeLocateQueueOverflow_ = true;
        return;
    }
    pendingPoliceLocateEvents_.push_back({true, {}, event, PoliceTick()});
}

std::string SAMPBridge::ResolvePoliceCheckpointDistrict(float x, float y, float z) const {
    // The server RPC is observed before the game applies it. Only announce a
    // marker that is actually enabled at the received location this frame.
    const auto* game = GetGame();
    if (!game || !game->m_checkpoint.m_bEnabled
        || !std::isfinite(game->m_checkpoint.m_position.x)
        || !std::isfinite(game->m_checkpoint.m_position.y)
        || !std::isfinite(game->m_checkpoint.m_position.z)
        || std::abs(game->m_checkpoint.m_position.x - x) > 0.25f
        || std::abs(game->m_checkpoint.m_position.y - y) > 0.25f
        || std::abs(game->m_checkpoint.m_position.z - z) > 0.25f) return {};
#ifdef KEYBINDER_TESTING
    const auto& hooks = GetTestingHooks();
    return hooks.getDistrictAtPosition ? hooks.getDistrictAtPosition(x, y, z) : std::string{};
#else
    const CVector position(x, y, z);
    CZone* zone = CTheZones::FindSmallestZoneForPosition(position, false);
    return zone ? ZoneTextToUtf8(zone->GetTranslatedName()) : std::string{};
#endif
}

void SAMPBridge::UpdatePoliceLocate(unsigned long now) {
    const auto* network = GetNetGame();
    const auto* local = GetLocalPlayer();
    const std::string playerName = GetLocalPlayerName();
    const bool allowed = initialized_ && network && !playerName.empty()
        && network->m_nGameState == sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        && local && local->m_bIsActive && !local->m_bIsWasted
        && keybinder::IsPoliceMember(GetLocalFaction());
    std::string identity;
    if (allowed) {
        identity = std::string(network->m_szHostAddress, strnlen(network->m_szHostAddress, 257))
            + ":" + std::to_string(network->m_nPort) + "/" + playerName;
        std::transform(identity.begin(), identity.end(), identity.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    }
    std::vector<PendingPoliceLocateEvent> events;
    bool overflow = false;
    {
        std::lock_guard<std::mutex> lock(policeLocateMutex_);
        if (!allowed || identity != policeLocateSession_) pendingPoliceLocateEvents_.clear();
        overflow = policeLocateQueueOverflow_;
        policeLocateQueueOverflow_ = false;
        policeLocateCaptureActive_ = allowed;
        policeLocateSession_ = identity;
        events.swap(pendingPoliceLocateEvents_);
    }
    if (overflow) policeLocateTracker_.Reset();
    policeLocateTracker_.SetSession(allowed, identity);
    if (!allowed) return;
    for (const auto& event : events) {
        if (!event.isCheckpoint) policeLocateTracker_.ObserveChat(event.text, event.at);
        else if (!event.checkpoint.enabled) policeLocateTracker_.ObserveCheckpointDisabled();
        else policeLocateTracker_.ObserveCheckpoint({event.checkpoint.x, event.checkpoint.y,
            event.checkpoint.z, event.checkpoint.radius}, event.at);
    }
    if (GetInput() == nullptr) return;
    const std::string radio = policeLocateTracker_.TakeRadioMessage(now, [this](float x, float y, float z) {
        return ResolvePoliceCheckpointDistrict(x, y, z);
    });
    if (!radio.empty()) SendCommand(radio);
}

#ifdef KEYBINDER_TESTING
bool SAMPBridge::ProcessServerCheckpointRpcForTesting(unsigned char id, RakNet::BitStream* payload) {
    return policeCheckpointAdapter_ && policeCheckpointAdapter_->ProcessRpcForTesting(id, payload);
}
#endif
