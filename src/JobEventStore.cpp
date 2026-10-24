#include "../include/JobEventStore.h"

#include "../include/Logger.h"
#include "../include/PersistentPaths.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace jobs {

namespace {

constexpr int kStorageVersion = 1;
constexpr const char* kEventTypeEarning = "earning";
constexpr const char* kEventTypeSkillLevelAbsolute = "skill_level_absolute";
constexpr const char* kEventTypeJobMessage = "job_message";
constexpr const char* kEventTypeManualEarning = "manual_earning";
constexpr const char* kEventTypeManualSkillLevel = "manual_skill_level";
constexpr const char* kSourceChatAuto = "chat_auto";
constexpr const char* kSourceManualAdd = "manual_add";
constexpr const char* kGeneratedEventPrefix = "evt_";

bool IsEscapedJsonCharacter(const std::string& text, std::size_t pos) {
    if (pos == 0 || pos >= text.size()) {
        return false;
    }

    std::size_t backslashCount = 0;
    std::size_t checkPos = pos;
    while (checkPos > 0 && text[checkPos - 1] == '\\') {
        ++backslashCount;
        --checkPos;
    }

    return (backslashCount % 2) != 0;
}

std::size_t FindMatchingDelimiter(const std::string& text, std::size_t openPos, char openDelimiter, char closeDelimiter) {
    if (openPos == std::string::npos || openPos >= text.size() || text[openPos] != openDelimiter) {
        return std::string::npos;
    }

    std::size_t depth = 0;
    bool inString = false;
    for (std::size_t i = openPos; i < text.size(); ++i) {
        const char ch = text[i];

        if (ch == '"' && !IsEscapedJsonCharacter(text, i)) {
            inString = !inString;
            continue;
        }

        if (inString) {
            continue;
        }

        if (ch == openDelimiter) {
            ++depth;
        } else if (ch == closeDelimiter) {
            if (depth == 0) {
                return std::string::npos;
            }

            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

bool FindFieldColon(const std::string& object, const char* key, std::size_t& colonPos) {
    const std::size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    colonPos = object.find(':', keyPos);
    return colonPos != std::string::npos;
}

std::string UnescapeJsonString(const std::string& value) {
    std::string unescaped;
    unescaped.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch != '\\' || i + 1 >= value.size()) {
            unescaped.push_back(ch);
            continue;
        }

        const char escaped = value[++i];
        switch (escaped) {
            case '"': unescaped.push_back('"'); break;
            case '\\': unescaped.push_back('\\'); break;
            case '/': unescaped.push_back('/'); break;
            case 'b': unescaped.push_back('\b'); break;
            case 'f': unescaped.push_back('\f'); break;
            case 'n': unescaped.push_back('\n'); break;
            case 'r': unescaped.push_back('\r'); break;
            case 't': unescaped.push_back('\t'); break;
            default:
                unescaped.push_back(escaped);
                break;
        }
    }

    return unescaped;
}

std::string EscapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (char ch : value) {
        switch (ch) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

bool ParseStringField(const std::string& object, const char* key, std::string& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t firstQuote = object.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return false;
    }

    std::size_t secondQuote = firstQuote + 1;
    while (secondQuote < object.size()) {
        secondQuote = object.find('"', secondQuote);
        if (secondQuote == std::string::npos) {
            return false;
        }

        if (!IsEscapedJsonCharacter(object, secondQuote)) {
            break;
        }

        ++secondQuote;
    }

    if (secondQuote == std::string::npos || secondQuote >= object.size()) {
        return false;
    }

    outValue = UnescapeJsonString(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
    return true;
}

bool ParseBoolField(const std::string& object, const char* key, bool& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t valuePos = object.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valuePos == std::string::npos) {
        return false;
    }

    if (object.compare(valuePos, 4, "true") == 0) {
        outValue = true;
        return true;
    }

    if (object.compare(valuePos, 5, "false") == 0) {
        outValue = false;
        return true;
    }

    return false;
}

bool ParseInt64Field(const std::string& object, const char* key, long long& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t numStart = object.find_first_of("0123456789-", colonPos + 1);
    if (numStart == std::string::npos) {
        return false;
    }

    const std::size_t numEnd = object.find_first_not_of("0123456789-", numStart);
    const std::string token = object.substr(numStart, numEnd == std::string::npos ? std::string::npos : numEnd - numStart);
    char* endPtr = nullptr;
    const long long parsed = std::strtoll(token.c_str(), &endPtr, 10);
    if (endPtr == nullptr || *endPtr != '\0') {
        return false;
    }

    outValue = parsed;
    return true;
}

bool ParseIntField(const std::string& object, const char* key, int& outValue) {
    long long value = 0;
    if (!ParseInt64Field(object, key, value)) {
        return false;
    }

    if (value < static_cast<long long>((std::numeric_limits<int>::min)()) ||
        value > static_cast<long long>((std::numeric_limits<int>::max)())) {
        return false;
    }

    outValue = static_cast<int>(value);
    return true;
}

bool ExtractJsonArrayField(const std::string& object, const char* key, std::string& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t arrayStart = object.find('[', colonPos + 1);
    if (arrayStart == std::string::npos) {
        return false;
    }

    const std::size_t arrayEnd = FindMatchingDelimiter(object, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos) {
        return false;
    }

    outValue = object.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
    return true;
}

bool ExtractJsonObjectField(const std::string& object, const char* key, std::string& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t objectStart = object.find('{', colonPos + 1);
    if (objectStart == std::string::npos) {
        return false;
    }

    const std::size_t objectEnd = FindMatchingDelimiter(object, objectStart, '{', '}');
    if (objectEnd == std::string::npos) {
        return false;
    }

    outValue = object.substr(objectStart + 1, objectEnd - objectStart - 1);
    return true;
}

std::vector<std::string> ExtractObjectEntries(const std::string& arrayBody) {
    std::vector<std::string> objects;
    std::size_t cursor = 0;

    while (cursor < arrayBody.size()) {
        const std::size_t openPos = arrayBody.find('{', cursor);
        if (openPos == std::string::npos) {
            break;
        }

        const std::size_t closePos = FindMatchingDelimiter(arrayBody, openPos, '{', '}');
        if (closePos == std::string::npos) {
            objects.clear();
            return objects;
        }

        objects.push_back(arrayBody.substr(openPos, closePos - openPos + 1));
        cursor = closePos + 1;
    }

    return objects;
}

std::vector<std::pair<std::string, std::string>> ExtractNamedObjectEntries(const std::string& objectBody) {
    std::vector<std::pair<std::string, std::string>> entries;
    std::size_t cursor = 0;

    while (cursor < objectBody.size()) {
        const std::size_t keyStart = objectBody.find('"', cursor);
        if (keyStart == std::string::npos) {
            break;
        }

        std::size_t keyEnd = keyStart + 1;
        while (keyEnd < objectBody.size()) {
            keyEnd = objectBody.find('"', keyEnd);
            if (keyEnd == std::string::npos) {
                entries.clear();
                return entries;
            }

            if (!IsEscapedJsonCharacter(objectBody, keyEnd)) {
                break;
            }

            ++keyEnd;
        }

        const std::size_t colonPos = objectBody.find(':', keyEnd + 1);
        if (colonPos == std::string::npos) {
            entries.clear();
            return entries;
        }

        const std::size_t valueStart = objectBody.find('{', colonPos + 1);
        if (valueStart == std::string::npos) {
            entries.clear();
            return entries;
        }

        const std::size_t valueEnd = FindMatchingDelimiter(objectBody, valueStart, '{', '}');
        if (valueEnd == std::string::npos) {
            entries.clear();
            return entries;
        }

        const std::string key = UnescapeJsonString(objectBody.substr(keyStart + 1, keyEnd - keyStart - 1));
        entries.push_back(std::make_pair(key, objectBody.substr(valueStart, valueEnd - valueStart + 1)));
        cursor = valueEnd + 1;
    }

    return entries;
}

bool IsEarningEventType(const std::string& type) {
    return type == kEventTypeEarning || type == kEventTypeManualEarning;
}

bool IsSkillEventType(const std::string& type) {
    return type == kEventTypeSkillLevelAbsolute || type == kEventTypeManualSkillLevel;
}

bool IsMessageEventType(const std::string& type) {
    return type == kEventTypeJobMessage;
}

bool IsSupportedEventType(const std::string& type) {
    return IsEarningEventType(type) || IsSkillEventType(type) || IsMessageEventType(type);
}

bool IsSupportedSource(const std::string& source) {
    return source == kSourceChatAuto || source == kSourceManualAdd;
}

std::string BuildDayKeyFromTm(const std::tm& localTm) {
    std::ostringstream dayKey;
    dayKey << std::setw(4) << std::setfill('0') << (localTm.tm_year + 1900)
           << std::setw(2) << std::setfill('0') << (localTm.tm_mon + 1)
           << std::setw(2) << std::setfill('0') << localTm.tm_mday;
    return dayKey.str();
}

std::vector<std::string> BuildRollingWindowDayKeys(std::time_t now, int dayCount) {
    std::vector<std::string> keys;
    if (dayCount <= 0) {
        return keys;
    }

    if (now == 0) {
        now = std::time(nullptr);
    }

    std::tm baseLocalTime = {};
    if (localtime_s(&baseLocalTime, &now) != 0) {
        return keys;
    }

    baseLocalTime.tm_hour = 12;
    baseLocalTime.tm_min = 0;
    baseLocalTime.tm_sec = 0;
    baseLocalTime.tm_isdst = -1;

    keys.reserve(static_cast<std::size_t>(dayCount));
    for (int i = 0; i < dayCount; ++i) {
        std::tm dayLocalTime = baseLocalTime;
        dayLocalTime.tm_mday -= i;
        const std::time_t dayTime = std::mktime(&dayLocalTime);
        if (dayTime == static_cast<std::time_t>(-1)) {
            continue;
        }

        std::tm normalizedLocalTime = {};
        if (localtime_s(&normalizedLocalTime, &dayTime) != 0) {
            continue;
        }

        keys.push_back(BuildDayKeyFromTm(normalizedLocalTime));
    }

    return keys;
}

bool ParseJobEventObject(const std::string& object, JobEvent& event) {
    event = {};

    if (!ParseStringField(object, "\"id\"", event.id)) {
        return false;
    }

    if (!ParseStringField(object, "\"jobId\"", event.jobId)) {
        return false;
    }

    if (!ParseStringField(object, "\"type\"", event.type)) {
        return false;
    }

    if (!ParseIntField(object, "\"amount\"", event.amount)) {
        return false;
    }

    if (!ParseIntField(object, "\"level\"", event.level)) {
        return false;
    }

    if (!ParseStringField(object, "\"source\"", event.source)) {
        return false;
    }

    long long timestamp = 0;
    if (!ParseInt64Field(object, "\"timestamp\"", timestamp)) {
        return false;
    }
    event.timestamp = static_cast<std::time_t>(timestamp);

    if (!ParseStringField(object, "\"dayKey\"", event.dayKey)) {
        return false;
    }

    if (!ParseBoolField(object, "\"deleted\"", event.deleted)) {
        return false;
    }

    ParseStringField(object, "\"note\"", event.note);

    return true;
}

bool ParseJobStateObject(const std::string& object, const std::string& fallbackJobId, JobStats& stats) {
    stats = {};
    stats.jobId = fallbackJobId;

    ParseStringField(object, "\"jobId\"", stats.jobId);

    if (!ParseIntField(object, "\"totalEvents\"", stats.totalEvents)) {
        return false;
    }

    if (!ParseIntField(object, "\"totalAmount\"", stats.totalAmount)) {
        return false;
    }

    if (!ParseIntField(object, "\"highestLevel\"", stats.highestLevel)) {
        return false;
    }

    long long timestamp = 0;
    if (!ParseInt64Field(object, "\"lastEventTimestamp\"", timestamp)) {
        return false;
    }
    stats.lastEventTimestamp = static_cast<std::time_t>(timestamp);

    long long skillTimestamp = 0;
    if (ParseInt64Field(object, "\"lastSkillTimestamp\"", skillTimestamp)) {
        stats.lastSkillTimestamp = static_cast<std::time_t>(skillTimestamp);
    }

    if (!ParseStringField(object, "\"lastDayKey\"", stats.lastDayKey)) {
        return false;
    }

    return !stats.jobId.empty();
}

bool DirectoryExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsDriveRoot(const std::string& path) {
    if (path.size() == 2) {
        return path[1] == ':';
    }

    return path.size() == 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

bool EnsureDirectoryRecursive(const std::string& rawPath) {
    if (rawPath.empty() || IsDriveRoot(rawPath) || DirectoryExists(rawPath)) {
        return true;
    }

    const std::size_t separator = rawPath.find_last_of("\\/");
    if (separator != std::string::npos) {
        const std::string parent = rawPath.substr(0, separator);
        if (!parent.empty() && !IsDriveRoot(parent) && !EnsureDirectoryRecursive(parent)) {
            return false;
        }
    }

    if (CreateDirectoryA(rawPath.c_str(), nullptr) != 0) {
        return true;
    }

    const DWORD error = GetLastError();
    return error == ERROR_ALREADY_EXISTS && DirectoryExists(rawPath);
}

bool EnsureParentDirectoryExists(const std::string& filepath) {
    const std::size_t separator = filepath.find_last_of("\\/");
    if (separator == std::string::npos) {
        return true;
    }

    return EnsureDirectoryRecursive(filepath.substr(0, separator));
}

} // namespace

JobEventStore::JobEventStore()
    : filepath_(keybinder::PersistentPaths::JobsDataFile())
    , nextGeneratedId_(1) {
}

JobEventStore::JobEventStore(const std::string& filepath)
    : filepath_(filepath)
    , nextGeneratedId_(1) {
}

JobEventStore::~JobEventStore() = default;

bool JobEventStore::Load() {
    Clear();

    if (!FileExists()) {
        return true;
    }

    std::ifstream file(filepath_);
    if (!file.is_open()) {
        keybinder::Logger::Warning("[JobEventStore] Failed to open jobs data file: %s", filepath_.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::vector<JobEvent> events;
    std::unordered_map<JobId, JobStats> parsedJobState;
    std::unordered_map<JobId, JobSummaryAdjustment> parsedAdjustments;
    if (!DeserializeStore(buffer.str(), events, parsedJobState, parsedAdjustments)) {
        keybinder::Logger::Warning("[JobEventStore] Failed to parse jobs data file: %s", filepath_.c_str());
        Clear();
        return false;
    }

    events_ = std::move(events);
    summaryAdjustments_ = std::move(parsedAdjustments);
    RebuildDerivedState();
    keybinder::Logger::Info("[JobEventStore] Loaded %zu job events from %s", events_.size(), filepath_.c_str());
    return true;
}

bool JobEventStore::Save() const {
    if (filepath_.empty()) {
        keybinder::Logger::Error("[JobEventStore] Cannot save jobs data: empty file path");
        return false;
    }

    if (!EnsureParentDirectoryExists(filepath_)) {
        keybinder::Logger::Error("[JobEventStore] Failed to create jobs data directory for %s", filepath_.c_str());
        return false;
    }

    const std::string json = SerializeStore();

    std::ofstream file(filepath_);
    if (!file.is_open()) {
        keybinder::Logger::Error("[JobEventStore] Failed to open jobs data file for writing: %s", filepath_.c_str());
        return false;
    }

    file << json;
    const bool writeSuccess = file.good();
    file.close();

    if (!writeSuccess) {
        keybinder::Logger::Error("[JobEventStore] Failed to write jobs data file: %s", filepath_.c_str());
        return false;
    }

    keybinder::Logger::Info("[JobEventStore] Saved %zu job events to %s", events_.size(), filepath_.c_str());
    return true;
}

bool JobEventStore::FileExists() const {
    if (filepath_.empty()) {
        return false;
    }

    std::ifstream file(filepath_);
    return file.good();
}

void JobEventStore::Clear() {
    events_.clear();
    jobState_.clear();
    summaryAdjustments_.clear();
    earningsByDay_.clear();
    nextGeneratedId_ = 1;
}

bool JobEventStore::RecordEvent(JobEvent event) {
    if (event.timestamp == 0) {
        event.timestamp = std::time(nullptr);
    }

    event.dayKey = BuildDayKey(event.timestamp);
    event.deleted = false;

    if (event.id.empty()) {
        event.id = GenerateEventId(event.timestamp);
    }

    if (!ValidateEvent(event)) {
        keybinder::Logger::Warning("[JobEventStore] Rejected invalid event for job '%s'", event.jobId.c_str());
        return false;
    }

    events_.push_back(event);
    ApplyActiveEvent(events_.back());
    return true;
}

bool JobEventStore::SoftDeleteEvent(const std::string& eventId) {
    if (eventId.empty()) {
        return false;
    }

    for (JobEvent& event : events_) {
        if (event.id != eventId) {
            continue;
        }

        if (event.deleted) {
            return true;
        }

        event.deleted = true;
        RebuildDerivedState();
        return true;
    }

    return false;
}

bool JobEventStore::GetEventById(const std::string& eventId, JobEvent& event) const {
    for (const JobEvent& current : events_) {
        if (current.id == eventId) {
            event = current;
            return true;
        }
    }

    return false;
}

std::vector<JobEvent> JobEventStore::GetEventsForJob(const JobId& jobId, bool includeDeleted) const {
    std::vector<JobEvent> result;
    for (const JobEvent& event : events_) {
        if (event.jobId != jobId) {
            continue;
        }

        if (!includeDeleted && event.deleted) {
            continue;
        }

        result.push_back(event);
    }

    return result;
}

JobWindowAggregates JobEventStore::GetAggregates(const JobId& jobId, std::time_t now) const {
    JobWindowAggregates aggregates;

    const auto bucketsIt = earningsByDay_.find(jobId);
    if (bucketsIt != earningsByDay_.end()) {
        const DayBucketMap& dayBuckets = bucketsIt->second;

        const std::vector<std::string> todayKeys = BuildRollingWindowDayKeys(now, 1);
        const std::vector<std::string> last7Keys = BuildRollingWindowDayKeys(now, 7);
        const std::vector<std::string> last30Keys = BuildRollingWindowDayKeys(now, 30);

        for (const std::string& key : todayKeys) {
            const auto bucket = dayBuckets.find(key);
            if (bucket != dayBuckets.end()) {
                aggregates.today += bucket->second;
            }
        }

        for (const std::string& key : last7Keys) {
            const auto bucket = dayBuckets.find(key);
            if (bucket != dayBuckets.end()) {
                aggregates.last7Days += bucket->second;
            }
        }

        for (const std::string& key : last30Keys) {
            const auto bucket = dayBuckets.find(key);
            if (bucket != dayBuckets.end()) {
                aggregates.last30Days += bucket->second;
            }
        }

        for (const auto& bucket : dayBuckets) {
            aggregates.bestDay = std::max(aggregates.bestDay, bucket.second);
        }
    }

    const auto statsIt = jobState_.find(jobId);
    if (statsIt != jobState_.end()) {
        aggregates.lifetime = statsIt->second.totalAmount;
    }

    // Apply earnings offset to lifetime only; day windows stay event-derived
    const auto adjIt = summaryAdjustments_.find(jobId);
    if (adjIt != summaryAdjustments_.end()) {
        aggregates.lifetime = std::max(aggregates.lifetime + adjIt->second.earningsOffset, 0);
    }

    return aggregates;
}

int JobEventStore::GetGlobalBestDay() const {
    std::unordered_map<std::string, int> combinedByDay;

    for (const auto& jobEntry : earningsByDay_) {
        for (const auto& dayEntry : jobEntry.second) {
            combinedByDay[dayEntry.first] += dayEntry.second;
        }
    }

    int bestDay = 0;
    for (const auto& dayEntry : combinedByDay) {
        bestDay = (std::max)(bestDay, dayEntry.second);
    }

    return bestDay;
}

bool JobEventStore::GetJobStats(const JobId& jobId, JobStats& stats) const {
    const auto it = jobState_.find(jobId);
    if (it == jobState_.end()) {
        return false;
    }

    stats = it->second;
    return true;
}

JobSummaryAdjustment JobEventStore::GetJobSummaryAdjustments(const JobId& jobId) const {
    const auto it = summaryAdjustments_.find(jobId);
    if (it != summaryAdjustments_.end()) {
        return it->second;
    }

    JobSummaryAdjustment adj;
    adj.jobId = jobId;
    return adj;
}

void JobEventStore::SetJobSummaryAdjustments(const JobId& jobId, const JobSummaryAdjustment& adjustment) {
    summaryAdjustments_[jobId] = adjustment;
    summaryAdjustments_[jobId].jobId = jobId;
}

int JobEventStore::GetAdjustedLifetimeEarnings(const JobId& jobId) const {
    int base = 0;
    const auto statsIt = jobState_.find(jobId);
    if (statsIt != jobState_.end()) {
        base = statsIt->second.totalAmount;
    }

    int offset = 0;
    const auto adjIt = summaryAdjustments_.find(jobId);
    if (adjIt != summaryAdjustments_.end()) {
        offset = adjIt->second.earningsOffset;
    }

    return std::max(base + offset, 0);
}

int JobEventStore::GetAdjustedHighestSkill(const JobId& jobId) const {
    int base = 0;
    const auto statsIt = jobState_.find(jobId);
    if (statsIt != jobState_.end()) {
        base = statsIt->second.highestLevel;
    }

    int offset = 0;
    const auto adjIt = summaryAdjustments_.find(jobId);
    if (adjIt != summaryAdjustments_.end()) {
        offset = adjIt->second.skillOffset;
    }

    return std::max(base + offset, 0);
}

std::string JobEventStore::BuildDayKey(std::time_t timestamp) {
    if (timestamp == 0) {
        return {};
    }

    std::tm localTime = {};
    if (localtime_s(&localTime, &timestamp) != 0) {
        return {};
    }

    return BuildDayKeyFromTm(localTime);
}

std::string JobEventStore::SerializeStore() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": " << kStorageVersion << ",\n";
    json << "  \"events\": [\n";

    for (std::size_t i = 0; i < events_.size(); ++i) {
        const JobEvent& event = events_[i];
        if (i != 0) {
            json << ",\n";
        }

        json << "    {\n";
        json << "      \"id\": \"" << EscapeJsonString(event.id) << "\",\n";
        json << "      \"jobId\": \"" << EscapeJsonString(event.jobId) << "\",\n";
        json << "      \"type\": \"" << EscapeJsonString(event.type) << "\",\n";
        json << "      \"amount\": " << event.amount << ",\n";
        json << "      \"level\": " << event.level << ",\n";
        json << "      \"source\": \"" << EscapeJsonString(event.source) << "\",\n";
        json << "      \"timestamp\": " << static_cast<long long>(event.timestamp) << ",\n";
        json << "      \"dayKey\": \"" << EscapeJsonString(event.dayKey) << "\",\n";
        if (!event.note.empty()) {
            json << "      \"note\": \"" << EscapeJsonString(event.note) << "\",\n";
        }
        json << "      \"deleted\": " << (event.deleted ? "true" : "false") << "\n";
        json << "    }";
    }

    json << "\n  ],\n";
    json << "  \"jobState\": {\n";

    std::vector<JobId> jobIds;
    jobIds.reserve(jobState_.size());
    for (const auto& entry : jobState_) {
        jobIds.push_back(entry.first);
    }
    std::sort(jobIds.begin(), jobIds.end());

    for (std::size_t i = 0; i < jobIds.size(); ++i) {
        const JobStats& stats = jobState_.at(jobIds[i]);
        if (i != 0) {
            json << ",\n";
        }

        json << "    \"" << EscapeJsonString(jobIds[i]) << "\": {\n";
        json << "      \"jobId\": \"" << EscapeJsonString(stats.jobId) << "\",\n";
        json << "      \"totalEvents\": " << stats.totalEvents << ",\n";
        json << "      \"totalAmount\": " << stats.totalAmount << ",\n";
        json << "      \"highestLevel\": " << stats.highestLevel << ",\n";
        json << "      \"lastEventTimestamp\": " << static_cast<long long>(stats.lastEventTimestamp) << ",\n";
        json << "      \"lastSkillTimestamp\": " << static_cast<long long>(stats.lastSkillTimestamp) << ",\n";
        json << "      \"lastDayKey\": \"" << EscapeJsonString(stats.lastDayKey) << "\"\n";
        json << "    }";
    }

    json << "\n  },\n";

    json << "  \"summaryAdjustments\": {\n";

    std::vector<JobId> adjJobIds;
    adjJobIds.reserve(summaryAdjustments_.size());
    for (const auto& entry : summaryAdjustments_) {
        adjJobIds.push_back(entry.first);
    }
    std::sort(adjJobIds.begin(), adjJobIds.end());

    for (std::size_t i = 0; i < adjJobIds.size(); ++i) {
        const JobSummaryAdjustment& adj = summaryAdjustments_.at(adjJobIds[i]);
        if (i != 0) {
            json << ",\n";
        }

        json << "    \"" << EscapeJsonString(adjJobIds[i]) << "\": {\n";
        json << "      \"earningsOffset\": " << adj.earningsOffset << ",\n";
        json << "      \"skillOffset\": " << adj.skillOffset << "\n";
        json << "    }";
    }

    json << "\n  }\n";
    json << "}\n";
    return json.str();
}

bool JobEventStore::DeserializeStore(const std::string& json,
    std::vector<JobEvent>& events,
    std::unordered_map<JobId, JobStats>& jobState,
    std::unordered_map<JobId, JobSummaryAdjustment>& summaryAdjustments) const {

    events.clear();
    jobState.clear();
    summaryAdjustments.clear();

    int version = 0;
    if (!ParseIntField(json, "\"version\"", version) || version != kStorageVersion) {
        return false;
    }

    std::string eventsArray;
    if (!ExtractJsonArrayField(json, "\"events\"", eventsArray)) {
        return false;
    }

    const std::vector<std::string> eventObjects = ExtractObjectEntries(eventsArray);
    if (!eventsArray.empty() && eventObjects.empty()) {
        return false;
    }

    for (const std::string& object : eventObjects) {
        JobEvent event;
        if (!ParseJobEventObject(object, event)) {
            events.clear();
            jobState.clear();
            return false;
        }

        event.dayKey = BuildDayKey(event.timestamp);
        if (!ValidateEvent(event)) {
            events.clear();
            jobState.clear();
            return false;
        }

        events.push_back(std::move(event));
    }

    std::string jobStateObject;
    if (ExtractJsonObjectField(json, "\"jobState\"", jobStateObject)) {
        const std::vector<std::pair<std::string, std::string>> entries = ExtractNamedObjectEntries(jobStateObject);
        bool onlyWs = true;
        for (char c : jobStateObject) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                onlyWs = false;
                break;
            }
        }
        if (!onlyWs && entries.empty()) {
            events.clear();
            return false;
        }

        for (const auto& entry : entries) {
            JobStats stats;
            if (!ParseJobStateObject(entry.second, entry.first, stats)) {
                events.clear();
                jobState.clear();
                summaryAdjustments.clear();
                return false;
            }

            jobState[entry.first] = std::move(stats);
        }
    }

    std::string adjustmentsObject;
    if (ExtractJsonObjectField(json, "\"summaryAdjustments\"", adjustmentsObject)) {
        const std::vector<std::pair<std::string, std::string>> adjEntries = ExtractNamedObjectEntries(adjustmentsObject);
        // An object body that is only whitespace is a valid empty object
        bool onlyWhitespace = true;
        for (char c : adjustmentsObject) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                onlyWhitespace = false;
                break;
            }
        }
        if (!onlyWhitespace && adjEntries.empty()) {
            events.clear();
            jobState.clear();
            summaryAdjustments.clear();
            return false;
        }

