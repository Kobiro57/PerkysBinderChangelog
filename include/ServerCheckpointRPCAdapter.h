#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace RakNet { class BitStream; }

namespace keybinder {

struct ServerCheckpointEvent {
    bool enabled = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float radius = 0.0f;
};

struct ServerCheckpointAdapterState {
    bool alive = false;
    std::function<void(const ServerCheckpointEvent&)> callback;
    std::mutex mutex;
};

// Observes server checkpoints, preserving both the RPC and its read offset.
// Local /setgps destinations do not pass through this adapter.
class ServerCheckpointRPCAdapter {
public:
    ServerCheckpointRPCAdapter();
    ~ServerCheckpointRPCAdapter();
    bool Initialize();
    void Shutdown();
    void SetCallback(std::function<void(const ServerCheckpointEvent&)> callback);
#ifdef KEYBINDER_TESTING
    bool ProcessRpcForTesting(unsigned char id, RakNet::BitStream* payload);
#endif
private:
    bool hookRegistered_ = false;
    std::shared_ptr<ServerCheckpointAdapterState> state_;
};

} // namespace keybinder
