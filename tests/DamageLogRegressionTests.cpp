#define KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/RakHook/include/RakNet/BitStream.h"

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
    int sounds = 0;
    unsigned long tick = 1000;
    std::vector<std::string> messages;
    keybinder::SettingsManager settings;
    SAMPBridge bridge;
    bool ready = false;

    explicit DamageFixture(float initialHp = 100.0f, float initialArmour = 0.0f)
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
        hooks.sendInput = [](sampapi::v03dl::CInput*, const char*) {};
        hooks.sendChat = [](sampapi::v03dl::CLocalPlayer*, const char*) {};
        hooks.addMessage = [this](sampapi::v03dl::CChat*, unsigned long, const char* text) {
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
        hooks.getLocalPlayerName = [] { return std::string("DamagePlayer"); };
        hooks.readLocalDamageSample = [this](float& health, float& vest, int& weapon, const void*& identity) {
            health = hp; vest = armour; weapon = source; identity = ped; return available;
        };
        hooks.isStartupStatsReady = [] { return false; };
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
    bool Rpc(unsigned char id, float value) {
        RakNet::BitStream payload;
        payload.Write(value);
        const int offset = payload.GetReadOffset();
        bridge.ProcessScriptedHealthRpcForTesting(id, &payload);
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
#undef RUN
    std::printf("DamageLogRegressionTests: %d checks, %d failures\n", checks, failed);
    return failed;
}
