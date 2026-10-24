#pragma once

#include "JobChatAdapter.h"
#include "JobRegistry.h"

#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace jobs {

struct JobParseDiagnostics {
    int unmatchedCount = 0;
    std::string lastUnmatchedLine;
};

class JobParser {
public:
    JobParser();

    std::vector<JobEvent> Parse(const JobChatEvent& event, const JobRegistry::Snapshot& snapshot);

    void ResetDiagnostics();
    int GetUnmatchedCount(const JobId& jobId) const;
    const std::unordered_map<JobId, JobParseDiagnostics>& GetDiagnostics() const { return diagnostics_; }

private:
    using RegexCache = std::unordered_map<std::string, std::regex>;

    static bool EqualsNoCase(const std::string& left, const std::string& right);
    static std::string TrimCopy(const std::string& text);
    static std::string ExtractPayload(const JobChatEvent& event);
    static bool IsInfoLine(const JobChatEvent& event, const std::string& payload);
    static bool IsStartTaskLine(const std::string& payload);
    static int ParseCaptureInt(const std::smatch& match, int captureIndex);
    static std::string ResolveStoredEventType(const ParserRule& rule);
    static std::string BuildCacheKey(const JobDefinition& definition, const ParserRule& rule);

    const JobDefinition* FindJobDefinition(const JobChatEvent& event, const JobRegistry::Snapshot& snapshot) const;
    const std::regex* GetOrCreateRegex(const JobDefinition& definition, const ParserRule& rule);
    void TrackUnmatchedLine(const JobId& jobId, const std::string& line);

    RegexCache regexCache_;
    std::unordered_map<JobId, JobParseDiagnostics> diagnostics_;
};

} // namespace jobs
