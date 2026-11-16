#include "../include/SAMPBridge.h"
#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/RakHook/include/RakNet/BitStream.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] PoliceLocateBridge: %s (line %d)\n", #value, __LINE__); } } while (0)

template<class Type> struct FakeObject {
    std::aligned_storage_t<sizeof(Type), alignof(Type)> bytes{};
    Type* get() { return reinterpret_cast<Type*>(&bytes); }
};

constexpr float markerX = 1927.8734f;
constexpr float markerY = 168.8767f;
constexpr float markerZ = 40.1172f;
constexpr float markerRadius = 4.0f;
const char* startLine = "Detektiv: Das Handy und die Ortung von Lesch wurde gestartet und dauert 30 Sekunden.";
const char* successLine = "Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt.";
const char* expectedRadio = "/r [ORTUNG] Gesuchter Lesch wurde zuletzt in [Palomino Creek] geortet.";

struct Context {
    FakeObject<sampapi::v03dl::CInput> input;
    FakeObject<sampapi::v03dl::CChat> chat;
    FakeObject<sampapi::v03dl::CDialog> dialog;
    FakeObject<sampapi::v03dl::CGame> game;
    FakeObject<sampapi::v03dl::CNetGame> network;
    FakeObject<sampapi::v03dl::CLocalPlayer> local;
    std::string name = "PoliceOfficer";
    std::string faction = "SAPD";
    unsigned long now = 100000;
    bool inputAvailable = true;
    std::vector<std::string> commands, serverChat, localMessages;
    std::vector<std::tuple<float, float, float>> resolvedPositions;

    Context() {
        network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
        std::strcpy(network.get()->m_szHostAddress, "127.0.0.1");
        network.get()->m_nPort = 7777;
        local.get()->m_bIsActive = TRUE;
        local.get()->m_pPed = reinterpret_cast<sampapi::v03dl::CPed*>(0x2000);
    }

    void ApplyMarker(float x = markerX, float y = markerY, float z = markerZ) {
        auto& checkpoint = game.get()->m_checkpoint;
        checkpoint.m_position.x = x;
        checkpoint.m_position.y = y;
        checkpoint.m_position.z = z;
        checkpoint.m_size.x = checkpoint.m_size.y = checkpoint.m_size.z = markerRadius;
        checkpoint.m_bEnabled = TRUE;
    }

    void ApplyRacingMarker(float x = markerX, float y = markerY, float z = markerZ) {
        auto& checkpoint = game.get()->m_racingCheckpoint;
        checkpoint.m_currentPosition.x = x;
        checkpoint.m_currentPosition.y = y;
        checkpoint.m_currentPosition.z = z;
        checkpoint.m_fSize = markerRadius;
        checkpoint.m_bEnabled = TRUE;
    }

    SAMPBridge::TestingHooks Hooks() {
        SAMPBridge::TestingHooks hooks{};
        hooks.getModuleHandle = [](const char*) { return reinterpret_cast<HMODULE>(1); };
        hooks.getSampBase = []() { return static_cast<uintptr_t>(1); };
        hooks.getRakHookVersion = []() { return rakhook::samp_ver::v03dlr1; };
        hooks.initializeRakHook = []() { return true; };
        hooks.destroyRakHook = []() {};
        hooks.getInput = [&]() { return inputAvailable ? input.get() : nullptr; };
        hooks.getChat = [&]() { return chat.get(); };
        hooks.getDialog = [&]() { return dialog.get(); };
        hooks.getGame = [&]() { return game.get(); };
        hooks.getNetGame = [&]() { return network.get(); };
        hooks.getLocalPlayer = [&]() { return local.get(); };
        hooks.getLocalPlayerName = [&]() { return name; };
        hooks.getLocalPlayerId = []() { return 5; };
        hooks.getPlayerName = [](int) { return std::string("OtherPlayer"); };
        hooks.getTickCount = [&]() { return now; };
        hooks.readLocalDamageSample = [&](float& health, float& armour, int& source, const void*& ped) {
            health = 100.0f; armour = 0.0f; source = 0; ped = local.get()->m_pPed; return true;
        };
        hooks.isStartupStatsReady = []() { return false; };
        hooks.isStartupJobSkillsReady = []() { return false; };
        hooks.getDistrictName = []() { return std::string("Market"); };
        hooks.getDistrictAtPosition = [&](float x, float y, float z) {
            resolvedPositions.emplace_back(x, y, z);
            return std::string("Palomino Creek");
        };
        hooks.sendInput = [&](sampapi::v03dl::CInput*, const char* text) { commands.emplace_back(text); };
        hooks.sendChat = [&](sampapi::v03dl::CLocalPlayer*, const char* text) { serverChat.emplace_back(text); };
        hooks.addMessage = [&](sampapi::v03dl::CChat*, unsigned long, const char* text) { localMessages.emplace_back(text); };
        hooks.getCommandHandler = [](sampapi::v03dl::CInput*, const char*) -> sampapi::CMDPROC { return nullptr; };
        hooks.addCommand = [](sampapi::v03dl::CInput*, const char*, sampapi::CMDPROC) {};
        hooks.setDefaultCommand = [](sampapi::v03dl::CInput*, sampapi::CMDPROC) {};
        hooks.installIncomingChatInterceptor = [](const auto&) {};
        return hooks;
    }
};