        for (const auto& entry : adjEntries) {
            JobSummaryAdjustment adj;
            adj.jobId = entry.first;

            int earningsOffset = 0;
            int skillOffset = 0;
            ParseIntField(entry.second, "\"earningsOffset\"", earningsOffset);
            ParseIntField(entry.second, "\"skillOffset\"", skillOffset);

            adj.earningsOffset = earningsOffset;
            adj.skillOffset = skillOffset;
            summaryAdjustments[entry.first] = adj;
        }
    }

    return true;
}

bool JobEventStore::ValidateEvent(const JobEvent& event) const {
    if (event.id.empty() || event.jobId.empty() || event.timestamp == 0 || event.dayKey.empty()) {
        return false;
    }

    if (!IsSupportedEventType(event.type) || !IsSupportedSource(event.source)) {
        return false;
    }

    if (event.dayKey.size() != 8) {
        return false;
    }

    return true;
}

std::string JobEventStore::GenerateEventId(std::time_t timestamp) {
    std::string eventId;
    JobEvent existingEvent;

    do {
        std::ostringstream stream;
        stream << kGeneratedEventPrefix << static_cast<long long>(timestamp) << '_' << nextGeneratedId_++;
        eventId = stream.str();
    } while (GetEventById(eventId, existingEvent));

    return eventId;
}

