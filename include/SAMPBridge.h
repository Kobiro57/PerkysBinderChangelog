#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>
#include <tuple>
#include <vector>
#include <ctime>

#include <windows.h>

#include "ActionExecutor.h"
#include "KillTracker.h"
#include "KillfeedAdapter.h"
#include "DamageRPCAdapter.h"
#include "IncomingDamageDetector.h"
#include "ServerStatsSnapshot.h"
#include "StatsDialogRPCAdapter.h"
#include "SpeedTicketFlow.h"
#include "JobSkillsSnapshot.h"
#include "DeathMessageRPCAdapter.h"
#include "JobChatAdapter.h"
#include "JobTracker.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CChat.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CDialog.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CGame.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CInput.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CNetGame.h"
#include "SAMP-API/include/sampapi/0.3.DL-1/CLocalPlayer.h"

namespace rakhook {
enum class samp_ver : int;
}

namespace keybinder {
class SettingsManager;
}

class SAMPBridge : public keybinder::SAMPBridgeInterface, public keybinder::GameDataProvider {
public:
    enum class Version {
        Unknown,
        V03DLR1,
        Unsupported,
    };

    using CommandCallback = std::function<void(const std::string&)>;

    SAMPBridge();
    ~SAMPBridge();

    bool Initialize();
    void Shutdown();

    bool SendChat(const std::string& text) const override;
    bool SendCommand(const std::string& cmd) const override;
    void ObserveStatsCommand(const std::string& cmd) const;
    bool SendSimulatedKillMessage(const std::string& text, std::string& error) const;
    bool RequestSpeedTicket(const std::string& arguments, std::string& error);

    bool IsChatOpen() const;
    bool IsDialogOpen() const;
    bool IsCursorActive() const;
    void SetSettingsManager(const keybinder::SettingsManager* manager);
    void SetStatsSynchronizedCallback(std::function<bool(const keybinder::ServerStatsSnapshot&)> callback) { statsSynchronizedCallback_ = std::move(callback); }
    const keybinder::ServerStatsSnapshot& GetSynchronizedStats() const { return synchronizedStats_; }
    bool AreStatsSynchronized() const { return startupStatsComplete_; }
    bool AreJobSkillsSynchronized() const { return startupJobSkillsComplete_; }

    void Update();

    sampapi::v03dl::CGame* GetGame() const;
    sampapi::v03dl::CNetGame* GetNetGame() const;
    sampapi::v03dl::CLocalPlayer* GetLocalPlayer() const;

    bool RegisterCommand(const std::string& name, CommandCallback callback);
    Version GetVersion() const;
    
    void SetSlashCommandInterceptor(std::function<bool(const std::string&)> interceptor);
    bool InterceptSlashCommand(const std::string& text) const;
    void RegisterSlashCommands(const std::vector<std::string>& commands);
    void ClearSlashCommands();

    void InitializeKillTracker();
    void ShutdownKillTracker();
    void UpdateKillTracker();
    killtracker::KillTracker* GetKillTracker() const;
    bool IsKillTrackerEnabled() const;
    void SetKillTrackerEnabled(bool enabled);
    void SetKillTrackerLocalKillMessageEnabled(bool enabled);
    void SetKillTrackerLocalDeathMessageEnabled(bool enabled);
    void SetKillTrackerServerKillMessageEnabled(bool enabled);
    void SetKillTrackerServerDeathMessageEnabled(bool enabled);
    void SetKillTrackerTrackDamage(bool enabled);
    void SetHitmarkerDamageCallback(std::function<void(const killtracker::DamageEvent&)> callback);
    void SetHitsoundDamageCallback(std::function<void(const killtracker::DamageEvent&)> callback);
    void SetHitsoundIncomingCallback(std::function<void()> callback);
    void SetHitmarkerKillCallback(std::function<void(const killtracker::KillEvent&)> callback);
    void SetKillTrackerLocalKillMessageTemplate(const std::string& messageTemplate);
    void SetKillTrackerLocalDeathMessageTemplate(const std::string& messageTemplate);
    void SetKillTrackerServerKillMessageTemplate(const std::string& messageTemplate);
    void SetKillTrackerServerDeathMessageTemplate(const std::string& messageTemplate);
    void SetAutoHudScaleFix(bool enabled);
    void SetAutoEngineStart(bool enabled);
    void SetEngineInputBlockedCallback(std::function<bool()> callback) { engineInputBlocked_ = std::move(callback); }

