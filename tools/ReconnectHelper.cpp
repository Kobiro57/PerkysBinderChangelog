// Launched only by the user's /reconnect command. It never terminates the game.
#include "../include/ReconnectManager.h"
#include <windows.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <string>
#include <vector>

namespace {

struct WorkerOptions {
    DWORD parentId = 0;
    std::wstring readyEvent;
    std::wstring mode;
    std::wstring launcher;
    std::wstring directory;
    std::vector<std::wstring> arguments;
};

bool ParseOptions(int count, wchar_t** arguments, WorkerOptions& options) {
    int index = 1;
    while (index < count && wcslen(arguments[index]) >= 2 && arguments[index][0] == L'-' && arguments[index][1] == L'-') {
        const std::wstring key = arguments[index++];
        if (index >= count) return false;
        const std::wstring value = arguments[index++];
        if (key == L"--parent") {
            wchar_t* end = nullptr;
            const unsigned long id = wcstoul(value.c_str(), &end, 10);
            if (!end || *end || id == 0) return false;
            options.parentId = id;
        } else if (key == L"--ready") options.readyEvent = value;
        else if (key == L"--mode") options.mode = value;
        else if (key == L"--launcher") options.launcher = value;
        else if (key == L"--working-directory") options.directory = value;
        else return false;
    }
    for (; index < count; ++index) options.arguments.emplace_back(arguments[index]);
    return options.parentId != 0 && !options.readyEvent.empty() && !options.launcher.empty()
        && (options.mode == L"nova" || options.mode == L"openmp");
}

void NotifyFailure(const wchar_t* text) {
    MessageBoxW(nullptr, text, L"NL-BINDER – Reconnect", MB_OK | MB_ICONINFORMATION);
}

bool Launch(const WorkerOptions& options) {
    std::wstring command = keybinder::ReconnectCommandLine(options.launcher, options.arguments);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(options.launcher.c_str(), command.data(), nullptr, nullptr, FALSE,
        0, nullptr, options.directory.empty() ? nullptr : options.directory.c_str(), &startup, &process)) return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

struct WindowSearch {
    const std::wstring* executable;
    HWND window = nullptr;
    bool ambiguous = false;
};

BOOL CALLBACK FindNovaWindow(HWND window, LPARAM data) {
    auto& search = *reinterpret_cast<WindowSearch*>(data);
    if (GetWindow(window, GW_OWNER) || GetWindowTextLengthW(window) == 0) return TRUE;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return TRUE;
    wchar_t executable[32768] = {};
    DWORD length = 32768;
    const bool match = QueryFullProcessImageNameW(process, 0, executable, &length)
        && _wcsicmp(executable, search.executable->c_str()) == 0;
    CloseHandle(process);
    if (match) {
        if (search.window != nullptr) search.ambiguous = true;
        else search.window = window;
    }
    return TRUE;
}

bool InvokeNovaPlay(IUIAutomation* automation, HWND window) {
    IUIAutomationElement* root = nullptr;
    if (FAILED(automation->ElementFromHandle(window, &root)) || !root) return false;
    // Invoke only the normal game-start button. Do not automate authentication,
    // updates, anti-cheat options, permission dialogs or other launcher settings.
    VARIANT buttonType;
    VariantInit(&buttonType);
    buttonType.vt = VT_I4;
    buttonType.lVal = UIA_ButtonControlTypeId;
    IUIAutomationCondition* typeCondition = nullptr;
    automation->CreatePropertyCondition(UIA_ControlTypePropertyId, buttonType, &typeCondition);
    VARIANT buttonName;
    VariantInit(&buttonName);
    buttonName.vt = VT_BSTR;
    buttonName.bstrVal = SysAllocString(L"Spiel starten");
    IUIAutomationCondition* nameCondition = nullptr;
    automation->CreatePropertyCondition(UIA_NamePropertyId, buttonName, &nameCondition);
    VariantClear(&buttonName);
    IUIAutomationCondition* condition = nullptr;
    if (typeCondition && nameCondition) automation->CreateAndCondition(typeCondition, nameCondition, &condition);
    IUIAutomationElementArray* matches = nullptr;
    if (condition) root->FindAll(TreeScope_Descendants, condition, &matches);
    int count = 0;
    if (matches) matches->get_Length(&count);
    bool invoked = false;
    if (count == 1) {
        IUIAutomationElement* button = nullptr;
        matches->GetElement(0, &button);
        BOOL enabled = FALSE;
        if (button && SUCCEEDED(button->get_CurrentIsEnabled(&enabled)) && enabled) {
            IUIAutomationInvokePattern* invoke = nullptr;
            if (SUCCEEDED(button->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke))) && invoke) {
                invoked = SUCCEEDED(invoke->Invoke());
                invoke->Release();
            }
        }
        if (button) button->Release();
    }
    if (matches) matches->Release();
    if (condition) condition->Release();
    if (nameCondition) nameCondition->Release();
    if (typeCondition) typeCondition->Release();
    root->Release();
    return invoked;
}

bool StartNovaGame(const std::wstring& executable) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return false;
    IUIAutomation* automation = nullptr;
    const HRESULT created = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&automation));
    bool invoked = false;
    if (SUCCEEDED(created) && automation) {
        const ULONGLONG deadline = GetTickCount64() + 20000;
        while (GetTickCount64() < deadline) {
            WindowSearch search{&executable};
            EnumWindows(FindNovaWindow, reinterpret_cast<LPARAM>(&search));
            if (search.window && !search.ambiguous && InvokeNovaPlay(automation, search.window)) {
                invoked = true;
                break;
            }
            Sleep(250);
        }
        automation->Release();
    }
    CoUninitialize();
    return invoked;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    WorkerOptions options;
    const bool valid = arguments && ParseOptions(count, arguments, options);
    if (arguments) LocalFree(arguments);
    if (!valid) return 2;
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, options.parentId);
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, options.readyEvent.c_str());
    const DWORD attributes = GetFileAttributesW(options.launcher.c_str());
    if (!parent || !ready || attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        if (parent) CloseHandle(parent);
        if (ready) CloseHandle(ready);
        return 3;
    }
    SetEvent(ready);
    CloseHandle(ready);
    // A failed /q must never lead to a second live game instance later.
    const DWORD wait = WaitForSingleObject(parent, 60000);
    CloseHandle(parent);
    if (wait != WAIT_OBJECT_0) return 4;
    if (!Launch(options)) {
        NotifyFailure(L"Der ausgewählte Launcher konnte nicht gestartet werden. Bitte den Launcher-Pfad unter General prüfen.");
        return 5;
    }
    if (options.mode == L"nova" && !StartNovaGame(options.launcher)) {
        NotifyFailure(L"Nova Launcher wurde geöffnet. Der normale Knopf ‚Spiel starten‘ konnte nicht automatisch betätigt werden. Bitte im Launcher anmelden oder das Spiel dort starten.");
        return 6;
    }
    return 0;
}
