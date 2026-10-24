#include "../include/JobParser.h"

#include "../include/Logger.h"

#include <cctype>
#include <ctime>
#include <cstdlib>
#include <utility>

namespace jobs {

namespace {

constexpr const char* kEventTypeManualEarning = "manual_earning";
constexpr const char* kEventTypeEarning = "earning";
constexpr const char* kEventTypeSkillLevelAbsolute = "skill_level_absolute";
constexpr const char* kEventTypeJobMessage = "job_message";
constexpr const char* kEventTypeIgnore = "ignore";
constexpr const char* kSourceChatAuto = "chat_auto";

bool ContainsAnyDigit(const std::string& text) {
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return true;
        }
    }

    return false;
}

} // namespace

JobParser::JobParser() = default;

std::vector<JobEvent> JobParser::Parse(const JobChatEvent& event, const JobRegistry::Snapshot& snapshot) {
    std::vector<JobEvent> parsedEvents;

    const JobDefinition* definition = FindJobDefinition(event, snapshot);
    if (definition == nullptr || !definition->enabled) {
        if (!event.jobPrefix.empty()) {
            keybinder::Logger::Debug(
                "[JobParser] No enabled job definition for prefix='%s' line='%s'",
                event.jobPrefix.c_str(),
                event.normalizedLine.c_str());
        }
        return parsedEvents;
    }

    const std::string payload = ExtractPayload(event);
    if (payload.empty()) {
        keybinder::Logger::Debug("[JobParser] Empty payload for job '%s' line='%s'",
            definition->id.c_str(),
            event.normalizedLine.c_str());
        return parsedEvents;
    }

    if (IsInfoLine(event, payload)) {
        keybinder::Logger::Debug("[JobParser] Ignored INFO line for job '%s': '%s'",
            definition->id.c_str(),
            payload.c_str());
        return parsedEvents;
    }

    if (IsStartTaskLine(payload)) {
        keybinder::Logger::Debug("[JobParser] Ignored start-task line for job '%s': '%s'",
            definition->id.c_str(),
            payload.c_str());
        return parsedEvents;
    }

    for (const ParserRule& rule : definition->parserRules) {
        const bool isIgnoredRule = (rule.eventType == kEventTypeIgnore);
        const bool isJobMessageRule = (rule.eventType == kEventTypeJobMessage);
        const std::string storedType = isIgnoredRule ? std::string() : (isJobMessageRule ? std::string(kEventTypeJobMessage) : ResolveStoredEventType(rule));
        if (!isIgnoredRule && storedType.empty()) {
            continue;
        }

        const std::regex* pattern = GetOrCreateRegex(*definition, rule);
        if (pattern == nullptr) {
            continue;
        }

        std::smatch match;
        if (!std::regex_match(payload, match, *pattern)) {
            continue;
        }

        if (isIgnoredRule) {
            keybinder::Logger::Debug("[JobParser] Ignored rule matched for job '%s': '%s'",
                definition->id.c_str(),
                payload.c_str());
            return parsedEvents;
        }

        JobEvent parsedEvent;
        parsedEvent.jobId = definition->id;
        parsedEvent.type = storedType;
        parsedEvent.source = kSourceChatAuto;
        parsedEvent.timestamp = event.arrivalTime != 0 ? event.arrivalTime : std::time(nullptr);
        if (isJobMessageRule) {
            parsedEvent.note = payload;
        }

        const auto amountIt = rule.captureGroups.find("amount");
        if (amountIt != rule.captureGroups.end()) {
            parsedEvent.amount = ParseCaptureInt(match, amountIt->second);
        }

        const auto levelIt = rule.captureGroups.find("level");
        if (levelIt != rule.captureGroups.end()) {
            parsedEvent.level = ParseCaptureInt(match, levelIt->second);
        }

        parsedEvents.push_back(std::move(parsedEvent));
        keybinder::Logger::Debug(
            "[JobParser] Matched job='%s' type='%s' amount=%d level=%d payload='%s'",
            definition->id.c_str(),
            parsedEvents.back().type.c_str(),
            parsedEvents.back().amount,
            parsedEvents.back().level,
            payload.c_str());
        return parsedEvents;
    }

    keybinder::Logger::Debug("[JobParser] No rule matched for job '%s' payload='%s'",
        definition->id.c_str(),
        payload.c_str());
    TrackUnmatchedLine(definition->id, !event.normalizedLine.empty() ? event.normalizedLine : payload);
    return parsedEvents;
}

void JobParser::ResetDiagnostics() {
    diagnostics_.clear();
}

