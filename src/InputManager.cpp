#include "InputManager.h"

#include <algorithm>
#include <vector>

#include "../include/ActionExecutor.h"
#include "../include/ImGuiOverlay.h"
#include "../include/KeybindManager.h"
#include "../include/Logger.h"
#include "../include/SAMPBridge.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../plugin-sdk/shared/extensions/KeyCheck.h"

#ifndef KEYBINDER_TESTING
#include "../plugin-sdk/plugin_sa/game_sa/CPad.h"
#endif

namespace keybinder {

namespace {

bool IsValidEarlyKey(int key) {
    return key >= 0 && key < 256;
}

}

#ifdef KEYBINDER_TESTING
namespace {
InputManager::TestingHooks g_testingHooks;
}
#endif

InputManager::InputManager()
    : initialized_(false),
      keybindManager_(nullptr),
      actionExecutor_(nullptr),
      sampBridge_(nullptr),
      overlay_(nullptr),
      quickMapVisibleCallback_(nullptr),
      lastOverlayVisible_(false),
      lastInputGated_(false),
      frameCounter_(0) {}

void InputManager::Initialize() {
    if (initialized_) {
        return;
    }

    if (keybindManager_ == nullptr) {
        Logger::Warning("[InputManager] Initialize: keybindManager is null");
    }
    if (actionExecutor_ == nullptr) {
        Logger::Warning("[InputManager] Initialize: actionExecutor is null");
    }

    PollKeyStates();
    ReconcileEarlyKeyState();
    lastOverlayVisible_ = false;
    lastInputGated_ = false;
    initialized_ = true;
    Logger::Debug("[InputManager] Initialized");
}

void InputManager::Update() {
    if (!initialized_) {
        Initialize();
    }

    if (actionExecutor_ != nullptr) {
        actionExecutor_->Update();
    }

    PollKeyStates();
    ReconcileEarlyKeyState();
    
    frameCounter_++;
    if (frameCounter_ % 300 == 0) {
        size_t keybindCount = keybindManager_ ? keybindManager_->Count() : 0;
        Logger::Info("[InputManager] Heartbeat: %d frames, %zu keybind(s) registered, input_gated=%s", 
            frameCounter_, keybindCount, IsInputGated() ? "true" : "false");
    }
    
    bool overlay_visible = false;
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.isInputGatedOverlayVisible) {
        overlay_visible = g_testingHooks.isInputGatedOverlayVisible();
    }
#else
    overlay_visible = overlay_ != nullptr && overlay_->IsVisible();
#endif
    if (overlay_visible != lastOverlayVisible_) {
        Logger::Info("[InputManager] Overlay %s; %s GTA mouse/camera input suppression",
            overlay_visible ? "visible" : "hidden",
            overlay_visible ? "enabling" : "disabling");
        lastOverlayVisible_ = overlay_visible;
    }

    if (overlay_visible) {
        SuppressOverlayMouseInput();
        if (IsImGuiCapturingKeyboard()) {
            SuppressOverlayKeyboardInput();
        }
    }

    const bool quickMapVisible = quickMapVisibleCallback_ ? quickMapVisibleCallback_() : false;
    if (quickMapVisible && !overlay_visible) {
        SuppressOverlayMouseInput();
    }

    const bool input_gated = IsInputGated();
    if (input_gated != lastInputGated_) {
        Logger::Info("[InputManager] Input gating %s (%s)",
            input_gated ? "active" : "inactive",
            input_gated ? GetInputGateReason() : "no blocker active");
        lastInputGated_ = input_gated;
    }

    std::vector<Action> pendingActions;
    if (!pendingEarlyActions_.empty()) {
        pendingActions.swap(pendingEarlyActions_);
    }

    DispatchTriggeredBinds(pendingActions);

    if (!pendingActions.empty() && actionExecutor_ != nullptr) {
        Logger::Info("[InputManager] Queueing %zu action(s) for execution", pendingActions.size());
        actionExecutor_->Execute(pendingActions);
    }
}

void InputManager::SetKeybindManager(KeybindManager* manager) {
    keybindManager_ = manager;
}

void InputManager::SetActionExecutor(ActionExecutor* executor) {
    actionExecutor_ = executor;
}

void InputManager::SetSAMPBridge(SAMPBridge* bridge) {
    sampBridge_ = bridge;
}

void InputManager::SetImGuiOverlay(ImGuiOverlay* overlay) {
    overlay_ = overlay;
}

