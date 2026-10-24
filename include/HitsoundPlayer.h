#pragma once

#include "KillTrackerTypes.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#ifndef KEYBINDER_TESTING
struct IXAudio2;
struct IXAudio2MasteringVoice;
#endif

namespace keybinder {

struct Settings;

class HitsoundPlayer {
public:
    enum class Direction {
        Outgoing = 0,
        Incoming = 1
    };

#ifdef KEYBINDER_TESTING
    struct TestingHooks {
        bool failInitialization = false;
        std::function<std::chrono::steady_clock::time_point()> nowProvider;
        std::function<bool(Direction direction, const std::string& sourceKey, bool useEmbeddedDefault)> canLoadSource;
    };

    static void SetTestingHooks(const TestingHooks& hooks);
    static void ResetTestingHooks();
#endif

    static constexpr std::chrono::milliseconds kDirectionCooldown{90};

    HitsoundPlayer();
    ~HitsoundPlayer();

    void ApplySettings(const Settings& settings);
    void OnDamageEvent(const killtracker::DamageEvent& event);
    // Queue an incoming-damage hitsound detected outside the give/take damage
    // RPC (e.g. via local health monitoring). Reuses the same cooldown and
    // playback path as OnDamageEvent so per-direction throttling still applies.
    void OnIncomingDamage();
    void Update();

    void Shutdown();
    void PreviewOutgoing();
    void PreviewIncoming();
    void PreviewPath(Direction direction, const std::string& logicalPath);

    bool IsEnabled() const;
    bool IsOutgoingEnabled() const;
    bool IsIncomingEnabled() const;
    const std::string& GetOutgoingPath() const;
    const std::string& GetIncomingPath() const;
    float GetOutgoingVolume() const;
    float GetIncomingVolume() const;

    struct SlotState;

#ifdef KEYBINDER_TESTING
    std::size_t GetPendingEventCountForTesting() const;
    int GetPlayCountForTesting(Direction direction) const;
    int GetPreviewPlayCountForTesting() const;
    bool HasLoadedSoundForTesting(Direction direction) const;
    bool HasLoadedPreviewSoundForTesting() const;
    bool IsRuntimeAvailableForTesting() const;
    float GetVolumeForTesting(Direction direction) const;
    float GetPreviewVolumeForTesting() const;
    const std::string& GetPreviewSourceKeyForTesting() const;
#endif

private:
    void EnsureRuntimeReady();
    void InitializeRuntime();
    void ShutdownRuntime();
    void DisableRuntime(const char* reason);

    void ReloadState(SlotState& slot, Direction direction, bool enabled, const std::string& logicalPath, float volume, int defaultResourceId);
    void ReloadSlot(Direction direction, bool enabled, const std::string& logicalPath, float volume, int defaultResourceId);
    void ProcessEvent(const killtracker::DamageEvent& event);
    bool TryPlayState(SlotState& slot, Direction direction, bool enforceCooldown);
    bool TryPlay(Direction direction);

    SlotState& GetSlot(Direction direction);
    const SlotState& GetSlot(Direction direction) const;
    std::chrono::steady_clock::time_point Now() const;

    bool enabled_ = true;
    bool runtimeAvailable_ = false;
    bool comInitialized_ = false;

    std::vector<killtracker::DamageEvent> pendingEvents_;
    mutable std::mutex pendingEventsMutex_;

#ifdef KEYBINDER_TESTING
    int playCounts_[2] = {0, 0};
    int previewPlayCount_ = 0;
#endif

    SlotState* outgoingSlot_ = nullptr;
    SlotState* incomingSlot_ = nullptr;
    SlotState* previewSlot_ = nullptr;
    bool runtimeInitializationAttempted_ = false;
    bool slotsDirty_[2] = {true, true};

#ifndef KEYBINDER_TESTING
    IXAudio2* engine_ = nullptr;
    IXAudio2MasteringVoice* masteringVoice_ = nullptr;
#endif
};

} // namespace keybinder
