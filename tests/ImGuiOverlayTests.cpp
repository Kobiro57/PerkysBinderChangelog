#include "ImGuiOverlay.h"
#include "PersistentPaths.h"
#include "SAMPBridge.h"
#include "SettingsManager.h"

#include "imgui.h"
#include "../include/RakHook/include/RakHook/samp.hpp"
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

extern std::atomic<int> g_shutdown_phase;

namespace {

struct OverlayTestState {
    int win32_init_calls = 0;
    int dx9_init_calls = 0;
    int win32_shutdown_calls = 0;
    int dx9_shutdown_calls = 0;
    int win32_new_frame_calls = 0;
    int dx9_new_frame_calls = 0;
    int render_draw_calls = 0;
    int invalidate_calls = 0;
    int create_calls = 0;
    int install_calls = 0;
    int remove_calls = 0;
    int shutdown_remove_calls = 0;
    int show_cursor_calls = 0;
    int hide_cursor_calls = 0;
    int suppress_game_mouse_calls = 0;
    bool cursor_visible = false;
};

OverlayTestState g_state;

struct MockSampContext {
    std::aligned_storage_t<sizeof(sampapi::v03dl::CInput), alignof(sampapi::v03dl::CInput)> inputStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CDialog), alignof(sampapi::v03dl::CDialog)> dialogStorage{};
    std::aligned_storage_t<sizeof(sampapi::v03dl::CGame), alignof(sampapi::v03dl::CGame)> gameStorage{};

    sampapi::v03dl::CInput* input = nullptr;
    sampapi::v03dl::CDialog* dialog = nullptr;
    sampapi::v03dl::CGame* game = nullptr;

    MockSampContext() {
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

void ResetState() {
    g_state = {};
    g_shutdown_phase.store(0);
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }
}

IDirect3DDevice9* FakeDiscoverDevice() {
    return reinterpret_cast<IDirect3DDevice9*>(0x1000);
}

HWND FakeDiscoverWindow() {
    return reinterpret_cast<HWND>(0x2000);
}

bool FakeWin32Init(void*) {
    ++g_state.win32_init_calls;
    return true;
}

bool FakeDx9Init(IDirect3DDevice9*) {
    ++g_state.dx9_init_calls;
    return true;
}

void FakeWin32Shutdown() {
    ++g_state.win32_shutdown_calls;
}

void FakeDx9Shutdown() {
    ++g_state.dx9_shutdown_calls;
}

void FakeWin32NewFrame() {
    ++g_state.win32_new_frame_calls;
}

void FakeDx9NewFrame() {
    ++g_state.dx9_new_frame_calls;
}

void FakeRenderDrawData(void*) {
    ++g_state.render_draw_calls;
}

void FakeInvalidateDeviceObjects() {
    ++g_state.invalidate_calls;
}

bool FakeCreateDeviceObjects() {
    ++g_state.create_calls;
    return true;
}

LRESULT FakeWndProcHandler(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}

LRESULT CALLBACK FakeOriginalWndProc(HWND, UINT, WPARAM, LPARAM) {
    return 0;
}

LRESULT CALLBACK FakeOriginalWndProcWithSentinel(HWND, UINT msg, WPARAM, LPARAM) {
    if (msg == WM_INPUT) {
        return 77;
    }
    if (msg == WM_MOUSEMOVE) {
        return 66;
    }
    return 0;
}

WNDPROC FakeInstallWndProcHook(HWND, WNDPROC) {
    ++g_state.install_calls;
    return &FakeOriginalWndProc;
}

bool FakeRemoveWndProcHook(HWND, WNDPROC, WNDPROC) {
    ++g_state.remove_calls;
    return true;
}

bool FakeRemoveWndProcHookOnShutdown(HWND, WNDPROC, WNDPROC) {
    ++g_state.shutdown_remove_calls;
    return true;
}

LRESULT FakeCallWindowProc(WNDPROC proc, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    return proc != nullptr ? proc(hwnd, msg, w_param, l_param) : 0;
}

WNDPROC FakeInstallWndProcHookWithSentinel(HWND, WNDPROC) {
    ++g_state.install_calls;
    return &FakeOriginalWndProcWithSentinel;
}

void FakeShowCursor(bool visible) {
    if (visible) {
        ++g_state.show_cursor_calls;
    } else {
        ++g_state.hide_cursor_calls;
    }
    g_state.cursor_visible = visible;
}

void FakeSuppressGameMouse() {
    ++g_state.suppress_game_mouse_calls;
}

keybinder::ImGuiOverlayDependencies MakeDependencies() {
    return {
        &FakeDiscoverDevice,
        &FakeDiscoverWindow,
        &FakeWin32Init,
        &FakeDx9Init,
        &FakeWin32Shutdown,
        &FakeDx9Shutdown,
        &FakeWin32NewFrame,
        &FakeDx9NewFrame,
        &FakeRenderDrawData,
        &FakeInvalidateDeviceObjects,
        &FakeCreateDeviceObjects,
        &FakeWndProcHandler,
        &FakeInstallWndProcHook,
        &FakeRemoveWndProcHook,
        &FakeRemoveWndProcHookOnShutdown,
        &FakeCallWindowProc,
        &FakeShowCursor,
        &FakeSuppressGameMouse,
    };
}

keybinder::ImGuiOverlayDependencies MakeSentinelDependencies() {
    return {
        &FakeDiscoverDevice,
        &FakeDiscoverWindow,
        &FakeWin32Init,
        &FakeDx9Init,
        &FakeWin32Shutdown,
        &FakeDx9Shutdown,
        &FakeWin32NewFrame,
        &FakeDx9NewFrame,
        &FakeRenderDrawData,
        &FakeInvalidateDeviceObjects,
        &FakeCreateDeviceObjects,
        &FakeWndProcHandler,
        &FakeInstallWndProcHookWithSentinel,
        &FakeRemoveWndProcHook,
        &FakeRemoveWndProcHookOnShutdown,
        &FakeCallWindowProc,
        &FakeShowCursor,
        &FakeSuppressGameMouse,
    };
}

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[FAIL] %s\n", message);
        return false;
    }
    return true;
}

