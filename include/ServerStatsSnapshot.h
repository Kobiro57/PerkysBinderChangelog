#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace keybinder {

struct ServerStatsSnapshot {
    std::string playerName;
    std::string serverAddress;
    int serverPort = 0;
    std::optional<int> kills, deaths, level, gold, eventcoins, wantedPoints;
    std::optional<long long> cash, bank, wealth;
    std::optional<int> experience, nextLevelExperience, nextPaydayMinutes;

    bool HasCounters() const { return kills.has_value() && deaths.has_value(); }
    bool HasCore() const { return HasCounters() && cash.has_value() && bank.has_value(); }
    std::optional<long long> GetAvailableWealth() const {
        if (wealth) return wealth;
        if (!cash || !bank) return {};
        if ((*bank > 0 && *cash > (std::numeric_limits<long long>::max)() - *bank)
            || (*bank < 0 && *cash < (std::numeric_limits<long long>::min)() - *bank)) return {};
        return *cash + *bank;
    }
};

// Collects only the short, explicitly own server-statistics context. It never
// sends a command, touches game memory, or interprets arbitrary player chat.
class ServerStatsCollector {
public:
    static constexpr unsigned long kContextTtlMs = 3000;
    static constexpr int kMaximumBodyLines = 16;

    void Reset() { snapshot_ = {}; ownContext_ = false; startedAt_ = 0; bodyLines_ = 0; }
    const ServerStatsSnapshot& Snapshot() const { return snapshot_; }
    bool IsOwnContext() const { return ownContext_; }

    bool ConsumeLine(const std::string& normalizedUtf8, const std::string& ownName, unsigned long now) {
        const std::string line = Clean(normalizedUtf8);
        const std::string expected = Trim(ownName);
        if (expected.empty()) { Reset(); return false; }
        if (const auto header = HeaderName(line)) {
            if (!EqualName(*header, expected)) { Reset(); return false; }
            Reset();
            snapshot_.playerName = *header;
            ownContext_ = true;
            startedAt_ = now;
            return true;
        }
        if (!ownContext_) return false;
        if (!EqualName(snapshot_.playerName, expected)) { Reset(); return false; }
        if (now - startedAt_ >= kContextTtlMs || bodyLines_ >= kMaximumBodyLines) {
            ownContext_ = false;
            return false;
        }
        ++bodyLines_;
        const bool recognized = IsRecognizedStatsLine(line);
        if (recognized) ParseValues(line);
        if (bodyLines_ >= kMaximumBodyLines) ownContext_ = false;
        return recognized;
    }

    static bool IsRecognizedStatsLine(const std::string& text) {
        const std::string line = Lower(Clean(text));
        if (line.empty()) return true;
        constexpr const char* numericLabels[] = {"level", "erfahrungspunkte", "bargeld", "kontostand", "bankguthaben", "bank",
            "vermögen", "vermoegen", "gesamtvermögen", "gesamtvermoegen", "kills", "tode", "gold", "eventcoins", "wantedpunkte",
            "nächster zahltag in", "naechster zahltag in"};
        for (const char* label : numericLabels) {
            const std::size_t length = std::char_traits<char>::length(label);
            if (line.compare(0, length, label) != 0 || !FieldBoundary(line, length)) continue;
            std::size_t value = length;
            SkipSpaceAndColon(line, value);
            if (value < line.size() && (line[value] == '[' || line[value] == '+' || line[value] == '-'
                || std::isdigit(static_cast<unsigned char>(line[value])))) return true;
        }
        // Remaining server-statistic rows contain descriptive data, not
        // numbers needed by the Binder. Recognize them without inventing values.
        constexpr const char* descriptiveLabels[] = {"fraktion", "gang", "geschlecht", "alter", "offene rechnungen",
            "führerscheine", "fuehrerscheine", "lizenzen", "waffenschein", "waffenscheine", "führerschein", "fuehrerschein"};
        for (const char* label : descriptiveLabels) {
            const std::size_t length = std::char_traits<char>::length(label);
            if (line.compare(0, length, label) == 0 && FieldBoundary(line, length)) return true;
        }
        return false;
    }