    // Incoming chat line callback for external consumers (e.g. OnlineEventDetector)
    using IncomingChatCallback = std::function<void(const std::string&)>;
    void SetIncomingChatCallback(IncomingChatCallback callback);

    // Called from internal hooks to relay incoming chat text
    void NotifyIncomingChat(const std::string& text);

    void InitializeJobTracker();
    void ShutdownJobTracker();
    jobs::JobTracker* GetJobTracker() const;
    std::vector<jobs::JobEvent> ExportJobEvents() const;
    void ImportJobEvents(const std::vector<jobs::JobEvent>& events);

    std::vector<std::tuple<int, float, int>> ExportWeaponStats() const;
    void ImportWeaponStats(const std::vector<std::tuple<int, float, int>>& stats);
    std::vector<std::tuple<std::string, int, std::time_t, float>> ExportRecentKills() const;
    void ImportRecentKills(const std::vector<std::tuple<std::string, int, std::time_t, float>>& kills);
    int ExportKillCount() const;
    void ImportKillCount(int count);

    std::vector<std::tuple<std::string, int, std::time_t>> ExportRecentDeaths() const;
    void ImportRecentDeaths(const std::vector<std::tuple<std::string, int, std::time_t>>& deaths);
    int ExportDeathCount() const;
    void ImportDeathCount(int count);

#ifdef KEYBINDER_TESTING
    struct TestingHooks {
        std::function<HMODULE(const char*)> getModuleHandle;
        std::function<uintptr_t()> getSampBase;
        std::function<rakhook::samp_ver()> getRakHookVersion;
        std::function<bool()> initializeRakHook;
        std::function<void()> destroyRakHook;
        std::function<sampapi::v03dl::CInput*()> getInput;
        std::function<sampapi::v03dl::CChat*()> getChat;
        std::function<sampapi::v03dl::CDialog*()> getDialog;
        std::function<sampapi::v03dl::CGame*()> getGame;
        std::function<sampapi::v03dl::CNetGame*()> getNetGame;
        std::function<sampapi::v03dl::CLocalPlayer*()> getLocalPlayer;
        std::function<void(sampapi::v03dl::CInput*, const char*)> sendInput;
        std::function<void(sampapi::v03dl::CLocalPlayer*, const char*)> sendChat;
        std::function<void(sampapi::v03dl::CChat*, unsigned long, const char*)> addMessage;
        std::function<sampapi::CMDPROC(sampapi::v03dl::CInput*, const char*)> getCommandHandler;
        std::function<void(sampapi::v03dl::CInput*, const char*, sampapi::CMDPROC)> addCommand;
        std::function<void(sampapi::v03dl::CInput*, sampapi::CMDPROC)> setDefaultCommand;
        std::function<void(const std::function<void(sampapi::v03dl::CChat*, const char*, unsigned long, const char*)>&)> installIncomingChatInterceptor;
        std::function<void(const std::string&)> logSink;
        std::function<int()> getLocalPlayerId;
        std::function<std::string()> getLocalPlayerName;
        std::function<std::string(int)> getPlayerName;
        std::function<unsigned long()> getTickCount;
        std::function<bool(unsigned short&, bool&)> getDriverVehicle;
        std::function<bool()> isEngineInputAvailable;
        std::function<void(bool)> setEngineStartKeyDown;
        std::function<bool(float&, float&, int&, const void*&)> readLocalDamageSample;
        std::function<int()> getCityLevel;
        std::function<std::string()> getDistrictName;
        std::function<bool()> isStartupStatsReady;
        std::function<bool()> isStartupJobSkillsReady;
        std::function<void(int)> closeStatsDialog;
        std::function<bool(RakNet::BitStream*)> sendDialogResponse;
        std::function<void(int)> hideDialog;
    };

