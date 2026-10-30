#define KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/NativePlayerHealth.h"
#include "../include/RakHook/include/RakNet/BitStream.h"
#include "../include/RakHook/include/RakNet/PacketEnumerations.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace {
std::string failure;
int checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { failure = #value; return false; } } while (0)

template<class T> struct FakeObject {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage{};
    T* get() { return reinterpret_cast<T*>(&storage); }
};

struct DamageFixture {
    FakeObject<sampapi::v03dl::CInput> input;
    FakeObject<sampapi::v03dl::CChat> chat;
    FakeObject<sampapi::v03dl::CDialog> dialog;
    FakeObject<sampapi::v03dl::CGame> game;
    FakeObject<sampapi::v03dl::CNetGame> net;
    FakeObject<sampapi::v03dl::CLocalPlayer> local;
    float hp = 100.0f, armour = 100.0f;
    int source = keybinder::damage_source::kFall;
    const void* ped = reinterpret_cast<const void*>(0x1000);
    bool available = true;
    bool useNetworkVitals = false;
    unsigned char syncMode = ID_PLAYER_SYNC;
    unsigned short syncVehicle = 0xFFFF;
    int sounds = 0;
    unsigned long tick = 1000;
    std::vector<std::string> messages, localMessages, commands;
    std::string name = "DamagePlayer";
    bool statsReady = false;
    int closed = 0;
    keybinder::SettingsManager settings;
    SAMPBridge bridge;
    bool ready = false;

    explicit DamageFixture(float initialHp = 100.0f, float initialArmour = 0.0f, bool rawNative = false)
        : hp(initialHp), armour(initialArmour) {
        net.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
        local.get()->m_pPed = reinterpret_cast<sampapi::v03dl::CPed*>(0x2000);
        local.get()->m_bIsActive = TRUE;
        SAMPBridge::TestingHooks hooks;
        hooks.getModuleHandle = [](const char*) { return reinterpret_cast<HMODULE>(1); };
        hooks.getSampBase = [] { return uintptr_t(1); };
        hooks.getRakHookVersion = [] { return rakhook::samp_ver::v03dlr1; };
        hooks.initializeRakHook = [] { return true; };
        hooks.destroyRakHook = [] {};
        hooks.getInput = [this] { return input.get(); };
        hooks.getChat = [this] { return chat.get(); };
        hooks.getDialog = [this] { return dialog.get(); };
        hooks.getGame = [this] { return game.get(); };
        hooks.getNetGame = [this] { return net.get(); };
        hooks.getLocalPlayer = [this] { return local.get(); };
        hooks.getTickCount = [this] { return tick; };
        hooks.sendInput = [this](sampapi::v03dl::CInput*, const char* text) { commands.emplace_back(text); };
        hooks.sendChat = [](sampapi::v03dl::CLocalPlayer*, const char*) {};
        hooks.addMessage = [this](sampapi::v03dl::CChat*, unsigned long, const char* text) {
            if (text) localMessages.emplace_back(text);
            if (text && std::string(text).find(" HP {FFFFFF}Schaden (") != std::string::npos)
                messages.emplace_back(text);
        };
        hooks.getCommandHandler = [](sampapi::v03dl::CInput*, const char*) -> sampapi::CMDPROC { return nullptr; };
        hooks.addCommand = [](sampapi::v03dl::CInput*, const char*, sampapi::CMDPROC) {};
        hooks.setDefaultCommand = [](sampapi::v03dl::CInput* value, sampapi::CMDPROC command) {
            value->m_pDefaultCommand = command;
        };
        hooks.installIncomingChatInterceptor = [](const auto&) {};
        hooks.logSink = [](const std::string&) {};
        hooks.getLocalPlayerId = [] { return 5; };
        hooks.getLocalPlayerName = [this] { return name; };
        hooks.readLocalDamageSample = [this](float& health, float& vest, int& weapon, const void*& identity) {
            health = hp; vest = armour; weapon = source; identity = ped;
            if (useNetworkVitals) bridge.ApplyLocalSyncVitalsForTesting(health, vest, syncMode, syncVehicle, ped);
            return available;
        };
        if (rawNative) hooks.readRawLocalDamageSample = [this](float& health, float& vest, int& weapon, const void*& identity) {
            health = hp; vest = armour; weapon = source; identity = ped; return available;
        };
        hooks.getLocalNativePedState = [] { return 1; };
        hooks.isLocalPlayerControllable = [] { return true; };
        hooks.closeStatsDialog = [this](int) { ++closed; dialog.get()->m_bIsActive = FALSE; };
        hooks.isStartupStatsReady = [this] { return statsReady; };
        hooks.isStartupJobSkillsReady = [] { return false; };
        SAMPBridge::SetTestingHooks(hooks);
        settings.GetMutable().damage_log_enabled = true;
        settings.GetMutable().kill_tracker_count_kills = false;
        settings.GetMutable().kill_tracker_count_deaths = false;
        bridge.SetSettingsManager(&settings);
        bridge.SetKillTrackerEnabled(false);
        bridge.SetKillTrackerTrackDamage(false);
        bridge.SetHitsoundIncomingCallback([this] { ++sounds; });
        ready = bridge.Initialize();
        Step();
    }
    ~DamageFixture() { bridge.Shutdown(); SAMPBridge::ResetTestingHooks(); }
    void Step() { tick += 16; bridge.Update(); }
    void ArmLife() { for (int i = 0; i < 3; ++i) { tick += 400; Step(); } }
    void ReplyStats(int deaths) {
        const std::string caption = "Statistiken \xBB " + name;
        dialog.get()->m_bIsActive = TRUE;
        dialog.get()->m_nId = 499; dialog.get()->m_nType = 0;
        std::strcpy(dialog.get()->m_szCaption, caption.c_str());
        dialog.get()->m_szText = nullptr;
        bridge.ProcessStatsDialogForTesting({499, 0, caption,
            "Name: " + name + "\nKills: 65\nTode: " + std::to_string(deaths)});
        Step();
    }
    bool EnableVerifiedDeaths(int baseline = 214) {
        statsReady = true;
        settings.GetMutable().kill_tracker_count_deaths = true;
        bridge.SetKillTrackerEnabled(true);
        Step(); ReplyStats(baseline); ArmLife();
        return bridge.AreStatsSynchronized() && bridge.GetTotalDeaths() == baseline;
    }
    int DeathMessages() const {
        return static_cast<int>(std::count_if(localMessages.begin(), localMessages.end(), [](const auto& text) {
            return text.find("Du bist gestorben. Deine aktuelle Tode:") != std::string::npos;
        }));
    }
    bool Rpc(unsigned char id, float value) {
        RakNet::BitStream payload;
        payload.Write(value);
        const int offset = payload.GetReadOffset();
        bridge.ProcessScriptedHealthRpcForTesting(id, &payload);
        return offset == payload.GetReadOffset();
    }
    bool TakeDamage(float amount, unsigned int weapon = 54, bool incoming = true) {
        RakNet::BitStream payload;
        payload.Write(incoming);
        payload.Write(static_cast<unsigned short>(0xFFFF)); // No player attacker for a fall.
        payload.Write(amount);
        payload.Write(weapon);
        payload.Write(static_cast<unsigned int>(3));
        const int offset = payload.GetReadOffset();
        bridge.ProcessDamageRpcForTesting(&payload);
        return offset == payload.GetReadOffset();
    }
    bool Sync(unsigned char mode, unsigned char health, unsigned char vest, unsigned short vehicle = 0xFFFF) {
        RakNet::BitStream payload;
        payload.Write(mode);
        if (mode == ID_PLAYER_SYNC) {
            sampapi::v03dl::Synchronization::OnfootData data{};
            data.m_nHealth = health; data.m_nArmor = vest;
            payload.Write(data);
        } else if (mode == ID_VEHICLE_SYNC) {
            sampapi::v03dl::Synchronization::IncarData data{};
            data.m_nVehicle = vehicle; data.m_nDriverHealth = health; data.m_nDriverArmor = vest;
            payload.Write(data);
        } else if (mode == ID_PASSENGER_SYNC) {
            sampapi::v03dl::Synchronization::PassengerData data{};
            data.m_nVehicleId = vehicle; data.m_nHealth = health; data.m_nArmor = vest;
            payload.Write(data);
        }
        const int offset = payload.GetReadOffset();
        bridge.ProcessLocalSyncPacketForTesting(&payload);
        return offset == payload.GetReadOffset();
    }
    bool Message(std::size_t index, const char* amount, const char* kind = "HP Schaden") const {
        return index < messages.size() && messages[index] == std::string("{33CCFF}[NL-BINDER] {FF0000}-")
            + amount + " HP {FFFFFF}Schaden (" + kind + ")";
    }
};

bool NonlethalAndLethalFallsAreLoggedWithoutHitsounds() {
    DamageFixture f;
    CHECK(f.ready);
    f.hp = 82.5f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "17,5"));
    f.hp = 70.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "12,5"));
    f.hp = -8000000.0f;
    f.local.get()->m_bIsActive = FALSE;
    f.local.get()->m_bIsWasted = TRUE;
    f.Step(); f.Step();
    CHECK(f.messages.size() == 3 && f.Message(2, "70,0"));
    CHECK(f.sounds == 0);
    return true;
}

