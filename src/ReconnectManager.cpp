#include "../include/ReconnectManager.h"
#include "../include/SettingsManager.h"
#include "../include/SAMPBridge.h"
#include "../include/PersistentPaths.h"
#include "../include/ResourceIds.h"
#include "../include/Logger.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstring>
#include <windows.h>
#include <shellapi.h>
#include <sampapi/0.3.DL-1/Commands.h>

namespace keybinder {
namespace {

bool quitRequested = false;
unsigned long long quitDeadline = 0;
constexpr unsigned long long kQuitTimeoutMilliseconds = 8000;

#ifdef KEYBINDER_TESTING
ReconnectTestingHooks testingHooks;
#else
HANDLE preparedWorker = nullptr;

void StopWorker(HANDLE worker) {
    if (!worker) return;
    if (WaitForSingleObject(worker, 0) == WAIT_TIMEOUT) {
        // This handle belongs only to the helper started by this command.
        TerminateProcess(worker, 1);
        WaitForSingleObject(worker, 1000);
    }
    CloseHandle(worker);
}
#endif

unsigned long long NowMilliseconds() {
#ifdef KEYBINDER_TESTING
    if (testingHooks.nowMilliseconds) return testingHooks.nowMilliseconds();
#endif
    return GetTickCount64();
}

std::wstring Widen(const std::string& value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

bool IsExecutable(const std::wstring& path) {
    if (path.size() < 5 || path.find(L'\0') != std::wstring::npos) return false;
    std::wstring suffix = path.substr(path.size() - 4);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::towlower);
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return suffix == L".exe" && attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring EnvironmentPath(const wchar_t* name) {
    wchar_t value[32768] = {};
    const DWORD length = GetEnvironmentVariableW(name, value, 32768);
    return length > 0 && length < 32768 ? std::wstring(value, length) : std::wstring();
}

std::wstring FindLauncher(const Settings& settings) {
    const std::wstring configured = Widen(settings.reconnect_use_nova_launcher
        ? settings.reconnect_nova_launcher_path : settings.reconnect_openmp_launcher_path);
    if (!configured.empty()) return configured;
    if (settings.reconnect_use_nova_launcher) {
        return EnvironmentPath(L"LOCALAPPDATA") + L"\\Nova Legacy Launcher\\nova-legacy-launcher.exe";
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\omp\\shell\\open\\command", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        wchar_t command[32768] = {};
        DWORD size = sizeof(command);
        const LONG result = RegQueryValueExW(key, nullptr, nullptr, nullptr, reinterpret_cast<BYTE*>(command), &size);
        RegCloseKey(key);
        if (result == ERROR_SUCCESS) {
            int count = 0;
            wchar_t** arguments = CommandLineToArgvW(command, &count);
            std::wstring executable;
            if (arguments && count > 0) executable = arguments[0];
            if (arguments) LocalFree(arguments);
            if (IsExecutable(executable)) return executable;
        }
    }
    return EnvironmentPath(L"ProgramFiles(x86)") + L"\\Open Multiplayer Launcher\\omp-launcher.exe";
}

#ifndef KEYBINDER_TESTING
bool ExtractWorker(std::wstring& path, std::string& error) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&PrepareReconnect), &module)) {
        error = "Reconnect-Modul konnte nicht gefunden werden.";
        return false;
    }
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_RECONNECT_HELPER), MAKEINTRESOURCEW(10));
    HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
    const DWORD size = resource ? SizeofResource(module, resource) : 0;
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    if (!bytes || size == 0) { error = "Reconnect-Helfer fehlt in der ASI."; return false; }
    const std::string root = PersistentPaths::EnsureLayout();
    if (root.empty()) { error = "NLBinder-Datenordner konnte nicht angelegt werden."; return false; }
    // PersistentPaths uses the Windows ANSI profile path, so retain its encoding here.
    const int length = MultiByteToWideChar(CP_ACP, 0, root.c_str(), -1, nullptr, 0);
    std::wstring directory(length, L'\0');
    MultiByteToWideChar(CP_ACP, 0, root.c_str(), -1, directory.data(), length);
    if (!directory.empty()) directory.pop_back();
    path = directory + L"\\NLBinderReconnect.exe";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = "Reconnect-Helfer konnte nicht geschrieben werden."; return false; }
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes, size, &written, nullptr) && written == size;
    CloseHandle(file);
    if (!ok) error = "Reconnect-Helfer konnte nicht gespeichert werden.";
    return ok;
}
#endif

} // namespace

