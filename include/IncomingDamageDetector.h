#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

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

    // Keep the measured amount for the damage log, including fractional hits.
    // Scripted drops remain distinguishable so only a nearby combat RPC may
    // confirm one; a SetHealth reset alone must never produce a log message.
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

// Only measured losses may become chat lines. RPCs can split a measured loss
// into individual shots when their sum agrees; a lone RPC never invents HP
// damage. A brief window accommodates RPC/health updates arriving in either
// order. Without matching RPCs, each eligible sampled loss is logged once.
class IncomingDamageLog {
public:
    static constexpr unsigned long kMatchWindowMs = 180;

    void Report(float damage, unsigned long tick) {
        if (std::isfinite(damage) && damage > 0.0001f) {
            reports_.push_back({damage, tick, false});
            if (reports_.size() > 64) reports_.erase(reports_.begin());
        }
    }

    void Observe(const IncomingDamageObservation& observation, unsigned long tick) {
        if (observation.damage > 0.0001f && std::isfinite(observation.damage)) {
            // A server-applied hit can retain an old environmental weapon ID.
            // Only an accompanying combat RPC may confirm such a loss.
            losses_.push_back({observation.damage, tick, !observation.scripted && !observation.environmental});
        }
    }

    std::vector<float> Drain(unsigned long now, bool finishLife = false) {
        std::vector<float> result;
        while (!losses_.empty()) {
            bool matched = false;
            float measuredSum = 0.0f;
            for (std::size_t lossCount = 1; lossCount <= losses_.size() && !matched; ++lossCount) {
                measuredSum += losses_[lossCount - 1].damage;
                if (!WithinWindow(losses_.front().tick, losses_[lossCount - 1].tick)) break;
                for (std::size_t firstReport = 0; firstReport < reports_.size() && !matched; ++firstReport) {
                    float reportedSum = 0.0f;
                    for (std::size_t reportCount = 1; firstReport + reportCount <= reports_.size(); ++reportCount) {
                        const auto& report = reports_[firstReport + reportCount - 1];
                        if (!WithinWindow(report.tick, losses_.front().tick)
                            || !WithinWindow(report.tick, losses_[lossCount - 1].tick)) break;
                        reportedSum += report.damage;
                        if (std::fabs(measuredSum - reportedSum) <= 0.001f) {
                            float emitted = 0.0f;
                            for (std::size_t index = 0; index < reportCount; ++index) {
                                // Preserve the exact measured total despite float rounding.
                                const float amount = index + 1 == reportCount
                                    ? measuredSum - emitted : reports_[firstReport + index].damage;
                                if (amount > 0.0001f) result.push_back(amount);
                                emitted += amount;
                            }
                            losses_.erase(losses_.begin(), losses_.begin() + lossCount);
                            reports_.erase(reports_.begin() + firstReport,
                                           reports_.begin() + firstReport + reportCount);
                            matched = true;
                            break;
                        }
                        if (reportedSum > measuredSum + 0.001f) break;
                    }
                }
            }
            if (matched) continue;
            if (!finishLife && now - losses_.front().tick < kMatchWindowMs) break;
            const auto proof = std::find_if(reports_.begin(), reports_.end(), [this](const Entry& report) {
                return WithinWindow(report.tick, losses_.front().tick);
            });
            if (proof != reports_.end()) {
                // Server rules/overkill may make raw weapon damage disagree
                // with HP actually lost. The RPC confirms combat, but cannot
                // justify inventing per-shot values: report the measured sum.
                const unsigned long firstTick = losses_.front().tick;
                const unsigned long proofTick = proof->tick;
                float measured = 0.0f;
                std::size_t count = 0;
                while (count < losses_.size() && WithinWindow(firstTick, losses_[count].tick)
                       && WithinWindow(proofTick, losses_[count].tick)) {
                    measured += losses_[count++].damage;
                }
                result.push_back(measured);
                losses_.erase(losses_.begin(), losses_.begin() + count);
                reports_.erase(std::remove_if(reports_.begin(), reports_.end(), [firstTick](const Entry& report) {
                    return WithinWindow(firstTick, report.tick);
                }), reports_.end());
                continue;
            }
            if (losses_.front().allowFallback) result.push_back(losses_.front().damage);
            losses_.erase(losses_.begin());
        }
        reports_.erase(std::remove_if(reports_.begin(), reports_.end(), [now](const Entry& report) {
            return now - report.tick > kMatchWindowMs;
        }), reports_.end());
        return result;
    }

    void Reset() { losses_.clear(); reports_.clear(); }

private:
    struct Entry {
        float damage;
        unsigned long tick;
        bool allowFallback;
    };
    static bool WithinWindow(unsigned long first, unsigned long second) {
        return (std::min)(first - second, second - first) <= kMatchWindowMs;
    }
    std::vector<Entry> reports_;
    std::vector<Entry> losses_;
};

}  // namespace keybinder
