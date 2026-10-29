/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Queued action executor - frame-driven action sequence processing
*/

#include "ActionExecutor.h"
#include "LocationNames.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "../include/Logger.h"

#include <windows.h>
#ifndef KEYBINDER_TESTING
#include "CTheZones.h"
#include "common.h"
#endif

namespace keybinder {

#ifndef KEYBINDER_TESTING
namespace {

bool IsMouseVirtualKey(int vk)
{
    return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2;
}

bool IsExtendedVirtualKey(int vk)
{
    switch (vk) {
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

bool TrySimulateMouseClick(int vk)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[1].type = INPUT_MOUSE;

    switch (vk) {
    case VK_LBUTTON:
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        break;
    case VK_RBUTTON:
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
    case VK_MBUTTON:
        inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
    case VK_XBUTTON1:
    case VK_XBUTTON2:
        inputs[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_XUP;
        inputs[0].mi.mouseData = (vk == VK_XBUTTON1) ? XBUTTON1 : XBUTTON2;
        inputs[1].mi.mouseData = inputs[0].mi.mouseData;
        break;
    default:
        return false;
    }

    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool TrySimulateKeyboardScanCode(WORD scanCode, bool extended)
{
    if (scanCode == 0) {
        return false;
    }

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = scanCode;
    inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE | (extended ? KEYEVENTF_EXTENDEDKEY : 0);

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = scanCode;
    inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | (extended ? KEYEVENTF_EXTENDEDKEY : 0);

    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool TryResolveSingleCharacterScanCode(const std::string& name, WORD& scanCode, UINT* resolvedVkOut = nullptr)
{
    if (name.size() != 1) {
        return false;
    }

    unsigned char raw = static_cast<unsigned char>(name[0]);
    if (!std::isalnum(raw)) {
        return false;
    }

    char keyChar = static_cast<char>(raw);
    if (std::isalpha(raw)) {
        keyChar = static_cast<char>(std::tolower(raw));
    }

    HKL layout = GetKeyboardLayout(0);
    SHORT keyInfo = VkKeyScanExA(keyChar, layout);
    if (keyInfo == -1) {
        return false;
    }

    UINT resolvedVk = LOBYTE(static_cast<WORD>(keyInfo));
    UINT scan = MapVirtualKeyExA(resolvedVk, MAPVK_VK_TO_VSC, layout);
    if (scan == 0) {
        return false;
    }

    if (resolvedVkOut != nullptr) {
        *resolvedVkOut = resolvedVk;
    }
    scanCode = static_cast<WORD>(scan);
    return true;
}

std::string GetLocalizedKeyName(WORD scanCode, bool extended)
{
    if (scanCode == 0) {
        return "Unknown";
    }

    char name[128] = {};
    LONG keyInfo = static_cast<LONG>(scanCode) << 16;
    if (extended) {
        keyInfo |= 1 << 24;
    }

    const int result = GetKeyNameTextA(keyInfo, name, static_cast<int>(sizeof(name)));
    if (result > 0) {
        return std::string(name);
    }

    return "Unknown";
}

std::string FormatKeyPressDescription(const std::string& requestedName,
                                      const char* mode,
                                      UINT vk,
                                      WORD scanCode,
                                      bool extended,
                                      const std::string& localizedName)
{
    char buffer[256] = {};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "requested='%s', mode=%s, localized='%s', vk=%u, scan=%u, extended=%s",
                  requestedName.c_str(),
                  mode,
                  localizedName.c_str(),
                  static_cast<unsigned>(vk),
                  static_cast<unsigned>(scanCode),
                  extended ? "true" : "false");
    return std::string(buffer);
}

} // namespace
#endif

ActionExecutor::ActionExecutor()
    : state_(State::Idle)
    , currentIndex_(0)
    , delayStartTick_(0)
    , delayDuration_(0)
    , updating_(false)
    , manualInputBridge_(nullptr)
    , bridge_(nullptr)
    , zoneProvider_(nullptr)
    , gameDataProvider_(nullptr)
    , tickFunc_(nullptr)
{
}

ActionExecutor::~ActionExecutor()
{
    // During emergency exit (DLL_PROCESS_DETACH from /q), skip cleanup.
    // Stop() calls queue_.clear() which touches the CRT allocator -
    // safe in most cases but unnecessary during process termination.
    extern std::atomic<int> g_shutdown_phase;
    if (g_shutdown_phase.load() != 0 && g_shutdown_phase.load() != 2 && g_shutdown_phase.load() != 3) {
        return;
    }
    Stop();
}

void ActionExecutor::Execute(const std::vector<Action>& actions)
{
    Stop();

    if (actions.empty()) {
        if (OnComplete) OnComplete();
        return;
    }

    queue_ = actions;
    currentIndex_ = 0;
    const bool requiresInput = std::any_of(queue_.begin(), queue_.end(), [](const Action& action) {
        return (action.type == ActionType::Chat || action.type == ActionType::Command)
            && action.data.find("{input}") != std::string::npos;
    });
    // Slash-command callbacks run before native chat closes. Open the new chat
    // on Update, and obtain input before any part of this bind is executed.
    state_ = requiresInput ? State::RequestingInput : State::Executing;
}

void ActionExecutor::Stop()
{
    SAMPBridgeInterface* inputBridge = manualInputBridge_;
    manualInputBridge_ = nullptr;
    state_ = State::Idle;
    queue_.clear();
    currentIndex_ = 0;
    delayStartTick_ = 0;
    delayDuration_ = 0;
    manualInput_.clear();
    if (inputBridge != nullptr) {
        inputBridge->CancelManualTextInput();
    }
}

bool ActionExecutor::IsExecuting() const
{
    return state_ != State::Idle;
}

void ActionExecutor::Update()
{
    if (state_ == State::Idle) return;

    // Re-entrancy guard: prevent recursive Update calls from callbacks
    if (updating_) return;
    updating_ = true;

    switch (state_) {
    case State::RequestingInput:
        if (bridge_ != nullptr && bridge_->BeginManualTextInput()) {
            manualInputBridge_ = bridge_;
            state_ = State::WaitingInput;
        } else {
            Stop();
        }
        break;

    case State::WaitingInput: {
        std::string value;
        const ManualTextInputStatus result = manualInputBridge_ != nullptr
            ? manualInputBridge_->PollManualTextInput(value)
            : ManualTextInputStatus::Cancelled;
        if (result == ManualTextInputStatus::Submitted) {
            manualInputBridge_ = nullptr;
            manualInput_ = std::move(value);
            state_ = State::Executing;
        } else if (result == ManualTextInputStatus::Cancelled) {
            // A terminal poll result releases the bridge's request ownership.
            manualInputBridge_ = nullptr;
            Stop();
        }
        break;
    }

    case State::Executing:
        ProcessCurrentAction();
        break;

    case State::WaitingDelay: {
        unsigned long now = GetTick();
        unsigned long elapsed = now - delayStartTick_;
        if (elapsed >= delayDuration_) {
            AdvanceToNextAction();
        }
        break;
    }

    case State::Idle:
        break;
    }

    updating_ = false;
}

void ActionExecutor::SetSAMPBridge(SAMPBridgeInterface* bridge)
{
    if (manualInputBridge_ != nullptr && manualInputBridge_ != bridge) {
        Stop();
    }
    bridge_ = bridge;
}

void ActionExecutor::SetZoneProvider(ZoneProvider* provider)
{
    zoneProvider_ = provider;
}

void ActionExecutor::SetGameDataProvider(GameDataProvider* provider)
{
    gameDataProvider_ = provider;
}

void ActionExecutor::SetTickFunction(TickFunction func)
{
    tickFunc_ = func;
}

void ActionExecutor::ProcessCurrentAction()
{
    if (currentIndex_ >= queue_.size()) {
        FinishExecution();
        return;
    }

    const Action& action = queue_[currentIndex_];

    switch (action.type) {
    case ActionType::Chat:
        ExecuteChat(action.data);
        AdvanceToNextAction();
        break;

    case ActionType::Command:
        ExecuteCommand(action.data);
        AdvanceToNextAction();
        break;

    case ActionType::Delay:
        BeginDelay(action.data);
        break;

    case ActionType::KeyPress:
        ExecuteKeyPress(action.data);
        AdvanceToNextAction();
        break;
    }
}

void ActionExecutor::AdvanceToNextAction()
{
    currentIndex_++;
    if (currentIndex_ >= queue_.size()) {
        FinishExecution();
    } else {
        state_ = State::Executing;
    }
}

void ActionExecutor::FinishExecution()
{
    state_ = State::Idle;
    queue_.clear();
    currentIndex_ = 0;
    delayStartTick_ = 0;
    delayDuration_ = 0;
    manualInput_.clear();

    if (OnComplete) {
        OnComplete();
    }
}

void ActionExecutor::ExecuteChat(const std::string& data)
{
    if (!bridge_) {
        Logger::Warning("[ActionExecutor] ExecuteChat: no SAMPBridge set, skipping chat '%s'", data.c_str());
        return;
    }

    std::string expanded = ExpandPlaceholders(data);
    if (!expanded.empty() && expanded.front() == '/') {
        bridge_->SendCommand(expanded);
    } else {
        bridge_->SendChat(expanded);
    }
}

void ActionExecutor::ExecuteCommand(const std::string& data)
{
    if (!bridge_) {
        Logger::Warning("[ActionExecutor] ExecuteCommand: no SAMPBridge set, skipping command '%s'", data.c_str());
        return;
    }

    std::string expanded = ExpandPlaceholders(data);
    bridge_->SendCommand(expanded);
}

void ActionExecutor::ExecuteKeyPress(const std::string& data)
{
    const int vk = ResolveKeyPressVirtualKey(data);
    if (vk != 0) {
        SimulateKeyPress(vk);
    }
}

void ActionExecutor::BeginDelay(const std::string& data)
{
    unsigned long ms = 0;
    try {
        ms = static_cast<unsigned long>(std::stoul(data));
    } catch (...) {
        Logger::Warning("[ActionExecutor] Invalid delay value '%s', skipping", data.c_str());
        AdvanceToNextAction();
        return;
    }

    if (ms == 0) {
        AdvanceToNextAction();
        return;
    }

    delayStartTick_ = GetTick();
    delayDuration_ = ms;
    state_ = State::WaitingDelay;
}

std::string ActionExecutor::ExpandPlaceholders(const std::string& text) const
{
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{') {
            size_t end = text.find('}', i + 1);
            if (end != std::string::npos) {
                std::string tag = text.substr(i + 1, end - i - 1);

                if (tag == "input") {
                    // Append verbatim: user input containing another {tag}
                    // must never be interpreted as a second template.
                    result += manualInput_;
                    i = end;
                    continue;
                }

                if (tag == "city") {
                    result += GetCurrentCity();
                    i = end;
                    continue;
                }

                if (tag == "bezirk" || tag == "district" || tag == "zone") {
                    result += GetCurrentZone();
                    i = end;
                    continue;
                }

                if (tag == "ZONE") {
                    std::string zone = GetCurrentZone();
                    std::string upper;
                    upper.reserve(zone.size());
                    for (char c : zone) {
                        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    result += upper;
                    i = end;
                    continue;
                }

                if (tag == "hp") {
                    result += GetPlayerHPString();
                    i = end;
                    continue;
                }

                if (tag == "weste") {
                    result += GetPlayerArmourString();
                    i = end;
                    continue;
                }

                if (tag == "carhp") {
                    result += GetVehicleHPString();
                    i = end;
                    continue;
                }

                if (tag == "carname") {
                    result += GetVehicleNameString();
                    i = end;
                    continue;
                }

                if (tag == "total_kills") {
                    result += GetTotalKillsString();
                    i = end;
                    continue;
                }

                if (tag == "total_deaths") {
                    result += GetTotalDeathsString();
                    i = end;
                    continue;
                }

                if (tag == "kd") {
                    result += GetKDString();
                    i = end;
                    continue;
                }
            }
        }
        result += text[i];
    }

    return result;
}

std::string ActionExecutor::GetCurrentZone() const
{
    // If a zone provider is injected (used in tests), prefer it
    if (zoneProvider_) {
        std::string zone = zoneProvider_->GetCurrentZoneName();
        return zone.empty() ? "Unknown" : zone;
    }

    if (gameDataProvider_) {
        const std::string district = gameDataProvider_->GetDistrictName();
        if (!district.empty()) return district;
    }

#ifndef KEYBINDER_TESTING
    // Runtime: use GTA SA CTheZones
    CPlayerPed* player = FindPlayerPed(0);
    if (!player) return "Unknown";

    CVector pos = player->GetPosition();
    CZone* zone = CTheZones::FindSmallestZoneForPosition(pos, false);
    if (!zone) return "Unknown";

    const char* name = zone->GetTranslatedName();
    if (!name || name[0] == '\0') {
        // Fallback to zone label if no translated name
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

std::string ActionExecutor::GetCurrentCity() const
{
    if (gameDataProvider_) {
        const std::string city = gameDataProvider_->GetCityName();
        if (!city.empty()) return city;
    }
#ifndef KEYBINDER_TESTING
    CPlayerPed* player = FindPlayerPed(0);
    if (!player) return "Unknown";
    const CVector position = player->GetPosition();
    return CityNameFromGtaLevel(static_cast<int>(CTheZones::GetLevelFromPosition(&position)));
#else
    return "Unknown";
#endif
}

std::string ActionExecutor::GetPlayerHPString() const
{
    if (gameDataProvider_) {
        return std::to_string(static_cast<int>(gameDataProvider_->GetPlayerHP()));
    }
    return "0";
}

std::string ActionExecutor::GetPlayerArmourString() const
{
    if (gameDataProvider_) {
        return std::to_string(static_cast<int>(gameDataProvider_->GetPlayerArmour()));
    }
    return "0";
}

std::string ActionExecutor::GetVehicleHPString() const
{
    if (gameDataProvider_) {
        return std::to_string(static_cast<int>(gameDataProvider_->GetVehicleHP()));
    }
    return "0";
}

std::string ActionExecutor::GetVehicleNameString() const
{
    if (gameDataProvider_) {
        return gameDataProvider_->GetVehicleName();
    }
    return "Zu Fu\xC3\x9F";
}

std::string ActionExecutor::GetTotalKillsString() const
{
    if (gameDataProvider_) {
        return std::to_string(gameDataProvider_->GetTotalKills());
    }
    return "0";
}

std::string ActionExecutor::GetTotalDeathsString() const
{
    if (gameDataProvider_) {
        return std::to_string(gameDataProvider_->GetTotalDeaths());
    }
    return "0";
}

std::string ActionExecutor::GetKDString() const
{
    if (gameDataProvider_) {
        int kills = gameDataProvider_->GetTotalKills();
        int deaths = gameDataProvider_->GetTotalDeaths();
        if (deaths == 0) return std::to_string(kills) + ".00";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", static_cast<float>(kills) / static_cast<float>(deaths));
        return std::string(buf);
    }
    return "0.00";
}

std::string ActionExecutor::DescribeKeyPressByName(const std::string& name)
{
#ifndef KEYBINDER_TESTING
    WORD scanCode = 0;
    UINT resolvedVk = 0;
    if (TryResolveSingleCharacterScanCode(name, scanCode, &resolvedVk)) {
        return FormatKeyPressDescription(name,
                                         "scancode",
                                         resolvedVk,
                                         scanCode,
                                         false,
                                         GetLocalizedKeyName(scanCode, false));
    }

    int vk = ParseKeyName(name);
    if (vk == 0) {
        return "requested='" + name + "', mode=unresolved";
    }

    if (IsMouseVirtualKey(vk)) {
        return FormatKeyPressDescription(name, "mouse", static_cast<UINT>(vk), 0, false, name);
    }

    const bool extended = IsExtendedVirtualKey(vk);
    HKL layout = GetKeyboardLayout(0);
    WORD scanCodeFromVk = static_cast<WORD>(MapVirtualKeyExA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC, layout));
    return FormatKeyPressDescription(name,
                                     "virtual-key",
                                     static_cast<UINT>(vk),
                                     scanCodeFromVk,
                                     extended,
                                     GetLocalizedKeyName(scanCodeFromVk, extended));
#else
    return "requested='" + name + "', mode=test";
#endif
}

void ActionExecutor::SimulateKeyPressByName(const std::string& name)
{
#ifndef KEYBINDER_TESTING
    WORD scanCode = 0;
    if (TryResolveSingleCharacterScanCode(name, scanCode)) {
        if (TrySimulateKeyboardScanCode(scanCode, false)) {
            return;
        }
    }
#else
    (void)name;
#endif

    int vk = ParseKeyName(name);
    if (vk != 0) {
        SimulateKeyPress(vk);
    }
}

void ActionExecutor::SimulateKeyPress(int vk)
{
#ifndef KEYBINDER_TESTING
    if (IsMouseVirtualKey(vk)) {
        if (TrySimulateMouseClick(vk)) {
            return;
        }
    }

    HKL layout = GetKeyboardLayout(0);
    UINT scan = MapVirtualKeyExA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC, layout);
    if (TrySimulateKeyboardScanCode(static_cast<WORD>(scan), IsExtendedVirtualKey(vk))) {
        return;
    }

    INPUT inputs[2] = {};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = static_cast<WORD>(vk);

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = static_cast<WORD>(vk);
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
#else
    (void)vk;
#endif
}

int ActionExecutor::ParseKeyName(const std::string& name)
{
    if (name.empty()) return 0;

    std::string upper;
    upper.reserve(name.size());
    for (char c : name) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    if (upper.size() >= 2 && upper[0] == 'F' && std::isdigit(static_cast<unsigned char>(upper[1]))) {
        int num = 0;
        for (size_t i = 1; i < upper.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(upper[i]))) return 0;
            num = num * 10 + (upper[i] - '0');
        }
        if (num >= 1 && num <= 24) return VK_F1 + (num - 1);
        return 0;
    }

    if (upper == "ENTER" || upper == "RETURN") return VK_RETURN;
    if (upper == "ESCAPE" || upper == "ESC") return VK_ESCAPE;
    if (upper == "TAB") return VK_TAB;
    if (upper == "SPACE" || upper == "SPACEBAR") return VK_SPACE;
    if (upper == "BACKSPACE" || upper == "BACK") return VK_BACK;
    if (upper == "DELETE" || upper == "DEL") return VK_DELETE;
    if (upper == "INSERT" || upper == "INS") return VK_INSERT;
    if (upper == "HOME") return VK_HOME;
    if (upper == "END") return VK_END;
    if (upper == "PAGEUP" || upper == "PGUP") return VK_PRIOR;
    if (upper == "PAGEDOWN" || upper == "PGDN") return VK_NEXT;

    if (upper == "UP") return VK_UP;
    if (upper == "DOWN") return VK_DOWN;
    if (upper == "LEFT") return VK_LEFT;
    if (upper == "RIGHT") return VK_RIGHT;

    if (upper == "LBUTTON" || upper == "MOUSE1") return VK_LBUTTON;
    if (upper == "RBUTTON" || upper == "MOUSE2") return VK_RBUTTON;
    if (upper == "MBUTTON" || upper == "MOUSE3") return VK_MBUTTON;
    if (upper == "XBUTTON1" || upper == "MOUSE4") return VK_XBUTTON1;
    if (upper == "XBUTTON2" || upper == "MOUSE5") return VK_XBUTTON2;

    if (upper.size() == 7 && upper.substr(0, 6) == "NUMPAD") {
        char digit = upper[6];
        if (digit >= '0' && digit <= '9') return VK_NUMPAD0 + (digit - '0');
    }
    if (upper == "NUMLOCK") return VK_NUMLOCK;
    if (upper == "MULTIPLY") return VK_MULTIPLY;
    if (upper == "ADD") return VK_ADD;
    if (upper == "SUBTRACT") return VK_SUBTRACT;
    if (upper == "DECIMAL") return VK_DECIMAL;
    if (upper == "DIVIDE") return VK_DIVIDE;

    if (upper == "SHIFT" || upper == "LSHIFT") return VK_LSHIFT;
    if (upper == "RSHIFT") return VK_RSHIFT;
    if (upper == "CTRL" || upper == "CONTROL" || upper == "LCTRL") return VK_LCONTROL;
    if (upper == "RCTRL") return VK_RCONTROL;
    if (upper == "ALT" || upper == "LALT") return VK_LMENU;
    if (upper == "RALT") return VK_RMENU;

    if (upper == "CAPSLOCK") return VK_CAPITAL;
    if (upper == "SCROLLLOCK") return VK_SCROLL;
    if (upper == "PRINTSCREEN") return VK_SNAPSHOT;
    if (upper == "PAUSE") return VK_PAUSE;

    if (upper.size() == 1) {
        char ch = upper[0];
        if (ch >= 'A' && ch <= 'Z') return ch;
        if (ch >= '0' && ch <= '9') return ch;
    }

    return 0;
}

int ActionExecutor::ResolveKeyPressVirtualKey(const std::string& data)
{
    // The keybind editor stores KeyPress targets as a numeric virtual-key code
    // (e.g. "90" for Z). Resolve that first. Hand-edited or legacy configs may use
    // a symbolic key name instead, so fall back to name resolution.
    if (!data.empty()) {
        bool allDigits = true;
        for (char c : data) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            int vk = 0;
            for (char c : data) {
                vk = vk * 10 + (c - '0');
                if (vk > 255) {  // out-of-range / overflow guard
                    return 0;
                }
            }
            // Valid virtual-key codes are 0x01..0xFE.
            return (vk >= 1 && vk <= 254) ? vk : 0;
        }
    }

    return ParseKeyName(data);
}

unsigned long ActionExecutor::GetTick() const
{
    if (tickFunc_) {
        return tickFunc_();
    }

#ifndef KEYBINDER_TESTING
    return GetTickCount();
#else
    return 0;
#endif
}

} // namespace keybinder