bool ExplosionLogsOnlyTheRemainingHealthAndArmourOnce() {
    DamageFixture f(50.0f, 25.0f);
    CHECK(f.ready);
    f.source = 51;
    f.hp = -8000000.0f; f.armour = -8000000.0f;
    f.Step(); f.Step();
    CHECK(f.messages.size() == 1);
    CHECK(f.Message(0, "75,0", "Weste + HP Schaden"));
    CHECK(f.sounds == 0);
    return true;
}

bool BriefInvalidNativeSamplesKeepTheSamePedsDamageBaseline() {
    DamageFixture f(100.0f, 25.0f);
    CHECK(f.ready);
    f.hp = 8000050.0f; f.Step();
    f.hp = 70.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "30,0"));
    f.hp = std::numeric_limits<float>::quiet_NaN(); f.Step();
    f.hp = 60.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "10,0"));
    f.hp = 8000050.0f;
    for (int i = 0; i < 4; ++i) f.Step();
    f.hp = 50.0f; f.Step();
    CHECK(f.messages.size() == 2); // Long protection interval deliberately rebases.
    f.hp = 40.0f; f.Step();
    CHECK(f.messages.size() == 3 && f.Message(2, "10,0"));
    CHECK(f.sounds == 0);
    return true;
}

bool InvalidArmourDoesNotHideARealFallHealthLoss() {
    DamageFixture f(100.0f, 50.0f);
    CHECK(f.ready);
    f.armour = 8000050.0f; f.hp = 80.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    f.armour = 40.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "10,0", "Weste getroffen"));
    return true;
}

