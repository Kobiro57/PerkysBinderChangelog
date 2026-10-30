#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/RakHook/include/RakHook/samp.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] CombatChatBridge line %d: %s\n", __LINE__, #value); return; } } while (0)
template<class T> struct Fake {
    std::aligned_storage_t<sizeof(T), alignof(T)> memory{};
    T* get() { return reinterpret_cast<T*>(&memory); }
};
struct Context {
    Fake<sampapi::v03dl::CInput> input;
    Fake<sampapi::v03dl::CChat> chat;
    Fake<sampapi::v03dl::CDialog> dialog;
    Fake<sampapi::v03dl::CGame> game;
    Fake<sampapi::v03dl::CNetGame> network;
    Fake<sampapi::v03dl::CLocalPlayer> local;
    std::string name = "[RCB]Own_Player";
    unsigned long now = 1000;
    std::vector<std::string> sent, localMessages;
    keybinder::SettingsManager settings;
    SAMPBridge bridge;
    bool mirrorLocalMessages = false;
    bool statsReady = false;
    bool victimNamesAvailable = true;
    bool useAliasedVictimNames = false;
    Context() {
        network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
        local.get()->m_bIsActive = TRUE;
        local.get()->m_pPed = reinterpret_cast<sampapi::v03dl::CPed*>(0x2000);
        SAMPBridge::TestingHooks hooks{};
        hooks.getModuleHandle = [](const char*) { return reinterpret_cast<HMODULE>(1); };
        hooks.getSampBase = []() { return static_cast<uintptr_t>(1); };
        hooks.getRakHookVersion = []() { return rakhook::samp_ver::v03dlr1; };
        hooks.initializeRakHook = []() { return true; }; hooks.destroyRakHook = []() {};
        hooks.getInput = [&]() { return input.get(); }; hooks.getChat = [&]() { return chat.get(); };
        hooks.getDialog = [&]() { return dialog.get(); }; hooks.getGame = [&]() { return game.get(); };
        hooks.getNetGame = [&]() { return network.get(); }; hooks.getLocalPlayer = [&]() { return local.get(); };
        hooks.getLocalPlayerName = [&]() { return name; }; hooks.getLocalPlayerId = []() { return 5; };
        hooks.getPlayerName = [&](int id) {
            if (id == 5) return name;
            if (!victimNamesAvailable) return std::string();
            if (useAliasedVictimNames) return std::string("Enemy");
            return id == 7 ? std::string("Enemy_One") : std::string("Enemy_Two");
        };
        hooks.getTickCount = [&]() { return now; };
        hooks.isStartupStatsReady = [&]() { return statsReady; };
        hooks.closeStatsDialog = [&](int) { dialog.get()->m_bIsActive = FALSE; };
        hooks.sendInput = [&](sampapi::v03dl::CInput*, const char* text) { sent.emplace_back(text); };
        hooks.sendChat = [&](sampapi::v03dl::CLocalPlayer*, const char* text) { sent.emplace_back(text); };
        hooks.addMessage = [&](sampapi::v03dl::CChat*, unsigned long, const char* text) {
            localMessages.emplace_back(text);
            if (mirrorLocalMessages) bridge.ProcessIncomingChatLineForTesting(nullptr, text);
        };
        hooks.getCommandHandler = [](sampapi::v03dl::CInput*, const char*) -> sampapi::CMDPROC { return nullptr; };
        hooks.addCommand = [](sampapi::v03dl::CInput*, const char*, sampapi::CMDPROC) {};
        hooks.setDefaultCommand = [](sampapi::v03dl::CInput*, sampapi::CMDPROC) {};
        hooks.installIncomingChatInterceptor = [](const auto&) {};
        SAMPBridge::SetTestingHooks(hooks);
        bridge.SetSettingsManager(&settings);
        bridge.SetKillTrackerEnabled(true);
        bridge.SetKillTrackerServerKillMessageEnabled(true);
        bridge.SetKillTrackerServerKillMessageTemplate("/r Kill {total_kills}");
        bridge.Initialize();
        bridge.ImportKillCount(10); bridge.ImportDeathCount(3);
        bridge.Update();
    }
    ~Context() { bridge.Shutdown(); SAMPBridge::ResetTestingHooks(); }
    void Line(const std::string& text, const char* prefix = nullptr) { bridge.ProcessIncomingChatLineForTesting(prefix, text.c_str()); }
    void Stats(int kills, int deaths) {
        bridge.NotifyIncomingChat("*** " + name + " ***");
        bridge.NotifyIncomingChat("Kills: [" + std::to_string(kills) + "] Tode: [" + std::to_string(deaths) + "] Fraktion: [RCB]");
    }
    void StatsDialog(int kills, int deaths) {
        statsReady = true;
        bridge.Update();
        dialog.get()->m_nId = 401; dialog.get()->m_nType = 0; dialog.get()->m_bIsActive = TRUE;
        std::strcpy(dialog.get()->m_szCaption, "Statistiken");
        bridge.ObserveStatsCommand("/stats");
        bridge.ProcessStatsDialogForTesting({401, 0, "Statistiken", "Kills: " + std::to_string(kills) + "\nTode: " + std::to_string(deaths)});
        bridge.Update();
        dialog.get()->m_bIsActive = FALSE;
    }
};

