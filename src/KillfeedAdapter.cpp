#include "../include/KillfeedAdapter.h"

#ifndef KEYBINDER_TESTING
#include "SAMP-API/include/sampapi/0.3.DL-1/CDeathWindow.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CNetGame.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CPlayerPool.h"
#include "SAMP-API/include/sampapi/sampapi.h"
#endif

#include "../include/Logger.h"

#include <cstring>
#ifndef KEYBINDER_TESTING
#include <windows.h>
#endif

#ifndef KEYBINDER_TESTING
// SEH-guarded probes for reading the SA-MP death window. These free functions
// use __try/__except to catch access violations from invalid pointers returned
// by RefDeathWindow(). They must be free functions (no C++ objects with
// destructors) because MSVC forbids __try in functions requiring unwinding.

// Returns 0=AV (invalid pointer), 1=pointer OK but disabled, 2=enabled.
static int ProbeDeathWindowEnabled(sampapi::v03dl::CDeathWindow* dw) {
    int result = 0;
    __try {
        result = dw->m_bEnabled ? 2 : 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
    }
    return result;
}

struct RawDeathWindowEntry {
    char killer[25];
    char victim[25];
    char weapon;
};

// Returns true if entry was read successfully without AV.
static bool ProbeDeathWindowEntry(
    sampapi::v03dl::CDeathWindow* dw,
    int index,
    RawDeathWindowEntry& out)
{
    bool ok = false;
    __try {
        const auto& entry = dw->m_entry[index];
        memcpy(out.killer, entry.m_szKiller, 25);
        memcpy(out.victim, entry.m_szVictim, 25);
        out.weapon = entry.m_nWeapon;
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    return ok;
}
#endif

namespace killtracker {

KillfeedAdapter::KillfeedAdapter()
    : initialized_(false), pollFrameCounter_(0), localPlayerRefreshCounter_(0) {
    lastKnownState_.reserve(5);
}

KillfeedAdapter::~KillfeedAdapter() = default;

std::vector<KillEvent> KillfeedAdapter::PollForNewKills() {
    std::vector<KillEvent> newKills;

    pollFrameCounter_++;
    if (pollFrameCounter_ < kPollIntervalFrames) {
        return newKills;
    }
    pollFrameCounter_ = 0;

    auto* deathWindow = GetDeathWindow();
    if (!deathWindow) {
        return ProcessSnapshot({}, false);
    }

    // If the death window pointer previously caused an access violation,
    // stop polling entirely. The kill tracker still works via RPC adapters.
    if (deathWindowFaulted_) {
        return ProcessSnapshot({}, false);
    }

    if (!RefreshLocalPlayerName()) {
        return ProcessSnapshot({}, false);
    }

    bool snapshotAvailable = false;
    std::vector<DeathWindowEntry> currentState = ReadDeathWindow(deathWindow, &snapshotAvailable);
    static int logCounter = 0;
    logCounter++;
    if (!currentState.empty() || logCounter >= 60) {
        if (!currentState.empty()) {
            keybinder::Logger::Debug("[KillfeedAdapter] Read %zu entries from death window", currentState.size());
        }
        logCounter = 0;
    }

    return ProcessSnapshot(std::move(currentState), snapshotAvailable);
}

std::vector<KillEvent> KillfeedAdapter::ProcessSnapshot(std::vector<DeathWindowEntry> currentState,
                                                      bool snapshotAvailable) {
    std::vector<KillEvent> newKills;
    if (!snapshotAvailable) {
        // A disabled, missing or faulted window is unknown, rather than a
        // confirmed empty feed. Seed again once a readable feed reappears.
        lastKnownState_.clear();
        initialized_ = false;
        return newKills;
    }
    if (!initialized_) {
        // The first readable window may already contain earlier kills. Seed the
        // baseline without replaying history when the tracker is enabled.
        lastKnownState_ = std::move(currentState);
        initialized_ = true;
        return newKills;
    }

    newKills.reserve(currentState.size());

    for (size_t i = 0; i < currentState.size(); ++i) {
        const auto& entry = currentState[i];
        if (!entry.valid) {
            keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: invalid", i);
            continue;
        }

        keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: killer='%s' victim='%s' weapon=%d", 
            i, entry.killer.c_str(), entry.victim.c_str(), entry.weaponId);
        
        if (IsDuplicateEntry(entry)) {
            keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: DUPLICATE (killer='%s' victim='%s')", 
                i, entry.killer.c_str(), entry.victim.c_str());
            continue;
        }

        if (!IsLocalPlayerKill(entry)) {
            keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: NOT local player kill (killer='%s' != local='%s')", 
                i, entry.killer.c_str(), localPlayerNameCache_.c_str());
            continue;
        }

        KillEvent event = ConvertToKillEvent(entry);
        
        if (event.victim.empty()) {
            keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: REJECTED - empty victim", i);
            continue;
        }
        
        if (event.isSuicide) {
            keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: REJECTED - suicide", i);
            continue;
        }

        keybinder::Logger::Debug("[KillfeedAdapter] Entry %zu: ACCEPTED - You killed %s with weapon %d", 
            i, event.victim.c_str(), event.weaponId);
        newKills.push_back(event);
    }

    lastKnownState_ = std::move(currentState);
    initialized_ = true;
    return newKills;
}