void InputManager::SetQuickMapVisibleCallback(std::function<bool()> callback) {
    quickMapVisibleCallback_ = std::move(callback);
}

bool InputManager::HandleEarlyKeyEvent(int key, bool keyDown) {
    if (!IsValidEarlyKey(key)) {
        return false;
    }

    if (!keyDown) {
        earlyWindowKeyDown_[key] = false;
        earlyHandledKeys_[key] = false;
        earlySuppressedKeys_[key] = false;
        return false;
    }

    if (earlyWindowKeyDown_[key]) {
        return earlySuppressedKeys_[key];
    }

    earlyWindowKeyDown_[key] = true;

    if (keybindManager_ == nullptr || actionExecutor_ == nullptr) {
        return false;
    }

    if (IsInputGated()) {
        return false;
    }

    std::vector<int> downKeys;
    downKeys.reserve(8);
    for (unsigned int currentKey = 0; currentKey < earlyWindowKeyDown_.size(); ++currentKey) {
        if (earlyWindowKeyDown_[currentKey]) {
            downKeys.push_back(static_cast<int>(currentKey));
        }
    }

    const std::vector<size_t> matches = CollectMatchingKeybinds(downKeys, key);
    if (matches.empty()) {
        return false;
    }

    bool suppressGameInput = false;
    bool dispatched = false;
    for (size_t index : matches) {
        const Keybind* keybind = keybindManager_->Get(index);
        if (keybind == nullptr) {
            Logger::Warning("[InputManager] Early keybind at index %zu is null", index);
            continue;
        }

        if (!keybind->enabled) {
            Logger::Debug("[InputManager] Early keybind '%s' is disabled, skipping", keybind->name.c_str());
            continue;
        }

        Logger::Info("[InputManager] Early-triggered keybind '%s' (key=%d, keys=%zu, actions=%zu, passthrough=%s)",
            keybind->name.c_str(), keybind->key, keybind->GetAllKeys().size(), keybind->actions.size(),
            keybind->passthrough ? "true" : "false");

        pendingEarlyActions_.insert(pendingEarlyActions_.end(), keybind->actions.begin(), keybind->actions.end());
        dispatched = true;

        if (!keybind->passthrough) {
            suppressGameInput = true;
            for (int comboKey : keybind->GetAllKeys()) {
                if (IsValidEarlyKey(comboKey)) {
                    earlySuppressedKeys_[comboKey] = true;
                }
            }
            earlySuppressedKeys_[key] = true;
        }
    }

    if (!dispatched) {
        return false;
    }

    earlyHandledKeys_[key] = true;
    return suppressGameInput;
}

void InputManager::ClearTransientInputState() {
    earlyHandledKeys_.fill(false);
    earlySuppressedKeys_.fill(false);
    earlyWindowKeyDown_.fill(false);
    pendingEarlyActions_.clear();
}

void InputManager::ApplyEarlySuppressedKeys() const {
    for (unsigned int key = 0; key < earlySuppressedKeys_.size(); ++key) {
        if (earlySuppressedKeys_[key]) {
            SuppressGameKey(static_cast<int>(key));
        }
    }
}

#ifdef KEYBINDER_TESTING
void InputManager::SetTestingHooks(const TestingHooks& hooks) {
    g_testingHooks = hooks;
}

void InputManager::ResetTestingHooks() {
    g_testingHooks = {};
}
#endif

void InputManager::PollKeyStates() const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.updateKeyStates) {
        g_testingHooks.updateKeyStates();
    }
#else
    KeyCheck::Update();
#endif
}

