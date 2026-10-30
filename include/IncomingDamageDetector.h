#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace keybinder {

// These are sampling bounds, not limits imposed on player health. Servers can
// use extremely large health values as temporary script/protection states;
// differences across such states are not measurable combat damage. Ordinary
// enhanced health/armour values (including values well above 100) remain valid.
inline bool IsMeasurablePlayerVital(float value) {
    return std::isfinite(value) && value < 1000000.0f;
}

// GTA:SA eWeaponType values that CPed::m_nLastWeaponDamage reports for
// non-combat damage sources. The plugin-sdk eWeaponType enum omits these
// engine "pseudo weapons", so they are defined here explicitly. They let the
// incoming-hitsound logic ignore environmental/self damage the same way the
// give/take damage RPC path ignored SA-MP weapon 54.
namespace damage_source {
constexpr int kRammedByCar = 49;
constexpr int kRunOverByCar = 50;
constexpr int kDrowning = 53;
constexpr int kFall = 54;  // also collision / self-inflicted; matches SA-MP weapon 54
}  // namespace damage_source

// Returns true when a CPed::m_nLastWeaponDamage value represents a non-combat
// (environmental/self) damage source that must NOT trigger an incoming
// hitsound.
inline bool IsNonCombatDamageSource(int weaponType) {
    switch (weaponType) {
        case damage_source::kRammedByCar:
        case damage_source::kRunOverByCar:
        case damage_source::kDrowning:
        case damage_source::kFall:
            return true;
        default:
            return false;
    }
}

// RPC115 also reports environmental hits even when a server restores the ped's
// HP before the next frame. Do not turn ordinary combat notifications into a
// second damage source; their measured HP path already works reliably.
inline bool IsEnvironmentalDamageRpcSource(int weaponType) {
    return IsNonCombatDamageSource(weaponType)
        || weaponType == 16 || weaponType == 18 || weaponType == 35
        || weaponType == 36 || weaponType == 37 || weaponType == 39 || weaponType == 51;
}

struct IncomingDamageObservation {
    float damage = 0.0f;
    bool environmental = false;
    bool scripted = false;
    bool IsCombat() const { return damage > 0.0f && !environmental && !scripted; }
};

// Detects incoming combat damage by watching the local player's combined
// health+armour for decreases each frame. This works on every server,
// including those with server-side health where SA-MP never emits a
// client-sent take-damage RPC. It owns no game state and performs no I/O, so
// the decision logic is fully unit-testable.
class IncomingDamageDetector {
public:
    // Minimum combined health+armour drop (in HP units) treated as a hit.
    // Game health is integral, so a half-point epsilon ignores float noise
    // while still catching any real 1+ HP loss.
    static constexpr float kDamageEpsilon = 0.5f;

    // Maximum frames a scripted-suppression window may stay armed without
    // being consumed by an observed HP drop. The window is armed when the
    // server sends a ScrSetPlayerHealth/ScrSetPlayerArmour RPC carrying a
    // lower value; the actual ped HP change may lag the RPC by a few frames.
    // If no matching drop arrives in this many frames, the window expires so
    // it cannot mute a later genuine combat hit. Kept small (3 frames) to
    // minimize the false-negative window where a stale-baseline heal RPC
    // could suppress a real inter-poll combat hit.
    static constexpr int kScriptedSuppressionFrameCap = 3;

    // Feeds one per-frame sample.
    //   alive         - whether a living local player exists this frame
    //   combinedHp    - current health + armour
    //   environmental - whether the latest damage source was non-combat
    // Returns true exactly once per detected combat hit. The baseline always
    // advances to the latest value, so a suppressed environmental drop does
    // not mask the next genuine combat hit.
    bool Observe(bool alive, float combinedHp, bool environmental) {
        const auto observation = ObserveDetailed(alive, combinedHp, environmental);
        return observation.IsCombat() && observation.damage > kDamageEpsilon;
    }

