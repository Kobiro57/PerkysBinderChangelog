#include "ImGuiOverlay.h"
#include "MainWindowUI.h"

#include "plugin.h"
#include "RenderWare.h"
#include "common.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "misc/freetype/imgui_freetype.h"
#include "safetyhook.hpp"
#define DIRECTINPUT_VERSION 0x0800
#include "dinput.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <windowsx.h>
#include <utility>

#include "../include/Logger.h"
#include "../include/PersistentPaths.h"
#include "../include/ResourceIds.h"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/ThemeManager.h"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CChat.h"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CInput.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
extern std::atomic<int> g_shutdown_phase;

namespace keybinder {

ImGuiOverlay* ImGuiOverlay::active_instance_ = nullptr;
#ifndef KEYBINDER_TESTING
safetyhook::InlineHook* ImGuiOverlay::set_cursor_pos_hook_ = nullptr;
safetyhook::InlineHook* ImGuiOverlay::direct_input_get_device_state_hook_ = nullptr;
safetyhook::InlineHook* ImGuiOverlay::direct_input_get_device_data_hook_ = nullptr;
safetyhook::InlineHook* ImGuiOverlay::chat_draw_hook_ = nullptr;
#endif

namespace {

bool TryTranslateEarlyInputMessage(UINT msg, WPARAM w_param, int& vk, bool& keyDown) {
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        vk = static_cast<int>(w_param);
        keyDown = true;
        return true;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        vk = static_cast<int>(w_param);
        keyDown = false;
        return true;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        vk = VK_LBUTTON;
        keyDown = true;
        return true;
    case WM_LBUTTONUP:
        vk = VK_LBUTTON;
        keyDown = false;
        return true;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        vk = VK_RBUTTON;
        keyDown = true;
        return true;
    case WM_RBUTTONUP:
        vk = VK_RBUTTON;
        keyDown = false;
        return true;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        vk = VK_MBUTTON;
        keyDown = true;
        return true;
    case WM_MBUTTONUP:
        vk = VK_MBUTTON;
        keyDown = false;
        return true;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
        vk = (GET_XBUTTON_WPARAM(w_param) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
        keyDown = true;
        return true;
    case WM_XBUTTONUP:
        vk = (GET_XBUTTON_WPARAM(w_param) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
        keyDown = false;
        return true;
    default:
        return false;
    }
}

constexpr const char* kPrimaryFontFallbackPath = "fonts\\Inter_28pt-Regular.ttf";
constexpr const char* kMaterialSymbolsFontFallbackPath = "third_party\\assets\\fonts\\icons\\MaterialSymbols.ttf";
constexpr const char* kLatinFallbackFontFileName = "Fonts\\segoeui.ttf";
constexpr float kUiScaleBaselineWidth = 1920.0f;
constexpr float kUiScaleBaselineHeight = 1080.0f;
constexpr float kUiScaleReference4KScale = 1.41421356f;
constexpr float kUiScaleReference4KNormalizedSize = 2.0f;
constexpr float kUiScaleMinimum = 0.70f;
constexpr float kUiScaleMaximum = 1.44f;
constexpr float kUiScaleEpsilon = 0.02f;

float ComputeResolutionRelativeUiScale(float width, float height) {
    if (width <= 0.0f || height <= 0.0f) {
        return 1.0f;
    }

    const float normalizedSize = (std::min)(width / kUiScaleBaselineWidth, height / kUiScaleBaselineHeight);
    const float scalePerNormalizedSize = kUiScaleReference4KScale / kUiScaleReference4KNormalizedSize;
    return SettingsManager::ClampScale((std::clamp)(normalizedSize * scalePerNormalizedSize, kUiScaleMinimum, kUiScaleMaximum));
}
constexpr std::size_t kDirectInputGetDeviceStateVtableIndex = 9;
constexpr std::size_t kDirectInputGetDeviceDataVtableIndex = 10;
constexpr GUID kDirectInput8InterfaceId = { 0xBF798030, 0x483A, 0x4DA2, { 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 } };
constexpr GUID kDirectInputSystemMouseGuid = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
constexpr ImWchar kMaterialSymbolsGlyphRanges[] = {
    0xE312, 0xE312, // keyboard
    0xE3C9, 0xE3C9, // edit
    0xE87B, 0xE87B, // extension
    0xE8B8, 0xE8B8, // settings
    0xE8FD, 0xE8FD, // help
    0xEA23, 0xEA23, // emoji_events
    0
};

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct EmbeddedResourceView {
    const void* data = nullptr;
    std::size_t size = 0;
};

bool ResolveAssetPath(const char* relativePath, char* resolvedPath, size_t resolvedPathCapacity) {
    if (relativePath == nullptr || resolvedPath == nullptr || resolvedPathCapacity == 0) {
        return false;
    }

    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    if (module == nullptr) {
        return false;
    }

    wchar_t modulePath[32768] = {};
    DWORD length = GetModuleFileNameW(module, modulePath, static_cast<DWORD>(sizeof(modulePath) / sizeof(modulePath[0])));
    if (length == 0 || length >= (sizeof(modulePath) / sizeof(modulePath[0]))) {
        return false;
    }

    wchar_t* slash = wcsrchr(modulePath, L'\\');
    wchar_t* forwardSlash = wcsrchr(modulePath, L'/');
    if (forwardSlash != nullptr && (slash == nullptr || forwardSlash > slash)) {
        slash = forwardSlash;
    }
    if (slash == nullptr) {
        return false;
    }
    *slash = L'\0';

    wchar_t relativeWide[1024] = {};
    if (MultiByteToWideChar(CP_ACP, 0, relativePath, -1, relativeWide, static_cast<int>(sizeof(relativeWide) / sizeof(relativeWide[0]))) == 0) {
        return false;
    }

    wchar_t fullPath[32768] = {};
    if (wcscpy_s(fullPath, sizeof(fullPath) / sizeof(fullPath[0]), modulePath) != 0 ||
        wcscat_s(fullPath, sizeof(fullPath) / sizeof(fullPath[0]), L"\\") != 0 ||
        wcscat_s(fullPath, sizeof(fullPath) / sizeof(fullPath[0]), relativeWide) != 0) {
        return false;
    }

    return WideCharToMultiByte(CP_ACP, 0, fullPath, -1, resolvedPath, static_cast<int>(resolvedPathCapacity), nullptr, nullptr) != 0;
}

bool ResolveWindowsFontPath(const char* relativePath, char* resolvedPath, size_t resolvedPathCapacity) {
    if (relativePath == nullptr || resolvedPath == nullptr || resolvedPathCapacity == 0) {
        return false;
    }

    wchar_t windowsPath[32768] = {};
    UINT length = GetWindowsDirectoryW(windowsPath, static_cast<UINT>(sizeof(windowsPath) / sizeof(windowsPath[0])));
    if (length == 0 || length >= (sizeof(windowsPath) / sizeof(windowsPath[0]))) {
        return false;
    }

    wchar_t relativeWide[1024] = {};
    if (MultiByteToWideChar(CP_ACP, 0, relativePath, -1, relativeWide, static_cast<int>(sizeof(relativeWide) / sizeof(relativeWide[0]))) == 0) {
        return false;
    }

    wchar_t fullPath[32768] = {};
    if (wcscpy_s(fullPath, sizeof(fullPath) / sizeof(fullPath[0]), windowsPath) != 0 ||
        wcscat_s(fullPath, sizeof(fullPath) / sizeof(fullPath[0]), L"\\") != 0 ||
        wcscat_s(fullPath, sizeof(fullPath) / sizeof(fullPath[0]), relativeWide) != 0) {
        return false;
    }

    return WideCharToMultiByte(CP_ACP, 0, fullPath, -1, resolvedPath, static_cast<int>(resolvedPathCapacity), nullptr, nullptr) != 0;
}

bool TryLoadEmbeddedResource(WORD resourceId, EmbeddedResourceView* outResource) {
    if (outResource == nullptr) {
        return false;
    }

    *outResource = {};

    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    if (module == nullptr) {
        return false;
    }

    const LPCWSTR resourceType = reinterpret_cast<LPCWSTR>(RT_RCDATA);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), resourceType);
    if (resource == nullptr) {
        return false;
    }

    const DWORD resourceSize = SizeofResource(module, resource);
    if (resourceSize == 0) {
        return false;
    }

    const HGLOBAL loadedResource = LoadResource(module, resource);
    if (loadedResource == nullptr) {
        return false;
    }

    const void* resourceData = LockResource(loadedResource);
    if (resourceData == nullptr) {
        return false;
    }

