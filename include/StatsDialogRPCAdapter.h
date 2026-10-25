#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace RakNet { class BitStream; }

namespace keybinder {

// Strings retain the server's game encoding. The bridge normalizes them before
// parsing and imports the result on the game thread, never in this callback.
struct StatsDialogEvent {
    unsigned short id = 0;
    unsigned char style = 0;
    std::string caption;
    std::string text;
};

using StatsDialogCallback = std::function<void(const StatsDialogEvent&)>;

struct StatsDialogAdapterState {
    std::atomic<bool> alive{false};
    StatsDialogCallback callback;
    std::mutex mutex;
};

// Observes complete ShowDialog payloads, including list/tablist text that the
// native CDialog may split into controls. It never suppresses or edits an RPC.
class StatsDialogRPCAdapter {
public:
    StatsDialogRPCAdapter();
    ~StatsDialogRPCAdapter();
    bool Initialize();
    void Shutdown();
    void SetDialogCallback(StatsDialogCallback callback);
#ifdef KEYBINDER_TESTING
    // True means an active adapter decoded and delivered a supported dialog.
    // This is a test result, not a decision to suppress the server RPC.
    bool ProcessRpcForTesting(unsigned char rpcId, RakNet::BitStream* payload);
#endif

private:
    bool hookRegistered_ = false;
    bool compressorHeld_ = false;
    std::shared_ptr<StatsDialogAdapterState> state_;
};

} // namespace keybinder