    static void SetTestingHooks(const TestingHooks& hooks);
    static void ResetTestingHooks();
    static const TestingHooks& GetTestingHooks();
    static std::string BuildTrackedIncomeMessageForTesting(int amount, int todayTotal);
    void ProcessDeathMessageForTesting(const killtracker::DeathMessageEvent& event);
    void ProcessDamageFeedbackForTesting(const killtracker::DamageEvent& event);
    void ProcessDamageRpcForTesting(RakNet::BitStream* payload);
    void RequestScriptedHealthSuppressionForTesting();
    bool ProcessIncomingChatLineForTesting(const char* prefix, const char* text);
    void ProcessStatsDialogForTesting(const keybinder::StatsDialogEvent& event);
    bool ProcessStatsDialogRpcForTesting(RakNet::BitStream* payload);
#endif

private:
    void HandleDeathMessage(const killtracker::DeathMessageEvent& event);
    void HandleLocalKill(killtracker::KillEvent event);
    void ObserveServerCombatStats(const std::string& text);
    void UpdateStartupStats(unsigned long now);
    void ResetStartupStats();
    void CaptureStartupStatsLine(const std::string& text);
    bool ShouldDisplayStartupStatsChat(const jobs::JobChatEvent& event);
    void CompleteStartupStats(unsigned long now);
    void InitializeStartupStatsAdapter();
    void CaptureStartupStatsDialog(const keybinder::StatsDialogEvent& event);
    bool AcceptStartupStatsDialog(const keybinder::StatsDialogEvent& event, unsigned long now);
    void CloseStartupStatsDialog(unsigned long now);
    void UpdateSpeedTicketFlow(unsigned long now);
    bool SendSpeedDialogChoice(const keybinder::SpeedTicketAction& action);
    void UpdateStartupJobSkills(unsigned long now);
    void UpdateAutoEngineStart(unsigned long now);
    void ObserveServerEngineStartMessage(const std::string& text);
    bool ReadDriverVehicle(unsigned short& vehicleId, bool& engineOn) const;
    bool IsEngineInputAvailable() const;
    void SetEngineStartKeyDown(bool down);
    struct CommandRegistration {
        std::string name;
        CommandCallback callback;
        std::size_t dispatcherIndex;
        bool directRegistered;
    };

    bool DetectAndValidateVersion();
    bool InitializeRakHook();
    bool RegisterPendingCommands();
    bool TryRegisterDirect(CommandRegistration& registration);
    bool InstallFallbackInterceptor();
    bool InstallSendHook();
    bool HasCommandRouting() const;
    void RestoreFallbackInterceptor();
    bool SendStartupMessage() const;

    sampapi::v03dl::CInput* GetInput() const;
    sampapi::v03dl::CChat* GetChat() const;
    sampapi::v03dl::CDialog* GetDialog() const;

    static std::string NormalizeCommandName(const std::string& name);
    static std::string ExtractCommandName(const char* text);
    static std::string ExtractCommandArguments(const char* text);
    static bool IsVisibleCursorMode(int cursorMode);
    static const char* VersionToString(Version version);
    std::string FormatKillTrackerLocalKillMessage(const killtracker::KillEvent& kill) const;
    std::string FormatKillTrackerLocalDeathMessage(const std::string& killer, int weaponId) const;
    std::string FormatKillTrackerServerKillMessage(const killtracker::KillEvent& kill) const;
    std::string FormatKillTrackerServerDeathMessage(const std::string& killer, int weaponId) const;
    std::string ExpandMessageVars(const std::string& tmpl,
                                  const std::string& victim,
                                  const std::string& killer,
                                  const std::string& weapon) const;

public:
    std::string GetCurrentZone() const;
    std::string GetLocalPlayerName() const;
    float GetLocalPlayerHP() const;
    float GetLocalPlayerArmour() const;
    float GetLocalPlayerCarHP() const;

    float GetPlayerHP() const override;
    float GetPlayerArmour() const override;
    float GetVehicleHP() const override;
    std::string GetVehicleName() const override;
    std::string GetCityName() const override;
    std::string GetDistrictName() const override;
    int GetTotalKills() const override;
    int GetTotalDeaths() const override;

    bool IsLocalPlayerDead() const;
    void ResetKillTrackerDeathInferenceState();
    void RecordLocalPlayerDeath(const std::string& killer, int weaponId);
    void UpdateLocalDeathFallback();
    void SendKillTrackerServerMessage(const std::string& message) const;

private:
    void EnsureKillTrackerRpcAdaptersRegistered();
    void RefreshDamageTracking();
    void PollLocalIncomingDamage();
    bool ReadLocalDamageSample(float& health, float& armour, int& damageSource, const void*& pedIdentity) const;
    bool IsLocalCombatActive(bool allowFinalDeathSample = false) const;
    void ResetIncomingDamageState();
    void NotifyDamageFeedback(const killtracker::DamageEvent& event);
    void LogIncomingDamage(const keybinder::IncomingDamageLogEntry& damage);
    bool IsDamageLogEnabled() const;
    void InstallScriptedHealthRpcHook();