    // A requested statistics dialog can legitimately omit the chat header.
    // Accept only neutral statistics captions or an explicitly own identity;
    // reject every foreign name before modifying this collector's state.
    bool ConsumeDialog(const std::string& caption, const std::string& body, const std::string& ownName,
                       unsigned long now, bool trustedOwnRequest) {
        if (!trustedOwnRequest || Trim(ownName).empty()) return false;
        const std::string cleanCaption = Clean(caption);
        const auto captionName = HeaderName(cleanCaption);
        if (captionName && !EqualName(*captionName, ownName)) return false;
        const std::string lowerCaption = Lower(cleanCaption);
        constexpr const char* harmlessCaptions[] = {"stats", "statistik", "statistiken", "deine stats", "deine statistik",
            "deine statistiken", "spielerstatistiken", "spieler-statistiken", "spieler statistik", "spieler-statistik"};
        bool safeCaption = captionName.has_value();
        for (const char* harmless : harmlessCaptions) if (lowerCaption == harmless) safeCaption = true;
        constexpr const char* namedPrefixes[] = {"stats von ", "statistik von ", "statistiken von "};
        for (const char* prefix : namedPrefixes) {
            const std::size_t length = std::char_traits<char>::length(prefix);
            if (lowerCaption.compare(0, length, prefix) == 0) {
                if (!EqualName(cleanCaption.substr(length), ownName)) return false;
                safeCaption = true;
            }
        }
        const std::vector<std::string> lines = SplitLines(body);
        bool ownHeader = false;
        for (const std::string& raw : lines) {
            const std::string line = Clean(raw);
            if (const auto name = HeaderName(line)) {
                if (!EqualName(*name, ownName)) return false;
                ownHeader = true;
            }
            for (const char* identity : {"Spieler", "Name"}) {
                if (const auto value = FindField(line, identity)) {
                    if (!EqualName(*value, ownName)) return false;
                }
            }
        }
        if (!safeCaption && !ownHeader) return false;
        // An unknown caption with an own header is accepted only if it does
        // not claim a different player's statistics in a known named form.
        ServerStatsCollector temporary;
        temporary.ConsumeLine("*** " + Trim(ownName) + " ***", ownName, now);
        for (const std::string& line : lines) temporary.ConsumeLine(line, ownName, now);
        if (!temporary.HasAnyValue()) return false;
        *this = std::move(temporary);
        return true;
    }

private:
    static std::string Trim(std::string_view value) {
        std::size_t first = 0, end = value.size();
        while (first < end && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
        while (end > first && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
        return std::string(value.substr(first, end - first));
    }
    static std::string Lower(std::string value) {
        for (char& ch : value) if (static_cast<unsigned char>(ch) < 128) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }
    static bool EqualName(const std::string& first, const std::string& second) { return Lower(Trim(first)) == Lower(Trim(second)); }
    static std::string Clean(const std::string& text) {
        std::string result;
        for (std::size_t index = 0; index < text.size();) {
            if (text[index] == '{') {
                const auto end = text.find('}', index + 1);
                if (end != std::string::npos && (end - index == 7 || end - index == 9)
                    && std::all_of(text.begin() + index + 1, text.begin() + end, [](unsigned char ch) { return std::isxdigit(ch) != 0; })) {
                    index = end + 1;
                    continue;
                }
            }
            result += text[index++];
        }
        return Trim(result);
    }
    static std::optional<std::string> HeaderName(const std::string& line) {
        if (line.size() < 7 || line.compare(0, 3, "***") != 0 || line.compare(line.size() - 3, 3, "***") != 0) return {};
        const std::string name = Trim(std::string_view(line).substr(3, line.size() - 6));
        if (name.empty() || name.find_first_of("*\r\n") != std::string::npos) return {};
        return name;
    }
    static bool FieldBoundary(const std::string& line, std::size_t at) {
        return at == line.size() || std::isspace(static_cast<unsigned char>(line[at])) || line[at] == ':' || line[at] == '[';
    }
    static void SkipSpaceAndColon(const std::string& line, std::size_t& at) {
        while (at < line.size() && std::isspace(static_cast<unsigned char>(line[at]))) ++at;
        if (at < line.size() && line[at] == ':') ++at;
        while (at < line.size() && std::isspace(static_cast<unsigned char>(line[at]))) ++at;
    }
    static std::optional<std::string> FindField(const std::string& line, const std::string& label) {
        const std::string lower = Lower(line), key = Lower(label);
        std::size_t at = 0;
        while ((at = lower.find(key, at)) != std::string::npos) {
            const bool begins = at == 0 || std::isspace(static_cast<unsigned char>(line[at - 1])) || line[at - 1] == '|';
            std::size_t value = at + key.size();
            if (begins && FieldBoundary(line, value)) {
                SkipSpaceAndColon(line, value);
                if (value >= line.size()) return {};
                if (line[value] == '[') {
                    const auto end = line.find(']', value + 1);
                    if (end == std::string::npos) return {};
                    return Trim(std::string_view(line).substr(value + 1, end - value - 1));
                }
                std::size_t end = value;
                while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end])) && line[end] != '|') ++end;
                return Trim(std::string_view(line).substr(value, end - value));
            }
            at += key.size();
        }
        return {};
    }
    static std::optional<long long> Integer(std::string value, bool signedValue = true) {
        value = Trim(value);
        if (!value.empty() && value.front() == '$') value.erase(value.begin());
        if (!value.empty() && value.back() == '$') value.pop_back();
        value = Trim(value);
        bool negative = false;
        if (!value.empty() && (value.front() == '-' || value.front() == '+')) {
            negative = value.front() == '-';
            value.erase(value.begin());
        }
        if (value.empty() || (negative && !signedValue)) return {};
        std::string digits;
        std::size_t groupStart = 0;
        bool grouped = value.find('.') != std::string::npos;
        for (std::size_t index = 0; index <= value.size(); ++index) {
            if (index == value.size() || value[index] == '.') {
                const std::size_t length = index - groupStart;
                if (length == 0 || (grouped && (groupStart == 0 ? length > 3 : length != 3))) return {};
                groupStart = index + 1;
            } else {
                const unsigned char ch = static_cast<unsigned char>(value[index]);
                if (ch < '0' || ch > '9') return {};
                digits += static_cast<char>(ch);
            }
        }
        const std::uint64_t limit = static_cast<std::uint64_t>((std::numeric_limits<long long>::max)()) + (negative ? 1ULL : 0ULL);
        std::uint64_t result = 0;
        for (char ch : digits) {
            const unsigned digit = static_cast<unsigned>(ch - '0');
            if (result > (limit - digit) / 10) return {};
            result = result * 10 + digit;
        }
        if (!negative) return static_cast<long long>(result);
        if (result == limit) return (std::numeric_limits<long long>::min)();
        return -static_cast<long long>(result);
    }
    static std::optional<int> SmallInteger(const std::string& value) {
        const auto parsed = Integer(value, false);
        if (!parsed || *parsed > (std::numeric_limits<int>::max)()) return {};
        return static_cast<int>(*parsed);
    }
    void ParseValues(const std::string& line) {
        const auto readSmallInteger = [&](const char* label, std::optional<int>& output) {
            if (const auto value = FindField(line, label)) if (const auto parsed = SmallInteger(*value)) output = parsed;
        };
        const auto money = [&](const char* label, std::optional<long long>& output) {
            if (const auto value = FindField(line, label)) if (const auto parsed = Integer(*value)) output = parsed;
        };
        readSmallInteger("Level", snapshot_.level); readSmallInteger("Kills", snapshot_.kills); readSmallInteger("Tode", snapshot_.deaths);
        readSmallInteger("Gold", snapshot_.gold); readSmallInteger("Eventcoins", snapshot_.eventcoins); readSmallInteger("Wantedpunkte", snapshot_.wantedPoints);
        money("Bargeld", snapshot_.cash); money("Kontostand", snapshot_.bank); money("Bankguthaben", snapshot_.bank);
        money("Bank", snapshot_.bank); money("Vermögen", snapshot_.wealth); money("Vermoegen", snapshot_.wealth);
        money("Gesamtvermögen", snapshot_.wealth); money("Gesamtvermoegen", snapshot_.wealth);
        if (const auto experience = FindField(line, "Erfahrungspunkte")) {
            const auto separator = experience->find('/');
            if (separator != std::string::npos && experience->find('/', separator + 1) == std::string::npos) {
                const auto current = SmallInteger(experience->substr(0, separator));
                const auto next = SmallInteger(experience->substr(separator + 1));
                if (current && next) { snapshot_.experience = current; snapshot_.nextLevelExperience = next; }
            }
        }
        for (const char* label : {"Nächster Zahltag in", "Naechster Zahltag in"}) {
            if (auto value = FindField(line, label)) {
                const auto space = value->find(' ');
                if (space != std::string::npos) *value = value->substr(0, space);
                if (const auto parsed = SmallInteger(*value)) snapshot_.nextPaydayMinutes = parsed;
            }
        }
    }
    static std::vector<std::string> SplitLines(const std::string& value) {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start <= value.size()) {
            const auto end = value.find('\n', start);
            lines.push_back(value.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return lines;
    }
    bool HasAnyValue() const {
        return snapshot_.kills || snapshot_.deaths || snapshot_.level || snapshot_.gold || snapshot_.eventcoins || snapshot_.wantedPoints
            || snapshot_.cash || snapshot_.bank || snapshot_.wealth || snapshot_.experience || snapshot_.nextLevelExperience || snapshot_.nextPaydayMinutes;
    }

    ServerStatsSnapshot snapshot_;
    bool ownContext_ = false;
    unsigned long startedAt_ = 0;
    int bodyLines_ = 0;
};

} // namespace keybinder
