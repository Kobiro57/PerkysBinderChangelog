#include "../include/ServerStatsSnapshot.h"

#include <cstdio>
#include <limits>
#include <string>

namespace {
int checks = 0;
int failures = 0;
#define STATS_CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] ServerStatsParser line %d: %s\n", __LINE__, #value); } } while (0)

void TestOwnChatBlockCollectsAllRows() {
    keybinder::ServerStatsCollector collector;
    const std::string name = "[TEST]Sample_Player";
    STATS_CHECK(!collector.ConsumeLine("Kills: [999] Tode: [999]", name, 1000));
    STATS_CHECK(!collector.Snapshot().HasCounters());
    STATS_CHECK(collector.ConsumeLine("{FFCC00}*** [TEST]Sample_Player ***", "[test]sample_player", 1000));
    STATS_CHECK(collector.IsOwnContext());
    STATS_CHECK(collector.ConsumeLine("Level:[12] Erfahrungspunkte [345/1200] Geschlecht: [Männlich] Alter: [4] Bargeld: [12.345$] Kontostand: [67.890$]", name, 1010));
    STATS_CHECK(!collector.Snapshot().HasCounters());
    STATS_CHECK(collector.ConsumeLine("Kills: [41] Tode: [19] Fraktion: [Testfraktion] Gang: [-] Nächster Zahltag in: [37 Min] Gold: [8] Eventcoins: [3]", name, 1020));
    STATS_CHECK(collector.Snapshot().HasCore());
    STATS_CHECK(collector.IsOwnContext());
    STATS_CHECK(collector.ConsumeLine("Wantedpunkte: [2] Offene Rechnungen: [Nein]", name, 1030));
    STATS_CHECK(collector.ConsumeLine("Führerscheine: [Auto, Boot]", name, 1040));
    const auto& snapshot = collector.Snapshot();
    STATS_CHECK(snapshot.playerName == name);
    STATS_CHECK(snapshot.kills == 41);
    STATS_CHECK(snapshot.deaths == 19);
    STATS_CHECK(snapshot.cash == 12345);
    STATS_CHECK(snapshot.bank == 67890);
    STATS_CHECK(snapshot.GetAvailableWealth() == 80235);
    STATS_CHECK(snapshot.level == 12);
    STATS_CHECK(snapshot.experience == 345);
    STATS_CHECK(snapshot.nextLevelExperience == 1200);
    STATS_CHECK(snapshot.nextPaydayMinutes == 37);
    STATS_CHECK(snapshot.gold == 8);
    STATS_CHECK(snapshot.eventcoins == 3);
    STATS_CHECK(snapshot.wantedPoints == 2);
}

void TestForeignPlayerOrOrdinaryChatCannotSupplyValues() {
    keybinder::ServerStatsCollector collector;
    STATS_CHECK(!collector.ConsumeLine("*** Other_Player ***", "Own_Player", 1000));
    STATS_CHECK(!collector.IsOwnContext());
    STATS_CHECK(!collector.ConsumeLine("Kills: [100] Tode: [200]", "Own_Player", 1010));
    STATS_CHECK(!collector.Snapshot().kills);
    STATS_CHECK(collector.ConsumeLine("*** Own_Player ***", "Own_Player", 1020));
    STATS_CHECK(!collector.ConsumeLine("Other_Player sagt: Kills: [900] Tode: [800]", "Own_Player", 1030));
    STATS_CHECK(!collector.Snapshot().HasCounters());
    STATS_CHECK(!collector.ConsumeLine("Level: Ich habe Kills: [777]", "Own_Player", 1040));
    STATS_CHECK(!collector.Snapshot().kills);
    STATS_CHECK(collector.ConsumeLine("Kills: [11]", "Own_Player", 1050));
    STATS_CHECK(!collector.ConsumeLine("*** Other_Player ***", "Own_Player", 1060));
    STATS_CHECK(!collector.IsOwnContext());
    STATS_CHECK(!collector.Snapshot().kills);
    STATS_CHECK(!collector.ConsumeLine("Tode: [22]", "Own_Player", 1070));
    STATS_CHECK(!collector.Snapshot().deaths);
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", 1080);
    STATS_CHECK(!collector.ConsumeLine("Kills: [11]", "Changed_Player", 1090));
    STATS_CHECK(!collector.IsOwnContext());
    STATS_CHECK(!collector.Snapshot().kills);
}

void TestContextHasBoundedLifetimeAndLineBudget() {
    keybinder::ServerStatsCollector collector;
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", 1000);
    STATS_CHECK(collector.ConsumeLine("Kills: [11]", "Own_Player", 3999));
    STATS_CHECK(!collector.ConsumeLine("Tode: [22]", "Own_Player", 4000));
    STATS_CHECK(!collector.IsOwnContext());
    STATS_CHECK(!collector.Snapshot().deaths);
    collector.Reset();
    STATS_CHECK(!collector.Snapshot().kills);
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", 5000);
    for (int line = 0; line < keybinder::ServerStatsCollector::kMaximumBodyLines; ++line)
        STATS_CHECK(collector.ConsumeLine("", "Own_Player", 5001 + line));
    STATS_CHECK(!collector.IsOwnContext());
    STATS_CHECK(!collector.ConsumeLine("Kills: [99] Tode: [88]", "Own_Player", 5030));
    STATS_CHECK(!collector.Snapshot().HasCounters());
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", (std::numeric_limits<unsigned long>::max)() - 20);
    STATS_CHECK(collector.ConsumeLine("Kills: [11] Tode: [22]", "Own_Player", 10));
    STATS_CHECK(collector.Snapshot().HasCounters());
}

void TestGermanNumbersAndOverflowNeverWrap() {
    keybinder::ServerStatsCollector collector;
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", 1000);
    STATS_CHECK(collector.ConsumeLine("Kills: [2.147.483.647] Tode: [0] Bargeld: [9.223.372.036.854.775.807$] Kontostand: [0$]", "Own_Player", 1010));
    STATS_CHECK(collector.Snapshot().kills == (std::numeric_limits<int>::max)());
    STATS_CHECK(collector.Snapshot().cash == (std::numeric_limits<long long>::max)());
    STATS_CHECK(collector.Snapshot().GetAvailableWealth() == (std::numeric_limits<long long>::max)());
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", 1020);
    collector.ConsumeLine("Kills: [2.147.483.648] Tode: [-1] Bargeld: [9.223.372.036.854.775.808$] Kontostand: [12.34$]", "Own_Player", 1030);
    STATS_CHECK(!collector.Snapshot().kills);
    STATS_CHECK(!collector.Snapshot().deaths);
    STATS_CHECK(!collector.Snapshot().cash);
    STATS_CHECK(!collector.Snapshot().bank);
    collector.ConsumeLine("Bargeld: [-9.223.372.036.854.775.808$] Kontostand: [-1$]", "Own_Player", 1040);
    STATS_CHECK(collector.Snapshot().cash == (std::numeric_limits<long long>::min)());
    STATS_CHECK(!collector.Snapshot().GetAvailableWealth());
    collector.ConsumeLine("Vermögen: [123.456$]", "Own_Player", 1050);
    STATS_CHECK(collector.Snapshot().GetAvailableWealth() == 123456);
    collector.ConsumeLine("*** Own_Player ***", "Own_Player", 1060);
    collector.ConsumeLine("Bargeld: [100$]", "Own_Player", 1070);
    STATS_CHECK(!collector.Snapshot().GetAvailableWealth());
    STATS_CHECK(!collector.Snapshot().HasCore());
    collector.ConsumeLine("Kontostand: [-25$]", "Own_Player", 1080);
    STATS_CHECK(collector.Snapshot().GetAvailableWealth() == 75);
    collector.ConsumeLine("Erfahrungspunkte [1.234/9.876]", "Own_Player", 1090);
    STATS_CHECK(collector.Snapshot().experience == 1234);
    STATS_CHECK(collector.Snapshot().nextLevelExperience == 9876);
    collector.ConsumeLine("Erfahrungspunkte [2147483648/9]", "Own_Player", 1100);
    STATS_CHECK(collector.Snapshot().experience == 1234);
}

void TestDialogRequiresTrustedOwnRequestAndRejectsForeignIdentity() {
    const std::string body = "Level:\t12\tBargeld:\t12.345$\tKontostand:\t67.890$\nKills:\t41\tTode:\t19\nGold:\t8\tEventcoins:\t3";
    keybinder::ServerStatsCollector collector;
    STATS_CHECK(!collector.ConsumeDialog("Statistiken", body, "Own_Player", 1000, false));
    STATS_CHECK(!collector.Snapshot().HasCore());
    STATS_CHECK(collector.ConsumeDialog("{FFCC00}Statistiken", body, "Own_Player", 1000, true));
    STATS_CHECK(collector.Snapshot().HasCore());
    STATS_CHECK(collector.Snapshot().cash == 12345);
    STATS_CHECK(collector.Snapshot().bank == 67890);
    STATS_CHECK(collector.Snapshot().kills == 41);
    STATS_CHECK(collector.Snapshot().deaths == 19);
    STATS_CHECK(collector.Snapshot().gold == 8);
    STATS_CHECK(!collector.ConsumeDialog("Statistiken von Other_Player", body, "Own_Player", 1010, true));
    STATS_CHECK(!collector.ConsumeDialog("Statistiken", "*** Other_Player ***\n" + body, "Own_Player", 1020, true));
    STATS_CHECK(!collector.ConsumeDialog("Statistiken", "Name: [Other_Player]\n" + body, "Own_Player", 1030, true));
    STATS_CHECK(!collector.ConsumeDialog("Inventar", body, "Own_Player", 1040, true));
    STATS_CHECK(collector.ConsumeDialog("Persönliche Übersicht", "*** OWN_PLAYER ***\n" + body, "Own_Player", 1050, true));
    STATS_CHECK(collector.Snapshot().HasCore());
    STATS_CHECK(!collector.ConsumeDialog("*** Other_Player ***", "*** Own_Player ***\n" + body, "Own_Player", 1060, true));
    STATS_CHECK(!collector.ConsumeDialog("Statistiken", "Nur ein Text ohne Werte", "Own_Player", 1070, true));
}

void TestRecognizedRowsDoNotIncludeOrdinaryChat() {
    STATS_CHECK(keybinder::ServerStatsCollector::IsRecognizedStatsLine(""));
    STATS_CHECK(keybinder::ServerStatsCollector::IsRecognizedStatsLine("  {FFFFFF}Kills: [5] Tode: [6]"));
    STATS_CHECK(keybinder::ServerStatsCollector::IsRecognizedStatsLine("Bargeld: [1.234$] Kontostand: [5.678$]"));
    STATS_CHECK(keybinder::ServerStatsCollector::IsRecognizedStatsLine("Führerscheine: [Auto]"));
    STATS_CHECK(keybinder::ServerStatsCollector::IsRecognizedStatsLine("Waffenscheine: A: [Ja] B: [Nein] C: [Nein]"));
    STATS_CHECK(!keybinder::ServerStatsCollector::IsRecognizedStatsLine("Other_Player: Kills: [5]"));
    STATS_CHECK(!keybinder::ServerStatsCollector::IsRecognizedStatsLine("*** Other_Player ***"));
    STATS_CHECK(!keybinder::ServerStatsCollector::IsRecognizedStatsLine("Level: Das ist Spielerchat"));
}
} // namespace

int RunServerStatsParserTests() {
    checks = failures = 0;
    TestOwnChatBlockCollectsAllRows();
    TestForeignPlayerOrOrdinaryChatCannotSupplyValues();
    TestContextHasBoundedLifetimeAndLineBudget();
    TestGermanNumbersAndOverflowNeverWrap();
    TestDialogRequiresTrustedOwnRequestAndRejectsForeignIdentity();
    TestRecognizedRowsDoNotIncludeOrdinaryChat();
    std::printf("ServerStatsParserTests: %d checks, %d failures\n", checks, failures);
    return failures;
}

#ifdef SERVER_STATS_PARSER_TEST_MAIN
int main() { return RunServerStatsParserTests(); }
#endif