struct Fixture {
    Context context;
    SAMPBridge bridge;

    Fixture() {
        SAMPBridge::SetTestingHooks(context.Hooks());
        bridge.SetLocalFactionProvider([&]() { return context.faction; });
        CHECK(bridge.Initialize());
        bridge.Update(); // Establish a verified session before server events.
        CHECK(context.commands.empty());
    }
    ~Fixture() {
        bridge.Shutdown();
        SAMPBridge::ResetTestingHooks();
    }
    void Chat(const char* text, const char* prefix = nullptr) {
        CHECK(bridge.ProcessIncomingChatLineForTesting(prefix, text));
    }
    void Start() {
        Chat(startLine);
        bridge.Update();
        CHECK(context.commands.empty());
    }
    void Marker(bool apply = true) {
        RakNet::BitStream payload;
        payload.Write(markerX); payload.Write(markerY); payload.Write(markerZ); payload.Write(markerRadius);
        CHECK(bridge.ProcessServerCheckpointRpcForTesting(107, &payload));
        CHECK(payload.GetReadOffset() == 0);
        if (apply) context.ApplyMarker();
    }
    void DisableMarker() {
        CHECK(bridge.ProcessServerCheckpointRpcForTesting(37, nullptr));
        context.game.get()->m_checkpoint.m_bEnabled = FALSE;
    }
    void Complete() {
        context.now += 30000;
        Marker();
        Chat(successLine);
        bridge.Update();
    }
};

void ActualCheckpointApplicationAndMarkerDistrictAreRequired() {
    Fixture fixture;
    auto& context = fixture.context;
    auto& bridge = fixture.bridge;
    context.ApplyMarker(100.0f, 200.0f, 3.0f); // Old checkpoint, unrelated to this locate.
    fixture.Start();
    context.now += 30000;
    fixture.Marker(false); // The receive hook runs before native checkpoint application.
    fixture.Chat(successLine);
    bridge.Update();
    CHECK(context.commands.empty());
    CHECK(context.resolvedPositions.empty());
    context.ApplyMarker();
    ++context.now;
    bridge.Update();
    CHECK(context.commands == std::vector<std::string>{expectedRadio});
    CHECK(context.serverChat.empty());
    CHECK((context.resolvedPositions == std::vector<std::tuple<float, float, float>>{
        std::make_tuple(markerX, markerY, markerZ)}));
    CHECK(bridge.GetDistrictName() == "Market"); // Must not leak into radio's target district.
    ++context.now;
    fixture.Chat(successLine);
    fixture.Marker();
    bridge.Update();
    bridge.Update();
    CHECK(context.commands.size() == 1);
}

void SuccessBeforeCheckpointWorksAndDuplicateHooksDoNotRepeatRadio() {
    Fixture fixture;
    fixture.Start();
    fixture.context.now += 30000;
    fixture.Chat(successLine);
    fixture.bridge.Update();
    CHECK(fixture.context.commands.empty());
    fixture.Chat(successLine);
    ++fixture.context.now;
    fixture.Marker();
    fixture.Marker();
    fixture.bridge.Update();
    CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    CHECK(fixture.context.resolvedPositions.size() == 1);
    fixture.Chat(successLine);
    fixture.bridge.Update();
    CHECK(fixture.context.commands.size() == 1);
}

void DisabledAndMismatchedNativeMarkersCannotBeAnnounced() {
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 30000;
        fixture.Marker();
        fixture.Chat(successLine);
        fixture.DisableMarker();
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        CHECK(fixture.context.resolvedPositions.empty());
        fixture.context.ApplyMarker(); // A local GPS change is still not a fresh server event.
        ++fixture.context.now;
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        fixture.Marker();
        fixture.bridge.Update();
        CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    }
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 30000;
        fixture.Marker();
        fixture.Chat(successLine);
        fixture.context.game.get()->m_checkpoint.m_bEnabled = FALSE;
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        fixture.context.ApplyMarker(markerX + 0.26f, markerY, markerZ);
        ++fixture.context.now;
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        fixture.context.ApplyMarker(markerX, markerY, std::numeric_limits<float>::quiet_NaN());
        ++fixture.context.now;
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        CHECK(fixture.context.resolvedPositions.empty());
        fixture.context.ApplyMarker();
        ++fixture.context.now;
        fixture.bridge.Update();
        CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    }
}

