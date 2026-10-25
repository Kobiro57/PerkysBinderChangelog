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
    static constexpr std::size_t kMaximumDialogLines = 128;
    static constexpr std::size_t kMaximumDialogBytes = 65536;

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
        constexpr const char* numericLabels[] = {"level", "erfahrungspunkte", "xp", "bargeld", "kontostand", "bankguthaben", "bank",
            "vermögen", "vermoegen", "gesamtvermögen", "gesamtvermoegen", "kills", "tode", "gold", "eventcoins", "wantedpunkte",
            "kills/tode", "kills / tode", "nächster zahltag in", "naechster zahltag in"};
        for (const char* label : numericLabels) {
            const std::size_t length = std::char_traits<char>::length(label);
            if (line.compare(0, length, label) != 0 || !FieldBoundary(line, length)) continue;
            std::size_t value = length;
            SkipSpaceAndColon(line, value);
            if (value < line.size() && (line[value] == '[' || line[value] == '(' || line[value] == '+' || line[value] == '-' || line[value] == '$'
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

    // Dialogs carry a complete payload rather than the short legacy chat block.
    // Their table headings and unrelated rows must not spend the chat budget.
    // Trust is still scoped by the caller to an own /stats request, and an
    // explicit foreign identity always rejects the whole payload.
    bool ConsumeDialog(const std::string& caption, const std::string& body, const std::string& ownName,
                       unsigned long now, bool trustedOwnRequest) {
        if (!trustedOwnRequest || Trim(ownName).empty() || body.size() > kMaximumDialogBytes) return false;
        const std::string cleanCaption = Clean(caption);
        bool foreignCaption = false;
        const bool safeCaption = IsStatsCaption(cleanCaption, ownName, foreignCaption);
        if (foreignCaption) return false;
        const std::vector<std::string> lines = SplitLines(body);
        if (lines.size() > kMaximumDialogLines) return false;
        bool ownIdentity = false;
        for (const std::string& raw : lines) {
            const std::string line = Clean(raw);
            if (const auto name = HeaderName(line)) {
                if (!EqualName(*name, ownName)) return false;
                ownIdentity = true;
            }
            for (const char* identity : {"Spielername", "Spieler", "Name"}) {
                if (const auto value = FindField(line, identity)) {
                    if (!EqualName(*value, ownName)) return false;
                    ownIdentity = true;
                }
            }
        }
        if (!safeCaption && !ownIdentity) return false;
        ServerStatsCollector temporary;
        temporary.ConsumeLine("*** " + Trim(ownName) + " ***", ownName, now);
        for (const std::string& raw : lines) {
            const std::string line = Clean(raw);
            if (IsRecognizedStatsLine(line)) temporary.ParseValues(line);
        }
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
    static std::size_t CaptionSeparatorAt(const std::string& value, std::size_t at) {
        if (at >= value.size()) return 0;
        if (value[at] == ':' || value[at] == '|' || value[at] == '-') return 1;
        // Nova formats its own stats caption as "Statistiken » Player_Name".
        // These UTF-8 separators belong to the title, never to the identity.
        for (const char* separator : {"\xC2\xBB", "\xC2\xAB"})
            if (value.compare(at, 2, separator) == 0) return 2;
        return 0;
    }
    static std::string TrimCaptionPart(std::string value) {
        value = Trim(value);
        while (const auto size = CaptionSeparatorAt(value, 0))
            value = Trim(std::string_view(value).substr(size));
        while (!value.empty()) {
            const auto at = value.size() >= 2 && CaptionSeparatorAt(value, value.size() - 2) == 2
                ? value.size() - 2 : value.size() - 1;
            if (CaptionSeparatorAt(value, at) == 0) break;
            value.resize(at);
            value = Trim(value);
        }
        if (value.size() >= 2 && ((value.front() == '(' && value.back() == ')')
            || (value.front() == '[' && value.back() == ']')))
            value = Trim(std::string_view(value).substr(1, value.size() - 2));
        return value;
    }
    static bool IsStatsCaption(const std::string& caption, const std::string& ownName, bool& foreignIdentity) {
        foreignIdentity = false;
        if (const auto name = HeaderName(caption)) {
            foreignIdentity = !EqualName(*name, ownName);
            return !foreignIdentity;
        }
        if (EqualName(caption, ownName)) return true;
        const std::string lower = Lower(caption);
        for (const char* word : {"spielerstatistiken", "spielerstatistik", "statistiken", "statistik", "stats"}) {
            const std::size_t size = std::char_traits<char>::length(word);
            const auto at = lower.find(word);
            if (at == std::string::npos || (at > 0 && std::isalnum(static_cast<unsigned char>(lower[at - 1])))
                || (at + size < lower.size() && std::isalnum(static_cast<unsigned char>(lower[at + size])))) continue;
            const std::string prefix = TrimCaptionPart(caption.substr(0, at));
            std::string suffix = TrimCaptionPart(caption.substr(at + size));
            const std::string lowerPrefix = Lower(prefix);
            bool neutralPrefix = prefix.empty();
            for (const char* neutral : {"deine", "persönliche", "persoenliche", "spieler", "nova legacy", "nova-legacy", "nova legacy roleplay"})
                if (lowerPrefix == neutral) neutralPrefix = true;
            if (!neutralPrefix && !EqualName(prefix, ownName)) { foreignIdentity = true; return false; }
            for (const char* named : {"von ", "für ", "fuer "}) {
                const std::size_t length = std::char_traits<char>::length(named);
                if (Lower(suffix).compare(0, length, named) == 0) { suffix = TrimCaptionPart(suffix.substr(length)); break; }
            }
            if (suffix.empty() || EqualName(suffix, ownName)) return true;
            foreignIdentity = true;
            return false;
        }
        return false;
    }
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
                if (line[value] == '[' || line[value] == '(') {
                    const auto end = line.find(line[value] == '[' ? ']' : ')', value + 1);
                    if (end == std::string::npos) return {};
                    // [TAG]Player is a nickname, [Player] an outer wrapper.
                    if (end + 1 == line.size() || std::isspace(static_cast<unsigned char>(line[end + 1])) || line[end + 1] == '|')
                        return Trim(std::string_view(line).substr(value + 1, end - value - 1));
                }
                std::size_t end = value;
                if (line[end] == '$') {
                    ++end;
                    while (end < line.size() && std::isspace(static_cast<unsigned char>(line[end]))) ++end;
                }
                while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end])) && line[end] != '|') ++end;
                // A tabular XP or Kills/Tode pair may put spaces around '/'.
                if (key == "erfahrungspunkte" || key == "xp" || key == "kills/tode" || key == "kills / tode") {
                    std::size_t slash = end;
                    while (slash < line.size() && line[slash] == ' ') ++slash;
                    if (slash < line.size() && line[slash] == '/') {
                        end = slash + 1;
                        while (end < line.size() && line[end] == ' ') ++end;
                        while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end])) && line[end] != '|') ++end;
                    }
                }
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
        for (const char* label : {"Erfahrungspunkte", "XP"}) if (const auto experience = FindField(line, label)) {
            const auto separator = experience->find('/');
            if (separator != std::string::npos && experience->find('/', separator + 1) == std::string::npos) {
                const auto current = SmallInteger(experience->substr(0, separator));
                const auto next = SmallInteger(experience->substr(separator + 1));
                if (current && next) { snapshot_.experience = current; snapshot_.nextLevelExperience = next; }
            }
        }
        for (const char* label : {"Kills/Tode", "Kills / Tode"}) if (const auto counters = FindField(line, label)) {
            const auto separator = counters->find('/');
            if (separator != std::string::npos && counters->find('/', separator + 1) == std::string::npos) {
                const auto kills = SmallInteger(counters->substr(0, separator));
                const auto deaths = SmallInteger(counters->substr(separator + 1));
                if (kills && deaths) { snapshot_.kills = kills; snapshot_.deaths = deaths; }
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