void InputManager::DispatchTriggeredBinds(std::vector<Action>& pendingActions) {
    if (keybindManager_ == nullptr) {
        Logger::Debug("[InputManager] DispatchTriggeredBinds: keybindManager is null");
        return;
    }
    
    if (actionExecutor_ == nullptr) {
        Logger::Debug("[InputManager] DispatchTriggeredBinds: actionExecutor is null");
        return;
    }
    
    if (IsInputGated()) {
        Logger::Debug("[InputManager] DispatchTriggeredBinds: input is gated (%s)", GetInputGateReason());
        return;
    }

    std::vector<int> downKeys;
    for (unsigned int key = 0; key < 256; ++key) {
        if (IsKeyDown(key)) {
            downKeys.push_back(static_cast<int>(key));
        }
    }

    int triggeredCount = 0;
    int keysPressed = 0;

    for (unsigned int key = 0; key < 256; ++key) {
        if (!IsKeyJustDown(key)) {
            continue;
        }
        if (earlyHandledKeys_[key]) {
            Logger::Debug("[InputManager] Key %d already handled by early hook, skipping polled dispatch", key);
            continue;
        }
        keysPressed++;

        const std::vector<size_t> matches = CollectMatchingKeybinds(downKeys, static_cast<int>(key));

        if (matches.empty()) {
            Logger::Debug("[InputManager] Key %d pressed but no keybinds match",
                key);
            continue;
        }

        Logger::Debug("[InputManager] Key pressed: %d, found %zu match(es)",
            key, matches.size());

        bool suppressGameInput = false;
        bool dispatched = false;
        for (size_t index : matches) {
            const Keybind* keybind = keybindManager_->Get(index);
            if (keybind == nullptr) {
                Logger::Warning("[InputManager] Keybind at index %zu is null", index);
                continue;
            }
            
            if (!keybind->enabled) {
                Logger::Debug("[InputManager] Keybind '%s' is disabled, skipping", keybind->name.c_str());
                continue;
            }

            Logger::Info("[InputManager] Triggered keybind '%s' (key=%d, keys=%zu, actions=%zu, passthrough=%s)",
                keybind->name.c_str(), keybind->key, keybind->GetAllKeys().size(), keybind->actions.size(),
                keybind->passthrough ? "true" : "false");

            pendingActions.insert(pendingActions.end(), keybind->actions.begin(), keybind->actions.end());
            suppressGameInput = suppressGameInput || !keybind->passthrough;
            dispatched = true;
            triggeredCount++;
        }

        if (dispatched && suppressGameInput) {
            std::vector<int> allKeys = matches.empty() ? std::vector<int>{} : 
                keybindManager_->Get(matches[0])->GetAllKeys();
            for (int k : allKeys) {
                SuppressGameKey(k);
            }
            SuppressGameKey(static_cast<int>(key));
            Logger::Debug("[InputManager] Suppressed game input for key combination");
        }
    }

    if (keysPressed > 0) {
        Logger::Debug("[InputManager] Frame summary: %d key(s) pressed, %d keybind(s) triggered",
            keysPressed, triggeredCount);
    }
}

void InputManager::ReconcileEarlyKeyState() {
    for (unsigned int key = 0; key < earlyWindowKeyDown_.size(); ++key) {
        if ((earlyWindowKeyDown_[key] || earlyHandledKeys_[key] || earlySuppressedKeys_[key]) && !IsKeyDown(key)) {
            earlyWindowKeyDown_[key] = false;
            earlyHandledKeys_[key] = false;
            earlySuppressedKeys_[key] = false;
        }
    }
}

std::vector<size_t> InputManager::CollectMatchingKeybinds(const std::vector<int>& downKeys, int justPressedKey) const {
    if (keybindManager_ == nullptr) {
        return {};
    }

    const std::vector<size_t> singleMatches = keybindManager_->GetByKey(justPressedKey);
    const std::vector<size_t> multiMatches = keybindManager_->GetByKeys(downKeys, justPressedKey);
    std::vector<size_t> matches = singleMatches;

    for (size_t idx : multiMatches) {
        if (std::find(matches.begin(), matches.end(), idx) == matches.end()) {
            matches.push_back(idx);
        }
    }

    return matches;
}

bool InputManager::IsInputGated() {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.isInputGatedOverlayVisible && g_testingHooks.isInputGatedOverlayVisible()) {
        return true;
    }
#else
    if (overlay_ != nullptr && overlay_->IsVisible()) {
        return true;
    }
#endif

    if (IsImGuiCapturingKeyboard()) {
        return true;
    }

    if (sampBridge_ == nullptr) {
        return false;
    }

    if (sampBridge_->IsChatOpen()) {
        return true;
    }

    if (sampBridge_->IsDialogOpen()) {
        return true;
    }

    return sampBridge_->IsCursorActive();
}

const char* InputManager::GetInputGateReason() const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.isInputGatedOverlayVisible && g_testingHooks.isInputGatedOverlayVisible()) {
        return "overlay visible";
    }
#else
    if (overlay_ != nullptr && overlay_->IsVisible()) {
        return "overlay visible";
    }
