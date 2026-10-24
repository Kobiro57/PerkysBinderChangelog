#include "../include/DeathMessageRPCAdapter.h"

// RakNet::BitStream is always needed for parsing
#include "../include/RakHook/include/RakNet/BitStream.h"

// RakHook hooks are only available in production (not testing)
#ifndef KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/rakhook.hpp"
#endif

#include "../include/Logger.h"
#include <ctime>
#include <mutex>

namespace killtracker {

// Parse death message from BitStream
// Format: killerId (short, -1 if no killer), victimId (short), weaponId (byte)
static bool ParseDeathMessageBitStream(RakNet::BitStream* bs, DeathMessageEvent& event) {
    if (!bs) {
        return false;
    }

    // Save current read position to restore later (non-mutating read)
    int originalReadOffset = bs->GetReadOffset();

    // Read death message data
    short killerId;
    short victimId;
    unsigned char weaponId;

    if (!bs->Read(killerId)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }
    if (!bs->Read(victimId)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }
    if (!bs->Read(weaponId)) {
        bs->SetReadOffset(originalReadOffset);
        return false;
    }

    // Restore original read position before returning
    bs->SetReadOffset(originalReadOffset);

    event.killerId = killerId;
    event.victimId = victimId;
    event.weaponId = static_cast<int>(weaponId);
    event.hasKiller = (killerId != -1);
    event.timestamp = std::time(nullptr);

    return true;
}

DeathMessageRPCAdapter::DeathMessageRPCAdapter()
    : initialized_(false)
    , state_(std::make_shared<DeathMessageAdapterState>()) {
}

DeathMessageRPCAdapter::~DeathMessageRPCAdapter() {
    if (initialized_.load()) {
        Shutdown();
    }
}

bool DeathMessageRPCAdapter::Initialize() {
    keybinder::Logger::Debug("[DeathMessageRPCAdapter] Initialize() called, initialized_=%d", initialized_.load());
    
    if (initialized_.load()) {
        state_->isAlive.store(true);
        keybinder::Logger::Debug("[DeathMessageRPCAdapter] Already initialized, skipping");
        return true;
    }

#ifdef KEYBINDER_TESTING
    // In test mode, just mark as initialized
    initialized_ = true;
    state_->isAlive.store(true);
    return true;
#else
    // Register RPC handler with RakHook
    state_->isAlive.store(true);
    std::weak_ptr<DeathMessageAdapterState> weakState = state_;
    keybinder::Logger::Debug("[DeathMessageRPCAdapter] Registering RPC hook with RakHook...");
    
    // Hook incoming death messages (ScrDeathMessage)
    rakhook::on_receive_rpc += [weakState](unsigned char& rpcId, RakNet::BitStream* bs) -> bool {
        if (!rakhook::initialized) {
            return true;
        }

        if (rpcId != RPC_DEATH_MESSAGE) {
            return true; // Not our RPC, pass through
        }
        
        if (auto state = weakState.lock()) {
            {
                std::lock_guard<std::mutex> lock(state->callbackMutex);
                if (!state->isAlive.load() || !state->callback) {
                    return true;
                }
            }

            keybinder::Logger::Debug("[DeathMessageRPCAdapter] RPC 55 (ScrDeathMessage) received!");

            DeathMessageEvent event;
            if (!ParseDeathMessageBitStream(bs, event)) {
                keybinder::Logger::Warning("[DeathMessageRPCAdapter] Failed to parse death message BitStream");
                return true;
            }

            std::lock_guard<std::mutex> lock(state->callbackMutex);
            const bool isAlive = state->isAlive.load();
            const bool hasCallback = !!state->callback;
            if (isAlive && hasCallback) {
                keybinder::Logger::Debug("[DeathMessageRPCAdapter] Death message parsed: killer=%d victim=%d weapon=%d",
                    event.killerId, event.victimId, event.weaponId);
                state->callback(event);
            } else {
                keybinder::Logger::Debug("[DeathMessageRPCAdapter] Callback not available (isAlive=%d, hasCallback=%d)", 
                    isAlive, hasCallback);
            }
        }
        return true; // Pass through to game
    };

    initialized_ = true;
    keybinder::Logger::Info("[DeathMessageRPCAdapter] RPC hook pre-registered successfully, initialized_=%d", initialized_.load());
    return true;
#endif
}

void DeathMessageRPCAdapter::Shutdown() {
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
    keybinder::Logger::Info("[DeathMessageRPCAdapter] Shutdown - callbacks disabled");
#endif

    // Keep initialized_ true: RakHook lambdas stay registered for process life.
}

void DeathMessageRPCAdapter::SetDeathMessageCallback(DeathMessageCallback callback) {
    std::lock_guard<std::mutex> lock(state_->callbackMutex);
    state_->callback = callback;
}

#ifdef KEYBINDER_TESTING
void DeathMessageRPCAdapter::DispatchForTesting(const DeathMessageEvent& event) {
    std::lock_guard<std::mutex> lock(state_->callbackMutex);
    if (state_->isAlive.load() && state_->callback) state_->callback(event);
}
#endif

} // namespace killtracker