    outResource->data = resourceData;
    outResource->size = static_cast<std::size_t>(resourceSize);
    return true;
}

bool FileExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool WriteBytesToFile(const std::string& path, const void* data, std::size_t size) {
    if (path.empty() || data == nullptr) {
        return false;
    }

    const DWORD expectedBytes = static_cast<DWORD>(size);

    HANDLE file = CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesWritten = 0;
    const bool success = WriteFile(
        file,
        data,
        expectedBytes,
        &bytesWritten,
        nullptr) != FALSE && bytesWritten == expectedBytes;
    CloseHandle(file);
    return success;
}

#ifndef KEYBINDER_TESTING
void SeedBundledImGuiIniIfMissing(const std::string& path) {
    if (FileExists(path)) {
        return;
    }

    EmbeddedResourceView resource;
    if (!TryLoadEmbeddedResource(IDR_IMGUI_DEFAULT_INI, &resource)) {
        Logger::Warning("[ImGuiOverlay] Bundled imgui.ini resource unavailable; ImGui will create '%s' on first save", path.c_str());
        return;
    }

    if (!WriteBytesToFile(path, resource.data, resource.size)) {
        Logger::Warning("[ImGuiOverlay] Failed to seed bundled imgui.ini at '%s'", path.c_str());
        return;
    }

    Logger::Info("[ImGuiOverlay] Seeded bundled imgui.ini at '%s'", path.c_str());
}
#endif

ImFont* AddFontFromBundledResourceOrFile(
    ImFontAtlas* atlas,
    WORD resourceId,
    const char* fallbackPath,
    float fontSize,
    ImFontConfig* config,
    const ImWchar* glyphRanges,
    const char* logLabel,
    bool* loadedFromResource) {
    if (atlas == nullptr || config == nullptr || fallbackPath == nullptr || logLabel == nullptr) {
        return nullptr;
    }

    if (loadedFromResource != nullptr) {
        *loadedFromResource = false;
    }

    EmbeddedResourceView embeddedResource;
    if (TryLoadEmbeddedResource(resourceId, &embeddedResource)) {
        ImFontConfig resourceConfig = *config;
        resourceConfig.FontDataOwnedByAtlas = false;
        ImFont* font = atlas->AddFontFromMemoryTTF(
            const_cast<void*>(embeddedResource.data),
            static_cast<int>(embeddedResource.size),
            fontSize,
            &resourceConfig,
            glyphRanges);
        if (font != nullptr) {
            if (loadedFromResource != nullptr) {
                *loadedFromResource = true;
            }
            Logger::Info("[ImGuiOverlay] Loaded bundled %s resource (%u) at %.1f px", logLabel, static_cast<unsigned>(resourceId), fontSize);
            return font;
        }

        Logger::Warning("[ImGuiOverlay] Failed to load bundled %s resource (%u); trying loose file fallback", logLabel, static_cast<unsigned>(resourceId));
    }

    char resolvedPath[MAX_PATH * 4] = {};
    const bool fallbackResolved = ResolveAssetPath(fallbackPath, resolvedPath, sizeof(resolvedPath));
    const char* effectivePath = fallbackResolved ? resolvedPath : fallbackPath;
    const DWORD attrs = GetFileAttributesA(effectivePath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        Logger::Warning("[ImGuiOverlay] %s fallback file not found: '%s'", logLabel, effectivePath);
        return nullptr;
    }

    ImFont* font = atlas->AddFontFromFileTTF(effectivePath, fontSize, config, glyphRanges);
    if (font != nullptr) {
        Logger::Info("[ImGuiOverlay] Loaded fallback %s file '%s' at %.1f px", logLabel, effectivePath, fontSize);
    } else {
        Logger::Warning("[ImGuiOverlay] AddFontFromFileTTF failed for %s fallback '%s'", logLabel, effectivePath);
    }

    return font;
}

void DefaultShowCursor(bool visible) {
    keybinder::Logger::Debug("DefaultShowCursor called with visible=%s", visible ? "true" : "false");
    int result = ::ShowCursor(visible ? TRUE : FALSE);
    keybinder::Logger::Debug("ShowCursor returned %d", result);
}

void SyncImGuiCursorVisibility(bool visible) {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible;
}

bool IsSameProcessWindow(HWND hwnd) {
    DWORD window_process_id = 0;
    GetWindowThreadProcessId(hwnd, &window_process_id);
    return window_process_id == GetCurrentProcessId();
}

BOOL CALLBACK EnumProcessWindows(HWND hwnd, LPARAM l_param) {
    if (!IsWindow(hwnd) || !IsSameProcessWindow(hwnd)) {
        return TRUE;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    HWND* result = reinterpret_cast<HWND*>(l_param);
    *result = hwnd;
    return FALSE;
}

IDirect3DDevice9* DiscoverGameDevice() {
    return static_cast<IDirect3DDevice9*>(GetD3DDevice());
}

HWND DiscoverGameWindow() {
    HWND window = nullptr;
    EnumWindows(EnumProcessWindows, reinterpret_cast<LPARAM>(&window));
    if (window != nullptr) {
        return window;
    }

    HWND foreground_window = GetForegroundWindow();
    if (foreground_window != nullptr && IsSameProcessWindow(foreground_window)) {
        return foreground_window;
    }

    HWND active_window = GetActiveWindow();
    if (active_window != nullptr && IsSameProcessWindow(active_window)) {
        return active_window;
    }

    return nullptr;
}

WNDPROC InstallWndProcHookSafe(HWND hwnd, WNDPROC replacement) {
    WNDPROC original = nullptr;
    safetyhook::execute_while_frozen([&]() {
        original = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
        if (original != nullptr) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(replacement));
        }
    });
    return original;
}

bool RemoveWndProcHookSafe(HWND hwnd, WNDPROC expected, WNDPROC original) {
    bool restored = false;
    safetyhook::execute_while_frozen([&]() {
        auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
        if (current == expected) {
            SetLastError(0);
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
            restored = true;
        }
    });
    return restored;
}

bool RemoveWndProcHookOnShutdown(HWND hwnd, WNDPROC expected, WNDPROC original) {
    if (!IsWindow(hwnd)) {
        return false;
    }

    auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
    if (current != expected) {
        return false;
    }

    // CRITICAL: Clear last error before calling SetWindowLongPtrW to correctly
    // detect success/failure. Without this, a stale last-error value can make
    // a failed restore look successful.
    SetLastError(0);
    LONG_PTR result = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
    return result != 0 || GetLastError() == 0;
}

bool IsShutdownInProgress() {
    return g_shutdown_phase.load() != 0;
}

int GetUiToggleHotkey(const SettingsManager* settingsManager) {
    if (settingsManager == nullptr) {
        return VK_F2;
    }

    const int hotkey = settingsManager->Get().ui_toggle_hotkey;
    switch (hotkey) {
    case 0:
    case VK_ESCAPE:
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LBUTTON:
    case VK_RBUTTON:
    case VK_MBUTTON:
    case VK_XBUTTON1:
    case VK_XBUTTON2:
        return VK_F2;
    default:
        return hotkey;
    }
}

const char* GetUiToggleHotkeyLabel(int hotkey) {
    switch (hotkey) {
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    default: return "F2";
    }
}

LRESULT CallOriginalWndProc(WNDPROC proc, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    return CallWindowProcW(proc, hwnd, msg, w_param, l_param);
}

void RenderDefaultOverlayWindow(bool* visible) {
    float scale = ImGui::GetIO().FontGlobalScale;
    ImGui::SetNextWindowSize(ImVec2(420.0f * scale, 180.0f * scale), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Keybinder", visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("Overlay aktiv.");
        ImGui::Separator();
        ImGui::TextUnformatted("Nutze den konfigurierten Hotkey, um das Overlay umzuschalten.");
        ImGui::TextUnformatted("Die Belegung kannst du im Einstellungen-Tab ändern.");
    }
    ImGui::End();
}

bool TryReadRawInput(HRAWINPUT rawInput, RAWINPUT& input) {
    if (rawInput == nullptr) {
        return false;
    }

    input = {};
    UINT inputSize = sizeof(input);
    const UINT read = GetRawInputData(rawInput, RID_INPUT, &input, &inputSize, sizeof(RAWINPUTHEADER));
    return read != static_cast<UINT>(-1);
}

bool IsRawInputMouseEvent(HRAWINPUT rawInput) {
    RAWINPUT input{};
    return TryReadRawInput(rawInput, input) && input.header.dwType == RIM_TYPEMOUSE;
}

bool IsRawInputMouseWheelEvent(HRAWINPUT rawInput) {
    RAWINPUT input{};
    if (!TryReadRawInput(rawInput, input) || input.header.dwType != RIM_TYPEMOUSE) {
        return false;
    }

    const USHORT buttonFlags = input.data.mouse.usButtonFlags;
    return (buttonFlags & RI_MOUSE_WHEEL) != 0 || (buttonFlags & RI_MOUSE_HWHEEL) != 0;
}

