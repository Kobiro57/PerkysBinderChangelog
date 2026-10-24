#pragma once

#include <d3d9.h>
#include <functional>
#include <string>
#include <windows.h>

#ifndef KEYBINDER_TESTING
namespace safetyhook { class InlineHook; }
#endif

struct ImDrawList;
namespace keybinder { class SettingsManager; }
class SAMPBridge;

namespace keybinder {

class MainWindowUI;

struct ImGuiOverlayDependencies {
    IDirect3DDevice9* (*discover_device)() = nullptr;
    HWND (*discover_window)() = nullptr;
    bool (*imgui_win32_init)(void* hwnd) = nullptr;
    bool (*imgui_dx9_init)(IDirect3DDevice9* device) = nullptr;
    void (*imgui_win32_shutdown)() = nullptr;
    void (*imgui_dx9_shutdown)() = nullptr;
    void (*imgui_win32_new_frame)() = nullptr;
    void (*imgui_dx9_new_frame)() = nullptr;
    void (*imgui_dx9_render_draw_data)(void* draw_data) = nullptr;
    void (*imgui_dx9_invalidate_device_objects)() = nullptr;
    bool (*imgui_dx9_create_device_objects)() = nullptr;
    LRESULT (*imgui_wndproc_handler)(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) = nullptr;
    WNDPROC (*install_wndproc_hook)(HWND hwnd, WNDPROC replacement) = nullptr;
    bool (*remove_wndproc_hook)(HWND hwnd, WNDPROC expected, WNDPROC original) = nullptr;
    bool (*remove_wndproc_hook_on_shutdown)(HWND hwnd, WNDPROC expected, WNDPROC original) = nullptr;
    LRESULT (*call_window_proc)(WNDPROC proc, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) = nullptr;
    void (*show_cursor)(bool visible) = nullptr;
    void (*suppress_game_mouse)() = nullptr;
    void (*restore_game_mouse)() = nullptr;
};

class ImGuiOverlay {
public:
    ImGuiOverlay();
    explicit ImGuiOverlay(ImGuiOverlayDependencies dependencies);
    ~ImGuiOverlay();

    bool Initialize();
    void Shutdown();
    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;
    void Render();
    void OnDeviceLost();
    void OnDeviceReset();
    LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
    IDirect3DDevice9* GetDevice() const { return device_; }
    void SetHitmarkerRenderCallback(std::function<void()> callback);
    void SetToastRenderCallback(std::function<void()> callback);
    void SetQuickMapRenderCallback(std::function<void()> callback);
    void SetVitalStatsRenderCallback(std::function<void()> callback);
    void SetSpeedometerRenderCallback(std::function<void()> callback);
    void SetQuickMapWheelCaptureCallback(std::function<bool()> callback);
    void SetQuickMapWheelHandlerCallback(std::function<void(float)> callback);
    void SetOnlineQuickOverlayRenderCallback(std::function<void()> callback);
    void SetEarlyKeybindHandler(std::function<bool(int, bool)> callback);
    void SetFocusLostHandler(std::function<void()> callback);
    void SetAuxiliaryInteractionActiveCallback(std::function<bool()> callback);
    void SetAuxiliaryInteractionCancelCallback(std::function<void()> callback);

    void SetMainWindowUI(MainWindowUI* ui) { mainWindowUI_ = ui; }

    void SetSettingsManager(SettingsManager* manager);
    void SetSAMPBridge(SAMPBridge* bridge);
    bool ApplyDpiScale(float scale);
    float GetCurrentScale() const { return current_scale_; }
#ifdef KEYBINDER_TESTING
    static float ComputeAutomaticScaleForTesting(float width, float height);
#endif

    MainWindowUI* GetMainWindowUI() const { return mainWindowUI_; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
#ifndef KEYBINDER_TESTING
    static BOOL WINAPI HookedSetCursorPos(int x, int y);
    static HRESULT WINAPI HookedDirectInputGetDeviceState(void* device, DWORD dataSize, void* data);
    static HRESULT WINAPI HookedDirectInputGetDeviceData(void* device, DWORD objectDataSize, void* objectData, DWORD* inOutCount, DWORD flags);
    static void __fastcall HookedChatDraw(void* chat, void* edx);
#endif

    bool DiscoverDevice();
    bool DiscoverWindow();
    bool InstallWndProcHook();
    void RemoveWndProcHook(bool shuttingDown = false);
    bool EnsureContext();
    float ComputeAutomaticScale() const;
    void RefreshAutomaticScaleIfNeeded();
    void ApplyFontScaleState(float scale);
    void RebuildFontAtlas();
    bool ShouldCaptureMessage(bool visible, UINT msg, WPARAM w_param) const;
    bool IsQuickMapWheelCaptureActive() const;
    bool IsKeyboardCaptureActive() const;
    bool IsGameInputActive() const;
    void RenderFrame(bool fromChatDrawHook);
    bool InstallSetCursorPosHook();
    void RemoveSetCursorPosHook();
    bool InstallDirectInputMouseHooks();
    void RemoveDirectInputMouseHooks();
    bool InstallChatDrawHook();
    void RemoveChatDrawHook();

    void RenderBackdropDecor(ImDrawList* drawList);

    MainWindowUI* mainWindowUI_ = nullptr;

    ImGuiOverlayDependencies dependencies_{};
    IDirect3DDevice9* device_ = nullptr;
    HWND window_ = nullptr;
    WNDPROC original_wndproc_ = nullptr;
    std::function<void()> hitmarkerRenderCallback_;
    std::function<void()> toastRenderCallback_;
    std::function<void()> quickMapRenderCallback_;
    std::function<void()> vitalStatsRenderCallback_;
    std::function<void()> speedometerRenderCallback_;
    std::function<void()> onlineQuickOverlayRenderCallback_;
    std::function<bool()> quickMapWheelCaptureCallback_;
    std::function<void(float)> quickMapWheelHandlerCallback_;
    std::function<bool(int, bool)> earlyKeybindHandlerCallback_;
    std::function<void()> focusLostHandlerCallback_;
    std::function<bool()> auxiliaryInteractionActiveCallback_;
    std::function<void()> auxiliaryInteractionCancelCallback_;
    bool initialized_ = false;
    bool visible_ = false;
    bool context_owned_ = false;
    bool device_objects_invalidated_ = false;
    bool cursor_shown_ = false;
    bool ui_toggle_hotkey_suppressed_ = false;

    SettingsManager* settings_manager_ = nullptr;
    SAMPBridge* samp_bridge_ = nullptr;
    std::string ini_path_;
    float current_scale_ = 1.0f;

    bool shutting_down_ = false;
    bool exit_started_ = false;
    bool font_atlas_dirty_ = false;


    static ImGuiOverlay* active_instance_;
#ifndef KEYBINDER_TESTING
    // Raw pointer: deliberately has no destructor, so static destruction order
    // across translation units cannot cause InlineHook::~InlineHook() to crash
    // during ExitProcess teardown. Normal shutdown deletes it; emergency exit leaks it.
    static safetyhook::InlineHook* set_cursor_pos_hook_;
    static safetyhook::InlineHook* direct_input_get_device_state_hook_;
    static safetyhook::InlineHook* direct_input_get_device_data_hook_;
    static safetyhook::InlineHook* chat_draw_hook_;
#endif
};

} // namespace keybinder
