#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/MainWindowUI.h"
#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/RakHook/include/RakNet/BitStream.h"
#include "../include/RakHook/include/RakNet/StringCompressor.h"
#include <cstring>
#include <cstdio>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] StartupStats: %s (line %d)\n", #value, __LINE__); return; } } while (0)

template<class Type> struct FakeObject {
    std::aligned_storage_t<sizeof(Type), alignof(Type)> bytes{};
    Type* get() { return reinterpret_cast<Type*>(&bytes); }
};

struct Context {
    FakeObject<sampapi::v03dl::CInput> input;
    FakeObject<sampapi::v03dl::CChat> chat;
    FakeObject<sampapi::v03dl::CDialog> dialog;
    FakeObject<sampapi::v03dl::CGame> game;
    FakeObject<sampapi::v03dl::CNetGame> network;
    FakeObject<sampapi::v03dl::CLocalPlayer> local;
    std::string name = "SyncPlayer";
    bool ready = false;
    bool jobSkillsReady = false; // Phase two is exercised by its own complete-dialog test below.
    unsigned long now = 1000;
    int closed = 0;
    int hidden = 0;
    std::vector<std::tuple<int, int, int, std::string>> replies;
    std::vector<std::string> commands, messages, localMessages;

    Context() {
        network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
        std::strcpy(network.get()->m_szHostAddress, "127.0.0.1");
        network.get()->m_nPort = 7777;
        local.get()->m_bIsActive = TRUE;
        local.get()->m_pPed = reinterpret_cast<sampapi::v03dl::CPed*>(0x2000);
    }
    SAMPBridge::TestingHooks Hooks() {
        SAMPBridge::TestingHooks hooks{};
        hooks.getModuleHandle = [](const char*) { return reinterpret_cast<HMODULE>(1); };
        hooks.getSampBase = []() { return static_cast<uintptr_t>(1); };
        hooks.getRakHookVersion = []() { return rakhook::samp_ver::v03dlr1; };
        hooks.initializeRakHook = []() { return true; };
        hooks.destroyRakHook = []() {};
        hooks.getInput = [&]() { return input.get(); };
        hooks.getChat = [&]() { return chat.get(); };
        hooks.getDialog = [&]() { return dialog.get(); };
        hooks.getGame = [&]() { return game.get(); };
        hooks.getNetGame = [&]() { return network.get(); };
        hooks.getLocalPlayer = [&]() { return local.get(); };
        hooks.getLocalPlayerName = [&]() { return name; };
        hooks.getLocalPlayerId = []() { return 5; };
        hooks.getPlayerName = [](int) { return std::string("AnotherPlayer"); };
        hooks.getTickCount = [&]() { return now; };
        hooks.isStartupStatsReady = [&]() { return ready; };
        hooks.isStartupJobSkillsReady = [&]() { return jobSkillsReady; };
        hooks.getCityLevel = []() { return 1; };
        hooks.getDistrictName = []() { return std::string("Market"); };
        hooks.sendInput = [&](sampapi::v03dl::CInput*, const char* text) { commands.push_back(text); };
        hooks.sendChat = [&](sampapi::v03dl::CLocalPlayer*, const char* text) { messages.push_back(text); };
        hooks.addMessage = [&](sampapi::v03dl::CChat*, unsigned long, const char* text) { localMessages.push_back(text); };
        hooks.getCommandHandler = [](sampapi::v03dl::CInput*, const char*) -> sampapi::CMDPROC { return nullptr; };
        hooks.addCommand = [](sampapi::v03dl::CInput*, const char*, sampapi::CMDPROC) {};
        hooks.setDefaultCommand = [](sampapi::v03dl::CInput*, sampapi::CMDPROC) {};
        hooks.installIncomingChatInterceptor = [](const auto&) {};
        hooks.closeStatsDialog = [&](int id) { if (id == dialog.get()->m_nId) { ++closed; dialog.get()->m_bIsActive = FALSE; } };
        hooks.hideDialog = [&](int id) { if (id == dialog.get()->m_nId) { ++hidden; dialog.get()->m_bIsActive = FALSE; } };
        hooks.sendDialogResponse = [&](RakNet::BitStream* payload) {
            short id = -1, row = -1;
            unsigned char response = 0, length = 0;
            if (!payload->Read(id) || !payload->Read(response) || !payload->Read(row) || !payload->Read(length)) return false;
            std::string text(length, '\0');
            if (length && !payload->Read(text.data(), length)) return false;
            replies.emplace_back(id, response, row, text);
            return payload->GetNumberOfUnreadBits() == 0;
        };
        return hooks;
    }
    std::size_t StatsImports() const {
        std::size_t count = 0;
        for (const auto& message : localMessages)
            if (message.find("Statistik \xC3\xBC" "bernommen.") != std::string::npos
                || message.find("Statistik \xFC" "bernommen.") != std::string::npos) ++count;
        return count;
    }
};

const char* levelLine = "Level:[9] Erfahrungspunkte [21/500] Geschlecht: [Mann] Alter: [22] Bargeld: [1.234$] Kontostand: [5.000$]";
const char* countLine = "Kills: [12] Tode: [4] Fraktion: [Zivilist] Gang: [-] Nächster Zahltag in: [10 Min] Gold: [3] Eventcoins: [2]";