void PlayerPrefixesCannotSpoofPersonalDetectiveMessages() {
    Fixture fixture;
    fixture.Chat(startLine, "OtherPlayer");
    fixture.bridge.Update();
    fixture.context.now += 30000;
    fixture.Marker();
    fixture.Chat(successLine);
    fixture.bridge.Update();
    CHECK(fixture.context.commands.empty());

    fixture.context.now += 1000;
    fixture.Start();
    fixture.context.now += 30000;
    fixture.Marker();
    fixture.Chat(successLine, "OtherPlayer");
    fixture.Chat("Detektiv: Du hast die Fähigkeit auf Stufe 191 verbessert.");
    fixture.bridge.Update();
    CHECK(fixture.context.commands.empty());
    fixture.Chat(successLine);
    fixture.bridge.Update();
    CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
}

void OnlyVerifiedPoliceAliasesCanUseTheFeature() {
    for (const std::string faction : {"", "Zivilist", "Red County Brotherhood", "RCB", "Army", "NotSAPD", "SAPD supporter"}) {
        Fixture fixture;
        fixture.context.faction = faction;
        fixture.bridge.Update();
        fixture.Start();
        fixture.Complete();
        CHECK(fixture.context.commands.empty());
        CHECK(fixture.context.resolvedPositions.empty());
    }
    for (const std::string faction : {"SAPD", "LSPD", "San Andreas Police Department", "Los Santos Police Department"}) {
        Fixture fixture;
        fixture.context.faction = faction;
        fixture.bridge.Update();
        fixture.Start();
        fixture.Complete();
        CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    }
}

void RevokedOrChangedIdentityDiscardsPendingEventsAndNeverReplaysThem() {
    // Each case changes identity or validity after receive callbacks queued a
    // complete result, proving Update revalidates before any radio output.
    for (int change = 0; change < 8; ++change) {
        Fixture fixture;
        auto& context = fixture.context;
        fixture.Start();
        context.now += 30000;
        fixture.Marker();
        fixture.Chat(successLine);
        if (change == 0) context.faction = "Red County Brotherhood";
        if (change == 1) context.faction.clear();
        if (change == 2) context.name = "OtherOfficer";
        if (change == 3) std::strcpy(context.network.get()->m_szHostAddress, "192.0.2.42");
        if (change == 4) context.network.get()->m_nPort = 8888;
        if (change == 5) context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_WAITCONNECT;
        if (change == 6) context.local.get()->m_bIsActive = FALSE;
        if (change == 7) context.local.get()->m_bIsWasted = TRUE;
        fixture.bridge.Update();
        CHECK(context.commands.empty());
        CHECK(context.resolvedPositions.empty());

        context.faction = "SAPD";
        context.name = "PoliceOfficer";
        std::strcpy(context.network.get()->m_szHostAddress, "127.0.0.1");
        context.network.get()->m_nPort = 7777;
        context.network.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
        context.local.get()->m_bIsActive = TRUE;
        context.local.get()->m_bIsWasted = FALSE;
        ++context.now;
        fixture.bridge.Update();
        fixture.Chat(successLine);
        fixture.Marker();
        fixture.bridge.Update();
        CHECK(context.commands.empty());
        context.now += 1000;
        fixture.Start();
        fixture.Complete();
        CHECK(context.commands == std::vector<std::string>{expectedRadio});
    }
}

void LocalGpsAndOldServerCheckpointsNeverCountAsLocatedTarget() {
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 30000;
        fixture.context.ApplyMarker(); // No incoming RPC, as with /setgps.
        fixture.Chat(successLine);
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        CHECK(fixture.context.resolvedPositions.empty());
    }
    {
        Fixture fixture;
        fixture.Marker();
        fixture.bridge.Update();
        fixture.Start();
        fixture.context.now += 30000;
        fixture.Chat(successLine);
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        CHECK(fixture.context.resolvedPositions.empty());
    }
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 1000;
        fixture.Marker();
        fixture.bridge.Update();
        fixture.context.now += 29000;
        fixture.Chat(successLine);
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        fixture.Marker();
        fixture.bridge.Update();
        CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    }
}

