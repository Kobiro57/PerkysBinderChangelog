#pragma once

#include "JobsTypes.h"

#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace jobs {

struct JobWindowAggregates {
    int today;
    int last7Days;
    int last30Days;
    int bestDay;
    int lifetime;

    JobWindowAggregates()
        : today(0)
        , last7Days(0)
        , last30Days(0)
        , bestDay(0)
        , lifetime(0) {}
};

class JobEventStore {
public:
    JobEventStore();
    explicit JobEventStore(const std::string& filepath);
    ~JobEventStore();

    bool Load();
    bool Save() const;
    bool FileExists() const;

    void Clear();

    bool RecordEvent(JobEvent event);
    bool SoftDeleteEvent(const std::string& eventId);
    bool GetEventById(const std::string& eventId, JobEvent& event) const;

    const std::vector<JobEvent>& GetEvents() const { return events_; }
    std::vector<JobEvent> GetEventsForJob(const JobId& jobId, bool includeDeleted = false) const;

    JobWindowAggregates GetAggregates(const JobId& jobId, std::time_t now = 0) const;
    int GetGlobalBestDay() const;
    bool GetJobStats(const JobId& jobId, JobStats& stats) const;
    const std::unordered_map<JobId, JobStats>& GetAllJobStats() const { return jobState_; }

    JobSummaryAdjustment GetJobSummaryAdjustments(const JobId& jobId) const;
    void SetJobSummaryAdjustments(const JobId& jobId, const JobSummaryAdjustment& adjustment);
    const std::unordered_map<JobId, JobSummaryAdjustment>& GetAllSummaryAdjustments() const { return summaryAdjustments_; }

    int GetAdjustedLifetimeEarnings(const JobId& jobId) const;
    int GetAdjustedHighestSkill(const JobId& jobId) const;

    std::string GetFilePath() const { return filepath_; }

    static std::string BuildDayKey(std::time_t timestamp);

private:
    using DayBucketMap = std::unordered_map<std::string, int>;
    using JobDayBuckets = std::unordered_map<JobId, DayBucketMap>;

    std::string SerializeStore() const;
    bool DeserializeStore(const std::string& json,
        std::vector<JobEvent>& events,
        std::unordered_map<JobId, JobStats>& jobState,
        std::unordered_map<JobId, JobSummaryAdjustment>& summaryAdjustments) const;
    bool ValidateEvent(const JobEvent& event) const;
    std::string GenerateEventId(std::time_t timestamp);

    void RebuildDerivedState();
    void ApplyActiveEvent(const JobEvent& event);

    std::string filepath_;
    std::vector<JobEvent> events_;
    std::unordered_map<JobId, JobStats> jobState_;
    std::unordered_map<JobId, JobSummaryAdjustment> summaryAdjustments_;
    JobDayBuckets earningsByDay_;
    std::size_t nextGeneratedId_;
};

} // namespace jobs
