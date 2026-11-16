#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace keybinder {

struct LocateCheckpoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float radius = 0.0f;
};

// Correlates the server's personal detective messages with a newly received
// server checkpoint. It never reads the local GPS destination or sends chat.
// All methods are called on the game thread. The caller supplies a current,
// verified police role and a host/port/player identity before observing events.
class PoliceLocateTracker {
public:
    using DistrictResolver = std::function<std::string(float, float, float)>;

    static constexpr std::uint32_t kOperationTimeoutMs = 120000;
    static constexpr std::uint32_t kCompletionWindowMs = 5000;
    static constexpr std::uint32_t kEarlyMarkerToleranceMs = 2000;

    void SetSession(bool policeAllowed, const std::string& connectionIdentity);
    void Reset();

    bool ObserveChat(const std::string& line, std::uint32_t nowMs);
    bool ObserveCheckpoint(const LocateCheckpoint& checkpoint, std::uint32_t nowMs);
    void ObserveCheckpointDisabled();

    // Returns an already formatted /r command once, only after the matching
    // success and checkpoint were observed. A missing district never falls
    // back to the local player's location or an old checkpoint.
    std::string TakeRadioMessage(std::uint32_t nowMs, const DistrictResolver& resolveDistrict);

    bool HasPendingLocate() const { return pending_; }

private:
    void ClearOperation();
    bool ValidatePending(std::uint32_t nowMs);

    bool allowed_ = false;
    std::string identity_;
    bool pending_ = false;
    std::string target_;
    std::uint32_t startedAt_ = 0;
    std::uint32_t durationMs_ = 0;
    bool successObserved_ = false;
    std::uint32_t successAt_ = 0;
    bool markerObserved_ = false;
    std::uint32_t markerAt_ = 0;
    LocateCheckpoint marker_{};
};

} // namespace keybinder