bool DeliverDialog(SAMPBridge& bridge, Context& context, unsigned short id, unsigned char style,
                   const std::string& caption, const std::string& body) {
    auto* dialog = context.dialog.get();
    dialog->m_bIsActive = TRUE;
    dialog->m_nId = id;
    dialog->m_nType = style;
    std::strcpy(dialog->m_szCaption, caption.c_str());
    dialog->m_szText = nullptr;
    RakNet::BitStream payload;
    payload.Write(id); payload.Write(style);
    for (const auto& text : {caption, std::string("Auswaehlen"), std::string("Zurueck")}) {
        payload.Write(static_cast<unsigned char>(text.size()));
        if (!text.empty()) payload.Write(text.data(), static_cast<int>(text.size()));
    }
    StringCompressor::Instance()->EncodeString(body.c_str(), 0, &payload);
    return bridge.ProcessStatsDialogRpcForTesting(&payload) && payload.GetReadOffset() == 0;
}

std::string TestGameEncoding(const std::string& utf8) {
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), count);
    const int size = WideCharToMultiByte(CP_ACP, 0, wide.data(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.data(), -1, result.data(), size, nullptr, nullptr);
    result.resize(size - 1);
    return result;
}

void TestOneFastRequestImportsBaselineAndPreservesChatChannelForSimulation() {
    Context context;
    SAMPBridge::SetTestingHooks(context.Hooks());
    keybinder::SettingsManager settings;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    int saved = 0;
    bridge.SetStatsSynchronizedCallback([&](const keybinder::ServerStatsSnapshot& snapshot) {
        settings.GetMutable().synchronized_player_stats = snapshot;
        ++saved;
        return true;
    });
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(context.commands.empty());
    context.ready = true;
    context.dialog.get()->m_bIsActive = TRUE;
    bridge.Update();
    CHECK(context.commands.empty());
    context.dialog.get()->m_bIsActive = FALSE;
    context.input.get()->m_bEnabled = TRUE;
    bridge.Update();
    CHECK(context.commands.empty());
    context.input.get()->m_bEnabled = FALSE;
    bridge.Update();
    CHECK(context.commands == std::vector<std::string>{"/stats"});
    for (int index = 0; index < 4; ++index) bridge.Update();
    CHECK(context.commands.size() == 1);
    CHECK(bridge.ProcessIncomingChatLineForTesting("OtherPlayer", "Kills: [99] Tode: [99]"));
    CHECK(!bridge.ProcessIncomingChatLineForTesting(nullptr, "***SyncPlayer***"));
    CHECK(!bridge.ProcessIncomingChatLineForTesting(nullptr, levelLine));
    CHECK(!bridge.ProcessIncomingChatLineForTesting(nullptr, countLine));
    bridge.Update();
    CHECK(bridge.AreStatsSynchronized());
    CHECK(context.StatsImports() == 1);
    CHECK(bridge.GetTotalKills() == 12 && bridge.GetTotalDeaths() == 4);
    CHECK(bridge.GetKillTracker()->GetSessionKills() == 0 && bridge.GetKillTracker()->GetSessionDeaths() == 0);
    CHECK(context.commands.size() == 1 && context.messages.empty());
    CHECK(saved == 1);
    CHECK(bridge.GetSynchronizedStats().cash == 1234 && bridge.GetSynchronizedStats().bank == 5000);
    CHECK(bridge.GetSynchronizedStats().GetAvailableWealth() == 6234);
    CHECK(bridge.GetSynchronizedStats().experience == 21 && bridge.GetSynchronizedStats().nextLevelExperience == 500);
    CHECK(bridge.GetSynchronizedStats().gold == 3 && bridge.GetSynchronizedStats().eventcoins == 2);
    CHECK(bridge.ProcessIncomingChatLineForTesting(nullptr, "Ein normaler Serverhinweis"));
    CHECK(!bridge.ProcessIncomingChatLineForTesting(nullptr, "Wantedpunkte: [5] Offene Rechnungen: [Nein]"));
    ++context.now;
    bridge.Update();
    CHECK(bridge.GetSynchronizedStats().wantedPoints == 5 && saved == 2);
    CHECK(context.StatsImports() == 1);
    std::string error;
    CHECK(bridge.SendSimulatedKillMessage("/r Kills {total_kills} in {city}, {bezirk}", error));
    CHECK(context.commands.back() == "/r [Testkill] Kills 12 in Los Santos, Market");
    CHECK(bridge.SendSimulatedKillMessage("Meine Kills: {total_kills}", error));
    CHECK(context.messages.back() == "[Testkill] Meine Kills: 12");
    CHECK(bridge.GetTotalKills() == 12 && bridge.GetKillTracker()->GetSessionKills() == 0);
    CHECK(!bridge.SendSimulatedKillMessage(" \t ", error));
    CHECK(!error.empty());
    // A manual own header after completion must cancel any remaining hiding,
    // even when its automatic block did not include the known footer.
    CHECK(bridge.ProcessIncomingChatLineForTesting(nullptr, "*** SyncPlayer ***"));
    CHECK(bridge.ProcessIncomingChatLineForTesting(nullptr, levelLine));
    CHECK(bridge.ProcessIncomingChatLineForTesting(nullptr, countLine));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestTimeoutDoesNotSpamOrClaimSuccessAndReconnectCanSyncAgain() {
    Context context;
    context.ready = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(context.commands.size() == 1);
    context.now += 8000;
    bridge.Update();
    CHECK(!bridge.AreStatsSynchronized() && context.StatsImports() == 0);
    for (int index = 0; index < 10; ++index) bridge.Update();
    CHECK(context.commands.size() == 1);
    CHECK(bridge.ProcessIncomingChatLineForTesting(nullptr, "*** SyncPlayer ***"));
    CHECK(bridge.ProcessIncomingChatLineForTesting(nullptr, countLine));
    context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_RESTARTING;
    bridge.Update();
    context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    bridge.Update();
    CHECK(context.commands.size() == 2 && context.commands.back() == "/stats");
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestOnlyAnIdentifiedOwnStatsDialogCloses() {
    Context context;
    context.ready = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    std::string body = std::string("*** OtherPlayer ***\n") + levelLine + "\n" + countLine;
    auto* dialog = context.dialog.get();
    dialog->m_bIsActive = TRUE;
    dialog->m_nId = 123;
    dialog->m_nType = sampapi::v03dl::CDialog::DIALOG_MESSAGEBOX;
    std::strcpy(dialog->m_szCaption, "Stats");
    dialog->m_szText = body.data();
    bridge.Update();
    CHECK(context.closed == 0 && !bridge.AreStatsSynchronized());
    body = std::string("*** SyncPlayer ***\n") + levelLine + "\n" + countLine;
    dialog->m_szText = body.data();
    bridge.Update();
    CHECK(context.closed == 1 && !dialog->m_bIsActive);
    CHECK(bridge.AreStatsSynchronized() && context.StatsImports() == 1);
    CHECK(bridge.GetTotalKills() == 12 && bridge.GetTotalDeaths() == 4);
    CHECK(context.commands.size() == 1);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestStatsCacheRoundTripAndOldConfigurations() {
    keybinder::SettingsManager settings;
    CHECK(!settings.Get().synchronized_player_stats.HasCore());
    CHECK(settings.LoadFromJson("{}"));
    auto& snapshot = settings.GetMutable().synchronized_player_stats;
    snapshot.playerName = "SyncPlayer";
    snapshot.serverAddress = "127.0.0.1";
    snapshot.serverPort = 7777;
    snapshot.kills = 123;
    snapshot.deaths = 45;
    snapshot.cash = 1234;
    snapshot.bank = 9000000001LL;
    snapshot.level = 9;
    snapshot.wantedPoints = 5;
    keybinder::SettingsManager restored;
    CHECK(restored.LoadFromJson(settings.GetSettingsJson()));
    const auto& cache = restored.Get().synchronized_player_stats;
    CHECK(cache.HasCore() && cache.playerName == "SyncPlayer");
    CHECK(cache.serverAddress == "127.0.0.1" && cache.serverPort == 7777);
    CHECK(cache.kills == 123 && cache.deaths == 45 && cache.cash == 1234 && cache.bank == 9000000001LL);
    CHECK(cache.level == 9 && cache.wantedPoints == 5 && !cache.wealth);
    CHECK(cache.GetAvailableWealth() == 9000001235LL);
    CHECK(restored.LoadFromJson("{\"stats_cache_player\":\"Nobody\",\"stats_cache_kills\":\"bad\"}"));
    CHECK(!restored.Get().synchronized_player_stats.HasCore());
}

void TestAllStatsDialogStylesImportCloseAndSendRealTotalInSameFrame() {
    for (unsigned char style : {0, 2, 4, 5}) {
        Context context;
        context.ready = true;
        SAMPBridge::SetTestingHooks(context.Hooks());
        keybinder::SettingsManager settings;
        SAMPBridge bridge;
        bridge.SetSettingsManager(&settings);
        int saved = 0;
        bridge.SetStatsSynchronizedCallback([&](const keybinder::ServerStatsSnapshot& snapshot) {
            settings.GetMutable().synchronized_player_stats = snapshot;
            ++saved;
            return true;
        });
        CHECK(bridge.Initialize());
        bridge.Update();
        CHECK(context.commands == std::vector<std::string>{"/stats"});
        auto* dialog = context.dialog.get();
        dialog->m_bIsActive = TRUE;
        dialog->m_nId = 234;
        dialog->m_nType = style;
        std::strcpy(dialog->m_szCaption, "Statistiken");
        // A native list may expose no combined body. Only the original RPC
        // contains the full table, including counters beyond 16 rows.
        dialog->m_szText = nullptr;
        std::string body;
        for (int row = 0; row < 25; ++row) body += "Zusatzwert\tvorhanden\n";
        body += "Kills:\t(341)\nTode:\t(119)\nBargeld:\t12.345$\nKontostand:\t67.890$";
        bridge.ProcessStatsDialogForTesting({234, style, "Statistiken", body});
        bridge.Update(); // same clock value: no artificial delay or timeout wait
        CHECK(bridge.AreStatsSynchronized());
        CHECK(context.closed == 1 && !dialog->m_bIsActive);
        CHECK(context.StatsImports() == 1 && saved == 1);
        CHECK(bridge.GetTotalKills() == 341 && bridge.GetTotalDeaths() == 119);
        CHECK(bridge.GetKillTracker()->GetSessionKills() == 0);
        CHECK(bridge.GetSynchronizedStats().GetAvailableWealth() == 80235);
        std::string error;
        CHECK(bridge.SendSimulatedKillMessage("/f Kills: {total_kills}", error));
        CHECK(context.commands.back() == "/f [Testkill] Kills: 341");
        // Later manual stats update the snapshot without closing the user's UI.
        dialog->m_bIsActive = TRUE;
        bridge.ObserveStatsCommand("/stats");
        bridge.ProcessStatsDialogForTesting({234, style, "Statistiken", "Kills: 342\nTode: 120"});
        bridge.Update();
        CHECK(context.closed == 1 && dialog->m_bIsActive);
        CHECK(bridge.GetTotalKills() == 342 && bridge.GetTotalDeaths() == 120);
        CHECK(context.StatsImports() == 1 && saved == 2);
        CHECK(!bridge.GetSynchronizedStats().cash && !bridge.GetSynchronizedStats().bank);
        keybinder::SettingsManager restored;
        CHECK(restored.LoadFromJson(settings.GetSettingsJson()));
        CHECK(restored.Get().synchronized_player_stats.HasCounters());
        CHECK(restored.Get().synchronized_player_stats.kills == 342);
        // A neutral statistics dialog without another own /stats request
        // must not replace our account totals with some other player's data.
        bridge.ObserveStatsCommand("/stats 99");
        bridge.ProcessStatsDialogForTesting({234, style, "Statistiken", "Kills: 999\nTode: 888"});
        bridge.Update();
        CHECK(bridge.GetTotalKills() == 342 && bridge.GetTotalDeaths() == 120);
        bridge.Shutdown();
        SAMPBridge::ResetTestingHooks();
    }
}

void TestManualStatsRecoversAfterTimeoutAndOtherDialogsStayOpen() {
    Context context;
    context.ready = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    context.now += 8000;
    bridge.Update();
    CHECK(!bridge.AreStatsSynchronized() && context.StatsImports() == 0);
    auto* dialog = context.dialog.get();
    dialog->m_bIsActive = TRUE;
    dialog->m_nId = 345;
    dialog->m_nType = sampapi::v03dl::CDialog::DIALOG_TABLIST;
    std::strcpy(dialog->m_szCaption, "Inventar");
    bridge.ObserveStatsCommand("/stats");
    bridge.ProcessStatsDialogForTesting({345, 4, "Inventar", "Kills: 67\nTode: 89"});
    bridge.Update();
    CHECK(!bridge.AreStatsSynchronized() && context.closed == 0 && dialog->m_bIsActive);
    std::strcpy(dialog->m_szCaption, "Statistiken");
    bridge.ProcessStatsDialogForTesting({345, 4, "Statistiken", "Name: OtherPlayer\nKills: 67\nTode: 89"});
    bridge.Update();
    CHECK(!bridge.AreStatsSynchronized() && context.closed == 0);
    bridge.ProcessStatsDialogForTesting({345, 4, "Statistiken", "Kills: 67\nTode: 89"});
    bridge.Update();
    CHECK(bridge.AreStatsSynchronized() && context.StatsImports() == 1);
    CHECK(context.closed == 1 && !dialog->m_bIsActive);
    CHECK(bridge.GetTotalKills() == 67 && bridge.GetTotalDeaths() == 89);
    CHECK(context.commands == std::vector<std::string>{"/stats"});
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestObservedNovaScreenshotLayoutThroughCompressedRpcWithDynamicPlayerName() {
    // Reproduce the screenshot's layout and game encoding, with synthetic
    // identities/account values. No screenshot or personal account data is used.
    for (const char* playerName : {"Fixture_East", "[TAG]Fixture_West"}) {
        Context context;
        context.name = playerName;
        context.ready = true;
        SAMPBridge::SetTestingHooks(context.Hooks());
        keybinder::SettingsManager settings;
        SAMPBridge bridge;
        bridge.SetSettingsManager(&settings);
        int saved = 0;
        bridge.SetStatsSynchronizedCallback([&](const auto& snapshot) {
            settings.GetMutable().synchronized_player_stats = snapshot;
            ++saved;
            return true;
        });
        CHECK(bridge.Initialize());
        bridge.Update();
        CHECK(context.commands == std::vector<std::string>{"/stats"});
        const std::string caption = std::string("Statistiken \xBB ") + context.name;
        const std::string body = "Name: " + context.name + "\nAlter: 0\nGeschlecht: M\xE4nnlich\n"
            "Level: 17\nErfahrungspunkte: 1234/28176\nN\xE4" "chster Zahltag in:26 Minuten\n\n"
            "Handynummer: 10000\n\nKills: 73\nTode: 219\n\nFraktion: Testfraktion\nFraktionsrang: Test\n\n"
            "Bargeld: 12.345$\nKontostand: 67.890$\nGold: 9\nEventcoins: 3\n\nWantedpunkte: 0\n\n"
            "Offene Rechnungen: Nein\n\nF\xFChrer" "scheine:\nAuto: Nicht vorhanden\nMotorrad: Nicht vorhanden\n"
            "Flugzeug: Nicht vorhanden\nBoot: Nicht vorhanden\nWaffenschein A: Nicht vorhanden\n"
            "Waffenschein B: Nicht vorhanden\nWaffenschein C: Nicht vorhanden";
        auto* dialog = context.dialog.get();
        dialog->m_bIsActive = TRUE;
        dialog->m_nId = 789;
        dialog->m_nType = sampapi::v03dl::CDialog::DIALOG_MESSAGEBOX;
        std::strcpy(dialog->m_szCaption, caption.c_str());
        dialog->m_szText = nullptr; // prove the full RPC is sufficient
        RakNet::BitStream payload;
        payload.Write(static_cast<unsigned short>(789));
        payload.Write(static_cast<unsigned char>(0));
        for (const auto& value : {caption, std::string("Schlie\xDF" "en"), std::string()}) {
            payload.Write(static_cast<unsigned char>(value.size()));
            if (!value.empty()) payload.Write(value.data(), static_cast<int>(value.size()));
        }
        StringCompressor::Instance()->EncodeString(body.c_str(), 0, &payload);
        CHECK(bridge.ProcessStatsDialogRpcForTesting(&payload));
        CHECK(payload.GetReadOffset() == 0);
        bridge.Update(); // no clock advance or manual dialog close
        CHECK(bridge.AreStatsSynchronized() && saved == 1);
        CHECK(context.closed == 1 && !dialog->m_bIsActive);
        CHECK(context.StatsImports() == 1);
        CHECK(bridge.GetTotalKills() == 73 && bridge.GetTotalDeaths() == 219);
        const auto& snapshot = bridge.GetSynchronizedStats();
        CHECK(snapshot.playerName == context.name);
        CHECK(snapshot.cash == 12345 && snapshot.bank == 67890 && snapshot.GetAvailableWealth() == 80235);
        CHECK(snapshot.level == 17 && snapshot.experience == 1234 && snapshot.nextLevelExperience == 28176);
        CHECK(snapshot.nextPaydayMinutes == 26 && snapshot.gold == 9 && snapshot.eventcoins == 3 && snapshot.wantedPoints == 0);
        std::string error;
        CHECK(bridge.SendSimulatedKillMessage("/r Kills: {total_kills}, Tode: {total_deaths}", error));
        CHECK(context.commands.back() == "/r [Testkill] Kills: 73, Tode: 219");
        keybinder::SettingsManager restored;
        CHECK(restored.LoadFromJson(settings.GetSettingsJson()));
        CHECK(restored.Get().synchronized_player_stats.playerName == context.name);
        CHECK(restored.Get().synchronized_player_stats.kills == 73 && restored.Get().synchronized_player_stats.deaths == 219);
        bridge.Shutdown();
        SAMPBridge::ResetTestingHooks();
    }
}

void TestSpeedCommandPerformsThreeExactDialogRepliesAndSubmitsOnlyOnce() {
    Context context;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    std::string error;
    CHECK(!bridge.RequestSpeedTicket("1004", error));
    CHECK(context.commands.empty());
    CHECK(bridge.RequestSpeedTicket("57", error));
    CHECK(context.commands == std::vector<std::string>{"/su"});
    CHECK(!bridge.RequestSpeedTicket("58", error));
    CHECK(DeliverDialog(bridge, context, 500, 2, "Staat \xBB Wantedkatalog", "Ordnungswidrigkeit\nVergehen\nVerbrechen"));
    bridge.Update();
    CHECK(context.replies.size() == 1 && context.hidden == 1);
    CHECK(context.replies[0] == std::make_tuple(500, 1, 0, std::string("Ordnungswidrigkeit")));
    const std::string offence = "Geschwindigkeits\xFC" "berschreitung ab 20km/h";
    const std::string catalogue = "Tatbestand\tWantedpunkte\nLeichter Versto\xDF gegen die StVO\t3\n"
        "Schwerer Versto\xDF gegen die StVO\t5\nParken innerhalb eines Sperrgebiets\t2\n"
        "Erregung \xF6" "ffentlichen \xC4rgernisses\t8\nUmweltverschmutzung\t9\n" + offence + "\t15";
    CHECK(DeliverDialog(bridge, context, 501, 5, "Wanteds \xBB Ordnungswidrigkeit", catalogue));
    bridge.Update();
    CHECK(context.replies.size() == 2 && context.hidden == 2);
    CHECK(context.replies[1] == std::make_tuple(501, 1, 5, offence));
    CHECK(DeliverDialog(bridge, context, 502, 1, "Wanteds vergeben", "Grund: " + offence
        + "\nWantedpunkte: 15\n\nBitte Spieler-ID oder Name eingeben:"));
    bridge.Update();
    CHECK(context.replies.size() == 3 && context.hidden == 3);
    CHECK(context.replies[2] == std::make_tuple(502, 1, -1, std::string("57")));
    CHECK(DeliverDialog(bridge, context, 502, 1, "Wanteds vergeben", "Grund: " + offence
        + "\nWantedpunkte: 15\n\nBitte Spieler-ID oder Name eingeben:"));
    bridge.Update();
    CHECK(context.replies.size() == 3);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestStartupSynchronizesStatsThenJobSkillsAndKeepsExistingEarnings() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(context.commands == std::vector<std::string>{"/stats"});
    CHECK(DeliverDialog(bridge, context, 700, 0, "Statistiken \xBB SyncPlayer", "Name: SyncPlayer\nKills: 73\nTode: 219"));
    bridge.Update();
    CHECK(bridge.AreStatsSynchronized() && !bridge.AreJobSkillsSynchronized());
    CHECK(context.commands == (std::vector<std::string>{"/stats", "/jobskills"}));
    auto* tracker = bridge.GetJobTracker();
    CHECK(tracker && tracker->HasRegistryData());
    std::string table = "T\xC3\xA4tigkeit\tStufe\n";
    std::map<jobs::JobId, int> expected;
    std::map<jobs::JobId, int> earnings;
    int index = 0;
    for (const auto& job : tracker->GetJobs()) {
        const int points = index++ * 100;
        expected[job.id] = points;
        earnings[job.id] = tracker->GetAggregates(job.id).lifetime;
        table += job.name + "\t" + std::to_string(points) + "\n";
    }
    table += "Security Guard\t0\nTrucker\t0\n";
    CHECK(DeliverDialog(bridge, context, 701, 5, "Jobs \xBB Skill\xFC" "bersicht", TestGameEncoding(table)));
    bridge.Update();
    CHECK(bridge.AreJobSkillsSynchronized() && context.closed == 2);
    int notices = 0;
    for (const auto& message : context.localMessages) if (message.find("Synchronisation abgeschlossen.") != std::string::npos) ++notices;
    CHECK(notices == 1);
    for (const auto& point : expected) {
        CHECK(tracker->GetAdjustedHighestSkill(point.first) == point.second);
        CHECK(tracker->GetAggregates(point.first).lifetime == earnings[point.first]);
        const auto feed = tracker->GetEventsForJob(point.first);
        CHECK(!feed.empty());
    }
    const auto eventCount = tracker->GetEvents().size();
    bridge.ObserveStatsCommand("/jobskills");
    CHECK(DeliverDialog(bridge, context, 701, 5, "Jobs \xBB Skill\xFC" "bersicht", TestGameEncoding(table)));
    bridge.Update();
    CHECK(tracker->GetEvents().size() == eventCount);
    CHECK(context.dialog.get()->m_bIsActive && context.closed == 2); // later manual view stays open
    CHECK(context.commands == (std::vector<std::string>{"/stats", "/jobskills"}));
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}
std::string ManualSyncSkillsTable(SAMPBridge& bridge, int firstPoints) {
    std::string table = "T\xC3\xA4tigkeit\tStufe\n";
    int index = 0;
    for (const auto& job : bridge.GetJobTracker()->GetJobs())
        table += job.name + "\t" + std::to_string(firstPoints + index++) + "\n";
    return table + "Security Guard\t0\nTrucker\t0\n";
}

bool CompleteFreshSyncDialogs(SAMPBridge& bridge, Context& context, int kills, int deaths, int skillPoints) {
    const std::string stats = "Name: " + context.name + "\nKills: " + std::to_string(kills)
        + "\nTode: " + std::to_string(deaths) + "\nBargeld: 2.000$\nKontostand: 8.000$\nGold: 8";
    if (!DeliverDialog(bridge, context, 800, 0, "Statistiken \xBB " + context.name, stats)) return false;
    bridge.Update();
    if (!bridge.AreStatsSynchronized() || context.commands.back() != "/jobskills") return false;
    if (!DeliverDialog(bridge, context, 801, 5, "Jobs \xBB Skill\xFC" "bersicht",
            TestGameEncoding(ManualSyncSkillsTable(bridge, skillPoints)))) return false;
    bridge.Update();
    return bridge.AreStatsSynchronized() && bridge.AreJobSkillsSynchronized();
}

int ManualSyncSuccessNotices(const Context& context) {
    int notices = 0;
    for (const auto& message : context.localMessages)
        if (message.find("Du hast deinen Spielestand manuell synchronisiert. Deine Werte sind jetzt im aktuellsten Stand.")
            != std::string::npos) ++notices;
    return notices;
}

void TestManualFullSyncReusesPipelineAndImportsFreshValuesWithoutNewKills() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    keybinder::SettingsManager settings;
    SAMPBridge bridge;
    bridge.SetSettingsManager(&settings);
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(false);
    int saves = 0;
    bridge.SetStatsSynchronizedCallback([&](const auto& value) {
        settings.GetMutable().synchronized_player_stats = value;
        ++saves;
        return true;
    });
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 100, 40, 10));
    CHECK(saves == 1 && ManualSyncSuccessNotices(context) == 0);
    bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    CHECK(bridge.GetTotalKills() == 101 && bridge.GetKillTracker()->GetSessionKills() == 1);
    auto* jobTracker = bridge.GetJobTracker();
    CHECK(!jobTracker->GetJobs().empty());
    const auto job = jobTracker->GetJobs().front().id;
    const auto income = jobTracker->GetAggregates(job).lifetime;
    const auto commandsBefore = context.commands.size();
    std::string error;
    CHECK(bridge.RequestManualStateSynchronization(error));
    CHECK(error.empty() && bridge.IsManualStateSynchronizationPending());
    CHECK(!bridge.RequestManualStateSynchronization(error) && !error.empty());
    CHECK(context.commands.size() == commandsBefore); // Request is deferred until UI closes.
    CHECK(bridge.GetTotalKills() == 101 && jobTracker->GetAdjustedHighestSkill(job) == 10);

    // A queued stale response from before the new /stats must not win.
    bridge.ProcessStatsDialogForTesting({899, 0, "Statistiken", "Name: SyncPlayer\nKills: 999\nTode: 999"});
    context.game.get()->m_nCursorMode = 2; // Binder menu remains open for this frame.
    bridge.Update();
    CHECK(context.commands.size() == commandsBefore && bridge.GetTotalKills() == 101);
    context.game.get()->m_nCursorMode = 0;
    bridge.Update();
    CHECK(context.commands.size() == commandsBefore + 1 && context.commands.back() == "/stats");
    CHECK(!bridge.AreStatsSynchronized() && bridge.GetTotalKills() == 101);
    CHECK(ManualSyncSuccessNotices(context) == 0);
    CHECK(DeliverDialog(bridge, context, 800, 0, "Statistiken \xBB SyncPlayer",
        "Name: SyncPlayer\nKills: 25\nTode: 8\nBargeld: 4.000$\nKontostand: 9.000$\nGold: 6"));
    // A previous jobskills response must likewise be discarded before phase two.
    bridge.ProcessStatsDialogForTesting({899, 5, "Jobs \xBB Skill\xFC" "bersicht",
        TestGameEncoding(ManualSyncSkillsTable(bridge, 999))});
    bridge.Update();
    CHECK(bridge.GetTotalKills() == 25 && bridge.GetTotalDeaths() == 8);
    CHECK(saves == 2 && settings.Get().synchronized_player_stats.kills == 25);
    CHECK(bridge.GetSynchronizedStats().GetAvailableWealth() == 13000);
    CHECK(context.commands.back() == "/jobskills" && !bridge.AreJobSkillsSynchronized());
    CHECK(jobTracker->GetAdjustedHighestSkill(job) == 10 && ManualSyncSuccessNotices(context) == 0);
    CHECK(bridge.IsManualStateSynchronizationPending());
    CHECK(DeliverDialog(bridge, context, 801, 5, "Jobs \xBB Skill\xFC" "bersicht",
        TestGameEncoding(ManualSyncSkillsTable(bridge, 222))));
    bridge.Update();
    CHECK(bridge.AreStatsSynchronized() && bridge.AreJobSkillsSynchronized());
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 1);
    CHECK(jobTracker->GetAdjustedHighestSkill(job) == 222 && jobTracker->GetAggregates(job).lifetime == income);
    CHECK(bridge.GetKillTracker()->GetSessionKills() == 1 && bridge.GetKillTracker()->GetSessionDeaths() == 0);
    CHECK(context.closed == 4 && context.messages.empty());
    for (int i = 0; i < 4; ++i) bridge.Update();
    CHECK(ManualSyncSuccessNotices(context) == 1);

    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 26, 9, 223));
    CHECK(saves == 3 && ManualSyncSuccessNotices(context) == 2);
    CHECK(bridge.GetTotalKills() == 26 && bridge.GetTotalDeaths() == 9);
    CHECK(jobTracker->GetAdjustedHighestSkill(job) == 223 && bridge.GetKillTracker()->GetSessionKills() == 1);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestManualSyncTimeoutPreservesValuesAndCanBeRetried() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 20));
    const auto job = bridge.GetJobTracker()->GetJobs().front().id;
    std::string error;
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    context.now += 8000;
    bridge.Update();
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 0);
    CHECK(bridge.GetTotalKills() == 10 && bridge.GetTotalDeaths() == 3);
    CHECK(bridge.GetJobTracker()->GetAdjustedHighestSkill(job) == 20);
    const auto stoppedCommands = context.commands.size();
    for (int i = 0; i < 3; ++i) bridge.Update();
    CHECK(context.commands.size() == stoppedCommands);
    CHECK(DeliverDialog(bridge, context, 802, 0, "Statistiken \xBB SyncPlayer", "Name: SyncPlayer\nKills: 999\nTode: 999"));
    bridge.Update();
    CHECK(bridge.GetTotalKills() == 10 && ManualSyncSuccessNotices(context) == 0);
    context.dialog.get()->m_bIsActive = FALSE;
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    CHECK(DeliverDialog(bridge, context, 800, 0, "Statistiken \xBB SyncPlayer", "Name: SyncPlayer\nKills: 12\nTode: 5"));
    bridge.Update();
    CHECK(bridge.GetTotalKills() == 12 && bridge.IsManualStateSynchronizationPending());
    context.now += 8000;
    bridge.Update();
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 0);
    CHECK(bridge.GetJobTracker()->GetAdjustedHighestSkill(job) == 20);
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 13, 6, 40));
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 1);
    CHECK(bridge.GetTotalKills() == 13 && bridge.GetJobTracker()->GetAdjustedHighestSkill(job) == 40);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestManualSyncSaveFailureDoesNotClaimSuccessOrFetchJobSkills() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 20));
    const auto job = bridge.GetJobTracker()->GetJobs().front().id;
    bridge.SetStatsSynchronizedCallback([](const auto&) { return false; });
    const auto commandsBefore = context.commands.size();
    std::string error;
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    CHECK(DeliverDialog(bridge, context, 800, 0, "Statistiken \xBB SyncPlayer", "Name: SyncPlayer\nKills: 14\nTode: 7"));
    bridge.Update();
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 0);
    CHECK(context.commands.size() == commandsBefore + 1 && context.commands.back() == "/stats");
    CHECK(bridge.GetJobTracker()->GetAdjustedHighestSkill(job) == 20);
    CHECK(bridge.GetTotalKills() == 14); // Valid read stays available despite persistence failure.
    bridge.SetStatsSynchronizedCallback([](const auto&) { return true; });
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 15, 8, 41));
    CHECK(ManualSyncSuccessNotices(context) == 1);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestManualSyncRequestRejectsBusyOrOfflineAndUiClosesOnlyWhenAccepted() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    std::string error;
    CHECK(!bridge.RequestManualStateSynchronization(error) && !error.empty()); // Active login sync.
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 20));
    const auto commandsBefore = context.commands.size();
    keybinder::MainWindowUI ui;
    ui.SetSAMPBridge(&bridge);
    context.dialog.get()->m_bIsActive = TRUE;
    CHECK(!ui.RequestManualStateSynchronization() && !ui.WasCloseRequested());
    context.dialog.get()->m_bIsActive = FALSE;
    context.input.get()->m_bEnabled = TRUE;
    CHECK(!ui.RequestManualStateSynchronization() && !ui.WasCloseRequested());
    context.input.get()->m_bEnabled = FALSE;
    CHECK(bridge.BeginManualTextInput());
    CHECK(!bridge.RequestManualStateSynchronization(error));
    bridge.CancelManualTextInput();
    context.local.get()->m_bIsWasted = TRUE;
    CHECK(!ui.RequestManualStateSynchronization() && !ui.WasCloseRequested());
    context.local.get()->m_bIsWasted = FALSE;
    context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_RESTARTING;
    CHECK(!ui.RequestManualStateSynchronization() && !ui.WasCloseRequested());
    context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    CHECK(context.commands.size() == commandsBefore && !bridge.IsManualStateSynchronizationPending());
    context.game.get()->m_nCursorMode = 2;
    CHECK(ui.RequestManualStateSynchronization() && ui.WasCloseRequested());
    ui.ResetCloseRequest();
    CHECK(!ui.RequestManualStateSynchronization() && !ui.WasCloseRequested());
    CHECK(context.commands.size() == commandsBefore);
    context.game.get()->m_nCursorMode = 0;
    bridge.Update();
    CHECK(context.commands.size() == commandsBefore + 1);
    context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_RESTARTING;
    bridge.Update();
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 0);
    CHECK(bridge.GetTotalKills() == 10);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestManualSyncChatCountersAreImportedWithoutArtificialSessionEvents() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(true);
    bridge.SetKillTrackerServerKillMessageTemplate("/f must-not-send-for-import");
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 20));
    std::string error;
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    CHECK(!bridge.ProcessIncomingChatLineForTesting(nullptr, "*** SyncPlayer ***"));
    CHECK(!bridge.ProcessIncomingChatLineForTesting(nullptr,
        "Kills: [11] Tode: [4] Fraktion: [Zivilist] Gang: [-]"));
    bridge.Update();
    CHECK(bridge.GetTotalKills() == 11 && bridge.GetTotalDeaths() == 4);
    CHECK(bridge.GetKillTracker()->GetSessionKills() == 0 && bridge.GetKillTracker()->GetSessionDeaths() == 0);
    CHECK(context.commands == (std::vector<std::string>{"/stats", "/jobskills", "/stats", "/jobskills"}));
    CHECK(bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 0);
    CHECK(DeliverDialog(bridge, context, 801, 5, "Jobs \xBB Skill\xFC" "bersicht",
        TestGameEncoding(ManualSyncSkillsTable(bridge, 21))));
    bridge.Update();
    CHECK(ManualSyncSuccessNotices(context) == 1 && !bridge.IsManualStateSynchronizationPending());
    CHECK(context.messages.empty());
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestManualSyncCannotCompleteAcrossPlayerIdentityChange() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 20));
    std::string error;
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    context.name = "NextPlayer";
    bridge.ProcessStatsDialogForTesting({899, 0, "Statistiken", "Name: SyncPlayer\nKills: 999\nTode: 999"});
    bridge.Update();
    CHECK(!bridge.IsManualStateSynchronizationPending() && ManualSyncSuccessNotices(context) == 0);
    CHECK(!bridge.AreStatsSynchronized() && bridge.GetTotalKills() == 10);
    CHECK(context.commands == (std::vector<std::string>{"/stats", "/jobskills", "/stats", "/stats"}));
    CHECK(CompleteFreshSyncDialogs(bridge, context, 15, 8, 30));
    CHECK(bridge.GetSynchronizedStats().playerName == "NextPlayer");
    CHECK(bridge.GetTotalKills() == 15 && ManualSyncSuccessNotices(context) == 0);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