    // Retain the measured amount and suppression category for hitsounds.
    // The independently sampled chat log below does not use this suppression.
    IncomingDamageObservation ObserveDetailed(bool alive, float combinedHp, bool environmental) {
        if (!alive || !std::isfinite(combinedHp) || combinedHp < 0.0f) {
            initialized_ = false;
            scriptedSuppressionArmed_ = false;
            scriptedSuppressionFramesLeft_ = 0;
            return {};
        }

        if (!initialized_) {
            initialized_ = true;
            lastCombinedHp_ = combinedHp;
            scriptedSuppressionArmed_ = false;
            scriptedSuppressionFramesLeft_ = 0;
            return {};
        }

        const float decrease = lastCombinedHp_ - combinedHp;
        lastCombinedHp_ = combinedHp;

        if (decrease <= 0.0001f) {
            // No drop this frame; only expire an armed window, never fire.
            if (scriptedSuppressionArmed_ && --scriptedSuppressionFramesLeft_ <= 0) {
                scriptedSuppressionArmed_ = false;
            }
            return {};
        }

        // A real HP drop arrived. Environmental drops (fall/collision/drown)
        // are already suppressed by their own filter, so they must not consume
        // the scripted-suppression window -- otherwise a fall could burn it
        // and let the later script SetHealth drop play a false hitsound. But
        // they still tick the TTL so a chain of environmental drops cannot keep
        // the window alive past its frame cap.
        if (environmental) {
            if (scriptedSuppressionArmed_ && --scriptedSuppressionFramesLeft_ <= 0) {
                scriptedSuppressionArmed_ = false;
            }
            return {decrease, true, false};
        }

        // A combat-like HP drop. If a scripted-suppression window is armed
        // (server sent ScrSetPlayerHealth/Armour with a lower value), consume
        // it and treat this drop as non-combat exactly once.
        if (scriptedSuppressionArmed_) {
            scriptedSuppressionArmed_ = false;
            scriptedSuppressionFramesLeft_ = 0;
            return {decrease, false, true};
        }

        return {decrease, false, false};
    }

    // Arms a one-shot suppression for the next observed combined-HP decrease.
    // Called by the game thread when it consumes a scripted-health-decrease
    // request flagged by the ScrSetPlayerHealth/ScrSetPlayerArmour RPC hook.
    // The window suppresses exactly the first matching drop, then clears; if no
    // drop arrives within kScriptedSuppressionFrameCap frames, it expires.
    void ArmScriptedSuppression() {
        scriptedSuppressionArmed_ = true;
        scriptedSuppressionFramesLeft_ = kScriptedSuppressionFrameCap;
    }

    bool IsScriptedSuppressionArmed() const { return scriptedSuppressionArmed_; }

    void Reset() {
        initialized_ = false;
        lastCombinedHp_ = 0.0f;
        scriptedSuppressionArmed_ = false;
        scriptedSuppressionFramesLeft_ = 0;
    }

private:
    bool initialized_ = false;
    float lastCombinedHp_ = 0.0f;
    bool scriptedSuppressionArmed_ = false;
    int scriptedSuppressionFramesLeft_ = 0;
};

// The chat damage log deliberately mirrors the AHK HP/armour comparison.
// Server-applied health changes are real measured losses too; this path does
// not depend on damage RPCs, scripted-health suppression or weapon metadata.
struct IncomingDamageLogEntry {
    float healthDamage = 0.0f;
    float armourDamage = 0.0f;
    bool HasDamage() const { return healthDamage >= 1.0f || armourDamage >= 1.0f; }
    float Total() const { return healthDamage + armourDamage; }
    const char* Type() const {
        if (armourDamage > 0.0f && healthDamage <= 0.0f) return "Weste getroffen";
        if (armourDamage > 0.0f && healthDamage > 0.0f) return "Weste + HP Schaden";
        return "HP Schaden";
    }
};

class IncomingDamageLog {
public:
    IncomingDamageLogEntry Observe(float health, float armour,
                                   bool sampleHealth = true, bool sampleArmour = true) {
        // A heal in one pool must not cancel damage to the other pool.
        IncomingDamageLogEntry damage{
            sampleHealth ? ObservePool(health, lastHealth_, healthInitialized_) : 0.0f,
            sampleArmour ? ObservePool(armour, lastArmour_, armourInitialized_) : 0.0f
        };
        return damage.HasDamage() ? damage : IncomingDamageLogEntry{};
    }