bool BuildReconnectPlan(ReconnectLauncher launcher, const std::wstring& executable,
                        const std::wstring& gameDirectory, const std::string& host,
                        int port, const std::string& nickname,
                        ReconnectLaunchPlan& plan, std::string& error) {
    plan = {};
    error.clear();
    const bool validHost = !host.empty() && host.size() <= 253
        && std::all_of(host.begin(), host.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '.' || ch == '-' || ch == ':';
        });
    const bool validNickname = !nickname.empty() && nickname.size() <= 24
        && std::all_of(nickname.begin(), nickname.end(), [](unsigned char ch) { return ch > 32 && ch < 127; });
    if (executable.empty() || executable.find(L'\0') != std::wstring::npos || gameDirectory.empty()
        || !validHost || port < 1 || port > 65535 || !validNickname) {
        error = "Launcher, Serveradresse oder Spielername ist nicht verfügbar.";
        return false;
    }
    plan.launcher = launcher;
    plan.executable = executable;
    const auto separator = executable.find_last_of(L"\\/");
    plan.workingDirectory = separator == std::wstring::npos ? gameDirectory
        : executable.substr(0, separator == 2 && executable[1] == L':' ? 3 : separator);
    if (launcher == ReconnectLauncher::OpenMp) {
        plan.arguments = {L"-h", Widen(host), L"-p", std::to_wstring(port), L"-n", Widen(nickname), L"-g", gameDirectory};
    }
    return true;
}

bool PrepareReconnect(const Settings& settings, SAMPBridge& bridge, std::string& error) {
#ifdef KEYBINDER_TESTING
    if (testingHooks.prepare) return testingHooks.prepare(error);
    error = "Spielneustart ist im Testmodus deaktiviert.";
    return false;
#else
    if (preparedWorker != nullptr) {
        if (WaitForSingleObject(preparedWorker, 0) == WAIT_TIMEOUT) {
            error = "Ein Reconnect ist bereits vorbereitet.";
            return false;
        }
        CloseHandle(preparedWorker);
        preparedWorker = nullptr;
    }
    auto* netGame = bridge.GetNetGame();
    if (!netGame || netGame->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED) {
        error = "Du bist aktuell mit keinem Server verbunden.";
        return false;
    }
    const std::wstring launcher = FindLauncher(settings);
    if (!IsExecutable(launcher)) {
        error = "Launcher nicht gefunden. Bitte den Launcher-Pfad unter General eintragen.";
        return false;
    }
    wchar_t gamePath[32768] = {};
    const DWORD length = GetModuleFileNameW(nullptr, gamePath, 32768);
    if (length == 0 || length >= 32768) { error = "Spielordner konnte nicht gelesen werden."; return false; }
    std::wstring gameDirectory(gamePath, length);
    const auto separator = gameDirectory.find_last_of(L"\\/");
    if (separator == std::wstring::npos) { error = "Spielordner ist ungültig."; return false; }
    gameDirectory.resize(separator);
    ReconnectLaunchPlan plan;
    if (!BuildReconnectPlan(settings.reconnect_use_nova_launcher ? ReconnectLauncher::Nova : ReconnectLauncher::OpenMp,
        launcher, gameDirectory, std::string(netGame->m_szHostAddress, strnlen(netGame->m_szHostAddress, 257)),
        netGame->m_nPort, bridge.GetLocalPlayerName(), plan, error)) return false;
    std::wstring worker;
    if (!ExtractWorker(worker, error)) return false;
    const std::wstring eventName = L"Local\\NLBinderReconnect-" + std::to_wstring(GetCurrentProcessId())
        + L"-" + std::to_wstring(GetTickCount64());
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    if (!ready) { error = "Reconnect konnte nicht vorbereitet werden."; return false; }
    std::vector<std::wstring> arguments = {L"--parent", std::to_wstring(GetCurrentProcessId()),
        L"--ready", eventName, L"--mode", settings.reconnect_use_nova_launcher ? L"nova" : L"openmp",
        L"--launcher", plan.executable, L"--working-directory", plan.workingDirectory};
    arguments.insert(arguments.end(), plan.arguments.begin(), plan.arguments.end());
    std::wstring command = ReconnectCommandLine(worker, arguments);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    const bool created = CreateProcessW(worker.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, plan.workingDirectory.c_str(), &startup, &process) != FALSE;
    if (!created) {
        CloseHandle(ready);
        error = "Reconnect-Helfer konnte nicht gestartet werden.";
        return false;
    }
    CloseHandle(process.hThread);
    HANDLE handles[] = {ready, process.hProcess};
    const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, 4000);
    CloseHandle(ready);
    if (wait != WAIT_OBJECT_0) {
        StopWorker(process.hProcess);
        error = "Reconnect-Helfer ist nicht bereit. Das Spiel bleibt geöffnet.";
        return false;
    }
    preparedWorker = process.hProcess;
    Logger::Info("[Reconnect] Worker ready; launcher=%s", settings.reconnect_use_nova_launcher ? "Nova" : "OpenMP");
    return true;