bool ServerHealthAndArmourRpcsAreCombinedAndNativeReflectionIsNotCountedTwice() {
    DamageFixture f(100.0f, 100.0f);
    CHECK(f.ready);
    CHECK(f.Rpc(14, 80.0f) && f.Rpc(66, 50.0f));
    // Hooks run before the native handler: this frame may still show old HP.
    f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "70,0", "Weste + HP Schaden"));
    f.Step(); // Even another stale frame must not re-arm the old HP loss.
    f.hp = 80.0f; f.armour = 50.0f; f.Step();
    CHECK(f.messages.size() == 1);
    CHECK(f.Rpc(14, 80.0f) && f.Rpc(66, 50.0f)); f.Step(); f.Step();
    CHECK(f.messages.size() == 1); // Identical repeated server RPCs are no hit.
    CHECK(f.Rpc(14, 100.0f) && f.Rpc(66, 100.0f));
    f.hp = f.armour = 100.0f; f.Step(); f.Step();
    CHECK(f.messages.size() == 1); // Heal is not damage.
    f.hp = 90.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "10,0"));
    CHECK(f.sounds == 0);
    return true;
}

bool ServerDamageAndHealBetweenFramesStillLogsEveryLoss() {
    DamageFixture f;
    CHECK(f.ready);
    CHECK(f.Rpc(14, 90.0f));
    CHECK(f.Rpc(14, 80.0f));
    CHECK(f.Rpc(14, 100.0f));
    f.Step(); f.Step();
    CHECK(f.messages.size() == 2 && f.Message(0, "10,0") && f.Message(1, "10,0"));
    CHECK(f.sounds == 0);
    return true;
}

bool DuplicateRpcWithDelayedNativeHealthIsNotASecondHit() {
    DamageFixture f;
    CHECK(f.ready);
    CHECK(f.Rpc(14, 90.0f) && f.Rpc(14, 90.0f));
    f.Step(); f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "10,0"));
    CHECK(f.Rpc(14, 90.0f)); f.Step();
    f.hp = 90.0f; f.Step(); f.Step();
    CHECK(f.messages.size() == 1);
    return true;
}

bool NativeDamageAfterAServerHealIsNotLostWhenTheHealWasBetweenFrames() {
    DamageFixture f(90.0f, 0.0f);
    CHECK(f.ready);
    CHECK(f.Rpc(14, 100.0f));
    f.hp = 80.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    f.Step();
    CHECK(f.messages.size() == 1);
    return true;
}

bool ServerFallWhileNativeIsTransientAndLethalExplosionHaveFiniteAmounts() {
    DamageFixture f(50.0f, 0.0f);
    CHECK(f.ready);
    CHECK(f.Rpc(14, 30.0f));
    f.hp = 8000050.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    f.hp = 30.0f; f.Step();
    CHECK(f.messages.size() == 1);
    CHECK(f.Rpc(14, -8000000.0f));
    f.source = 51; f.hp = -8000000.0f;
    f.local.get()->m_bIsActive = FALSE;
    f.local.get()->m_bIsWasted = TRUE;
    f.Step(); f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "30,0"));
    return true;
}

