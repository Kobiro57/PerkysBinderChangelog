#include "../include/JobRegistry.h"
#include "../include/JobsTypes.h"

#include <cstdio>
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

bool TestEmbeddedRegistryLoadsLandwirt() {
    jobs::JobRegistry registry;
    jobs::JobRegistry::Snapshot snapshot;

    ASSERT_TRUE(registry.LoadEmbedded(snapshot));
    ASSERT_EQ(snapshot.version, 3);
    ASSERT_EQ(snapshot.jobs.size(), jobs::kExpectedJobDefinitionCount);

    const jobs::JobDefinition* landwirt = nullptr;
    for (const jobs::JobDefinition& definition : snapshot.jobs) {
        if (definition.id == "landwirt") {
            landwirt = &definition;
            break;
        }
    }

    ASSERT_TRUE(landwirt != nullptr);
    ASSERT_EQ(landwirt->name, std::string("Landwirt"));
    ASSERT_TRUE(landwirt->enabled);
    ASSERT_EQ(landwirt->parserRules.size(), static_cast<std::size_t>(4));
    ASSERT_EQ(landwirt->skillTree.size(), static_cast<std::size_t>(4));
    ASSERT_TRUE(!landwirt->parserRules[0].pattern.empty());
    ASSERT_TRUE(!landwirt->parserRules[1].pattern.empty());
    ASSERT_EQ(landwirt->parserRules[0].eventType, std::string("job_message"));
    ASSERT_EQ(landwirt->parserRules[1].eventType, std::string("job_message"));
    const auto amountCapture = landwirt->parserRules[2].captureGroups.find("amount");
    const auto levelCapture = landwirt->parserRules[3].captureGroups.find("level");
    ASSERT_TRUE(amountCapture != landwirt->parserRules[2].captureGroups.end());
    ASSERT_TRUE(levelCapture != landwirt->parserRules[3].captureGroups.end());
    ASSERT_EQ(amountCapture->second, 1);
    ASSERT_EQ(levelCapture->second, 1);
    return true;
}