#ifndef KEYBINDER_TESTING
using DirectInput8CreateFn = HRESULT (WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

bool TryGetDirectInputDeviceType(void* device, DWORD& deviceType) {
    if (device == nullptr) {
        return false;
    }

    auto* directInputDevice = reinterpret_cast<IDirectInputDevice8A*>(device);
    DIDEVCAPS caps{};
    caps.dwSize = sizeof(caps);
    const HRESULT hr = directInputDevice->GetCapabilities(&caps);
    if (FAILED(hr)) {
        return false;
    }

    deviceType = GET_DIDEVICE_TYPE(caps.dwDevType);
    return true;
}

void SuppressDirectInputMouseState(DWORD dataSize, void* data) {
    if (data == nullptr || dataSize == 0) {
        return;
    }

    if (dataSize >= sizeof(DIMOUSESTATE2)) {
        auto* mouseState = static_cast<DIMOUSESTATE2*>(data);
        mouseState->lX = 0;
        mouseState->lY = 0;
        mouseState->lZ = 0;
        std::memset(mouseState->rgbButtons, 0, sizeof(mouseState->rgbButtons));
        return;
    }

    if (dataSize >= sizeof(DIMOUSESTATE)) {
        auto* mouseState = static_cast<DIMOUSESTATE*>(data);
        mouseState->lX = 0;
        mouseState->lY = 0;
        mouseState->lZ = 0;
        std::memset(mouseState->rgbButtons, 0, sizeof(mouseState->rgbButtons));
        return;
    }

    std::memset(data, 0, dataSize);
}

void SuppressDirectInputMouseWheelState(DWORD dataSize, void* data) {
    if (data == nullptr || dataSize == 0) {
        return;
    }

    if (dataSize >= sizeof(DIMOUSESTATE2)) {
        auto* mouseState = static_cast<DIMOUSESTATE2*>(data);
        mouseState->lZ = 0;
        return;
    }

    if (dataSize >= sizeof(DIMOUSESTATE)) {
        auto* mouseState = static_cast<DIMOUSESTATE*>(data);
        mouseState->lZ = 0;
    }
}

void SuppressDirectInputMouseEvents(DWORD objectDataSize, void* objectData, DWORD* inOutCount) {
    if (objectData != nullptr && objectDataSize != 0 && inOutCount != nullptr && *inOutCount != 0) {
        std::memset(objectData, 0, static_cast<std::size_t>(objectDataSize) * static_cast<std::size_t>(*inOutCount));
    }

    if (inOutCount != nullptr) {
        *inOutCount = 0;
    }
}

void SuppressDirectInputMouseWheelEvents(DWORD objectDataSize, void* objectData, DWORD* inOutCount) {
    if (objectData == nullptr || objectDataSize != sizeof(DIDEVICEOBJECTDATA) || inOutCount == nullptr || *inOutCount == 0) {
        return;
    }

    auto* events = static_cast<DIDEVICEOBJECTDATA*>(objectData);
    DWORD writeIndex = 0;
    for (DWORD readIndex = 0; readIndex < *inOutCount; ++readIndex) {
        if (events[readIndex].dwOfs == DIMOFS_Z) {
            continue;
        }

        if (writeIndex != readIndex) {
            events[writeIndex] = events[readIndex];
        }
        ++writeIndex;
    }

    if (writeIndex < *inOutCount) {
        std::memset(events + writeIndex, 0, sizeof(DIDEVICEOBJECTDATA) * static_cast<std::size_t>(*inOutCount - writeIndex));
    }
    *inOutCount = writeIndex;
}
#endif

}

float ImGuiOverlay::ComputeAutomaticScale() const {
    float width = 0.0f;
    float height = 0.0f;

    if (window_ != nullptr) {
        RECT clientRect{};
        if (::GetClientRect(window_, &clientRect) != FALSE) {
            width = static_cast<float>(clientRect.right - clientRect.left);
            height = static_cast<float>(clientRect.bottom - clientRect.top);
        }
    }

    if ((width <= 0.0f || height <= 0.0f) && ImGui::GetCurrentContext() != nullptr) {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        width = displaySize.x;
        height = displaySize.y;
    }

    return ComputeResolutionRelativeUiScale(width, height);
}

#ifdef KEYBINDER_TESTING
float ImGuiOverlay::ComputeAutomaticScaleForTesting(float width, float height) {
    return ComputeResolutionRelativeUiScale(width, height);
}
#endif

void ImGuiOverlay::RefreshAutomaticScaleIfNeeded() {
    if (!initialized_) {
        return;
    }

    const float automaticScale = ComputeAutomaticScale();
    if (std::fabs(automaticScale - current_scale_) <= kUiScaleEpsilon) {
        return;
    }

    Logger::Info("[ImGuiOverlay] Automatic UI scale changed %.2f -> %.2f", current_scale_, automaticScale);
    ApplyDpiScale(automaticScale);
}

#ifndef KEYBINDER_TESTING
void SuppressGameMouseInput() {
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

    CMouseControllerState emptyMouse = {};
    CPad::PCTempMouseControllerState = emptyMouse;
    CPad::NewMouseControllerState = emptyMouse;
    CPad::OldMouseControllerState = emptyMouse;

    clearMouseState(CPad::PCTempMouseControllerState);
    clearMouseState(CPad::NewMouseControllerState);
    clearMouseState(CPad::OldMouseControllerState);

    // Only neutralize mouse-look/camera state. Leave movement controls intact so
    // opening the UI does not behave like an instant stop.
    CPad* pad = CPad::GetPad(0);
    if (pad != nullptr) {
        clearCameraLookState(pad->NewState);
        clearCameraLookState(pad->OldState);
        clearCameraLookState(pad->PCTempKeyState);
        clearCameraLookState(pad->PCTempJoyState);
        clearCameraLookState(pad->PCTempMouseState);
    }
    
    CPad::ClearMouseHistory();
    
    sampapi::v03dl::CInput* sampInput = sampapi::v03dl::RefInputBox();
    if (sampInput != nullptr && sampInput->m_bEnabled) {
        sampInput->Close();
    }
}

void RestoreGameMouseInput() {
}
#else
void SuppressGameMouseInput() {
}
void RestoreGameMouseInput() {
}
#endif

ImGuiOverlay::ImGuiOverlay()
    : ImGuiOverlay({
          &DiscoverGameDevice,
          &DiscoverGameWindow,
          &ImGui_ImplWin32_Init,
          &ImGui_ImplDX9_Init,
          &ImGui_ImplWin32_Shutdown,
          &ImGui_ImplDX9_Shutdown,
          &ImGui_ImplWin32_NewFrame,
          &ImGui_ImplDX9_NewFrame,
          reinterpret_cast<void (*)(void*)>(&ImGui_ImplDX9_RenderDrawData),
          &ImGui_ImplDX9_InvalidateDeviceObjects,
          &ImGui_ImplDX9_CreateDeviceObjects,
          &ImGui_ImplWin32_WndProcHandler,
          &InstallWndProcHookSafe,
          &RemoveWndProcHookSafe,
          &RemoveWndProcHookOnShutdown,
          &CallOriginalWndProc,
          &DefaultShowCursor,
          &SuppressGameMouseInput,
          &RestoreGameMouseInput,
      }) {}

ImGuiOverlay::ImGuiOverlay(ImGuiOverlayDependencies dependencies)
    : dependencies_(std::move(dependencies)) {}

ImGuiOverlay::~ImGuiOverlay() {
    extern std::atomic<int> g_shutdown_phase;
    int phase = g_shutdown_phase.load();

    if (phase != 2 && phase != 3) {
#ifndef KEYBINDER_TESTING
        set_cursor_pos_hook_ = nullptr;
        direct_input_get_device_state_hook_ = nullptr;
        direct_input_get_device_data_hook_ = nullptr;
#endif
        OutputDebugStringA("[ImGuiOverlay] Destructor - emergency exit (hook pointer nulled)");
        return;
    }
    
    // Phase 3 = EmergencyShutdown already completed (config saved, bridge down).
    // Overlay was intentionally NOT shut down during emergency path to avoid
    // crashing other hook chains (SpecialK). Just null the pointer and bail.
    if (phase == 3) {
#ifndef KEYBINDER_TESTING
        set_cursor_pos_hook_ = nullptr;
        direct_input_get_device_state_hook_ = nullptr;
        direct_input_get_device_data_hook_ = nullptr;
#endif
        return;
    }
    
    // Normal shutdown path (phase==2 via shutdownRwEvent) - idempotent cleanup
    Shutdown();
}

