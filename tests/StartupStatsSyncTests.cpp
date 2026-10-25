#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/RakHook/include/RakHook/samp.hpp"
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
    unsigned long now = 1000;
    int closed = 0;
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
        return hooks;
    }
    std::size_t Completions() const {
        std::size_t count = 0;
        for (const auto& message : localMessages) if (message.find("Synchronisation abgeschlossen.") != std::string::npos) ++count;
        return count;
    }
};

const char* levelLine = "Level:[9] Erfahrungspunkte [21/500] Geschlecht: [Mann] Alter: [22] Bargeld: [1.234$] Kontostand: [5.000$]";
const char* countLine = "Kills: [12] Tode: [4] Fraktion: [Zivilist] Gang: [-] Nächster Zahltag in: [10 Min] Gold: [3] Eventcoins: [2]";

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
    CHECK(context.Completions() == 1);
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
    CHECK(context.Completions() == 1);
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
    CHECK(!bridge.AreStatsSynchronized() && context.Completions() == 0);
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
    CHECK(bridge.AreStatsSynchronized() && context.Completions() == 1);
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
        CHECK(context.Completions() == 1 && saved == 1);
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
        CHECK(context.Completions() == 1 && saved == 2);
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
    CHECK(!bridge.AreStatsSynchronized() && context.Completions() == 0);
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
    CHECK(bridge.AreStatsSynchronized() && context.Completions() == 1);
    CHECK(context.closed == 1 && !dialog->m_bIsActive);
    CHECK(bridge.GetTotalKills() == 67 && bridge.GetTotalDeaths() == 89);
    CHECK(context.commands == std::vector<std::string>{"/stats"});
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
    std::printf("StartupStatsSyncTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
