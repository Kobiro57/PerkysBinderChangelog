#include "../include/SAMPBridge.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <climits>
#include <regex>
#include <unordered_map>

#include "../include/Logger.h"
#include "../include/KeyInputUtils.h"
#include "../include/SettingsManager.h"
#include "../include/WeaponNames.h"
#include "../include/DeathMessageRPCAdapter.h"
#include "../include/KillMessageTest.h"
#include "../include/LocationNames.h"

#include "../include/RakHook/include/RakHook/rakhook.hpp"
#include "../include/SAMP-API/include/sampapi/sampapi.h"
#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/CPlayerPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CVehicle.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CModelInfo.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CText.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CVehicleModelInfo.h"
#include "CTheZones.h"
#include "common.h"
#endif
#include "safetyhook.hpp"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CPlayerInfo.h"

extern std::atomic<int> g_shutdown_phase;
static std::string StripSampColorCodes(const std::string& text);

namespace {
constexpr std::size_t kInvalidDispatcherIndex = static_cast<std::size_t>(-1);
constexpr std::size_t kMaxDirectCommands = 8;
constexpr unsigned long kStartupMessageColor = 0xFFFFFFFF;
constexpr int kTrackedIncomeMessageDelayFrames = 10;
constexpr int kLocalDeathFallbackDelayFrames = 12;
// A short entry guard gives an already queued server-addon start priority.
// Stopped engines no longer wait for the full seating-confirmation window.
constexpr unsigned long kAutoEngineStartDelayMs = 100;
constexpr unsigned long kAutoEngineRunningConfirmationMs = 1500;
constexpr unsigned long kAutoEngineStartHoldMs = 150;
constexpr unsigned long kAutoEngineServerNoticeWindowMs = 1000;

SAMPBridge* g_activeBridge = nullptr;
thread_local bool g_emittingBinderLocalMessage = false;

#ifndef KEYBINDER_TESTING
constexpr std::size_t kVehicleGameNameLength = 8;
constexpr uintptr_t kTheTextAddress = 0xC1B340;
constexpr uintptr_t kCTextGetAddress = 0x6A0050;

std::string CopyVehicleGameNameKey(const char* gameName, std::size_t maxLength)
{
    if (gameName == nullptr || maxLength == 0) {
        return {};
    }

    std::size_t length = 0;
    while (length < maxLength && gameName[length] != '\0') {
        ++length;
    }

    while (length > 0 && std::isspace(static_cast<unsigned char>(gameName[length - 1]))) {
        --length;
    }

    return std::string(gameName, length);
}

std::string FormatVehicleGameNameFallback(const std::string& gameName)
{
    std::string result;
    result.reserve(gameName.size());
    for (std::size_t index = 0; index < gameName.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(gameName[index]);
        result += static_cast<char>(index == 0 ? std::toupper(ch) : std::tolower(ch));
    }

    return result;
}

const char* TranslateVehicleGameNameKey(const std::string& gameNameKey)
{
    if (gameNameKey.empty()) {
        return nullptr;
    }

    using CTextGet = const char* (__thiscall*)(CText*, const char*);
    CText* text = reinterpret_cast<CText*>(kTheTextAddress);
    CTextGet getText = reinterpret_cast<CTextGet>(kCTextGetAddress);
    return getText(text, gameNameKey.c_str());
}
#endif

std::string ConvertUtf8ToGameEncoding(const std::string& text) {
    if (text.empty()) {
        return text;
    }

    const int wideLength = ::MultiByteToWideChar(CP_UTF8,
                                                 MB_ERR_INVALID_CHARS,
                                                 text.data(),
                                                 static_cast<int>(text.size()),
                                                 nullptr,
                                                 0);
    if (wideLength <= 0) {
        return text;
    }

    std::wstring wideText(static_cast<std::size_t>(wideLength), L'\0');
    if (::MultiByteToWideChar(CP_UTF8,
                              MB_ERR_INVALID_CHARS,
                              text.data(),
                              static_cast<int>(text.size()),
                              &wideText[0],
                              wideLength) <= 0) {
        return text;
    }

    const int encodedLength = ::WideCharToMultiByte(CP_ACP,
                                                    0,
                                                    wideText.data(),
                                                    wideLength,
                                                    nullptr,
                                                    0,
                                                    "?",
                                                    nullptr);
    if (encodedLength <= 0) {
        return text;
    }

    std::string encodedText(static_cast<std::size_t>(encodedLength), '\0');
    if (::WideCharToMultiByte(CP_ACP,
                              0,
                              wideText.data(),
                              wideLength,
                              &encodedText[0],
                              encodedLength,
                              "?",
                              nullptr) <= 0) {
        return text;
    }

    return encodedText;
}

std::string NormalizeUiOpenCommand(std::string command) {
    const std::size_t first = command.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = command.find_last_not_of(" \t\n\r");
    command = command.substr(first, last - first + 1);

    while (!command.empty() && command.front() == '/') {
        command.erase(command.begin());
    }

    const std::size_t separator = command.find_first_of(" \t\n\r");
    if (separator != std::string::npos) {
        command.resize(separator);
    }

    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return command;
}

std::string GetUiToggleHotkeyLabel(int hotkey) {
    if (hotkey == 0) {
        return "F2";
    }

    return keybinder::GetLocalizedVirtualKeyName(hotkey);
}

std::string FormatMoneyCompact(int amount) {
    return std::to_string(amount) + "$";
}

std::string ResolveJobDisplayName(const jobs::JobTracker* tracker, const std::string& jobId) {
    if (tracker != nullptr) {
        for (const jobs::JobDefinition& job : tracker->GetJobs()) {
            if (job.id == jobId) {
                return job.name;
            }
        }
    }

    return jobId;
}

std::string BuildTrackedIncomeMessage(const jobs::JobTracker* tracker, const jobs::JobEvent& event, int todayTotal) {
    const std::string jobDisplayName = ResolveJobDisplayName(tracker, event.jobId);
    return std::string("{33CCFF}[NLBinder]{FFFFFF} Einkommen")
        + (jobDisplayName.empty() ? std::string() : std::string(" in ") + jobDisplayName)
        + " erfasst: {00CC00}"
        + FormatMoneyCompact(event.amount)
        + "{FFFFFF} | Heute: {33CCFF}"
        + FormatMoneyCompact(todayTotal);
}

std::string FormatUiOpenOptions(const keybinder::SettingsManager* settingsManager) {
    if (settingsManager == nullptr) {
        return "/kb, /keybinder und F2";
    }

    const keybinder::Settings& settings = settingsManager->Get();
    const std::string primary = NormalizeUiOpenCommand(settings.ui_open_command_primary);
    const std::string secondary = NormalizeUiOpenCommand(settings.ui_open_command_secondary);

    std::string result;
    if (!primary.empty()) {
        result = "/" + primary;
    } else {
        result = "/kb";
    }

    if (!secondary.empty() && secondary != primary) {
        result += ", /" + secondary;
    }

    result += " und ";
    result += GetUiToggleHotkeyLabel(settings.ui_toggle_hotkey);
    return result;
}

HMODULE RealGetModuleHandle(const char* moduleName) {
    return ::GetModuleHandleA(moduleName);
}

uintptr_t RealGetSampBase() {
#ifdef KEYBINDER_TESTING
    return 1;
#else
    return sampapi::GetBase();
#endif
}

rakhook::samp_ver RealGetRakHookVersion() {
#ifdef KEYBINDER_TESTING
    return rakhook::samp_ver::v03dlr1;
#else
    return rakhook::samp_version();
#endif
}

bool RealInitializeRakHook() {
#ifdef KEYBINDER_TESTING
    return true;
#else
    return rakhook::initialize();
#endif
}

void RealDestroyRakHook() {
#ifndef KEYBINDER_TESTING
    // CRITICAL: Do NOT call rakhook::destroy() during process exit.
    // RakHook stores lambdas with captures (from DamageRPCAdapter) that reference
    // our objects. Calling destroy() during /q exit can cause crashes because:
    // 1. Our objects may already be partially destroyed
    // 2. The lambdas reference dangling 'this' pointers
    // 3. SA-MP's exit path may still trigger RakNet calls during teardown
    //
    // The OS will clean up all resources when the process exits anyway.
    // This is safer than trying to manipulate hooks during rapid shutdown.
    // 
    // Only destroy if we're doing a clean shutdown (not during /q).
    // We detect this by checking if we're in the shutdownRwEvent handler.
    if (g_shutdown_phase.load() == 2) {
        // Normal shutdown via shutdownRwEvent - safe to destroy
        rakhook::destroy();
        rakhook::initialized = false;
        rakhook::orig = nullptr;
    } else {
        // Emergency shutdown (e.g., /q command) - skip RakHook destroy
        // Just mark as not initialized to prevent callbacks from running
        rakhook::initialized = false;
    }
#endif
}

sampapi::v03dl::CInput* RealGetInput() {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    return sampapi::v03dl::RefInputBox();
#endif
}

sampapi::v03dl::CChat* RealGetChat() {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    return sampapi::v03dl::RefChat();
#endif
}

sampapi::v03dl::CDialog* RealGetDialog() {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    return sampapi::v03dl::RefDialog();
#endif
}

sampapi::v03dl::CGame* RealGetGame() {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    return sampapi::v03dl::RefGame();
#endif
}

sampapi::v03dl::CNetGame* RealGetNetGame() {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    return sampapi::v03dl::RefNetGame();
#endif
}

sampapi::v03dl::CLocalPlayer* RealGetLocalPlayer() {
#ifdef KEYBINDER_TESTING
    return nullptr;
#else
    auto* netGame = sampapi::v03dl::RefNetGame();
    if (netGame == nullptr) return nullptr;
    auto* playerPool = netGame->GetPlayerPool();
    if (playerPool == nullptr) return nullptr;
    return playerPool->GetLocalPlayer();
#endif
}

void RealSendChat(sampapi::v03dl::CLocalPlayer* localPlayer, const char* text) {
#ifdef KEYBINDER_TESTING
    (void)localPlayer;
    (void)text;
#else
    localPlayer->Chat(text);
#endif
}

void RealSendInput(sampapi::v03dl::CInput* input, const char* text) {
#ifdef KEYBINDER_TESTING
    (void)input;
    (void)text;
#else
    input->Send(text);
#endif
}

void RealAddMessage(sampapi::v03dl::CChat* chat, unsigned long color, const char* text) {
#ifdef KEYBINDER_TESTING
    (void)chat;
    (void)color;
    (void)text;
#else
    chat->AddMessage(color, text);
#endif
}

sampapi::CMDPROC RealGetCommandHandler(sampapi::v03dl::CInput* input, const char* name) {
#ifdef KEYBINDER_TESTING
    (void)input;
    (void)name;
    return nullptr;
#else
    return input->GetCommandHandler(name);
#endif
}

void RealAddCommand(sampapi::v03dl::CInput* input, const char* name, sampapi::CMDPROC handler) {
#ifdef KEYBINDER_TESTING
    (void)input;
    (void)name;
    (void)handler;
#else
    input->AddCommand(name, handler);
#endif
}

void RealSetDefaultCommand(sampapi::v03dl::CInput* input, sampapi::CMDPROC handler) {
#ifdef KEYBINDER_TESTING
    (void)input;
    (void)handler;
#else
    input->SetDefaultCommand(handler);
#endif
}

#ifdef KEYBINDER_TESTING
SAMPBridge::TestingHooks g_testingHooks;
#endif

unsigned long RuntimeTickCount() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getTickCount) return g_testingHooks.getTickCount();
#endif
    return GetTickCount();
}

HMODULE GetModuleHandleHook(const char* moduleName) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getModuleHandle) {
        return g_testingHooks.getModuleHandle(moduleName);
    }
#endif
    return RealGetModuleHandle(moduleName);
}

uintptr_t GetSampBaseHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getSampBase) {
        return g_testingHooks.getSampBase();
    }
#endif
    return RealGetSampBase();
}

rakhook::samp_ver GetRakHookVersionHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getRakHookVersion) {
        return g_testingHooks.getRakHookVersion();
    }
#endif
    return RealGetRakHookVersion();
}

bool InitializeRakHookHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.initializeRakHook) {
        return g_testingHooks.initializeRakHook();
    }
#endif
    return RealInitializeRakHook();
}

void DestroyRakHookHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.destroyRakHook) {
        g_testingHooks.destroyRakHook();
        return;
    }
#endif
    RealDestroyRakHook();
}

sampapi::v03dl::CInput* GetInputHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getInput) {
        return g_testingHooks.getInput();
    }
#endif
    return RealGetInput();
}

sampapi::v03dl::CChat* GetChatHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getChat) {
        return g_testingHooks.getChat();
    }
#endif
    return RealGetChat();
}

sampapi::v03dl::CDialog* GetDialogHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getDialog) {
        return g_testingHooks.getDialog();
    }
#endif
    return RealGetDialog();
}

sampapi::v03dl::CGame* GetGameHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getGame) {
        return g_testingHooks.getGame();
    }
#endif
    return RealGetGame();
}

sampapi::v03dl::CNetGame* GetNetGameHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getNetGame) {
        return g_testingHooks.getNetGame();
    }
#endif
    return RealGetNetGame();
}

sampapi::v03dl::CLocalPlayer* GetLocalPlayerHook() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getLocalPlayer) {
        return g_testingHooks.getLocalPlayer();
    }
#endif
    return RealGetLocalPlayer();
}

void SendChatHook(sampapi::v03dl::CLocalPlayer* localPlayer, const char* text) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.sendChat) {
        g_testingHooks.sendChat(localPlayer, text);
        return;
    }
#endif
    RealSendChat(localPlayer, text);
}

void SendInputHook(sampapi::v03dl::CInput* input, const char* text) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.sendInput) {
        g_testingHooks.sendInput(input, text);
        return;
    }
#endif
    RealSendInput(input, text);
}

void AddMessageHook(sampapi::v03dl::CChat* chat, unsigned long color, const char* text) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.addMessage) {
        g_testingHooks.addMessage(chat, color, text);
        return;
    }