void TestManualSyncPreservesRealEventsConfirmedAfterTheRequest() {
    Context context;
    context.ready = context.jobSkillsReady = true;
    SAMPBridge::SetTestingHooks(context.Hooks());
    SAMPBridge bridge;
    bridge.SetKillTrackerEnabled(true);
    bridge.SetKillTrackerServerKillMessageEnabled(false);
    CHECK(bridge.Initialize());
    bridge.Update();
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 20));
    std::string error;
    CHECK(bridge.RequestManualStateSynchronization(error));
    bridge.Update();
    bridge.ProcessDeathMessageForTesting({5, 9, 31, std::time(nullptr), true});
    bridge.ProcessDeathMessageForTesting({8, 5, 31, std::time(nullptr), true});
    CHECK(bridge.GetTotalKills() == 11 && bridge.GetTotalDeaths() == 3);
    CHECK(bridge.GetKillTracker()->GetSessionDeaths() == 0); // Native victim RPCs do not count.
    bridge.ProcessIncomingChatLineForTesting(nullptr,
        "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abl\xC3\xA4uft.");
    bridge.Update(); // The cemetery line is the live death confirmed after this sync request.
    CHECK(bridge.GetTotalKills() == 11 && bridge.GetTotalDeaths() == 4);
    CHECK(bridge.GetKillTracker()->GetSessionKills() == 1 && bridge.GetKillTracker()->GetSessionDeaths() == 1);
    CHECK(CompleteFreshSyncDialogs(bridge, context, 10, 3, 21)); // Reply created before the real events.
    CHECK(bridge.GetTotalKills() == 11 && bridge.GetTotalDeaths() == 4);
    CHECK(context.commands == (std::vector<std::string>{"/stats", "/jobskills", "/stats", "/jobskills"}));
    CHECK(ManualSyncSuccessNotices(context) == 1 && !bridge.IsManualStateSynchronizationPending());
    bridge.ProcessDeathMessageForTesting({5, 11, 31, std::time(nullptr), true});
    CHECK(bridge.GetTotalKills() == 12 && bridge.GetKillTracker()->GetSessionKills() == 2);
    context.now += 9000;
    for (int i = 0; i < 4; ++i) bridge.Update(); // Unchanged collector is never reimported later.
    CHECK(bridge.GetTotalKills() == 12 && bridge.GetTotalDeaths() == 4);
    CHECK(bridge.GetKillTracker()->GetSessionKills() == 2 && bridge.GetKillTracker()->GetSessionDeaths() == 1);
    CHECK(ManualSyncSuccessNotices(context) == 1);
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
}

} // namespace