    float LastHealth() const { return lastHealth_; }
    float LastArmour() const { return lastArmour_; }
    void RebaseHealth(float value) { healthInitialized_ = false; Observe(value, 0.0f, true, false); }
    void RebaseArmour(float value) { armourInitialized_ = false; Observe(0.0f, value, false, true); }

    void Reset() {
        healthInitialized_ = false;
        armourInitialized_ = false;
        lastHealth_ = 0.0f;
        lastArmour_ = 0.0f;
    }

private:
    static float ObservePool(float value, float& previous, bool& initialized) {
        // A transient protection value must never replace a measured baseline.
        // The bridge bounds how long it may retain it for the same active ped.
        if (!IsMeasurablePlayerVital(value)) return 0.0f;
        value = (std::max)(0.0f, value);
        const float damage = initialized ? (std::max)(0.0f, previous - value) : 0.0f;
        previous = value;
        initialized = true;
        return damage;
    }
    bool healthInitialized_ = false;
    bool armourInitialized_ = false;
    float lastHealth_ = 0.0f;
    float lastArmour_ = 0.0f;
};

// Reconcile two observations of one loss: actual HP changes and client-sent
// TakeDamage notifications. Either can arrive first. Credits are consumed,
// never compared only by timestamp/amount, so two real identical hits survive.
class IncomingDamageRpcEvidence {
public:
    static constexpr unsigned long kReflectionWindowMs = 750;
    void AdvanceFrame() { ++frame_; }
    bool HasKnownVitalsForSource(int source) const {
        const bool healthOnly = source == damage_source::kFall || source == damage_source::kDrowning;
        return healthKnown_ && (healthOnly || armourKnown_);
    }

    void ObserveVitals(float health, float armour) {
        if (IsMeasurablePlayerVital(health)) {
            health = (std::max)(0.0f, health);
            if (!healthKnown_ || health != lastHealth_) healthBudget_ = health;
            if (healthKnown_ && health > lastHealth_) InvalidatePool(1);
            lastHealth_ = health;
            healthKnown_ = true;
        }
        if (IsMeasurablePlayerVital(armour)) {
            armour = (std::max)(0.0f, armour);
            if (!armourKnown_ || armour != lastArmour_) armourBudget_ = armour;
            if (armourKnown_ && armour > lastArmour_) InvalidatePool(2);
            lastArmour_ = armour;
            armourKnown_ = true;
        }
    }

    void ObserveServerVital(bool health, float value) {
        if (!IsMeasurablePlayerVital(value)) return;
        value = (std::max)(0.0f, value);
        // An actual SetHealth RPC is a new authoritative sample even if the
        // raw ped still shows the same protected value. A per-frame poll is not.
        if (health) {
            if (value > healthBudget_) InvalidatePool(1);
            healthBudget_ = lastHealth_ = value;
            healthKnown_ = true;
        } else {
            if (value > armourBudget_) InvalidatePool(2);
            armourBudget_ = lastArmour_ = value;
            armourKnown_ = true;
        }
    }

    IncomingDamageLogEntry ObserveMeasured(IncomingDamageLogEntry damage, int source, unsigned long now) {
        const int family = SourceFamily(source);
        if (family < 0) return damage;
        Expire(now);
        const float covered = Consume(rpcCredits_, damage.Total(), family);
        float remaining = covered;
        const float fromHealth = (std::min)(damage.healthDamage, remaining);
        damage.healthDamage -= fromHealth;
        remaining -= fromHealth;
        damage.armourDamage = (std::max)(0.0f, damage.armourDamage - remaining);
        if (damage.HasDamage()) {
            if (damage.healthDamage > 0.0f) Add(measuredCredits_, damage.healthDamage, family, 1, now);
            if (damage.armourDamage > 0.0f) Add(measuredCredits_, damage.armourDamage, family, 2, now);
        }
        return damage;
    }