#endif
    RealAddMessage(chat, color, text);

}

sampapi::CMDPROC GetCommandHandlerHook(sampapi::v03dl::CInput* input, const char* name) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getCommandHandler) {
        return g_testingHooks.getCommandHandler(input, name);
    }
#endif
    return RealGetCommandHandler(input, name);
}

void AddCommandHook(sampapi::v03dl::CInput* input, const char* name, sampapi::CMDPROC handler) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.addCommand) {
        g_testingHooks.addCommand(input, name, handler);
        return;
    }
#endif
    RealAddCommand(input, name, handler);
}

void SetDefaultCommandHook(sampapi::v03dl::CInput* input, sampapi::CMDPROC handler) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.setDefaultCommand) {
        g_testingHooks.setDefaultCommand(input, handler);
        return;
    }
#endif
    RealSetDefaultCommand(input, handler);
}

void LogMessage(const std::string& message) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.logSink) {
        g_testingHooks.logSink(message);
    }
#endif
    keybinder::Logger::Debug("[SAMPBridge] %s", message.c_str());
    std::string fullMessage = "[NLBinder][SAMPBridge] " + message + "\n";
    ::OutputDebugStringA(fullMessage.c_str());
}

constexpr std::array<sampapi::CMDPROC, kMaxDirectCommands> kDispatchers = {
    &SAMPBridge::DispatchDirectCommand0,
    &SAMPBridge::DispatchDirectCommand1,
    &SAMPBridge::DispatchDirectCommand2,
    &SAMPBridge::DispatchDirectCommand3,
    &SAMPBridge::DispatchDirectCommand4,
    &SAMPBridge::DispatchDirectCommand5,
    &SAMPBridge::DispatchDirectCommand6,
    &SAMPBridge::DispatchDirectCommand7,
};

safetyhook::InlineHook* g_sendHook = nullptr;
safetyhook::InlineHook* g_processInputHook = nullptr;
bool g_bypassSendHook = false;

void ClearManualInputText(sampapi::v03dl::CInput* input) {
    if (!input) return;
    input->m_szInput[0] = '\0';
    input->m_szCurrentBuffer[0] = '\0';
#ifndef KEYBINDER_TESTING
    if (input->m_pEditbox) {
        // DL-R1 CDXUTEditBox::SetTextA, also used by native ProcessInput.
        using SetTextA = void(__thiscall*)(CDXUTEditBox*, const char*, bool);
        reinterpret_cast<SetTextA>(sampapi::GetBase() + 0x85000)(input->m_pEditbox, "", false);
    }
#endif
}

void __fastcall HookedCInputProcessInput(sampapi::v03dl::CInput* input, void*) {
    if (g_activeBridge && g_activeBridge->IsAwaitingManualTextInput(input)) {
        std::string text;
#ifdef KEYBINDER_TESTING
        text = input->m_szInput;
#else
        if (input->m_pEditbox) {
            // DL-R1 GetTextA returns an ACP buffer; copy it before clearing.
            // It has NO stack argument (the native push 0x80 belongs to strncpy).
            using GetTextA = const char*(__thiscall*)(CDXUTEditBox*);
            const char* nativeText = reinterpret_cast<GetTextA>(sampapi::GetBase() + 0x850D0)(input->m_pEditbox);
            if (nativeText) text.assign(nativeText);
        }
#endif
        g_activeBridge->InterceptManualTextInput(text);
        // Native ProcessInput dispatches normal chat and known local commands
        // without CInput::Send. Never enter any of those routes for an answer.
        return;
    }
    if (g_processInputHook) g_processInputHook->thiscall<void>(input);
}

// __fastcall simulates __thiscall on x86 MSVC: first arg in ECX (this), second ignored (EDX), rest on stack
void __fastcall HookedCInputSend(sampapi::v03dl::CInput* pThis, void* /*edx*/, const char* szString) {
    if (!g_bypassSendHook && g_activeBridge != nullptr && szString != nullptr
        && g_activeBridge->InterceptManualTextInput(szString)) {
        return;
    }
    if (!g_bypassSendHook && g_activeBridge != nullptr && szString != nullptr && szString[0] == '/') {
        if (g_activeBridge->InterceptSlashCommand(szString)) {
            return;
        }
        g_activeBridge->ObserveStatsCommand(szString);
    }
    if (g_sendHook != nullptr) {
        g_sendHook->thiscall<void>(pThis, szString);
    }
}

} // namespace

SAMPBridge::SAMPBridge()
    : version_(Version::Unknown),
      initialized_(false),
      rakhookInitialized_(false),
      fallbackInstalled_(false),
      missingModuleLogged_(false),
      unsupportedVersionLogged_(false),
      originalDefaultCommand_(nullptr),
      startupMessagePending_(false),
      startupMessageDelayFrames_(0),
      hudScaleFixPending_(false),
      hudScaleFixDelayFrames_(0),
      hudScaleFixEnabled_(false),
      autoEngineStartEnabled_(false),
      autoEngineStartPending_(false),
      autoEngineStartAt_(0),
      autoEngineStartHoldUntil_(0),
      autoEngineStartReleasePending_(false),
      killTrackerEnabled_(false),
      killTrackerLocalKillMessageEnabled_(true),
      killTrackerLocalDeathMessageEnabled_(true),
      killTrackerServerKillMessageEnabled_(false),
      killTrackerServerDeathMessageEnabled_(false),
      killTrackerTrackDamage_(true),
      killTrackerLocalKillMessageTemplate_("{FF6600}[NL-BINDER] {FFFFFF}Kill in {city}, {bezirk}. Kills: {total_kills}"),
      killTrackerLocalDeathMessageTemplate_("{FF3333}[NL-BINDER] {FFFFFF}Du bist gestorben. Deine aktuellen Tode: {total_deaths}"),
      killTrackerServerKillMessageTemplate_("Ich habe einen Kill in {city}, {bezirk}. Meine Kills: {total_kills}"),
      killTrackerServerDeathMessageTemplate_("Ich bin gestorben. Meine Tode: {total_deaths}"),
      killTrackerInitialized_(false),
      localPlayerWasDead_(false),
      localDeathHandledForCurrentLife_(false),
      localDeathFallbackDelayFrames_(0),
      jobTrackerInitialized_(false),
      scriptedHealthState_(std::make_shared<ScriptedHealthSuppressionState>()) {}

SAMPBridge::~SAMPBridge() {
    // CRITICAL: During process exit (DLL_PROCESS_DETACH with lpReserved != NULL),
    // we must NOT attempt any cleanup. The OS will reclaim all resources.
    // The destructor is called during static object destruction, which happens
    // after DLL_PROCESS_DETACH if shutdownRwEvent didn't fire.
    //
    // Only attempt cleanup during NORMAL shutdown (phase == 2 or 3).
    // During emergency exit (phase == 0 or 1), do absolutely nothing.
    if (g_shutdown_phase.load() != 2 && g_shutdown_phase.load() != 3) {
        // Emergency exit - DO NOTHING
        OutputDebugStringA("[SAMPBridge] Destructor skipped - emergency exit");
        return;
    }
    Shutdown();
}

bool SAMPBridge::Initialize() {
    if (initialized_) {
        return true;
    }

    if (GetModuleHandleHook("samp.dll") == nullptr || GetSampBaseHook() == 0) {
        if (!missingModuleLogged_) {
            Log("SA-MP is not loaded yet; bridge initialization deferred.");
            missingModuleLogged_ = true;
        }
        return false;
    }

    missingModuleLogged_ = false;

    if (!DetectAndValidateVersion()) {
        return false;
    }

    g_activeBridge = this;
    EnsureKillTrackerRpcAdaptersRegistered();

    if (!InitializeRakHook()) {
        if (g_activeBridge == this) {
            g_activeBridge = nullptr;
        }
        return false;
    }
    RegisterPendingCommands();
    InstallSendHook();

    InitializeJobTracker();

    // Install the scripted-health RPC hook after RakHook is ready so server-
    // side SetPlayerHealth/SetPlayerArmour RPCs can arm suppression windows
    // and prevent false incoming hitsounds on script-driven HP changes.
    InstallScriptedHealthRpcHook();
    InitializeStartupStatsAdapter();

    initialized_ = true;
    // Damage feedback/logging stays available independently of the counters.
    RefreshDamageTracking();
    keybinder::Logger::Info("[SAMPBridge] Attached to SA-MP %s.", VersionToString(version_));
    
    if (killTrackerEnabled_) {
        InitializeKillTracker();
    }
    
    startupMessagePending_ = true;
    startupMessageDelayFrames_ = 30;

    if (hudScaleFixEnabled_) {
        hudScaleFixPending_ = true;
        hudScaleFixDelayFrames_ = 120;
    }

    return true;
}

bool SAMPBridge::InstallSendHook() {
#ifdef KEYBINDER_TESTING
    return true;
#else
    if (g_processInputHook == nullptr) {
        auto processHook = safetyhook::InlineHook::create(
            reinterpret_cast<void*>(sampapi::GetBase() + 0x69410),
            reinterpret_cast<void*>(&HookedCInputProcessInput));
        if (processHook) g_processInputHook = new safetyhook::InlineHook(std::move(*processHook));
        else Log("WARNING: Native chat input hook unavailable - manual bind input disabled.");
    }
    if (g_sendHook != nullptr) {
        return true;
    }

    void* sendFn = reinterpret_cast<void*>(sampapi::GetBase() + 0x69340);
    auto hook = safetyhook::InlineHook::create(sendFn, reinterpret_cast<void*>(&HookedCInputSend));
    if (!hook) {
        Log("WARNING: Failed to install CInput::Send hook - slash commands will trigger 'Unknown command' from server.");
        return false;
    }

    g_sendHook = new safetyhook::InlineHook(std::move(*hook));
    Log("CInput::Send hook installed - slash command keybinds will be silently consumed.");
    return true;
#endif
}

void SAMPBridge::Shutdown() {
    // Guard against double-cleanup during shutdown (can be called from both
    // shutdownRwEvent and destructor)
    if (shutting_down_) {
        Log("Shutdown already in progress, skipping");
        return;
    }
    shutting_down_ = true;
    CancelManualTextInput();
    if (statsDialogAdapter_) statsDialogAdapter_->Shutdown();
    statsDialogAdapter_.reset();
    ResetStartupStats();
    captureSpeedDialogs_.store(false, std::memory_order_release);
    speedTicketFlow_.Reset();
    pendingSpeedDialogs_.clear();

    Log("Shutting down SAMPBridge");
    SetAutoEngineStart(false);
    ResetIncomingDamageState();

    delete g_sendHook;
    g_sendHook = nullptr;
    delete g_processInputHook;
    g_processInputHook = nullptr;

    ShutdownJobTracker();

    ShutdownKillTracker();
    if (damageAdapter_) damageAdapter_->Shutdown();
    if (deathMessageAdapter_) deathMessageAdapter_->Shutdown();

    RestoreFallbackInterceptor();

    if (rakhookInitialized_) {
        Log("Destroying RakHook");
        DestroyRakHookHook();
        rakhookInitialized_ = false;
        Log("RakHook detached.");
    }

    for (auto& command : commands_) {
        command.directRegistered = false;
    }

    initialized_ = false;
    version_ = Version::Unknown;
    if (g_activeBridge == this) {
        g_activeBridge = nullptr;
    }

    Log("SAMPBridge shutdown complete");
}

bool SAMPBridge::SendChat(const std::string& text) const {
    auto* localPlayer = GetLocalPlayer();
    if (localPlayer == nullptr || text.empty()) {
        return false;
    }

    const std::string encodedText = ConvertUtf8ToGameEncoding(text);
    SendChatHook(localPlayer, encodedText.c_str());
    return true;
}

bool SAMPBridge::SendLocalMessage(const std::string& text, unsigned long color) const {
    auto* chat = GetChat();
    if (chat == nullptr || text.empty()) {
        return false;
    }
    const std::string encodedText = ConvertUtf8ToGameEncoding(text);
    const bool previous = g_emittingBinderLocalMessage;
    g_emittingBinderLocalMessage = true;
    AddMessageHook(chat, color, encodedText.c_str());
    g_emittingBinderLocalMessage = previous;
    return true;
}

void SAMPBridge::QueueLocalMessage(const std::string& text, unsigned long color, int delayFrames) {
    if (text.empty()) {
        return;
    }

    const int normalizedDelayFrames = (std::max)(0, delayFrames);
    if (normalizedDelayFrames == 0 && SendLocalMessage(text, color)) {
        return;
    }

    pendingLocalChatMessages_.push_back({ text, color, normalizedDelayFrames });
}

bool SAMPBridge::SendCommand(const std::string& cmd) const {
    auto* input = GetInput();
    if (input == nullptr || cmd.empty()) {
        return false;
    }

    std::string normalized = cmd;
    if (normalized.front() != '/') {
        normalized.insert(normalized.begin(), '/');
    }

    const std::string encodedText = ConvertUtf8ToGameEncoding(normalized);
    ObserveStatsCommand(normalized);
    g_bypassSendHook = true;
    SendInputHook(input, encodedText.c_str());
    g_bypassSendHook = false;
    return true;
}

void SAMPBridge::SendKillTrackerServerMessage(const std::string& message) const {
    if (message.empty()) return;

    if (message.front() == '/') {
        SendCommand(message);
    } else {
        SendChat(message);
    }
}

bool SAMPBridge::SendSimulatedKillMessage(const std::string& text, std::string& error) const {
    std::string message;
    const std::string expanded = StripSampColorCodes(ExpandMessageVars(text, "", GetLocalPlayerName(), ""));
    if (!keybinder::BuildSimulatedKillMessage(expanded, message, error)) return false;
    const bool sent = message.front() == '/' ? SendCommand(message) : SendChat(message);
    if (!sent) error = "Die Testnachricht konnte gerade nicht gesendet werden.";
    return sent;
}

bool SAMPBridge::IsChatOpen() const {
    const auto* input = GetInput();
    return input != nullptr && input->m_bEnabled != FALSE;
}

