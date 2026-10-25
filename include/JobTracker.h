#pragma once

#include "JobChatAdapter.h"
#include "JobEventStore.h"
#include "JobParser.h"
#include "JobRegistry.h"

#include <ctime>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace jobs {

class JobTracker {
public:
    using RegistryLoader = std::function<bool(JobRegistry::Snapshot&)>;
    using ChatEventParser = std::function<std::vector<JobEvent>(const JobChatEvent&, const JobRegistry::Snapshot&)>;

    JobTracker();
    explicit JobTracker(const std::string& dataFilePath);
    JobTracker(RegistryLoader registryLoader, const std::string& dataFilePath = std::string());
    ~JobTracker();

    bool Initialize();
    void Shutdown();
    bool Flush();

    bool IsInitialized() const { return initialized_; }
    bool IsDirty() const { return dirty_; }
    bool HasRegistryData() const { return !registrySnapshot_.jobs.empty(); }
    std::string GetDataFilePath() const;

    const JobRegistry::Snapshot& GetRegistrySnapshot() const { return registrySnapshot_; }
    const std::vector<JobDefinition>& GetJobs() const { return registrySnapshot_.jobs; }
    const std::vector<JobEvent>& GetEvents() const;
    std::vector<JobEvent> GetEventsForJob(const JobId& jobId, bool includeDeleted = false) const;
    JobWindowAggregates GetAggregates(const JobId& jobId, std::time_t now = 0) const;
    int GetGlobalBestDay() const;
    bool GetJobStats(const JobId& jobId, JobStats& stats) const;
    const std::unordered_map<JobId, JobStats>& GetAllJobStats() const;
    int GetAdjustedHighestSkill(const JobId& jobId) const;

    void SetParserCallback(ChatEventParser callback);
    std::vector<JobEvent> HandleChatEvent(const JobChatEvent& event);

    bool RecordEvent(JobEvent event);
    bool AddManualEarningCorrection(const JobId& jobId, int amount, std::time_t timestamp = 0);
    bool AddManualSkillLevelCorrection(const JobId& jobId, int level, std::time_t timestamp = 0);
    std::string AddManualEvent(const JobId& jobId, const std::string& type, int amount, int level,
                                std::time_t timestamp, const std::string& note = std::string());
    bool DeleteEvent(const std::string& eventId);

    JobEditSnapshot GetJobEditSnapshot(const JobId& jobId) const;
    void ApplyJobSummaryAdjustments(const JobId& jobId, int earningsOffset, int skillOffset);

    std::vector<JobEvent> ExportEvents() const;
    bool ImportEvents(const std::vector<JobEvent>& events);
    bool ImportSynchronizedSkills(const std::map<JobId, int>& points, std::time_t timestamp = 0);

private:
    bool LoadRegistry();
    bool LoadPersistedEvents();
    bool IsKnownJobId(const JobId& jobId) const;
    void MarkAggregatesDirty();
    void RefreshAggregateCache(std::time_t now = 0) const;
    static std::time_t ResolveNow(std::time_t now);
    static std::string ResolveDayKey(std::time_t now);

    std::unique_ptr<JobRegistry> registry_;
    std::unique_ptr<JobEventStore> eventStore_;
    std::unique_ptr<JobParser> parser_;
    RegistryLoader registryLoader_;
    ChatEventParser parserCallback_;
    JobRegistry::Snapshot registrySnapshot_;
    bool initialized_;
    bool dirty_;

    mutable std::unordered_map<JobId, JobWindowAggregates> aggregateCache_;
    mutable std::string aggregateCacheDayKey_;
    mutable bool aggregateCacheDirty_;
};

} // namespace jobs
