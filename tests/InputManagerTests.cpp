#define KEYBINDER_TESTING

#include "../include/InputManager.h"
#include "../include/ActionExecutor.h"
#include "../include/KeybindManager.h"
#include "../include/RakHook/include/RakHook/samp.hpp"
#include "../include/SAMPBridge.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace keybinder;

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

TestState g_state;

#define INPUT_ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        g_state.error = "Assertion failed: " #condition; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define INPUT_ASSERT_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        g_state.error = "Assertion failed: " #actual " == " #expected; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define INPUT_RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

struct MockBridge : public SAMPBridgeInterface {
    mutable std::vector<std::string> chats;
    mutable std::vector<std::string> commands;

    bool SendChat(const std::string& text) const override {
        chats.push_back(text);
        return true;
    }

    bool SendCommand(const std::string& cmd) const override {
        commands.push_back(cmd);
        return true;
    }
};

struct MockContext {
    std::aligned_storage_t<sizeof(sampapi::v03dl::CInput), alignof(sampapi::v03dl::CInput)> inputStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CDialog), alignof(sampapi::v03dl::CDialog)> dialogStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CGame), alignof(sampapi::v03dl::CGame)> gameStorage{};

    sampapi::v03dl::CInput* input = nullptr;
    sampapi::v03dl::CDialog* dialog = nullptr;
    sampapi::v03dl::CGame* game = nullptr;

    MockContext() {
        input = reinterpret_cast<sampapi::v03dl::CInput*>(&inputStorage);
        dialog = reinterpret_cast<sampapi::v03dl::CDialog*>(&dialogStorage);
        game = reinterpret_cast<sampapi::v03dl::CGame*>(&gameStorage);
        std::memset(input, 0, sizeof(*input));
        std::memset(dialog, 0, sizeof(*dialog));
        std::memset(game, 0, sizeof(*game));
    }

    SAMPBridge::TestingHooks BuildHooks() {
        return SAMPBridge::TestingHooks{
            [](const char*) { return reinterpret_cast<HMODULE>(0x1); },
            []() -> uintptr_t { return 1; },
            []() { return rakhook::samp_ver::v03dlr1; },
            []() { return true; },
            []() {},
            [this]() { return input; },
            []() -> sampapi::v03dl::CChat* { return nullptr; },
            [this]() { return dialog; },
            [this]() { return game; },
            []() -> sampapi::v03dl::CNetGame* { return nullptr; },
            []() -> sampapi::v03dl::CLocalPlayer* { return nullptr; },
            [](sampapi::v03dl::CInput*, const char*) {},
            [](sampapi::v03dl::CLocalPlayer*, const char*) {},
            [](sampapi::v03dl::CChat*, unsigned long, const char*) {},
            [](sampapi::v03dl::CInput*, const char*) -> sampapi::CMDPROC { return nullptr; },
            [](sampapi::v03dl::CInput*, const char*, sampapi::CMDPROC) {},
            [](sampapi::v03dl::CInput*, sampapi::CMDPROC) {},
            [](const std::function<void(sampapi::v03dl::CChat*, const char*, unsigned long, const char*)>&) {},
            [](const std::string&) {},
        };
    }
};

struct KeyFrameState {
    std::set<unsigned int> down;
    std::set<unsigned int> justDown;
};

struct InputTestContext {
    MockContext sampContext;
    SAMPBridge bridge;
    KeybindManager manager{"test_input_manager.json"};
    ActionExecutor executor;
    MockBridge actionBridge;
    InputManager input;
    std::vector<int> suppressedKeys;
    int overlayMouseSuppressCalls = 0;
    int overlayKeyboardSuppressCalls = 0;
    KeyFrameState frame;
    bool imguiCapture = false;
    bool overlayVisible = false;

