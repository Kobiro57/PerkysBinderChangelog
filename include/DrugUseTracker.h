#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace keybinder {

struct DrugTimerDisplay {
    std::string displayName;
    int cooldownSeconds = 0;
    int aftereffectSeconds = 0;
    int cooldownDuration = 0;
    int aftereffectDuration = 0;
};

// The server supplies consumption events; deadlines use a monotonic clock,
// independently of render frequency. Only the caller's own actor is accepted.
class DrugUseTracker {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    bool ObserveChat(const std::string& line, const std::string& localName, TimePoint now) {
        if (localName.empty()) return false;
        const std::string text = NormalizeLine(line);
        const std::string prefix = localName + " nimmt ";
        const std::string suffix = " zu sich.";
        if (text.size() <= prefix.size() + suffix.size()
            || text.compare(0, prefix.size(), prefix) != 0
            || text.compare(text.size() - suffix.size(), suffix.size(), suffix) != 0) return false;

        std::string drug = text.substr(prefix.size(), text.size() - prefix.size() - suffix.size());
        for (char& ch : drug) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        if (drug == "ectasy") drug = "ecstasy";
        const auto& definitions = Definitions();
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            const auto& definition = definitions[index];
            if (drug != definition.key) continue;
            auto& use = uses_[index];
            // Some chat hooks can observe the same server action twice. Later
            // legitimate consumption restarts only the matching drug's clocks.
            if (use.observed && now >= use.started && now - use.started < std::chrono::seconds(1)) return false;
            use.observed = true;
            use.started = now;
            use.cooldownDeadline = now + std::chrono::seconds(definition.cooldown);
            use.aftereffectDeadline = now + std::chrono::seconds(definition.aftereffect);
            use.readyPending = definition.cooldown > 0;
            return true;
        }
        return false;
    }

    std::vector<DrugTimerDisplay> Snapshot(TimePoint now) const {
        std::vector<DrugTimerDisplay> result;
        const auto& definitions = Definitions();
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            const auto& use = uses_[index];
            if (!use.observed) continue;
            const auto& definition = definitions[index];
            const int cooldown = Remaining(use.cooldownDeadline, now, definition.cooldown);
            const int aftereffect = Remaining(use.aftereffectDeadline, now, definition.aftereffect);
            if (cooldown == 0 && aftereffect == 0) continue;
            result.push_back({definition.name, cooldown, aftereffect, definition.cooldown, definition.aftereffect});
        }
        return result;
    }

    // A snapshot never consumes these notifications, including when both
    // timers have expired between two rendered frames.
    std::vector<std::string> TakeReadyDrugs(TimePoint now) {
        std::vector<std::string> result;
        const auto& definitions = Definitions();
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            auto& use = uses_[index];
            if (use.observed && use.readyPending && now >= use.cooldownDeadline) {
                use.readyPending = false;
                result.emplace_back(definitions[index].name);
            }
        }
        return result;
    }

    void Reset() { uses_ = {}; }

private:
    struct Definition {
        const char* key;
        const char* name;
        int cooldown;
        int aftereffect;
    };
    struct ActiveUse {
        bool observed = false;
        bool readyPending = false;
        TimePoint started{};
        TimePoint cooldownDeadline{};
        TimePoint aftereffectDeadline{};
    };

    static const std::array<Definition, 7>& Definitions() {
        static const std::array<Definition, 7> definitions{{
            {"kokain", "Kokain", 120, 120},
            {"heroin", "Heroin", 120, 60},
            {"speed", "Speed", 180, 180},
            {"ecstasy", "Ecstasy", 180, 60},
            {"cannabis", "Cannabis", 0, 40},
            {"adrenalin", "Adrenalin", 45, 0},
            {"morphium", "Morphium", 45, 45},
        }};
        return definitions;
    }

    static int Remaining(TimePoint deadline, TimePoint now, int duration) {
        if (duration <= 0 || now >= deadline) return 0;
        // Clamp to the configured duration if a malformed test/input clock
        // predates consumption. The production clock cannot go backwards.
        const double seconds = std::chrono::duration<double>(deadline - now).count();
        return seconds >= duration ? duration : static_cast<int>(std::ceil(seconds));
    }

    static bool IsHex(char value) {
        return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
    }

    static std::string NormalizeLine(const std::string& line) {
        std::string result;
        result.reserve(line.size());
        for (std::size_t offset = 0; offset < line.size();) {
            if (line[offset] == '{') {
                const auto close = line.find('}', offset + 1);
                if (close != std::string::npos && (close - offset == 7 || close - offset == 9)) {
                    bool valid = true;
                    for (std::size_t digit = offset + 1; digit < close; ++digit) valid = valid && IsHex(line[digit]);
                    if (valid) { offset = close + 1; continue; }
                }
            }
            result.push_back(line[offset++]);
        }
        const auto first = result.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        result = result.substr(first, result.find_last_not_of(" \t\r\n") - first + 1);
        if (result.size() >= 11 && result[0] == '[' && result[3] == ':' && result[6] == ':'
            && result[9] == ']' && result[10] == ' ') {
            bool digits = true;
            for (const int index : {1, 2, 4, 5, 7, 8}) digits = digits && result[index] >= '0' && result[index] <= '9';
            if (digits) result.erase(0, 11);
        }
        return result;
    }

    std::array<ActiveUse, 7> uses_{};
};

} // namespace keybinder