bool ImGuiOverlay::Initialize() {
    if (initialized_) {
        return true;
    }

    if (!DiscoverDevice() || !DiscoverWindow() || !EnsureContext()) {
        Logger::Warning("[ImGuiOverlay] Initialize: failed to discover device/window or create context");
        return false;
    }

    Logger::Debug("[CURSOR_DEBUG] Initialize: device=%p, window=%p", device_, window_);

    if (dependencies_.imgui_win32_init == nullptr || dependencies_.imgui_dx9_init == nullptr) {
        Logger::Error("[ImGuiOverlay] Initialize: missing ImGui init dependencies");
        return false;
    }

    if (!dependencies_.imgui_win32_init(window_) || !dependencies_.imgui_dx9_init(device_)) {
        if (dependencies_.imgui_win32_shutdown != nullptr) {
            dependencies_.imgui_win32_shutdown();
        }
        Logger::Error("[ImGuiOverlay] Initialize: ImGui backend init failed");
        return false;
    }

    Logger::Debug("[CURSOR_DEBUG] Initialize: ImGui backends initialized, imgui_wndproc_handler=%p", 
        dependencies_.imgui_wndproc_handler);

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        Logger::Debug("[CURSOR_DEBUG] Initialize: io.BackendPlatformUserData=%p, io.BackendFlags=0x%X", 
            io.BackendPlatformUserData, io.BackendFlags);
        Logger::Debug("[CURSOR_DEBUG] Initialize: io.MouseDrawCursor=%s, io.ConfigFlags=0x%X", 
            io.MouseDrawCursor ? "true" : "false", io.ConfigFlags);
    }

    if (!InstallWndProcHook()) {
        if (dependencies_.imgui_dx9_shutdown != nullptr) {
            dependencies_.imgui_dx9_shutdown();
        }
        if (dependencies_.imgui_win32_shutdown != nullptr) {
            dependencies_.imgui_win32_shutdown();
        }
        Logger::Error("[ImGuiOverlay] Initialize: WndProc hook install failed");
        return false;
    }

    if (!InstallSetCursorPosHook()) {
        Logger::Warning("[ImGuiOverlay] Initialize: SetCursorPos hook install failed (non-fatal)");
    }

    if (!InstallDirectInputMouseHooks()) {
        Logger::Warning("[ImGuiOverlay] Initialize: DirectInput mouse hook install failed (non-fatal)");
    }

    if (!InstallChatDrawHook()) {
        Logger::Warning("[ImGuiOverlay] Initialize: CChat::Draw hook install failed; using drawFontsEvent fallback");
    }

    device_objects_invalidated_ = false;
    initialized_ = true;
    Logger::Info("[ImGuiOverlay] Initialized successfully");
    Logger::Debug("[CURSOR_DEBUG] Initialize: WndProc hook installed, original_wndproc=%p", original_wndproc_);
    return true;
}

void ImGuiOverlay::Shutdown() {
    // Guard against double-cleanup during shutdown (can be called from both
    // shutdownRwEvent and destructor)
    if (shutting_down_) {
        Logger::Debug("[ImGuiOverlay] Shutdown already in progress, skipping");
        return;
    }
    
    // CRITICAL: Check if we're in emergency exit mode (DLL_PROCESS_DETACH from /q)
    // During emergency exit, DO NOT attempt to remove hooks or destroy contexts.
    // Research from apitrace project: "There is no effective way to control the
    // order DLLs receive DLL_PROCESS_DETACH... unpatching our hooks doesn't always
    // work. So instead just do nothing."
    //
    // The OS will reclaim all resources when the process terminates.
    // Attempting to manipulate hooks during process exit can cause crashes.
#ifndef KEYBINDER_TESTING
    extern std::atomic<int> g_shutdown_phase;
    if (g_shutdown_phase.load() != 2) {
        // Emergency exit (phase != 2 means we're not in shutdownRwEvent)
        // DO NOTHING - let the OS clean up
        Logger::Info("[ImGuiOverlay] Emergency exit detected - skipping cleanup");
        shutting_down_ = true;
        return;
    }
#endif
    
    // Normal shutdown via shutdownRwEvent (phase == 2) - safe to clean up
    shutting_down_ = true;

    // Early exit if nothing to clean up
    if (!initialized_ && !context_owned_ && original_wndproc_ == nullptr && !cursor_shown_) {
        Logger::Debug("[ImGuiOverlay] Nothing to clean up, shutdown complete");
        return;
    }

    Logger::Info("[ImGuiOverlay] Shutting down");

    if (cursor_shown_ && dependencies_.show_cursor != nullptr) {
        dependencies_.show_cursor(false);
        cursor_shown_ = false;
    }
    SyncImGuiCursorVisibility(false);

    RemoveChatDrawHook();
    RemoveWndProcHook(true);
    RemoveSetCursorPosHook();
    RemoveDirectInputMouseHooks();

    if (initialized_) {
        OnDeviceLost();

        if (dependencies_.imgui_dx9_shutdown != nullptr) {
            dependencies_.imgui_dx9_shutdown();
        }
        if (dependencies_.imgui_win32_shutdown != nullptr) {
            dependencies_.imgui_win32_shutdown();
        }
    }

    if (context_owned_ && ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }

    initialized_ = false;
    visible_ = false;
    context_owned_ = false;
    device_objects_invalidated_ = false;
    font_atlas_dirty_ = false;
    cursor_shown_ = false;
    device_ = nullptr;
    window_ = nullptr;
}

void ImGuiOverlay::Show() {
    // Initialize synchronously if not already done - ensures WndProc hook is installed
    // before we return. This closes the timing gap where /kb would show the overlay
    // but gameplay input could leak through before Render() called Initialize().
    if (!initialized_) {
        if (!Initialize()) {
            return;
        }
    }
    if (visible_) {
        return;
    }

    Logger::Info("[ImGuiOverlay] Showing overlay");
    visible_ = true;
    keybinder::Logger::Debug("ImGuiOverlay::Show() - showing cursor, show_cursor=%p", dependencies_.show_cursor);
    if (dependencies_.show_cursor != nullptr) {
        dependencies_.show_cursor(true);
    }
    cursor_shown_ = true;
    SyncImGuiCursorVisibility(true);
}

void ImGuiOverlay::Hide() {
    if (!visible_) {
        return;
    }

    if (mainWindowUI_ != nullptr) {
        mainWindowUI_->PersistWindowState();
    }

    Logger::Info("[ImGuiOverlay] Hiding overlay");
    visible_ = false;
    
    // Restore game controls BEFORE hiding cursor
    if (dependencies_.restore_game_mouse != nullptr) {
        dependencies_.restore_game_mouse();
    }
    
    if (cursor_shown_ && dependencies_.show_cursor != nullptr) {
        dependencies_.show_cursor(false);
    }
    cursor_shown_ = false;
    SyncImGuiCursorVisibility(false);
}

void ImGuiOverlay::Toggle() {
    if (visible_) {
        Hide();
    } else {
        Show();
    }
}

bool ImGuiOverlay::IsVisible() const {
    return visible_;
}

void ImGuiOverlay::SetHitmarkerRenderCallback(std::function<void()> callback) {
    hitmarkerRenderCallback_ = std::move(callback);
}

void ImGuiOverlay::SetToastRenderCallback(std::function<void()> callback) {
    toastRenderCallback_ = std::move(callback);
}

void ImGuiOverlay::SetQuickMapRenderCallback(std::function<void()> callback) {
    quickMapRenderCallback_ = std::move(callback);
}

void ImGuiOverlay::SetVitalStatsRenderCallback(std::function<void()> callback) {
    vitalStatsRenderCallback_ = std::move(callback);
}

void ImGuiOverlay::SetSpeedometerRenderCallback(std::function<void()> callback) {
    speedometerRenderCallback_ = std::move(callback);
}

void ImGuiOverlay::SetQuickMapWheelCaptureCallback(std::function<bool()> callback) {
    quickMapWheelCaptureCallback_ = std::move(callback);
}

void ImGuiOverlay::SetOnlineQuickOverlayRenderCallback(std::function<void()> callback) {
    onlineQuickOverlayRenderCallback_ = std::move(callback);
}

void ImGuiOverlay::SetQuickMapWheelHandlerCallback(std::function<void(float)> callback) {
    quickMapWheelHandlerCallback_ = std::move(callback);
}

bool ImGuiOverlay::IsQuickMapWheelCaptureActive() const {
    return !visible_ && quickMapWheelCaptureCallback_ != nullptr && quickMapWheelCaptureCallback_();
}

