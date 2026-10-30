#include "../include/CombatChatParser.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <windows.h>

namespace killtracker {
namespace {
std::string Normalize(std::string value) {
    if (value.empty() || value.size() > 4096) return {};
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0)) {
        const int count = MultiByteToWideChar(1252, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring wide(count, L'\0');
        MultiByteToWideChar(1252, 0, value.data(), static_cast<int>(value.size()), wide.data(), count);
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
        value.assign(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, value.data(), bytes, nullptr, nullptr);
    }
    const std::pair<const char*, const char*> replacements[] = {
        {"\xc3\xa4", "ae"}, {"\xc3\xb6", "oe"}, {"\xc3\xbc", "ue"},
        {"\xc3\x84", "Ae"}, {"\xc3\x96", "Oe"}, {"\xc3\x9c", "Ue"}, {"\xc3\x9f", "ss"}
    };
    for (const auto& pair : replacements) {
        std::size_t at = 0;
        while ((at = value.find(pair.first, at)) != std::string::npos) {
            value.replace(at, 2, pair.second); at += std::char_traits<char>::length(pair.second);
        }
    }
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '{') {
            auto end = value.find('}', i + 1);
            if (end != std::string::npos && (end - i == 7 || end - i == 9)
                && std::all_of(value.begin() + i + 1, value.begin() + end, [](unsigned char ch) { return std::isxdigit(ch) != 0; })) {
                i = end; continue;
            }
        }
        if (std::isspace(static_cast<unsigned char>(value[i]))) {
            if (!result.empty() && result.back() != ' ') result += ' ';
        } else result += value[i];
    }
    if (!result.empty() && result.back() == ' ') result.pop_back();
    static const std::regex timestamp(R"(^\[[0-9]{2}:[0-9]{2}:[0-9]{2}\]\s*)");
    result = std::regex_replace(result, timestamp, "");
    if (result.compare(0, 6, "INFO: ") == 0) result.erase(0, 6);
    return result;
}
bool ValidName(const std::string& name) {
    return !name.empty() && name.size() <= 24 && std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return c > 32 && c < 127 && c != '<' && c != '>' && c != ':' && c != '{' && c != '}';
    });
}
CombatChatNotice Kill(CombatChatFamily family, const std::string& local, std::string victim = {}) {
    CombatChatNotice result;
    result.kind = CombatChatKind::Kill; result.family = family; result.killer = local;
    result.victim = std::move(victim); result.victimKnown = !result.victim.empty();
    return result;
}
}

bool SameCombatPlayerName(const std::string& left, const std::string& right) {
    return !left.empty() && left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

CombatChatNotice ParseCombatChat(const std::string& line, const std::string& localName, bool hasPlayerPrefix) {
    if (hasPlayerPrefix || !ValidName(localName)) return {};
    const auto text = Normalize(line);
    if (text.empty()) return {};
    const auto flags = std::regex::ECMAScript | std::regex::icase;
    static const std::regex murder(R"(^Du hast ein Verbrechen begangen\s*\(\s*Vorsaetzlicher Mord\s*\)\s*\.\s*Reporter:\s*Anonym$)", flags);
    static const std::regex killed(R"(^Du hast einen Spieler getoetet\.$)", flags);
    static const std::regex gangwar(R"(^GANGWAR\s*\([^\r\n)]{1,80}\): Du hast einen Feind ausgeschaltet!$)", flags);
    if (std::regex_match(text, murder)) return Kill(CombatChatFamily::Murder, localName);
    if (std::regex_match(text, killed)) return Kill(CombatChatFamily::PlayerKilled, localName);
    if (std::regex_match(text, gangwar)) return Kill(CombatChatFamily::Gangwar, localName);
    static const std::regex cemetery(R"(^Willkommen auf dem Friedhof\. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit ablaeuft\.$)", flags);
    if (std::regex_match(text, cemetery)) {
        CombatChatNotice result;
        result.kind = CombatChatKind::Death; result.family = CombatChatFamily::Cemetery;
        result.victim = localName; result.victimKnown = true; return result;
    }
    std::smatch match;
    static const std::regex criminal(R"(^<<\s*(\S{1,24}) hat den gesuchten Verbrecher (\S{1,24}) getoetet\.\s*>>$)", flags);
    if (std::regex_match(text, match, criminal) && ValidName(match[1]) && ValidName(match[2])) {
        if (SameCombatPlayerName(match[2], localName)) {
            CombatChatNotice result; result.kind = CombatChatKind::Death; result.family = CombatChatFamily::Criminal;
            result.killer = match[1]; result.victim = localName; result.victimKnown = true; return result;
        }
        if (SameCombatPlayerName(match[1], localName)) return Kill(CombatChatFamily::Criminal, localName, match[2]);
        return {};
    }
    static const std::regex prison(R"(^<<\s*(\S{1,24}) konnte nicht inhaftiert werden, da das Gefaengnis gesprengt wurde\.\s*>>$)", flags);
    if (std::regex_match(text, match, prison) && ValidName(match[1])) {
        CombatChatNotice result; result.kind = CombatChatKind::KillCorroboration; result.family = CombatChatFamily::PrisonDestroyed;
        result.victim = match[1]; result.victimKnown = true; return result;
    }
    static const std::regex territory(R"(^REVIERKAMPF: (\S{1,24}) hat (\S{1,24}) getoetet(?: (\(Teamkill\)))?\.(?: Neuer Spielstand: Angreifer: -?[0-9]+ - Verteidiger: -?[0-9]+)?$)", flags);
    if (std::regex_match(text, match, territory) && ValidName(match[1]) && ValidName(match[2])
        && SameCombatPlayerName(match[1], localName) && !SameCombatPlayerName(match[2], localName))
        return Kill(match[3].matched ? CombatChatFamily::TerritoryTeamkill : CombatChatFamily::Territory, localName, match[2]);
    static const std::regex teamkill(R"(^REVIERKAMPF: (\S{1,24}) hat einen Teamkill begangen\.$)", flags);
    if (std::regex_match(text, match, teamkill) && SameCombatPlayerName(match[1], localName))
        return Kill(CombatChatFamily::TerritoryTeamkillAnonymous, localName);
    static const std::regex streetwar(R"(^STREETWAR: (\S{1,24}) hat (\S{1,24}) \([^\r\n)]{1,80}\) getoetet\. Aktuelle Kills: -?[0-9]+$)", flags);
    if (std::regex_match(text, match, streetwar) && ValidName(match[1]) && ValidName(match[2])
        && SameCombatPlayerName(match[1], localName) && !SameCombatPlayerName(match[2], localName))
        return Kill(CombatChatFamily::Streetwar, localName, match[2]);
    static const std::regex deathzone(R"(^DEATHZONE: (\S{1,24}) hat (\S{1,24}) mit einer .{1,80} getoetet\.$)", flags);
    if (std::regex_match(text, match, deathzone) && ValidName(match[1]) && ValidName(match[2])
        && SameCombatPlayerName(match[1], localName) && !SameCombatPlayerName(match[2], localName))
        return Kill(CombatChatFamily::Deathzone, localName, match[2]);
    return {};
}
} // namespace killtracker
