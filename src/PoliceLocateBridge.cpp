#include "../include/SAMPBridge.h"
#include "../include/FactionAccess.h"
#include "../include/Logger.h"
#include "../include/CheckpointLocation.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#ifndef KEYBINDER_TESTING
#include "CTheZones.h"
#endif

namespace {
bool SamePoliceCheckpoint(const keybinder::LocateCheckpoint& left, const keybinder::LocateCheckpoint& right) {
    return std::abs(left.x - right.x) <= 0.25f && std::abs(left.y - right.y) <= 0.25f
        && std::abs(left.z - right.z) <= 0.25f && std::abs(left.radius - right.radius) <= 0.25f;
}

bool NearPoliceCompletion(unsigned long at, unsigned long completedAt) {
    return static_cast<unsigned long>(at - completedAt) <= keybinder::PoliceLocateTracker::kCompletionWindowMs
        || static_cast<unsigned long>(completedAt - at) <= keybinder::PoliceLocateTracker::kCompletionWindowMs;
}

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

std::vector<keybinder::CheckpointLocationZone> ReadCheckpointZones(bool mapZones) {
    const int count = mapZones ? CTheZones::TotalNumberOfMapZones : CTheZones::TotalNumberOfInfoZones;
    if (count < 1 || count > (mapZones ? 39 : 380)) return {};
    std::vector<keybinder::CheckpointLocationZone> zones;
    zones.reserve(count - 1);
    // Index zero is the whole-world container, not a city or a district.
    for (short index = 1; index < count; ++index) {
        CZone* native = mapZones ? CTheZones::GetMapZone(index) : CTheZones::GetInfoZone(index);
        if (!native) continue;
        keybinder::NativeCheckpointZoneRecord record{};
        std::memcpy(&record, native, sizeof(record));
        if (mapZones ? record.type != 3 : record.type > 1) continue;
        const std::string name = ZoneTextToUtf8(native->GetTranslatedName());
        if (keybinder::CleanCheckpointPlace(name).empty()) continue;
        zones.push_back({static_cast<float>(record.x1), static_cast<float>(record.y1),
            static_cast<float>(record.z1), static_cast<float>(record.x2),
            static_cast<float>(record.y2), static_cast<float>(record.z2), record.type, record.level, name});
    }
    return zones;
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
    policeLocatePreferredRacing_ = false;
    policeLocateHasSelectedCheckpoint_ = false;
    for (auto& checkpoint : policeLocateCheckpoints_) checkpoint = {};
    policeLocateCheckpointSequence_ = policeLocateSuccessAt_ = 0;
}

void SAMPBridge::CapturePoliceLocateChat(const std::string& text) {
    if (text.empty() || text.size() > 1024) return;
    std::lock_guard<std::mutex> lock(policeLocateMutex_);
    if (!policeLocateCaptureActive_) {
        if (text.rfind("Detektiv: Das Handy und die Ortung von ", 0) == 0
            || text == "Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.")
            keybinder::Logger::Info("[SAMPBridge] Detective message ignored: verified police session is not ready.");
        return;
    }
    if (pendingPoliceLocateEvents_.size() >= 64) {
        pendingPoliceLocateEvents_.clear();
        policeLocateCaptureActive_ = false;
        policeLocateQueueOverflow_ = true;
        return;
    }
    PendingPoliceLocateEvent event{false, text, {}, PoliceTick()};
    // Snapshot at receive time, before a same-frame checkpoint packet can
    // replace the destination. An already active job/GPS marker is baseline.
    for (int kind = 0; kind < 2; ++kind)
        event.native[kind].enabled = ReadPoliceCheckpointKind(kind != 0, event.native[kind].checkpoint);
    pendingPoliceLocateEvents_.push_back(std::move(event));
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

bool SAMPBridge::ReadPoliceCheckpointKind(bool racing, keybinder::LocateCheckpoint& checkpoint) const {
    const auto* game = GetGame();
    if (!game) return false;
    const bool enabled = racing ? game->m_racingCheckpoint.m_bEnabled != FALSE
        : game->m_checkpoint.m_bEnabled != FALSE;
    if (!enabled) return false;
    const auto& position = racing ? game->m_racingCheckpoint.m_currentPosition
        : game->m_checkpoint.m_position;
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
        || std::abs(position.x) > 20000.0f || std::abs(position.y) > 20000.0f
        || std::abs(position.z) > 20000.0f) return false;
    const float radius = racing ? game->m_racingCheckpoint.m_fSize : game->m_checkpoint.m_size.x;
    checkpoint = {position.x, position.y, position.z,
        std::isfinite(radius) && radius > 0.0f && radius <= 1000.0f ? radius : 1.0f};
    return true;
}

bool SAMPBridge::ReadPoliceCheckpoint(keybinder::LocateCheckpoint& checkpoint) const {
    // Validation must read the exact kind selected for this operation. Falling
    // back to another active marker could report a completely different town.
    return policeLocateHasSelectedCheckpoint_
        && ReadPoliceCheckpointKind(policeLocatePreferredRacing_, checkpoint);
}

std::string SAMPBridge::ResolvePoliceCheckpointDistrict(float x, float y, float z) const {
    // Validate against the marker actually active in the game, including race
    // checkpoints. A missing RPC must not suppress a confirmed detective result.
    keybinder::LocateCheckpoint current;
    if (!ReadPoliceCheckpoint(current) || std::abs(current.x - x) > 0.25f
        || std::abs(current.y - y) > 0.25f || std::abs(current.z - z) > 0.25f) return {};
#ifdef KEYBINDER_TESTING
    const auto& hooks = GetTestingHooks();
    const auto city = hooks.getCityAtPosition ? hooks.getCityAtPosition(x, y, z) : std::string{};
    const auto district = hooks.getDistrictAtPosition ? hooks.getDistrictAtPosition(x, y, z) : std::string{};
    return keybinder::FormatCheckpointLocation(city, district);
#else
    return keybinder::ResolveCheckpointLocation(x, y, z, ReadCheckpointZones(false), ReadCheckpointZones(true));
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
        if (!allowed || identity != policeLocateSession_) {
            pendingPoliceLocateEvents_.clear();
            policeLocatePreferredRacing_ = false;
            policeLocateHasSelectedCheckpoint_ = false;
            for (auto& checkpoint : policeLocateCheckpoints_) checkpoint = {};
            policeLocateCheckpointSequence_ = policeLocateSuccessAt_ = 0;
        }
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
        if (!event.isCheckpoint) {
            if (policeLocateTracker_.ObserveChat(event.text, event.at)) {
                if (policeLocateTracker_.HasSuccessfulLocate()) policeLocateSuccessAt_ = event.at;
                else {
                    policeLocateHasSelectedCheckpoint_ = false;
                    policeLocateCheckpointSequence_ = policeLocateSuccessAt_ = 0;
                    for (int kind = 0; kind < 2; ++kind) {
                        policeLocateCheckpoints_[kind] = {};
                        policeLocateCheckpoints_[kind].lastNative = event.native[kind];
                    }
                }
                keybinder::Logger::Info("[SAMPBridge] Detective locate %s accepted: %s",
                    policeLocateTracker_.HasSuccessfulLocate() ? "completion" : "start", event.text.c_str());
            }
        } else if (policeLocateTracker_.HasPendingLocate()) {
            auto& state = policeLocateCheckpoints_[event.checkpoint.racing ? 1 : 0];
            if (!event.checkpoint.enabled) {
                state.rpcObserved = state.nativeChanged = false;
                state.blockedAfterDisable = true;
            } else {
                state.rpcObserved = true;
                state.blockedAfterDisable = false;
                state.rpcCheckpoint = {event.checkpoint.x, event.checkpoint.y,
                    event.checkpoint.z, event.checkpoint.radius};
                state.rpcAt = event.at;
                state.rpcSequence = ++policeLocateCheckpointSequence_;
            }
        }
    }
    PoliceLocateNativeSnapshot native[2]{};
    if (policeLocateTracker_.HasPendingLocate()) {
        for (int kind = 0; kind < 2; ++kind) {
            auto& state = policeLocateCheckpoints_[kind];
            native[kind].enabled = ReadPoliceCheckpointKind(kind != 0, native[kind].checkpoint);
            if (!native[kind].enabled) {
                state.nativeChanged = false;
                // The disable packet has now actually reached the game. A
                // later disabled-to-enabled transition is a new destination,
                // even if its enable packet is missed by the receive hook.
                state.blockedAfterDisable = false;
            }
            else if ((!state.lastNative.enabled
                || !SamePoliceCheckpoint(native[kind].checkpoint, state.lastNative.checkpoint))
                && !state.blockedAfterDisable) {
                state.nativeChanged = true;
                state.nativeChangedAt = now;
            }
            state.lastNative = native[kind];
        }
    }
    const bool completedBeforePolling = policeLocateTracker_.HasSuccessfulLocate();
    const bool allowNativeFallback = policeLocateTracker_.ReadyForCurrentCheckpoint(now);
    policeLocateHasSelectedCheckpoint_ = false;
    int selected = -1;
    unsigned long latestSequence = 0;
    bool hasFreshRpc = false;
    if (policeLocateTracker_.HasSuccessfulLocate()) {
        // A fresh server packet identifies the destination even if another
        // checkpoint kind remains active. Wait until native application agrees.
        for (int kind = 0; kind < 2; ++kind) {
            const auto& state = policeLocateCheckpoints_[kind];
            if (!state.rpcObserved || !NearPoliceCompletion(state.rpcAt, policeLocateSuccessAt_)) continue;
            hasFreshRpc = true;
            if (state.rpcSequence > latestSequence) {
                latestSequence = state.rpcSequence;
                selected = native[kind].enabled
                    && SamePoliceCheckpoint(native[kind].checkpoint, state.rpcCheckpoint) ? kind : -1;
            }
        }
        // A receive hook may miss the packet. In that case use only a native
        // marker that actually changed during this locate, near its completion.
        // Two changed kinds are ambiguous: never announce an arbitrary first one.
        if (selected < 0 && !hasFreshRpc && allowNativeFallback) {
            int changedCount = 0;
            for (int kind = 0; kind < 2; ++kind) {
                const auto& state = policeLocateCheckpoints_[kind];
                if (native[kind].enabled && state.nativeChanged && !state.blockedAfterDisable
                    && NearPoliceCompletion(state.nativeChangedAt, policeLocateSuccessAt_)) {
                    selected = kind;
                    ++changedCount;
                }
            }
            if (changedCount != 1) selected = -1;
        }
    }
    if (selected >= 0) {
        policeLocatePreferredRacing_ = selected != 0;
        policeLocateHasSelectedCheckpoint_ = true;
        // Freshness was checked against each original event timestamp above.
        // The tracker must receive the current observation time: replaying a
        // pre-success timestamp here looks like a wrapped/expired operation.
        policeLocateTracker_.ObserveCheckpoint(native[selected].checkpoint, now);
    } else policeLocateTracker_.ObserveCheckpointDisabled();
    if (completedBeforePolling && !policeLocateTracker_.HasPendingLocate()) {
        const auto* game = GetGame();
        keybinder::Logger::Warning("[SAMPBridge] Detective result expired before radio dispatch (normal=%d race=%d input=%d selected=%d).",
            game && game->m_checkpoint.m_bEnabled ? 1 : 0,
            game && game->m_racingCheckpoint.m_bEnabled ? 1 : 0,
            GetInput() ? 1 : 0, selected);
    }
    if (GetInput() == nullptr) return;
    const std::string radio = policeLocateTracker_.TakeRadioMessage(now, [this](float x, float y, float z) {
        return ResolvePoliceCheckpointDistrict(x, y, z);
    });
    if (!radio.empty()) {
        keybinder::Logger::Info("[SAMPBridge] Detective radio result from the current checkpoint: %s", radio.c_str());
        if (!SendCommand(radio)) keybinder::Logger::Warning("[SAMPBridge] Detective radio command could not be sent.");
    }
}

#ifdef KEYBINDER_TESTING
bool SAMPBridge::ProcessServerCheckpointRpcForTesting(unsigned char id, RakNet::BitStream* payload) {
    return policeCheckpointAdapter_ && policeCheckpointAdapter_->ProcessRpcForTesting(id, payload);
}
#endif