#endif

    if (IsImGuiCapturingKeyboard()) {
        return "ImGui keyboard capture";
    }

    if (sampBridge_ == nullptr) {
        return "no SAMP bridge";
    }

    if (sampBridge_->IsChatOpen()) {
        return "SA-MP chat open";
    }

    if (sampBridge_->IsDialogOpen()) {
        return "SA-MP dialog open";
    }

    if (sampBridge_->IsCursorActive()) {
        return "SA-MP cursor active";
    }

    return "no blocker active";
}

bool InputManager::IsImGuiCapturingKeyboard() const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.isImGuiCapturingKeyboard) {
        return g_testingHooks.isImGuiCapturingKeyboard();
    }
    return false;
#else
    if (overlay_ == nullptr || !overlay_->IsVisible() || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard || io.WantTextInput;
#endif
}

bool InputManager::IsKeyDown(unsigned int key) const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.checkKey) {
        return g_testingHooks.checkKey(key);
    }
    return false;
#else
    return KeyCheck::Check(key);
#endif
}

bool InputManager::IsKeyJustDown(unsigned int key) const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.checkJustDown) {
        return g_testingHooks.checkJustDown(key);
    }
    return false;
#else
    return KeyCheck::CheckJustDown(key);
#endif
}

void InputManager::SuppressOverlayMouseInput() const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.suppressOverlayMouseInput) {
        g_testingHooks.suppressOverlayMouseInput();
        return;
    }
#endif

#ifndef KEYBINDER_TESTING
    Logger::Debug("[CURSOR_DEBUG] SuppressOverlayMouseInput: Clearing game mouse/camera state");
    
    auto clearMouseState = [](CMouseControllerState& state) {
        state.lmb = 0;
        state.rmb = 0;
        state.mmb = 0;
        state.wheelUp = 0;
        state.wheelDown = 0;
        state.bmx1 = 0;
        state.bmx2 = 0;
        state.z = 0.0f;
        state.x = 0.0f;
        state.y = 0.0f;
    };

    auto clearCameraLookState = [](CControllerState& state) {
        state.RightStickX = 0;
        state.RightStickY = 0;
        state.m_bVehicleMouseLook = 0;
    };

    clearMouseState(CPad::PCTempMouseControllerState);
    clearMouseState(CPad::NewMouseControllerState);
    clearMouseState(CPad::OldMouseControllerState);

    CPad* pad = CPad::GetPad(0);
    if (pad != nullptr) {
        clearCameraLookState(pad->NewState);
        clearCameraLookState(pad->OldState);
        clearCameraLookState(pad->PCTempKeyState);
        clearCameraLookState(pad->PCTempJoyState);
        clearCameraLookState(pad->PCTempMouseState);
    }

    CPad::ClearMouseHistory();
    
    Logger::Debug("[CURSOR_DEBUG] SuppressOverlayMouseInput: Game mouse state cleared");
#endif
}

void InputManager::SuppressOverlayKeyboardInput() const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.suppressOverlayKeyboardInput) {
        g_testingHooks.suppressOverlayKeyboardInput();
        return;
    }
#endif

#ifndef KEYBINDER_TESTING
    CKeyboardState emptyKeys = {};
    CPad::NewKeyState = emptyKeys;
    CPad::OldKeyState = emptyKeys;

    CPad* pad = CPad::GetPad(0);
    if (pad != nullptr) {
        CControllerState emptyCtrl = {};
        pad->NewState = emptyCtrl;
        pad->OldState = emptyCtrl;
        pad->PCTempKeyState = emptyCtrl;
        pad->PCTempJoyState = emptyCtrl;
        pad->PCTempMouseState = emptyCtrl;
    }
#endif
}

void InputManager::SuppressGameKey(int key) const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.suppressGameKey) {
        g_testingHooks.suppressGameKey(key);
        return;
    }
#endif

