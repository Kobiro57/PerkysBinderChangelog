#include "../include/ServerCheckpointRPCAdapter.h"
#include "../include/RakHook/include/RakNet/BitStream.h"
#ifndef KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/rakhook.hpp"
#endif
#include <cmath>
#include <utility>

namespace keybinder {
namespace {
// Protocol layout: openmultiplayer/open.mp Shared/NetCode/checkpoint.hpp.
// https://github.com/openmultiplayer/open.mp/blob/master/Shared/NetCode/checkpoint.hpp
constexpr unsigned char kSetCheckpoint = 107;
constexpr unsigned char kDisableCheckpoint = 37;
constexpr unsigned char kSetRaceCheckpoint = 38;
constexpr unsigned char kDisableRaceCheckpoint = 39;

bool Dispatch(const std::shared_ptr<ServerCheckpointAdapterState>& state,
              unsigned char id, RakNet::BitStream* payload) {
    if (!state || (id != kSetCheckpoint && id != kDisableCheckpoint
        && id != kSetRaceCheckpoint && id != kDisableRaceCheckpoint)) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->alive || !state->callback) return false;
    ServerCheckpointEvent event;
    event.racing = id == kSetRaceCheckpoint || id == kDisableRaceCheckpoint;
    if (id == kSetCheckpoint || id == kSetRaceCheckpoint) {
        if (!payload) return false;
        const int offset = payload->GetReadOffset();
        unsigned char type = 0;
        float nextX = 0.0f, nextY = 0.0f, nextZ = 0.0f;
        const bool read = (!event.racing || payload->Read(type))
            && payload->Read(event.x) && payload->Read(event.y) && payload->Read(event.z)
            && (!event.racing || (payload->Read(nextX) && payload->Read(nextY) && payload->Read(nextZ)))
            && payload->Read(event.radius);
        payload->SetReadOffset(offset);
        if (!read || !std::isfinite(event.x) || !std::isfinite(event.y)
            || !std::isfinite(event.z) || !std::isfinite(event.radius)
            || std::abs(event.x) > 100000.0f || std::abs(event.y) > 100000.0f
            || std::abs(event.z) > 100000.0f || event.radius <= 0.0f
            || event.radius > 10000.0f) return false;
        event.enabled = true;
    }
    state->callback(event);
    return true;
}
} // namespace

ServerCheckpointRPCAdapter::ServerCheckpointRPCAdapter()
    : state_(std::make_shared<ServerCheckpointAdapterState>()) {}
ServerCheckpointRPCAdapter::~ServerCheckpointRPCAdapter() { Shutdown(); }

bool ServerCheckpointRPCAdapter::Initialize() {
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->alive = true;
    }
    if (hookRegistered_) return true;
#ifndef KEYBINDER_TESTING
    std::weak_ptr<ServerCheckpointAdapterState> weak = state_;
    rakhook::on_receive_rpc += [weak](unsigned char& id, RakNet::BitStream* payload) -> bool {
        if (rakhook::initialized) Dispatch(weak.lock(), id, payload);
        return true;
    };
#endif
    hookRegistered_ = true;
    return true;
}

void ServerCheckpointRPCAdapter::Shutdown() {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->alive = false;
    state_->callback = nullptr;
}

void ServerCheckpointRPCAdapter::SetCallback(std::function<void(const ServerCheckpointEvent&)> callback) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->callback = std::move(callback);
}

#ifdef KEYBINDER_TESTING
bool ServerCheckpointRPCAdapter::ProcessRpcForTesting(unsigned char id, RakNet::BitStream* payload) {
    return Dispatch(state_, id, payload);
}
#endif
} // namespace keybinder
