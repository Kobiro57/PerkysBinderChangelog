#pragma once

#include "KillTrackerTypes.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// Forward declarations for RakHook types
namespace RakNet {
    class BitStream;
}

namespace killtracker {

// Callback type for damage events
typedef std::function<void(const DamageEvent&)> DamageEventCallback;

// Shared state for safe callback management
// This allows callbacks to check if the adapter is still alive
struct DamageAdapterState {
    std::atomic<bool> isAlive{false};
    DamageEventCallback callback;
    mutable std::mutex callbackMutex;
};

// Adapter for damage-related RPC interception
// Hooks into RakHook to capture damage events without modifying packets
class DamageRPCAdapter {
public:
    DamageRPCAdapter();
    ~DamageRPCAdapter();

    // Initialize the adapter and register RPC hooks
    // Must be called after RakHook is initialized
    bool Initialize();

    // Shutdown and unregister hooks
    void Shutdown();

    // Set callback for damage events
    void SetDamageCallback(DamageEventCallback callback);

    // Check if adapter is initialized
    bool IsInitialized() const { return initialized_.load(); }

private:
    std::atomic<bool> initialized_;
    std::shared_ptr<DamageAdapterState> state_;

    // SA-MP RPC 115 (RPC_GIVETAKEDAMAGE) carries both give and take damage; a
    // leading bool flag (0 = give/outgoing, 1 = take/incoming) selects which.
    // We hook only outgoing sends; incoming damage is detected from local
    // health changes (see SAMPBridge::PollLocalIncomingDamage).
    static const unsigned char RPC_SEND_GIVE_DAMAGE = 115;
};

} // namespace killtracker
