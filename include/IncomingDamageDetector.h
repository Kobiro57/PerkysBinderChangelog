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
    IncomingDamageLogEntry Observe(float health, float armour) {
        if (!IsMeasurablePlayerVital(health) || !IsMeasurablePlayerVital(armour)) {
            Reset();
            return {};
        }
        health = (std::max)(0.0f, health);
        armour = (std::max)(0.0f, armour);
        if (!initialized_) {
            initialized_ = true;
            lastHealth_ = health;
            lastArmour_ = armour;
            return {};
        }
        // A heal in one pool must not cancel damage to the other pool.
        IncomingDamageLogEntry damage{
            (std::max)(0.0f, lastHealth_ - health),
            (std::max)(0.0f, lastArmour_ - armour)
        };
        lastHealth_ = health;
        lastArmour_ = armour;
        return damage.HasDamage() ? damage : IncomingDamageLogEntry{};
    }

    void Reset() {
        initialized_ = false;
        lastHealth_ = 0.0f;
        lastArmour_ = 0.0f;
    }

private:
    bool initialized_ = false;
    float lastHealth_ = 0.0f;
    float lastArmour_ = 0.0f;
};

} // namespace keybinder