bool Approximately(float actual, float expected, float tolerance) {
    return std::fabs(actual - expected) <= tolerance;
}

bool TestAutomaticScaleKeepsRelativeFootprintAcrossStandardResolutions() {
    std::printf("Running test...\n"); fflush(stdout);
    const float fullHdScale = keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(1920.0f, 1080.0f);
    const float qhdScale = keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(2560.0f, 1440.0f);
    const float uhdScale = keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(3840.0f, 2160.0f);

    if (!Expect(Approximately(fullHdScale, 0.7071f, 0.0010f), "Full HD automatic scale should be approximately 0.707")) return false;
    if (!Expect(Approximately(qhdScale, 0.9428f, 0.0010f), "1440p automatic scale should be approximately 0.943")) return false;
    if (!Expect(Approximately(uhdScale, 1.4142f, 0.0010f), "4K automatic scale should remain approximately 1.414")) return false;

    const float fullHdRatio = fullHdScale / 1.0f;
    const float qhdRatio = qhdScale / (2560.0f / 1920.0f);
    const float uhdRatio = uhdScale / (3840.0f / 1920.0f);
    if (!Expect(Approximately(qhdRatio, fullHdRatio, 0.0010f), "1440p scale should preserve the Full HD relative footprint ratio")) return false;
    if (!Expect(Approximately(uhdRatio, fullHdRatio, 0.0010f), "4K scale should preserve the Full HD relative footprint ratio")) return false;

    return true;
}

bool TestAutomaticScaleClampsSmallAndInvalidDisplays() {
    std::printf("Running test...\n"); fflush(stdout);
    if (!Expect(Approximately(keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(1280.0f, 720.0f), 0.70f, 0.0001f),
                "Small displays should clamp to the minimum readable UI scale")) return false;
    if (!Expect(Approximately(keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(0.0f, 1080.0f), 1.0f, 0.0001f),
                "Zero width should fall back to 1.0")) return false;
    if (!Expect(Approximately(keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(1920.0f, 0.0f), 1.0f, 0.0001f),
                "Zero height should fall back to 1.0")) return false;
    if (!Expect(Approximately(keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(-1920.0f, 1080.0f), 1.0f, 0.0001f),
                "Negative width should fall back to 1.0")) return false;
    if (!Expect(Approximately(keybinder::ImGuiOverlay::ComputeAutomaticScaleForTesting(1920.0f, -1080.0f), 1.0f, 0.0001f),
                "Negative height should fall back to 1.0")) return false;

    return true;
}

bool TestInitializationAndCleanup() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;
    if (!Expect(overlay.Initialize(), "Initialize should stay idempotent")) return false;
    if (!Expect(g_state.win32_init_calls == 1, "Win32 backend should initialize once")) return false;
    if (!Expect(g_state.dx9_init_calls == 1, "DX9 backend should initialize once")) return false;
    if (!Expect(g_state.install_calls == 1, "WndProc hook should install once")) return false;

    overlay.Shutdown();

    if (!Expect(g_state.remove_calls == 0, "Runtime remove hook should not be used during shutdown")) return false;
    if (!Expect(g_state.shutdown_remove_calls == 1, "Shutdown should use the dedicated WndProc cleanup path")) return false;
    if (!Expect(g_state.win32_shutdown_calls == 1, "Win32 backend should shut down once")) return false;
    if (!Expect(g_state.dx9_shutdown_calls == 1, "DX9 backend should shut down once")) return false;
    if (!Expect(ImGui::GetCurrentContext() == nullptr, "Overlay should destroy owned ImGui context")) return false;
    return true;
}