void JobEventStore::RebuildDerivedState() {
    jobState_.clear();
    earningsByDay_.clear();

    for (JobEvent& event : events_) {
        event.dayKey = BuildDayKey(event.timestamp);
        if (!event.deleted) {
            ApplyActiveEvent(event);
        }
    }

    nextGeneratedId_ = events_.size() + 1;
}

void JobEventStore::ApplyActiveEvent(const JobEvent& event) {
    JobStats& stats = jobState_[event.jobId];
    const std::time_t previousLastTimestamp = stats.lastEventTimestamp;

    stats.jobId = event.jobId;
    stats.totalEvents += 1;

    if (event.timestamp >= previousLastTimestamp) {
        stats.lastEventTimestamp = event.timestamp;
        stats.lastDayKey = event.dayKey;
    }

    if (IsEarningEventType(event.type)) {
        stats.totalAmount += event.amount;
        earningsByDay_[event.jobId][event.dayKey] += event.amount;
    }

    if (IsSkillEventType(event.type)) {
        if (event.timestamp >= stats.lastSkillTimestamp) {
            stats.highestLevel = event.level;
            stats.lastSkillTimestamp = event.timestamp;
        }
    }

    if (stats.lastDayKey.empty()) {
        stats.lastDayKey = event.dayKey;
    }
}

} // namespace jobs