    IncomingDamageLogEntry ObserveRpc(float amount, int source, unsigned long now) {
        if (!std::isfinite(amount) || amount < 1.0f || !IsEnvironmentalDamageRpcSource(source)) return {};
        const int family = SourceFamily(source);
        Expire(now);
        amount -= Consume(measuredCredits_, amount, family);
        if (amount < 1.0f) return {};
        // Falling/drowning bypass armour. Explosions may consume both pools;
        // the RPC supplies a total, so no unobserved armour split is printed.
        const bool healthOnly = source == damage_source::kFall || source == damage_source::kDrowning;
        const float available = healthBudget_ + (healthOnly ? 0.0f : armourBudget_);
        if (!HasKnownVitalsForSource(source)) return {}; // Wait for an actual finite/sync vital sample.
        amount = (std::min)(amount, available);
        if (amount < 1.0f) return {};
        float remaining = amount;
        if (!healthOnly) {
            const float armour = (std::min)(armourBudget_, remaining);
            armourBudget_ -= armour;
            remaining -= armour;
        }
        healthBudget_ = (std::max)(0.0f, healthBudget_ - remaining);
        Add(rpcCredits_, amount, family, healthOnly ? 1 : 3, now);
        return {amount, 0.0f};
    }

    void Reset() { *this = IncomingDamageRpcEvidence{}; }

private:
    struct Credit { float amount; int source; unsigned int pools; unsigned long at; unsigned long frame; };
    static int SourceFamily(int source) {
        if (source == damage_source::kFall || source == damage_source::kDrowning) return source;
        if (source == damage_source::kRammedByCar || source == damage_source::kRunOverByCar) return 49;
        return IsEnvironmentalDamageRpcSource(source) ? 51 : -1;
    }
    void Add(std::vector<Credit>& credits, float amount, int source, unsigned int pools, unsigned long now) {
        if (credits.size() == 64) credits.erase(credits.begin());
        credits.push_back({amount, source, pools, now, frame_});
    }
    void InvalidatePool(unsigned int pool) {
        // Healing establishes a new basis in that pool in both directions:
        // an old measured loss must not cover a fresh hit after a heal, and an
        // armour-only heal must not erase a pending health-only fall reflection.
        const auto affected = [pool](const Credit& credit) { return (credit.pools & pool) != 0; };
        measuredCredits_.erase(std::remove_if(measuredCredits_.begin(), measuredCredits_.end(), affected), measuredCredits_.end());
        rpcCredits_.erase(std::remove_if(rpcCredits_.begin(), rpcCredits_.end(), affected), rpcCredits_.end());
    }
    static float Consume(std::vector<Credit>& credits, float amount, int source) {
        const float original = amount;
        for (auto& credit : credits) {
            if (credit.source != source) continue;
            const float used = (std::min)(credit.amount, amount);
            credit.amount -= used;
            amount -= used;
            if (amount <= 0.0f) break;
        }
        credits.erase(std::remove_if(credits.begin(), credits.end(),
            [](const Credit& credit) { return credit.amount <= 0.001f; }), credits.end());
        return original - amount;
    }
    void Expire(unsigned long now) {
        const auto old = [this, now](const Credit& credit) {
            // A slow frame is not evidence that a previously emitted hit was
            // never reflected. Require actual intervening polls as well.
            return static_cast<unsigned long>(now - credit.at) > kReflectionWindowMs
                && static_cast<unsigned long>(frame_ - credit.frame) > 3;
        };
        measuredCredits_.erase(std::remove_if(measuredCredits_.begin(), measuredCredits_.end(), old), measuredCredits_.end());
        rpcCredits_.erase(std::remove_if(rpcCredits_.begin(), rpcCredits_.end(), old), rpcCredits_.end());
    }
    std::vector<Credit> measuredCredits_, rpcCredits_;
    bool healthKnown_ = false, armourKnown_ = false;
    float lastHealth_ = 0.0f, lastArmour_ = 0.0f;
    float healthBudget_ = 0.0f, armourBudget_ = 0.0f;
    unsigned long frame_ = 0;
};

} // namespace keybinder