void* KillfeedAdapter::GetDeathWindow() const {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    return sampapi::v03dl::RefDeathWindow();
#endif
}

bool KillfeedAdapter::IsDeathWindowAvailable() const {
    return GetDeathWindow() != nullptr;
}

std::vector<DeathWindowEntry> KillfeedAdapter::GetCurrentDeathWindowState() const {
    auto* dw = GetDeathWindow();
    if (!dw) return {};
    return ReadDeathWindow(dw);
}

void KillfeedAdapter::ClearState() {
    lastKnownState_.clear();
    initialized_ = false;
    pollFrameCounter_ = 0;
    localPlayerRefreshCounter_ = 0;
    localPlayerNameCache_.clear();
    deathWindowFaulted_ = false;
    keybinder::Logger::Debug("[KillfeedAdapter] State cleared");
}

KillEvent KillfeedAdapter::ConvertToKillEvent(const DeathWindowEntry& entry) {
    KillEvent event;
    event.killer = entry.killer;
    event.victim = entry.victim;
    event.weaponId = entry.weaponId;
    event.timestamp = std::time(nullptr);
    event.isLocalPlayer = true;
    event.isSuicide = (entry.killer == entry.victim);
    event.isSystem = entry.killer.empty();
    return event;
}

bool KillfeedAdapter::IsLocalPlayerKill(const DeathWindowEntry& entry) const {
#ifdef KEYBINDER_TESTING
    return true;
#else
    return !localPlayerNameCache_.empty() && entry.killer == localPlayerNameCache_;
#endif
}

bool KillfeedAdapter::RefreshLocalPlayerName() {
#ifdef KEYBINDER_TESTING
    return true;
#else
    localPlayerRefreshCounter_++;
    if (!localPlayerNameCache_.empty() && localPlayerRefreshCounter_ < kLocalPlayerRefreshPolls) {
        return true;
    }

    auto* netGame = sampapi::v03dl::RefNetGame();
    if (!netGame) {
        localPlayerNameCache_.clear();
        localPlayerRefreshCounter_ = 0;
        return false;
    }

    auto* playerPool = netGame->GetPlayerPool();
    if (!playerPool) {
        localPlayerNameCache_.clear();
        localPlayerRefreshCounter_ = 0;
        return false;
    }

    const char* localName = playerPool->GetLocalPlayerName();
    if (!localName || localName[0] == '\0') {
        localPlayerNameCache_.clear();
        localPlayerRefreshCounter_ = 0;
        return false;
    }

    localPlayerNameCache_ = localName;
    localPlayerRefreshCounter_ = 0;
    return true;
#endif
}