bool SAMPBridge::BeginManualTextInput() {
    if (!initialized_) return false;
    auto* input = GetInput();
    auto* connection = GetNetGame();
    auto* player = GetLocalPlayer();
    if (!input || IsChatOpen() || IsDialogOpen()
        || !connection || connection->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        || !player || !player->m_bIsActive || player->m_bIsWasted
        || manualInputOwner_ != nullptr) {
        return false;
    }
#ifndef KEYBINDER_TESTING
    // Capturing must work before we ever invite the player to enter private text.
    if (!g_processInputHook || !input->m_pEditbox) return false;
#endif
    manualInputOwner_ = input;
    manualInputConnection_ = connection;
    manualInputValue_.clear();
    manualInputStatus_ = keybinder::ManualTextInputStatus::Waiting;
    ClearManualInputText(input);
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.openChatInput) g_testingHooks.openChatInput(input);
    else input->m_bEnabled = TRUE;
#else
    // Use SA-MP's own input lifecycle, including its cursor and game controls.
    input->Close();
    input->Open();
#endif
    if (!input->m_bEnabled) {
        CancelManualTextInput();
        return false;
    }
    SendLocalMessage("{33CCFF}[NL-BINDER]{FFFFFF} Gib hier deinen Input-Text ein: (Enter best\xC3\xA4tigt, Esc bricht ab)");
    return true;
}

keybinder::ManualTextInputStatus SAMPBridge::PollManualTextInput(std::string& value) {
    using Status = keybinder::ManualTextInputStatus;
    if (!manualInputOwner_) return Status::Cancelled;
    auto* connection = GetNetGame();
    auto* player = GetLocalPlayer();
    if (!initialized_ || GetInput() != manualInputOwner_ || connection != manualInputConnection_
        || !connection || connection->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        || !player || !player->m_bIsActive || player->m_bIsWasted || IsDialogOpen()) {
        CancelManualTextInput();
        return Status::Cancelled;
    }
    if (manualInputStatus_ == Status::Submitted) {
        value = std::move(manualInputValue_);
        manualInputOwner_ = nullptr;
        manualInputConnection_ = nullptr;
        manualInputStatus_ = Status::Cancelled;
        return Status::Submitted;
    }
    if (!manualInputOwner_->m_bEnabled) {
        CancelManualTextInput(); // Escape, a server dialog or a native input close.
        return Status::Cancelled;
    }
    return Status::Waiting;
}

void SAMPBridge::CancelManualTextInput() {
    auto* owner = manualInputOwner_;
    manualInputOwner_ = nullptr;
    manualInputConnection_ = nullptr;
    manualInputValue_.clear();
    manualInputStatus_ = keybinder::ManualTextInputStatus::Cancelled;
    if (owner && GetInput() == owner) {
        ClearManualInputText(owner);
        if (!owner->m_bEnabled) return;
#ifdef KEYBINDER_TESTING
        if (g_testingHooks.closeChatInput) g_testingHooks.closeChatInput(owner);
        else owner->m_bEnabled = FALSE;
#else
        owner->Close();
#endif
    }
}

bool SAMPBridge::InterceptManualTextInput(const std::string& gameText) {
    if (manualInputStatus_ != keybinder::ManualTextInputStatus::Waiting
        || !manualInputOwner_ || GetInput() != manualInputOwner_ || !manualInputOwner_->m_bEnabled) {
        return false;
    }
    if (gameText.find_first_not_of(" \t\r\n") == std::string::npos) {
        CancelManualTextInput();
        return true;
    }
    // Native chat uses the Windows game code page; action templates use UTF-8.
    const int wideLength = MultiByteToWideChar(CP_ACP, 0, gameText.data(), static_cast<int>(gameText.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>((std::max)(0, wideLength)), L'\0');
    if (wideLength > 0) {
        MultiByteToWideChar(CP_ACP, 0, gameText.data(), static_cast<int>(gameText.size()), wide.data(), wideLength);
        const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);
        manualInputValue_.resize(static_cast<std::size_t>((std::max)(0, utf8Length)));
        if (utf8Length > 0) WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, manualInputValue_.data(), utf8Length, nullptr, nullptr);
    }
    // Keep a single message even if another local component supplied line breaks.
    std::replace(manualInputValue_.begin(), manualInputValue_.end(), '\r', ' ');
    std::replace(manualInputValue_.begin(), manualInputValue_.end(), '\n', ' ');
    manualInputStatus_ = keybinder::ManualTextInputStatus::Submitted;
    ClearManualInputText(manualInputOwner_);
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.closeChatInput) g_testingHooks.closeChatInput(manualInputOwner_);
    else manualInputOwner_->m_bEnabled = FALSE;
#else
    manualInputOwner_->Close();
#endif
    return true; // Never forward the answer itself as chat or as a command.
}

bool SAMPBridge::IsAwaitingManualTextInput(const sampapi::v03dl::CInput* input) const {
    return input && input == manualInputOwner_ && input->m_bEnabled
        && manualInputStatus_ == keybinder::ManualTextInputStatus::Waiting;
}

#ifdef KEYBINDER_TESTING
void SAMPBridge::ProcessNativeChatInputForTesting(sampapi::v03dl::CInput* input) {
    HookedCInputProcessInput(input, nullptr);
}
#endif

bool SAMPBridge::IsDialogOpen() const {
    const auto* dialog = GetDialog();
    return dialog != nullptr && dialog->m_bIsActive != FALSE;
}

bool SAMPBridge::IsCursorActive() const {
    const auto* game = GetGame();
    if (game == nullptr) {
        return false;
    }

    int cursorMode = game->m_nCursorMode;
    static int lastLoggedMode = -1;
    if (cursorMode != lastLoggedMode) {
        keybinder::Logger::Info("[SAMPBridge] Cursor mode changed: %d", cursorMode);
        lastLoggedMode = cursorMode;
    }

    return IsVisibleCursorMode(cursorMode);
}

void SAMPBridge::SetSettingsManager(const keybinder::SettingsManager* manager) {
    settingsManager_ = manager;
    if (manager && killTracker_) {
        killTracker_->SetCountKills(manager->Get().kill_tracker_count_kills);
        killTracker_->SetCountDeaths(manager->Get().kill_tracker_count_deaths);
    }
    if (manager && !startupStatsComplete_ && manager->Get().synchronized_player_stats.HasCounters())
        synchronizedStats_ = manager->Get().synchronized_player_stats;
}