void ExpiredOperationsCannotSendAndUnavailableInputDefersOutput() {
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += keybinder::PoliceLocateTracker::kOperationTimeoutMs + 1;
        fixture.Marker();
        fixture.Chat(successLine);
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
    }
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 30000;
        fixture.Chat(successLine);
        fixture.bridge.Update();
        fixture.context.now += keybinder::PoliceLocateTracker::kCompletionWindowMs + 1;
        fixture.Marker();
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
    }
    {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 30000;
        fixture.Marker();
        fixture.Chat(successLine);
        fixture.context.inputAvailable = false;
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        fixture.context.inputAvailable = true;
        ++fixture.context.now;
        fixture.bridge.Update();
        CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    }
}

void OverflowDiscardsAmbiguousBatchesInsteadOfReplayingResults() {
    Fixture fixture;
    fixture.Start();
    fixture.context.now += 30000;
    fixture.Marker();
    fixture.Chat(successLine);
    for (int line = 0; line < 65; ++line) fixture.Chat("Ein normaler Serverhinweis");
    fixture.bridge.Update();
    CHECK(fixture.context.commands.empty());
    CHECK(fixture.context.resolvedPositions.empty());
    fixture.Chat(successLine);
    fixture.Marker();
    fixture.bridge.Update();
    CHECK(fixture.context.commands.empty());
}

void SuccessReadsCurrentMarkerEvenWhenNoCheckpointRpcWasReceived() {
    for (const bool racing : {false, true}) {
        Fixture fixture;
        fixture.Start();
        fixture.context.now += 1000; // The server success is authoritative, no forced 28-second gate.
        fixture.Chat(successLine);
        fixture.bridge.Update();
        CHECK(fixture.context.commands.empty());
        if (racing) fixture.context.ApplyRacingMarker();
        else fixture.context.ApplyMarker();
        fixture.context.now += keybinder::PoliceLocateTracker::kNativeCheckpointSettleMs;
        fixture.bridge.Update();
        CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
        CHECK((fixture.context.resolvedPositions == std::vector<std::tuple<float, float, float>>{
            std::make_tuple(markerX, markerY, markerZ)}));
        fixture.Chat(successLine);
        fixture.context.now += 1000;
        fixture.bridge.Update();
        CHECK(fixture.context.commands.size() == 1);
    }
    Fixture fixture;
    fixture.Start();
    fixture.context.now += 30000;
    fixture.Chat(successLine);
    fixture.bridge.Update();
    fixture.context.now += 8000;
    fixture.context.ApplyMarker(); // Marker arrived after the former five-second deadline.
    fixture.bridge.Update();
    CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    CHECK(fixture.bridge.GetDistrictName() == "Market");
}

void NewRacingCheckpointTakesPriorityOverAnOlderNormalCheckpoint() {
    Fixture fixture;
    fixture.context.ApplyMarker(100.0f, 200.0f, 3.0f);
    fixture.Start();
    fixture.context.now += 30000;
    RakNet::BitStream payload;
    payload.Write(static_cast<unsigned char>(0));
    payload.Write(markerX); payload.Write(markerY); payload.Write(markerZ);
    payload.Write(markerX + 10.0f); payload.Write(markerY); payload.Write(markerZ);
    payload.Write(markerRadius);
    CHECK(fixture.bridge.ProcessServerCheckpointRpcForTesting(38, &payload));
    CHECK(payload.GetReadOffset() == 0);
    fixture.context.ApplyRacingMarker();
    fixture.Chat(successLine);
    fixture.bridge.Update();
    CHECK(fixture.context.commands == std::vector<std::string>{expectedRadio});
    CHECK((fixture.context.resolvedPositions == std::vector<std::tuple<float, float, float>>{
        std::make_tuple(markerX, markerY, markerZ)}));
}

} // namespace

int RunPoliceLocateBridgeTests() {
    checks = failures = 0;
    ActualCheckpointApplicationAndMarkerDistrictAreRequired();
    SuccessBeforeCheckpointWorksAndDuplicateHooksDoNotRepeatRadio();
    DisabledAndMismatchedNativeMarkersCannotBeAnnounced();
    PlayerPrefixesCannotSpoofPersonalDetectiveMessages();
    OnlyVerifiedPoliceAliasesCanUseTheFeature();
    RevokedOrChangedIdentityDiscardsPendingEventsAndNeverReplaysThem();
    LocalGpsAndOldServerCheckpointsNeverCountAsLocatedTarget();
    ExpiredOperationsCannotSendAndUnavailableInputDefersOutput();
    OverflowDiscardsAmbiguousBatchesInsteadOfReplayingResults();
    SuccessReadsCurrentMarkerEvenWhenNoCheckpointRpcWasReceived();
    NewRacingCheckpointTakesPriorityOverAnOlderNormalCheckpoint();
    std::printf("PoliceLocateBridgeTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