bool TestInitializationUsesPersistentImGuiIniPath() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const char* iniFilename = ImGui::GetIO().IniFilename;
    const std::string expectedPath = keybinder::PersistentPaths::ImGuiIniFile();
    if (!Expect(iniFilename != nullptr, "Initialize should configure an ImGui ini filename")) return false;
    if (!Expect(!expectedPath.empty(), "Persistent imgui.ini path should resolve")) return false;
    if (!Expect(expectedPath == iniFilename, "ImGui should persist layout under the NLBinder directory")) return false;

    overlay.Shutdown();
    return true;
}

bool TestDeviceLostAndResetHandling() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed before device events")) return false;

    overlay.OnDeviceLost();
    overlay.OnDeviceLost();
    if (!Expect(g_state.invalidate_calls == 1, "Device objects should invalidate once per lost cycle")) return false;

    overlay.OnDeviceReset();
    overlay.OnDeviceReset();
    if (!Expect(g_state.create_calls == 1, "Device objects should recreate once per reset cycle")) return false;

    overlay.Shutdown();
    return true;
}

bool TestToggleFunctionality() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(!overlay.IsVisible(), "Overlay should start hidden")) return false;

    overlay.Toggle();
    if (!Expect(overlay.IsVisible(), "Toggle should show overlay")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(!overlay.IsVisible(), "F2 key should hide overlay")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(overlay.IsVisible(), "F2 key should show overlay")) return false;

    overlay.Shutdown();
    return true;
}

bool TestCursorManagement() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.Show();
    if (!Expect(g_state.show_cursor_calls == 1, "Show should call show_cursor(true) once")) return false;
    if (!Expect(g_state.cursor_visible, "Cursor should be visible after Show")) return false;
    if (!Expect(ImGui::GetCurrentContext() != nullptr, "Show should initialize ImGui context")) return false;
    if (!Expect(ImGui::GetIO().MouseDrawCursor, "Show should enable ImGui software cursor")) return false;

    overlay.Show();
    if (!Expect(g_state.show_cursor_calls == 1, "Redundant Show should not call show_cursor again")) return false;

    overlay.Hide();
    if (!Expect(g_state.hide_cursor_calls == 1, "Hide should call show_cursor(false) once")) return false;
    if (!Expect(!g_state.cursor_visible, "Cursor should be hidden after Hide")) return false;
    if (!Expect(!ImGui::GetIO().MouseDrawCursor, "Hide should disable ImGui software cursor")) return false;

    overlay.Hide();
    if (!Expect(g_state.hide_cursor_calls == 1, "Redundant Hide should not call show_cursor again")) return false;

    overlay.Toggle();
    if (!Expect(g_state.show_cursor_calls == 2, "Toggle to visible should call show_cursor(true)")) return false;
    if (!Expect(g_state.cursor_visible, "Cursor should be visible after Toggle on")) return false;
    if (!Expect(ImGui::GetIO().MouseDrawCursor, "Toggle on should enable ImGui software cursor")) return false;

    overlay.Toggle();
    if (!Expect(g_state.hide_cursor_calls == 2, "Toggle to hidden should call show_cursor(false)")) return false;
    if (!Expect(!g_state.cursor_visible, "Cursor should be hidden after Toggle off")) return false;
    if (!Expect(!ImGui::GetIO().MouseDrawCursor, "Toggle off should disable ImGui software cursor")) return false;

    overlay.Shutdown();
    return true;
}

bool TestCursorCleanupOnShutdown() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.Show();
    if (!Expect(overlay.IsVisible(), "Overlay should be visible after Show")) return false;
    if (!Expect(g_state.cursor_visible, "Cursor should be visible after Show")) return false;

    int hide_before = g_state.hide_cursor_calls;
    overlay.Shutdown();

    if (!Expect(g_state.hide_cursor_calls == hide_before + 1, "Shutdown should hide cursor if it was shown")) return false;
    if (!Expect(!g_state.cursor_visible, "Cursor should be hidden after Shutdown")) return false;
    if (ImGui::GetCurrentContext() != nullptr && !Expect(!ImGui::GetIO().MouseDrawCursor, "Shutdown should disable ImGui software cursor")) return false;

    return true;
}

bool TestShutdownNoCursorCleanupWhenHidden() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    int hide_before = g_state.hide_cursor_calls;
    overlay.Shutdown();

    if (!Expect(g_state.hide_cursor_calls == hide_before, "Shutdown should not hide cursor if it was never shown")) return false;

    return true;
}

