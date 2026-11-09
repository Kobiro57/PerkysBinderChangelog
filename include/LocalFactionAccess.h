#pragma once

#include "FactionAccess.h"
#include <optional>
#include <string>

namespace keybinder {

// A current-server own-statistics reading is required. An API entry alone is
// not bound to the active game server, even when the nickname matches.
// Conflicting known roles require a fresh synchronization.
inline std::string ReconcileLocalFaction(const std::string& synchronizedFaction,
                                        const std::optional<std::string>& liveFaction) {
    const auto synchronizedRole = ClassifyFaction(synchronizedFaction);
    if (synchronizedRole == FactionRole::Unknown) return {};
    const auto liveRole = liveFaction ? ClassifyFaction(*liveFaction) : FactionRole::Unknown;
    if (liveRole != FactionRole::Unknown && synchronizedRole != liveRole) return {};
    return synchronizedFaction;
}

} // namespace keybinder
