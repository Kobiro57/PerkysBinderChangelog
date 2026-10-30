#include "../include/CombatChatParser.h"
#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] CombatChatParser line %d: %s\n", __LINE__, #value); } } while (0)
std::string Cp1252(const std::string& utf8) {
    int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(count, L'\0'); MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), count);
    count = WideCharToMultiByte(1252, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string bytes(count, '\0'); WideCharToMultiByte(1252, 0, wide.data(), static_cast<int>(wide.size()), bytes.data(), count, nullptr, nullptr);
    return bytes;
}
}

int RunCombatChatParserTests() {
    using namespace killtracker;
    checks = failures = 0;
    for (const std::string local : {"KOBiRO", "[RCB]Nick.Name", "Another_Player"}) {
        const std::vector<std::string> personal = {
            "Du hast ein Verbrechen begangen ( Vorsätzlicher Mord ). Reporter: Anonym",
            "Du hast einen Spieler getötet.",
            "GANGWAR (Red County Brotherhood): Du hast einen Feind ausgeschaltet!"
        };
        for (const auto& line : personal) {
            const auto notice = ParseCombatChat(line, local);
            CHECK(notice.kind == CombatChatKind::Kill && notice.killer == local && !notice.victimKnown);
            CHECK(ParseCombatChat(Cp1252(line), local).kind == CombatChatKind::Kill);
            CHECK(ParseCombatChat("{FFAA00}[12:34:56] {FFFFFF}" + line, local).kind == CombatChatKind::Kill);
            CHECK(ParseCombatChat(line, local, true).kind == CombatChatKind::None);
            CHECK(ParseCombatChat("Someone sagt: " + line, local).kind == CombatChatKind::None);
        }
        const std::vector<std::string> named = {
            "<< " + local + " hat den gesuchten Verbrecher Enemy_One getötet. >>",
            "REVIERKAMPF: " + local + " hat Enemy_One getötet. Neuer Spielstand: Angreifer: 2 - Verteidiger: 3",
            "REVIERKAMPF: " + local + " hat Enemy_One getötet (Teamkill). Neuer Spielstand: Angreifer: 2 - Verteidiger: 3",
            "REVIERKAMPF: " + local + " hat Enemy_One getötet.",
            "STREETWAR: " + local + " hat Enemy_One (Los Santos Police Department) getötet. Aktuelle Kills: 4",
            "DEATHZONE: " + local + " hat Enemy_One mit einer Desert Eagle getötet."
        };
        for (const auto& line : named) {
            const auto notice = ParseCombatChat(line, local);
            CHECK(notice.kind == CombatChatKind::Kill && notice.killer == local && notice.victim == "Enemy_One" && notice.victimKnown);
            CHECK(ParseCombatChat(Cp1252(line), local).kind == CombatChatKind::Kill);
            CHECK(ParseCombatChat(line, "Other").kind == CombatChatKind::None);
            CHECK(ParseCombatChat(line, local, true).kind == CombatChatKind::None);
            CHECK(ParseCombatChat(line, local + "Suffix").kind == CombatChatKind::None);
        }
        CHECK(ParseCombatChat("REVIERKAMPF: " + local + " hat einen Teamkill begangen.", local).kind == CombatChatKind::Kill);
        const std::string killedLocal = "<< Enemy_One hat den gesuchten Verbrecher " + local + " getötet. >>";
        const auto death = ParseCombatChat(killedLocal, local);
        CHECK(death.kind == CombatChatKind::Death && death.killer == "Enemy_One" && death.victim == local);
        CHECK(ParseCombatChat(Cp1252(killedLocal), local).kind == CombatChatKind::Death);
        CHECK(ParseCombatChat(killedLocal, local, true).kind == CombatChatKind::None);
        CHECK(ParseCombatChat("<< Enemy_One hat den gesuchten Verbrecher Other getötet. >>", local).kind == CombatChatKind::None);
        CHECK(ParseCombatChat("<< " + local + "Suffix hat den gesuchten Verbrecher Enemy_One getötet. >>", local).kind == CombatChatKind::None);
        CHECK(ParseCombatChat("<< Enemy_One hat den gesuchten Verbrecher " + local + "Suffix getötet. >>", local).kind == CombatChatKind::None);
        CHECK(ParseCombatChat("REVIERKAMPF: Enemy_One hat " + local + " getötet.", local).kind == CombatChatKind::None);
        const auto prison = ParseCombatChat("<< Enemy_One konnte nicht inhaftiert werden, da das Gefängnis gesprengt wurde. >>", local);
        CHECK(prison.kind == CombatChatKind::KillCorroboration && prison.victim == "Enemy_One" && prison.killer.empty());
        const std::string cemetery = "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.";
        CHECK(ParseCombatChat(cemetery, local).kind == CombatChatKind::Death);
        CHECK(ParseCombatChat(Cp1252(cemetery), local).family == CombatChatFamily::Cemetery);
        CHECK(ParseCombatChat(cemetery, local, true).kind == CombatChatKind::None);
        CHECK(ParseCombatChat("{33CCFF}[23:57:06] {FFFFFF}" + cemetery, local).kind == CombatChatKind::Death);
        CHECK(ParseCombatChat(Cp1252("[23:57:06] " + cemetery), local).kind == CombatChatKind::Death);
        CHECK(ParseCombatChat("INFO: " + cemetery, local).kind == CombatChatKind::Death);
    }
    CHECK(ParseCombatChat("REVIERKAMPF: local hat enemy getötet.", "LOCAL").kind == CombatChatKind::Kill);
    CHECK(ParseCombatChat("REVIERKAMPF: Local hat Enemy getötet (Teamkill). Neuer Spielstand: Angreifer: -2 - Verteidiger: 3", "Local").kind == CombatChatKind::Kill);
    CHECK(ParseCombatChat("REVIERKAMPF: Local hat Enemy getötet. Neuer Spielstand: Angreifer: 2 - Verteidiger: -3", "Local").kind == CombatChatKind::Kill);
    CHECK(ParseCombatChat("STREETWAR: Local hat Enemy (RCB) getötet. Aktuelle Kills: -1", "Local").kind == CombatChatKind::Kill);
    CHECK(ParseCombatChat("Du hast einen Spieler getötet.", "").kind == CombatChatKind::None);
    CHECK(ParseCombatChat("Du hast einen Spieler getötet.", "Invalid Name").kind == CombatChatKind::None);
    CHECK(ParseCombatChat("Du hast ein Verbrechen begangen ( Raub ). Reporter: Anonym", "Local").kind == CombatChatKind::None);
    CHECK(ParseCombatChat("STREETWAR: Local hat Enemy (Gang) getötet. Aktuelle Kills: invalid", "Local").kind == CombatChatKind::None);
    CHECK(ParseCombatChat("REVIERKAMPF: Local hat Local getötet.", "Local").kind == CombatChatKind::None);
    CHECK(ParseCombatChat(std::string(5000, 'x'), "Local").kind == CombatChatKind::None);
    std::printf("CombatChatParserTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