int JobParser::GetUnmatchedCount(const JobId& jobId) const {
    const auto it = diagnostics_.find(jobId);
    return it != diagnostics_.end() ? it->second.unmatchedCount : 0;
}

bool JobParser::EqualsNoCase(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }

    return true;
}

std::string JobParser::TrimCopy(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = text.find_last_not_of(" \t\n\r");
    return text.substr(first, last - first + 1);
}

std::string JobParser::ExtractPayload(const JobChatEvent& event) {
    const std::string line = !event.normalizedLine.empty() ? event.normalizedLine : event.timestampFreeLine;
    if (line.empty()) {
        return {};
    }

    if (!event.jobPrefix.empty()) {
        const std::string prefixedLabel = event.jobPrefix + ":";
        if (line.rfind(prefixedLabel, 0) == 0) {
            return TrimCopy(line.substr(prefixedLabel.size()));
        }
    }

    return TrimCopy(line);
}

bool JobParser::IsInfoLine(const JobChatEvent& event, const std::string& payload) {
    const std::string line = !event.normalizedLine.empty() ? event.normalizedLine : payload;
    return line.rfind("INFO:", 0) == 0 || payload.rfind("INFO:", 0) == 0;
}

bool JobParser::IsStartTaskLine(const std::string& payload) {
    if (payload.empty() || ContainsAnyDigit(payload) || payload.find('$') != std::string::npos) {
        return false;
    }

    if (payload.find("Job-Skill") != std::string::npos || payload.find("erhältst du") != std::string::npos) {
        return false;
    }

    return payload.find("...") != std::string::npos;
}

int JobParser::ParseCaptureInt(const std::smatch& match, int captureIndex) {
    if (captureIndex <= 0 || captureIndex >= static_cast<int>(match.size())) {
        return 0;
    }

    const std::string token = match[static_cast<std::size_t>(captureIndex)].str();
    std::string digits;
    digits.reserve(token.size());
    for (char ch : token) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-') {
            digits.push_back(ch);
        }
    }

    if (digits.empty() || digits == "-") {
        return 0;
    }

    return std::atoi(digits.c_str());
}

std::string JobParser::ResolveStoredEventType(const ParserRule& rule) {
    if (rule.captureGroups.find("amount") != rule.captureGroups.end()) {
        return kEventTypeEarning;
    }

    if (rule.captureGroups.find("level") != rule.captureGroups.end()) {
        return kEventTypeSkillLevelAbsolute;
    }

    if (rule.eventType == kEventTypeEarning || rule.eventType == kEventTypeManualEarning) {
        return kEventTypeEarning;
    }

    if (rule.eventType == kEventTypeSkillLevelAbsolute || rule.eventType == "level_up") {
        return kEventTypeSkillLevelAbsolute;
    }

    return {};
}

std::string JobParser::BuildCacheKey(const JobDefinition& definition, const ParserRule& rule) {
    return definition.id + "\n" + rule.eventType + "\n" + rule.pattern;
}

const JobDefinition* JobParser::FindJobDefinition(const JobChatEvent& event, const JobRegistry::Snapshot& snapshot) const {
    if (event.jobPrefix.empty()) {
        return nullptr;
    }

    for (const JobDefinition& definition : snapshot.jobs) {
        if (!definition.enabled) {
            continue;
        }

        if (EqualsNoCase(event.jobPrefix, definition.name) || EqualsNoCase(event.jobPrefix, definition.id)) {
            return &definition;
        }

        for (const std::string& alias : definition.speakerAliases) {
            if (EqualsNoCase(event.jobPrefix, alias)) {
                return &definition;
            }
        }
    }

    return nullptr;
}

const std::regex* JobParser::GetOrCreateRegex(const JobDefinition& definition, const ParserRule& rule) {
    const std::string cacheKey = BuildCacheKey(definition, rule);
    const auto it = regexCache_.find(cacheKey);
    if (it != regexCache_.end()) {
        return &it->second;
    }

    try {
        auto inserted = regexCache_.emplace(cacheKey, std::regex(rule.pattern, std::regex::ECMAScript | std::regex::icase));
        return &inserted.first->second;
    } catch (const std::regex_error&) {
        keybinder::Logger::Warning("[JobParser] Invalid parser regex for job '%s': %s",
            definition.id.c_str(),
            rule.pattern.c_str());
        return nullptr;
    }
}

void JobParser::TrackUnmatchedLine(const JobId& jobId, const std::string& line) {
    JobParseDiagnostics& diagnostics = diagnostics_[jobId];
    ++diagnostics.unmatchedCount;
    diagnostics.lastUnmatchedLine = line;
}

} // namespace jobs
