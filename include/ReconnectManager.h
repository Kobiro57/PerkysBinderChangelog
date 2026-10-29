#pragma once

#include <string>
#include <vector>
#include <functional>

class SAMPBridge;

namespace keybinder {

struct Settings;

enum class ReconnectLauncher { Nova, OpenMp };

struct ReconnectLaunchPlan {
    ReconnectLauncher launcher = ReconnectLauncher::Nova;
    std::wstring executable;
    std::wstring workingDirectory;
    std::wstring gameExecutable;
    std::vector<std::wstring> arguments;
};

// Quote one argv element for CreateProcess/CommandLineToArgvW. No shell is used.
inline std::wstring QuoteReconnectArgument(const std::wstring& argument) {
    std::wstring quoted = L"\"";
    std::size_t slashes = 0;
    for (wchar_t ch : argument) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'\"') {
            quoted.append(slashes * 2 + 1, L'\\');
        } else {
            quoted.append(slashes, L'\\');
        }
        slashes = 0;
        quoted += ch;
    }
    quoted.append(slashes * 2, L'\\');
    quoted += L'\"';
    return quoted;
}

inline std::wstring ReconnectCommandLine(const std::wstring& executable,
                                        const std::vector<std::wstring>& arguments) {
    std::wstring command = QuoteReconnectArgument(executable);
    for (const auto& argument : arguments) command += L" " + QuoteReconnectArgument(argument);
    return command;
}

bool BuildReconnectPlan(ReconnectLauncher launcher, const std::wstring& executable,
                        const std::wstring& gameDirectory, const std::string& host,
                        int port, const std::string& nickname,
                        ReconnectLaunchPlan& plan, std::string& error);

// Starts and handshakes with a worker that waits for this game process to exit.
// Only a ready worker may be followed by saving state and quitting the client.
bool PrepareReconnect(const Settings& settings, SAMPBridge& bridge, std::string& error);
void CancelPreparedReconnect();

// Saves state and invokes the local SA-MP quit handler. The client may finish
// quitting on a later frame; UpdateReconnect cancels a request that stalls.
bool RequestReconnect(const Settings& settings, SAMPBridge& bridge,
                      const std::function<bool()>& saveState, std::string& error);
bool UpdateReconnect(std::string& error);

// Called only after the replacement client's local player has actually spawned.
// No action is taken if no reconnect worker is awaiting this exact process.
void NotifyReconnectGameEntered();

#ifdef KEYBINDER_TESTING
struct ReconnectTestingHooks {
    std::function<bool(std::string&)> prepare;
    std::function<void()> quitClient;
    std::function<void()> cancel;
    std::function<unsigned long long()> nowMilliseconds;
};
void SetReconnectTestingHooks(ReconnectTestingHooks hooks);
void ResetReconnectTestingHooks();
#endif

} // namespace keybinder