    InputTestContext() {
        SAMPBridge::SetTestingHooks(sampContext.BuildHooks());
        bridge.Initialize();
        executor.SetSAMPBridge(&actionBridge);
        input.SetKeybindManager(&manager);
        input.SetActionExecutor(&executor);
        input.SetSAMPBridge(&bridge);
        input.SetImGuiOverlay(nullptr);
        InputManager::SetTestingHooks({
            []() {},
            [this](unsigned int key) { return frame.down.count(key) != 0; },
            [this](unsigned int key) { return frame.justDown.count(key) != 0; },
            [this](int key) { suppressedKeys.push_back(key); },
            [this]() { ++overlayMouseSuppressCalls; },
            [this]() { ++overlayKeyboardSuppressCalls; },
            [this]() { return imguiCapture; },
            [this]() {
                return overlayVisible
                    || sampContext.input->m_bEnabled != FALSE
                    || sampContext.dialog->m_bIsActive != FALSE
                    || sampContext.game->m_nCursorMode != sampapi::v03dl::CURSOR_NONE;
            },
        });
        input.Initialize();
    }

    ~InputTestContext() {
        InputManager::ResetTestingHooks();
        bridge.Shutdown();
        SAMPBridge::ResetTestingHooks();
        std::remove("test_input_manager.json");
    }

    void SetFrame(std::initializer_list<unsigned int> downKeys, std::initializer_list<unsigned int> justDownKeys) {
        frame.down.clear();
        frame.justDown.clear();
        frame.down.insert(downKeys.begin(), downKeys.end());
        frame.justDown.insert(justDownKeys.begin(), justDownKeys.end());
    }

    void FlushExecutor() {
        while (executor.IsExecuting()) {
            executor.Update();
        }
    }

    void CreateImGuiCapture(bool wantCaptureKeyboard) {
        imguiCapture = wantCaptureKeyboard;
    }

    void SetOverlayVisible(bool visible) {
        overlayVisible = visible;
    }
};

bool TestCompositeKeyDetectionAndEdgeDispatch() {
    InputTestContext context;
    context.manager.Add(Keybind("Composite", 'F', "Test", true, false,
        {Action(ActionType::Chat, "composite") }));

    context.SetFrame({VK_CONTROL, VK_LCONTROL, VK_SHIFT, VK_LSHIFT, 'F'}, { 'F' });
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("composite"));

    context.SetFrame({VK_CONTROL, VK_LCONTROL, VK_SHIFT, VK_LSHIFT, 'F'}, {});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));

    context.SetFrame({VK_CONTROL, VK_LCONTROL, VK_SHIFT, VK_LSHIFT, VK_MENU, VK_LMENU, 'F'}, { 'F' });
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(2));
    INPUT_ASSERT_EQ(context.actionBridge.chats[1], std::string("composite"));
    return true;
}

bool TestInputGatingBehavior() {
    InputTestContext context;
    context.manager.Add(Keybind("Gate", 'G', "Test", true, false,
        {Action(ActionType::Chat, "gate") }));

    context.CreateImGuiCapture(true);
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_TRUE(context.actionBridge.chats.empty());

    context.imguiCapture = false;
    context.sampContext.input->m_bEnabled = TRUE;
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_TRUE(context.actionBridge.chats.empty());

    context.sampContext.input->m_bEnabled = FALSE;
    context.sampContext.dialog->m_bIsActive = TRUE;
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_TRUE(context.actionBridge.chats.empty());

    context.sampContext.dialog->m_bIsActive = FALSE;
    context.sampContext.game->m_nCursorMode = sampapi::v03dl::CURSOR_LOCKCAM;
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_TRUE(context.actionBridge.chats.empty());

    context.sampContext.game->m_nCursorMode = 0;
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    return true;
}