#endif
}

void CancelPreparedReconnect() {
    quitRequested = false;
    quitDeadline = 0;
#ifdef KEYBINDER_TESTING
    if (testingHooks.cancel) testingHooks.cancel();
#else
    StopWorker(preparedWorker);
    preparedWorker = nullptr;
#endif
}

bool RequestReconnect(const Settings& settings, SAMPBridge& bridge,
                      const std::function<bool()>& saveState, std::string& error) {
    error.clear();
    if (quitRequested) {
        error = "Das Spiel wird bereits für den Reconnect beendet.";
        return false;
    }
    if (!PrepareReconnect(settings, bridge, error)) return false;

    if (!saveState || !saveState()) {
        CancelPreparedReconnect();
        error = "Binder-Daten konnten nicht gespeichert werden. Reconnect abgebrochen.";
        return false;
    }

    // CInput::Send forwards slash commands to the server. It does not dispatch
    // local commands such as /q, so it cannot close the game for this worker.
    // Call exactly the local handler used by the supported SA-MP client.
    Logger::Info("[Reconnect] State saved; invoking local client quit");
    quitRequested = true;
    quitDeadline = NowMilliseconds() + kQuitTimeoutMilliseconds;
#ifdef KEYBINDER_TESTING
    if (!testingHooks.quitClient) {
        CancelPreparedReconnect();
        error = "Spielneustart ist im Testmodus deaktiviert.";
        return false;
    }
    testingHooks.quitClient();
#else
    sampapi::v03dl::Commands::Quit("");
#endif
    return true;
}

bool UpdateReconnect(std::string& error) {
    if (!quitRequested || NowMilliseconds() < quitDeadline) return false;
    // The quit handler may request an asynchronous exit. Only cancel if the
    // game is still processing frames well after the normal exit request.
    CancelPreparedReconnect();
    error = "Spiel konnte nicht beendet werden. Reconnect abgebrochen; erneuter Versuch ist möglich.";
    Logger::Warning("[Reconnect] Local quit did not close the game; worker cancelled");
    return true;
}

#ifdef KEYBINDER_TESTING
void SetReconnectTestingHooks(ReconnectTestingHooks hooks) {
    testingHooks = std::move(hooks);
}

void ResetReconnectTestingHooks() {
    testingHooks = {};
    quitRequested = false;
    quitDeadline = 0;
}
#endif

} // namespace keybinder