void SAMPBridge::Update() {
    if (!initialized_) {
        return;
    }

    const unsigned long now = RuntimeTickCount();

    if (!jobTrackerInitialized_) {
        InitializeJobTracker();
    }
    // Apply queued live combat confirmations before a same-frame /stats
    // snapshot, so an already included kill cannot be counted a second time.
    UpdateCombatChat(now);
    UpdateStartupStats(now);
    UpdateSpeedTicketFlow(now);
    UpdateStartupJobSkills(now);

    if (!HasCommandRouting()) {
        RegisterPendingCommands();
    }

    if (startupMessageDelayFrames_ > 0) {
        startupMessageDelayFrames_--;
    }
    if (hudScaleFixDelayFrames_ > 0) {
        hudScaleFixDelayFrames_--;
    }

    if (startupMessagePending_) {
        if (startupMessageDelayFrames_ == 0 && GetChat() != nullptr) {
            if (SendStartupMessage()) {
                startupMessagePending_ = false;
            }
        }
    }

    for (auto it = pendingLocalChatMessages_.begin(); it != pendingLocalChatMessages_.end();) {
        if (it->delayFrames > 0) {
            --it->delayFrames;
        }

        if (it->delayFrames <= 0) {
            if (SendLocalMessage(it->text, it->color)) {
                it = pendingLocalChatMessages_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    if (hudScaleFixPending_ && manualInputOwner_ == nullptr) {
        if (hudScaleFixDelayFrames_ == 0) {
            auto* input = GetInput();
            if (input != nullptr) {
                auto handler = GetCommandHandlerHook(input, "hudscalefix");
                if (handler != nullptr) {
                    handler("");
                    Log("Auto-executed /hudscalefix via direct handler call");
                } else {
#ifndef KEYBINDER_TESTING
                    strncpy(input->m_szInput, "/hudscalefix", sizeof(input->m_szInput) - 1);
                    input->m_szInput[sizeof(input->m_szInput) - 1] = '\0';
                    input->ProcessInput();
                    Log("Auto-executed /hudscalefix via ProcessInput (no direct handler)");
#endif
                }
            } else {
                Log("Auto-executed /hudscalefix: GetInput() returned null, waiting...");
                hudScaleFixDelayFrames_ = 30;
            }
            hudScaleFixPending_ = false;
        }
    }

    UpdateAutoEngineStart(now);

    PollLocalIncomingDamage();
    UpdateKillTracker();
}

bool SAMPBridge::RegisterCommand(const std::string& name, CommandCallback callback) {
    const std::string normalized = NormalizeCommandName(name);
    if (normalized.empty() || !callback) {
        return false;
    }

    auto it = std::find_if(commands_.begin(), commands_.end(), [&normalized](const CommandRegistration& registration) {
        return registration.name == normalized;
    });

    if (it == commands_.end()) {
        commands_.push_back({normalized, std::move(callback), kInvalidDispatcherIndex, false});
        it = std::prev(commands_.end());
    } else {
        it->callback = std::move(callback);
    }

    if (!initialized_) {
        return true;
    }

    if (TryRegisterDirect(*it)) {
        return true;
    }

    return InstallFallbackInterceptor();
}

SAMPBridge::Version SAMPBridge::GetVersion() const {
    return version_;
}

#ifdef KEYBINDER_TESTING
void SAMPBridge::SetTestingHooks(const TestingHooks& hooks) {
    g_testingHooks = hooks;
}

void SAMPBridge::ResetTestingHooks() {
    g_testingHooks = {};
}

const SAMPBridge::TestingHooks& SAMPBridge::GetTestingHooks() {
    return g_testingHooks;
}
#endif

bool SAMPBridge::DetectAndValidateVersion() {
    switch (GetRakHookVersionHook()) {
    case rakhook::samp_ver::v03dlr1:
        version_ = Version::V03DLR1;
        unsupportedVersionLogged_ = false;
        return true;
    case rakhook::samp_ver::unknown:
    default:
        version_ = Version::Unsupported;
        if (!unsupportedVersionLogged_) {
            Log("Unsupported SA-MP build detected; expected 0.3.DL-R1. SA-MP features disabled.");
            unsupportedVersionLogged_ = true;
        }
        return false;
    }
}

bool SAMPBridge::InitializeRakHook() {
    if (rakhookInitialized_) {
        return true;
    }

    if (!InitializeRakHookHook()) {
        Log("RakHook initialization failed after SA-MP detection.");
        return false;
    }

    rakhookInitialized_ = true;
    Log("RakHook initialized for SA-MP packet/RPC interception.");
    return true;
}

bool SAMPBridge::RegisterPendingCommands() {
    bool allDirect = true;

    for (auto& registration : commands_) {
        if (!TryRegisterDirect(registration)) {
            allDirect = false;
        }
    }

    InstallFallbackInterceptor();

    return allDirect;
}

bool SAMPBridge::HasCommandRouting() const {
    if (commands_.empty() || fallbackInstalled_) {
        return true;
    }

    return std::all_of(commands_.begin(), commands_.end(), [](const CommandRegistration& registration) {
        return registration.directRegistered;
    });
}

bool SAMPBridge::TryRegisterDirect(CommandRegistration& registration) {
    auto* input = GetInput();
    if (input == nullptr) {
        return false;
    }

    if (registration.dispatcherIndex == kInvalidDispatcherIndex) {
        const auto it = std::find_if(commands_.begin(), commands_.end(), [&registration](const CommandRegistration& current) {
            return &current == &registration;
        });

        const std::size_t nextIndex = static_cast<std::size_t>(std::distance(commands_.begin(), it));
        if (nextIndex >= kDispatchers.size()) {
            Log("Direct command slots exhausted; using fallback interception for /" + registration.name + ".");
            return false;
        }

        registration.dispatcherIndex = nextIndex;
    }

    const auto expectedHandler = kDispatchers[registration.dispatcherIndex];
    const auto existingHandler = GetCommandHandlerHook(input, registration.name.c_str());
    if (existingHandler == expectedHandler) {
        registration.directRegistered = true;
        return true;
    }

    if (existingHandler != nullptr && existingHandler != expectedHandler) {
        Log("Command /" + registration.name + " is already claimed by SA-MP; keeping fallback interception active.");
        return false;
    }

    if (input->m_nCommandCount >= sampapi::v03dl::CInput::MAX_CLIENT_CMDS) {
        Log("SA-MP command table is full; using fallback interception for /" + registration.name + ".");
        return false;
    }

    AddCommandHook(input, registration.name.c_str(), expectedHandler);
    registration.directRegistered = true;
    return true;
}

bool SAMPBridge::InstallFallbackInterceptor() {
    if (fallbackInstalled_) {
        Log("InstallFallbackInterceptor: already installed");
        return true;
    }

    auto* input = GetInput();
    if (input == nullptr) {
        Log("InstallFallbackInterceptor: failed - input is null");
        return false;
    }

    originalDefaultCommand_ = input->m_pDefaultCommand;
    Log("InstallFallbackInterceptor: original handler = " + std::to_string(reinterpret_cast<uintptr_t>(originalDefaultCommand_)));
    SetDefaultCommandHook(input, &DefaultCommandInterceptor);
    fallbackInstalled_ = true;
    Log("Installed fallback command interception for bridge aliases.");
    return true;
}

void SAMPBridge::RestoreFallbackInterceptor() {
    if (!fallbackInstalled_) {
        return;
    }

    if (auto* input = GetInput()) {
        SetDefaultCommandHook(input, originalDefaultCommand_);
    }

    originalDefaultCommand_ = nullptr;
    fallbackInstalled_ = false;
}

sampapi::v03dl::CInput* SAMPBridge::GetInput() const {
    return version_ == Version::V03DLR1 ? GetInputHook() : nullptr;
}

sampapi::v03dl::CChat* SAMPBridge::GetChat() const {
    return version_ == Version::V03DLR1 ? GetChatHook() : nullptr;
}

sampapi::v03dl::CDialog* SAMPBridge::GetDialog() const {
    return version_ == Version::V03DLR1 ? GetDialogHook() : nullptr;
}

sampapi::v03dl::CGame* SAMPBridge::GetGame() const {
    return version_ == Version::V03DLR1 ? GetGameHook() : nullptr;
}

sampapi::v03dl::CNetGame* SAMPBridge::GetNetGame() const {
    return version_ == Version::V03DLR1 ? GetNetGameHook() : nullptr;
}

sampapi::v03dl::CLocalPlayer* SAMPBridge::GetLocalPlayer() const {
    return version_ == Version::V03DLR1 ? GetLocalPlayerHook() : nullptr;
}

std::string SAMPBridge::NormalizeCommandName(const std::string& name) {
    std::string normalized;
    normalized.reserve(name.size());

    for (char ch : name) {
        if (ch == '/') {
            continue;
        }

        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return normalized;
}

std::string SAMPBridge::ExtractCommandName(const char* text) {
    if (text == nullptr) {
        return {};
    }

    std::string value(text);
    if (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }

    const std::size_t separator = value.find(' ');
    if (separator != std::string::npos) {
        value.resize(separator);
    }

    return NormalizeCommandName(value);
}

std::string SAMPBridge::ExtractCommandArguments(const char* text) {
    if (text == nullptr) {
        return {};
    }

    std::string value(text);
    if (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }

    const std::size_t separator = value.find(' ');
    if (separator == std::string::npos) {
        return {};
    }

    return value.substr(separator + 1);
}

bool SAMPBridge::IsVisibleCursorMode(int cursorMode) {
    return cursorMode == sampapi::v03dl::CURSOR_LOCKCAMANDCONTROL;
}

const char* SAMPBridge::VersionToString(Version version) {
    switch (version) {
    case Version::V03DLR1:
        return "0.3.DL-R1";
    case Version::Unsupported:
        return "unsupported";
    case Version::Unknown:
    default:
        return "unknown";
    }
}

void SAMPBridge::Log(const std::string& message) const {
    LogMessage(message);
}

bool SAMPBridge::SendStartupMessage() const {
    auto* chat = GetChat();
    if (chat == nullptr) {
        return false;
    }

    const std::string line1 = "{33CCFF}[NLBinder]{FFFFFF} NLBinder v.0.1.1 by KOBiRO wurde {00CC00}erfolgreich{FFFFFF} gestartet!";
    const std::string encodedLine1 = ConvertUtf8ToGameEncoding(line1);
    AddMessageHook(chat, kStartupMessageColor, encodedLine1.c_str());

    const std::string line2 = "{33CCFF}[NLBinder]{FFFFFF} Nutze "
        + FormatUiOpenOptions(settingsManager_)
        + " um das Menü zu öffnen!";
    const std::string encodedLine2 = ConvertUtf8ToGameEncoding(line2);
    AddMessageHook(chat, kStartupMessageColor, encodedLine2.c_str());

    return true;
}

#ifdef KEYBINDER_TESTING
std::string SAMPBridge::BuildTrackedIncomeMessageForTesting(int amount, int todayTotal) {
    jobs::JobEvent event;
    event.amount = amount;
    return BuildTrackedIncomeMessage(nullptr, event, todayTotal);
}
#endif

void __cdecl SAMPBridge::DispatchDirectCommand0(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(0, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand1(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(1, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand2(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(2, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand3(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(3, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand4(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(4, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand5(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(5, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand6(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(6, params);
    }
}

void __cdecl SAMPBridge::DispatchDirectCommand7(const char* params) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->DispatchRegisteredCommand(7, params);
    }
}

void __cdecl SAMPBridge::DefaultCommandInterceptor(const char* text) {
    if (g_activeBridge != nullptr) {
        g_activeBridge->Log("DefaultCommandInterceptor called with text: '" + std::string(text) + "'");
    }
    
    if (g_activeBridge != nullptr && g_activeBridge->DispatchFallbackCommand(text)) {
        g_activeBridge->Log("DefaultCommandInterceptor: command was handled by DispatchFallbackCommand");
        return;
    }

    if (g_activeBridge != nullptr && g_activeBridge->originalDefaultCommand_ != nullptr && g_activeBridge->originalDefaultCommand_ != &DefaultCommandInterceptor) {
        g_activeBridge->Log("DefaultCommandInterceptor: passing to original handler");
        g_activeBridge->originalDefaultCommand_(text);
    } else if (g_activeBridge != nullptr) {
        g_activeBridge->Log("DefaultCommandInterceptor: no original handler to call");
    }
}

bool SAMPBridge::DispatchRegisteredCommand(std::size_t dispatcherIndex, const char* params) const {
    auto it = std::find_if(commands_.begin(), commands_.end(), [dispatcherIndex](const CommandRegistration& registration) {
        return registration.dispatcherIndex == dispatcherIndex;
    });

    if (it == commands_.end() || !it->callback) {
        return false;
    }

    if (slashCommandInterceptor_) {
        std::string fullCommand = "/" + it->name;
        if (params && params[0] != '\0') {
            fullCommand += " ";
            fullCommand += params;
        }
        if (slashCommandInterceptor_(fullCommand)) {
            return true;
        }
    }

    it->callback(params != nullptr ? params : "");
    return true;
}

bool SAMPBridge::DispatchFallbackCommand(const char* text) const {
    if (text == nullptr || text[0] != '/') {
        return false;
    }

    const std::string name = ExtractCommandName(text);
    if (name.empty()) {
        return false;
    }

    Log("DispatchFallbackCommand: text='" + std::string(text) + "', extracted name='" + name + "'");

    if (slashCommandInterceptor_) {
        Log("DispatchFallbackCommand: checking slash command interceptor");
        if (slashCommandInterceptor_(text)) {
            Log("DispatchFallbackCommand: intercepted by keybind system");
            return true;
        }
    } else {
        Log("DispatchFallbackCommand: no slash command interceptor set");
    }

    auto it = std::find_if(commands_.begin(), commands_.end(), [&name](const CommandRegistration& registration) {
        return registration.name == name;
    });

    if (it == commands_.end() || !it->callback) {
        return false;
    }

    it->callback(ExtractCommandArguments(text));
    return true;
}

void SAMPBridge::SetSlashCommandInterceptor(std::function<bool(const std::string&)> interceptor) {
    slashCommandInterceptor_ = std::move(interceptor);
}

bool SAMPBridge::InterceptSlashCommand(const std::string& text) const {
    if (slashCommandInterceptor_) {
        return slashCommandInterceptor_(text);
    }
    return false;
}

void SAMPBridge::RegisterSlashCommands(const std::vector<std::string>& commands) {
    (void)commands;
}

void SAMPBridge::ClearSlashCommands() {
    registeredSlashCommands_.clear();
}

void SAMPBridge::InitializeJobTracker() {
    if (jobTrackerInitialized_) {
        return;
    }

    if (jobTracker_ == nullptr) {
        jobTracker_ = std::make_unique<jobs::JobTracker>();
    }

    if (!jobTracker_->Initialize()) {
        keybinder::Logger::Warning("[SAMPBridge] Failed to initialize Job Tracker state.");
        return;
    }

    if (jobChatAdapter_ == nullptr) {
        jobChatAdapter_ = std::make_unique<jobs::JobChatAdapter>();
    }

    jobChatAdapter_->SetCallback([this](const jobs::JobChatEvent& event) {
        if (!event.hasPlayerPrefix) ObserveServerEngineStartMessage(event.normalizedLine);
        if (!event.hasPlayerPrefix && !g_emittingBinderLocalMessage) QueueCombatChat(event.normalizedLine);
        NotifyIncomingChat(event.normalizedLine);
        if (jobTracker_ != nullptr) {
            const std::vector<jobs::JobEvent> recordedEvents = jobTracker_->HandleChatEvent(event);
            for (const jobs::JobEvent& recordedEvent : recordedEvents) {
                if (recordedEvent.type != "earning" || recordedEvent.source != "chat_auto") {
                    continue;
                }

                const jobs::JobWindowAggregates aggregates = jobTracker_->GetAggregates(recordedEvent.jobId, recordedEvent.timestamp);
                pendingLocalChatMessages_.push_back({
                    BuildTrackedIncomeMessage(jobTracker_.get(), recordedEvent, aggregates.today),
                    kStartupMessageColor,
                    kTrackedIncomeMessageDelayFrames
                });
            }
        }
    });
    jobChatAdapter_->SetVisibilityFilter([this](const jobs::JobChatEvent& event) {
        return ShouldDisplayStartupStatsChat(event);
    });

    if (!jobChatAdapter_->Initialize()) {
        keybinder::Logger::Warning("[SAMPBridge] Failed to initialize Job Chat Adapter; jobs chat pipeline will retry on the next bridge update.");
        jobChatAdapter_->Shutdown();
        jobChatAdapter_.reset();
        jobTrackerInitialized_ = false;
        return;
    }

    jobTrackerInitialized_ = true;
    keybinder::Logger::Info("[SAMPBridge] Job Tracker initialized");
}

void SAMPBridge::ShutdownJobTracker() {
    if (!jobTrackerInitialized_ && jobTracker_ == nullptr && jobChatAdapter_ == nullptr) {
        return;
    }

    keybinder::Logger::Info("[SAMPBridge] Shutting down Job Tracker");

    if (jobChatAdapter_ != nullptr) {
        jobChatAdapter_->Shutdown();
        jobChatAdapter_.reset();
    }

    if (jobTracker_ != nullptr) {
        jobTracker_->Shutdown();
        jobTracker_.reset();
    }

    jobTrackerInitialized_ = false;
    keybinder::Logger::Info("[SAMPBridge] Job Tracker shutdown complete");
}

jobs::JobTracker* SAMPBridge::GetJobTracker() const {
    return jobTracker_.get();
}

std::vector<jobs::JobEvent> SAMPBridge::ExportJobEvents() const {
    return jobTracker_ != nullptr ? jobTracker_->ExportEvents() : std::vector<jobs::JobEvent>{};
}

void SAMPBridge::ImportJobEvents(const std::vector<jobs::JobEvent>& events) {
    if (jobTracker_ == nullptr) {
        jobTracker_ = std::make_unique<jobs::JobTracker>();
    }

    if (!jobTracker_->IsInitialized()) {
        jobTracker_->Initialize();
    }

    jobTracker_->ImportEvents(events);
}

void SAMPBridge::InitializeKillTracker() {
    if (killTrackerInitialized_) {
        return;
    }

    if (killTracker_ == nullptr) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
        killTracker_->SetEnabled(killTrackerEnabled_);
    }

    Log("Initializing Kill Tracker");

    {
        bool rakReady = false;
#ifndef KEYBINDER_TESTING
        rakReady = rakhook::initialized;
#endif
        auto* netGame = GetNetGame();
        auto* localPlayer = GetLocalPlayer();
        Log("Kill tracker init state: bridge=" + std::to_string(initialized_) +
            " rakhook=" + std::to_string(rakReady) +
            " netGame=" + std::to_string(reinterpret_cast<uintptr_t>(netGame)) +
            " localPlayer=" + std::to_string(reinterpret_cast<uintptr_t>(localPlayer)));
    }

    killfeedAdapter_ = std::make_unique<killtracker::KillfeedAdapter>();
    
    // Set initialized flag BEFORE RefreshDamageTracking so it doesn't return early
    killTrackerInitialized_ = true;
    ResetKillTrackerDeathInferenceState();
    
    RefreshDamageTracking();
    
    Log("Kill Tracker initialized");
}

void SAMPBridge::ShutdownKillTracker() {
    ResetCombatChat();
    if (!killTrackerInitialized_) {
        return;
    }

    Log("Shutting down Kill Tracker");

    ResetKillTrackerDeathInferenceState();
    killfeedAdapter_.reset();

    killTrackerInitialized_ = false;
    Log("Kill Tracker shutdown complete");
}

void SAMPBridge::UpdateKillTracker() {
    UpdateCombatChat(RuntimeTickCount());
    if (!killTrackerInitialized_ || !killTracker_ || !killfeedAdapter_ || !killTracker_->IsEnabled()) {
        return;
    }

    auto newKills = killfeedAdapter_->PollForNewKills();

    for (const auto& kill : newKills) {
        HandleLocalKill(kill);
    }

    UpdateLocalDeathFallback();
}

bool SAMPBridge::IsKillTrackerEnabled() const {
    return killTrackerEnabled_;
}

void SAMPBridge::SetKillTrackerEnabled(bool enabled) {
    killTrackerEnabled_ = enabled;

    if (killTracker_ == nullptr) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
    }

    killTracker_->SetEnabled(enabled);
    if (settingsManager_ != nullptr) {
        killTracker_->SetCountKills(settingsManager_->Get().kill_tracker_count_kills);
        killTracker_->SetCountDeaths(settingsManager_->Get().kill_tracker_count_deaths);
    }

    Log("SetKillTrackerEnabled(" + std::to_string(enabled) +
        ") initialized=" + std::to_string(initialized_));

    if (!initialized_) {
        return;
    }

    if (enabled) {
        InitializeKillTracker();
    } else if (killTrackerInitialized_) {
        ShutdownKillTracker();
    }
}

void SAMPBridge::SetKillTrackerLocalKillMessageEnabled(bool enabled) {
    killTrackerLocalKillMessageEnabled_ = enabled;
}

void SAMPBridge::SetKillTrackerLocalDeathMessageEnabled(bool enabled) {
    killTrackerLocalDeathMessageEnabled_ = enabled;
}

void SAMPBridge::SetKillTrackerServerKillMessageEnabled(bool enabled) {
    killTrackerServerKillMessageEnabled_ = enabled;
}

void SAMPBridge::SetKillTrackerServerDeathMessageEnabled(bool enabled) {
    killTrackerServerDeathMessageEnabled_ = enabled;
}

void SAMPBridge::SetKillTrackerTrackDamage(bool enabled) {
    killTrackerTrackDamage_ = enabled;

    if (killTracker_ != nullptr && !enabled) {
        killTracker_->SetEnabled(killTrackerEnabled_);
    }

    if (initialized_) {
        RefreshDamageTracking();
    }
}

void SAMPBridge::SetHitmarkerDamageCallback(std::function<void(const killtracker::DamageEvent&)> callback) {
    hitmarkerDamageCallback_ = std::move(callback);
}

void SAMPBridge::SetHitsoundDamageCallback(std::function<void(const killtracker::DamageEvent&)> callback) {
    hitsoundDamageCallback_ = std::move(callback);
}

void SAMPBridge::SetHitsoundIncomingCallback(std::function<void()> callback) {
    hitsoundIncomingCallback_ = std::move(callback);
}

void SAMPBridge::SetHitmarkerKillCallback(std::function<void(const killtracker::KillEvent&)> callback) {
    hitmarkerKillCallback_ = std::move(callback);
}

void SAMPBridge::SetKillTrackerLocalKillMessageTemplate(const std::string& messageTemplate) {
    if (messageTemplate.empty()) {
        killTrackerLocalKillMessageTemplate_ = "{FF6600}[NL-BINDER] {FFFFFF}Kill in {city}, {bezirk}. Kills: {total_kills}";
        return;
    }
    killTrackerLocalKillMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetKillTrackerLocalDeathMessageTemplate(const std::string& messageTemplate) {
    if (messageTemplate.empty()) {
        killTrackerLocalDeathMessageTemplate_ = "{FF3333}[NL-BINDER] {FFFFFF}Du bist gestorben. Deine aktuellen Tode: {total_deaths}";
        return;
    }
    killTrackerLocalDeathMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetKillTrackerServerKillMessageTemplate(const std::string& messageTemplate) {
    killTrackerServerKillMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetKillTrackerServerDeathMessageTemplate(const std::string& messageTemplate) {
    if (messageTemplate.empty()) {
        killTrackerServerDeathMessageTemplate_ = "Ich bin gestorben. Meine Tode: {total_deaths}";
        return;
    }
    killTrackerServerDeathMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetAutoHudScaleFix(bool enabled) {
    hudScaleFixEnabled_ = enabled;

    if (enabled && initialized_ && !hudScaleFixPending_) {
        hudScaleFixPending_ = true;
        hudScaleFixDelayFrames_ = 90;
    }
}

void SAMPBridge::SetAutoEngineStart(bool enabled) {
    if (autoEngineStartEnabled_ == enabled && !shutting_down_) return;
    autoEngineStartEnabled_ = enabled;

    if (!enabled) {
        autoEngineStartPending_ = false;
        autoEngineStartAt_ = 0;
        autoEngineStartHoldUntil_ = 0;
        if (autoEngineStartReleasePending_) {
            SetEngineStartKeyDown(false);
        }
        autoEngineStartReleasePending_ = false;
        if (shutting_down_) {
            std::lock_guard<std::mutex> lock(autoEngineNoticeMutex_);
            autoEngineServerNotices_.clear();
            autoEnginePublishedVehicle_ = autoEngineObservedVehicle_ = 0xFFFF;
            autoEnginePublishedGeneration_ = autoEngineEntryGeneration_ = 0;
            autoEnginePreSeatNotice_.reset();
            autoEngineEntryConsumed_ = false;
        }
    }
}

void SAMPBridge::ObserveServerEngineStartMessage(const std::string& text) {
    if (text != "INFO: Automatischer Motorstart: Der Motor deines Fahrzeugs wurde automatisch gestartet.") return;
    // Chat callbacks never inspect native ped/vehicle memory. The main update
    // publishes a small identity snapshot used to scope this fresh notice.
    std::lock_guard<std::mutex> lock(autoEngineNoticeMutex_);
    if (autoEngineServerNotices_.size() == 4) autoEngineServerNotices_.erase(autoEngineServerNotices_.begin());
    autoEngineServerNotices_.push_back({RuntimeTickCount(), autoEnginePublishedVehicle_, autoEnginePublishedGeneration_});
}

bool SAMPBridge::ReadDriverVehicle(unsigned short& vehicleId, bool& engineOn) const {
#ifdef KEYBINDER_TESTING
    return g_testingHooks.getDriverVehicle && g_testingHooks.getDriverVehicle(vehicleId, engineOn);
#else
    auto* local = GetLocalPlayer();
    auto* ped = local && local->m_pPed ? local->m_pPed->m_pGamePed : nullptr;
    if (!ped || !ped->bInVehicle || !ped->m_pVehicle || ped->m_pVehicle->m_pDriver != ped
        || local->m_nCurrentVehicle == 0xFFFF || local->m_pPed->IsPassenger()) return false;
    vehicleId = local->m_nCurrentVehicle;
    engineOn = ped->m_pVehicle->bEngineOn;
    return true;
#endif
}

bool SAMPBridge::IsEngineInputAvailable() const {
    if (engineInputBlocked_ && engineInputBlocked_()) return false;
#ifdef KEYBINDER_TESTING
    return g_testingHooks.isEngineInputAvailable && g_testingHooks.isEngineInputAvailable();
#else
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    return foregroundProcess == GetCurrentProcessId() && !IsChatOpen() && !IsDialogOpen() && !IsCursorActive();
#endif
}

void SAMPBridge::SetEngineStartKeyDown(bool down) {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.setEngineStartKeyDown) g_testingHooks.setEngineStartKeyDown(down);
#else
    // Keep a physical key down across input frames. Writing CPad in DrawHUD
    // was overwritten by DirectInput before SA-MP could observe the press.
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyExA('Z', MAPVK_VK_TO_VSC, GetKeyboardLayout(0)));
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    if (input.ki.wScan != 0 && SendInput(1, &input, sizeof(input)) != 1) {
        Log("Auto engine start: keyboard input was not accepted");
    }
#endif
}

void SAMPBridge::UpdateAutoEngineStart(unsigned long now) {
    const bool inputAvailable = IsEngineInputAvailable();
    std::vector<EngineServerStartNotice> serverNotices;
    {
        std::lock_guard<std::mutex> lock(autoEngineNoticeMutex_);
        serverNotices.swap(autoEngineServerNotices_);
    }
    // A callback may arrive after the frame's cached `now` was read. Sample
    // after draining, so its newer timestamp cannot wrap into a stale age.
    const unsigned long noticeNow = RuntimeTickCount();
    auto publishEntry = [this](unsigned short vehicle) {
        std::lock_guard<std::mutex> lock(autoEngineNoticeMutex_);
        autoEnginePublishedVehicle_ = vehicle;
        autoEnginePublishedGeneration_ = autoEngineEntryGeneration_;
    };
    auto releaseKey = [this]() {
        if (autoEngineStartReleasePending_) SetEngineStartKeyDown(false);
        autoEngineStartReleasePending_ = false;
        autoEngineStartHoldUntil_ = 0;
    };
    if (autoEngineStartReleasePending_ && (!autoEngineStartEnabled_ || !inputAvailable
        || static_cast<long>(now - autoEngineStartHoldUntil_) >= 0)) releaseKey();
    unsigned short vehicleId = 0xFFFF;
    bool engineOn = false;
    if (!ReadDriverVehicle(vehicleId, engineOn)) {
        if (autoEngineObservedVehicle_ != 0xFFFF) {
            ++autoEngineEntryGeneration_;
            autoEnginePreSeatNotice_.reset();
        } else {
            // A server auto-start message can precede the first usable native
            // driver frame. Keep only this fresh, as-yet-unassigned notice.
            for (const auto& notice : serverNotices)
                if (notice.vehicle == 0xFFFF && notice.generation == autoEngineEntryGeneration_
                    && noticeNow - notice.at <= kAutoEngineServerNoticeWindowMs) autoEnginePreSeatNotice_ = notice;
        }
        if (autoEnginePreSeatNotice_ && noticeNow - autoEnginePreSeatNotice_->at > kAutoEngineServerNoticeWindowMs)
            autoEnginePreSeatNotice_.reset();
        autoEngineObservedVehicle_ = 0xFFFF;
        autoEngineEntryConsumed_ = false;
        autoEngineStartPending_ = false;
        publishEntry(0xFFFF);
        releaseKey();
        return;
    }
    const bool newEntry = vehicleId != autoEngineObservedVehicle_;
    const unsigned short previousVehicle = autoEngineObservedVehicle_;
    const unsigned long previousGeneration = autoEngineEntryGeneration_;
    if (newEntry) {
        ++autoEngineEntryGeneration_;
        autoEngineObservedVehicle_ = vehicleId;
        autoEngineEntryConsumed_ = false;
        autoEngineStartPending_ = false;
        releaseKey();
    }
    publishEntry(vehicleId);
    bool serverAlreadyStarted = false;
    for (const auto& notice : serverNotices) {
        if (noticeNow - notice.at > kAutoEngineServerNoticeWindowMs) continue;
        if (notice.vehicle == vehicleId && notice.generation == autoEngineEntryGeneration_)
            serverAlreadyStarted = true;
        if (newEntry && previousVehicle == 0xFFFF && notice.vehicle == 0xFFFF
            && notice.generation == previousGeneration) serverAlreadyStarted = true;
    }
    if (newEntry && previousVehicle == 0xFFFF && autoEnginePreSeatNotice_
        && autoEnginePreSeatNotice_->generation == previousGeneration
        && noticeNow - autoEnginePreSeatNotice_->at <= kAutoEngineServerNoticeWindowMs) serverAlreadyStarted = true;
    if (newEntry) autoEnginePreSeatNotice_.reset();
    if (serverAlreadyStarted) {
        autoEngineEntryConsumed_ = true;
        autoEngineStartPending_ = false;
        releaseKey();
        Log("Auto engine start: server already started this entry; skipping motor toggle");
    }
    // Keep the seat identity while disabled too. Reapplying or toggling the
    // setting cannot create another automatic press in the same entry.
    if (!autoEngineStartEnabled_ || autoEngineEntryConsumed_) return;
    if (!autoEngineStartPending_) {
        // Start as soon as the driver's seat is usable and the short server-
        // addon guard elapsed. Transient engine-on states are observed below.
        autoEngineStartPending_ = true;
        autoEngineStartAt_ = now + kAutoEngineStartDelayMs;
        Log("Auto engine start: driver vehicle detected, id=" + std::to_string(vehicleId));
    }
    if (static_cast<long>(now - autoEngineStartAt_) < 0) return;
    if (engineOn) {
        // GTA can briefly report engine-on during seating before the server
        // applies manual engine-off. Observe that transition without delaying
        // the fast path for a genuinely stopped engine. An addon confirmation
        // consumes the entry immediately, even while this native flag lags.
        if (static_cast<long>(now - (autoEngineStartAt_ - kAutoEngineStartDelayMs
            + kAutoEngineRunningConfirmationMs)) < 0) return;
        // An already running engine completes this entry as well. A later
        // manual stop must not be undone by the binder.
        autoEngineEntryConsumed_ = true;
        autoEngineStartPending_ = false;
        return;
    }
    if (!inputAvailable) return;
    // The server motor key is a toggle. Consume this entry before the one
    // physical press; delayed server state can never cause an off/on retry.
    autoEngineEntryConsumed_ = true;
    autoEngineStartPending_ = false;
    SetEngineStartKeyDown(true);
    autoEngineStartReleasePending_ = true;
    autoEngineStartHoldUntil_ = now + kAutoEngineStartHoldMs;
    Log("Auto engine start: pressed Z once for this driver entry");
}

killtracker::KillTracker* SAMPBridge::GetKillTracker() const {
    if (killTracker_ == nullptr) {
        auto* self = const_cast<SAMPBridge*>(this);
        self->killTracker_ = std::make_unique<killtracker::KillTracker>();
        self->killTracker_->SetEnabled(killTrackerEnabled_);
    }

    return killTracker_.get();
}

std::string SAMPBridge::GetCurrentZone() const {
#ifndef KEYBINDER_TESTING
    CPlayerPed* player = FindPlayerPed(0);
    if (!player) return "Unknown";
    CVector pos = player->GetPosition();
    CZone* zone = CTheZones::FindSmallestZoneForPosition(pos, false);
    if (!zone) return "Unknown";
    const char* name = zone->GetTranslatedName();
    if (!name || name[0] == '\0') {
        char label[9] = {};
        std::memcpy(label, zone->m_szLabel, 8);
        label[8] = '\0';
        return (label[0] != '\0') ? std::string(label) : std::string("Unknown");
    }
    return std::string(name);
#else
    return g_testingHooks.getDistrictName ? g_testingHooks.getDistrictName() : "Unknown";
#endif
}

std::string SAMPBridge::GetCityName() const {
#ifndef KEYBINDER_TESTING
    CPlayerPed* player = FindPlayerPed(0);
    if (!player) return "Unknown";
    CVector position = player->GetPosition();
    return keybinder::CityNameFromGtaLevel(static_cast<int>(CTheZones::GetLevelFromPosition(&position)));
#else
    return g_testingHooks.getCityLevel ? keybinder::CityNameFromGtaLevel(g_testingHooks.getCityLevel()) : "Unknown";
#endif
}

std::string SAMPBridge::GetDistrictName() const {
    return GetCurrentZone();
}

std::string SAMPBridge::GetLocalPlayerName() const {
#ifndef KEYBINDER_TESTING
    auto* netGame = GetNetGame();
    if (!netGame) {
        return "";
    }

    auto* playerPool = netGame->GetPlayerPool();
    if (!playerPool) {
        return "";
    }

    const char* localName = playerPool->GetLocalPlayerName();
    if (!localName || localName[0] == '\0') {
        return "";
    }

    return std::string(localName);
#else
    return g_testingHooks.getLocalPlayerName ? g_testingHooks.getLocalPlayerName() : "";
#endif
}

float SAMPBridge::GetLocalPlayerHP() const {
    float health = 0.0f, armour = 0.0f;
    int source = 0;
    const void* ped = nullptr;
    if (ReadLocalDamageSample(health, armour, source, ped) && ped && std::isfinite(health)) {
        return (std::max)(0.0f, health);
    }
    return 0.0f;
}

float SAMPBridge::GetLocalPlayerArmour() const {
    float health = 0.0f, armour = 0.0f;
    int source = 0;
    const void* ped = nullptr;
    if (ReadLocalDamageSample(health, armour, source, ped) && ped && std::isfinite(armour)) {
        return (std::max)(0.0f, armour);
    }
    return 0.0f;
}

float SAMPBridge::GetLocalPlayerCarHP() const {
#ifndef KEYBINDER_TESTING
    CPlayerPed* playerPed = FindPlayerPed(0);
    if (playerPed && playerPed->bInVehicle && playerPed->m_pVehicle) {
        return playerPed->m_pVehicle->m_fHealth;
    }
#endif
    return 0.0f;
}

int SAMPBridge::GetTotalKills() const {
    auto* tracker = GetKillTracker();
    return tracker ? tracker->GetTotalKills() : 0;
}

int SAMPBridge::GetTotalDeaths() const {
    auto* tracker = GetKillTracker();
    return tracker ? tracker->GetTotalDeaths() : 0;
}

float SAMPBridge::GetPlayerHP() const {
    return GetLocalPlayerHP();
}

float SAMPBridge::GetPlayerArmour() const {
    return GetLocalPlayerArmour();
}

float SAMPBridge::GetVehicleHP() const {
    return GetLocalPlayerCarHP();
}

std::string SAMPBridge::GetVehicleName() const {
#ifndef KEYBINDER_TESTING
    CPlayerPed* playerPed = FindPlayerPed(0);
    if (playerPed && playerPed->bInVehicle && playerPed->m_pVehicle) {
        unsigned short modelIndex = playerPed->m_pVehicle->m_nModelIndex;
        CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelIndex);
        if (modelInfo) {
            CVehicleModelInfo* vehicleModelInfo = reinterpret_cast<CVehicleModelInfo*>(modelInfo);
            if (vehicleModelInfo->m_szGameName[0] != '\0') {
                const std::string gameNameKey = CopyVehicleGameNameKey(vehicleModelInfo->m_szGameName, kVehicleGameNameLength);
                const char* translatedName = TranslateVehicleGameNameKey(gameNameKey);
                if (translatedName != nullptr && translatedName[0] != '\0') {
                    return translatedName;
                }

                std::string fallbackName = FormatVehicleGameNameFallback(gameNameKey);
                if (!fallbackName.empty()) {
                    return fallbackName;
                }
            }
        }
    }
#endif
    return "Zu Fu\xC3\x9F";
}

bool SAMPBridge::IsLocalPlayerDead() const {
    auto* localPlayer = GetLocalPlayer();
    if (!localPlayer) {
        return false;
    }

    if (localPlayer->m_bIsWasted) {
        return true;
    }

#ifdef KEYBINDER_TESTING
    return false;
#else
    return localPlayer->m_pPed != nullptr && localPlayer->m_pPed->GetHealth() <= 0.0f;
#endif
}

void SAMPBridge::ResetKillTrackerDeathInferenceState() {
    const bool isDead = IsLocalPlayerDead();
    localPlayerWasDead_ = isDead;
    localDeathHandledForCurrentLife_ = isDead;
    localDeathFallbackDelayFrames_ = 0;
    localDeathAwaitingNativeDead_ = false;
    localDeathAwaitDeadline_ = 0;

    if (killTracker_ != nullptr) {
        killTracker_->UpdateDeathWindowState({});
    }
}

void SAMPBridge::RecordLocalPlayerDeath(const std::string& killer, int weaponId) {
    if (!killTracker_) {
        return;
    }

    const bool countingDeaths = killTracker_->IsCountingDeaths();
    killTracker_->RecordDeath(killer, weaponId);
    // A server death notification can precede the native wasted flag. Keep the
    // life guard through those alive frames, then reset on the observed respawn.
    localPlayerWasDead_ = IsLocalPlayerDead();
    localDeathAwaitingNativeDead_ = !localPlayerWasDead_;
    localDeathAwaitDeadline_ = RuntimeTickCount() + 2000;
    localDeathHandledForCurrentLife_ = true;
    localDeathFallbackDelayFrames_ = 0;

    if (!countingDeaths) {
        return;
    }

    Log("[SAMPBridge] Local player death recorded: killer=" +
        (killer.empty() ? std::string("<none>") : killer) +
        " weapon=" + std::to_string(weaponId));

    lastLocalDeathTime_ = std::time(nullptr);
    hasCombatChatDeath_ = true;
    lastCombatChatDeathAt_ = RuntimeTickCount();
    SendLocalMessage("{33CCFF}[NL-BINDER]{FFFFFF} Du bist gestorben. Deine aktuelle Tode: " +
        std::to_string(killTracker_->GetTotalDeaths()));
}

void SAMPBridge::UpdateLocalDeathFallback() {
    if (!killTracker_ || !killfeedAdapter_) {
        return;
    }

    const bool isDead = IsLocalPlayerDead();
    if (!isDead) {
        if (localDeathAwaitingNativeDead_
            && static_cast<long>(RuntimeTickCount() - localDeathAwaitDeadline_) < 0) {
            return;
        }
        const bool observedRespawn = localPlayerWasDead_;
        ResetKillTrackerDeathInferenceState();
        if (observedRespawn) hasCombatChatDeath_ = false;
        return;
    }

    localDeathAwaitingNativeDead_ = false;
    if (!localPlayerWasDead_) {
        localPlayerWasDead_ = true;
        if (!localDeathHandledForCurrentLife_) {
            localDeathFallbackDelayFrames_ = kLocalDeathFallbackDelayFrames;
        }
    }

    const std::vector<killtracker::DeathWindowEntry> deathWindowState = killfeedAdapter_->GetCurrentDeathWindowState();

    if (!localDeathHandledForCurrentLife_) {
        const std::string localPlayerName = GetLocalPlayerName();
        if (!localPlayerName.empty()) {
            for (const auto& entry : deathWindowState) {
                if (!entry.valid || entry.victim != localPlayerName || !killTracker_->IsNewDeathWindowEntry(entry)) {
                    continue;
                }

                std::string killerName = entry.killer;
                if (killerName == localPlayerName) {
                    killerName.clear();
                }

                RecordLocalPlayerDeath(killerName, entry.weaponId);
                break;
            }
        }
    }

    killTracker_->UpdateDeathWindowState(deathWindowState);

    if (localDeathHandledForCurrentLife_) {
        return;
    }

    if (localDeathFallbackDelayFrames_ > 0) {
        --localDeathFallbackDelayFrames_;
    }

    if (localDeathFallbackDelayFrames_ == 0) {
        RecordLocalPlayerDeath("", -1);
    }
}

void SAMPBridge::InstallScriptedHealthRpcHook() {
    if (scriptedHealthHookInstalled_) {
        return;
    }

#ifdef KEYBINDER_TESTING
    scriptedHealthHookInstalled_ = true;
#else
    scriptedHealthHookInstalled_ = true;

    // SA-MP server->client RPC IDs (verified against the SA-MP 0.3.7 RakNet
    // RPC table, not inferred from RPCHandlers.h header order):
    //   RPC 14 = ScrSetPlayerHealth  (float health)
    //   RPC 66 = ScrSetPlayerArmour  (float armour)
    static constexpr unsigned char kRpcScrSetPlayerHealth = 14;
    static constexpr unsigned char kRpcScrSetPlayerArmour = 66;

    std::weak_ptr<ScriptedHealthSuppressionState> weakState = scriptedHealthState_;

    rakhook::on_receive_rpc += [weakState](unsigned char& rpcId, RakNet::BitStream* bs) -> bool {
        if (!rakhook::initialized) {
            return true;
        }

        if (rpcId != kRpcScrSetPlayerHealth && rpcId != kRpcScrSetPlayerArmour) {
            return true;
        }

        auto state = weakState.lock();
        if (!state || !state->active.load(std::memory_order_acquire)) {
            return true;
        }

        if (bs == nullptr) {
            return true;
        }

        // Non-mutating read: save and restore the read offset so SA-MP's own
        // RPC handler still sees the full, unconsumed BitStream.
        int originalReadOffset = bs->GetReadOffset();

        float newValue;
        if (!bs->Read(newValue)) {
            bs->SetReadOffset(originalReadOffset);
            return true;
        }

        bs->SetReadOffset(originalReadOffset);

        // Compare the incoming RPC value against the last-polled ped HP/armour
        // (the pre-RPC baseline published by PollLocalIncomingDamage). Only arm
        // suppression when the RPC carries a strictly lower value -- a heal or
        // no-op must not arm suppression, otherwise a real combat hit right
        // after a heal RPC could be muted.
        //
        // Best-effort tradeoff: the baseline is last-polled HP, not current ped
        // HP, because reading ped memory from the RakNet receive thread is
        // unsafe (data race). A real combat hit landing in the same inter-poll
        // gap as a heal/no-op SetHealth RPC can make newValue < baseline and arm
        // suppression, which may then mute that real hit's drop at the next
        // poll. This is a false-negative window bounded by the one-shot TTL
        // (kScriptedSuppressionFrameCap frames); it cannot mute more than one
        // hit. The TTL is kept small (3 frames) to minimize this window.
        const bool isHealth = (rpcId == kRpcScrSetPlayerHealth);
        const float baseline = isHealth
            ? state->lastKnownHealth.load(std::memory_order_relaxed)
            : state->lastKnownArmour.load(std::memory_order_relaxed);
        if (newValue < baseline - keybinder::IncomingDamageDetector::kDamageEpsilon) {
            // Request the game thread to arm the one-shot suppression. store
            // (not exchange) because multiple RPCs in the same gap are
            // idempotent -- a single armed window suppresses the next drop.
            state->suppressionRequested.store(true, std::memory_order_release);
        }

        return true; // Always pass through to SA-MP
    };

    keybinder::Logger::Info("[SAMPBridge] Scripted-health RPC hook installed (RPC 14/66).");
#endif
}

bool SAMPBridge::IsLocalCombatActive(bool allowFinalDeathSample) const {
    const auto* netGame = GetNetGame();
    const auto* localPlayer = GetLocalPlayer();
    // A living GTA ped also exists in the login scene and class preview.
    // Only the spawned SA-MP player may produce gameplay hitsounds.
    return netGame != nullptr && netGame->m_nGameState == sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        && localPlayer != nullptr && localPlayer->m_pPed != nullptr
        && (localPlayer->m_bIsActive != FALSE || (allowFinalDeathSample && localPlayer->m_bIsWasted != FALSE))
        && (localPlayer->m_bIsWasted == FALSE || allowFinalDeathSample)
        && localPlayer->m_bDoesSpectating == FALSE
        && localPlayer->m_classSelection.m_bIsActive == FALSE
        && localPlayer->m_classSelection.m_bWaitingForSpawnRequestReply == FALSE;
}

void SAMPBridge::ResetIncomingDamageState() {
    scriptedHealthState_->active.store(false, std::memory_order_release);
    scriptedHealthState_->suppressionRequested.store(false, std::memory_order_release);
    scriptedHealthState_->lastKnownHealth.store(0.0f, std::memory_order_relaxed);
    scriptedHealthState_->lastKnownArmour.store(0.0f, std::memory_order_relaxed);
    incomingDamageDetector_.Reset();
    incomingDamageLog_.Reset();
    incomingDamagePed_ = nullptr;
}

bool SAMPBridge::IsDamageLogEnabled() const {
    return settingsManager_ != nullptr && settingsManager_->Get().damage_log_enabled;
}

void SAMPBridge::LogIncomingDamage(const keybinder::IncomingDamageLogEntry& damage) {
    if (!damage.HasDamage()) return;
    char amount[48];
    const double rounded = std::round(static_cast<double>(damage.Total()) * 10.0) / 10.0;
    std::snprintf(amount, sizeof(amount), "%.1f", rounded);
    std::string formatted = amount;
    std::replace(formatted.begin(), formatted.end(), '.', ',');
    SendLocalMessage("{1E90FF}[NL-BINDER] {FF0000}-" + formatted
        + " HP {FFFFFF}Schaden (" + damage.Type() + ")");
}

bool SAMPBridge::ReadLocalDamageSample(float& health, float& armour, int& damageSource, const void*& pedIdentity) const {
#ifdef KEYBINDER_TESTING
    const auto& hooks = GetTestingHooks();
    return hooks.readLocalDamageSample && hooks.readLocalDamageSample(health, armour, damageSource, pedIdentity);
#else
    CPlayerPed* ped = FindPlayerPed(0);
    const auto* localPlayer = GetLocalPlayer();
    if (ped == nullptr || localPlayer == nullptr || localPlayer->m_pPed == nullptr
        || localPlayer->m_pPed->m_pGamePed != ped) return false;
    health = ped->m_fHealth;
    armour = ped->m_fArmour;
    damageSource = static_cast<int>(ped->m_nLastWeaponDamage);
    pedIdentity = ped;
    return true;
#endif
}

void SAMPBridge::PollLocalIncomingDamage() {
    const bool logEnabled = IsDamageLogEnabled();
    const bool logJustEnabled = logEnabled && !incomingDamageLogEnabledLastFrame_;
    incomingDamageLogEnabledLastFrame_ = logEnabled;
    if (!logEnabled || logJustEnabled) {
        incomingDamageLog_.Reset();
    }
    const bool playing = IsLocalCombatActive();
    const bool finalDeathSample = !playing && incomingDamagePed_ != nullptr && IsLocalCombatActive(true);
    if ((!hitsoundIncomingCallback_ && !IsDamageLogEnabled()) || (!playing && !finalDeathSample)) {
        ResetIncomingDamageState();
        return;
    }

    float health = 0.0f;
    float armour = 0.0f;
    int damageSource = 0;
    const void* pedIdentity = nullptr;
    const bool hasSample = ReadLocalDamageSample(health, armour, damageSource, pedIdentity);
    if (!hasSample || pedIdentity == nullptr || !std::isfinite(health)
        || !std::isfinite(armour) || armour < 0.0f || !std::isfinite(health + armour)) {
        ResetIncomingDamageState();
        return;
    }
    // GTA can briefly retain a negative HP value on a lethal hit. Only the
    // remaining HP was actually available to lose; never log extra overkill.
    health = (std::max)(health, 0.0f);

    if (pedIdentity != incomingDamagePed_) {
        ResetIncomingDamageState();
        incomingDamagePed_ = pedIdentity;
    }

    // Publish only samples from this spawned ped. Login/spawn updates cannot
    // carry a baseline or suppression request into the next playing phase.
    scriptedHealthState_->lastKnownHealth.store(health, std::memory_order_relaxed);
    scriptedHealthState_->lastKnownArmour.store(armour, std::memory_order_relaxed);
    scriptedHealthState_->active.store(playing && health > 0.0f, std::memory_order_release);
    if (scriptedHealthState_->suppressionRequested.exchange(false, std::memory_order_acq_rel)) {
        incomingDamageDetector_.ArmScriptedSuppression();
    }

    const auto observation = incomingDamageDetector_.ObserveDetailed(true, health + armour,
                                      keybinder::IsNonCombatDamageSource(damageSource));
    if (health > 0.0f && observation.IsCombat()
        && observation.damage > keybinder::IncomingDamageDetector::kDamageEpsilon && hitsoundIncomingCallback_) {
        hitsoundIncomingCallback_();
    }
    if (logEnabled) {
        // Keep the log independent from hitsound suppression. In particular,
        // Nova's server-applied SetPlayerHealth/Armour must be logged at once.
        LogIncomingDamage(incomingDamageLog_.Observe(health, armour));
    }
    if (finalDeathSample || health == 0.0f) ResetIncomingDamageState();
}

void SAMPBridge::NotifyDamageFeedback(const killtracker::DamageEvent& event) {
    if (event.isOutgoing && hitsoundDamageCallback_ && IsLocalCombatActive()) {
        hitsoundDamageCallback_(event);
    }
    if (event.isOutgoing && hitmarkerDamageCallback_) {
        hitmarkerDamageCallback_(event);
    }
}

#ifdef KEYBINDER_TESTING
void SAMPBridge::ProcessDamageFeedbackForTesting(const killtracker::DamageEvent& event) {
    NotifyDamageFeedback(event);
}
void SAMPBridge::ProcessDamageRpcForTesting(RakNet::BitStream* payload) {
    if (damageAdapter_) damageAdapter_->ProcessRpcForTesting(payload);
}
void SAMPBridge::RequestScriptedHealthSuppressionForTesting() {
    scriptedHealthState_->suppressionRequested.store(true, std::memory_order_release);
}
bool SAMPBridge::ProcessIncomingChatLineForTesting(const char* prefix, const char* text) {
    return jobChatAdapter_ ? jobChatAdapter_->DispatchLineForTesting(prefix, text) : true;
}
#endif

static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
    return str;
}

static bool IsHexColorCodeChar(char ch);
static std::string StripSampColorCodes(const std::string& text);

std::string SAMPBridge::ExpandMessageVars(const std::string& tmpl,
                                           const std::string& victim,
                                           const std::string& killer,
                                           const std::string& weapon) const {
    std::string msg = tmpl;

    msg = ReplaceAll(msg, "{victim}", victim.empty() ? "Unbekannt" : victim);
    msg = ReplaceAll(msg, "{killer}", killer.empty() ? "Unbekannt" : killer);
    msg = ReplaceAll(msg, "{weapon}", weapon.empty() ? "Unbekannt" : weapon);
    if (msg.find("{city}") != std::string::npos) msg = ReplaceAll(msg, "{city}", GetCityName());
    if (msg.find("{bezirk}") != std::string::npos || msg.find("{district}") != std::string::npos) {
        const std::string district = GetDistrictName();
        msg = ReplaceAll(msg, "{bezirk}", district);
        msg = ReplaceAll(msg, "{district}", district);
    }

    if (msg.find("{zone}") != std::string::npos || msg.find("{ZONE}") != std::string::npos) {
        const std::string zone = GetCurrentZone();
        msg = ReplaceAll(msg, "{zone}", zone);
        std::string upperZone = zone;
        std::transform(upperZone.begin(), upperZone.end(), upperZone.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        msg = ReplaceAll(msg, "{ZONE}", upperZone);
    }
    if (msg.find("{hp}") != std::string::npos || msg.find("{weste}") != std::string::npos) {
        float hp = 0.0f, vest = 0.0f;
        int source = 0;
        const void* ped = nullptr;
        if (!ReadLocalDamageSample(hp, vest, source, ped) || !ped) hp = vest = 0.0f;
        auto safePoints = [](float value) {
            return std::isfinite(value) && value > 0.0f
                ? static_cast<int>((std::min)(value, 1000000.0f)) : 0;
        };
        msg = ReplaceAll(msg, "{hp}", std::to_string(safePoints(hp)));
        msg = ReplaceAll(msg, "{weste}", std::to_string(safePoints(vest)));
    }
    // Retain existing templates, while keeping these vehicle-only fields out
    // of the simplified combat help. Read them only when actually requested.
    if (msg.find("{carhp}") != std::string::npos)
        msg = ReplaceAll(msg, "{carhp}", std::to_string(static_cast<int>(GetLocalPlayerCarHP())));
    if (msg.find("{carname}") != std::string::npos)
        msg = ReplaceAll(msg, "{carname}", GetVehicleName());

    {
        const int totalKills = killTracker_ ? killTracker_->GetTotalKills() : 0;
        const int totalDeaths = killTracker_ ? killTracker_->GetTotalDeaths() : 0;
        msg = ReplaceAll(msg, "{total_kills}",    std::to_string(totalKills));
        msg = ReplaceAll(msg, "{total_deaths}",   std::to_string(totalDeaths));
        msg = ReplaceAll(msg, "{session_kills}",  std::to_string(killTracker_ ? killTracker_->GetSessionKills() : 0));
        msg = ReplaceAll(msg, "{session_deaths}", std::to_string(killTracker_ ? killTracker_->GetSessionDeaths() : 0));

        std::string kd;
        if (totalDeaths == 0) {
            kd = std::to_string(totalKills) + ".00";
        } else {
            float ratio = static_cast<float>(totalKills) / static_cast<float>(totalDeaths);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", ratio);
            kd = buf;
        }
        msg = ReplaceAll(msg, "{KD}", kd);
        msg = ReplaceAll(msg, "{kd}", kd);
    }

    if (msg.size() > 144) {
        msg.resize(144);
    }

    return msg;
}

std::string SAMPBridge::FormatKillTrackerLocalKillMessage(const killtracker::KillEvent& kill) const {
    return ExpandMessageVars(killTrackerLocalKillMessageTemplate_,
                             kill.victim,
                             kill.killer,
                             killtracker::IsValidWeaponId(kill.weaponId) ? killtracker::GetWeaponName(kill.weaponId) : "Unbekannt");
}

std::string SAMPBridge::FormatKillTrackerLocalDeathMessage(const std::string& killer, int weaponId) const {
    return ExpandMessageVars(killTrackerLocalDeathMessageTemplate_,
                             "",
                             killer,
                             killtracker::IsValidWeaponId(weaponId) ? killtracker::GetWeaponName(weaponId) : "Unbekannt");
}

std::string SAMPBridge::FormatKillTrackerServerKillMessage(const killtracker::KillEvent& kill) const {
    return StripSampColorCodes(ExpandMessageVars(killTrackerServerKillMessageTemplate_,
                                                  kill.victim,
                                                  kill.killer,
                                                  killtracker::IsValidWeaponId(kill.weaponId) ? killtracker::GetWeaponName(kill.weaponId) : "Unbekannt"));
}

std::string SAMPBridge::FormatKillTrackerServerDeathMessage(const std::string& killer, int weaponId) const {
    return StripSampColorCodes(ExpandMessageVars(killTrackerServerDeathMessageTemplate_,
                                                 "",
                                                 killer,
                                                 killtracker::IsValidWeaponId(weaponId) ? killtracker::GetWeaponName(weaponId) : "Unbekannt"));
}

void SAMPBridge::EnsureKillTrackerRpcAdaptersRegistered() {
    if (damageAdapter_ == nullptr) {
        damageAdapter_ = std::make_unique<killtracker::DamageRPCAdapter>();
        damageAdapter_->Initialize();
    }

    if (deathMessageAdapter_ == nullptr) {
        deathMessageAdapter_ = std::make_unique<killtracker::DeathMessageRPCAdapter>();
        deathMessageAdapter_->Initialize();
    }
}

void SAMPBridge::RefreshDamageTracking() {
    EnsureKillTrackerRpcAdaptersRegistered();
    damageAdapter_->Initialize();
    damageAdapter_->SetDamageCallback([this](const killtracker::DamageEvent& event) {
        if (killTrackerInitialized_ && killTrackerTrackDamage_ && killTracker_) {
            killTracker_->ProcessDamageEvent(event);

#ifndef KEYBINDER_TESTING
            auto* netGame = sampapi::v03dl::RefNetGame();
            if (netGame) {
                auto* playerPool = netGame->GetPlayerPool();
                if (playerPool && event.targetId >= 0 && event.targetId < 1004) {
                    if (playerPool->IsConnected(event.targetId)) {
                        const char* name = playerPool->GetName(event.targetId);
                        if (name && name[0] != '\0') {
                            std::string victimName(name);
                            killTracker_->RecordDamageToVictim(victimName, event.weaponId, event.damage);
                        }
                    }
                }
            }
#endif
        }

        // Outgoing combat hits drive the outgoing hitsound. Incoming damage is
        // detected by PollLocalIncomingDamage() (local health monitoring), so
        // the unreliable take-damage RPC is no longer relied on here.
        NotifyDamageFeedback(event);
    });
    deathMessageAdapter_->Initialize();
    deathMessageAdapter_->SetDeathMessageCallback([this](const killtracker::DeathMessageEvent& event) {
        HandleDeathMessage(event);
    });
}

void SAMPBridge::HandleLocalKill(killtracker::KillEvent event) {
    if (!killTracker_ || !killTracker_->IsEnabled()) return;
    if (event.timestamp == 0) event.timestamp = std::time(nullptr);
    if (event.isServerStatsOnly) event.victimKnown = false;
    const unsigned long now = RuntimeTickCount();
    const unsigned long noticeTick = processingCombatChat_ ? currentCombatNoticeTick_ : now;
    const auto age = [now](unsigned long at) { return static_cast<long>(now - at) < 0 ? 0ul : now - at; };
    combatKillObservations_.erase(std::remove_if(combatKillObservations_.begin(), combatKillObservations_.end(),
        [&age](const CombatKillObservation& item) { return age(item.at) > 5000; }), combatKillObservations_.end());
    CombatKillObservation* existing = nullptr;
    const bool nativeEvent = !event.isServerStatsOnly && !event.isServerChatOnly;
    const auto nativeConflict = [&](const CombatKillObservation& item) {
        return nativeEvent && item.native
            && ((item.victimId >= 0 && event.victimId >= 0 && item.victimId != event.victimId)
                || (killtracker::IsValidWeaponId(item.nativeWeaponId) && killtracker::IsValidWeaponId(event.weaponId)
                    && item.nativeWeaponId != event.weaponId));
    };
    if (!event.isServerStatsOnly) {
        if (nativeEvent && event.victimId >= 0) {
            for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                if (!nativeConflict(*it) && it->native && it->victimId == event.victimId) { existing = &*it; break; }
            }
        }
        // Prefer a named match. Distinct victims always remain distinct, even
        // when multiple kills arrive during the same rendered frame.
        if (!existing && event.victimKnown) {
            for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                if (!nativeConflict(*it) && it->victimKnown && killtracker::SameCombatPlayerName(it->victim, event.victim)) { existing = &*it; break; }
            }
            if (!existing) {
                for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                    if (!nativeConflict(*it) && !it->victimKnown && (!event.isServerChatOnly || !(it->families & event.chatNoticeFamily))) { existing = &*it; break; }
                }
            }
        } else if (!existing && nativeEvent && event.victimId >= 0) {
            for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                if (!nativeConflict(*it) && it->native && it->victimId == event.victimId) { existing = &*it; break; }
            }
            if (!existing) {
                for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                    if (!it->native && !it->victimKnown) { existing = &*it; break; }
                }
            }
        } else if (!existing && event.isServerChatOnly) {
            // Different personal confirmation formats can describe one kill.
            // Repeating the same format is another server-confirmed kill,
            // including two kills delivered in the same frame. Do not invent a
            // duplicate callback window that could discard an explosion kill.
            for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                if (age(it->at) > 1200) break;
                if (!(it->families & event.chatNoticeFamily)) { existing = &*it; break; }
            }
        }
    }
    if (existing) {
        killTracker_->EnrichKillObservation(existing->id, event);
        if (event.victimKnown) { existing->victimKnown = true; existing->victim = event.victim; }
        if (nativeEvent) {
            existing->native = true;
            if (killtracker::IsValidWeaponId(event.weaponId)) existing->nativeWeaponId = event.weaponId;
        }
        if (event.victimId >= 0) existing->victimId = event.victimId;
        existing->families |= event.chatNoticeFamily;
        existing->lastNoticeAt = noticeTick;
        return;
    }
    event.observationId = nextCombatObservationId_++;
    if (!killTracker_->ProcessKillEvent(event)) return;
    combatKillObservations_.push_back({event.observationId, now, noticeTick, event.victim, event.victimKnown,
        nativeEvent, event.chatNoticeFamily, event.victimId,
        nativeEvent && killtracker::IsValidWeaponId(event.weaponId) ? event.weaponId : -1});
    if (combatKillObservations_.size() > 64) combatKillObservations_.erase(combatKillObservations_.begin());
    lastLocalKillTime_ = std::time(nullptr);
    if (killTrackerLocalKillMessageEnabled_) SendLocalMessage(FormatKillTrackerLocalKillMessage(event));
    if (killTrackerServerKillMessageEnabled_) SendKillTrackerServerMessage(FormatKillTrackerServerKillMessage(event));
    if (hitmarkerKillCallback_) hitmarkerKillCallback_(event);
    Log("[SAMPBridge] Local kill counted: " + event.victim);
}

void SAMPBridge::QueueCombatChat(const std::string& text) {
    if (text.empty() || text.size() > 4096) return;
    std::lock_guard<std::mutex> lock(combatChatMutex_);
    if (pendingCombatChat_.size() >= 128) pendingCombatChat_.erase(pendingCombatChat_.begin());
    pendingCombatChat_.push_back({text, RuntimeTickCount()});
}

void SAMPBridge::ResetCombatChat() {
    { std::lock_guard<std::mutex> lock(combatChatMutex_); pendingCombatChat_.clear(); }
    combatKillObservations_.clear();
    combatChatConnected_ = false; combatChatPlayer_.clear();
    hasCombatChatDeath_ = false; processingCombatChat_ = false;
}

void SAMPBridge::UpdateCombatChat(unsigned long now) {
    std::vector<PendingCombatChat> lines;
    { std::lock_guard<std::mutex> lock(combatChatMutex_); lines.swap(pendingCombatChat_); }
    now = RuntimeTickCount();
    auto* network = GetNetGame();
    const bool connected = network && network->m_nGameState == sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    const auto localName = GetLocalPlayerName();
    if (!connected || !killTracker_ || !killTracker_->IsEnabled() || localName.empty()) {
        if (combatChatConnected_) ResetCombatChat();
        return;
    }
    if (combatChatConnected_ && !killtracker::SameCombatPlayerName(localName, combatChatPlayer_)) ResetCombatChat();
    combatChatConnected_ = true; combatChatPlayer_ = localName;
    for (const auto& line : lines) {
        if (static_cast<long>(now - line.at) >= 0 && now - line.at > 5000) continue;
        const auto notice = killtracker::ParseCombatChat(line.text, localName);
        if (notice.kind == killtracker::CombatChatKind::Kill) {
            killtracker::KillEvent event;
            event.killer = localName;
            event.victim = notice.victimKnown ? notice.victim : "Unbekannt";
            event.victimKnown = notice.victimKnown;
            event.isLocalPlayer = true; event.isServerChatOnly = true;
            event.chatNoticeFamily = static_cast<std::uint32_t>(notice.family);
            event.timestamp = std::time(nullptr);
            processingCombatChat_ = true; currentCombatNoticeTick_ = line.at;
            HandleLocalKill(std::move(event));
            processingCombatChat_ = false;
        } else if (notice.kind == killtracker::CombatChatKind::Death) {
            // The RPC/native wasted state may lag a server chat notification.
            // Keep the existing life guard, plus a short chat correlation window.
            if (!localDeathHandledForCurrentLife_ && (!hasCombatChatDeath_ || now - lastCombatChatDeathAt_ > 5000)) {
                RecordLocalPlayerDeath(notice.killer, -1);
                hasCombatChatDeath_ = true; lastCombatChatDeathAt_ = now;
            }
        } else if (notice.kind == killtracker::CombatChatKind::KillCorroboration) {
            // This line names the prisoner, not the killer. It may corroborate
            // a known own kill but cannot independently establish attribution.
            for (auto it = combatKillObservations_.rbegin(); it != combatKillObservations_.rend(); ++it) {
                if (now - it->at <= 5000 && it->victimKnown && killtracker::SameCombatPlayerName(it->victim, notice.victim)) {
                    it->families |= static_cast<std::uint32_t>(notice.family); break;
                }
            }
        }
    }
}

void SAMPBridge::HandleDeathMessage(const killtracker::DeathMessageEvent& event) {
    if (!killTrackerInitialized_ || !killTracker_ || !killTracker_->IsEnabled()) return;
    int localId = -1;
    auto playerName = [this](int id) -> std::string {
#ifdef KEYBINDER_TESTING
        return g_testingHooks.getPlayerName ? g_testingHooks.getPlayerName(id) : std::string();
#else
        auto* netGame = GetNetGame();
        auto* pool = netGame ? netGame->GetPlayerPool() : nullptr;
        if (!pool || id < 0 || id >= 1004) return {};
        const char* name = pool->GetName(id);
        return name ? name : "";
#endif
    };
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.getLocalPlayerId) localId = g_testingHooks.getLocalPlayerId();
#else
    auto* netGame = GetNetGame();
    auto* pool = netGame ? netGame->GetPlayerPool() : nullptr;
    if (pool) localId = pool->m_nLocalPlayerId;
#endif
    if (localId < 0 || event.victimId < 0 || event.victimId >= 1004) return;
    if (event.victimId == localId && !localDeathHandledForCurrentLife_) {
        RecordLocalPlayerDeath(event.hasKiller && event.killerId != localId ? playerName(event.killerId) : "", event.weaponId);
    }
    if (event.hasKiller && event.killerId == localId && event.victimId != localId) {
        killtracker::KillEvent kill;
        kill.killer = GetLocalPlayerName();
        kill.victim = playerName(event.victimId);
        kill.victimId = event.victimId;
        if (kill.victim.empty()) {
            kill.victimKnown = false;
            kill.victim = "Player_" + std::to_string(event.victimId);
        }
        kill.weaponId = event.weaponId;
        kill.timestamp = event.timestamp;
        kill.isLocalPlayer = true;
        HandleLocalKill(std::move(kill));
    }
}

#ifdef KEYBINDER_TESTING
void SAMPBridge::ProcessDeathMessageForTesting(const killtracker::DeathMessageEvent& event) {
    if (deathMessageAdapter_) deathMessageAdapter_->DispatchForTesting(event);
}
#endif

void SAMPBridge::ObserveServerCombatStats(const std::string& text) {
    // Only consume a server statistics block explicitly addressed to the local
    // player. A bare total or another player's statistics cannot change counts.
    static const std::regex headerPattern(R"(^\*\*\*\s+([^\s]+)\s+\*\*\*$)");
    std::smatch header;
    if (std::regex_match(text, header, headerPattern)) {
        std::string subject = header[1].str();
        std::string localName = GetLocalPlayerName();
        std::transform(subject.begin(), subject.end(), subject.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(localName.begin(), localName.end(), localName.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        ownStatsContext_ = !localName.empty() && subject == localName;
        ownStatsContextTick_ = RuntimeTickCount();
        ownStatsContextLines_ = 6;
        return;
    }
    if (!ownStatsContext_) return;
    if (RuntimeTickCount() - ownStatsContextTick_ > 3000 || --ownStatsContextLines_ < 0) {
        ownStatsContext_ = false;
        return;
    }
    static const std::regex statsPattern(R"(^Kills:\s*\[([0-9]+(?:\.[0-9]{3})*)\]\s+Tode:\s*\[([0-9]+(?:\.[0-9]{3})*)\]\s+Fraktion:\s*\[.*\].*$)");
    std::smatch match;
    if (!std::regex_match(text, match, statsPattern)) return;
    ownStatsContext_ = false;
    auto readTotal = [](const std::string& value) {
        long long total = 0;
        for (char ch : value) {
            if (ch == '.') continue;
            total = total * 10 + (ch - '0');
            if (total > INT_MAX) return -1;
        }
        return static_cast<int>(total);
    };
    const int kills = readTotal(match[1].str());
    const int deaths = readTotal(match[2].str());
    if (kills < 0 || deaths < 0) return;
    auto* tracker = GetKillTracker();
    const std::time_t now = std::time(nullptr);
    if (serverCombatBaselineEstablished_) {
        if (kills > lastServerKillTotal_ && kills - lastServerKillTotal_ == 1
            && kills > tracker->GetTotalKills() && kills - tracker->GetTotalKills() == 1) {
            killtracker::KillEvent kill;
            kill.killer = GetLocalPlayerName();
            kill.victim = "Unbekannt";
            kill.isLocalPlayer = true;
            kill.isServerStatsOnly = true;
            kill.timestamp = now;
            HandleLocalKill(std::move(kill));
        }
        if (deaths > lastServerDeathTotal_ && deaths - lastServerDeathTotal_ == 1
            && deaths > tracker->GetTotalDeaths() && deaths - tracker->GetTotalDeaths() == 1) {
            RecordLocalPlayerDeath("", -1);
        }
    }
    // A delayed copy of the previous statistics can arrive after the killfeed.
    // Preserve a just-observed event until the newer server snapshot arrives.
    if (!serverCombatBaselineEstablished_ || kills >= tracker->GetTotalKills()
        || now - lastLocalKillTime_ > 5) tracker->ImportKillCount(kills);
    if (!serverCombatBaselineEstablished_ || deaths >= tracker->GetTotalDeaths()
        || now - lastLocalDeathTime_ > 5) tracker->ImportDeathCount(deaths);
    lastServerKillTotal_ = kills;
    lastServerDeathTotal_ = deaths;
    serverCombatBaselineEstablished_ = true;
    Log("[SAMPBridge] Received server statistics: kills=" + std::to_string(kills) + " deaths=" + std::to_string(deaths));
}
std::vector<std::tuple<int, float, int>> SAMPBridge::ExportWeaponStats() const {
    std::vector<std::tuple<int, float, int>> result;
    if (killTracker_) {
        auto stats = killTracker_->GetAllWeaponStats();
        result.reserve(stats.size());
        for (const auto& stat : stats) {
            result.emplace_back(stat.weaponId, stat.totalDamage, stat.killCount);
        }
    }
    return result;
}

void SAMPBridge::ImportWeaponStats(const std::vector<std::tuple<int, float, int>>& stats) {
    if (!killTracker_) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
        killTracker_->SetEnabled(killTrackerEnabled_);
    }
    std::unordered_map<int, killtracker::WeaponStats> statsMap;
    for (const auto& [weaponId, totalDamage, killCount] : stats) {
        killtracker::WeaponStats ws(weaponId);
        ws.totalDamage = totalDamage;
        ws.killCount = killCount;
        statsMap[weaponId] = ws;
    }
    killTracker_->ImportWeaponStats(statsMap);
}

std::vector<std::tuple<std::string, int, std::time_t, float>> SAMPBridge::ExportRecentKills() const {
    std::vector<std::tuple<std::string, int, std::time_t, float>> result;
    if (killTracker_) {
        auto kills = killTracker_->ExportRecentKills();
        result.reserve(kills.size());
        for (const auto& kill : kills) {
            result.emplace_back(kill.victim, kill.weaponId, kill.timestamp, kill.damageDealt);
        }
    }
    return result;
}

int SAMPBridge::ExportKillCount() const {
    return killTracker_ ? killTracker_->GetTotalKills() : 0;
}

void SAMPBridge::ImportKillCount(int count) {
    if (!killTracker_) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
        killTracker_->SetEnabled(killTrackerEnabled_);
    }
    killTracker_->ImportKillCount(count);
}

void SAMPBridge::ImportRecentKills(const std::vector<std::tuple<std::string, int, std::time_t, float>>& kills) {
    if (!killTracker_) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
        killTracker_->SetEnabled(killTrackerEnabled_);
    }
    std::vector<killtracker::KillEvent> events;
    events.reserve(kills.size());
    for (const auto& [victim, weaponId, timestamp, damageDealt] : kills) {
        killtracker::KillEvent event;
        event.victim = victim;
        event.weaponId = weaponId;
        event.timestamp = timestamp;
        event.damageDealt = damageDealt;
        event.isLocalPlayer = true;
        events.push_back(event);
    }
    killTracker_->ImportRecentKills(events);
}

std::vector<std::tuple<std::string, int, std::time_t>> SAMPBridge::ExportRecentDeaths() const {
    std::vector<std::tuple<std::string, int, std::time_t>> result;
    if (killTracker_) {
        auto deaths = killTracker_->ExportRecentDeaths();
        result.reserve(deaths.size());
        for (const auto& d : deaths) {
            result.emplace_back(d.killer, d.weaponId, d.timestamp);
        }
    }
    return result;
}

void SAMPBridge::ImportRecentDeaths(const std::vector<std::tuple<std::string, int, std::time_t>>& deaths) {
    if (!killTracker_) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
        killTracker_->SetEnabled(killTrackerEnabled_);
    }
    std::vector<killtracker::KillEvent> events;
    for (const auto& [killer, weaponId, timestamp] : deaths) {
        killtracker::KillEvent event;
        event.killer = killer;
        event.weaponId = weaponId;
        event.timestamp = timestamp;
        event.isLocalPlayer = true;
        events.push_back(event);
    }
    killTracker_->ImportRecentDeaths(events);
}

int SAMPBridge::ExportDeathCount() const {
    return killTracker_ ? killTracker_->GetTotalDeaths() : 0;
}

void SAMPBridge::ImportDeathCount(int count) {
    if (!killTracker_) {
        killTracker_ = std::make_unique<killtracker::KillTracker>();
        killTracker_->SetEnabled(killTrackerEnabled_);
    }
    killTracker_->ImportDeathCount(count);
}
static bool IsHexColorCodeChar(char ch) {
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

static std::string StripSampColorCodes(const std::string& text) {
    std::string stripped;
    stripped.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{' && i + 7 < text.size() && text[i + 7] == '}') {
            bool isColorCode = true;
            for (size_t j = i + 1; j < i + 7; ++j) {
                if (!IsHexColorCodeChar(text[j])) {
                    isColorCode = false;
                    break;
                }
            }

            if (isColorCode) {
                i += 7;
                continue;
            }
        }

        stripped.push_back(text[i]);
    }

    return stripped;
}

void SAMPBridge::SetIncomingChatCallback(IncomingChatCallback callback) {
    incomingChatCallback_ = std::move(callback);
}

void SAMPBridge::NotifyIncomingChat(const std::string& text) {
    CaptureStartupStatsLine(text);
    ObserveServerCombatStats(text);
    if (incomingChatCallback_) {
        incomingChatCallback_(text);
    }
}