std::string CemeteryCp1252(const std::string& text) {
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
    const int bytes = WideCharToMultiByte(1252, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
    std::string result(bytes, '\0');
    WideCharToMultiByte(1252, 0, wide.data(), count, result.data(), bytes, nullptr, nullptr);
    return result;
}

void TestCemeteryFormsCountOnceAndLaterMessageWorksWithoutNativeDeath() {
    Context context;
    const auto localMessagesBefore = context.localMessages.size();
    const std::string cemetery = "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.";
    const std::vector<std::string> forms = {
        cemetery, CemeteryCp1252(cemetery), "[23:57:06] " + cemetery,
        "{33CCFF}[23:57:06] {FFFFFF}" + cemetery,
        CemeteryCp1252("INFO: " + cemetery)
    };
    for (const auto& text : forms) {
        context.Line(text);
        context.bridge.Update();
        CHECK(context.bridge.GetTotalDeaths() == 4);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
        CHECK(context.bridge.GetTotalKills() == 10);
        CHECK(context.bridge.GetKillTracker()->GetSessionKills() == 0);
        CHECK(context.localMessages.size() == localMessagesBefore + 1);
        CHECK(context.sent.empty());
    }
    context.now += 6000;
    context.Line(cemetery); context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 5);
    CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 2);
    CHECK(context.localMessages.size() == localMessagesBefore + 2);
}