bool LethalRpcBeforeNativeDeathNeverCountsTheRemainingHealthTwice() {
    DamageFixture f(50.0f, 0.0f);
    CHECK(f.ready);
    CHECK(f.Rpc(14, 0.0f));
    f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "50,0"));
    for (int i = 0; i < 6; ++i) f.Step(); // Native death is later than the ordinary RPC window.
    f.hp = 0.0f; f.Step(); f.Step();
    CHECK(f.messages.size() == 1);
    f.local.get()->m_bIsWasted = TRUE; f.local.get()->m_bIsActive = FALSE; f.Step();
    f.local.get()->m_bIsWasted = FALSE; f.local.get()->m_bIsActive = TRUE;
    f.hp = 100.0f; f.Step();
    f.hp = 90.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "10,0"));
    return true;
}

bool LoginToggleMissingPedAndPedReplacementDiscardOldDamage() {
    DamageFixture f;
    CHECK(f.ready);
    f.local.get()->m_bDoesSpectating = TRUE; f.Step();
    CHECK(f.Rpc(14, 20.0f));
    f.hp = 20.0f; f.Step();
    CHECK(f.messages.empty());
    f.local.get()->m_bDoesSpectating = FALSE; f.hp = 100.0f; f.Step();
    CHECK(f.Rpc(14, 80.0f));
    f.ped = reinterpret_cast<const void*>(0x3000); f.hp = 40.0f; f.Step();
    CHECK(f.messages.empty());
    f.hp = 30.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "10,0"));
    f.available = false; f.Step();
    f.available = true; f.hp = 20.0f; f.Step();
    CHECK(f.messages.size() == 1);
    f.settings.GetMutable().damage_log_enabled = false; f.Step();
    CHECK(f.Rpc(14, 10.0f)); f.hp = 10.0f; f.Step();
    f.settings.GetMutable().damage_log_enabled = true; f.Step();
    CHECK(f.messages.size() == 1);
    f.hp = 5.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "5,0"));
    return true;
}

bool EnhancedHealthIsNotCappedAtOneHundred() {
    DamageFixture f(5000.0f, 500.0f);
    CHECK(f.ready);
    f.hp = 4900.0f; f.armour = 450.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "150,0", "Weste + HP Schaden"));
    return true;
}

bool DamageRequestWithoutMeasuredLossDoesNotInventFallDamage() {
    DamageFixture f; CHECK(f.ready && f.TakeDamage(20.0f)); f.Step();
    CHECK(f.messages.empty());
    f.hp = 82.5f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "17,5"));
    CHECK(f.TakeDamage(8000050.0f, 51)); f.Step();
    CHECK(f.messages.size() == 1 && f.sounds == 0);
    return true;
}

bool ActualFallRpcAndMeasuredHpAreDeduplicatedInEitherOrder() {
    DamageFixture f;
    CHECK(f.ready && f.TakeDamage(20.0f)); f.Step();
    f.hp = 80.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    f.hp = 65.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "15,0"));
    CHECK(f.TakeDamage(15.0f)); f.Step();
    CHECK(f.messages.size() == 2);
    CHECK(f.Rpc(14, 55.0f) && f.TakeDamage(10.0f));
    f.hp = 55.0f; f.Step(); f.Step();
    CHECK(f.messages.size() == 3 && f.Message(2, "10,0"));
    return true;
}

bool GunAndFallDeltasAreBothLogged() {
    DamageFixture f; CHECK(f.ready);
    f.source = 24; f.hp = 80.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    CHECK(f.TakeDamage(20.0f)); f.source = 54; f.hp = 60.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "20,0"));
    return true;
}

bool AuthoritativeHealThenMeasuredFallUsesNewBasis() {
    DamageFixture f(50.0f, 0.0f); CHECK(f.ready);
    f.hp = 30.0f; f.Step(); CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    CHECK(f.Rpc(14, 50.0f)); f.hp = 50.0f; f.Step();
    CHECK(f.TakeDamage(40.0f)); f.hp = 10.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "40,0"));
    return true;
}

bool UnmeasurableVitalsDoNotInventDamageFromAnRpc() {
    DamageFixture f(100.0f, 0.0f); CHECK(f.ready);
    f.hp = std::numeric_limits<float>::quiet_NaN(); f.useNetworkVitals = true;
    CHECK(f.Sync(ID_PLAYER_SYNC, 248, 0)); CHECK(f.TakeDamage(12.5f)); f.Step();
    CHECK(f.messages.empty());
    CHECK(f.Sync(ID_PLAYER_SYNC, 0, 0)); f.Step(); CHECK(f.messages.empty());
    f.hp = 85.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "15,0"));
    return true;
}

bool OverkillRequestCannotSpendAnUnchangedHealthPool() {
    DamageFixture f(8000050.0f, 0.0f, true); f.source = 51;
    CHECK(f.ready && f.TakeDamage(50000.0f, 51)); f.Step(); CHECK(f.messages.empty());
    CHECK(f.Sync(ID_PLAYER_SYNC, 248, 0)); f.Step(); CHECK(f.messages.empty());
    f.hp = 8000000.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "50,0"));
    return true;
}