bool TestF2ToggleCursorLifecycle() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(overlay.IsVisible(), "F2 should show overlay")) return false;
    if (!Expect(g_state.show_cursor_calls == 1, "F2 show should call show_cursor(true)")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(!overlay.IsVisible(), "F2 should hide overlay")) return false;
    if (!Expect(g_state.hide_cursor_calls == 1, "F2 hide should call show_cursor(false)")) return false;

    overlay.Shutdown();
    return true;
}

bool TestConfiguredHotkeyToggle() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().ui_toggle_hotkey = 'K';
    overlay.SetSettingsManager(&settingsManager);

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(!overlay.IsVisible(), "Old F2 hotkey should not toggle after reconfiguration")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'K', 0);
    if (!Expect(overlay.IsVisible(), "Configured K hotkey should show overlay")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'K', 0);
    if (!Expect(!overlay.IsVisible(), "Configured K hotkey should hide overlay")) return false;

    overlay.Shutdown();
    return true;
}

bool TestConfiguredHotkeyIgnoredWhileChatOpen() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    MockSampContext sampContext;
    SAMPBridge::SetTestingHooks(sampContext.BuildHooks());

    SAMPBridge bridge;
    if (!Expect(bridge.Initialize(), "SAMPBridge should initialize for chat gating test")) return false;

    keybinder::ImGuiOverlay overlay(MakeDependencies());
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().ui_toggle_hotkey = 'X';
    overlay.SetSettingsManager(&settingsManager);
    overlay.SetSAMPBridge(&bridge);

    sampContext.input->m_bEnabled = TRUE;
    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'X', 0);
    if (!Expect(!overlay.IsVisible(), "Configured hotkey should not open overlay while chat input is active")) return false;

    sampContext.input->m_bEnabled = FALSE;
    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'X', 0);
    if (!Expect(overlay.IsVisible(), "Configured hotkey should open overlay once chat input is closed")) return false;

    overlay.Shutdown();
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestConfiguredHotkeyReleaseIgnoredAfterChatKeyDown() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    MockSampContext sampContext;
    SAMPBridge::SetTestingHooks(sampContext.BuildHooks());

    SAMPBridge bridge;
    if (!Expect(bridge.Initialize(), "SAMPBridge should initialize for chat key release gating test")) return false;

    keybinder::ImGuiOverlay overlay(MakeDependencies());
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().ui_toggle_hotkey = 'Y';
    overlay.SetSettingsManager(&settingsManager);
    overlay.SetSAMPBridge(&bridge);

    sampContext.input->m_bEnabled = TRUE;
    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYDOWN, 'Y', 0);

    sampContext.input->m_bEnabled = FALSE;
    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'Y', 0);
    if (!Expect(!overlay.IsVisible(), "Configured hotkey release should stay ignored after it began in chat input")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYDOWN, 'Y', 0);
    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'Y', 0);
    if (!Expect(overlay.IsVisible(), "Configured hotkey should toggle normally after chat input is closed")) return false;

    overlay.Shutdown();
    bridge.Shutdown();
    SAMPBridge::ResetTestingHooks();
    return true;
}

bool TestFocusLossInvokesTransientStateHandler() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    int focusLossCalls = 0;
    overlay.SetFocusLostHandler([&]() {
        ++focusLossCalls;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KILLFOCUS, 0, 0);
    if (!Expect(focusLossCalls == 1, "WM_KILLFOCUS should clear transient input state")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_SIZE, SIZE_MINIMIZED, 0);
    if (!Expect(focusLossCalls == 2, "SIZE_MINIMIZED should clear transient input state")) return false;

    overlay.Shutdown();
    return true;
}


bool TestHiddenOverlayDelegatesEarlyKeybindHandling() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    int callback_calls = 0;
    int callback_key = 0;
    bool callback_down = false;

    overlay.SetEarlyKeybindHandler([&](int key, bool keyDown) {
        ++callback_calls;
        callback_key = key;
        callback_down = keyDown;
        return key == 'P' && keyDown;
    });

    const LRESULT consumed = overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYDOWN, 'P', 0);
    if (!Expect(consumed == 1, "Hidden overlay should swallow key when early handler consumes it")) return false;
    if (!Expect(callback_calls == 1, "Early key handler should run for hidden overlay keydown")) return false;
    if (!Expect(callback_key == 'P' && callback_down, "Early key handler should receive correct keydown payload")) return false;

    const LRESULT passed = overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYDOWN, 'Q', 0);
    if (!Expect(passed == 0, "Hidden overlay should pass through key when early handler declines it")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, 'P', 0);
    if (!Expect(callback_calls == 3, "Early key handler should receive both keydown calls and keyup release")) return false;
    if (!Expect(callback_key == 'P' && !callback_down, "Early key handler should receive release payload")) return false;

    overlay.Shutdown();
    return true;
}

