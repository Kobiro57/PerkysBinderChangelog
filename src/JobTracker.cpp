#include "../include/JobTracker.h"

#include "../include/Logger.h"

#include <algorithm>
#include <ctime>
#include <utility>

namespace jobs {

constexpr const char* kEventTypeManualEarning = "manual_earning";
constexpr const char* kEventTypeManualSkillLevel = "manual_skill_level";
constexpr const char* kSourceChatAuto = "chat_auto";
constexpr const char* kSourceManualAdd = "manual_add";

std::unique_ptr<JobEventStore> CreateEventStore(const std::string& dataFilePath) {
    if (dataFilePath.empty()) {
        return std::make_unique<JobEventStore>();
    }

    return std::make_unique<JobEventStore>(dataFilePath);
}

JobTracker::JobTracker()
    : registry_(std::make_unique<JobRegistry>())
    , eventStore_(std::make_unique<JobEventStore>())
    , parser_(std::make_unique<JobParser>())
    , initialized_(false)
    , dirty_(false)
    , aggregateCacheDirty_(true) {
}

JobTracker::JobTracker(const std::string& dataFilePath)
    : registry_(std::make_unique<JobRegistry>())
    , eventStore_(CreateEventStore(dataFilePath))
    , parser_(std::make_unique<JobParser>())
    , initialized_(false)
    , dirty_(false)
    , aggregateCacheDirty_(true) {
}

JobTracker::JobTracker(RegistryLoader registryLoader, const std::string& dataFilePath)
    : registry_(std::make_unique<JobRegistry>())
    , eventStore_(CreateEventStore(dataFilePath))
    , parser_(std::make_unique<JobParser>())
    , registryLoader_(std::move(registryLoader))
    , initialized_(false)
    , dirty_(false)
    , aggregateCacheDirty_(true) {
}

JobTracker::~JobTracker() {
    if (initialized_) {
        Shutdown();
    }
}

bool JobTracker::Initialize() {
    if (initialized_) {
        return true;
    }

    registrySnapshot_ = {};
    if (!LoadRegistry()) {
        keybinder::Logger::Warning("[JobTracker] Registry unavailable or invalid; starting with empty jobs state");
        if (eventStore_ != nullptr) {
            eventStore_->Clear();
        }
        aggregateCache_.clear();
        aggregateCacheDayKey_.clear();
        dirty_ = false;
        aggregateCacheDirty_ = true;
        initialized_ = true;
        return true;
    }

    if (!LoadPersistedEvents() && eventStore_ != nullptr) {
        keybinder::Logger::Warning("[JobTracker] Jobs data load failed; continuing with empty persisted state");
        eventStore_->Clear();
    }

    dirty_ = false;
    aggregateCache_.clear();
    aggregateCacheDayKey_.clear();
    aggregateCacheDirty_ = true;
    initialized_ = true;

    keybinder::Logger::Info("[JobTracker] Initialized with %zu job definition(s) and %zu persisted event(s)",
        registrySnapshot_.jobs.size(),
        eventStore_ != nullptr ? eventStore_->GetEvents().size() : 0);
    return true;
}

void JobTracker::Shutdown() {
    if (!initialized_) {
        return;
    }

    Flush();
    initialized_ = false;
    keybinder::Logger::Info("[JobTracker] Shutdown complete");
}

bool JobTracker::Flush() {
    if (!initialized_ || eventStore_ == nullptr || !dirty_) {
        return true;
    }

    if (!eventStore_->Save()) {
        keybinder::Logger::Warning("[JobTracker] Failed to flush jobs data to disk");
        return false;
    }

    dirty_ = false;
    return true;
}

std::string JobTracker::GetDataFilePath() const {
    return eventStore_ != nullptr ? eventStore_->GetFilePath() : std::string();
}

const std::vector<JobEvent>& JobTracker::GetEvents() const {
    static const std::vector<JobEvent> kEmptyEvents;
    return eventStore_ != nullptr ? eventStore_->GetEvents() : kEmptyEvents;
}

std::vector<JobEvent> JobTracker::GetEventsForJob(const JobId& jobId, bool includeDeleted) const {
    if (eventStore_ == nullptr) {
        return {};
    }

    return eventStore_->GetEventsForJob(jobId, includeDeleted);
}

JobWindowAggregates JobTracker::GetAggregates(const JobId& jobId, std::time_t now) const {
    if (eventStore_ == nullptr) {
        return {};
    }

    RefreshAggregateCache(now);

    const auto it = aggregateCache_.find(jobId);
    if (it != aggregateCache_.end()) {
        return it->second;
    }

    return eventStore_->GetAggregates(jobId, ResolveNow(now));
}

int JobTracker::GetGlobalBestDay() const {
    if (eventStore_ == nullptr) {
        return 0;
    }

    return eventStore_->GetGlobalBestDay();
}

bool JobTracker::GetJobStats(const JobId& jobId, JobStats& stats) const {
    if (eventStore_ == nullptr) {
        return false;
    }

    return eventStore_->GetJobStats(jobId, stats);
}

const std::unordered_map<JobId, JobStats>& JobTracker::GetAllJobStats() const {
    static const std::unordered_map<JobId, JobStats> kEmptyStats;
    return eventStore_ != nullptr ? eventStore_->GetAllJobStats() : kEmptyStats;
}

int JobTracker::GetAdjustedHighestSkill(const JobId& jobId) const {
    if (eventStore_ == nullptr) {
        return 0;
    }
    return eventStore_->GetAdjustedHighestSkill(jobId);
}

void JobTracker::SetParserCallback(ChatEventParser callback) {
    parserCallback_ = std::move(callback);
}

std::vector<JobEvent> JobTracker::HandleChatEvent(const JobChatEvent& event) {
    if (!initialized_) {
        keybinder::Logger::Warning("[JobTracker] Ignored chat event while uninitialized: '%s'",
            event.normalizedLine.c_str());
        return {};
    }

    const std::vector<JobEvent> parsedEvents = parserCallback_
        ? parserCallback_(event, registrySnapshot_)
        : (parser_ != nullptr ? parser_->Parse(event, registrySnapshot_) : std::vector<JobEvent>());

    if (!event.jobPrefix.empty()) {
        keybinder::Logger::Debug("[JobTracker] Chat event for prefix='%s' produced %zu parsed event(s)",
            event.jobPrefix.c_str(),
            parsedEvents.size());
    }

    std::vector<JobEvent> recordedEvents;
    recordedEvents.reserve(parsedEvents.size());
    for (const JobEvent& parsedEvent : parsedEvents) {
        if (RecordEvent(parsedEvent)) {
            recordedEvents.push_back(parsedEvent);
            keybinder::Logger::Debug("[JobTracker] Recorded event job='%s' type='%s' amount=%d level=%d source='%s'",
                parsedEvent.jobId.c_str(),
                parsedEvent.type.c_str(),
                parsedEvent.amount,
                parsedEvent.level,
                parsedEvent.source.c_str());
        } else {
            keybinder::Logger::Warning("[JobTracker] Failed to record parsed event job='%s' type='%s' amount=%d level=%d source='%s'",
                parsedEvent.jobId.c_str(),
                parsedEvent.type.c_str(),
                parsedEvent.amount,
                parsedEvent.level,
                parsedEvent.source.c_str());
        }
    }

    return recordedEvents;
}

bool JobTracker::RecordEvent(JobEvent event) {
    if (!initialized_ || eventStore_ == nullptr) {
        return false;
    }

    if (!IsKnownJobId(event.jobId)) {
        keybinder::Logger::Warning("[JobTracker] Rejected event for unknown job '%s'", event.jobId.c_str());
        return false;
    }

    if (!eventStore_->RecordEvent(std::move(event))) {
        return false;
    }

    dirty_ = true;
    MarkAggregatesDirty();
    return true;
}

bool JobTracker::AddManualEarningCorrection(const JobId& jobId, int amount, std::time_t timestamp) {
    JobEvent event;
    event.jobId = jobId;
    event.type = kEventTypeManualEarning;
    event.amount = amount;
    event.level = 0;
    event.source = kSourceManualAdd;
    event.timestamp = timestamp;
    return RecordEvent(std::move(event));
}

bool JobTracker::AddManualSkillLevelCorrection(const JobId& jobId, int level, std::time_t timestamp) {
    JobEvent event;
    event.jobId = jobId;
    event.type = kEventTypeManualSkillLevel;
    event.amount = 0;
    event.level = level;
    event.source = kSourceManualAdd;
    event.timestamp = timestamp;
    return RecordEvent(std::move(event));
}

std::string JobTracker::AddManualEvent(const JobId& jobId, const std::string& type, int amount, int level,
                                        std::time_t timestamp, const std::string& note) {
    if (jobId.empty()) {
        keybinder::Logger::Warning("[JobTracker] AddManualEvent rejected: empty jobId");
        return {};
    }

    if (amount < 0) {
        keybinder::Logger::Warning("[JobTracker] AddManualEvent rejected: negative amount %d", amount);
        return {};
    }

    if (level < 0) {
        keybinder::Logger::Warning("[JobTracker] AddManualEvent rejected: negative level %d", level);
        return {};
    }

    if (!IsKnownJobId(jobId)) {
        keybinder::Logger::Warning("[JobTracker] AddManualEvent rejected: unknown job '%s'", jobId.c_str());
        return {};
    }

    JobEvent event;
    event.jobId = jobId;
    event.type = type;
    event.amount = amount;
    event.level = level;
    event.source = kSourceManualAdd;
    event.timestamp = timestamp;
    event.note = note;

    if (!RecordEvent(std::move(event))) {
        return {};
    }

    const auto& events = GetEvents();
    if (!events.empty()) {
        return events.back().id;
    }

    return {};
}

bool JobTracker::DeleteEvent(const std::string& eventId) {
    if (!initialized_ || eventStore_ == nullptr) {
        return false;
    }

    if (!eventStore_->SoftDeleteEvent(eventId)) {
        return false;
    }

    dirty_ = true;
    MarkAggregatesDirty();
    return true;
}

JobEditSnapshot JobTracker::GetJobEditSnapshot(const JobId& jobId) const {
    JobEditSnapshot snapshot;
    snapshot.jobId = jobId;

    if (eventStore_ == nullptr) {
        return snapshot;
    }

    JobStats stats;
    if (eventStore_->GetJobStats(jobId, stats)) {
        snapshot.baseTotalEarnings = stats.totalAmount;
        snapshot.baseHighestSkill = stats.highestLevel;
    }

    const JobSummaryAdjustment adj = eventStore_->GetJobSummaryAdjustments(jobId);
    snapshot.earningsOffset = adj.earningsOffset;
    snapshot.skillOffset = adj.skillOffset;

    snapshot.adjustedTotalEarnings = eventStore_->GetAdjustedLifetimeEarnings(jobId);
    snapshot.adjustedHighestSkill = eventStore_->GetAdjustedHighestSkill(jobId);

    return snapshot;
}

void JobTracker::ApplyJobSummaryAdjustments(const JobId& jobId, int earningsOffset, int skillOffset) {
    if (eventStore_ == nullptr) {
        return;
    }

    JobSummaryAdjustment adj;
    adj.jobId = jobId;
    adj.earningsOffset = earningsOffset;
    adj.skillOffset = skillOffset;

    eventStore_->SetJobSummaryAdjustments(jobId, adj);
    dirty_ = true;
    MarkAggregatesDirty();
}

std::vector<JobEvent> JobTracker::ExportEvents() const {
    if (eventStore_ == nullptr) {
        return {};
    }

    return eventStore_->GetEvents();
}

bool JobTracker::ImportSynchronizedSkills(const std::map<JobId, int>& points, std::time_t timestamp) {
    if (!initialized_ || !eventStore_ || points.empty()) return false;
    for (const auto& point : points)
        if (!IsKnownJobId(point.first) || point.second < 0) return false;
    const JobEventStore previous = *eventStore_;
    const bool previousDirty = dirty_;
    for (const auto& point : points) {
        JobStats stats;
        const bool exists = eventStore_->GetJobStats(point.first, stats);
        const auto before = GetJobEditSnapshot(point.first);
        if (exists && stats.highestLevel == point.second && before.skillOffset == 0) continue;
        JobEvent event;
        event.jobId = point.first;
        event.type = "skill_level_absolute";
        event.level = point.second;
        event.source = "chat_auto";
        event.timestamp = ResolveNow(timestamp);
        event.note = "Synchronisation /jobskills";
        if (!RecordEvent(std::move(event))) {
            *eventStore_ = previous;
            dirty_ = previousDirty;
            MarkAggregatesDirty();
            return false;
        }
        const auto after = GetJobEditSnapshot(point.first);
        // Preserve earnings and existing history. The received value is an
        // absolute server baseline, even if an older manual event is future-dated.
        ApplyJobSummaryAdjustments(point.first, before.earningsOffset, point.second - after.baseHighestSkill);
    }
    if (Flush()) return true;
    *eventStore_ = previous;
    dirty_ = previousDirty;
    MarkAggregatesDirty();
    return false;
}

bool JobTracker::ImportEvents(const std::vector<JobEvent>& events) {
    if (!initialized_ || eventStore_ == nullptr) {
        return false;
    }

    eventStore_->Clear();

    std::vector<std::string> deletedEventIds;
    deletedEventIds.reserve(events.size());

    for (JobEvent event : events) {
        const bool shouldSoftDelete = event.deleted;
        const std::string eventId = event.id;
        event.deleted = false;

        if (!RecordEvent(std::move(event))) {
            eventStore_->Clear();
            dirty_ = false;
            MarkAggregatesDirty();
            return false;
        }

        if (shouldSoftDelete) {
            deletedEventIds.push_back(eventId);
        }
    }

    for (const std::string& eventId : deletedEventIds) {
        if (!eventStore_->SoftDeleteEvent(eventId)) {
            eventStore_->Clear();
            dirty_ = false;
            MarkAggregatesDirty();
            return false;
        }
    }

    dirty_ = true;
    MarkAggregatesDirty();
    return true;
}

bool JobTracker::LoadRegistry() {
    JobRegistry::Snapshot snapshot;

    bool loaded = false;
    if (registryLoader_) {
        loaded = registryLoader_(snapshot);
    } else if (registry_ != nullptr) {
        loaded = registry_->LoadEmbedded(snapshot);
    }

    if (!loaded || !JobRegistry::Validate(snapshot)) {
        registrySnapshot_ = {};
        return false;
    }

    registrySnapshot_ = std::move(snapshot);
    return true;
}

bool JobTracker::LoadPersistedEvents() {
    if (eventStore_ == nullptr) {
        return false;
    }

    return eventStore_->Load();
}

bool JobTracker::IsKnownJobId(const JobId& jobId) const {
    return std::any_of(registrySnapshot_.jobs.begin(), registrySnapshot_.jobs.end(), [&jobId](const JobDefinition& definition) {
        return definition.id == jobId;
    });
}

void JobTracker::MarkAggregatesDirty() {
    aggregateCacheDirty_ = true;
    aggregateCacheDayKey_.clear();
}

void JobTracker::RefreshAggregateCache(std::time_t now) const {
    if (eventStore_ == nullptr) {
        aggregateCache_.clear();
        aggregateCacheDayKey_.clear();
        aggregateCacheDirty_ = false;
        return;
    }

    const std::time_t effectiveNow = ResolveNow(now);
    const std::string dayKey = ResolveDayKey(effectiveNow);
    if (!aggregateCacheDirty_ && aggregateCacheDayKey_ == dayKey) {
        return;
    }

    aggregateCache_.clear();
    aggregateCacheDayKey_ = dayKey;

    for (const auto& entry : eventStore_->GetAllJobStats()) {
        aggregateCache_[entry.first] = eventStore_->GetAggregates(entry.first, effectiveNow);
    }

    aggregateCacheDirty_ = false;
}

std::time_t JobTracker::ResolveNow(std::time_t now) {
    return now != 0 ? now : std::time(nullptr);
}

std::string JobTracker::ResolveDayKey(std::time_t now) {
    return JobEventStore::BuildDayKey(ResolveNow(now));
}

}
