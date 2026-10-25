#include "../include/JobTracker.h"
#include <cstdio>
#include <string>
#include <windows.h>

int RunJobSkillsImportTests() {
    int checks = 0, failures = 0;
    auto check = [&](bool value, const char* text) { ++checks; if (!value) { ++failures; std::printf("[FAIL] JobSkillsImport: %s\n", text); } };
    const std::string path = "build/jobskills-import-test-" + std::to_string(GetCurrentProcessId()) + ".json";
    DeleteFileA(path.c_str());
    auto registry = [](jobs::JobRegistry::Snapshot& snapshot) {
        snapshot.version = 1;
        jobs::JobDefinition first; first.id = "wood"; first.name = "Holzfaeller"; first.enabled = true;
        jobs::JobDefinition second; second.id = "build"; second.name = "Bauarbeiter"; second.enabled = true;
        snapshot.jobs = {first, second};
        for (int index = 0; index < 15; ++index) {
            jobs::JobDefinition filler;
            filler.id = "other" + std::to_string(index);
            filler.name = "Other " + std::to_string(index);
            filler.enabled = true;
            snapshot.jobs.push_back(filler);
        }
        return true;
    };
    {
        jobs::JobTracker tracker(registry, path);
        check(tracker.Initialize(), "initialise");
        check(tracker.AddManualEarningCorrection("wood", 400), "seed money");
        tracker.ApplyJobSummaryAdjustments("wood", 25, 88);
        check(tracker.ImportSynchronizedSkills({{"wood", 160}, {"build", 0}}), "import absolute server points including zero");
        check(tracker.GetAdjustedHighestSkill("wood") == 160 && tracker.GetAdjustedHighestSkill("build") == 0, "absolute current values");
        check(tracker.GetAggregates("wood").lifetime == 425, "earnings and offset preserved");
        const auto count = tracker.GetEvents().size();
        check(tracker.ImportSynchronizedSkills({{"wood", 160}, {"build", 0}}), "repeat snapshot");
        check(tracker.GetEvents().size() == count, "repeat does not duplicate feed");
        check(tracker.ImportSynchronizedSkills({{"wood", 161}}), "new server value");
        check(tracker.GetEvents().size() == count + 1 && tracker.GetAdjustedHighestSkill("wood") == 161, "one absolute feed entry");
        check(tracker.ImportSynchronizedSkills({{"wood", 100}}), "lower authoritative value");
        check(tracker.GetAdjustedHighestSkill("wood") == 100, "server value replaces local progress");
        const auto stable = tracker.GetEvents().size();
        check(!tracker.ImportSynchronizedSkills({{"wood", 999}, {"unknown", 4}}), "unknown job rejects whole batch");
        check(tracker.GetEvents().size() == stable && tracker.GetAdjustedHighestSkill("wood") == 100, "no partial mutation");
        check(!tracker.ImportSynchronizedSkills({{"build", -1}}), "negative points rejected");
        tracker.Shutdown();
    }
    {
        jobs::JobTracker restored(registry, path);
        check(restored.Initialize(), "reload");
        check(restored.GetAdjustedHighestSkill("wood") == 100 && restored.GetAdjustedHighestSkill("build") == 0, "saved absolute values");
        check(restored.GetAggregates("wood").lifetime == 425, "saved income preserved");
        restored.Shutdown();
    }
    DeleteFileA(path.c_str());
    std::printf("JobSkillsImportTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