bool TestHiddenOverlayDelegatesEarlyMouseKeybindHandling() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    int callback_calls = 0;
    int callback_key = 0;
    bool callback_down = false;

    overlay.SetEarlyKeybindHandler([&](int key, bool keyDown) {
        ++callback_calls;
        callback_key = key;
        callback_down = keyDown;
        return key == VK_XBUTTON2 && keyDown;
    });

    const WPARAM xbutton2 = MAKEWPARAM(0, XBUTTON2);
    const LRESULT consumed = overlay.WndProcHandler(FakeDiscoverWindow(), WM_XBUTTONDOWN, xbutton2, 0);
    if (!Expect(consumed == 1, "Hidden overlay should swallow XBUTTON2 when early handler consumes it")) return false;
    if (!Expect(callback_calls == 1, "Early mouse handler should run for hidden overlay xbutton down")) return false;
    if (!Expect(callback_key == VK_XBUTTON2 && callback_down, "Early mouse handler should receive VK_XBUTTON2 down")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_XBUTTONUP, xbutton2, 0);
    if (!Expect(callback_calls == 2, "Early mouse handler should receive xbutton release")) return false;
    if (!Expect(callback_key == VK_XBUTTON2 && !callback_down, "Early mouse handler should receive VK_XBUTTON2 release")) return false;

    overlay.Shutdown();
    return true;
}

bool TestOverlayVisibleCapturesMouseButNotGameplayKeyboardByDefault() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    overlay.Show();
    if (!Expect(overlay.IsVisible(), "Overlay should be visible")) return false;
    
    HWND hwnd = FakeDiscoverWindow();

    ImGui::GetIO().WantCaptureKeyboard = false;
    ImGui::GetIO().WantTextInput = false;

    LRESULT result_keydown = overlay.WndProcHandler(hwnd, WM_KEYDOWN, 0x41, 0);
    if (!Expect(result_keydown == 0, "Visible overlay should pass gameplay WM_KEYDOWN unless ImGui requests keyboard")) return false;

    LRESULT result_mousemove = overlay.WndProcHandler(hwnd, WM_MOUSEMOVE, 0, 0);
    if (!Expect(result_mousemove != 0, "Visible overlay should still capture WM_MOUSEMOVE")) return false;

    LRESULT result_lbuttondown = overlay.WndProcHandler(hwnd, WM_LBUTTONDOWN, 0, 0);
    if (!Expect(result_lbuttondown != 0, "Visible overlay should still capture WM_LBUTTONDOWN")) return false;

    ImGui::GetIO().WantCaptureKeyboard = true;
    LRESULT captured_keydown = overlay.WndProcHandler(hwnd, WM_KEYDOWN, 0x42, 0);
    if (!Expect(captured_keydown != 0, "Visible overlay should capture WM_KEYDOWN when ImGui requests keyboard")) return false;

    ImGui::GetIO().WantCaptureKeyboard = false;
    ImGui::GetIO().WantTextInput = true;
    LRESULT text_input_keydown = overlay.WndProcHandler(hwnd, WM_KEYDOWN, 0x43, 0);
    if (!Expect(text_input_keydown != 0, "Visible overlay should capture WM_KEYDOWN when ImGui requests text input")) return false;

    overlay.Shutdown();
    return true;
}

bool TestFirstOpenCapturesOverlayMouseImmediately() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.Show();

    if (!Expect(overlay.IsVisible(), "Overlay should be visible after Show")) return false;
    if (!Expect(g_state.win32_init_calls == 1, "Show should synchronously initialize Win32 backend")) return false;
    if (!Expect(g_state.dx9_init_calls == 1, "Show should synchronously initialize DX9 backend")) return false;
    if (!Expect(g_state.install_calls == 1, "Show should synchronously install WndProc hook")) return false;
    if (!Expect(ImGui::GetIO().MouseDrawCursor, "First-open Show should enable ImGui software cursor immediately")) return false;

    HWND hwnd = FakeDiscoverWindow();

    LRESULT result_mousemove = overlay.WndProcHandler(hwnd, WM_MOUSEMOVE, 0, 0);
    if (!Expect(result_mousemove != 0, "First-open overlay should capture WM_MOUSEMOVE immediately")) return false;

    overlay.Shutdown();
    return true;
}

bool TestOverlayHiddenPassesInput() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    if (!Expect(!overlay.IsVisible(), "Overlay should start hidden")) return false;

    HWND hwnd = FakeDiscoverWindow();

    LRESULT result_keydown = overlay.WndProcHandler(hwnd, WM_KEYDOWN, 0x41, 0);
    if (!Expect(result_keydown == 0, "Hidden overlay should pass WM_KEYDOWN to original WndProc")) return false;

    LRESULT result_mousemove = overlay.WndProcHandler(hwnd, WM_MOUSEMOVE, 0, 0);
    if (!Expect(result_mousemove == 0, "Hidden overlay should pass WM_MOUSEMOVE to original WndProc")) return false;

    LRESULT result_lbuttondown = overlay.WndProcHandler(hwnd, WM_LBUTTONDOWN, 0, 0);
    if (!Expect(result_lbuttondown == 0, "Hidden overlay should pass WM_LBUTTONDOWN to original WndProc")) return false;

    overlay.Shutdown();
    return true;
}

