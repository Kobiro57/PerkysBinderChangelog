#pragma once

#include "KillTrackerTypes.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// RakHook forward declarations
namespace RakNet {
    class BitStream;
}

namespace killtracker {

// RPC ID for death messages (from SA-MP)
// ScrDeathMessage = 55 (0x37) - killfeed packet with killer, victim, weapon
static const unsigned char RPC_DEATH_MESSAGE = 55;

struct DeathMessageEvent {
    int killerId;      // Player ID of killer (-1 for none/world)
    int victimId;      // Player ID of victim
    int weaponId;      // Weapon ID used
    std::time_t timestamp;
    bool hasKiller;    // false if killerId is -1 (suicide, world, etc.)
};

using DeathMessageCallback = std::function<void(const DeathMessageEvent&)>;

// State shared between adapter and callbacks
struct DeathMessageAdapterState {
    std::atomic<bool> isAlive{false};
    DeathMessageCallback callback;
    mutable std::mutex callbackMutex;
};

// Adapter for tracking kills via Death Message RPC
// This is more reliable than reading from the death window
class DeathMessageRPCAdapter {
public:
    DeathMessageRPCAdapter();
    ~DeathMessageRPCAdapter();

    bool Initialize();
    void Shutdown();

    void SetDeathMessageCallback(DeathMessageCallback callback);
#ifdef KEYBINDER_TESTING
    void DispatchForTesting(const DeathMessageEvent& event);
#endif

private:
    std::atomic<bool> initialized_;
    std::shared_ptr<DeathMessageAdapterState> state_;
};

} // namespace killtracker
