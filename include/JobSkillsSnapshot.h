#pragma once

#include "JobRegistry.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace jobs {

struct JobSkillsSnapshot {
    std::map<JobId, int> points;
    std::size_t totalRows = 0;
    std::size_t unknownRows = 0;
};

namespace jobskills_detail {
inline std::string Trim(std::string_view value) {
    std::size_t begin = 0, end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return std::string(value.substr(begin, end - begin));
}

inline std::string WithoutColors(const std::string& value) {
    std::string result;
    for (std::size_t at = 0; at < value.size();) {
        if (value[at] == '{') {
            const auto end = value.find('}', at + 1);
            if (end != std::string::npos && (end - at == 7 || end - at == 9)
                && std::all_of(value.begin() + at + 1, value.begin() + end, [](unsigned char ch) { return std::isxdigit(ch) != 0; })) {
                at = end + 1;
                continue;
            }
        }
        result += value[at++];
    }
    return Trim(result);
}

inline std::string Fold(std::string value) {
    value = WithoutColors(value);
    struct Replacement { const char* source; const char* target; };
    constexpr Replacement replacements[] = {
        {"\xC3\xA4", "ae"}, {"\xC3\x84", "ae"},
        {"\xC3\xB6", "oe"}, {"\xC3\x96", "oe"},
        {"\xC3\xBC", "ue"}, {"\xC3\x9C", "ue"}, {"\xC3\x9F", "ss"},
    };
    for (const auto& replacement : replacements) {
        std::size_t at = 0;
        while ((at = value.find(replacement.source, at)) != std::string::npos) {
            value.replace(at, 2, replacement.target);
            at += std::char_traits<char>::length(replacement.target);
        }
    }
    std::string result;
    for (unsigned char ch : value)
        if (ch < 128 && std::isalnum(ch)) result += static_cast<char>(std::tolower(ch));
    return result;
}

inline bool ParsePoints(std::string value, int& result) {
    value = Trim(value);
    if (value.empty()) return false;
    const bool grouped = value.find('.') != std::string::npos;
    std::size_t groupStart = 0;
    int parsed = 0;
    for (std::size_t at = 0; at <= value.size(); ++at) {
        if (at == value.size() || value[at] == '.') {
            const std::size_t count = at - groupStart;
            if (count == 0 || (grouped && (groupStart == 0 ? count > 3 : count != 3))) return false;
            groupStart = at + 1;
            continue;
        }
        const unsigned char ch = static_cast<unsigned char>(value[at]);
        if (ch < '0' || ch > '9') return false;
        const int digit = ch - '0';
        if (parsed > ((std::numeric_limits<int>::max)() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    result = parsed;
    return true;
}
} // namespace jobskills_detail

// The caller supplies normalized UTF-8 and proves an outstanding own
// /jobskills request. Never infer job IDs from screen positions or add XP.
// Unknown activities can exist on the server; known registry rows alone form
// the absolute snapshot. A malformed known row invalidates the entire import.
inline bool ParseJobSkillsDialog(const std::string& caption, const std::string& body,
                                const JobRegistry::Snapshot& registry, bool trustedOwnRequest,
                                JobSkillsSnapshot& output, std::string& error) {
    using namespace jobskills_detail;
    error.clear();
    if (!trustedOwnRequest) { error = "Keine eigene Jobskill-Abfrage aktiv."; return false; }
    if (caption.size() > 512 || body.size() > 65536 || body.find('\0') != std::string::npos
        || Fold(caption) != "jobsskilluebersicht") {
        error = "Kein passender Jobskill-Dialog.";
        return false;
    }
    std::map<std::string, JobId> names;
    for (const auto& job : registry.jobs) {
        if (job.id.empty()) continue;
        for (const auto& alias : {job.id, job.name}) {
            const auto key = Fold(alias);
            if (key.empty()) continue;
            const auto existing = names.find(key);
            if (existing != names.end() && existing->second != job.id) {
                error = "Mehrdeutige Jobnamen im Verzeichnis.";
                return false;
            }
            names[key] = job.id;
        }
    }
    JobSkillsSnapshot candidate;
    bool header = false;
    std::size_t begin = 0, rowCount = 0;
    while (begin <= body.size()) {
        if (++rowCount > 128) { error = "Zu viele Zeilen in der Jobskill-Antwort."; return false; }
        const auto end = body.find('\n', begin);
        const std::string row = WithoutColors(body.substr(begin, end == std::string::npos ? end : end - begin));
        begin = end == std::string::npos ? body.size() + 1 : end + 1;
        if (row.empty()) continue;
        const auto tab = row.find('\t');
        if (tab == std::string::npos || row.find('\t', tab + 1) != std::string::npos) {
            error = "Ungültige Jobskill-Tabellenzeile.";
            return false;
        }
        const auto name = Fold(row.substr(0, tab));
        const auto value = row.substr(tab + 1);
        if (!header) {
            if (name != "taetigkeit" || Fold(value) != "stufe") {
                error = "Die Jobskill-Tabellenüberschrift fehlt.";
                return false;
            }
            header = true;
            continue;
        }
        int points = 0;
        if (name.empty() || !ParsePoints(value, points)) {
            error = "Ungültiger Jobskill-Punktestand.";
            return false;
        }
        ++candidate.totalRows;
        const auto job = names.find(name);
        if (job == names.end()) { ++candidate.unknownRows; continue; }
        if (!candidate.points.emplace(job->second, points).second) {
            error = "Doppelter Jobskill-Eintrag.";
            return false;
        }
    }
    if (!header || candidate.points.empty()) {
        error = "Keine bekannten Jobskills empfangen.";
        return false;
    }
    output = std::move(candidate);
    return true;
}

} // namespace jobs
