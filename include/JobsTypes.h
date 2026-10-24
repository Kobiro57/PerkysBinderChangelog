#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace jobs {

using JobId = std::string;

constexpr std::size_t kExpectedJobDefinitionCount = 15;

struct JobEvent {
    std::string id;
    JobId jobId;
    std::string type;
    int amount;
    int level;
    std::string source;
    std::time_t timestamp;
    std::string dayKey;
    std::string note;
    bool deleted;

    JobEvent()
        : amount(0)
        , level(0)
        , timestamp(0)
        , deleted(false) {}
};

struct JobStats {
    JobId jobId;
    int totalEvents;
    int totalAmount;
    int highestLevel;
    std::time_t lastEventTimestamp;
    std::time_t lastSkillTimestamp;
    std::string lastDayKey;

    JobStats()
        : totalEvents(0)
        , totalAmount(0)
        , highestLevel(0)
        , lastEventTimestamp(0)
        , lastSkillTimestamp(0) {}
};

struct ParserRule {
    std::string pattern;
    std::string eventType;
    std::unordered_map<std::string, int> captureGroups;
};

struct SkillTreeNode {
    std::string id;
    std::string name;
    int maxPoints;
    int unlockOrder;
    std::string perkType;   // e.g., "skin", "lohn_bonus", "privatfahrzeug", "effizienz", "job_freigeschaltet", "spezial_mission", "dienstfahrzeug", "job_erweiterung", "fast_travel"
    std::string perkValue;  // e.g., "268", "25%", "Leviathan", "10 Pakete gleichzeitig", "Pilot"

    SkillTreeNode()
        : maxPoints(0)
        , unlockOrder(0) {}
};

struct JobDefinition {
    JobId id;
    std::string name;
    bool enabled;
    std::string icon;
    std::vector<std::string> speakerAliases;
    std::vector<ParserRule> parserRules;
    std::vector<SkillTreeNode> skillTree;

    JobDefinition()
        : enabled(false) {}
};

struct JobSummaryAdjustment {
    JobId jobId;
    int earningsOffset;
    int skillOffset;

    JobSummaryAdjustment()
        : earningsOffset(0)
        , skillOffset(0) {}
};

struct JobEditSnapshot {
    JobId jobId;
    int baseTotalEarnings;
    int baseHighestSkill;
    int earningsOffset;
    int skillOffset;
    int adjustedTotalEarnings;
    int adjustedHighestSkill;

    JobEditSnapshot()
        : baseTotalEarnings(0)
        , baseHighestSkill(0)
        , earningsOffset(0)
        , skillOffset(0)
        , adjustedTotalEarnings(0)
        , adjustedHighestSkill(0) {}
};

} // namespace jobs