bool DelayedNativeLossIsLoggedOnceAfterAnEarlierDamageRequest() {
    DamageFixture f; CHECK(f.ready && f.TakeDamage(20.0f)); f.Step(); CHECK(f.messages.empty());
    f.tick += 800; f.hp = 80.0f; f.Step(); CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    CHECK(f.TakeDamage(10.0f)); f.Step();
    f.tick += 800; CHECK(f.Rpc(14, 70.0f)); f.hp = 70.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "10,0"));
    return true;
}

bool EarlierDamageRequestDoesNotConsumeALaterRealFall() {
    DamageFixture f; CHECK(f.ready && f.TakeDamage(20.0f)); f.Step(); CHECK(f.messages.empty());
    for (int i = 0; i < 5; ++i) f.Step();
    f.tick += 800; f.hp = 80.0f; f.Step(); CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    return true;
}

bool ArmourHealingDoesNotCreateOrHideHealthDamage() {
    DamageFixture f(50.0f, 0.0f); CHECK(f.ready && f.TakeDamage(20.0f)); f.Step();
    CHECK(f.Rpc(66, 50.0f)); f.armour = 50.0f; f.Step();
    f.hp = 30.0f; f.Step(); CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    CHECK(f.TakeDamage(10.0f)); f.armour = 60.0f; f.Step(); f.hp = 20.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "10,0"));
    return true;
}

bool HealthHealingAndRepeatedFallsUseTheirActualMeasuredLoss() {
    DamageFixture f; CHECK(f.ready); f.hp = 80.0f; f.Step(); f.hp = 100.0f; f.Step();
    CHECK(f.TakeDamage(20.0f)); f.Step(); CHECK(f.messages.size() == 1);
    f.hp = 80.0f; f.Step(); f.hp = 60.0f; f.Step(); CHECK(f.messages.size() == 3);
    CHECK(f.Rpc(14, 100.0f)); f.hp = 100.0f; f.Step();
    CHECK(f.TakeDamage(20.0f)); f.hp = 80.0f; f.Step();
    CHECK(f.messages.size() == 4 && f.Message(3, "20,0"));
    return true;
}

bool ActualSentSyncVitalsMakeProtectedNativeHealthMeasurable() {
    DamageFixture f(8000050.0f, 0.0f, true);
    f.useNetworkVitals = true;
    CHECK(f.ready && f.Sync(ID_PLAYER_SYNC, 50, 0)); f.Step();
    CHECK(f.messages.empty());
    f.hp = 8000035.0f;
    CHECK(f.Sync(ID_PLAYER_SYNC, 35, 0)); f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "15,0"));
    CHECK(f.TakeDamage(15.0f)); f.Step();
    CHECK(f.messages.size() == 1); // The real RPC is the same observed fall.
    return true;
}

bool ExplosionDamageRequestWaitsForTheActualZeroHealthTransition() {
    DamageFixture f(8000050.0f, 0.0f, true); f.useNetworkVitals = true; f.source = 51;
    CHECK(f.ready && f.Sync(ID_PLAYER_SYNC, 50, 0)); f.Step();
    CHECK(f.TakeDamage(8000050.0f, 51)); f.Step(); CHECK(f.messages.empty());
    f.hp = 8000000.0f; CHECK(f.Sync(ID_PLAYER_SYNC, 0, 0)); f.Step(); f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "50,0"));
    return true;
}

bool LocalSyncFallbackRejectsOldWrongModeWrongVehicleAndWrongPed() {
    DamageFixture f(8000050.0f, 8000025.0f);
    CHECK(f.ready && f.Sync(ID_PLAYER_SYNC, 50, 25));
    float hp = f.hp, armour = f.armour;
    CHECK(f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_PLAYER_SYNC, 0xFFFF, f.ped));
    CHECK(hp == 8000050.0f && armour == 25.0f);
    hp = f.hp; armour = f.armour;
    CHECK(!f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_PASSENGER_SYNC, 42, f.ped));
    CHECK(!f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_PLAYER_SYNC, 0xFFFF,
        reinterpret_cast<const void*>(0x9999)));
    f.tick += 501;
    CHECK(!f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_PLAYER_SYNC, 0xFFFF, f.ped));
    CHECK(f.Sync(ID_VEHICLE_SYNC, 40, 20, 42));
    CHECK(!f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_VEHICLE_SYNC, 43, f.ped));
    CHECK(f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_VEHICLE_SYNC, 42, f.ped));
    CHECK(hp == 8000050.0f && armour == 20.0f);
    hp = f.hp; armour = f.armour;
    CHECK(f.Sync(ID_PASSENGER_SYNC, 30, 10, 42));
    CHECK(f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_PASSENGER_SYNC, 42, f.ped));
    CHECK(hp == 8000050.0f && armour == 10.0f);
    hp = 5000.0f; armour = 500.0f;
    CHECK(!f.bridge.ApplyLocalSyncVitalsForTesting(hp, armour, ID_PASSENGER_SYNC, 42, f.ped));
    CHECK(hp == 5000.0f && armour == 500.0f);
    return true;
}

