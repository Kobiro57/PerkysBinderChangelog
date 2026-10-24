#define KEYBINDER_TESTING

#include "../include/JobParser.h"

#include <cstdio>
#include <ctime>
#include <string>

namespace {

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

TestState g_state;

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        g_state.error = "Assertion failed: " #condition; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define ASSERT_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        g_state.error = "Assertion failed: " #actual " == " #expected; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

jobs::ParserRule BuildRule(const std::string& pattern,
                          const std::string& eventType,
                          const char* captureName = nullptr,
                          int captureIndex = 0) {
    jobs::ParserRule rule;
    rule.pattern = pattern;
    rule.eventType = eventType;
    if (captureName != nullptr && captureIndex > 0) {
        rule.captureGroups[captureName] = captureIndex;
    }
    return rule;
}

jobs::JobDefinition BuildJob(const std::string& id,
                             const std::string& name,
                             const std::vector<std::string>& aliases,
                             std::initializer_list<jobs::ParserRule> rules) {
    jobs::JobDefinition definition;
    definition.id = id;
    definition.name = name;
    definition.enabled = true;
    definition.icon = "work";
    definition.speakerAliases = aliases;
    definition.parserRules.assign(rules.begin(), rules.end());
    return definition;
}

jobs::JobRegistry::Snapshot BuildSnapshot() {
    jobs::JobRegistry::Snapshot snapshot;
    snapshot.version = 1;

    snapshot.jobs.push_back(BuildJob(
        "landwirt",
        "Landwirt",
        {},
        {
            BuildRule("^Die Kühe müssen gemolken werden\\. Begib dich zu den Kühen und melke sie\\.$", "job_message"),
            BuildRule("^Ein Feld muss mit neuem Weizen besäht werden\\.$", "job_message"),
            BuildRule("^Für das erfolgreiche Beenden deiner Tätigkeit erhältst du (\\d+)\\$ zum nächsten Zahltag\\.$", "earning", "amount", 1),
            BuildRule("^Du hast deinen Job-Skill auf (\\d+) verbessert\\.$", "skill_level_absolute", "level", 1)
        }));

    snapshot.jobs.push_back(BuildJob(
        "jaeger",
        "Jäger",
        {"Jack Hunter sagt"},
        {
            BuildRule("^Hey .+! Ich freue mich, dass du uns als Jäger unterstützt, um die Wildpopulation zu kontrollieren\\.$", "job_message"),
            BuildRule("^Deine Ausrüstung ist bereits vorbereitet, um vier Wildtiere zu erlegen\\.$", "job_message"),
            BuildRule("^Du hast \\d+ Kg Fleisch von dem toten Tier aufgesammelt$", "job_message"),
            BuildRule("^Für das erfolgreiche Beenden deiner Tätigkeit erhältst du (\\d+)\\$ zum nächsten Zahltag\\.$", "earning", "amount", 1),
            BuildRule("^Du hast deinen Job-Skill auf (\\d+) verbessert\\.$", "skill_level_absolute", "level", 1)
        }));

    snapshot.jobs.push_back(BuildJob(
        "strassenreiniger",
        "Straßenreiniger",
        {"George Carter sagt"},
        {
            BuildRule("^Hey .+! Ich freue mich, dass du die Sanitary Andreas Corp\\. als Straßenreiniger unterstützt\\.$", "job_message"),
            BuildRule("^Für das erfolgreiche Beenden deiner Tätigkeit erhältst du (\\d+)\\$ zum nächsten Zahltag\\.$", "earning", "amount", 1),
            BuildRule("^Du hast deinen Job-Skill auf (\\d+) verbessert\\.$", "skill_level_absolute", "level", 1)
        }));

    snapshot.jobs.push_back(BuildJob(
        "flughafenmitarbeiter",
        "Flughafenmitarbeiter",
        {"Jason Bennett"},
        {
            BuildRule("^Hallo .+\\. Ich freue mich, dass du die San Andreas Airport Service tatkräftig unterstützt\\.$", "job_message"),
            BuildRule("^Für das erfolgreiche Beenden deiner Tätigkeit erhältst du (\\d+)\\$ zum nächsten Zahltag\\.$", "earning", "amount", 1),
            BuildRule("^Du hast deinen Job-Skill auf (\\d+) verbessert\\.$", "skill_level_absolute", "level", 1)
        }));

    return snapshot;
}

jobs::JobChatEvent BuildChatEvent(const std::string& line, const std::string& prefix) {
    jobs::JobChatEvent event;
    event.rawLine = line;
    event.timestampFreeLine = line;
    event.normalizedLine = line;
    event.jobPrefix = prefix;
    event.arrivalTime = static_cast<std::time_t>(1713045600);
    return event;
}

bool TestParsesCompletionRulesForConfiguredJobs() {
    jobs::JobParser parser;
    const jobs::JobRegistry::Snapshot snapshot = BuildSnapshot();

    const auto landwirtEarning = parser.Parse(
        BuildChatEvent("Landwirt: Für das erfolgreiche Beenden deiner Tätigkeit erhältst du 115$ zum nächsten Zahltag.", "Landwirt"),
        snapshot);
    ASSERT_EQ(landwirtEarning.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(landwirtEarning[0].jobId, std::string("landwirt"));
    ASSERT_EQ(landwirtEarning[0].type, std::string("earning"));
    ASSERT_EQ(landwirtEarning[0].amount, 115);

    const auto jaegerSkill = parser.Parse(
        BuildChatEvent("Jäger: Du hast deinen Job-Skill auf 1 verbessert.", "Jäger"),
        snapshot);
    ASSERT_EQ(jaegerSkill.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(jaegerSkill[0].jobId, std::string("jaeger"));
    ASSERT_EQ(jaegerSkill[0].type, std::string("skill_level_absolute"));
    ASSERT_EQ(jaegerSkill[0].level, 1);

    const auto strassenreinigerEarning = parser.Parse(
        BuildChatEvent("Straßenreiniger: Für das erfolgreiche Beenden deiner Tätigkeit erhältst du 317$ zum nächsten Zahltag.", "Straßenreiniger"),
        snapshot);
    ASSERT_EQ(strassenreinigerEarning.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(strassenreinigerEarning[0].jobId, std::string("strassenreiniger"));
    ASSERT_EQ(strassenreinigerEarning[0].amount, 317);

    const auto flughafenSkill = parser.Parse(
        BuildChatEvent("Flughafenmitarbeiter: Du hast deinen Job-Skill auf 1 verbessert.", "Flughafenmitarbeiter"),
        snapshot);
    ASSERT_EQ(flughafenSkill.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(flughafenSkill[0].jobId, std::string("flughafenmitarbeiter"));
    ASSERT_EQ(flughafenSkill[0].level, 1);

    return true;
}

bool TestParsesConfiguredStartAndProgressLinesAsVisibleEvents() {
    jobs::JobParser parser;
    const jobs::JobRegistry::Snapshot snapshot = BuildSnapshot();

    const auto landwirtStart = parser.Parse(
        BuildChatEvent("Landwirt: Die Kühe müssen gemolken werden. Begib dich zu den Kühen und melke sie.", "Landwirt"),
        snapshot);
    ASSERT_EQ(landwirtStart.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(landwirtStart[0].type, std::string("job_message"));
    ASSERT_EQ(landwirtStart[0].note, std::string("Die Kühe müssen gemolken werden. Begib dich zu den Kühen und melke sie."));

    const auto jaegerIntro = parser.Parse(
        BuildChatEvent("Jack Hunter sagt: Hey perkys! Ich freue mich, dass du uns als Jäger unterstützt, um die Wildpopulation zu kontrollieren.", "Jack Hunter sagt"),
        snapshot);
    ASSERT_EQ(jaegerIntro.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(jaegerIntro[0].jobId, std::string("jaeger"));
    ASSERT_EQ(jaegerIntro[0].type, std::string("job_message"));

    const auto jaegerPrepared = parser.Parse(
        BuildChatEvent("Jack Hunter sagt: Deine Ausrüstung ist bereits vorbereitet, um vier Wildtiere zu erlegen.", "Jack Hunter sagt"),
        snapshot);
    ASSERT_EQ(jaegerPrepared.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(jaegerPrepared[0].type, std::string("job_message"));

    const auto jaegerPickup = parser.Parse(
        BuildChatEvent("Jäger: Du hast 10 Kg Fleisch von dem toten Tier aufgesammelt", "Jäger"),
        snapshot);
    ASSERT_EQ(jaegerPickup.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(jaegerPickup[0].type, std::string("job_message"));

    const auto strassenreinigerIntro = parser.Parse(
        BuildChatEvent("George Carter sagt: Hey perkys! Ich freue mich, dass du die Sanitary Andreas Corp. als Straßenreiniger unterstützt.", "George Carter sagt"),
        snapshot);
    ASSERT_EQ(strassenreinigerIntro.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(strassenreinigerIntro[0].jobId, std::string("strassenreiniger"));

    const auto flughafenIntro = parser.Parse(
        BuildChatEvent("Jason Bennett: Hallo perkys. Ich freue mich, dass du die San Andreas Airport Service tatkräftig unterstützt.", "Jason Bennett"),
        snapshot);
    ASSERT_EQ(flughafenIntro.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(flughafenIntro[0].jobId, std::string("flughafenmitarbeiter"));

    ASSERT_TRUE(parser.Parse(
        BuildChatEvent("INFO: Du kannst deine Tätigkeit mit /stopjob vorzeitig beenden. Beachte, dass du keine Erfahrungspunkte und keinen Lohn erhältst.", ""),
        snapshot).empty());

    ASSERT_EQ(parser.GetUnmatchedCount("landwirt"), 0);
    ASSERT_EQ(parser.GetUnmatchedCount("jaeger"), 0);
    ASSERT_EQ(parser.GetUnmatchedCount("strassenreiniger"), 0);
    ASSERT_EQ(parser.GetUnmatchedCount("flughafenmitarbeiter"), 0);
    return true;
}

bool TestTracksUnknownConfiguredLine() {
    jobs::JobParser parser;
    const jobs::JobRegistry::Snapshot snapshot = BuildSnapshot();

    ASSERT_TRUE(parser.Parse(
        BuildChatEvent("Jäger: Unbekannte Nachricht ohne Treffer.", "Jäger"),
        snapshot).empty());
    ASSERT_EQ(parser.GetUnmatchedCount("jaeger"), 1);

    const auto& diagnostics = parser.GetDiagnostics();
    const auto it = diagnostics.find("jaeger");
    ASSERT_TRUE(it != diagnostics.end());
    ASSERT_EQ(it->second.lastUnmatchedLine, std::string("Jäger: Unbekannte Nachricht ohne Treffer."));
    return true;
}

} // namespace

int RunJobParserTests() {
    g_state = {};

    RUN_TEST(TestParsesCompletionRulesForConfiguredJobs);
    RUN_TEST(TestParsesConfiguredStartAndProgressLinesAsVisibleEvents);
    RUN_TEST(TestTracksUnknownConfiguredLine);

    std::printf("JobParserTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