bool TestQuickMapWheelPassesThroughWhenCallbackMissing() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const WPARAM wheel_wparam = MAKEWPARAM(0, static_cast<WORD>(WHEEL_DELTA));
    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_MOUSEWHEEL, wheel_wparam, 0);
    if (!Expect(result == 0, "Hidden overlay should pass WM_MOUSEWHEEL when QuickMap callback is missing")) return false;

    overlay.Shutdown();
    return true;
}

bool TestQuickMapWheelPassesThroughWhenCaptureDeclined() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    int capture_checks = 0;
    bool handler_called = false;

    overlay.SetQuickMapWheelCaptureCallback([&]() {
        ++capture_checks;
        return false;
    });
    overlay.SetQuickMapWheelHandlerCallback([&](float) {
        handler_called = true;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const WPARAM wheel_wparam = MAKEWPARAM(0, static_cast<WORD>(WHEEL_DELTA));
    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_MOUSEWHEEL, wheel_wparam, 0);
    if (!Expect(result == 0, "Hidden overlay should pass WM_MOUSEWHEEL when QuickMap declines capture")) return false;
    if (!Expect(capture_checks == 1, "QuickMap wheel capture callback should be queried once")) return false;
    if (!Expect(!handler_called, "QuickMap wheel handler should not run when capture is declined")) return false;

    overlay.Shutdown();
    return true;
}

bool TestQuickMapWheelCapturedWhenOverlayHidden() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    int capture_checks = 0;
    int handler_calls = 0;
    float last_delta = 0.0f;

    overlay.SetQuickMapWheelCaptureCallback([&]() {
        ++capture_checks;
        return true;
    });
    overlay.SetQuickMapWheelHandlerCallback([&](float delta) {
        ++handler_calls;
        last_delta = delta;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const WPARAM wheel_wparam = MAKEWPARAM(0, static_cast<WORD>(WHEEL_DELTA));
    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_MOUSEWHEEL, wheel_wparam, 0);
    if (!Expect(result == 1, "Hidden overlay should swallow WM_MOUSEWHEEL when QuickMap captures it")) return false;
    if (!Expect(capture_checks == 1, "QuickMap wheel capture callback should be queried once for captured wheel input")) return false;
    if (!Expect(handler_calls == 1, "QuickMap wheel handler should run when wheel input is captured")) return false;
    if (!Expect(last_delta == 1.0f, "QuickMap wheel handler should receive normalized wheel delta")) return false;

    overlay.Shutdown();
    return true;
}

bool TestOverlayVisibleWheelCaptureStillUsesExistingBehavior() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    bool capture_checked = false;
    bool handler_called = false;

    overlay.SetQuickMapWheelCaptureCallback([&]() {
        capture_checked = true;
        return true;
    });
    overlay.SetQuickMapWheelHandlerCallback([&](float) {
        handler_called = true;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;
    overlay.Show();

    const WPARAM wheel_wparam = MAKEWPARAM(0, static_cast<WORD>(WHEEL_DELTA));
    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_MOUSEWHEEL, wheel_wparam, 0);
    if (!Expect(result == 1, "Visible overlay should keep capturing WM_MOUSEWHEEL via existing overlay path")) return false;
    if (!Expect(!capture_checked, "QuickMap capture callback should not run when overlay is visible")) return false;
    if (!Expect(!handler_called, "QuickMap wheel handler should not run when overlay is visible")) return false;

    overlay.Shutdown();
    return true;
}

bool TestQuickMapHorizontalWheelSwallowedWhenCaptureActive() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());
    int capture_checks = 0;
    bool handler_called = false;

    overlay.SetQuickMapWheelCaptureCallback([&]() {
        ++capture_checks;
        return true;
    });
    overlay.SetQuickMapWheelHandlerCallback([&](float) {
        handler_called = true;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const WPARAM wheel_wparam = MAKEWPARAM(0, static_cast<WORD>(WHEEL_DELTA));
    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_MOUSEHWHEEL, wheel_wparam, 0);
    if (!Expect(result == 1, "Hidden overlay should swallow WM_MOUSEHWHEEL when QuickMap captures wheel input")) return false;
    if (!Expect(capture_checks == 1, "QuickMap capture callback should be queried once for horizontal wheel input")) return false;
    if (!Expect(!handler_called, "Horizontal wheel swallow should not invoke the vertical wheel handler")) return false;

    overlay.Shutdown();
    return true;
}