    // Shared state between SAMPBridge and the ScrSetPlayerHealth/Armour RPC
    // hook. The RPC hook runs on an unknown (RakNet receive) thread, so it
    // must not touch the detector or read ped memory directly. Instead it
    // compares the incoming RPC float against the last polled ped HP/armour
    // (published by PollLocalIncomingDamage on the game thread) and, if the
    // value is a decrease, sets an atomic request that the game thread
    // consumes to arm the detector's one-shot suppression window.
    struct ScriptedHealthSuppressionState {
        std::atomic<bool> active{false};
        std::atomic<float> lastKnownHealth{0.0f};
        std::atomic<float> lastKnownArmour{0.0f};
        std::atomic<bool> suppressionRequested{false};
    };
    std::shared_ptr<ScriptedHealthSuppressionState> scriptedHealthState_;
    bool scriptedHealthHookInstalled_ = false;
public:
    bool SendLocalMessage(const std::string& text, unsigned long color = 0xFFFFFFFF) const;
    void QueueLocalMessage(const std::string& text, unsigned long color = 0xFFFFFFFF, int delayFrames = 0);
    static void __cdecl DispatchDirectCommand0(const char* params);
    static void __cdecl DispatchDirectCommand1(const char* params);
    static void __cdecl DispatchDirectCommand2(const char* params);
    static void __cdecl DispatchDirectCommand3(const char* params);
    static void __cdecl DispatchDirectCommand4(const char* params);
    static void __cdecl DispatchDirectCommand5(const char* params);
    static void __cdecl DispatchDirectCommand6(const char* params);
    static void __cdecl DispatchDirectCommand7(const char* params);
    static void __cdecl DefaultCommandInterceptor(const char* text);

private:
    void Log(const std::string& message) const;
    bool DispatchRegisteredCommand(std::size_t dispatcherIndex, const char* params) const;
    bool DispatchFallbackCommand(const char* text) const;

    Version version_;
    bool initialized_;
    bool rakhookInitialized_;
    bool fallbackInstalled_;
    bool missingModuleLogged_;
    bool unsupportedVersionLogged_;
    bool shutting_down_ = false;
    sampapi::CMDPROC originalDefaultCommand_;
    std::vector<CommandRegistration> commands_;

    bool startupMessagePending_;
    int startupMessageDelayFrames_;

    struct PendingLocalChatMessage {
        std::string text;
        unsigned long color = 0xFFFFFFFF;
        int delayFrames = 0;
    };
    std::vector<PendingLocalChatMessage> pendingLocalChatMessages_;

    bool hudScaleFixPending_;
    int hudScaleFixDelayFrames_;
    bool hudScaleFixEnabled_;

    bool autoEngineStartEnabled_;
    std::function<bool()> engineInputBlocked_;
    bool autoEngineStartPending_;
    unsigned long autoEngineStartAt_;
    unsigned long autoEngineStartHoldUntil_;
    bool autoEngineStartReleasePending_;
    unsigned short autoEngineObservedVehicle_ = 0xFFFF;
    bool autoEngineEntryConsumed_ = false;
    struct EngineServerStartNotice {
        unsigned long at = 0;
        unsigned short vehicle = 0xFFFF;
        unsigned long generation = 0;
    };
    std::mutex autoEngineNoticeMutex_;
    std::vector<EngineServerStartNotice> autoEngineServerNotices_;
    unsigned short autoEnginePublishedVehicle_ = 0xFFFF;
    unsigned long autoEnginePublishedGeneration_ = 0;
    unsigned long autoEngineEntryGeneration_ = 0;
    std::optional<EngineServerStartNotice> autoEnginePreSeatNotice_;

