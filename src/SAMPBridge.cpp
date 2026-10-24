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
#include "../include/plugin-sdk/plugin_sa/game_sa/enums/ePedState.h"
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
        unsigned char passenger;

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
    SetAutoEngineStart(false);
    ResetIncomingDamageState();

    delete g_sendHook;
    g_sendHook = nullptr;

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
    if (manager && killTracker_) {
        killTracker_->SetCountKills(manager->Get().kill_tracker_count_kills);
        killTracker_->SetCountDeaths(manager->Get().kill_tracker_count_deaths);
    }
}

void SAMPBridge::Update() {
    if (!initialized_) {
        return;
    }

    const unsigned long now = RuntimeTickCount();

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
    autoEngineObservedVehicle_ = 0xFFFF;

    if (!enabled) {
        autoEngineStartPending_ = false;
        autoEngineStartScheduledAt_ = 0;
        autoEngineStartNextAttemptAt_ = 0;
        autoEngineStartVehicleId_ = 0;
        autoEngineStartAttempts_ = 0;
        autoEngineStartReadySince_ = 0;
        autoEngineStartHoldUntil_ = 0;
        if (autoEngineStartReleasePending_) {
            SetEngineStartKeyDown(false);
        }
        autoEngineStartInjectedVk_ = 0;
        autoEngineStartReleasePending_ = false;
    }
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
    auto releaseKey = [this]() {
        if (autoEngineStartReleasePending_) SetEngineStartKeyDown(false);
        autoEngineStartReleasePending_ = false;
        autoEngineStartHoldUntil_ = 0;
    };
    if (autoEngineStartReleasePending_ && (!autoEngineStartEnabled_ || !inputAvailable
        || static_cast<long>(now - autoEngineStartHoldUntil_) >= 0)) releaseKey();
    if (!autoEngineStartEnabled_) return;

    unsigned short vehicleId = 0xFFFF;
    bool engineOn = false;
    if (!ReadDriverVehicle(vehicleId, engineOn)) {
        autoEngineObservedVehicle_ = 0xFFFF;
        autoEngineStartPending_ = false;
        releaseKey();
        return;
    }
    if (vehicleId != autoEngineObservedVehicle_) {
        autoEngineObservedVehicle_ = vehicleId;
        autoEngineStartPending_ = !engineOn;
        autoEngineStartScheduledAt_ = now;
        autoEngineStartNextAttemptAt_ = now + kAutoEngineStartDelayMs;
        autoEngineStartAttempts_ = 0;
        releaseKey();
        Log("Auto engine start: driver vehicle detected, id=" + std::to_string(vehicleId));
    }
    if (!autoEngineStartPending_) return;
    bool starting = false;
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.hasEngineStartSuccessText) starting = g_testingHooks.hasEngineStartSuccessText();
#else
    starting = autoEngineStartAttempts_ > 0 && DetectEngineStartSuccessTextSource() != nullptr;
#endif
    if (engineOn || starting || now - autoEngineStartScheduledAt_ > kAutoEngineStartRetryWindowMs) {
        autoEngineStartPending_ = false;
        releaseKey();
        return;
    }
    if (!inputAvailable || autoEngineStartAttempts_ >= kAutoEngineStartMaxAttempts
        || static_cast<long>(now - autoEngineStartNextAttemptAt_) < 0) return;
    SetEngineStartKeyDown(true);
    autoEngineStartReleasePending_ = true;
    autoEngineStartHoldUntil_ = now + kAutoEngineStartHoldMs;
    autoEngineStartNextAttemptAt_ = now + kAutoEngineStartRetryMs;
    ++autoEngineStartAttempts_;
    Log("Auto engine start: held Z, attempt=" + std::to_string(autoEngineStartAttempts_));
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
        ResetKillTrackerDeathInferenceState();
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
    {
        std::lock_guard<std::mutex> lock(incomingDamageRpcMutex_);
        incomingDamageRpcQueue_.clear();
    }
    incomingDamagePed_ = nullptr;
}

bool SAMPBridge::IsDamageLogEnabled() const {
    return settingsManager_ != nullptr && settingsManager_->Get().damage_log_enabled;
}

