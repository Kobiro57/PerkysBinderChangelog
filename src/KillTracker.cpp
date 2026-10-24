#include "../include/KillTracker.h"

namespace killtracker {

KillTracker::KillTracker()
    : enabled_(true)
    , kills_(0)
    , deaths_(0)
    , sessionKills_(0)
    , sessionDeaths_(0) {
}

KillTracker::~KillTracker() = default;

bool KillTracker::ProcessKillEvent(const KillEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (event.isSystem || event.victim.empty()) {
        return false;
    }

    if (event.isSuicide) {
        return false;
    }

    if (IsDuplicateKill(event)) {
        return false;
    }

    if (!event.isLocalPlayer) {
        return false;
    }

    KillEvent killWithDamage = event;

    const std::time_t now = std::time(nullptr);
    {
        std::unique_lock<std::shared_mutex> lock(victimDamageMutex_);
        CleanupStaleVictimDamageLocked(now);

        auto damageIt = victimDamage_.find(event.victim);
        if (damageIt != victimDamage_.end()) {
            killWithDamage.damageDealt = damageIt->second.totalDamage;
            victimDamage_.erase(damageIt);
        }
    }

    recentKills_.push_back(killWithDamage);
    TrimRecentKills();

    IncrementWeaponKill(event.weaponId);
    sessionKills_++;

    return true;
}

void KillTracker::ProcessDamageEvent(const DamageEvent& event) {
    if (!enabled_) {
        return;
    }

    if (event.damage <= 0.0f) {
        return;
    }

    AddWeaponDamage(event.weaponId, event.damage);
}

const WeaponStats* KillTracker::GetWeaponStats(int weaponId) const {
    auto it = weaponStats_.find(weaponId);
    if (it != weaponStats_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<WeaponStats> KillTracker::GetAllWeaponStats() const {
    std::vector<WeaponStats> result;
    result.reserve(weaponStats_.size());
    for (const auto& pair : weaponStats_) {
        result.push_back(pair.second);
    }
    return result;
}

float KillTracker::GetTotalDamage() const {
    float total = 0.0f;
    for (const auto& pair : weaponStats_) {
        total += pair.second.totalDamage;
    }
    return total;
}

int KillTracker::GetTotalKills() const {
    return kills_;
}

void KillTracker::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

bool KillTracker::IsEnabled() const {
    return enabled_;
}

bool KillTracker::IsDuplicateKill(const KillEvent& event) const {
    for (const auto& kill : recentKills_) {
        if (kill.victim == event.victim &&
            kill.weaponId == event.weaponId &&
            std::difftime(event.timestamp, kill.timestamp) < DUPLICATE_TIME_WINDOW_SECONDS) {
            return true;
        }
    }
    return false;
}

void KillTracker::RecordDeath(const std::string& killer, int weaponId) {
    if (!enabled_) return;

    deaths_++;
    sessionDeaths_++;

    KillEvent deathEvent;
    deathEvent.killer = killer;
    deathEvent.victim = "";  // local player
    deathEvent.weaponId = weaponId;
    deathEvent.timestamp = std::time(nullptr);
    deathEvent.isLocalPlayer = true;
    deathEvent.isSuicide = killer.empty();
    deathEvent.isSystem = false;
    deathEvent.damageDealt = 0.0f;

    recentDeaths_.push_back(deathEvent);
    TrimRecentDeaths();
}

int KillTracker::GetTotalDeaths() const {
    return deaths_;
}

void KillTracker::ImportRecentDeaths(const std::vector<KillEvent>& deaths) {
    recentDeaths_ = deaths;
    TrimRecentDeaths();
}

std::vector<KillEvent> KillTracker::ExportRecentDeaths() const {
    return recentDeaths_;
}

void KillTracker::TrimRecentDeaths() {
    if (recentDeaths_.size() > MAX_RECENT_DEATHS) {
        recentDeaths_.erase(recentDeaths_.begin(),
            recentDeaths_.begin() + (recentDeaths_.size() - MAX_RECENT_DEATHS));
    }
}

void KillTracker::ResetStats() {
    weaponStats_.clear();
    recentKills_.clear();
    recentDeaths_.clear();
    lastDeathWindowState_.clear();
    kills_ = 0;
    deaths_ = 0;
    sessionKills_ = 0;
    sessionDeaths_ = 0;
    {
        std::unique_lock<std::shared_mutex> lock(victimDamageMutex_);
        victimDamage_.clear();
    }
    lastVictimDamageCleanup_ = 0;
}

void KillTracker::RecordDamageToVictim(const std::string& victim, int weaponId, float damage) {
    if (!enabled_ || victim.empty() || damage <= 0.0f) {
        return;
    }

    {
        std::unique_lock<std::shared_mutex> lock(victimDamageMutex_);
        auto& entry = victimDamage_[victim];
        entry.totalDamage += damage;
        entry.lastUpdate = std::time(nullptr);
    }
}

#ifdef KEYBINDER_TESTING
void KillTracker::MarkVictimDamageStaleForTesting(const std::string& victim) {
    std::unique_lock<std::shared_mutex> lock(victimDamageMutex_);
    auto it = victimDamage_.find(victim);
    if (it != victimDamage_.end()) {
        it->second.lastUpdate = std::time(nullptr) - VICTIM_DAMAGE_TTL_SECONDS - 1;
    }
    lastVictimDamageCleanup_ = 0;
}
#endif

void KillTracker::CleanupStaleVictimDamage() {
    std::unique_lock<std::shared_mutex> lock(victimDamageMutex_);
    CleanupStaleVictimDamageLocked(std::time(nullptr));
}

void KillTracker::CleanupStaleVictimDamageLocked(std::time_t now) {
    if (lastVictimDamageCleanup_ != 0 &&
        std::difftime(now, lastVictimDamageCleanup_) < VICTIM_DAMAGE_CLEANUP_INTERVAL_SECONDS) {
        return;
    }

    for (auto it = victimDamage_.begin(); it != victimDamage_.end(); ) {
        if (std::difftime(now, it->second.lastUpdate) > VICTIM_DAMAGE_TTL_SECONDS) {
            it = victimDamage_.erase(it);
        } else {
            ++it;
        }
    }
    lastVictimDamageCleanup_ = now;
}

void KillTracker::ImportWeaponStats(const std::unordered_map<int, WeaponStats>& stats) {
    int importedKills = 0;

    for (const auto& pair : stats) {
        const WeaponStats& loaded = pair.second;
        importedKills += loaded.killCount;
        auto it = weaponStats_.find(loaded.weaponId);
        if (it == weaponStats_.end()) {
            weaponStats_[loaded.weaponId] = loaded;
        } else {
            it->second.totalDamage += loaded.totalDamage;
            it->second.killCount += loaded.killCount;
        }
    }

    kills_ += importedKills;
}

std::unordered_map<int, WeaponStats> KillTracker::ExportWeaponStats() const {
    return weaponStats_;
}

void KillTracker::ImportRecentKills(const std::vector<KillEvent>& kills) {
    recentKills_ = kills;
    TrimRecentKills();
}

std::vector<KillEvent> KillTracker::ExportRecentKills() const {
    return recentKills_;
}

void KillTracker::UpdateDeathWindowState(const std::vector<DeathWindowEntry>& entries) {
    lastDeathWindowState_ = entries;
}

bool KillTracker::IsNewDeathWindowEntry(const DeathWindowEntry& entry) const {
    for (const auto& oldEntry : lastDeathWindowState_) {
        if (oldEntry.killer == entry.killer &&
            oldEntry.victim == entry.victim &&
            oldEntry.weaponId == entry.weaponId) {
            return false;
        }
    }
    return true;
}

void KillTracker::AddWeaponDamage(int weaponId, float damage) {
    auto it = weaponStats_.find(weaponId);
    if (it == weaponStats_.end()) {
        weaponStats_[weaponId] = WeaponStats(weaponId);
        it = weaponStats_.find(weaponId);
    }
    it->second.totalDamage += damage;
}

void KillTracker::IncrementWeaponKill(int weaponId) {
    auto it = weaponStats_.find(weaponId);
    if (it == weaponStats_.end()) {
        weaponStats_[weaponId] = WeaponStats(weaponId);
        it = weaponStats_.find(weaponId);
    }
    kills_++;
    it->second.killCount++;
}

void KillTracker::TrimRecentKills() {
    if (recentKills_.size() > MAX_RECENT_KILLS) {
        recentKills_.erase(recentKills_.begin(), 
            recentKills_.begin() + (recentKills_.size() - MAX_RECENT_KILLS));
    }
}

} // namespace killtracker