bool TestPassThroughBehavior() {
    InputTestContext context;
    context.manager.Add(Keybind("Block", 'H', "Test", true, false,
        {Action(ActionType::Chat, "block") }));

    context.SetFrame({'H'}, {'H'});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_TRUE(!context.suppressedKeys.empty());
    INPUT_ASSERT_EQ(context.suppressedKeys.front(), static_cast<int>('H'));

    context.suppressedKeys.clear();
    context.manager.GetAll().clear();
    context.manager.Add(Keybind("Allow", 'J', "Test", true, true,
        {Action(ActionType::Chat, "allow") }));

    context.SetFrame({'J'}, {'J'});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(2));
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());
    return true;
}

bool TestBindDispatch() {
    InputTestContext context;
    context.manager.Add(Keybind("Disabled", 'K', "Test", false, false,
        {Action(ActionType::Chat, "disabled") }));
    context.manager.Add(Keybind("Dispatch", 'K', "Test", true, false,
        {Action(ActionType::Chat, "dispatch") , Action(ActionType::Command, "/wave") }));

    context.SetFrame({'K'}, {'K'});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("dispatch"));
    INPUT_ASSERT_EQ(context.actionBridge.commands.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.commands[0], std::string("/wave"));
    return true;
}

bool TestEarlyKeyHandlingSuppressesAndAvoidsDuplicateDispatch() {
    InputTestContext context;
    context.manager.Add(Keybind("EarlyBlock", 'H', "Test", true, false,
        {Action(ActionType::Chat, "early_block") }));

    INPUT_ASSERT_TRUE(context.input.HandleEarlyKeyEvent('H', true));

    context.input.ApplyEarlySuppressedKeys();
    INPUT_ASSERT_EQ(context.suppressedKeys.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.suppressedKeys[0], static_cast<int>('H'));

    context.SetFrame({'H'}, {'H'});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("early_block"));

    context.input.HandleEarlyKeyEvent('H', false);
    return true;
}

bool TestEarlyKeyHandlingHonorsPassthrough() {
    InputTestContext context;
    context.manager.Add(Keybind("EarlyAllow", 'J', "Test", true, true,
        {Action(ActionType::Chat, "early_allow") }));

    INPUT_ASSERT_TRUE(!context.input.HandleEarlyKeyEvent('J', true));

    context.input.ApplyEarlySuppressedKeys();
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());

    context.SetFrame({'J'}, {'J'});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("early_allow"));

    context.input.HandleEarlyKeyEvent('J', false);
    return true;
}

bool TestEarlyKeyHandlingSuppressesKeyboardMouseCombo() {
    InputTestContext context;
    Keybind combo("EarlyCombo", 'F', "Test", true, false,
        {Action(ActionType::Chat, "early_combo") });
    combo.keys = { 'F', VK_XBUTTON2 };
    context.manager.Add(combo);

    INPUT_ASSERT_TRUE(!context.input.HandleEarlyKeyEvent('F', true));
    INPUT_ASSERT_TRUE(context.input.HandleEarlyKeyEvent(VK_XBUTTON2, true));

    context.input.ApplyEarlySuppressedKeys();
    INPUT_ASSERT_EQ(context.suppressedKeys.size(), static_cast<size_t>(2));
    INPUT_ASSERT_TRUE(std::find(context.suppressedKeys.begin(), context.suppressedKeys.end(), static_cast<int>('F')) != context.suppressedKeys.end());
    INPUT_ASSERT_TRUE(std::find(context.suppressedKeys.begin(), context.suppressedKeys.end(), static_cast<int>(VK_XBUTTON2)) != context.suppressedKeys.end());

    context.SetFrame({static_cast<unsigned int>('F'), static_cast<unsigned int>(VK_XBUTTON2)}, {static_cast<unsigned int>(VK_XBUTTON2)});
    context.input.Update();
    context.FlushExecutor();

    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("early_combo"));

    context.input.HandleEarlyKeyEvent(VK_XBUTTON2, false);
    context.input.HandleEarlyKeyEvent('F', false);
    return true;
}