bool ImGuiOverlay::IsKeyboardCaptureActive() const {
    if (!visible_ || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard || io.WantTextInput;
}

bool ImGuiOverlay::IsGameInputActive() const {
    return samp_bridge_ != nullptr
        && (samp_bridge_->IsChatOpen() || samp_bridge_->IsDialogOpen() || samp_bridge_->IsCursorActive());
}

void ImGuiOverlay::Render() {
    RenderFrame(false);
}

void ImGuiOverlay::RenderFrame(bool from_chat_draw_hook) {
    if (shutting_down_ || exit_started_ || IsShutdownInProgress()) {
        visible_ = false;
        cursor_shown_ = false;
        SyncImGuiCursorVisibility(false);
        return;
    }

#ifndef KEYBINDER_TESTING
    if (!from_chat_draw_hook && chat_draw_hook_ != nullptr) {
        return;
    }
#endif

    if (!initialized_ && !Initialize()) {
        return;
    }

    RefreshAutomaticScaleIfNeeded();

    if (dependencies_.imgui_dx9_new_frame == nullptr || dependencies_.imgui_win32_new_frame == nullptr) {
        return;
    }

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    const bool auxiliaryInteractionActive = auxiliaryInteractionActiveCallback_ != nullptr && auxiliaryInteractionActiveCallback_();
    const bool interactionActive = visible_ || auxiliaryInteractionActive;
    if (interactionActive && dependencies_.suppress_game_mouse != nullptr) {
        dependencies_.suppress_game_mouse();
    }

    if (interactionActive && ctx != nullptr) {
        const ImGuiIO& io_pre = ImGui::GetIO();
        keybinder::Logger::Debug("[CURSOR_DEBUG] Render() PRE: io.MousePos=(%.1f, %.1f), io.MouseDrawCursor=%s", 
            io_pre.MousePos.x, io_pre.MousePos.y, io_pre.MouseDrawCursor ? "true" : "false");
    }

    if (interactionActive) {
        if (!cursor_shown_ && dependencies_.show_cursor != nullptr) {
            dependencies_.show_cursor(true);
        }
        cursor_shown_ = true;
        SyncImGuiCursorVisibility(true);
    } else if (cursor_shown_) {
        if (dependencies_.show_cursor != nullptr) {
            dependencies_.show_cursor(false);
        }
        cursor_shown_ = false;
        SyncImGuiCursorVisibility(false);
        if (dependencies_.restore_game_mouse != nullptr) {
            dependencies_.restore_game_mouse();
        }
    }
    dependencies_.imgui_dx9_new_frame();
    dependencies_.imgui_win32_new_frame();
    ImGui::NewFrame();

    if (interactionActive && ctx != nullptr) {
        const ImGuiIO& io_post = ImGui::GetIO();
        keybinder::Logger::Debug("[CURSOR_DEBUG] Render() POST-NewFrame: io.MousePos=(%.1f, %.1f)", 
            io_post.MousePos.x, io_post.MousePos.y);
    }

    bool next_visible = visible_;

    if (visible_ && !auxiliaryInteractionActive) {
        ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();
        RenderBackdropDecor(bgDrawList);

        if (mainWindowUI_ != nullptr) {
            mainWindowUI_->Render();
            if (mainWindowUI_->WasCloseRequested()) {
                mainWindowUI_->ResetCloseRequest();
                next_visible = false;
            }
        } else {
            RenderDefaultOverlayWindow(&next_visible);
        }
    }

    if (quickMapRenderCallback_) {
        quickMapRenderCallback_();
    }

    if (onlineQuickOverlayRenderCallback_) {
        onlineQuickOverlayRenderCallback_();
    }

    if (vitalStatsRenderCallback_) {
        vitalStatsRenderCallback_();
    }

    if (hitmarkerRenderCallback_) {
        hitmarkerRenderCallback_();
    }

    if (toastRenderCallback_) {
        toastRenderCallback_();
    }

    if (speedometerRenderCallback_) {
        speedometerRenderCallback_();
    }

    ImGui::EndFrame();
    ImGui::Render();

    if (dependencies_.imgui_dx9_render_draw_data != nullptr) {
        dependencies_.imgui_dx9_render_draw_data(ImGui::GetDrawData());
    }

    if (!next_visible && visible_) {
        Hide();
    }
}


void ImGuiOverlay::RenderBackdropDecor(ImDrawList* drawList) {
    if (drawList == nullptr) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float screenW = io.DisplaySize.x;
    const float screenH = io.DisplaySize.y;

    if (screenW <= 0.0f || screenH <= 0.0f) {
        return;
    }

    ThemeManager& theme = ThemeManager::Instance();
    // Keep the game readable behind the window: a restrained dim gradient only.
    constexpr bool blurActive = false;
    ImVec4 bgTop = theme.GetColor(ThemeColorRole::Background);
    ImVec4 bgBottom = theme.GetColor(ThemeColorRole::Background);
    bgTop.w = blurActive ? 0.08f : 0.14f;
    bgBottom.w = blurActive ? 0.16f : 0.24f;
    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f),
        ImVec2(screenW, screenH),
        ImGui::GetColorU32(bgTop),
        ImGui::GetColorU32(bgTop),
        ImGui::GetColorU32(bgBottom),
        ImGui::GetColorU32(bgBottom));
}

void ImGuiOverlay::OnDeviceLost() {
    if (!initialized_ || device_objects_invalidated_) {
        return;
    }

    Logger::Info("[ImGuiOverlay] OnDeviceLost: invalidating device objects and font atlas");

    if (dependencies_.imgui_dx9_invalidate_device_objects != nullptr) {
        dependencies_.imgui_dx9_invalidate_device_objects();
    }
    device_objects_invalidated_ = true;

    // Mark font atlas as dirty so it gets rebuilt on reset.
    // The ImGui DX9 backend font atlas texture lives in D3DPOOL_DEFAULT
    // which is destroyed on device lost. We must rebuild it from scratch.
    font_atlas_dirty_ = true;

    // Null out device pointer - it may be invalid after reset
    device_ = nullptr;
}

void ImGuiOverlay::OnDeviceReset() {
    if (!initialized_) {
        return;
    }

    if (!device_objects_invalidated_) {
        return;
    }

    Logger::Info("[ImGuiOverlay] OnDeviceReset: rebuilding device objects");

    device_ = nullptr;
    if (!DiscoverDevice()) {
        Logger::Error("[ImGuiOverlay] OnDeviceReset: failed to re-discover device");
        return;
    }

    const float restoredScale = ComputeAutomaticScale();
    if (restoredScale != current_scale_) {
        Logger::Info("[ImGuiOverlay] OnDeviceReset: UI scale changed %.2f -> %.2f", current_scale_, restoredScale);
    }
    current_scale_ = restoredScale;

    // Rebuild font atlas if it was marked dirty (always after device lost).
    // This must happen BEFORE CreateDeviceObjects because the DX9 backend
    // creates the font atlas texture from the current pixel data.
    if (font_atlas_dirty_) {
        RebuildFontAtlas();
    }

    if (dependencies_.imgui_dx9_create_device_objects != nullptr) {
        if (!dependencies_.imgui_dx9_create_device_objects()) {
            Logger::Error("[ImGuiOverlay] OnDeviceReset: CreateDeviceObjects failed");
            return;
        }
    }
    device_objects_invalidated_ = false;


    // Re-apply theme and keep font scaling factors neutral after the font atlas
    // rebuild. Fonts are already rasterized at current_scale_, so using either
    // style.FontScaleMain or the legacy io.FontGlobalScale here would double-scale
    // text and desync geometry from the canonical DPI setting.
    ThemeManager::Instance().ApplyConfiguredTheme(settings_manager_, current_scale_);
    ApplyFontScaleState(current_scale_);

    Logger::Info("[ImGuiOverlay] OnDeviceReset: device objects rebuilt successfully");
}