bool KillfeedAdapter::IsDuplicateEntry(const DeathWindowEntry& entry) const {
    if (!initialized_) {
        return false;
    }
    
    for (const auto& known : lastKnownState_) {
        if (known.killer == entry.killer &&
            known.victim == entry.victim &&
            known.weaponId == entry.weaponId) {
            return true;
        }
    }
    return false;
}

std::vector<DeathWindowEntry> KillfeedAdapter::ReadDeathWindow(void* deathWindow, bool* snapshotAvailable) const {
    std::vector<DeathWindowEntry> entries;
    if (snapshotAvailable) *snapshotAvailable = false;

#ifndef KEYBINDER_TESTING
    if (!deathWindow) {
        return entries;
    }

    auto* dw = static_cast<sampapi::v03dl::CDeathWindow*>(deathWindow);

    // SEH-guarded probe: RefDeathWindow() can return a non-null stale/garbage
    // pointer if the SA-MP death window hasn't been fully initialized or another
    // ASI corrupts the global. On first fault we set deathWindowFaulted_=true
    // and subsequent PollForNewKills calls skip the killfeed entirely.
    // RPC-based kill tracking continues to work regardless.
    int probeResult = ProbeDeathWindowEnabled(dw);
    if (probeResult == 0) {
        keybinder::Logger::Warning("[KillfeedAdapter] Access violation probing death window at %p - disabling killfeed polling", deathWindow);
        const_cast<KillfeedAdapter*>(this)->deathWindowFaulted_ = true;
        return entries;
    }
    if (probeResult == 1) {
        keybinder::Logger::Debug("[KillfeedAdapter] Death window is disabled");
        return entries;
    }

    entries.reserve(sampapi::v03dl::CDeathWindow::MAX_DEATHMESSAGES);

    for (int i = 0; i < sampapi::v03dl::CDeathWindow::MAX_DEATHMESSAGES; ++i) {
        RawDeathWindowEntry raw;
        if (!ProbeDeathWindowEntry(dw, i, raw)) {
            keybinder::Logger::Warning("[KillfeedAdapter] Access violation reading death window entry %d at %p - disabling killfeed polling", i, deathWindow);
            const_cast<KillfeedAdapter*>(this)->deathWindowFaulted_ = true;
            entries.clear();
            return entries;
        }
        
        if (raw.victim[0] == '\0') continue;
        
        int weaponId = static_cast<int>(raw.weapon);
        if (weaponId < 0 || weaponId > 50) continue;
        
        bool validVictim = true;
        for (int j = 0; j < 25 && raw.victim[j] != '\0'; ++j) {
            if (raw.victim[j] < 32 || raw.victim[j] > 126) {
                validVictim = false;
                break;
            }
        }
        if (!validVictim) continue;

        bool validKiller = true;
        for (int j = 0; j < 25 && raw.killer[j] != '\0'; ++j) {
            if (raw.killer[j] < 32 || raw.killer[j] > 126) {
                validKiller = false;
                break;
            }
        }

        bool looksLikePlayerName = false;
        int victimLen = strnlen(raw.victim, 25);
        for (int j = 0; j < victimLen; ++j) {
            if ((raw.victim[j] >= 'A' && raw.victim[j] <= 'Z') ||
                (raw.victim[j] >= 'a' && raw.victim[j] <= 'z')) {
                looksLikePlayerName = true;
                break;
            }
        }
        if (!looksLikePlayerName || victimLen < 2 || victimLen > 20) continue;

        DeathWindowEntry dwEntry;
        if (validKiller) {
            dwEntry.killer = std::string(raw.killer, strnlen(raw.killer, 25));
        } else {
            dwEntry.killer = "";
        }
        dwEntry.victim = std::string(raw.victim, victimLen);
        dwEntry.weaponId = weaponId;
        dwEntry.valid = true;
        entries.push_back(std::move(dwEntry));
    }
    if (snapshotAvailable) *snapshotAvailable = true;
#endif

    return entries;
}

} // namespace killtracker
