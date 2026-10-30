#pragma once

#include <cstdint>
#include <string>

namespace killtracker {

enum class CombatChatKind { None, Kill, Death, KillCorroboration };
enum class CombatChatFamily : std::uint32_t {
    Murder = 1, PlayerKilled = 2, Gangwar = 4, Criminal = 8,
    PrisonDestroyed = 16, Territory = 32, TerritoryTeamkill = 64,
    Streetwar = 128, Deathzone = 256, Cemetery = 512, TerritoryTeamkillAnonymous = 1024
};

struct CombatChatNotice {
    CombatChatKind kind = CombatChatKind::None;
    CombatChatFamily family = CombatChatFamily::PlayerKilled;
    std::string killer;
    std::string victim;
    bool victimKnown = false;
};

// Only trusted server lines qualify. Local names are compared as whole names;
// no nickname or regular expression is hard-coded into the patterns.
CombatChatNotice ParseCombatChat(const std::string& line, const std::string& localName,
                                bool hasPlayerPrefix = false);
bool SameCombatPlayerName(const std::string& left, const std::string& right);

} // namespace killtracker
