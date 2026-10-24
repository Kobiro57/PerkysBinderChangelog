#include "../include/ReconnectManager.h"
#include "../include/SettingsManager.h"
#include "../include/SAMPBridge.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int failures = 0;
int checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] Reconnect: %s (line %d)\n", #value, __LINE__); } } while (0)

void TestOpenMpPlanAndArgumentQuoting() {
    keybinder::ReconnectLaunchPlan plan;
    std::string error;
    CHECK(keybinder::BuildReconnectPlan(keybinder::ReconnectLauncher::OpenMp,
        L"C:\\Program Files (x86)\\Open Multiplayer Launcher\\omp-launcher.exe",
        L"C:\\Games\\Grand Theft Auto\\", "samp.nova-legacy.de", 7777, "[RCB]Player", plan, error));
    CHECK(error.empty());
    CHECK(plan.arguments.size() == 8);
    CHECK(plan.arguments[0] == L"-h");
    CHECK(plan.arguments[1] == L"samp.nova-legacy.de");
    CHECK(plan.arguments[2] == L"-p");
    CHECK(plan.arguments[3] == L"7777");
    CHECK(plan.arguments[4] == L"-n");
    CHECK(plan.arguments[5] == L"[RCB]Player");
    CHECK(plan.arguments[6] == L"-g");
    CHECK(plan.arguments[7] == L"C:\\Games\\Grand Theft Auto\\");
    CHECK(plan.workingDirectory == L"C:\\Program Files (x86)\\Open Multiplayer Launcher");

    const std::vector<std::wstring> values = {L"", L"normal", L"with spaces", L"C:\\Games\\", L"a\\\"b", L"a\\\\\"b", L"& % ; $ not a shell"};
    const std::wstring command = keybinder::ReconnectCommandLine(plan.executable, values);
    int count = 0;
    wchar_t** parsed = CommandLineToArgvW(command.c_str(), &count);
    CHECK(parsed != nullptr);
    CHECK(count == static_cast<int>(values.size()) + 1);
    if (parsed && count == static_cast<int>(values.size()) + 1) {
        CHECK(std::wstring(parsed[0]) == plan.executable);
        for (std::size_t i = 0; i < values.size(); ++i) CHECK(std::wstring(parsed[i + 1]) == values[i]);
    }
    if (parsed) LocalFree(parsed);
}

void TestNovaUsesNormalLauncherStart() {
    keybinder::ReconnectLaunchPlan plan;
    std::string error;
    CHECK(keybinder::BuildReconnectPlan(keybinder::ReconnectLauncher::Nova,
        L"C:\\User Space\\Nova Legacy Launcher\\nova-legacy-launcher.exe",
        L"C:\\Games\\GTA", "127.0.0.1", 7777, "Player", plan, error));
    CHECK(plan.launcher == keybinder::ReconnectLauncher::Nova);
    CHECK(plan.arguments.empty()); // No invented direct-connect API or bypass of the launcher.
    CHECK(plan.workingDirectory == L"C:\\User Space\\Nova Legacy Launcher");
}

void TestLauncherAtDriveRootKeepsAbsoluteWorkingDirectory() {
    keybinder::ReconnectLaunchPlan plan;
    std::string error;
    CHECK(keybinder::BuildReconnectPlan(keybinder::ReconnectLauncher::OpenMp,
        L"C:\\omp-launcher.exe", L"D:\\Games\\GTA", "127.0.0.1", 7777, "Player", plan, error));
    CHECK(plan.workingDirectory == L"C:\\");
    CHECK(keybinder::BuildReconnectPlan(keybinder::ReconnectLauncher::Nova,
        L"C:/nova-legacy-launcher.exe", L"D:\\Games\\GTA", "127.0.0.1", 7777, "Player", plan, error));
    CHECK(plan.workingDirectory == L"C:/");
}

void TestRejectsIncompleteConnectionData() {
    keybinder::ReconnectLaunchPlan plan;
    std::string error;
    auto valid = [&](const std::string& host, int port, const std::string& nick) {
        return keybinder::BuildReconnectPlan(keybinder::ReconnectLauncher::OpenMp,
            L"C:\\Launcher\\omp-launcher.exe", L"C:\\GTA", host, port, nick, plan, error);
    };
    CHECK(!valid("", 7777, "Player"));
    CHECK(!error.empty());
    CHECK(!valid("localhost", 0, "Player"));
    CHECK(!valid("localhost", 65536, "Player"));
    CHECK(!valid("localhost --other", 7777, "Player"));
    CHECK(!valid("localhost\nother", 7777, "Player"));
    CHECK(!valid("localhost", 7777, ""));
    CHECK(!valid("localhost", 7777, "Player Name"));
    CHECK(!valid("localhost", 7777, std::string(25, 'x')));
    CHECK(valid("127.0.0.1", 1, std::string(24, 'x')));
    CHECK(valid("nova-server.example", 65535, "Player_One"));
}