void TestAllFormsReachTheConfiguredChatChannelAndDynamicName() {
    Context context;
    const std::vector<std::string> forms = {
        "Du hast ein Verbrechen begangen ( Vorsätzlicher Mord ). Reporter: Anonym",
        "Du hast einen Spieler getötet.",
        "GANGWAR (RCB): Du hast einen Feind ausgeschaltet!",
        "<< " + context.name + " hat den gesuchten Verbrecher Enemy_One getötet. >>",
        "REVIERKAMPF: " + context.name + " hat Enemy_One getötet. Neuer Spielstand: Angreifer: 1 - Verteidiger: 4",
        "REVIERKAMPF: " + context.name + " hat Enemy_One getötet (Teamkill). Neuer Spielstand: Angreifer: 1 - Verteidiger: 4",
        "STREETWAR: " + context.name + " hat Enemy_One (Gang) getötet. Aktuelle Kills: 9",
        "DEATHZONE: " + context.name + " hat Enemy_One mit einer Desert Eagle getötet.",
        "REVIERKAMPF: " + context.name + " hat Enemy_One getötet.",
        "REVIERKAMPF: " + context.name + " hat einen Teamkill begangen."
    };
    int expected = 10;
    for (const auto& line : forms) {
        context.now += 6000;
        context.Line(line); CHECK(context.bridge.GetTotalKills() == expected);
        context.bridge.Update();
        CHECK(context.bridge.GetTotalKills() == ++expected);
        CHECK(context.sent.back() == "/r Kill " + std::to_string(expected));
    }
    CHECK(context.sent.size() == forms.size());
    CHECK(context.bridge.GetKillTracker()->GetSessionKills() == static_cast<int>(forms.size()));
    CHECK(context.bridge.GetKillTracker()->GetWeaponStats(-1) == nullptr);
    context.name = "A_New_Name";
    context.now += 6000;
    context.Line("REVIERKAMPF: A_New_Name hat Another_Enemy getötet.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == expected + 1);
}

void TestPersonalNamedRpcAndStatsCorrelateWithoutRepeatingBind() {
    Context context;
    context.Stats(10, 3);
    context.bridge.GetKillTracker()->RecordDamageToVictim("Enemy_One", 24, 46.2f);
    context.Line("Du hast einen Spieler getötet.");
    context.Line("Du hast ein Verbrechen begangen ( Vorsätzlicher Mord ). Reporter: Anonym");
    context.Line("GANGWAR (RCB): Du hast einen Feind ausgeschaltet!");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
    context.Line("DEATHZONE: " + context.name + " hat Enemy_One mit einer Desert Eagle getötet.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
    context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
    CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
    CHECK(context.bridge.GetKillTracker()->GetRecentKills().back().victim == "Enemy_One");
    CHECK(context.bridge.GetKillTracker()->GetRecentKills().back().damageDealt > 46.0f);
    CHECK(context.bridge.GetKillTracker()->GetWeaponStats(24)->killCount == 1);
    CHECK(context.bridge.GetKillTracker()->GetWeaponStats(-1) == nullptr);
    context.Stats(10, 3); // A queued older snapshot must not undo a live kill.
    CHECK(context.bridge.GetTotalKills() == 11);
    context.Stats(11, 3);
    CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
    context.Line("<< Enemy_One konnte nicht inhaftiert werden, da das Gefängnis gesprengt wurde. >>");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
}

void TestRpcNewerThanQueuedChatAndDistinctRapidVictims() {
    Context context;
    context.Line("Du hast einen Spieler getötet."); // Captured at T, drained after the native RPC at T+20.
    context.now += 20;
    context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
    context.Line("REVIERKAMPF: " + context.name + " hat Enemy_Two getötet.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
    context.bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
}

void TestTwoSameFramePersonalKillsAreNeverDropped() {
    Context context;
    context.Line("Du hast einen Spieler getötet.");
    context.Line("Du hast einen Spieler getötet.");
    context.Line("Du hast ein Verbrechen begangen ( Vorsätzlicher Mord ). Reporter: Anonym");
    context.Line("Du hast ein Verbrechen begangen ( Vorsätzlicher Mord ). Reporter: Anonym");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
    context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
    context.bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
    CHECK(context.bridge.GetKillTracker()->GetWeaponStats(24)->killCount == 1);
    CHECK(context.bridge.GetKillTracker()->GetWeaponStats(31)->killCount == 1);
}

void TestOnlyCemeteryCountsAndOtherDeathSourcesDoNotRepeatIt() {
    Context context;
    context.Line("<< Enemy_One hat den gesuchten Verbrecher " + context.name + " getötet. >>");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 3 && context.localMessages.empty());
    context.Line("Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 4 && context.bridge.GetTotalKills() == 10);
    CHECK(context.sent.empty());
    CHECK(context.localMessages.back().find("Tode: 4") != std::string::npos);
    context.bridge.ProcessDeathMessageForTesting({7, 5, 24, std::time(nullptr), true});
    CHECK(context.bridge.GetTotalDeaths() == 4);
    context.local.get()->m_bIsWasted = TRUE; context.bridge.Update();
    context.local.get()->m_bIsWasted = FALSE; context.bridge.Update();
    context.Line("Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 4);
    CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
    context.now += 6000;
    context.Line("<< Enemy_Two hat den gesuchten Verbrecher " + context.name + " getötet. >>");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 4);
    context.Line("Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 5);
}

void TestSpoofingUnrelatedPrisonAndLocalBinderEchoDoNotCount() {
    Context context;
    context.Line("Du hast einen Spieler getötet.", "Someone");
    context.Line("REVIERKAMPF: Other hat " + context.name + " getötet.");
    context.Line("<< Other konnte nicht inhaftiert werden, da das Gefängnis gesprengt wurde. >>");
    context.Line("<< Other hat den gesuchten Verbrecher Stranger getötet. >>");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 10 && context.bridge.GetTotalDeaths() == 3 && context.sent.empty());
    context.mirrorLocalMessages = true;
    context.bridge.SendLocalMessage("Du hast einen Spieler getötet.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 10 && context.sent.empty());
    context.bridge.SetKillTrackerEnabled(false);
    context.Line("Du hast einen Spieler getötet."); context.bridge.Update();
    context.bridge.SetKillTrackerEnabled(true); context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 10 && context.sent.empty());
}

void TestDialogStatsBeforeAndAfterChatDoNotRepeatOrUndoLiveEvent() {
    Context context;
    context.StatsDialog(10, 3);
    CHECK(context.bridge.AreStatsSynchronized() && context.bridge.GetTotalKills() == 10);
    context.Line("Du hast einen Spieler getötet."); context.bridge.Update();
    CHECK(context.bridge.GetTotalKills() == 11);
    context.StatsDialog(10, 3);
    CHECK(context.bridge.GetTotalKills() == 11 && context.bridge.GetKillTracker()->GetSessionKills() == 1);
    context.StatsDialog(11, 3);
    CHECK(context.bridge.GetTotalKills() == 11 && context.bridge.GetKillTracker()->GetSessionKills() == 1);
    context.now += 6000;
    context.Line("REVIERKAMPF: " + context.name + " hat Enemy_Two getötet.");
    context.StatsDialog(12, 3);
    CHECK(context.bridge.GetTotalKills() == 12 && context.bridge.GetKillTracker()->GetSessionKills() == 2);
    context.Line("Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.");
    context.bridge.Update();
    CHECK(context.bridge.GetTotalDeaths() == 4 && context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
    context.bridge.ProcessDeathMessageForTesting({7, 5, 24, std::time(nullptr), true});
    context.StatsDialog(12, 3);
    CHECK(context.bridge.GetTotalDeaths() == 4 && context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
    context.StatsDialog(12, 4);
    CHECK(context.bridge.GetTotalDeaths() == 4 && context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
}

void TestNativeVictimIdentityAndUnavailableNameCanBeEnriched() {
    {
        Context context;
        context.useAliasedVictimNames = true;
        context.bridge.ProcessDeathMessageForTesting({5, 7, 31, std::time(nullptr), true});
        context.bridge.ProcessDeathMessageForTesting({5, 9, -1, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
        context.bridge.ProcessDeathMessageForTesting({5, 9, -1, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
    }
    {
        Context context;
        context.useAliasedVictimNames = true;
        context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
        context.bridge.ProcessDeathMessageForTesting({5, 9, 24, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 12 && context.sent.size() == 2);
        CHECK(context.bridge.GetKillTracker()->GetWeaponStats(24)->killCount == 2);
    }
    {
        Context context;
        context.victimNamesAvailable = false;
        context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
        CHECK(!context.bridge.GetKillTracker()->GetRecentKills().back().victimKnown);
        context.Line("REVIERKAMPF: " + context.name + " hat Enemy_One getötet.");
        context.bridge.Update();
        CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
        CHECK(context.bridge.GetKillTracker()->GetRecentKills().back().victim == "Enemy_One");
        context.victimNamesAvailable = true;
        context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
        CHECK(context.bridge.GetKillTracker()->GetWeaponStats(24)->killCount == 1);
    }
    {
        Context context;
        context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
        context.bridge.ProcessDeathMessageForTesting({5, 7, -1, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
        CHECK(context.bridge.GetKillTracker()->GetRecentKills().back().weaponId == 24);
        context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 11 && context.bridge.GetKillTracker()->GetWeaponStats(24)->killCount == 1);
    }
    {
        Context context;
        context.bridge.ProcessDeathMessageForTesting({5, 7, -1, std::time(nullptr), true});
        context.bridge.ProcessDeathMessageForTesting({5, 7, 24, std::time(nullptr), true});
        CHECK(context.bridge.GetTotalKills() == 11 && context.sent.size() == 1);
        CHECK(context.bridge.GetKillTracker()->GetRecentKills().back().weaponId == 24);
        CHECK(context.bridge.GetKillTracker()->GetWeaponStats(24)->killCount == 1);
        CHECK(context.bridge.GetKillTracker()->GetWeaponStats(-1) == nullptr);
    }
}

void TestStatsBeforeAndAfterCemeteryKeepOneAbsoluteDeathAndOneNotice() {
    const std::string cemetery = "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.";
    for (bool dialog : {false, true}) {
        Context context;
        auto stats = [&](int deaths) {
            if (dialog) context.StatsDialog(10, deaths);
            else context.Stats(10, deaths);
        };
        stats(3);
        stats(4); // The absolute server snapshot can arrive before its admission line.
        CHECK(context.bridge.GetTotalDeaths() == 4);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 0);
        const auto before = context.localMessages.size();
        context.Line(cemetery); context.bridge.Update();
        CHECK(context.bridge.GetTotalDeaths() == 4);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
        CHECK(context.localMessages.size() == before + 1);
        CHECK(context.localMessages.back().find("Tode: 4") != std::string::npos);
        stats(4);
        context.Line(cemetery); context.bridge.Update();
        CHECK(context.bridge.GetTotalDeaths() == 4);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
        CHECK(context.localMessages.size() == before + 1);
        context.now += 6000;
        context.Line(cemetery); context.bridge.Update();
        CHECK(context.bridge.GetTotalDeaths() == 5);
        stats(5); // A matching later snapshot does not replay the death notice.
        CHECK(context.bridge.GetTotalDeaths() == 5);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 2);
        CHECK(context.localMessages.size() == before + 2);
        CHECK(context.sent == (dialog ? std::vector<std::string>{"/stats"} : std::vector<std::string>{}));
    }
}

void TestFirstAutomaticStatsSyncCreditsPersistedDeathBeforeCemetery() {
    const std::string cemetery = "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.";
    for (bool dialog : {false, true}) {
        Context context;
        context.bridge.ImportDeathCount(213); // The saved count is loaded before the first server response.
        CHECK(!context.bridge.AreStatsSynchronized());
        context.statsReady = true;
        context.bridge.Update();
        CHECK(std::find(context.sent.begin(), context.sent.end(), "/stats") != context.sent.end());
        if (dialog) {
            context.dialog.get()->m_nId = 401;
            context.dialog.get()->m_nType = 0;
            context.dialog.get()->m_bIsActive = TRUE;
            std::strcpy(context.dialog.get()->m_szCaption, "Statistiken");
            // Reply to the automatic request, without observing a manual /stats command.
            context.bridge.ProcessStatsDialogForTesting({401, 0, "Statistiken", "Kills: 10\nTode: 214"});
        } else {
            context.Stats(10, 214);
        }
        context.bridge.Update();
        CHECK(context.bridge.AreStatsSynchronized());
        CHECK(context.bridge.GetTotalDeaths() == 214);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 0);
        auto notices = [&]() {
            return std::count_if(context.localMessages.begin(), context.localMessages.end(), [](const std::string& value) {
                return value.find("Du bist gestorben.") != std::string::npos;
            });
        };
        CHECK(notices() == 0);
        context.Line(cemetery); context.bridge.Update();
        CHECK(context.bridge.GetTotalDeaths() == 214);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
        CHECK(notices() == 1);
        context.Line(cemetery); context.bridge.Update();
        CHECK(context.bridge.GetTotalDeaths() == 214);
        CHECK(context.bridge.GetKillTracker()->GetSessionDeaths() == 1);
        CHECK(notices() == 1);
    }
    Context localImport;
    localImport.bridge.ImportDeathCount(4); // A local/settings edit is not a server death response.
    localImport.Line(cemetery); localImport.bridge.Update();
    CHECK(localImport.bridge.GetTotalDeaths() == 5);
    CHECK(localImport.bridge.GetKillTracker()->GetSessionDeaths() == 1);
}
}

int RunCombatChatBridgeTests() {
    checks = failures = 0;
    TestAllFormsReachTheConfiguredChatChannelAndDynamicName();
    TestPersonalNamedRpcAndStatsCorrelateWithoutRepeatingBind();
    TestRpcNewerThanQueuedChatAndDistinctRapidVictims();
    TestTwoSameFramePersonalKillsAreNeverDropped();
    TestCemeteryFormsCountOnceAndLaterMessageWorksWithoutNativeDeath();
    TestOnlyCemeteryCountsAndOtherDeathSourcesDoNotRepeatIt();
    TestSpoofingUnrelatedPrisonAndLocalBinderEchoDoNotCount();
    TestDialogStatsBeforeAndAfterChatDoNotRepeatOrUndoLiveEvent();
    TestNativeVictimIdentityAndUnavailableNameCanBeEnriched();
    TestStatsBeforeAndAfterCemeteryKeepOneAbsoluteDeathAndOneNotice();
    TestFirstAutomaticStatsSyncCreditsPersistedDeathBeforeCemetery();
    std::printf("CombatChatBridgeTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
