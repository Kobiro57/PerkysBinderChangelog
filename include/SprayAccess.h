#pragma once

#include "FactionAccess.h"
#include <optional>
#include <string>

namespace keybinder {
inline bool IsSprayCivilianOrUnknown(const std::string& affiliation) {
    const auto name = NormalizeFactionName(affiliation);
    return name.empty() || name == "zivilist" || name == "zivilisten"
        || name == "civilian" || name == "civilians" || name == "keine"
        || name == "keiner" || name == "keine fraktion" || name == "keine gang"
        || name == "none" || name == "unknown" || name == "unbekannt"
        || name == "n/a" || name == "-" || name == "0";
}

// An own-statistics reading binds access to the active server/account. A fresh
// named live affiliation also includes gangs, which the police-only faction
// reconciliation deliberately excludes. A current civilian reading revokes use.
inline bool CanUseSprayTags(const std::string& synchronizedFaction,
    const std::optional<std::string>& liveAffiliation = {}) {
    const auto own = NormalizeFactionName(synchronizedFaction);
    if (own.empty() || own == "unknown" || own == "unbekannt" || own == "n/a") return false;
    if (liveAffiliation && !NormalizeFactionName(*liveAffiliation).empty())
        return !IsSprayCivilianOrUnknown(*liveAffiliation);
    return !IsSprayCivilianOrUnknown(synchronizedFaction);
}
} // namespace keybinder
