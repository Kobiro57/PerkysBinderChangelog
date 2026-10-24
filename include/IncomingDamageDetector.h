#pragma once

namespace keybinder {

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
        if (!alive) {
            initialized_ = false;
            scriptedSuppressionArmed_ = false;
            scriptedSuppressionFramesLeft_ = 0;
            return false;
        }

        if (!initialized_) {
            initialized_ = true;
            lastCombinedHp_ = combinedHp;
            scriptedSuppressionArmed_ = false;
            scriptedSuppressionFramesLeft_ = 0;
            return false;
        }

        const bool decreased = combinedHp < lastCombinedHp_ - kDamageEpsilon;
        lastCombinedHp_ = combinedHp;

        if (!decreased) {
            // No drop this frame; only expire an armed window, never fire.
            if (scriptedSuppressionArmed_ && --scriptedSuppressionFramesLeft_ <= 0) {
                scriptedSuppressionArmed_ = false;
            }
            return false;
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
            return false;
        }

        // A combat-like HP drop. If a scripted-suppression window is armed
        // (server sent ScrSetPlayerHealth/Armour with a lower value), consume
        // it and treat this drop as non-combat exactly once.
        if (scriptedSuppressionArmed_) {
            scriptedSuppressionArmed_ = false;
            scriptedSuppressionFramesLeft_ = 0;
            return false;
        }

        return true;
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

}  // namespace keybinder
