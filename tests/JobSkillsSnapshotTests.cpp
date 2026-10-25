#include "../include/JobSkillsSnapshot.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] JobSkills: %s (line %d)\n", #value, __LINE__); return; } } while (0)

jobs::JobRegistry::Snapshot Registry() {
    const std::vector<std::pair<std::string, std::string>> definitions = {
        {"frachtpilot", "Frachtpilot"}, {"flughafenmitarbeiter", "Flughafenmitarbeiter"},
        {"hochseefischer", "Hochseefischer"}, {"pilot", "Pilot"}, {"bergarbeiter", "Bergarbeiter"},
        {"beachpatrol", "Beachpatrol"}, {"abschleppfahrer", "Abschleppfahrer"},
        {"strassenreiniger", "Stra\xC3\x9F" "enreiniger"}, {"landwirt", "Landwirt"}, {"bestatter", "Bestatter"},
        {"winterdienst", "Winterdienst"}, {"taxifahrer", "Taxifahrer"}, {"jaeger", "J\xC3\xA4ger"},
        {"busfahrer", "Busfahrer"}, {"kurier", "Kurier"}, {"holzfaeller", "Holzf\xC3\xA4ller"}, {"bauarbeiter", "Bauarbeiter"},
    };
    jobs::JobRegistry::Snapshot registry;
    registry.version = 3;
    for (const auto& item : definitions) {
        jobs::JobDefinition job;
        job.id = item.first;
        job.name = item.second;
        job.enabled = true;
        registry.jobs.push_back(job);
    }
    return registry;
}

std::string Table(const jobs::JobRegistry::Snapshot& registry) {
    std::string body = "{AABBCC}T\xC3\xA4tigkeit\t{FFFFFF}Stufe\n";
    int value = 0;
    for (const auto& job : registry.jobs) body += "{FFFFFF}" + job.name + "\t" + std::to_string(value++ * 100) + "\n";
    for (const char* unknown : {"Security Guard", "Trucker", "Erntehelfer", "Mueller", "Unbekannt A", "Unbekannt B"})
        body += std::string(unknown) + "\t0\n";
    return body;
}

void TestObservedTableImportsKnownJobsIncludingZeroAndIgnoresUnknownActivities() {
    const auto registry = Registry();
    jobs::JobSkillsSnapshot snapshot;
    std::string error;
    CHECK(jobs::ParseJobSkillsDialog("Jobs \xC2\xBB Skill\xC3\xBC" "bersicht", Table(registry), registry, true, snapshot, error));
    CHECK(error.empty());
    CHECK(snapshot.totalRows == 23 && snapshot.unknownRows == 6 && snapshot.points.size() == 17);
    CHECK(snapshot.points.at("frachtpilot") == 0);
    CHECK(snapshot.points.at("holzfaeller") == 1500);
    CHECK(snapshot.points.at("bauarbeiter") == 1600);
    CHECK(snapshot.points.find("trucker") == snapshot.points.end());
    for (std::size_t index = 0; index < registry.jobs.size(); ++index)
        CHECK(snapshot.points.at(registry.jobs[index].id) == static_cast<int>(index) * 100);
}

void TestRegistryNamesAndIdsResolveWithoutScreenPositionOrSpellingDependence() {
    auto registry = Registry();
    jobs::JobSkillsSnapshot snapshot;
    std::string error;
    const std::string body = "Taetigkeit\tStufe\r\nHolzfaeller\t1.234\r\nBAU-ARBEITER\t0\r\n"
        "STRASSENREINIGER\t200\r\nBeach Patrol\t300\r\nJaeger\t400";
    CHECK(jobs::ParseJobSkillsDialog("{112233}Jobs » Skillübersicht", body, registry, true, snapshot, error));
    CHECK(snapshot.points.size() == 5);
    CHECK(snapshot.points.at("holzfaeller") == 1234 && snapshot.points.at("bauarbeiter") == 0);
    CHECK(snapshot.points.at("strassenreiniger") == 200 && snapshot.points.at("beachpatrol") == 300 && snapshot.points.at("jaeger") == 400);
    // IDs are supplied by the registry, never hardcoded in the parser.
    registry.jobs.back().id = "server_building_job";
    CHECK(jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", "Tätigkeit\tStufe\nBauarbeiter\t57", registry, true, snapshot, error));
    CHECK(snapshot.points.at("server_building_job") == 57);
    CHECK(snapshot.points.find("bauarbeiter") == snapshot.points.end());
}

void TestUntrustedWrongMalformedAndDuplicateResponsesCannotReplaceSnapshot() {
    const auto registry = Registry();
    jobs::JobSkillsSnapshot snapshot;
    snapshot.points["sentinel"] = 99;
    std::string error;
    CHECK(!jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", Table(registry), registry, false, snapshot, error));
    CHECK(!jobs::ParseJobSkillsDialog("Statistiken » Other_Player", Table(registry), registry, true, snapshot, error));
    for (const char* body : {"Bauarbeiter\t57", "Job\tPunkte\nBauarbeiter\t57", "Tätigkeit\tStufe\nBauarbeiter\t-1",
        "Tätigkeit\tStufe\nBauarbeiter\t2147483648", "Tätigkeit\tStufe\nBauarbeiter\t1.23",
        "Tätigkeit\tStufe\nBauarbeiter\t57\nBAU-ARBEITER\t58", "Tätigkeit\tStufe\nBauarbeiter\t57\textra",
        "Tätigkeit\tStufe\nOnly Unknown Activity\t57", "Tätigkeit\tStufe\nBauarbeiter 57"}) {
        CHECK(!jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", body, registry, true, snapshot, error));
        CHECK(!error.empty());
        CHECK(snapshot.points.size() == 1 && snapshot.points.at("sentinel") == 99);
    }
    std::string oversized = "Tätigkeit\tStufe\nBauarbeiter\t57";
    for (int index = 0; index < 130; ++index) oversized += "\nUnknown\t0";
    CHECK(!jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", oversized, registry, true, snapshot, error));
    CHECK(!jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", std::string(65537, 'x'), registry, true, snapshot, error));
    std::string nulBody = "Tätigkeit\tStufe\nBauarbeiter\t57";
    nulBody.push_back('\0');
    nulBody += "bad";
    CHECK(!jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", nulBody, registry, true, snapshot, error));
    auto ambiguous = registry;
    ambiguous.jobs.push_back(ambiguous.jobs.back());
    ambiguous.jobs.back().id = "different_builder";
    CHECK(!jobs::ParseJobSkillsDialog("Jobs » Skillübersicht", Table(registry), ambiguous, true, snapshot, error));
}
} // namespace

int RunJobSkillsSnapshotTests() {
    checks = failures = 0;
    TestObservedTableImportsKnownJobsIncludingZeroAndIgnoresUnknownActivities();
    TestRegistryNamesAndIdsResolveWithoutScreenPositionOrSpellingDependence();
    TestUntrustedWrongMalformedAndDuplicateResponsesCannotReplaceSnapshot();
    std::printf("JobSkillsSnapshotTests: %d checks, %d failures\n", checks, failures);
    return failures;
}

#ifdef JOB_SKILLS_TEST_MAIN
int main() { return RunJobSkillsSnapshotTests(); }
#endif
