#pragma once

#include "KillTrackerTypes.h"
#include <vector>
#include <string>

// Forward declaration - don't include SAMP-API header here
// This keeps the adapter interface pure
typedef unsigned long D3DCOLOR;

namespace killtracker {

class KillfeedAdapter {
public:
    KillfeedAdapter();
    ~KillfeedAdapter();

    std::vector<KillEvent> PollForNewKills();

    bool IsDeathWindowAvailable() const;

    std::vector<DeathWindowEntry> GetCurrentDeathWindowState() const;

    void ClearState();

#ifdef KEYBINDER_TESTING
    std::vector<KillEvent> ProcessSnapshotForTesting(const std::vector<DeathWindowEntry>& entries,
                                                    bool snapshotAvailable = true) {
        return ProcessSnapshot(entries, snapshotAvailable);
    }
#endif

private:
    static constexpr int kPollIntervalFrames = 10;
    static constexpr int kLocalPlayerRefreshPolls = 30;

    std::vector<DeathWindowEntry> lastKnownState_;
    bool initialized_;
    int pollFrameCounter_;
    int localPlayerRefreshCounter_;
    std::string localPlayerNameCache_;
    bool deathWindowFaulted_ = false;

    KillEvent ConvertToKillEvent(const DeathWindowEntry& entry);

    bool RefreshLocalPlayerName();

    bool IsLocalPlayerKill(const DeathWindowEntry& entry) const;

    bool IsDuplicateEntry(const DeathWindowEntry& entry) const;

    std::vector<KillEvent> ProcessSnapshot(std::vector<DeathWindowEntry> entries, bool snapshotAvailable);

    void* GetDeathWindow() const;

    std::vector<DeathWindowEntry> ReadDeathWindow(void* deathWindow, bool* snapshotAvailable = nullptr) const;
};

} // namespace killtracker
