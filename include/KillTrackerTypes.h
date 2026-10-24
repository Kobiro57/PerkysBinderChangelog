#pragma once

#include <string>
#include <cstdint>
#include <ctime>

namespace killtracker {

// Pure data structure for a kill event
struct KillEvent {
    std::string killer;
    std::string victim;
    int weaponId;
    std::time_t timestamp;
    bool isLocalPlayer;
    bool isSuicide;
    bool isSystem;
    float damageDealt;  // Total damage dealt to victim in this kill
    bool isServerStatsOnly = false;

    KillEvent()
        : weaponId(-1)
        , timestamp(0)
        , isLocalPlayer(false)
        , isSuicide(false)
        , isSystem(false)
        , damageDealt(0.0f) {}
};

// Pure data structure for a damage event
struct DamageEvent {
    int targetId;
    float damage;
    int weaponId;
    int bodyPart;
    std::time_t timestamp;
    bool isOutgoing;

    DamageEvent()
        : targetId(-1)
        , damage(0.0f)
        , weaponId(-1)
        , bodyPart(0)
        , timestamp(0)
        , isOutgoing(true) {}
};

// Per-weapon lifetime statistics
struct WeaponStats {
    int weaponId;
    float totalDamage;
    int killCount;

    WeaponStats()
        : weaponId(-1)
        , totalDamage(0.0f)
        , killCount(0) {}

    explicit WeaponStats(int id)
        : weaponId(id)
        , totalDamage(0.0f)
        , killCount(0) {}
};

// Death window entry snapshot for deduplication
struct DeathWindowEntry {
    std::string killer;
    std::string victim;
    int weaponId;
    bool valid;

    DeathWindowEntry()
        : weaponId(-1)
        , valid(false) {}
};

} // namespace killtracker