LRESULT ImGuiOverlay::WndProcHandler(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    // Never block close/destroy messages or the game will hang on exit.
    const UINT sys_cmd = static_cast<UINT>(w_param & 0xFFF0u);
    if (msg == WM_CLOSE || msg == WM_DESTROY ||
        (msg == WM_SYSCOMMAND && sys_cmd == SC_CLOSE) ||
        msg == WM_NCDESTROY) {
        Logger::Info("[WNDPROC] Shutdown message received: msg=%u (WM_CLOSE=%d, WM_DESTROY=%d, SC_CLOSE=%d, WM_NCDESTROY=%d)", 
            msg, msg == WM_CLOSE, msg == WM_DESTROY, 
            (msg == WM_SYSCOMMAND && sys_cmd == SC_CLOSE), msg == WM_NCDESTROY);

        if (!exit_started_) {
            exit_started_ = true;

            g_shutdown_phase.store(1);

            Logger::Info("[ImGuiOverlay] Exit fast-path initiated - removing ALL hooks immediately");

            visible_ = false;
            cursor_shown_ = false;
            SyncImGuiCursorVisibility(false);

            // Remove SetCursorPos hook first — it's the most dangerous during teardown.
            RemoveSetCursorPosHook();

            if (dependencies_.restore_game_mouse != nullptr) {
                dependencies_.restore_game_mouse();
            }

            // Remove WndProc hook immediately to prevent re-entrancy and crashes
            // during window destruction. SA-MP's /q command triggers immediate
            // teardown without clean message order.
            if (original_wndproc_ != nullptr) {
                Logger::Info("[WNDPROC] Shutdown message - restoring original WndProc immediately");

                WNDPROC saved_original = original_wndproc_;

                original_wndproc_ = nullptr;
                active_instance_ = nullptr;

                if (IsWindow(hwnd)) {
                    if (dependencies_.remove_wndproc_hook_on_shutdown != nullptr) {
                        dependencies_.remove_wndproc_hook_on_shutdown(hwnd, &StaticWndProc, saved_original);
                    } else if (dependencies_.remove_wndproc_hook != nullptr) {
                        dependencies_.remove_wndproc_hook(hwnd, &StaticWndProc, saved_original);
                    }
                }
            }
        }
        
        if (original_wndproc_ != nullptr && dependencies_.call_window_proc != nullptr) {
            return dependencies_.call_window_proc(original_wndproc_, hwnd, msg, w_param, l_param);
        }
        return DefWindowProcW(hwnd, msg, w_param, l_param);
    }

    bool forwardedFocusMessageToImGui = false;
    if ((msg == WM_SETFOCUS || msg == WM_KILLFOCUS) && initialized_ && dependencies_.imgui_wndproc_handler != nullptr) {
        dependencies_.imgui_wndproc_handler(hwnd, msg, w_param, l_param);
        forwardedFocusMessageToImGui = true;
    }

    const bool auxiliaryInteractionActive = auxiliaryInteractionActiveCallback_ != nullptr && auxiliaryInteractionActiveCallback_();
    const bool interactionActive = visible_ || auxiliaryInteractionActive;

    const bool focusLost = msg == WM_KILLFOCUS
        || (msg == WM_ACTIVATEAPP && w_param == FALSE)
        || (msg == WM_ACTIVATE && LOWORD(w_param) == WA_INACTIVE)
        || (msg == WM_SIZE && w_param == SIZE_MINIMIZED);
    if (focusLost) {
        if (focusLostHandlerCallback_ != nullptr) {
            focusLostHandlerCallback_();
        }
        ui_toggle_hotkey_suppressed_ = false;
        if (dependencies_.restore_game_mouse != nullptr) {
            dependencies_.restore_game_mouse();
        }
    }

    const int uiToggleHotkey = GetUiToggleHotkey(settings_manager_);
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && static_cast<int>(w_param) == uiToggleHotkey) {
        ui_toggle_hotkey_suppressed_ = IsGameInputActive();
    }

    if ((msg == WM_KEYUP || msg == WM_SYSKEYUP) && static_cast<int>(w_param) == uiToggleHotkey) {
        if (ui_toggle_hotkey_suppressed_ || IsGameInputActive()) {
            ui_toggle_hotkey_suppressed_ = false;
            return 0;
        }
        ui_toggle_hotkey_suppressed_ = false;
        Toggle();
        return 0;
    }

    if (msg == WM_KEYUP && w_param == VK_ESCAPE && interactionActive) {
        if (auxiliaryInteractionActive) {
            if (auxiliaryInteractionCancelCallback_ != nullptr) {
                auxiliaryInteractionCancelCallback_();
            }
            return 0;
        }
        if (mainWindowUI_ == nullptr || !mainWindowUI_->IsCapturingKey()) {
            Hide();
            return 0;
        }
    }

    if (!interactionActive && earlyKeybindHandlerCallback_ != nullptr) {
        int earlyVk = 0;
        bool earlyKeyDown = false;
        if (TryTranslateEarlyInputMessage(msg, w_param, earlyVk, earlyKeyDown)) {
            if (earlyKeyDown) {
                if (earlyKeybindHandlerCallback_(earlyVk, true)) {
                    return 1;
                }
            } else {
                earlyKeybindHandlerCallback_(earlyVk, false);
            }
        }
    }

    // Block only raw mouse input while the overlay owns the mouse. Raw keyboard
    // input must pass through so opening the menu never releases held movement keys.
    if (msg == WM_INPUT) {
        if (interactionActive && IsRawInputMouseEvent(reinterpret_cast<HRAWINPUT>(l_param))) {
            keybinder::Logger::Debug("[CURSOR_DEBUG] Blocked WM_INPUT raw mouse - overlay visible");
            return 0;
        }

        if (IsQuickMapWheelCaptureActive() && IsRawInputMouseWheelEvent(reinterpret_cast<HRAWINPUT>(l_param))) {
            keybinder::Logger::Debug("[CURSOR_DEBUG] Blocked WM_INPUT wheel - QuickMap wheel capture active");
            return 0;
        }
    }

    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        const bool quickMapCapturingWheel = IsQuickMapWheelCaptureActive();
        if (quickMapCapturingWheel && msg == WM_MOUSEWHEEL) {
            const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA);
            if (quickMapWheelHandlerCallback_) {
                quickMapWheelHandlerCallback_(delta);
            }
            return 1;
        }
        if (quickMapCapturingWheel && msg == WM_MOUSEHWHEEL) {
            return 1;
        }
    }

    if (interactionActive && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) {
        const int x = GET_X_LPARAM(l_param);
        const int y = GET_Y_LPARAM(l_param);
        keybinder::Logger::Debug("[CURSOR_DEBUG] WndProcHandler: msg=%u (WM_MOUSEMOVE=%d), pos=(%d,%d), interactionActive=%s, initialized=%s, wndproc_handler=%p", 
            msg, msg == WM_MOUSEMOVE, x, y, interactionActive ? "true" : "false", initialized_ ? "true" : "false", dependencies_.imgui_wndproc_handler);
    }

    if (interactionActive && initialized_ && dependencies_.imgui_wndproc_handler != nullptr) {
        // WM_MOUSEMOVE: use GetCursorPos for real position (GTA centers cursor for FPS camera)
        if (msg == WM_MOUSEMOVE) {
            POINT cursor_pos;
            if (::GetCursorPos(&cursor_pos) && ::ScreenToClient(hwnd, &cursor_pos)) {
                keybinder::Logger::Debug("[CURSOR_DEBUG] Real cursor pos via GetCursorPos: (%ld, %ld)", cursor_pos.x, cursor_pos.y);

                ImGuiIO& io = ImGui::GetIO();
                io.AddMousePosEvent((float)cursor_pos.x, (float)cursor_pos.y);
            }
        }

        if (msg != WM_MOUSEMOVE && !forwardedFocusMessageToImGui) {
            const LRESULT backend_result = dependencies_.imgui_wndproc_handler(hwnd, msg, w_param, l_param);
            if (backend_result != 0) {
                return backend_result;
            }
        }

        if (ShouldCaptureMessage(interactionActive, msg, w_param)) {
            if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
                keybinder::Logger::Debug("[CURSOR_DEBUG] Capturing mouse msg=%u (blocking game)", msg);
            }
            return 1;
        }
    }

    if (interactionActive && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) {
        keybinder::Logger::Debug("[CURSOR_DEBUG] Mouse msg=%u PASSTHROUGH to original_wndproc=%p", msg, original_wndproc_);
    }

    if (original_wndproc_ != nullptr && dependencies_.call_window_proc != nullptr) {
        return dependencies_.call_window_proc(original_wndproc_, hwnd, msg, w_param, l_param);
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

LRESULT CALLBACK ImGuiOverlay::StaticWndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    if (active_instance_ != nullptr) {
        return active_instance_->WndProcHandler(hwnd, msg, w_param, l_param);
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

bool ImGuiOverlay::DiscoverDevice() {
    if (device_ != nullptr) {
        return true;
    }

    if (dependencies_.discover_device == nullptr) {
        return false;
    }

    device_ = dependencies_.discover_device();
    return device_ != nullptr;
}

bool ImGuiOverlay::DiscoverWindow() {
    if (window_ != nullptr && IsWindow(window_)) {
        return true;
    }

    if (dependencies_.discover_window == nullptr) {
        return false;
    }

    window_ = dependencies_.discover_window();
    return window_ != nullptr;
}

bool ImGuiOverlay::InstallWndProcHook() {
    if (original_wndproc_ != nullptr) {
        return true;
    }

    if (dependencies_.install_wndproc_hook == nullptr || window_ == nullptr) {
        return false;
    }

    original_wndproc_ = dependencies_.install_wndproc_hook(window_, &StaticWndProc);
    if (original_wndproc_ == nullptr) {
        return false;
    }

    active_instance_ = this;
    return true;
}

void ImGuiOverlay::RemoveWndProcHook(bool shuttingDown) {
    if (original_wndproc_ == nullptr || window_ == nullptr) {
        if (active_instance_ == this) {
            active_instance_ = nullptr;
        }
        original_wndproc_ = nullptr;
        return;
    }

    const auto removeHook = shuttingDown && dependencies_.remove_wndproc_hook_on_shutdown != nullptr
        ? dependencies_.remove_wndproc_hook_on_shutdown
        : dependencies_.remove_wndproc_hook;

    if (removeHook != nullptr) {
        removeHook(window_, &StaticWndProc, original_wndproc_);
    }

    if (active_instance_ == this) {
        active_instance_ = nullptr;
    }
    original_wndproc_ = nullptr;
}

bool ImGuiOverlay::EnsureContext() {
    if (ImGui::GetCurrentContext() != nullptr) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    context_owned_ = true;

    ImGuiIO& io = ImGui::GetIO();
    const std::string persistentRoot = PersistentPaths::EnsureLayout();
    if (!persistentRoot.empty()) {
        ini_path_ = PersistentPaths::ImGuiIniFile();
        if (!ini_path_.empty()) {
#ifndef KEYBINDER_TESTING
            SeedBundledImGuiIniIfMissing(ini_path_);
#endif
            io.IniFilename = ini_path_.c_str();
            Logger::Info("[ImGuiOverlay] Using ImGui settings file '%s'", io.IniFilename);
        }
    } else {
        Logger::Warning("[ImGuiOverlay] Persistent layout directory unavailable; ImGui will fall back to the process working directory");
    }

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = visible_;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::StyleColorsDark();

    current_scale_ = ComputeAutomaticScale();
    ThemeManager::Instance().ApplyConfiguredTheme(settings_manager_, current_scale_);
    ApplyFontScaleState(current_scale_);
    Logger::Info("[ImGuiOverlay] Applied automatic UI scale: %.2f", current_scale_);

    RebuildFontAtlas();

    return true;
}

void ImGuiOverlay::ApplyFontScaleState(float scale) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const float clampedScale = SettingsManager::ClampScale(scale);
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

    // Fonts are rebuilt at the correct pixel size for clampedScale. Keep both the
    // new and legacy global scale knobs neutral so text metrics and shell layout
    // stay in lockstep with the configured theme style application.
    style.FontScaleMain = 1.0f;
    io.FontGlobalScale = 1.0f;

    Logger::Debug("[ImGuiOverlay] Neutralized global font scaling after atlas rebuild (scale=%.2f)", clampedScale);
}

void ImGuiOverlay::RebuildFontAtlas() {
    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();

    const float baseFontSize = 20.0f;
    const float fontSize = baseFontSize * current_scale_;

    ImFontConfig cfg;
    cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    ImFont* primaryFont = AddFontFromBundledResourceOrFile(
        io.Fonts,
        IDR_FONT_INTER,
        kPrimaryFontFallbackPath,
        fontSize,
        &cfg,
        nullptr,
        "primary font",
        nullptr);

    if (primaryFont != nullptr) {
        io.FontDefault = primaryFont;
    } else {
        io.FontDefault = io.Fonts->AddFontDefault(nullptr);
        Logger::Warning("[ImGuiOverlay] No bundled or loose primary font available; using ImGui default font");
    }

    char latinFallbackPath[MAX_PATH * 4] = {};
    if (ResolveWindowsFontPath(kLatinFallbackFontFileName, latinFallbackPath, sizeof(latinFallbackPath))) {
        const DWORD latinFallbackAttrs = GetFileAttributesA(latinFallbackPath);
        if (latinFallbackAttrs != INVALID_FILE_ATTRIBUTES && (latinFallbackAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            ImFontConfig latinFallbackCfg;
            latinFallbackCfg.MergeMode = true;
            latinFallbackCfg.PixelSnapH = true;
            latinFallbackCfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;

            if (io.Fonts->AddFontFromFileTTF(latinFallbackPath, fontSize, &latinFallbackCfg, nullptr) != nullptr) {
                Logger::Info("[ImGuiOverlay] Merged Latin fallback font '%s' for extended UI glyph coverage", latinFallbackPath);
            } else {
                Logger::Warning("[ImGuiOverlay] Failed to merge Latin fallback font '%s'", latinFallbackPath);
            }
        } else {
            Logger::Warning("[ImGuiOverlay] Latin fallback font not found: '%s'", latinFallbackPath);
        }
    } else {
        Logger::Warning("[ImGuiOverlay] Failed to resolve Windows font path for Latin fallback font");
    }

    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.GlyphMinAdvanceX = fontSize;
    iconCfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    if (AddFontFromBundledResourceOrFile(
            io.Fonts,
            IDR_FONT_MATERIAL_SYMBOLS,
            kMaterialSymbolsFontFallbackPath,
            fontSize,
            &iconCfg,
            kMaterialSymbolsGlyphRanges,
            "Material Symbols icon font",
            nullptr) == nullptr) {
        Logger::Warning("[ImGuiOverlay] Material Symbols icon font unavailable; continuing without merged icon glyphs");
    }

    io.Fonts->Build();

    font_atlas_dirty_ = false;
    Logger::Info("[ImGuiOverlay] Font atlas rebuilt at scale %.2f", current_scale_);
}

bool ImGuiOverlay::ShouldCaptureMessage(bool visible, UINT msg, WPARAM w_param) const {
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
        if (visible) {
            return true;
        }
        break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCOMMAND:
        break;
    default:
        return false;
    }

    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
        return io.WantCaptureMouse;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
        return io.WantCaptureKeyboard || io.WantTextInput;
    case WM_SYSCOMMAND:
        return (w_param & 0xFFF0U) == SC_KEYMENU;
    default:
        return false;
    }
}

