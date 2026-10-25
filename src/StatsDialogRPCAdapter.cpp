#include "../include/StatsDialogRPCAdapter.h"
#include "../include/RakHook/include/RakNet/BitStream.h"
#include "../include/RakHook/include/RakNet/StringCompressor.h"
#ifndef KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/rakhook.hpp"
#endif

#include <array>
#include <cstring>
#include <utility>

namespace keybinder {
namespace {
constexpr unsigned char kShowDialogRpc = 61;
constexpr std::size_t kMaximumBodyBytes = 32768;
std::mutex compressorMutex;

struct RestoreReadOffset {
    RakNet::BitStream& stream;
    int offset;
    ~RestoreReadOffset() { stream.SetReadOffset(offset); }
};

bool ReadByteString(RakNet::BitStream& stream, std::string& text) {
    unsigned char length = 0;
    if (!stream.Read(length) || stream.GetNumberOfUnreadBits() < static_cast<int>(length) * 8) return false;
    text.resize(length);
    return (length == 0 || stream.Read(text.data(), length)) && text.find('\0') == std::string::npos;
}

bool DecodeDialog(RakNet::BitStream* payload, StatsDialogEvent& event) {
    if (!payload) return false;
    RestoreReadOffset restore{*payload, payload->GetReadOffset()};
    StatsDialogEvent parsed;
    if (!payload->Read(parsed.id) || parsed.id == 0xFFFF || !payload->Read(parsed.style)) return false;
    // Input/password dialogs must never be treated as statistics. List and
    // tablist styles carry the same full text in this RPC as message boxes.
    if (parsed.style != 0 && parsed.style != 2 && parsed.style != 4 && parsed.style != 5) return false;
    std::string firstButton, secondButton;
    if (!ReadByteString(*payload, parsed.caption) || !ReadByteString(*payload, firstButton)
        || !ReadByteString(*payload, secondButton)) return false;
    std::array<char, kMaximumBodyBytes + 1> text{};
    // Our linked RakNet implementation has its own compressor singleton. The
    // adapter retains a reference rather than assuming SA-MP initialized it.
    std::lock_guard<std::mutex> compressorLock(compressorMutex);
    auto* compressor = StringCompressor::Instance();
    if (!compressor || !compressor->DecodeString(text.data(), static_cast<int>(text.size()), payload)) return false;
    const std::size_t size = std::strlen(text.data());
    if (size >= kMaximumBodyBytes) return false; // DecodeString otherwise silently truncates.
    parsed.text.assign(text.data(), size);
    event = std::move(parsed);
    return true;
}

bool DispatchDialog(const std::shared_ptr<StatsDialogAdapterState>& state,
                    unsigned char rpcId, RakNet::BitStream* payload) {
    if (rpcId != kShowDialogRpc || !state) return false;
    // Shutdown waits for an in-flight callback, so a queued bridge callback
    // cannot run after Shutdown returns or after its owner has been destroyed.
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->alive.load(std::memory_order_acquire) || !state->callback) return false;
    StatsDialogEvent event;
    if (!DecodeDialog(payload, event)) return false;
    state->callback(event);
    return true;
}
} // namespace

StatsDialogRPCAdapter::StatsDialogRPCAdapter() : state_(std::make_shared<StatsDialogAdapterState>()) {}
StatsDialogRPCAdapter::~StatsDialogRPCAdapter() { Shutdown(); }

bool StatsDialogRPCAdapter::Initialize() {
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!compressorHeld_) {
            std::lock_guard<std::mutex> compressorLock(compressorMutex);
            StringCompressor::AddReference();
            compressorHeld_ = true;
        }
        state_->alive.store(true, std::memory_order_release);
    }
    if (hookRegistered_) return true;
#ifndef KEYBINDER_TESTING
    std::weak_ptr<StatsDialogAdapterState> weakState = state_;
    rakhook::on_receive_rpc += [weakState](unsigned char& rpcId, RakNet::BitStream* payload) -> bool {
        if (rakhook::initialized) DispatchDialog(weakState.lock(), rpcId, payload);
        return true;
    };
#endif
    hookRegistered_ = true;
    return true;
}

void StatsDialogRPCAdapter::Shutdown() {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->alive.store(false, std::memory_order_release);
    state_->callback = nullptr;
    if (compressorHeld_) {
        std::lock_guard<std::mutex> compressorLock(compressorMutex);
        StringCompressor::RemoveReference();
        compressorHeld_ = false;
    }
}

void StatsDialogRPCAdapter::SetDialogCallback(StatsDialogCallback callback) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->callback = std::move(callback);
}

#ifdef KEYBINDER_TESTING
bool StatsDialogRPCAdapter::ProcessRpcForTesting(unsigned char rpcId, RakNet::BitStream* payload) {
    return DispatchDialog(state_, rpcId, payload);
}
#endif

} // namespace keybinder