void TestSettingsRoundTripAndNoRealRestartInTests() {
    keybinder::ResetReconnectTestingHooks();
    for (bool nova : {true, false}) {
        keybinder::SettingsManager settings;
        settings.GetMutable().reconnect_use_nova_launcher = nova;
        settings.GetMutable().reconnect_nova_launcher_path = "C:\\My Launchers\\Nova\\launcher.exe";
        settings.GetMutable().reconnect_openmp_launcher_path = "C:\\My Launchers\\OpenMP\\omp-launcher.exe";
        keybinder::SettingsManager restored;
        CHECK(restored.LoadFromJson(settings.GetSettingsJson()));
        CHECK(restored.Get().reconnect_use_nova_launcher == nova);
        CHECK(restored.Get().reconnect_nova_launcher_path == settings.Get().reconnect_nova_launcher_path);
        CHECK(restored.Get().reconnect_openmp_launcher_path == settings.Get().reconnect_openmp_launcher_path);
        SAMPBridge bridge;
        std::string error;
        CHECK(!keybinder::PrepareReconnect(restored.Get(), bridge, error));
        CHECK(!error.empty());
    }
}

void TestReconnectPreparationFailureKeepsGameOpen() {
    keybinder::Settings settings;
    SAMPBridge bridge;
    std::string error;
    int saved = 0, quit = 0, cancelled = 0;
    keybinder::SetReconnectTestingHooks({[&](std::string& message) {
        message = "Launcher fehlt";
        return false;
    }, [&]() { ++quit; }, [&]() { ++cancelled; }});
    CHECK(!keybinder::RequestReconnect(settings, bridge, [&]() { ++saved; return true; }, error));
    CHECK(error == "Launcher fehlt");
    CHECK(saved == 0);
    CHECK(quit == 0);
    CHECK(cancelled == 0);
    keybinder::ResetReconnectTestingHooks();
}

void TestFailedSaveCancelsPreparedWorker() {
    keybinder::Settings settings;
    SAMPBridge bridge;
    std::string error;
    std::vector<std::string> calls;
    keybinder::SetReconnectTestingHooks({[&](std::string&) {
        calls.push_back("ready"); return true;
    }, [&]() { calls.push_back("quit"); }, [&]() { calls.push_back("cancel"); }});
    CHECK(!keybinder::RequestReconnect(settings, bridge, [&]() {
        calls.push_back("save"); return false;
    }, error));
    CHECK(calls == std::vector<std::string>({"ready", "save", "cancel"}));
    CHECK(error.find("gespeichert") != std::string::npos);
    keybinder::ResetReconnectTestingHooks();
}

void TestDelayedQuitIsAllowedAndStalledQuitAllowsRetry() {
    keybinder::Settings settings;
    SAMPBridge bridge;
    std::string error;
    bool prepared = false;
    unsigned long long clock = 1000;
    std::vector<std::string> calls;
    keybinder::SetReconnectTestingHooks({[&](std::string&) {
        CHECK(!prepared);
        prepared = true; calls.push_back("ready"); return true;
    }, [&]() { calls.push_back("local-quit"); }, [&]() {
        prepared = false; calls.push_back("cancel");
    }, [&]() { return clock; }});
    for (int attempt = 0; attempt < 2; ++attempt) {
        CHECK(keybinder::RequestReconnect(settings, bridge, [&]() {
            calls.push_back("save"); return true;
        }, error));
        CHECK(prepared);
        CHECK(!keybinder::UpdateReconnect(error));
        CHECK(!keybinder::RequestReconnect(settings, bridge, []() { return true; }, error));
        CHECK(error.find("bereits") != std::string::npos);
        clock += 7999;
        CHECK(!keybinder::UpdateReconnect(error));
        CHECK(prepared);
        ++clock;
        CHECK(keybinder::UpdateReconnect(error));
        CHECK(!prepared);
        CHECK(error.find("erneuter Versuch") != std::string::npos);
        CHECK(!keybinder::UpdateReconnect(error));
    }
    CHECK(calls == std::vector<std::string>({"ready", "save", "local-quit", "cancel",
        "ready", "save", "local-quit", "cancel"}));
    keybinder::ResetReconnectTestingHooks();
}

void TestLocalQuitRunsOnlyAfterReadyWorkerAndSavedState() {
    struct SimulatedProcessExit {};
    keybinder::Settings settings;
    SAMPBridge bridge;
    std::string error;
    std::vector<std::string> calls;
    keybinder::SetReconnectTestingHooks({[&](std::string&) {
        calls.push_back("ready"); return true;
    }, [&]() {
        calls.push_back("local-quit");
        throw SimulatedProcessExit{};
    }, [&]() { calls.push_back("cancel"); }});
    bool exited = false;
    try {
        keybinder::RequestReconnect(settings, bridge, [&]() {
            calls.push_back("save"); return true;
        }, error);
    } catch (const SimulatedProcessExit&) {
        exited = true;
    }
    CHECK(exited);
    CHECK(error.empty());
    CHECK(calls == std::vector<std::string>({"ready", "save", "local-quit"}));
    keybinder::ResetReconnectTestingHooks();
}
} // namespace

int RunReconnectTests() {
    failures = 0;
    checks = 0;
    TestOpenMpPlanAndArgumentQuoting();
    TestNovaUsesNormalLauncherStart();
    TestLauncherAtDriveRootKeepsAbsoluteWorkingDirectory();
    TestRejectsIncompleteConnectionData();
    TestSettingsRoundTripAndNoRealRestartInTests();
    TestReconnectPreparationFailureKeepsGameOpen();
    TestFailedSaveCancelsPreparedWorker();
    TestDelayedQuitIsAllowedAndStalledQuitAllowsRetry();
    TestLocalQuitRunsOnlyAfterReadyWorkerAndSavedState();
    std::printf("ReconnectTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
