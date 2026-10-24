#include "../include/DamageRPCAdapter.h"

#include "../include/RakHook/include/RakNet/BitStream.h"

#ifndef KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/rakhook.hpp"
#endif

#include "../include/Logger.h"
#include <ctime>
#include <mutex>

namespace killtracker {

static bool ParseDamageBitStream(RakNet::BitStream* bs, DamageEvent& event, bool& outIsGive) {
    if (!bs) {
        return false;
    }

    int originalReadOffset = bs->GetReadOffset();

    // SA-MP RPC 115 (GiveTakeDamage) payload.
    // Skipping bGive shifts every subsequent field and corrupts parsing.
    bool bGive;
    if (!bs->Read(bGive)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }

    unsigned short targetId;
    float damage;
    unsigned int weaponId;
    unsigned int bodyPart;

    if (!bs->Read(targetId)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }
    if (!bs->Read(damage)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }
    if (!bs->Read(weaponId)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }
    if (!bs->Read(bodyPart)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }

    bs->SetReadOffset(originalReadOffset);

    event.targetId = targetId;
    event.damage = damage;
    event.weaponId = weaponId;
    event.bodyPart = bodyPart;
    event.timestamp = std::time(nullptr);
    outIsGive = bGive;

    return true;
}

static void DispatchDamageBitStream(const std::shared_ptr<DamageAdapterState>& state, RakNet::BitStream* bs) {
    DamageEvent event;
    bool isTake = false;
    if (!ParseDamageBitStream(bs, event, isTake)) return;
    event.isOutgoing = !isTake;
    std::lock_guard<std::mutex> lock(state->callbackMutex);
    if (state->isAlive.load() && state->callback) state->callback(event);
}

DamageRPCAdapter::DamageRPCAdapter()
    : initialized_(false)
    , state_(std::make_shared<DamageAdapterState>()) {
}

DamageRPCAdapter::~DamageRPCAdapter() {
    if (initialized_.load()) {
        Shutdown();
    }
}

bool DamageRPCAdapter::Initialize() {
    if (initialized_.load()) {
        state_->isAlive.store(true);
        return true;
    }

#ifdef KEYBINDER_TESTING
    initialized_ = true;
    state_->isAlive.store(true);
    return true;
#else
    state_->isAlive.store(true);
    std::weak_ptr<DamageAdapterState> weakState = state_;

    rakhook::on_send_rpc += [weakState](int& rpcId, RakNet::BitStream* bs,
                                   PacketPriority& priority, PacketReliability& reliability,
                                   char& ord_channel, bool& sh_timestamp) -> bool {
        if (!rakhook::initialized) {
            return true;
        }

        keybinder::Logger::Debug("[DamageRPCAdapter] RPC sent: id=%d", rpcId);
        if (rpcId != static_cast<int>(RPC_SEND_GIVE_DAMAGE)) {
            return true;
        }
        
        if (auto state = weakState.lock()) {
            DispatchDamageBitStream(state, bs);
        }
        return true;
    };

    initialized_ = true;
    keybinder::Logger::Debug("[DamageRPCAdapter] Pre-registered RPC hooks");
    return true;
#endif
}

void DamageRPCAdapter::Shutdown() {
    if (!initialized_.load()) {
        return;
    }

    if (!state_->isAlive.exchange(false)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_->callbackMutex);
        state_->callback = nullptr;
    }
    
#ifndef KEYBINDER_TESTING
    keybinder::Logger::Debug("[DamageRPCAdapter] Shutdown - callbacks disabled");
#endif

    // Keep initialized_ true: RakHook lambdas stay registered for process life.
}

void DamageRPCAdapter::SetDamageCallback(DamageEventCallback callback) {
    std::lock_guard<std::mutex> lock(state_->callbackMutex);
    state_->callback = callback;
}

#ifdef KEYBINDER_TESTING
void DamageRPCAdapter::ProcessRpcForTesting(RakNet::BitStream* payload) {
    DispatchDamageBitStream(state_, payload);
}
#endif

} // namespace killtracker