bool IncomingRpcFallbackKeepsLoginToggleAndDirectionGuards() {
    DamageFixture f;
    CHECK(f.ready && f.TakeDamage(20.0f, 54, false)); f.Step();
    CHECK(f.messages.empty()); // GiveDamage cannot be mistaken for self damage.
    CHECK(f.TakeDamage(std::numeric_limits<float>::quiet_NaN())); f.Step();
    CHECK(f.messages.empty());
    f.local.get()->m_bDoesSpectating = TRUE; f.Step();
    CHECK(f.TakeDamage(20.0f)); f.Step();
    CHECK(f.messages.empty());
    f.local.get()->m_bDoesSpectating = FALSE; f.Step();
    f.settings.GetMutable().damage_log_enabled = false; f.Step();
    CHECK(f.TakeDamage(20.0f)); f.Step();
    CHECK(f.messages.empty());
    return true;
}

// Anonymized values and relative ordering from the actual live explosion.
// No process identifiers, memory addresses, screenshots or account files.
bool LiveExplosionTraceDecodesProtectedHealthWithoutNativeDeathFlags() {
    DamageFixture f(8000094.0f, 100.0f, true);
    CHECK(f.ready && f.EnableVerifiedDeaths());
    f.source = 51;
    f.hp = 8000091.5f; f.Step();
    f.hp = 8000092.0f; f.Step();
    f.armour = 17.5f; f.Step();
    f.armour = 18.0f; f.Step();
    CHECK(f.TakeDamage(139.5f, 51));
    f.hp = 7999945.0f; f.armour = 0.0f; f.Step();
    f.hp = 8000000.0f; f.Step();
    f.hp = 7999969.5f; f.Step();
    f.hp = 8000000.0f; f.Step();
    CHECK(f.messages.size() == 3);
    CHECK(f.Message(0, "2,5") && f.Message(1, "82,5", "Weste getroffen"));
    CHECK(f.Message(2, "110,0", "Weste + HP Schaden"));
    CHECK(f.local.get()->m_bIsActive && !f.local.get()->m_bIsWasted);
    CHECK(f.DeathMessages() == 0);
    f.ReplyStats(215); f.Step();
    CHECK(f.bridge.GetTotalDeaths() == 215 && f.bridge.GetKillTracker()->GetSessionDeaths() == 1);
    CHECK(f.DeathMessages() == 1 && f.closed == 2);
    CHECK(f.commands == std::vector<std::string>({"/stats", "/stats"}));
    f.hp = 8000100.0f; f.tick += 3000; f.ArmLife();
    f.bridge.ProcessIncomingChatLineForTesting("", "Willkommen auf dem Friedhof. Hier verweilst du, bis du entweder reanimiert wirst oder deine Zeit abläuft.");
    f.Step();
    CHECK(f.DeathMessages() == 1 && f.bridge.GetTotalDeaths() == 215);
    return true;
}

bool LiveGunDeathCannotWrapMinusEightHpInto248Damage() {
    DamageFixture f(8000042.0f, 0.0f, true);
    CHECK(f.ready && f.EnableVerifiedDeaths());
    f.source = 24;
    for (float hp : {8000034.0f, 8000026.0f, 8000018.0f, 8000009.0f, 8000001.0f, 7999992.0f}) {
        f.hp = hp; f.Step();
    }
    f.local.get()->m_onfootData.m_nHealth = 248; f.Step();
    CHECK(f.messages.size() == 6 && f.Message(5, "1,0"));
    for (const auto& text : f.messages) CHECK(text.find("248,0") == std::string::npos);
    f.ReplyStats(215); f.Step();
    CHECK(f.DeathMessages() == 1 && f.bridge.GetTotalDeaths() == 215);
    return true;
}

bool ProtectedFallsUseFloatHpWithoutAnySyncPacketOrDamageRpc() {
    DamageFixture f(8000100.0f, 0.0f, true); CHECK(f.ready);
    f.hp = 8000082.5f; f.Step();
    f.hp = 8000070.0f; f.Step();
    CHECK(f.messages.size() == 2 && f.Message(0, "17,5") && f.Message(1, "12,5"));
    CHECK(f.sounds == 0 && f.commands.empty());
    return true;
}