bool TestFocusLossClearsTransientEarlyInputState() {
    InputTestContext context;
    context.manager.Add(Keybind("EarlyFocusLoss", 'H', "Test", true, false,
        {Action(ActionType::Chat, "focus_loss") }));

    INPUT_ASSERT_TRUE(context.input.HandleEarlyKeyEvent('H', true));

    context.input.ClearTransientInputState();
    context.input.ApplyEarlySuppressedKeys();
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());

    context.SetFrame({}, {});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_TRUE(context.actionBridge.chats.empty());

    context.SetFrame({'H'}, {'H'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("focus_loss"));
    return true;
}


bool TestOverlayVisibleGatesInput() {
    InputTestContext context;
    context.manager.Add(Keybind("OverlayGate", 'G', "Test", true, false,
        {Action(ActionType::Chat, "overlay_gate") }));

    context.SetOverlayVisible(true);
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_TRUE(context.actionBridge.chats.empty());
    INPUT_ASSERT_EQ(context.overlayMouseSuppressCalls, 1);

    context.SetOverlayVisible(false);
    context.SetFrame({'G'}, {'G'});
    context.input.Update();
    context.FlushExecutor();
    INPUT_ASSERT_EQ(context.actionBridge.chats.size(), static_cast<size_t>(1));
    INPUT_ASSERT_EQ(context.actionBridge.chats[0], std::string("overlay_gate"));
    INPUT_ASSERT_EQ(context.overlayMouseSuppressCalls, 1);
    return true;
}

bool TestOverlayKeyboardSuppressionFollowsImGuiCapture() {
    InputTestContext context;

    context.SetOverlayVisible(true);
    context.SetFrame({'W', VK_SHIFT}, {'W'});
    context.input.Update();
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());
    INPUT_ASSERT_EQ(context.overlayMouseSuppressCalls, 1);
    INPUT_ASSERT_EQ(context.overlayKeyboardSuppressCalls, 0);

    context.input.Update();
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());
    INPUT_ASSERT_EQ(context.overlayMouseSuppressCalls, 2);
    INPUT_ASSERT_EQ(context.overlayKeyboardSuppressCalls, 0);

    context.imguiCapture = true;
    context.input.Update();
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());
    INPUT_ASSERT_EQ(context.overlayMouseSuppressCalls, 3);
    INPUT_ASSERT_EQ(context.overlayKeyboardSuppressCalls, 1);

    context.SetOverlayVisible(false);
    context.input.Update();
    INPUT_ASSERT_TRUE(context.suppressedKeys.empty());
    INPUT_ASSERT_EQ(context.overlayMouseSuppressCalls, 3);
    INPUT_ASSERT_EQ(context.overlayKeyboardSuppressCalls, 1);
    return true;
}

} // namespace

int RunInputManagerTests() {
    std::printf("\n=== InputManager Tests ===\n\n");

    INPUT_RUN_TEST(TestCompositeKeyDetectionAndEdgeDispatch);
    INPUT_RUN_TEST(TestInputGatingBehavior);
    INPUT_RUN_TEST(TestPassThroughBehavior);
    INPUT_RUN_TEST(TestBindDispatch);
    INPUT_RUN_TEST(TestEarlyKeyHandlingSuppressesAndAvoidsDuplicateDispatch);
    INPUT_RUN_TEST(TestEarlyKeyHandlingHonorsPassthrough);
    INPUT_RUN_TEST(TestEarlyKeyHandlingSuppressesKeyboardMouseCombo);
    INPUT_RUN_TEST(TestFocusLossClearsTransientEarlyInputState);
    INPUT_RUN_TEST(TestOverlayVisibleGatesInput);
    INPUT_RUN_TEST(TestOverlayKeyboardSuppressionFollowsImGuiCapture);

    std::printf("\n=== InputManager Results: %d passed, %d failed ===\n\n", g_state.passed, g_state.failed);
    return g_state.failed;
}

#ifdef INPUT_MANAGER_TEST_STANDALONE
int main() {
    return RunInputManagerTests();
}
#endif