#ifndef KEYBINDER_TESTING
    if (key < 0 || key > 255) {
        return;
    }

    CPad::NewKeyState.standardKeys[key] = 0;
    CPad::OldKeyState.standardKeys[key] = 0;

    switch (key) {
    case VK_LBUTTON:
        CPad::NewMouseControllerState.lmb = 0;
        CPad::OldMouseControllerState.lmb = 0;
        break;
    case VK_RBUTTON:
        CPad::NewMouseControllerState.rmb = 0;
        CPad::OldMouseControllerState.rmb = 0;
        break;
    case VK_MBUTTON:
        CPad::NewMouseControllerState.mmb = 0;
        CPad::OldMouseControllerState.mmb = 0;
        break;
    case VK_XBUTTON1:
        CPad::NewMouseControllerState.bmx1 = 0;
        CPad::OldMouseControllerState.bmx1 = 0;
        break;
    case VK_XBUTTON2:
        CPad::NewMouseControllerState.bmx2 = 0;
        CPad::OldMouseControllerState.bmx2 = 0;
        break;
    case VK_SHIFT:
        CPad::NewKeyState.shift = 0;
        CPad::OldKeyState.shift = 0;
        break;
    case VK_LSHIFT:
        CPad::NewKeyState.lshift = 0;
        CPad::OldKeyState.lshift = 0;
        CPad::NewKeyState.shift = 0;
        CPad::OldKeyState.shift = 0;
        break;
    case VK_RSHIFT:
        CPad::NewKeyState.rshift = 0;
        CPad::OldKeyState.rshift = 0;
        CPad::NewKeyState.shift = 0;
        CPad::OldKeyState.shift = 0;
        break;
    case VK_CONTROL:
        CPad::NewKeyState.lctrl = 0;
        CPad::NewKeyState.rctrl = 0;
        CPad::OldKeyState.lctrl = 0;
        CPad::OldKeyState.rctrl = 0;
        break;
    case VK_LCONTROL:
        CPad::NewKeyState.lctrl = 0;
        CPad::OldKeyState.lctrl = 0;
        break;
    case VK_RCONTROL:
        CPad::NewKeyState.rctrl = 0;
        CPad::OldKeyState.rctrl = 0;
        break;
    case VK_MENU:
        CPad::NewKeyState.lmenu = 0;
        CPad::NewKeyState.rmenu = 0;
        CPad::OldKeyState.lmenu = 0;
        CPad::OldKeyState.rmenu = 0;
        break;
    case VK_LMENU:
        CPad::NewKeyState.lmenu = 0;
        CPad::OldKeyState.lmenu = 0;
        break;
    case VK_RMENU:
        CPad::NewKeyState.rmenu = 0;
        CPad::OldKeyState.rmenu = 0;
        break;
    case VK_ESCAPE:
        CPad::NewKeyState.esc = 0;
        CPad::OldKeyState.esc = 0;
        break;
    case VK_TAB:
        CPad::NewKeyState.tab = 0;
        CPad::OldKeyState.tab = 0;
        break;
    case VK_BACK:
        CPad::NewKeyState.back = 0;
        CPad::OldKeyState.back = 0;
        break;
    case VK_RETURN:
        CPad::NewKeyState.enter = 0;
        CPad::OldKeyState.enter = 0;
        CPad::NewKeyState.extenter = 0;
        CPad::OldKeyState.extenter = 0;
        break;
    case VK_DELETE:
        CPad::NewKeyState.del = 0;
        CPad::OldKeyState.del = 0;
        break;
    case VK_INSERT:
        CPad::NewKeyState.insert = 0;
        CPad::OldKeyState.insert = 0;
        break;
    case VK_HOME:
        CPad::NewKeyState.home = 0;
        CPad::OldKeyState.home = 0;
        break;
    case VK_END:
        CPad::NewKeyState.end = 0;
        CPad::OldKeyState.end = 0;
        break;
    case VK_PRIOR:
        CPad::NewKeyState.pgup = 0;
        CPad::OldKeyState.pgup = 0;
        break;
    case VK_NEXT:
        CPad::NewKeyState.pgdn = 0;
        CPad::OldKeyState.pgdn = 0;
        break;
    case VK_UP:
        CPad::NewKeyState.up = 0;
        CPad::OldKeyState.up = 0;
        break;
    case VK_DOWN:
        CPad::NewKeyState.down = 0;
        CPad::OldKeyState.down = 0;
        break;
    case VK_LEFT:
        CPad::NewKeyState.left = 0;
        CPad::OldKeyState.left = 0;
        break;
    case VK_RIGHT:
        CPad::NewKeyState.right = 0;
        CPad::OldKeyState.right = 0;
        break;
    default:
        if (key >= VK_F1 && key <= VK_F12) {
            const int index = key - VK_F1;
            CPad::NewKeyState.FKeys[index] = 0;
            CPad::OldKeyState.FKeys[index] = 0;
        }
        break;
    }
#endif
}

} // namespace keybinder