bool DelayedServerDeathIsVerifiedOnceAfterTheObservedGraveTransition() {
    DamageFixture f(8000050.0f, 0.0f, true); CHECK(f.EnableVerifiedDeaths());
    f.hp = 8000000.0f; f.Step(); f.Step(); f.ReplyStats(214);
    CHECK(f.DeathMessages() == 0 && f.bridge.GetTotalDeaths() == 214);
    f.hp = 8000100.0f; f.tick += 3000; f.Step();
    CHECK(f.commands.size() == 2);
    f.tick += 1000; f.Step(); f.ReplyStats(215); f.Step();
    CHECK(f.commands.size() == 3 && f.DeathMessages() == 1);
    CHECK(f.bridge.GetTotalDeaths() == 215 && f.bridge.GetKillTracker()->GetSessionDeaths() == 1);
    return true;
}

bool GraveRelogAndUnchangedStatsNeverInventAnotherDeath() {
    DamageFixture f(8000050.0f, 0.0f, true); CHECK(f.EnableVerifiedDeaths());
    f.hp = 8000000.0f; f.Step(); f.Step(); f.ReplyStats(215);
    CHECK(f.DeathMessages() == 1);
    f.net.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_WAITCONNECT; f.Step();
    f.name = "Different_GravePlayer";
    f.net.get()->m_nGameState = sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    f.Step(); f.ReplyStats(215);
    CHECK(f.DeathMessages() == 1 && f.bridge.GetTotalDeaths() == 215);
    f.hp = 8000100.0f; f.ArmLife(); // Funeral also restores HP and controls.
    f.hp = 8000000.0f; f.Step(); f.Step(); f.ReplyStats(215);
    f.tick += 4000; f.Step(); f.ReplyStats(215);
    CHECK(f.DeathMessages() == 1 && f.bridge.GetTotalDeaths() == 215);
    // A later actual death must still work; no fixed three-minute cooldown.
    f.hp = 8000100.0f; f.ArmLife();
    f.hp = 8000000.0f; f.Step(); f.Step(); f.ReplyStats(216);
    CHECK(f.DeathMessages() == 2 && f.bridge.GetTotalDeaths() == 216);
    return true;
}

bool InitiallyDeadLoginAndMissingStatsDoNotCreateASessionDeath() {
    DamageFixture f(8000000.0f, 0.0f, true); CHECK(f.EnableVerifiedDeaths());
    CHECK(f.DeathMessages() == 0 && f.bridge.GetKillTracker()->GetSessionDeaths() == 0);
    f.hp = 8000100.0f; f.ArmLife(); f.hp = 8000000.0f; f.Step(); f.Step();
    f.tick += 10001; f.Step();
    CHECK(f.DeathMessages() == 0 && f.bridge.GetTotalDeaths() == 214);
    return true;
}

bool AlreadyImportedDeathTotalIsNotIncrementedAgain() {
    DamageFixture f(8000050.0f, 0.0f, true); CHECK(f.EnableVerifiedDeaths());
    f.bridge.ObserveStatsCommand("/stats"); f.ReplyStats(215);
    f.dialog.get()->m_bIsActive = FALSE; // A manually opened window stays open by design.
    CHECK(f.DeathMessages() == 0 && f.bridge.GetTotalDeaths() == 215);
    f.hp = 8000000.0f; f.Step(); f.Step(); f.ReplyStats(215);
    CHECK(f.DeathMessages() == 1 && f.bridge.GetTotalDeaths() == 215);
    CHECK(f.bridge.GetKillTracker()->GetSessionDeaths() == 1);
    return true;
}

bool ProtectedServerHealthRpcsAndNativeFloatAreNotTwoDamageEvents() {
    DamageFixture f(8000050.0f, 25.0f, true); CHECK(f.ready);
    CHECK(f.Rpc(14, 8000030.0f)); f.Step();
    f.hp = 8000030.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "20,0"));
    CHECK(f.Rpc(14, 7999945.0f) && f.Rpc(66, 0.0f));
    f.hp = 7999945.0f; f.armour = 0.0f; f.Step(); f.Step();
    CHECK(f.messages.size() == 2 && f.Message(1, "55,0", "Weste + HP Schaden"));
    return true;
}

bool ProtectedExtendedHpKeepsTheActualAmountWithoutA100HpCap() {
    DamageFixture f(8005000.0f, 500.0f, true); CHECK(f.ready);
    f.hp = 8004900.0f; f.armour = 450.0f; f.Step();
    CHECK(f.messages.size() == 1 && f.Message(0, "150,0", "Weste + HP Schaden"));
    CHECK(keybinder::DecodeNativePlayerHealth(7999992.0f).value == 0.0f);
    CHECK(keybinder::DecodeNativePlayerHealth(8000000.0f).value == 0.0f);
    CHECK(keybinder::DecodeNativePlayerHealth(8000094.0f).value == 94.0f);
    CHECK(keybinder::DecodeNativePlayerHealth(5000.0f).value == 5000.0f);
    CHECK(!keybinder::DecodeNativePlayerHealth(5000.0f).protectedEncoding);
    return true;
}

