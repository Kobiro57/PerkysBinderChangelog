#pragma once

#include "Action.h"

#include <array>
#include <functional>
#include <vector>

class SAMPBridge;

namespace keybinder {

class ActionExecutor;
class ImGuiOverlay;
class KeybindManager;

class InputManager {
public:
    InputManager();

    void Initialize();
    void Update();

    void SetKeybindManager(KeybindManager* manager);
    void SetActionExecutor(ActionExecutor* executor);
    void SetSAMPBridge(SAMPBridge* bridge);
    void SetImGuiOverlay(ImGuiOverlay* overlay);
    void SetQuickMapVisibleCallback(std::function<bool()> callback);
    bool HandleEarlyKeyEvent(int key, bool keyDown);
    void ApplyEarlySuppressedKeys() const;
    void ClearTransientInputState();

#ifdef KEYBINDER_TESTING
    struct TestingHooks {
        std::function<void()> updateKeyStates;
        std::function<bool(unsigned int)> checkKey;
        std::function<bool(unsigned int)> checkJustDown;
        std::function<void(int)> suppressGameKey;
        std::function<void()> suppressOverlayMouseInput;
        std::function<void()> suppressOverlayKeyboardInput;
        std::function<bool()> isImGuiCapturingKeyboard;
        std::function<bool()> isInputGatedOverlayVisible;
    };

    static void SetTestingHooks(const TestingHooks& hooks);
    static void ResetTestingHooks();
#endif

private:
    void PollKeyStates() const;
    void DispatchTriggeredBinds(std::vector<Action>& pendingActions);
    bool IsInputGated();
    const char* GetInputGateReason() const;
    bool IsImGuiCapturingKeyboard() const;
    bool IsKeyDown(unsigned int key) const;
    bool IsKeyJustDown(unsigned int key) const;
    void ReconcileEarlyKeyState();
    std::vector<size_t> CollectMatchingKeybinds(const std::vector<int>& downKeys, int justPressedKey) const;
    void SuppressOverlayMouseInput() const;
    void SuppressOverlayKeyboardInput() const;
    void SuppressGameKey(int key) const;

    bool initialized_;
    KeybindManager* keybindManager_;
    ActionExecutor* actionExecutor_;
    SAMPBridge* sampBridge_;
    ImGuiOverlay* overlay_;
    std::function<bool()> quickMapVisibleCallback_;
    bool lastOverlayVisible_;
    bool lastInputGated_;
    int frameCounter_;
    std::array<bool, 256> earlyHandledKeys_{};
    std::array<bool, 256> earlySuppressedKeys_{};
    std::array<bool, 256> earlyWindowKeyDown_{};
    std::vector<Action> pendingEarlyActions_;
};

} // namespace keybinder