#ifndef KEYBINDER_TESTING
BOOL WINAPI ImGuiOverlay::HookedSetCursorPos(int x, int y) {
    // During shutdown, pass through to original immediately to avoid crashes
    if (active_instance_ != nullptr && active_instance_->exit_started_) {
        if (set_cursor_pos_hook_) {
            return set_cursor_pos_hook_->stdcall<BOOL, int, int>(x, y);
        }
        return ::SetCursorPos(x, y);
    }

    if (active_instance_ != nullptr && active_instance_->visible_) {
        Logger::Debug("[CURSOR_DEBUG] Blocked SetCursorPos(%d, %d) - overlay visible", x, y);
        return TRUE;
    }

    if (set_cursor_pos_hook_) {
        return set_cursor_pos_hook_->stdcall<BOOL, int, int>(x, y);
    }
    return ::SetCursorPos(x, y);
}

HRESULT WINAPI ImGuiOverlay::HookedDirectInputGetDeviceState(void* device, DWORD dataSize, void* data) {
    HRESULT hr = DIERR_GENERIC;
    if (direct_input_get_device_state_hook_ != nullptr) {
        hr = direct_input_get_device_state_hook_->stdcall<HRESULT, void*, DWORD, void*>(device, dataSize, data);
    }

    DWORD deviceType = 0;
    if (SUCCEEDED(hr)
        && active_instance_ != nullptr
        && !active_instance_->exit_started_
        && TryGetDirectInputDeviceType(device, deviceType)) {
        if (deviceType == DI8DEVTYPE_MOUSE) {
            if (active_instance_->visible_) {
                SuppressDirectInputMouseState(dataSize, data);
            } else if (active_instance_->IsQuickMapWheelCaptureActive()) {
                SuppressDirectInputMouseWheelState(dataSize, data);
            }
        } else if (deviceType == DI8DEVTYPE_KEYBOARD && active_instance_->IsKeyboardCaptureActive()) {
            if (data != nullptr && dataSize > 0) {
                std::memset(data, 0, dataSize);
            }
        }
    }

    return hr;
}

HRESULT WINAPI ImGuiOverlay::HookedDirectInputGetDeviceData(void* device, DWORD objectDataSize, void* objectData, DWORD* inOutCount, DWORD flags) {
    HRESULT hr = DIERR_GENERIC;
    if (direct_input_get_device_data_hook_ != nullptr) {
        hr = direct_input_get_device_data_hook_->stdcall<HRESULT, void*, DWORD, void*, DWORD*, DWORD>(device, objectDataSize, objectData, inOutCount, flags);
    }

    DWORD deviceType = 0;
    if (SUCCEEDED(hr)
        && active_instance_ != nullptr
        && !active_instance_->exit_started_
        && TryGetDirectInputDeviceType(device, deviceType)) {
        if (deviceType == DI8DEVTYPE_MOUSE) {
            if (active_instance_->visible_) {
                SuppressDirectInputMouseEvents(objectDataSize, objectData, inOutCount);
            } else if (active_instance_->IsQuickMapWheelCaptureActive()) {
                SuppressDirectInputMouseWheelEvents(objectDataSize, objectData, inOutCount);
            }
        } else if (deviceType == DI8DEVTYPE_KEYBOARD && active_instance_->IsKeyboardCaptureActive()) {
            if (objectData != nullptr && objectDataSize != 0 && inOutCount != nullptr && *inOutCount != 0) {
                std::memset(objectData, 0, static_cast<std::size_t>(objectDataSize) * static_cast<std::size_t>(*inOutCount));
            }
            if (inOutCount != nullptr) {
                *inOutCount = 0;
            }
        }
    }

    return hr;
}

void __fastcall ImGuiOverlay::HookedChatDraw(void* chat, void* /*edx*/) {
    if (chat_draw_hook_ != nullptr) {
        chat_draw_hook_->thiscall<void>(chat);
    }

    if (active_instance_ != nullptr) {
        active_instance_->RenderFrame(true);
    }
}