int RunStartupStatsSyncTests() {
    checks = failures = 0;
    TestOneFastRequestImportsBaselineAndPreservesChatChannelForSimulation();
    TestTimeoutDoesNotSpamOrClaimSuccessAndReconnectCanSyncAgain();
    TestOnlyAnIdentifiedOwnStatsDialogCloses();
    TestStatsCacheRoundTripAndOldConfigurations();
    TestAllStatsDialogStylesImportCloseAndSendRealTotalInSameFrame();
    TestManualStatsRecoversAfterTimeoutAndOtherDialogsStayOpen();
    TestObservedNovaScreenshotLayoutThroughCompressedRpcWithDynamicPlayerName();
    TestSpeedCommandPerformsThreeExactDialogRepliesAndSubmitsOnlyOnce();
    TestStartupSynchronizesStatsThenJobSkillsAndKeepsExistingEarnings();
    TestManualFullSyncReusesPipelineAndImportsFreshValuesWithoutNewKills();
    TestManualSyncTimeoutPreservesValuesAndCanBeRetried();
    TestManualSyncSaveFailureDoesNotClaimSuccessOrFetchJobSkills();
    TestManualSyncRequestRejectsBusyOrOfflineAndUiClosesOnlyWhenAccepted();
    TestManualSyncChatCountersAreImportedWithoutArtificialSessionEvents();
    TestManualSyncCannotCompleteAcrossPlayerIdentityChange();
    TestManualSyncPreservesRealEventsConfirmedAfterTheRequest();
    std::printf("StartupStatsSyncTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