bool TestQuickMapCaptureDoesNotSwallowMouseMove() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeSentinelDependencies());
    int capture_checks = 0;

    overlay.SetQuickMapWheelCaptureCallback([&]() {
        ++capture_checks;
        return true;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_MOUSEMOVE, 0, 0);
    if (!Expect(result == 66, "Hidden QuickMap capture should not swallow regular mouse movement")) return false;
    if (!Expect(capture_checks == 0, "QuickMap wheel capture callback should not be queried for regular mouse movement")) return false;

    overlay.Shutdown();
    return true;
}

bool TestQuickMapRawInputPassesThroughWhenCaptureActiveButHandleCannotBeClassified() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeSentinelDependencies());
    int capture_checks = 0;

    overlay.SetQuickMapWheelCaptureCallback([&]() {
        ++capture_checks;
        return true;
    });

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;

    const LRESULT result = overlay.WndProcHandler(FakeDiscoverWindow(), WM_INPUT, 0, 0);
    if (!Expect(result == 77, "Hidden QuickMap capture should not blanket-block WM_INPUT without wheel evidence")) return false;
    if (!Expect(capture_checks == 1, "QuickMap capture callback should be queried once for WM_INPUT passthrough")) return false;

    overlay.Shutdown();
    return true;
}

bool TestEscClosesVisibleOverlay() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.Show();
    if (!Expect(overlay.IsVisible(), "Overlay should be visible")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_ESCAPE, 0);
    if (!Expect(!overlay.IsVisible(), "ESC should hide overlay")) return false;
    if (!Expect(g_state.hide_cursor_calls == 1, "ESC hide should call show_cursor(false)")) return false;

    overlay.Shutdown();
    return true;
}

bool TestEscIgnoredWhenHidden() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    if (!Expect(overlay.Initialize(), "Initialize should succeed")) return false;
    if (!Expect(!overlay.IsVisible(), "Overlay should start hidden")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_ESCAPE, 0);
    if (!Expect(!overlay.IsVisible(), "ESC should not show overlay")) return false;

    overlay.Shutdown();
    return true;
}

bool TestEscAndF2Interop() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(overlay.IsVisible(), "F2 should show overlay")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_ESCAPE, 0);
    if (!Expect(!overlay.IsVisible(), "ESC should hide overlay")) return false;

    overlay.WndProcHandler(FakeDiscoverWindow(), WM_KEYUP, VK_F2, 0);
    if (!Expect(overlay.IsVisible(), "F2 should re-show overlay after ESC close")) return false;

    overlay.Shutdown();
    return true;
}

bool TestRenderSkipsWorkAfterShutdownStarts() {
    std::printf("Running test...\n"); fflush(stdout); ResetState();
    keybinder::ImGuiOverlay overlay(MakeDependencies());

    overlay.Show();
    if (!Expect(overlay.IsVisible(), "Overlay should be visible before shutdown begins")) return false;

    g_shutdown_phase.store(1);
    overlay.Render();

    if (!Expect(g_state.suppress_game_mouse_calls == 0, "Render should not suppress mouse after shutdown starts")) return false;
    if (!Expect(g_state.dx9_new_frame_calls == 0, "Render should not start DX9 frame after shutdown starts")) return false;
    if (!Expect(g_state.win32_new_frame_calls == 0, "Render should not start Win32 frame after shutdown starts")) return false;
    if (!Expect(g_state.render_draw_calls == 0, "Render should not submit draw data after shutdown starts")) return false;
    if (!Expect(!overlay.IsVisible(), "Render should force overlay hidden once shutdown starts")) return false;

    g_shutdown_phase.store(0);
    overlay.Shutdown();
    return true;
}

} // namespace

