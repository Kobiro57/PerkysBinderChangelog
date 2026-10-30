#pragma once

#include <cstdint>

namespace keybinder {
// A life begins only after a stable, spawned, healthy and controllable ped.
// Dead login/funeral states are a baseline, not another observed death.
class LocalDeathLifecycle {
public:
    enum class Phase { Baseline, Alive, Dead };
    static constexpr unsigned long kAliveConfirmationMs = 750;
    void Reset() { phase_ = Phase::Baseline; ped_ = nullptr; healthySamples_ = 0; healthySince_ = 0; }
    bool Observe(bool connected, bool spawned, bool controllable, bool healthy,
                 bool dead, const void* ped, unsigned long now) {
        if (!connected || !ped) { Reset(); return false; }
        if (ped_ && ped_ != ped) Reset();
        ped_ = ped;
        if (dead) {
            const bool observedDeath = phase_ == Phase::Alive;
            phase_ = Phase::Dead;
            healthySamples_ = 0;
            return observedDeath;
        }
        if (!spawned || !controllable || !healthy) {
            healthySamples_ = 0;
            return false;
        }
        if (phase_ == Phase::Alive) return false;
        if (healthySamples_++ == 0) healthySince_ = now;
        if (healthySamples_ >= 3 && static_cast<unsigned long>(now - healthySince_) >= kAliveConfirmationMs) {
            phase_ = Phase::Alive;
            healthySamples_ = 0;
        }
        return false;
    }
    bool ObserveDeathNotification() {
        if (phase_ != Phase::Alive) return false;
        phase_ = Phase::Dead;
        healthySamples_ = 0;
        return true;
    }
    bool IsArmed() const { return phase_ == Phase::Alive; }
    Phase GetPhase() const { return phase_; }
private:
    Phase phase_ = Phase::Baseline;
    const void* ped_ = nullptr;
    unsigned int healthySamples_ = 0;
    unsigned long healthySince_ = 0;
};
} // namespace keybinder