bool TestEmbeddedRegistryLoadsGatheredMessageSupport() {
    jobs::JobRegistry registry;
    jobs::JobRegistry::Snapshot snapshot;

    ASSERT_TRUE(registry.LoadEmbedded(snapshot));

    const jobs::JobDefinition* flughafen = nullptr;
    const jobs::JobDefinition* strassenreiniger = nullptr;
    const jobs::JobDefinition* jaeger = nullptr;
    for (const jobs::JobDefinition& definition : snapshot.jobs) {
        if (definition.id == "flughafenmitarbeiter") {
            flughafen = &definition;
        } else if (definition.id == "strassenreiniger") {
            strassenreiniger = &definition;
        } else if (definition.id == "jaeger") {
            jaeger = &definition;
        }
    }

    ASSERT_TRUE(flughafen != nullptr);
    ASSERT_TRUE(strassenreiniger != nullptr);
    ASSERT_TRUE(jaeger != nullptr);

    ASSERT_EQ(flughafen->speakerAliases.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(flughafen->speakerAliases[0], std::string("Jason Bennett"));
    ASSERT_EQ(flughafen->parserRules[0].eventType, std::string("job_message"));

    ASSERT_EQ(strassenreiniger->speakerAliases.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(strassenreiniger->speakerAliases[0], std::string("George Carter sagt"));
    ASSERT_EQ(strassenreiniger->parserRules[0].eventType, std::string("job_message"));

    ASSERT_EQ(jaeger->speakerAliases.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(jaeger->speakerAliases[0], std::string("Jack Hunter sagt"));
    ASSERT_EQ(jaeger->parserRules.size(), static_cast<std::size_t>(5));
    ASSERT_EQ(jaeger->parserRules[0].eventType, std::string("job_message"));
    ASSERT_EQ(jaeger->parserRules[1].eventType, std::string("job_message"));
    ASSERT_EQ(jaeger->parserRules[2].eventType, std::string("job_message"));
    return true;
}

bool TestEmbeddedRegistryLoadsHolzfaellerAndBauarbeiter() {
    jobs::JobRegistry registry;
    jobs::JobRegistry::Snapshot snapshot;

    ASSERT_TRUE(registry.LoadEmbedded(snapshot));

    const jobs::JobDefinition* holzfaeller = nullptr;
    const jobs::JobDefinition* bauarbeiter = nullptr;
    for (const jobs::JobDefinition& definition : snapshot.jobs) {
        if (definition.id == "holzfaeller") {
            holzfaeller = &definition;
        } else if (definition.id == "bauarbeiter") {
            bauarbeiter = &definition;
        }
    }

    ASSERT_TRUE(holzfaeller != nullptr);
    ASSERT_TRUE(bauarbeiter != nullptr);
    ASSERT_EQ(holzfaeller->name, std::string("Holzfäller"));
    ASSERT_EQ(bauarbeiter->name, std::string("Bauarbeiter"));
    ASSERT_TRUE(holzfaeller->enabled);
    ASSERT_TRUE(bauarbeiter->enabled);
    ASSERT_EQ(holzfaeller->speakerAliases.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(holzfaeller->speakerAliases[0], std::string("Holzfaeller"));
    ASSERT_EQ(holzfaeller->speakerAliases[1], std::string("Vince Redwood"));
    ASSERT_EQ(holzfaeller->parserRules.size(), static_cast<std::size_t>(5));
    ASSERT_EQ(bauarbeiter->parserRules.size(), static_cast<std::size_t>(2));

    for (const jobs::JobDefinition* definition : {holzfaeller, bauarbeiter}) {
        ASSERT_EQ(definition->parserRules[0].eventType, std::string("earning"));
        ASSERT_EQ(definition->parserRules[0].captureGroups.at("amount"), 1);
        ASSERT_EQ(definition->parserRules[1].eventType, std::string("skill_level_absolute"));
        ASSERT_EQ(definition->parserRules[1].captureGroups.at("level"), 1);
        ASSERT_EQ(definition->skillTree.size(), static_cast<std::size_t>(6));
        for (std::size_t i = 0; i < definition->skillTree.size(); ++i) {
            ASSERT_EQ(definition->skillTree[i].unlockOrder, static_cast<int>(i + 1));
        }
    }

    const int holzfaellerThresholds[] = {50, 100, 250, 500, 1000, 1500};
    const char* holzfaellerRewards[] = {
        "5 Bäume fällen", "Holzfäller Skin freischalten", "Privatfahrzeug: Bobcat",
        "+10% Lohn", "+25% Lohn", "Job-Achievement freigeschaltet"
    };
    const int bauarbeiterThresholds[] = {25, 100, 500, 750, 1000, 1500};
    const char* bauarbeiterRewards[] = {
        "Großbaustellen werden freigeschaltet", "Bauarbeiter Skin freischalten", "+10% Lohn",
        "Business: Blueberry Construction", "+25% Lohn", "Job-Achievement freigeschaltet"
    };
    for (std::size_t i = 0; i < 6; ++i) {
        ASSERT_EQ(holzfaeller->skillTree[i].maxPoints, holzfaellerThresholds[i]);
        ASSERT_EQ(holzfaeller->skillTree[i].name, std::string(holzfaellerRewards[i]));
        ASSERT_EQ(bauarbeiter->skillTree[i].maxPoints, bauarbeiterThresholds[i]);
        ASSERT_EQ(bauarbeiter->skillTree[i].name, std::string(bauarbeiterRewards[i]));
    }
    ASSERT_EQ(holzfaeller->skillTree[2].perkValue, std::string("Bobcat"));
    ASSERT_EQ(holzfaeller->skillTree[3].perkValue, std::string("10%"));
    ASSERT_EQ(holzfaeller->skillTree[4].perkValue, std::string("25%"));
    ASSERT_EQ(bauarbeiter->skillTree[2].perkValue, std::string("10%"));
    ASSERT_EQ(bauarbeiter->skillTree[3].perkValue, std::string("Business: Blueberry Construction"));
    ASSERT_EQ(bauarbeiter->skillTree[4].perkValue, std::string("25%"));
    return true;
}

bool TestEmbeddedRegistryDecodesUnicodeEscapesInPatterns() {
    jobs::JobRegistry registry;
    jobs::JobRegistry::Snapshot snapshot;

    ASSERT_TRUE(registry.LoadEmbedded(snapshot));

    const jobs::JobDefinition* landwirt = nullptr;
    for (const jobs::JobDefinition& definition : snapshot.jobs) {
        if (definition.id == "landwirt") {
            landwirt = &definition;
            break;
        }
    }

    ASSERT_TRUE(landwirt != nullptr);
    ASSERT_TRUE(landwirt->parserRules.size() >= static_cast<std::size_t>(3));

    const std::string& earningPattern = landwirt->parserRules[2].pattern;
    ASSERT_TRUE(earningPattern.find("Für") != std::string::npos);
    ASSERT_TRUE(earningPattern.find("Tätigkeit") != std::string::npos);
    ASSERT_TRUE(earningPattern.find("erhältst") != std::string::npos);
    ASSERT_TRUE(earningPattern.find("nächsten") != std::string::npos);
    ASSERT_TRUE(earningPattern.find("\\u00") == std::string::npos);
    return true;
}

bool TestInvalidRegistryFailsSafely() {
    const std::string malformedJson = R"json({
        "version": 1,
        "jobs": [
            {
                "id": "landwirt",
                "name": "Landwirt",
                "enabled": true,
                "icon": "tractor",
                "parserRules": [
                    { "pattern": "broken", "eventType": "harvest", "captureGroups": { "amount": "oops" } }
                ],
                "skillTree": []
            }
        ]
    })json";

    jobs::JobRegistry registry;
    jobs::JobRegistry::Snapshot snapshot;

    ASSERT_TRUE(!registry.LoadFromJson(malformedJson, snapshot));
    ASSERT_EQ(snapshot.version, 0);
    ASSERT_TRUE(snapshot.jobs.empty());
    return true;
}

} // namespace

int RunJobRegistryTests() {
    g_state = {};

    RUN_TEST(TestEmbeddedRegistryLoadsLandwirt);
    RUN_TEST(TestEmbeddedRegistryLoadsGatheredMessageSupport);
    RUN_TEST(TestEmbeddedRegistryLoadsHolzfaellerAndBauarbeiter);
    RUN_TEST(TestEmbeddedRegistryDecodesUnicodeEscapesInPatterns);
    RUN_TEST(TestInvalidRegistryFailsSafely);

    std::printf("JobRegistryTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