int RunImGuiOverlayTests() {
    int failed = 0;

    if (TestAutomaticScaleKeepsRelativeFootprintAcrossStandardResolutions()) {
        std::printf("[PASS] TestAutomaticScaleKeepsRelativeFootprintAcrossStandardResolutions\n");
    } else {
        ++failed;
    }

    if (TestAutomaticScaleClampsSmallAndInvalidDisplays()) {
        std::printf("[PASS] TestAutomaticScaleClampsSmallAndInvalidDisplays\n");
    } else {
        ++failed;
    }

    if (TestInitializationAndCleanup()) {
        std::printf("[PASS] TestInitializationAndCleanup\n");
    } else {
        ++failed;
    }

    if (TestInitializationUsesPersistentImGuiIniPath()) {
        std::printf("[PASS] TestInitializationUsesPersistentImGuiIniPath\n");
    } else {
        ++failed;
    }

    if (TestDeviceLostAndResetHandling()) {
        std::printf("[PASS] TestDeviceLostAndResetHandling\n");
    } else {
        ++failed;
    }

    if (TestToggleFunctionality()) {
        std::printf("[PASS] TestToggleFunctionality\n");
    } else {
        ++failed;
    }

    if (TestCursorManagement()) {
        std::printf("[PASS] TestCursorManagement\n");
    } else {
        ++failed;
    }

    if (TestCursorCleanupOnShutdown()) {
        std::printf("[PASS] TestCursorCleanupOnShutdown\n");
    } else {
        ++failed;
    }

    if (TestShutdownNoCursorCleanupWhenHidden()) {
        std::printf("[PASS] TestShutdownNoCursorCleanupWhenHidden\n");
    } else {
        ++failed;
    }

    if (TestF2ToggleCursorLifecycle()) {
        std::printf("[PASS] TestF2ToggleCursorLifecycle\n");
    } else {
        ++failed;
    }

    if (TestConfiguredHotkeyToggle()) {
        std::printf("[PASS] TestConfiguredHotkeyToggle\n");
    } else {
        ++failed;
    }

    if (TestConfiguredHotkeyIgnoredWhileChatOpen()) {
        std::printf("[PASS] TestConfiguredHotkeyIgnoredWhileChatOpen\n");
    } else {
        ++failed;
    }

    if (TestConfiguredHotkeyReleaseIgnoredAfterChatKeyDown()) {
        std::printf("[PASS] TestConfiguredHotkeyReleaseIgnoredAfterChatKeyDown\n");
    } else {
        ++failed;
    }

    if (TestFocusLossInvokesTransientStateHandler()) {
        std::printf("[PASS] TestFocusLossInvokesTransientStateHandler\n");
    } else {
        ++failed;
    }


    if (TestHiddenOverlayDelegatesEarlyKeybindHandling()) {
        std::printf("[PASS] TestHiddenOverlayDelegatesEarlyKeybindHandling\n");
    } else {
        ++failed;
    }

    if (TestHiddenOverlayDelegatesEarlyMouseKeybindHandling()) {
        std::printf("[PASS] TestHiddenOverlayDelegatesEarlyMouseKeybindHandling\n");
    } else {
        ++failed;
    }

    if (TestOverlayVisibleCapturesMouseButNotGameplayKeyboardByDefault()) {
        std::printf("[PASS] TestOverlayVisibleCapturesMouseButNotGameplayKeyboardByDefault\n");
    } else {
        ++failed;
    }

    if (TestFirstOpenCapturesOverlayMouseImmediately()) {
        std::printf("[PASS] TestFirstOpenCapturesOverlayMouseImmediately\n");
    } else {
        ++failed;
    }

    if (TestOverlayHiddenPassesInput()) {
        std::printf("[PASS] TestOverlayHiddenPassesInput\n");
    } else {
        ++failed;
    }

    if (TestQuickMapWheelPassesThroughWhenCallbackMissing()) {
        std::printf("[PASS] TestQuickMapWheelPassesThroughWhenCallbackMissing\n");
    } else {
        ++failed;
    }

    if (TestQuickMapWheelPassesThroughWhenCaptureDeclined()) {
        std::printf("[PASS] TestQuickMapWheelPassesThroughWhenCaptureDeclined\n");
    } else {
        ++failed;
    }

    if (TestQuickMapWheelCapturedWhenOverlayHidden()) {
        std::printf("[PASS] TestQuickMapWheelCapturedWhenOverlayHidden\n");
    } else {
        ++failed;
    }

    if (TestOverlayVisibleWheelCaptureStillUsesExistingBehavior()) {
        std::printf("[PASS] TestOverlayVisibleWheelCaptureStillUsesExistingBehavior\n");
    } else {
        ++failed;
    }

    if (TestQuickMapHorizontalWheelSwallowedWhenCaptureActive()) {
        std::printf("[PASS] TestQuickMapHorizontalWheelSwallowedWhenCaptureActive\n");
    } else {
        ++failed;
    }

    if (TestQuickMapCaptureDoesNotSwallowMouseMove()) {
        std::printf("[PASS] TestQuickMapCaptureDoesNotSwallowMouseMove\n");
    } else {
        ++failed;
    }

    if (TestQuickMapRawInputPassesThroughWhenCaptureActiveButHandleCannotBeClassified()) {
        std::printf("[PASS] TestQuickMapRawInputPassesThroughWhenCaptureActiveButHandleCannotBeClassified\n");
    } else {
        ++failed;
    }

    if (TestEscClosesVisibleOverlay()) {
        std::printf("[PASS] TestEscClosesVisibleOverlay\n");
    } else {
        ++failed;
    }

    if (TestEscIgnoredWhenHidden()) {
        std::printf("[PASS] TestEscIgnoredWhenHidden\n");
    } else {
        ++failed;
    }

    if (TestEscAndF2Interop()) {
        std::printf("[PASS] TestEscAndF2Interop\n");
    } else {
        ++failed;
    }

    if (TestRenderSkipsWorkAfterShutdownStarts()) {
        std::printf("[PASS] TestRenderSkipsWorkAfterShutdownStarts\n");
    } else {
        ++failed;
    }

    return failed;
}