    bool killTrackerEnabled_;
    bool killTrackerLocalKillMessageEnabled_;
    bool killTrackerLocalDeathMessageEnabled_;
    bool killTrackerServerKillMessageEnabled_;
    bool killTrackerServerDeathMessageEnabled_;
    bool killTrackerTrackDamage_;
    const keybinder::SettingsManager* settingsManager_ = nullptr;
    std::string killTrackerLocalKillMessageTemplate_;
    std::string killTrackerLocalDeathMessageTemplate_;
    std::string killTrackerServerKillMessageTemplate_;
    std::string killTrackerServerDeathMessageTemplate_;
    std::unique_ptr<killtracker::KillTracker> killTracker_;
    std::unique_ptr<killtracker::KillfeedAdapter> killfeedAdapter_;
    std::unique_ptr<killtracker::DamageRPCAdapter> damageAdapter_;
    std::unique_ptr<killtracker::DeathMessageRPCAdapter> deathMessageAdapter_;
    std::unique_ptr<jobs::JobTracker> jobTracker_;
    std::unique_ptr<jobs::JobChatAdapter> jobChatAdapter_;
    std::function<void(const killtracker::DamageEvent&)> hitmarkerDamageCallback_;
    std::function<void(const killtracker::DamageEvent&)> hitsoundDamageCallback_;
    std::function<void(const killtracker::KillEvent&)> hitmarkerKillCallback_;
    std::function<void()> hitsoundIncomingCallback_;
    keybinder::IncomingDamageDetector incomingDamageDetector_;
    keybinder::IncomingDamageLog incomingDamageLog_;
    bool incomingDamageLogEnabledLastFrame_ = false;
    const void* incomingDamagePed_ = nullptr;
    bool killTrackerInitialized_;
    bool localPlayerWasDead_;
    bool localDeathHandledForCurrentLife_;
    int localDeathFallbackDelayFrames_;
    bool localDeathAwaitingNativeDead_ = false;
    unsigned long localDeathAwaitDeadline_ = 0;
    bool jobTrackerInitialized_;
    
    std::function<bool(const std::string&)> slashCommandInterceptor_;
    std::vector<std::string> registeredSlashCommands_;
    IncomingChatCallback incomingChatCallback_;
    bool serverCombatBaselineEstablished_ = false;
    bool ownStatsContext_ = false;
    unsigned long ownStatsContextTick_ = 0;
    int ownStatsContextLines_ = 0;
    int lastServerKillTotal_ = 0;
    int lastServerDeathTotal_ = 0;
    std::time_t lastLocalKillTime_ = 0;
    std::time_t lastLocalDeathTime_ = 0;
    keybinder::ServerStatsCollector startupStatsCollector_;
    std::unique_ptr<keybinder::StatsDialogRPCAdapter> statsDialogAdapter_;
    keybinder::ServerStatsSnapshot synchronizedStats_;
    std::function<bool(const keybinder::ServerStatsSnapshot&)> statsSynchronizedCallback_;
    bool startupStatsConnected_ = false;
    bool startupStatsRequested_ = false;
    bool startupStatsComplete_ = false;
    bool startupStatsFailed_ = false;
    mutable std::atomic<bool> ownStatsCommandObserved_{false};
    bool statsDialogRequestPending_ = false;
    unsigned long statsDialogRequestAt_ = 0;
    unsigned long startupStatsRequestedAt_ = 0;
    unsigned long startupStatsCompletedAt_ = 0;
    std::string startupStatsIdentity_;
    std::atomic<bool> captureStartupStats_{false};
    std::mutex startupStatsMutex_;
    std::vector<std::string> startupStatsLines_;
    std::vector<keybinder::StatsDialogEvent> startupStatsDialogs_;
    std::optional<keybinder::StatsDialogEvent> startupStatsDialogToClose_;
    unsigned long startupStatsDialogCloseAt_ = 0;
    std::string startupStatsOwnName_;
    bool startupStatsPendingChat_ = false;
    bool startupStatsHiddenChat_ = false;
    unsigned long startupStatsChatHeaderAt_ = 0;
    keybinder::SpeedTicketFlow speedTicketFlow_;
    std::atomic<bool> captureSpeedDialogs_{false};
    std::vector<keybinder::StatsDialogEvent> speedTicketDialogs_;
    std::vector<keybinder::StatsDialogEvent> pendingSpeedDialogs_;
    bool startupJobSkillsRequested_ = false;
    bool startupJobSkillsComplete_ = false;
    bool startupJobSkillsFailed_ = false;
    bool startupStatsSaved_ = false;
    bool startupSyncNotified_ = false;
    unsigned long startupJobSkillsRequestedAt_ = 0;
    bool jobSkillsRequestPending_ = false;
    unsigned long jobSkillsRequestAt_ = 0;
    mutable std::atomic<bool> ownJobSkillsCommandObserved_{false};
    std::vector<keybinder::StatsDialogEvent> jobSkillsDialogs_;
};
