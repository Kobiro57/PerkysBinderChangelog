#pragma once

#include <cctype>
#include <string>

namespace keybinder {

enum class FactionRole { Unknown, Other, Police, RedCountyBrotherhood };

inline std::string NormalizeFactionName(const std::string& value) {
    std::string normalized;
    bool pendingSpace = false;
    for (std::size_t at = 0; at < value.size();) {
        if (value[at] == '{') {
            const auto close = value.find('}', at + 1);
            if (close != std::string::npos && (close - at == 7 || close - at == 9)) {
                bool hex = true;
                for (std::size_t digit = at + 1; digit < close; ++digit)
                    hex = hex && std::isxdigit(static_cast<unsigned char>(value[digit]));
                if (hex) { at = close + 1; continue; }
            }
        }
        const unsigned char ch = static_cast<unsigned char>(value[at++]);
        if (std::isspace(ch)) { pendingSpace = !normalized.empty(); continue; }
        if (pendingSpace) normalized.push_back(' ');
        pendingSpace = false;
        normalized.push_back(ch < 128 ? static_cast<char>(std::tolower(ch)) : static_cast<char>(ch));
    }
    return normalized;
}

inline FactionRole ClassifyFaction(const std::string& faction) {
    const std::string name = NormalizeFactionName(faction);
    if (name.empty()) return FactionRole::Unknown;
    if (name == "red county brotherhood" || name == "rcb" || name == "redcounty brotherhood")
        return FactionRole::RedCountyBrotherhood;
    if (name == "lspd" || name == "sapd" || name == "los santos police department"
        || name == "san andreas police department" || name == "police department")
        return FactionRole::Police;
    return FactionRole::Other;
}

inline bool IsRcbMember(const std::string& faction) {
    return ClassifyFaction(faction) == FactionRole::RedCountyBrotherhood;
}

inline bool IsPoliceMember(const std::string& faction) {
    return ClassifyFaction(faction) == FactionRole::Police;
}

} // namespace keybinder