void SAMPBridge::QueueIncomingDamageReport(const killtracker::DamageEvent& event) {
    // This callback may run on a RakNet thread: no UI, settings or ped reads.
    if (event.isOutgoing || !std::isfinite(event.damage) || event.damage <= 0.0001f
        || event.targetId < 0 || event.targetId >= 1004 || event.weaponId < 0 || event.weaponId > 46
        || keybinder::IsNonCombatDamageSource(event.weaponId)
        || !scriptedHealthState_->active.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lock(incomingDamageRpcMutex_);
    incomingDamageRpcQueue_.push_back({event.damage, RuntimeTickCount()});
    if (incomingDamageRpcQueue_.size() > 64) incomingDamageRpcQueue_.erase(incomingDamageRpcQueue_.begin());
}

void SAMPBridge::FlushIncomingDamageLog(unsigned long now, bool finishLife) {
    if (!IsDamageLogEnabled()) {
        incomingDamageLog_.Reset();
        return;
    }
    for (const float damage : incomingDamageLog_.Drain(now, finishLife)) {
        char amount[48];
        std::snprintf(amount, sizeof(amount), "%.4f", static_cast<double>(damage));
        std::string formatted = amount;
        while (!formatted.empty() && formatted.back() == '0') formatted.pop_back();
        if (!formatted.empty() && formatted.back() == '.') formatted.pop_back();
        std::replace(formatted.begin(), formatted.end(), '.', ',');
        SendLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Du hast -" + formatted + " HP Schaden erhalten.");
    }
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
    const unsigned long now = RuntimeTickCount();
    const bool logEnabled = IsDamageLogEnabled();
    const bool logJustEnabled = logEnabled && !incomingDamageLogEnabledLastFrame_;
    incomingDamageLogEnabledLastFrame_ = logEnabled;
    if (!logEnabled || logJustEnabled) {
        incomingDamageLog_.Reset();
        std::lock_guard<std::mutex> lock(incomingDamageRpcMutex_);
        incomingDamageRpcQueue_.clear();
    }
    const bool playing = IsLocalCombatActive();
    const bool finalDeathSample = !playing && incomingDamagePed_ != nullptr && IsLocalCombatActive(true);
    if ((!hitsoundIncomingCallback_ && !IsDamageLogEnabled()) || (!playing && !finalDeathSample)) {
        FlushIncomingDamageLog(now, true);
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
        FlushIncomingDamageLog(now, true);
        ResetIncomingDamageState();
        return;
    }
    // GTA can briefly retain a negative HP value on a lethal hit. Only the
    // remaining HP was actually available to lose; never log extra overkill.
    health = (std::max)(health, 0.0f);

    if (pedIdentity != incomingDamagePed_) {
        FlushIncomingDamageLog(now, true);
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
    std::vector<IncomingDamageReport> reports;
    {
        std::lock_guard<std::mutex> lock(incomingDamageRpcMutex_);
        reports.swap(incomingDamageRpcQueue_);
    }
    if (logEnabled && !logJustEnabled) {
        for (const auto& report : reports) incomingDamageLog_.Report(report.damage, report.tick);
        incomingDamageLog_.Observe(observation, now);
    }
    FlushIncomingDamageLog(now, finalDeathSample || health == 0.0f);
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
        QueueIncomingDamageReport(event);
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
    if (!event.isServerStatsOnly && killTracker_->CompleteServerStatsKill(event)) {
        Log("[SAMPBridge] Death feed confirmed the already counted server-stat kill");
        return;
    }
    if (!killTracker_->ProcessKillEvent(event)) return;
    lastLocalKillTime_ = std::time(nullptr);
    if (killTrackerLocalKillMessageEnabled_) SendLocalMessage(FormatKillTrackerLocalKillMessage(event));
    if (killTrackerServerKillMessageEnabled_) SendKillTrackerServerMessage(FormatKillTrackerServerKillMessage(event));
    if (hitmarkerKillCallback_) hitmarkerKillCallback_(event);
    Log("[SAMPBridge] Local kill counted: " + event.victim);
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
        if (kill.victim.empty()) kill.victim = "Player_" + std::to_string(event.victimId);
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
    ObserveServerCombatStats(text);
    if (incomingChatCallback_) {
        incomingChatCallback_(text);
    }
}