bool DelayedInitialStatsCorrectionIsABaselineNotADeathCredit() {
    DamageFixture f(8000100.0f, 0.0f, true); CHECK(f.ready);
    f.bridge.ImportDeathCount(213);
    f.settings.GetMutable().kill_tracker_count_deaths = true;
    f.statsReady = true; f.bridge.SetKillTrackerEnabled(true);
    f.Step(); f.ArmLife(); // Initial server reply arrives after life is armed.
    f.ReplyStats(214);
    CHECK(f.bridge.GetTotalDeaths() == 214 && f.DeathMessages() == 0);
    f.hp = 8000000.0f; f.Step(); f.Step(); f.ReplyStats(214);
    f.tick += 4000; f.Step(); f.ReplyStats(214);
    CHECK(f.bridge.GetTotalDeaths() == 214 && f.DeathMessages() == 0);
    CHECK(f.bridge.GetKillTracker()->GetSessionDeaths() == 0);
    return true;
}

} // namespace

int RunDamageLogRegressionTests() {
    int failed = 0;
    checks = 0;
#define RUN(test) do { failure.clear(); if (test()) std::printf("[PASS] %s\n", #test); \
    else { ++failed; std::printf("[FAIL] %s: %s\n", #test, failure.c_str()); } } while (0)
    RUN(NonlethalAndLethalFallsAreLoggedWithoutHitsounds);
    RUN(ExplosionLogsOnlyTheRemainingHealthAndArmourOnce);
    RUN(BriefInvalidNativeSamplesKeepTheSamePedsDamageBaseline);
    RUN(InvalidArmourDoesNotHideARealFallHealthLoss);
    RUN(ServerHealthAndArmourRpcsAreCombinedAndNativeReflectionIsNotCountedTwice);
    RUN(ServerDamageAndHealBetweenFramesStillLogsEveryLoss);
    RUN(DuplicateRpcWithDelayedNativeHealthIsNotASecondHit);
    RUN(NativeDamageAfterAServerHealIsNotLostWhenTheHealWasBetweenFrames);
    RUN(ServerFallWhileNativeIsTransientAndLethalExplosionHaveFiniteAmounts);
    RUN(LethalRpcBeforeNativeDeathNeverCountsTheRemainingHealthTwice);
    RUN(LoginToggleMissingPedAndPedReplacementDiscardOldDamage);
    RUN(EnhancedHealthIsNotCappedAtOneHundred);
    RUN(DamageRequestWithoutMeasuredLossDoesNotInventFallDamage);
    RUN(ActualFallRpcAndMeasuredHpAreDeduplicatedInEitherOrder);
    RUN(GunAndFallDeltasAreBothLogged);
    RUN(AuthoritativeHealThenMeasuredFallUsesNewBasis);
    RUN(UnmeasurableVitalsDoNotInventDamageFromAnRpc);
    RUN(OverkillRequestCannotSpendAnUnchangedHealthPool);
    RUN(DelayedNativeLossIsLoggedOnceAfterAnEarlierDamageRequest);
    RUN(EarlierDamageRequestDoesNotConsumeALaterRealFall);
    RUN(ArmourHealingDoesNotCreateOrHideHealthDamage);
    RUN(HealthHealingAndRepeatedFallsUseTheirActualMeasuredLoss);
    RUN(ActualSentSyncVitalsMakeProtectedNativeHealthMeasurable);
    RUN(ExplosionDamageRequestWaitsForTheActualZeroHealthTransition);
    RUN(LocalSyncFallbackRejectsOldWrongModeWrongVehicleAndWrongPed);
    RUN(IncomingRpcFallbackKeepsLoginToggleAndDirectionGuards);
    RUN(LiveExplosionTraceDecodesProtectedHealthWithoutNativeDeathFlags);
    RUN(LiveGunDeathCannotWrapMinusEightHpInto248Damage);
    RUN(ProtectedFallsUseFloatHpWithoutAnySyncPacketOrDamageRpc);
    RUN(DelayedServerDeathIsVerifiedOnceAfterTheObservedGraveTransition);
    RUN(GraveRelogAndUnchangedStatsNeverInventAnotherDeath);
    RUN(InitiallyDeadLoginAndMissingStatsDoNotCreateASessionDeath);
    RUN(AlreadyImportedDeathTotalIsNotIncrementedAgain);
    RUN(ProtectedServerHealthRpcsAndNativeFloatAreNotTwoDamageEvents);
    RUN(ProtectedExtendedHpKeepsTheActualAmountWithoutA100HpCap);
    RUN(DelayedInitialStatsCorrectionIsABaselineNotADeathCredit);
#undef RUN
    std::printf("DamageLogRegressionTests: %d checks, %d failures\n", checks, failed);
    return failed;
}