bool ImGuiOverlay::InstallSetCursorPosHook() {
    if (set_cursor_pos_hook_) {
        return true;
    }
    
    Logger::Info("[ImGuiOverlay] Installing SetCursorPos hook");
    
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to get user32.dll handle");
        return false;
    }
    
    auto* set_cursor_pos_ptr = reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos"));
    if (set_cursor_pos_ptr == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to get SetCursorPos address");
        return false;
    }
    
    auto hook = safetyhook::InlineHook::create(set_cursor_pos_ptr, reinterpret_cast<void*>(&HookedSetCursorPos));
    if (!hook) {
        Logger::Error("[ImGuiOverlay] Failed to create SetCursorPos hook: %d", hook.error().type);
        return false;
    }
    
    set_cursor_pos_hook_ = new safetyhook::InlineHook(std::move(*hook));
    Logger::Info("[ImGuiOverlay] SetCursorPos hook installed successfully");
    return true;
}

void ImGuiOverlay::RemoveSetCursorPosHook() {
    if (set_cursor_pos_hook_) {
        Logger::Info("[ImGuiOverlay] Removing SetCursorPos hook");
        delete set_cursor_pos_hook_;
        set_cursor_pos_hook_ = nullptr;
    }
}

bool ImGuiOverlay::InstallDirectInputMouseHooks() {
    if (direct_input_get_device_state_hook_ != nullptr && direct_input_get_device_data_hook_ != nullptr) {
        return true;
    }

    Logger::Info("[ImGuiOverlay] Installing DirectInput mouse hooks");

    HMODULE dinput8 = GetModuleHandleW(L"dinput8.dll");
    if (dinput8 == nullptr) {
        dinput8 = LoadLibraryW(L"dinput8.dll");
    }
    if (dinput8 == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to load dinput8.dll");
        return false;
    }

    auto* directInput8Create = reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(dinput8, "DirectInput8Create"));
    if (directInput8Create == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to resolve DirectInput8Create");
        return false;
    }

    IDirectInput8A* directInput = nullptr;
    HRESULT hr = directInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, kDirectInput8InterfaceId, reinterpret_cast<void**>(&directInput), nullptr);
    if (FAILED(hr) || directInput == nullptr) {
        Logger::Error("[ImGuiOverlay] DirectInput8Create failed: 0x%08X", hr);
        return false;
    }

    IDirectInputDevice8A* mouseDevice = nullptr;
    hr = directInput->CreateDevice(kDirectInputSystemMouseGuid, &mouseDevice, nullptr);
    if (FAILED(hr) || mouseDevice == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to create system mouse device: 0x%08X", hr);
        directInput->Release();
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(mouseDevice);
    void* getDeviceStatePtr = vtable[kDirectInputGetDeviceStateVtableIndex];
    void* getDeviceDataPtr = vtable[kDirectInputGetDeviceDataVtableIndex];

    mouseDevice->Release();
    directInput->Release();

    if (getDeviceStatePtr == nullptr || getDeviceDataPtr == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to resolve DirectInput mouse vtable slots");
        return false;
    }

    auto stateHook = safetyhook::InlineHook::create(getDeviceStatePtr, reinterpret_cast<void*>(&HookedDirectInputGetDeviceState));
    if (!stateHook) {
        Logger::Error("[ImGuiOverlay] Failed to create DirectInput GetDeviceState hook: %d", stateHook.error().type);
        return false;
    }

    auto dataHook = safetyhook::InlineHook::create(getDeviceDataPtr, reinterpret_cast<void*>(&HookedDirectInputGetDeviceData));
    if (!dataHook) {
        Logger::Error("[ImGuiOverlay] Failed to create DirectInput GetDeviceData hook: %d", dataHook.error().type);
        return false;
    }

    direct_input_get_device_state_hook_ = new safetyhook::InlineHook(std::move(*stateHook));
    direct_input_get_device_data_hook_ = new safetyhook::InlineHook(std::move(*dataHook));
    Logger::Info("[ImGuiOverlay] DirectInput mouse hooks installed successfully");
    return true;
}

void ImGuiOverlay::RemoveDirectInputMouseHooks() {
    if (direct_input_get_device_state_hook_ != nullptr) {
        Logger::Info("[ImGuiOverlay] Removing DirectInput GetDeviceState hook");
        delete direct_input_get_device_state_hook_;
        direct_input_get_device_state_hook_ = nullptr;
    }

    if (direct_input_get_device_data_hook_ != nullptr) {
        Logger::Info("[ImGuiOverlay] Removing DirectInput GetDeviceData hook");
        delete direct_input_get_device_data_hook_;
        direct_input_get_device_data_hook_ = nullptr;
    }
}

bool ImGuiOverlay::InstallChatDrawHook() {
    if (chat_draw_hook_) {
        return true;
    }

    const unsigned long sampBase = sampapi::GetBase();
    if (sampBase == 0) {
        Logger::Warning("[ImGuiOverlay] Cannot install CChat::Draw hook because samp.dll is not loaded yet");
        return false;
    }

    void* chat_draw_ptr = reinterpret_cast<void*>(sampapi::GetAddress(0x67870));
    if (chat_draw_ptr == nullptr) {
        Logger::Error("[ImGuiOverlay] Failed to resolve CChat::Draw address");
        return false;
    }

    Logger::Info("[ImGuiOverlay] Installing CChat::Draw hook at %p", chat_draw_ptr);
    auto hook = safetyhook::InlineHook::create(chat_draw_ptr, reinterpret_cast<void*>(&HookedChatDraw));
    if (!hook) {
        Logger::Error("[ImGuiOverlay] Failed to create CChat::Draw hook: %d", hook.error().type);
        return false;
    }

    chat_draw_hook_ = new safetyhook::InlineHook(std::move(*hook));
    Logger::Info("[ImGuiOverlay] CChat::Draw hook installed successfully");
    return true;
}

void ImGuiOverlay::RemoveChatDrawHook() {
    if (chat_draw_hook_) {
        Logger::Info("[ImGuiOverlay] Removing CChat::Draw hook");
        delete chat_draw_hook_;
        chat_draw_hook_ = nullptr;
    }
}
#else
bool ImGuiOverlay::InstallSetCursorPosHook() {
    return true;
}

void ImGuiOverlay::RemoveSetCursorPosHook() {
}

bool ImGuiOverlay::InstallDirectInputMouseHooks() {
    return true;
}

void ImGuiOverlay::RemoveDirectInputMouseHooks() {
}

bool ImGuiOverlay::InstallChatDrawHook() {
    return true;
}

void ImGuiOverlay::RemoveChatDrawHook() {
}
#endif

void ImGuiOverlay::SetSettingsManager(SettingsManager* manager) {
    settings_manager_ = manager;
}

void ImGuiOverlay::SetSAMPBridge(SAMPBridge* bridge) {
    samp_bridge_ = bridge;
}

void ImGuiOverlay::SetEarlyKeybindHandler(std::function<bool(int, bool)> callback) {
    earlyKeybindHandlerCallback_ = std::move(callback);
}

void ImGuiOverlay::SetFocusLostHandler(std::function<void()> callback) {
    focusLostHandlerCallback_ = std::move(callback);
}

void ImGuiOverlay::SetAuxiliaryInteractionActiveCallback(std::function<bool()> callback) {
    auxiliaryInteractionActiveCallback_ = std::move(callback);
}

void ImGuiOverlay::SetAuxiliaryInteractionCancelCallback(std::function<void()> callback) {
    auxiliaryInteractionCancelCallback_ = std::move(callback);
}

bool ImGuiOverlay::ApplyDpiScale(float scale) {
    if (!initialized_) {
        return false;
    }

    scale = SettingsManager::ClampScale(scale);

    bool was_cursor_shown = cursor_shown_;

    current_scale_ = scale;
    RebuildFontAtlas();
    ThemeManager::Instance().ApplyConfiguredTheme(settings_manager_, current_scale_);
    ApplyFontScaleState(current_scale_);

    if (dependencies_.imgui_dx9_invalidate_device_objects != nullptr) {
        dependencies_.imgui_dx9_invalidate_device_objects();
    }
    if (dependencies_.imgui_dx9_create_device_objects != nullptr) {
        if (!dependencies_.imgui_dx9_create_device_objects()) {
            Logger::Error("[ImGuiOverlay] Failed to recreate DX9 device objects after DPI change");
            return false;
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    if (visible_ && was_cursor_shown) {
        io.MouseDrawCursor = true;
        SyncImGuiCursorVisibility(true);
        if (dependencies_.show_cursor != nullptr) {
            dependencies_.show_cursor(true);
        }
    }

    Logger::Info("[ImGuiOverlay] Applied DPI scale: %.2f (rebuilt font atlas)", scale);
    return true;
}

}
