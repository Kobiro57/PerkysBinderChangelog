#include "../include/SAMPBridge.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "../include/Logger.h"
#include "../include/KeyInputUtils.h"
#include "../include/SettingsManager.h"
#include "../include/WeaponNames.h"
#include "../include/DeathMessageRPCAdapter.h"

#include "../include/RakHook/include/RakHook/rakhook.hpp"
#include "../include/SAMP-API/include/sampapi/sampapi.h"
#ifndef KEYBINDER_TESTING
#include "../include/plugin-sdk/plugin_sa/game_sa/CHud.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CMessages.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CPad.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CPlayerPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CVehicle.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CModelInfo.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CText.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CVehicleModelInfo.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/ePedState.h"
#include "CTheZones.h"
#include "common.h"
#endif
#include "safetyhook.hpp"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CPlayerInfo.h"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/KeyStuff.h"

extern std::atomic<int> g_shutdown_phase;

namespace {
constexpr std::size_t kInvalidDispatcherIndex = static_cast<std::size_t>(-1);
constexpr std::size_t kMaxDirectCommands = 8;
constexpr unsigned long kStartupMessageColor = 0xFFFFFFFF;
constexpr int kTrackedIncomeMessageDelayFrames = 10;
constexpr int kLocalDeathFallbackDelayFrames = 12;
constexpr unsigned long kAutoEngineStartDelayMs = 1500;
constexpr unsigned long kAutoEngineStartReadyDwellMs = 400;
constexpr unsigned long kAutoEngineStartHoldMs = 150;
constexpr unsigned long kAutoEngineStartRetryMs = 1800;
constexpr unsigned long kAutoEngineStartRetryWindowMs = 12000;
constexpr int kAutoEngineStartMaxAttempts = 3;

SAMPBridge* g_activeBridge = nullptr;

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

#ifndef KEYBINDER_TESTING
void PulseNativeGameKeyDown(int vk)
{
    if (vk < 0 || vk > 255) {
        return;
    }

    CPad::NewKeyState.standardKeys[vk] = 0xFF;
    CPad::OldKeyState.standardKeys[vk] = 0;
}

void ReleaseNativeGameKey(int vk)
{
    if (vk < 0 || vk > 255) {
        return;
    }

    CPad::NewKeyState.standardKeys[vk] = 0;
    CPad::OldKeyState.standardKeys[vk] = 0;
}

bool ContainsAsciiNoCase(const char* haystack, const char* needle)
{
    if (haystack == nullptr || needle == nullptr || *needle == '\0') {
        return false;
    }

    const std::size_t needleLength = std::strlen(needle);
    const std::size_t haystackLength = std::strlen(haystack);
    if (needleLength > haystackLength) {
        return false;
    }

    for (std::size_t start = 0; start + needleLength <= haystackLength; ++start) {
        bool matches = true;
        for (std::size_t index = 0; index < needleLength; ++index) {
            const unsigned char left = static_cast<unsigned char>(haystack[start + index]);
            const unsigned char right = static_cast<unsigned char>(needle[index]);
            if (std::tolower(left) != std::tolower(right)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }

    return false;
}

const char* DetectEngineStartSuccessTextSource()
{
    if (CHud::m_pHelpMessageToPrint != nullptr && ContainsAsciiNoCase(CHud::m_pHelpMessageToPrint, "Motor Startet")) {
        return "help-to-print";
    }
    if (CHud::m_pHelpMessage != nullptr && ContainsAsciiNoCase(CHud::m_pHelpMessage, "Motor Startet")) {
        return "help";
    }
    if (CHud::m_Message != nullptr && ContainsAsciiNoCase(CHud::m_Message, "Motor Startet")) {
        return "message";
    }

    for (int i = 0; i < 7; ++i) {
        if (ContainsAsciiNoCase(CHud::m_BigMessage[i], "Motor Startet")) {
            return "big-message";
        }
    }

    for (const auto& message : CMessages::BriefMessages) {
        if (ContainsAsciiNoCase(message.m_pText, "Motor Startet") || ContainsAsciiNoCase(message.m_pString, "Motor Startet")) {
            return "brief-message";
        }
    }

    for (const auto& bigMessage : CMessages::BIGMessages) {
        if (ContainsAsciiNoCase(bigMessage.m_Current.m_pText, "Motor Startet") ||
            ContainsAsciiNoCase(bigMessage.m_Current.m_pString, "Motor Startet")) {
            return "queued-big-message";
        }
        for (const auto& stacked : bigMessage.m_Stack) {
            if (ContainsAsciiNoCase(stacked.m_pText, "Motor Startet") || ContainsAsciiNoCase(stacked.m_pString, "Motor Startet")) {
                return "queued-big-message-stack";
            }
        }
    }

    return nullptr;
}

bool IsLocalPlayerDriverReady(SAMPBridge* bridge, unsigned short vehicleId, std::string* reason)
{
    if (bridge == nullptr) {
        if (reason != nullptr) {
            *reason = "bridge-unavailable";
        }
        return false;
    }

    auto* localPlayer = bridge->GetLocalPlayer();
    if (localPlayer == nullptr || localPlayer->m_pPed == nullptr) {
        if (reason != nullptr) {
            *reason = "local-player-unavailable";
        }
        return false;
    }

    CPlayerPed* playerPed = FindPlayerPed(0);
    if (playerPed == nullptr) {
        if (reason != nullptr) {
            *reason = "game-ped-unavailable";
        }
        return false;
    }

    if (!playerPed->bInVehicle || playerPed->m_pVehicle == nullptr) {
        if (reason != nullptr) {
            *reason = "not-in-vehicle";
        }
        return false;
    }

    if (localPlayer->m_nCurrentVehicle != vehicleId) {
        if (reason != nullptr) {
            *reason = "vehicle-sync-pending";
        }
        return false;
    }

    if (localPlayer->m_pPed->IsPassenger()) {
        if (reason != nullptr) {
            *reason = "passenger-seat";
        }
        return false;
    }

    if (playerPed->m_ePedState != PEDSTATE_DRIVING) {
        if (reason != nullptr) {
            *reason = "ped-not-driving";
        }
        return false;
    }

    if (reason != nullptr) {
        *reason = "driver-ready";
    }
    return true;
}

bool IsLocalPlayerEngineAlreadyOn(SAMPBridge* bridge, unsigned short vehicleId)
{
    if (bridge == nullptr) {
        return false;
    }

    auto* localPlayer = bridge->GetLocalPlayer();
    if (localPlayer == nullptr || localPlayer->m_pPed == nullptr) {
        return false;
    }

    CPlayerPed* playerPed = FindPlayerPed(0);
    if (playerPed == nullptr || !playerPed->bInVehicle || playerPed->m_pVehicle == nullptr) {
        return false;
    }

    if (localPlayer->m_nCurrentVehicle != vehicleId) {
        return false;
    }

    if (localPlayer->m_pPed->IsPassenger()) {
        return false;
    }

    return playerPed->m_pVehicle->bEngineOn;
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

    // Notify incoming chat callback (used by OnlineEventDetector).
    // Only relay messages that look like server messages (not our own local messages).
    if (g_activeBridge && text != nullptr) {
        g_activeBridge->NotifyIncomingChat(std::string(text));
    }
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
bool g_bypassSendHook = false;

// __fastcall simulates __thiscall on x86 MSVC: first arg in ECX (this), second ignored (EDX), rest on stack
void __fastcall HookedCInputSend(sampapi::v03dl::CInput* pThis, void* /*edx*/, const char* szString) {
    if (!g_bypassSendHook && g_activeBridge != nullptr && szString != nullptr && szString[0] == '/') {
        if (g_activeBridge->InterceptSlashCommand(szString)) {
            return;
        }
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
      vehicleEntryHookInstalled_(false),
      originalDefaultCommand_(nullptr),
      startupMessagePending_(false),
      startupMessageDelayFrames_(0),
      hudScaleFixPending_(false),
      hudScaleFixDelayFrames_(0),
      hudScaleFixEnabled_(false),
      autoEngineStartEnabled_(false),
      autoEngineStartPending_(false),
      autoEngineStartScheduledAt_(0),
      autoEngineStartNextAttemptAt_(0),
      autoEngineStartReadySince_(0),
      autoEngineStartHoldUntil_(0),
      autoEngineStartVehicleId_(0),
      autoEngineStartAttempts_(0),
      autoEngineStartInjectedVk_(0),
      autoEngineStartReleasePending_(false),
      killTrackerEnabled_(false),
      killTrackerLocalKillMessageEnabled_(true),
      killTrackerLocalDeathMessageEnabled_(true),
      killTrackerServerKillMessageEnabled_(false),
      killTrackerServerDeathMessageEnabled_(false),
      killTrackerTrackDamage_(true),
      killTrackerLocalKillMessageTemplate_("{FF6600}[Kill] {FFFFFF}{victim} {AAAAAA}mit {FFCC00}{weapon}"),
      killTrackerLocalDeathMessageTemplate_("{FF3333}[Tod] {FFFFFF}{killer} {AAAAAA}mit {FFCC00}{weapon}"),
      killTrackerServerKillMessageTemplate_("Ich habe {victim} mit {weapon} getoetet."),
      killTrackerServerDeathMessageTemplate_("Ich wurde von {killer} mit {weapon} getoetet."),
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
    InstallVehicleEntryHook();

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

    initialized_ = true;
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

bool SAMPBridge::InstallVehicleEntryHook() {
#ifdef KEYBINDER_TESTING
    return true;
#else
    if (vehicleEntryHookInstalled_) {
        return true;
    }

    // Hook outgoing RPC 26 (EnterVehicleNotification) to detect when the
    // local player enters a vehicle. When entering as driver (passenger==false)
    // and auto-engine-start is enabled, schedule a delayed "Z" key press to
    // start the engine after the enter-vehicle animation completes.
    rakhook::on_send_rpc += [](int& rpcId, RakNet::BitStream* bs,
                               PacketPriority& priority, PacketReliability& reliability,
                               char& ord_channel, bool& sh_timestamp) -> bool {
        if (!rakhook::initialized) {
            return true;
        }

        if (rpcId != 26) { // EnterVehicleNotification
            return true;
        }

        if (g_activeBridge == nullptr || !g_activeBridge->autoEngineStartEnabled_) {
            return true;
        }

        // Non-mutating read: save and restore the read offset
        if (bs == nullptr) {
            return true;
        }

        int originalReadOffset = bs->GetReadOffset();

        unsigned short vehicleId;
        bool passenger;

        if (!bs->Read(vehicleId) || !bs->Read(passenger)) {
            bs->SetReadOffset(originalReadOffset);
            return true;
        }

        bs->SetReadOffset(originalReadOffset);

        if (!passenger) {
            if (g_activeBridge->autoEngineStartPending_ && g_activeBridge->autoEngineStartVehicleId_ == vehicleId) {
                g_activeBridge->Log("Vehicle entry detected (driver, vehicle=" + std::to_string(vehicleId) + ") while auto engine start is already pending; keeping existing schedule");
                return true;
            }

            g_activeBridge->autoEngineStartPending_ = true;
            g_activeBridge->autoEngineStartScheduledAt_ = GetTickCount();
            g_activeBridge->autoEngineStartNextAttemptAt_ = g_activeBridge->autoEngineStartScheduledAt_ + kAutoEngineStartDelayMs;
            g_activeBridge->autoEngineStartReadySince_ = 0;
            g_activeBridge->autoEngineStartHoldUntil_ = 0;
            g_activeBridge->autoEngineStartVehicleId_ = vehicleId;
            g_activeBridge->autoEngineStartAttempts_ = 0;
            g_activeBridge->Log("Vehicle entry detected (driver, vehicle=" + std::to_string(vehicleId) + "); scheduling auto engine start in " + std::to_string(kAutoEngineStartDelayMs) + " ms");
        }

        return true;
    };

    vehicleEntryHookInstalled_ = true;
    Log("Vehicle entry RPC hook installed for auto engine start");
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

    Log("Shutting down SAMPBridge");

    delete g_sendHook;
    g_sendHook = nullptr;

    ShutdownJobTracker();

    ShutdownKillTracker();

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
    AddMessageHook(chat, color, encodedText.c_str());
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

bool SAMPBridge::IsChatOpen() const {
    const auto* input = GetInput();
    return input != nullptr && input->m_bEnabled != FALSE;
}

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
}

void SAMPBridge::Update() {
    if (!initialized_) {
        return;
    }

    const unsigned long now = GetTickCount();

    if (!jobTrackerInitialized_) {
        InitializeJobTracker();
    }

    if (!HasCommandRouting()) {
        RegisterPendingCommands();
    }

    if (startupMessageDelayFrames_ > 0) {
        startupMessageDelayFrames_--;
    }
    if (hudScaleFixDelayFrames_ > 0) {
        hudScaleFixDelayFrames_--;
    }

#ifndef KEYBINDER_TESTING
    if (autoEngineStartReleasePending_) {
        if (now < autoEngineStartHoldUntil_) {
            PulseNativeGameKeyDown(autoEngineStartInjectedVk_);
        } else {
            ReleaseNativeGameKey(autoEngineStartInjectedVk_);
            autoEngineStartReleasePending_ = false;
            autoEngineStartInjectedVk_ = 0;
            autoEngineStartHoldUntil_ = 0;
        }
    }
#endif

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

    if (hudScaleFixPending_) {
        if (hudScaleFixDelayFrames_ == 0) {
            auto* input = GetInput();
            if (input != nullptr) {
                auto handler = RealGetCommandHandler(input, "hudscalefix");
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

    if (autoEngineStartPending_) {
        if (now - autoEngineStartScheduledAt_ > kAutoEngineStartRetryWindowMs) {
            Log("Auto engine start: giving up after " + std::to_string(kAutoEngineStartRetryWindowMs) + " ms without confirmed engine start");
            autoEngineStartPending_ = false;
        } else {
#ifndef KEYBINDER_TESTING
            if (autoEngineStartAttempts_ > 0) {
                const char* successSource = DetectEngineStartSuccessTextSource();
                if (successSource != nullptr) {
                    Log(std::string("Auto engine start: detected success text 'Motor Startet' in ") + successSource + "; stopping retries");
                    autoEngineStartPending_ = false;
                }
            }

            if (!autoEngineStartPending_) {
            } else {
                std::string readiness;
                const bool ready = IsLocalPlayerDriverReady(this, autoEngineStartVehicleId_, &readiness);
                if (ready) {
                    if (autoEngineStartReadySince_ == 0) {
                        autoEngineStartReadySince_ = now;
                        if (autoEngineStartAttempts_ == 0) {
                            Log("Auto engine start: driver-ready detected; waiting " + std::to_string(kAutoEngineStartReadyDwellMs) + " ms before first attempt");
                        }
                    }

                    const bool readyLongEnough = (now - autoEngineStartReadySince_) >= kAutoEngineStartReadyDwellMs;
                    if (readyLongEnough && autoEngineStartAttempts_ == 0 && IsLocalPlayerEngineAlreadyOn(this, autoEngineStartVehicleId_)) {
                        Log("Auto engine start: engine already on when driver-ready; skipping auto-start");
                        autoEngineStartPending_ = false;
                    } else if (readyLongEnough && autoEngineStartAttempts_ < kAutoEngineStartMaxAttempts && now >= autoEngineStartNextAttemptAt_) {
                        const int vk = keybinder::ActionExecutor::ParseKeyName("Z");
                        const std::string keyPressDescription = keybinder::ActionExecutor::DescribeKeyPressByName("Z");

                        if (vk != 0) {
                            PulseNativeGameKeyDown(vk);
                            autoEngineStartReleasePending_ = true;
                            autoEngineStartInjectedVk_ = vk;
                            autoEngineStartHoldUntil_ = now + kAutoEngineStartHoldMs;
                        }
                        autoEngineStartAttempts_++;
                        autoEngineStartNextAttemptAt_ = now + kAutoEngineStartRetryMs;

                        Log("Auto engine start attempt " + std::to_string(autoEngineStartAttempts_) + "/" + std::to_string(kAutoEngineStartMaxAttempts) +
                            ": native game key hold for " + std::to_string(kAutoEngineStartHoldMs) + " ms (" + keyPressDescription + ")");

                        if (autoEngineStartAttempts_ >= kAutoEngineStartMaxAttempts) {
                            Log("Auto engine start: reached retry limit; waiting for success text or timeout");
                        }
                    }
                } else {
                    autoEngineStartReadySince_ = 0;
                    if (autoEngineStartAttempts_ == 0 && now >= autoEngineStartNextAttemptAt_) {
                        Log("Auto engine start: waiting for driver-ready state (" + readiness + ")");
                        autoEngineStartNextAttemptAt_ = now + 250;
                    }
                }
            }
#else
            if (now >= autoEngineStartNextAttemptAt_) {
                keybinder::ActionExecutor::SimulateKeyPressByName("Z");
                autoEngineStartPending_ = false;
            }
#endif
        }
    }

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

    const std::string line1 = "{33CCFF}[NLBinder]{FFFFFF} Nova Legacy Binder von perkys wurde {00CC00}erfolgreich{FFFFFF} gestartet!";
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
    if (!killTrackerInitialized_) {
        return;
    }

    Log("Shutting down Kill Tracker");

    if (damageAdapter_) {
        damageAdapter_->Shutdown();
    }
    if (deathMessageAdapter_) {
        deathMessageAdapter_->Shutdown();
    }

    ResetKillTrackerDeathInferenceState();
    killfeedAdapter_.reset();

    killTrackerInitialized_ = false;
    Log("Kill Tracker shutdown complete");
}

void SAMPBridge::UpdateKillTracker() {
    if (!killTrackerInitialized_ || !killTracker_ || !killfeedAdapter_ || !killTracker_->IsEnabled()) {
        return;
    }

    auto newKills = killfeedAdapter_->PollForNewKills();

    for (const auto& kill : newKills) {
        if (killTracker_->ProcessKillEvent(kill)) {
            std::string localMessage = FormatKillTrackerLocalKillMessage(kill);
            std::string serverMessage = FormatKillTrackerServerKillMessage(kill);
            if (killTrackerLocalKillMessageEnabled_) {
                SendLocalMessage(localMessage);
            }
            if (killTrackerServerKillMessageEnabled_) {
                SendKillTrackerServerMessage(serverMessage);
            }
            Log(std::string("Kill: ") + localMessage);
        }
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

    if (killTrackerInitialized_) {
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
        killTrackerLocalKillMessageTemplate_ = "{FF6600}[Kill] {FFFFFF}{victim} {AAAAAA}mit {FFCC00}{weapon}";
        return;
    }
    killTrackerLocalKillMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetKillTrackerLocalDeathMessageTemplate(const std::string& messageTemplate) {
    if (messageTemplate.empty()) {
        killTrackerLocalDeathMessageTemplate_ = "{FF3333}[Tod] {FFFFFF}{killer} {AAAAAA}mit {FFCC00}{weapon}";
        return;
    }
    killTrackerLocalDeathMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetKillTrackerServerKillMessageTemplate(const std::string& messageTemplate) {
    if (messageTemplate.empty()) {
        killTrackerServerKillMessageTemplate_ = "Ich habe {victim} mit {weapon} getoetet.";
        return;
    }
    killTrackerServerKillMessageTemplate_ = messageTemplate;
}

void SAMPBridge::SetKillTrackerServerDeathMessageTemplate(const std::string& messageTemplate) {
    if (messageTemplate.empty()) {
        killTrackerServerDeathMessageTemplate_ = "Ich wurde von {killer} mit {weapon} getoetet.";
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
    autoEngineStartEnabled_ = enabled;

    if (!enabled) {
        autoEngineStartPending_ = false;
        autoEngineStartScheduledAt_ = 0;
        autoEngineStartNextAttemptAt_ = 0;
        autoEngineStartVehicleId_ = 0;
        autoEngineStartAttempts_ = 0;
        autoEngineStartReadySince_ = 0;
        autoEngineStartHoldUntil_ = 0;
#ifndef KEYBINDER_TESTING
        if (autoEngineStartReleasePending_) {
            ReleaseNativeGameKey(autoEngineStartInjectedVk_);
        }
#endif
        autoEngineStartInjectedVk_ = 0;
        autoEngineStartReleasePending_ = false;
    }
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
    return "Unknown";
#endif
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
    return "";
#endif
}

float SAMPBridge::GetLocalPlayerHP() const {
#ifndef KEYBINDER_TESTING
    auto* lp = GetLocalPlayer();
    if (!lp) return 0.0f;

    if (lp->m_incarData.m_nDriverHealth > 0 || lp->m_incarData.m_nDriverArmor > 0) {
        return static_cast<float>(lp->m_incarData.m_nDriverHealth);
    }

    if (lp->m_onfootData.m_nHealth > 0 || lp->m_onfootData.m_nArmor > 0) {
        return static_cast<float>(lp->m_onfootData.m_nHealth);
    }

    if (lp->m_pPed) {
        const float health = lp->m_pPed->GetHealth();
        if (std::isfinite(health)) {
            return static_cast<float>(std::lround(health));
        }
    }
#endif
    return 0.0f;
}

float SAMPBridge::GetLocalPlayerArmour() const {
#ifndef KEYBINDER_TESTING
    auto* lp = GetLocalPlayer();
    if (!lp) return 0.0f;

    if (lp->m_incarData.m_nDriverHealth > 0 || lp->m_incarData.m_nDriverArmor > 0) {
        return static_cast<float>(lp->m_incarData.m_nDriverArmor);
    }

    if (lp->m_onfootData.m_nHealth > 0 || lp->m_onfootData.m_nArmor > 0) {
        return static_cast<float>(lp->m_onfootData.m_nArmor);
    }

    if (lp->m_pPed) {
        const float armour = lp->m_pPed->GetArmour();
        if (std::isfinite(armour)) {
            return static_cast<float>(std::lround(armour));
        }
    }
#endif
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

    if (killTracker_ != nullptr) {
        killTracker_->UpdateDeathWindowState({});
    }
}

void SAMPBridge::RecordLocalPlayerDeath(const std::string& killer, int weaponId) {
    if (!killTracker_) {
        return;
    }

    killTracker_->RecordDeath(killer, weaponId);
    localPlayerWasDead_ = true;
    localDeathHandledForCurrentLife_ = true;
    localDeathFallbackDelayFrames_ = 0;

    Log("[SAMPBridge] Local player death recorded: killer=" +
        (killer.empty() ? std::string("<none>") : killer) +
        " weapon=" + std::to_string(weaponId));

    if (killTrackerLocalDeathMessageEnabled_) {
        SendLocalMessage(FormatKillTrackerLocalDeathMessage(killer, weaponId));
    }
    if (killTrackerServerDeathMessageEnabled_) {
        SendKillTrackerServerMessage(FormatKillTrackerServerDeathMessage(killer, weaponId));
    }
}

void SAMPBridge::UpdateLocalDeathFallback() {
    if (!killTracker_ || !killfeedAdapter_) {
        return;
    }

    const bool isDead = IsLocalPlayerDead();
    if (!isDead) {
        ResetKillTrackerDeathInferenceState();
        return;
    }

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
        if (!state) {
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

void SAMPBridge::PollLocalIncomingDamage() {
    if (!hitsoundIncomingCallback_) {
        return;
    }

#ifndef KEYBINDER_TESTING
    // Drive the incoming hitsound from the local player's health+armour
    // dropping rather than the client-sent take-damage RPC. SA-MP only emits
    // that RPC when it detects damage client-side, so on servers with
    // server-side health it never fires -- which is why incoming hitsounds
    // were unreliable. Reading the ped's own health works everywhere.
    CPlayerPed* ped = FindPlayerPed(0);
    const bool alive = ped != nullptr && ped->m_fHealth > 0.0f;

    float combinedHp = 0.0f;
    bool environmental = false;
    if (alive) {
        combinedHp = ped->m_fHealth + ped->m_fArmour;
        // m_nLastWeaponDamage names the cause of the most recent damage, so a
        // fall/drown/collision drop is filtered exactly like SA-MP weapon 54.
        environmental = keybinder::IsNonCombatDamageSource(static_cast<int>(ped->m_nLastWeaponDamage));

        // Publish the current ped HP/armour into shared state so the
        // ScrSetPlayerHealth/ScrSetPlayerArmour RPC hook (which runs on the
        // RakNet receive thread) can compare the incoming RPC value against
        // this last-polled baseline. This is the pre-RPC baseline the hook
        // uses to decide whether the RPC is a decrease. The cache may lag by
        // up to one frame depending on frame ordering (RPC may be processed
        // before this poll runs), but a one-frame-stale baseline is still a
        // valid pre-RPC reference: the ped HP cannot have changed between the
        // last poll and the RPC unless a prior scripted update already moved
        // it (in which case the prior RPC would have armed its own window).
        scriptedHealthState_->lastKnownHealth.store(ped->m_fHealth, std::memory_order_relaxed);
        scriptedHealthState_->lastKnownArmour.store(ped->m_fArmour, std::memory_order_relaxed);
    }

    // Consume any scripted-health-decrease request flagged by the RPC hook.
    // exchange(false) clears the request so each RPC arms at most one window.
    if (scriptedHealthState_->suppressionRequested.exchange(false, std::memory_order_acq_rel)) {
        incomingDamageDetector_.ArmScriptedSuppression();
    }

    if (incomingDamageDetector_.Observe(alive, combinedHp, environmental)) {
        hitsoundIncomingCallback_();
    }
#endif
}

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

    msg = ReplaceAll(msg, "{victim}", victim);
    msg = ReplaceAll(msg, "{killer}", killer);
    msg = ReplaceAll(msg, "{weapon}", weapon);

    msg = ReplaceAll(msg, "{zone}", GetCurrentZone());

    {
        int hp = static_cast<int>(GetLocalPlayerHP());
        int vest = static_cast<int>(GetLocalPlayerArmour());
        int carhp = static_cast<int>(GetLocalPlayerCarHP());
        msg = ReplaceAll(msg, "{hp}", std::to_string(hp));
        msg = ReplaceAll(msg, "{weste}", std::to_string(vest));
        msg = ReplaceAll(msg, "{carhp}", std::to_string(carhp));
    }

    msg = ReplaceAll(msg, "{carname}", GetVehicleName());

    if (killTracker_) {
        int totalKills  = killTracker_->GetTotalKills();
        int totalDeaths = killTracker_->GetTotalDeaths();
        msg = ReplaceAll(msg, "{total_kills}",    std::to_string(totalKills));
        msg = ReplaceAll(msg, "{total_deaths}",   std::to_string(totalDeaths));
        msg = ReplaceAll(msg, "{session_kills}",  std::to_string(killTracker_->GetSessionKills()));
        msg = ReplaceAll(msg, "{session_deaths}", std::to_string(killTracker_->GetSessionDeaths()));

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
                             killtracker::GetWeaponName(kill.weaponId));
}

std::string SAMPBridge::FormatKillTrackerLocalDeathMessage(const std::string& killer, int weaponId) const {
    return ExpandMessageVars(killTrackerLocalDeathMessageTemplate_,
                             "",
                             killer,
                             killtracker::GetWeaponName(weaponId));
}

std::string SAMPBridge::FormatKillTrackerServerKillMessage(const killtracker::KillEvent& kill) const {
    return StripSampColorCodes(ExpandMessageVars(killTrackerServerKillMessageTemplate_,
                                                  kill.victim,
                                                  kill.killer,
                                                  killtracker::GetWeaponName(kill.weaponId)));
}

std::string SAMPBridge::FormatKillTrackerServerDeathMessage(const std::string& killer, int weaponId) const {
    return StripSampColorCodes(ExpandMessageVars(killTrackerServerDeathMessageTemplate_,
                                                 "",
                                                 killer,
                                                 killtracker::GetWeaponName(weaponId)));
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
    if (!killTrackerInitialized_) {
        return;
    }

    EnsureKillTrackerRpcAdaptersRegistered();

    if (!killTrackerTrackDamage_) {
        if (damageAdapter_) {
            damageAdapter_->SetDamageCallback(killtracker::DamageEventCallback{});
            damageAdapter_->Shutdown();
        }
        if (deathMessageAdapter_) {
            deathMessageAdapter_->SetDeathMessageCallback(killtracker::DeathMessageCallback{});
            deathMessageAdapter_->Shutdown();
        }
        return;
    }

    damageAdapter_->Initialize();
    damageAdapter_->SetDamageCallback([this](const killtracker::DamageEvent& event) {
        if (killTracker_) {
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
        if (event.isOutgoing && hitsoundDamageCallback_) {
            hitsoundDamageCallback_(event);
        }

        if (event.isOutgoing && hitmarkerDamageCallback_) {
            hitmarkerDamageCallback_(event);
        }
    });

    deathMessageAdapter_->Initialize();
    deathMessageAdapter_->SetDeathMessageCallback([this](const killtracker::DeathMessageEvent& event) {
        Log("[SAMPBridge] Death message received: killer=" + std::to_string(event.killerId) +
            " victim=" + std::to_string(event.victimId) + " weapon=" + std::to_string(event.weaponId));

#ifndef KEYBINDER_TESTING
        auto* netGame = sampapi::v03dl::RefNetGame();
        if (!netGame) return;

        auto* playerPool = netGame->GetPlayerPool();
        if (!playerPool) return;

        int localPlayerId = playerPool->m_nLocalPlayerId;

        if (event.victimId == localPlayerId) {
            if (killTracker_ && !localDeathHandledForCurrentLife_) {
                std::string killerName;
                if (event.hasKiller && event.killerId >= 0 && event.killerId < 1004) {
                    const char* name = playerPool->GetName(event.killerId);
                    if (name && name[0] != '\0') {
                        killerName = name;
                    } else {
                        killerName = "Player_" + std::to_string(event.killerId);
                    }
                }
                RecordLocalPlayerDeath(killerName, event.weaponId);
            }
        }

        if (event.hasKiller && event.killerId == localPlayerId) {
            Log("[SAMPBridge] Local player kill detected!");

            std::string victimName;
            if (event.victimId >= 0 && event.victimId < 1004) {
                const char* name = playerPool->GetName(event.victimId);
                if (name && name[0] != '\0') {
                    victimName = name;
                } else {
                    victimName = "Player_" + std::to_string(event.victimId);
                }
            } else {
                victimName = "Unknown";
            }

            if (killTracker_) {
                killtracker::KillEvent killEvent;
                killEvent.killer = "perkys";
                killEvent.victim = victimName;
                killEvent.weaponId = event.weaponId;
                killEvent.timestamp = event.timestamp;
                killEvent.isLocalPlayer = true;
                killEvent.isSuicide = false;
                killEvent.isSystem = false;

                if (killTracker_->ProcessKillEvent(killEvent)) {
                    std::string localMessage = FormatKillTrackerLocalKillMessage(killEvent);
                    std::string serverMessage = FormatKillTrackerServerKillMessage(killEvent);
                    if (killTrackerLocalKillMessageEnabled_) {
                        SendLocalMessage(localMessage);
                    }
                    if (killTrackerServerKillMessageEnabled_) {
                        SendKillTrackerServerMessage(serverMessage);
                    }
                    if (hitmarkerKillCallback_) {
                        hitmarkerKillCallback_(killEvent);
                    }
                    Log(std::string("Kill: ") + localMessage);
                }
            }
        }
#endif
    });
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
    if (incomingChatCallback_) {
        incomingChatCallback_(text);
    }
}
