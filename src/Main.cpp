/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Built with plugin-sdk, SAMP-API, RakHook, and Dear ImGui
*/
#include "../include/plugin-sdk/shared/plugin.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <windows.h>

#include "safetyhook.hpp"

#include "../include/imgui-1.92.7/imgui.h"

#include "../include/ActionExecutor.h"
#include "../include/CrosshairSwitcher.h"
#include "../include/Hitmarker.h"
#include "../include/HitmarkerRenderer.h"
#include "../include/HitsoundPlayer.h"
#include "../include/ImGuiOverlay.h"
#include "../include/InputManager.h"
#include "../include/InterfaceEditor.h"
#include "../include/NativeHudEditor.h"
#include "../include/KeybindManager.h"
#include "../include/Logger.h"
#include "../include/MainWindowUI.h"
#include "../include/OnlinePlayerInfoService.h"
#include "../include/OnlineDataManager.h"
#include "../include/OnlineEventDetector.h"
#include "../include/OnlineOverlayController.h"
#include "../include/OnlineOverlayRenderer.h"
#include "../include/PersistentPaths.h"
#include "../include/ReconnectManager.h"
#include "../include/QuickMap.h"
#include "../include/ResourceIds.h"
#include "../include/RssFeedManager.h"
#include "../include/RssTrackerUI.h"
#include "../include/SAMPBridge.h"
#include "../include/SquareMap.h"
#include "../include/HudSpriteManager.h"
#include "../include/WeaponIconManager.h"
#include "../include/SettingsManager.h"
#include "../include/SpeedometerManager.h"
#include "../include/SpeedometerUI.h"
#include "../include/TextDrawAdapter.h"
#include "../include/ToastNotification.h"
#include "../include/TextureManager.h"
#include "../include/ThemeManager.h"

using namespace plugin;

// Global shutdown phase tracking for crash diagnostics
// 0=running, 1=exit_started, 2=shutdown_event, 3=shutdown_complete
std::atomic<int> g_shutdown_phase{0};

static LPTOP_LEVEL_EXCEPTION_FILTER g_original_exception_filter = nullptr;
static std::string g_crashLogPath;

static bool IsAbsolutePath(const std::string& path) {
    if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return true;
    }

    return path.size() >= 2 && (path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/');
}

static LONG WINAPI KeybinderCrashHandler(EXCEPTION_POINTERS* ep) {
    // Try to log the crash (may fail if crash is severe)
    char crash_buf[512];
    snprintf(crash_buf, sizeof(crash_buf),
        "[CRASH] Exception code=0x%08lX addr=%p phase=%d thread=%lu",
        ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress,
        g_shutdown_phase.load(),
        GetCurrentThreadId());
    
    // Output to debug string (always works even if file I/O fails)
    OutputDebugStringA(crash_buf);
    
    // Try file log
    if (!g_crashLogPath.empty() && IsAbsolutePath(g_crashLogPath)) {
        FILE* f = fopen(g_crashLogPath.c_str(), "a");
        if (f) {
            fprintf(f, "%s\n", crash_buf);
            fclose(f);
        }
    }
    
    if (g_original_exception_filter) {
        return g_original_exception_filter(ep);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void InstallCrashHandler() {
    g_original_exception_filter = SetUnhandledExceptionFilter(KeybinderCrashHandler);
}

static safetyhook::InlineHook* g_exit_process_hook = nullptr;
static safetyhook::InlineHook* g_terminate_process_hook = nullptr;

// SA-MP's /q calls TerminateProcess() which skips ALL cleanup:
// no DllMain DLL_PROCESS_DETACH, no destructors, no atexit.
// By hooking these we intercept the exit BEFORE teardown begins
// and mark the shutdown phase while the CRT is still alive.
// We do NOT attempt full cleanup here — just mark phase so
// destructors know to skip work if DLL_PROCESS_DETACH fires later.

namespace {

void ApplyConfiguredInterfaceEditorSettings(const keybinder::SettingsManager& settingsManager,
                                            keybinder::InterfaceEditor& interfaceEditor) {
    auto& presets = interfaceEditor.GetPresets();
    for (auto& preset : presets) {
        preset = keybinder::InterfacePreset{};
    }

    const auto& savedPresets = settingsManager.Get().interface_editor_saved_presets;
    const std::size_t presetCount = (std::min)(presets.size(), savedPresets.size());
    for (std::size_t i = 0; i < presetCount; ++i) {
        presets[i] = savedPresets[i];
    }

    interfaceEditor.SetActivePreset(settingsManager.Get().interface_editor_active_preset_index);
}

std::string NormalizeUiOpenCommand(std::string command) {
    const std::size_t first = command.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = command.find_last_not_of(" \t\n\r");
    command = command.substr(first, last - first + 1);

    while (!command.empty() && command.front() == '/') {
        command.erase(command.begin());
    }

    const std::size_t separator = command.find_first_of(" \t\n\r");
    if (separator != std::string::npos) {
        command.resize(separator);
    }

    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return command;
}

bool MatchesConfiguredUiOpenCommand(const keybinder::Settings& settings, const std::string& command) {
    const std::string normalized = NormalizeUiOpenCommand(command);
    if (normalized.empty()) {
        return false;
    }

    if (normalized == NormalizeUiOpenCommand(settings.ui_open_command_primary)) {
        return true;
    }

    const std::string secondary = NormalizeUiOpenCommand(settings.ui_open_command_secondary);
    return !secondary.empty() && normalized == secondary;
}

std::string TruncateForSampChat(std::string text, std::size_t maxLength) {
    if (text.size() <= maxLength) {
        return text;
    }

    if (maxLength <= 3) {
        return text.substr(0, maxLength);
    }

    text.resize(maxLength - 3);
    text += "...";
    return text;
}

std::string NormalizeSingleLineText(std::string text) {
    std::replace(text.begin(), text.end(), '\r', ' ');
    std::replace(text.begin(), text.end(), '\n', ' ');
    return text;
}

std::string NormalizeCommandArgument(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\n\r");
    value = value.substr(first, last - first + 1);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string BuildRssChatNotification(const keybinder::RssToastData& item) {
    const std::string author = NormalizeSingleLineText(item.author.empty() ? "Unbekannt" : item.author);
    const std::string title = NormalizeSingleLineText(item.title.empty() ? "Unbekanntes Thema" : item.title);
    return TruncateForSampChat("{FF8C00}[Forum] {FFFFFF}Neuer Post von {55AAFF}" + author + " {FFFFFF}im Thema {FFCC00}" + title, 144);
}

std::string BuildTextDrawDumpPath() {
    std::time_t now = std::time(nullptr);
    std::tm localTime = {};
    localtime_s(&localTime, &now);

    char fileName[64] = {};
    std::snprintf(
        fileName,
        sizeof(fileName),
        "textdraw_dump_%04d%02d%02d_%02d%02d%02d.txt",
        localTime.tm_year + 1900,
        localTime.tm_mon + 1,
        localTime.tm_mday,
        localTime.tm_hour,
        localTime.tm_min,
        localTime.tm_sec);

    std::string root = keybinder::PersistentPaths::EnsureLayout();
    if (root.empty()) {
        root = keybinder::PersistentPaths::RootDir();
    }

    if (root.empty()) {
        return fileName;
    }

    return root + "\\" + fileName;
}

} // namespace

class Keybinder;
static void DoEmergencyShutdown();

static void WINAPI HookedExitProcess(UINT uExitCode) {
    DoEmergencyShutdown();

    auto original = g_exit_process_hook->original<void(WINAPI*)(UINT)>();
    original(uExitCode);
}

static BOOL WINAPI HookedTerminateProcess(HANDLE hProcess, UINT uExitCode) {
    bool is_self = (hProcess == GetCurrentProcess());
    if (!is_self) {
        DWORD target_pid = GetProcessId(hProcess);
        is_self = (target_pid == GetCurrentProcessId());
    }

    if (is_self) {
        // If shutdown already done, pass through. ExitProcess internally calls
        // TerminateProcess — converting back would cause a loop.
        if (g_shutdown_phase.load() >= 3) {
            auto original = g_terminate_process_hook->original<BOOL(WINAPI*)(HANDLE, UINT)>();
            return original(hProcess, uExitCode);
        }

        DoEmergencyShutdown();

        // Convert hard TerminateProcess → graceful ExitProcess.
        // TerminateProcess skips DLL cleanup; ExitProcess triggers
        // proper DLL_PROCESS_DETACH for all DLLs.
        ExitProcess(0);
    }

    auto original = g_terminate_process_hook->original<BOOL(WINAPI*)(HANDLE, UINT)>();
    return original(hProcess, uExitCode);
}

static void InstallExitHooks() {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return;
    }
    
    auto* exit_proc = reinterpret_cast<void*>(GetProcAddress(kernel32, "ExitProcess"));
    if (exit_proc) {
        auto hook = safetyhook::InlineHook::create(exit_proc, reinterpret_cast<void*>(&HookedExitProcess));
        if (hook) {
            g_exit_process_hook = new safetyhook::InlineHook(std::move(*hook));
        }
    }
    
    auto* terminate_proc = reinterpret_cast<void*>(GetProcAddress(kernel32, "TerminateProcess"));
    if (terminate_proc) {
        auto hook = safetyhook::InlineHook::create(terminate_proc, reinterpret_cast<void*>(&HookedTerminateProcess));
        if (hook) {
            g_terminate_process_hook = new safetyhook::InlineHook(std::move(*hook));
        }
    }
}

// Forward declare the suppression function (defined in ImGuiOverlay.cpp in keybinder namespace)
namespace keybinder {
void SuppressGameMouseInput();
void RestoreGameMouseInput();
}

class Keybinder {
public:
    static void ExportKillTrackerStateToSettings() {
        auto weaponStats = bridge_.ExportWeaponStats();
        std::vector<keybinder::WeaponStatsData> wsData;
        for (const auto& [wid, dmg, kc] : weaponStats) {
            keybinder::WeaponStatsData ws;
            ws.weaponId = wid;
            ws.totalDamage = dmg;
            ws.killCount = kc;
            wsData.push_back(ws);
        }
        settingsManager_.SetWeaponStats(wsData);

        auto recentKills = bridge_.ExportRecentKills();
        std::vector<keybinder::KillEventData> keData;
        for (const auto& [victim, wid, ts, dmg] : recentKills) {
            keybinder::KillEventData ke;
            ke.victim = victim;
            ke.weaponId = wid;
            ke.timestamp = ts;
            ke.damageDealt = dmg;
            keData.push_back(ke);
        }
        settingsManager_.SetRecentKills(keData);

        auto recentDeaths = bridge_.ExportRecentDeaths();
        std::vector<keybinder::DeathEventData> deData;
        for (const auto& [killer, wid, ts] : recentDeaths) {
            keybinder::DeathEventData de;
            de.killer = killer;
            de.weaponId = wid;
            de.timestamp = ts;
            deData.push_back(de);
        }
        settingsManager_.SetRecentDeaths(deData);
        settingsManager_.SetKillCount(bridge_.ExportKillCount());
        settingsManager_.SetDeathCount(bridge_.ExportDeathCount());
    }

    // Called from TerminateProcess/ExitProcess hooks BEFORE any teardown.
    // CRT is fully alive here, all state is valid. This is our ONLY chance
    // to save config and log when SA-MP calls TerminateProcess on /q.
    static void EmergencyShutdown() {
        if (g_shutdown_phase.load() >= 2) {
            return;
        }
        
        // Phase 4 is emergency-only: save state and disable feature callbacks,
        // but avoid hook teardown while SA-MP is exiting via TerminateProcess.
        g_shutdown_phase.store(4);
        
        // Save keybinds + settings (CRT is alive, file I/O works)
        keybinder::Logger::Info("=== NLBinder emergency shutdown (TerminateProcess intercepted) ===");

        // Export kill tracker data to unified config before emergency save.
        // JobTracker persistence is flushed by bridge_.Shutdown() into jobs_data.json.
        ExportKillTrackerStateToSettings();

        rssFeedManager_.Shutdown();
        onlinePlayerInfoService_.Shutdown();
        onlineDataManager_.Shutdown();

        keybinder::Logger::Info("Saving %zu keybind(s) and settings", keybindManager_.Count());
        if (!keybindManager_.Save(&settingsManager_)) {
            keybinder::Logger::Error("Failed to save config on emergency shutdown");
        }

        nativeHudEditor_.Shutdown();
        interfaceEditor_.Shutdown();
        interfaceEditor_.RestoreOriginalValues();
        onlineOverlayRenderer_.Shutdown();
        onlineOverlayController_.Shutdown();
        onlineEventDetector_.Shutdown();
        quickMap_.Shutdown();
        speedometerManager_.Shutdown();
        textDrawAdapter_.Shutdown();
        
        // Shut down SAMPBridge (removes RakHook, command hooks)
        hitsoundPlayer_.Shutdown();
        weaponIconManager_.Shutdown();
        textureManager_.ReleaseAll();
        bridge_.Shutdown();
        
        // DO NOT shut down ImGuiOverlay here. We convert TerminateProcess→ExitProcess(0)
        // which triggers proper DLL_PROCESS_DETACH for all DLLs. Attempting to remove
        // hooks during emergency shutdown risks crashing other hook chains (SpecialK, etc.)
        // that share the same hook targets. The OS reclaims all resources on process exit.

        keybinder::Logger::Info("NLBinder emergency shutdown complete");
        g_shutdown_phase.store(3);
        keybinder::Logger::Shutdown();
    }
    
    Keybinder() {
        g_crashLogPath.clear();

        const std::string layoutRoot = keybinder::PersistentPaths::EnsureLayout();
        if (IsAbsolutePath(layoutRoot)) {
            const std::string crashLogPath = keybinder::PersistentPaths::CrashLogFile();
            if (IsAbsolutePath(crashLogPath)) {
                g_crashLogPath = crashLogPath;
            } else {
                OutputDebugStringA("NLBinder: persistent crash log path unavailable; crash file logging disabled.\n");
            }

            const std::string logPath = keybinder::PersistentPaths::LogFile();
            if (IsAbsolutePath(logPath)) {
                keybinder::Logger::Initialize(logPath.c_str());
            } else {
                OutputDebugStringA("NLBinder: persistent log path unavailable; file logging disabled.\n");
            }
        } else {
            OutputDebugStringA("NLBinder: failed to resolve or create persistent layout; file logging disabled.\n");
        }

        // Install crash handler FIRST before any other initialization
        InstallCrashHandler();
        
        // Hook ExitProcess/TerminateProcess to intercept exit before any teardown.
        // SA-MP's /q may call TerminateProcess which skips DLL_PROCESS_DETACH entirely.
        InstallExitHooks();
        
        // Enable info logging (change to Debug for verbose output)
        keybinder::Logger::SetMinLevel(keybinder::LogLevel::Info);
        
        keybinder::Logger::Info("NLBinder v0.1.1 by KOBiRO - SA-MP 0.3.DL Keybinder starting up");
        keybinder::Logger::Info("Crash handler installed");

        if (!keybindManager_.Load(&settingsManager_)) {
            keybinder::Logger::Warning("Config load failed or corrupt; starting with empty keybind set");
        } else {
            keybinder::Logger::Info("Loaded %zu keybind(s) from unified config", keybindManager_.Count());
        }

        ApplyConfiguredInterfaceEditorSettings(settingsManager_, interfaceEditor_);

        bridge_.SetKillTrackerEnabled(settingsManager_.Get().kill_tracker_enabled);
        bridge_.SetKillTrackerLocalKillMessageEnabled(settingsManager_.Get().kill_tracker_local_kill_message_enabled);
        bridge_.SetKillTrackerLocalDeathMessageEnabled(settingsManager_.Get().kill_tracker_local_death_message_enabled);
        bridge_.SetKillTrackerServerKillMessageEnabled(settingsManager_.Get().kill_tracker_server_kill_message_enabled);
        bridge_.SetKillTrackerServerDeathMessageEnabled(settingsManager_.Get().kill_tracker_server_death_message_enabled);
        bridge_.SetKillTrackerTrackDamage(settingsManager_.Get().kill_tracker_track_damage);
        bridge_.SetKillTrackerLocalKillMessageTemplate(settingsManager_.Get().kill_tracker_local_kill_message_template);
        bridge_.SetKillTrackerLocalDeathMessageTemplate(settingsManager_.Get().kill_tracker_local_death_message_template);
        bridge_.SetKillTrackerServerKillMessageTemplate(settingsManager_.Get().kill_tracker_server_kill_message_template);
        bridge_.SetKillTrackerServerDeathMessageTemplate(settingsManager_.Get().kill_tracker_server_death_message_template);
        bridge_.SetAutoHudScaleFix(settingsManager_.Get().auto_hudscalefix);
        bridge_.SetAutoEngineStart(settingsManager_.Get().auto_engine_start);
        bridge_.SetSettingsManager(&settingsManager_);
        bridge_.SetStatsSynchronizedCallback([&](const keybinder::ServerStatsSnapshot& snapshot) {
            settingsManager_.GetMutable().synchronized_player_stats = snapshot;
            ExportKillTrackerStateToSettings();
            return keybindManager_.Save(&settingsManager_);
        });
        textDrawAdapter_.SetSpeedometerEnabled(keybinder::speedometer::kCustomTachoAvailable && settingsManager_.Get().speedometer_enabled);
        textDrawAdapter_.Initialize();
        speedometerManager_.Initialize(&textDrawAdapter_, &bridge_);
        speedometerManager_.OnSettingsChanged(settingsManager_.Get().speedometer_enabled);
        interfaceEditor_.SetSettingsManager(&settingsManager_);
        interfaceEditor_.SetSAMPBridge(&bridge_);
        interfaceEditor_.SetOnRuntimePresetChanged([&]() {
            if (const auto* preset = interfaceEditor_.GetPreset(interfaceEditor_.GetActivePreset())) {
                nativeHudEditor_.SetLayout(preset->layout);
            }
            keybindManager_.Save(&settingsManager_);
        });
        interfaceEditor_.SetElementBoundsProvider([&](keybinder::HudElement element, float& x, float& y, float& width, float& height) {
            return nativeHudEditor_.GetElementBounds(element, x, y, width, height);
        });
        quickMap_.Initialize(&settingsManager_, &textureManager_, &bridge_);
        quickMap_.SetInputGateCallback([&]() {
            return overlay_.IsVisible();
        });
        quickMap_.SetRenderCallback([&]() {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            if (viewport == nullptr) {
                return;
            }

            const float scale = (std::max)(overlay_.GetCurrentScale(), 1.0f);
            const float padding = 24.0f * scale;
            const ImVec2 available(
                (std::max)(0.0f, viewport->Size.x - padding * 2.0f),
                (std::max)(0.0f, viewport->Size.y - padding * 2.0f));
            if (available.x <= 0.0f || available.y <= 0.0f) {
                return;
            }

            const float aspect = quickMap_.GetActiveMapAspectRatio();
            ImVec2 displaySize = available;
            if (displaySize.x / displaySize.y > aspect) {
                displaySize.x = displaySize.y * aspect;
            } else {
                displaySize.y = displaySize.x / aspect;
            }

            const ImVec2 displayPos(
                viewport->Pos.x + (viewport->Size.x - displaySize.x) * 0.5f,
                viewport->Pos.y + (viewport->Size.y - displaySize.y) * 0.5f);

            quickMap_.Render(ImGui::GetForegroundDrawList(), displayPos, displaySize);
        });

        {
            std::vector<std::tuple<int, float, int>> weaponStats;
            for (const auto& ws : settingsManager_.Get().kill_tracker_weapon_stats) {
                weaponStats.emplace_back(ws.weaponId, ws.totalDamage, ws.killCount);
            }
            bridge_.ImportWeaponStats(weaponStats);

            std::vector<std::tuple<std::string, int, std::time_t, float>> recentKills;
            for (const auto& kill : settingsManager_.Get().kill_tracker_recent_kills) {
                recentKills.emplace_back(kill.victim, kill.weaponId, kill.timestamp, kill.damageDealt);
            }
            bridge_.ImportRecentKills(recentKills);

            std::vector<std::tuple<std::string, int, std::time_t>> recentDeaths;
            for (const auto& death : settingsManager_.Get().kill_tracker_recent_deaths) {
                recentDeaths.emplace_back(death.killer, death.weaponId, death.timestamp);
            }
            bridge_.ImportRecentDeaths(recentDeaths);
            bridge_.ImportKillCount(settingsManager_.Get().kill_tracker_kill_count);
            bridge_.ImportDeathCount(settingsManager_.Get().kill_tracker_death_count);

            keybinder::Logger::Info("Imported %zu weapon stats, %zu recent kills, %zu recent deaths from config",
                weaponStats.size(), recentKills.size(), recentDeaths.size());
        }

        overlay_.SetSettingsManager(&settingsManager_);
        overlay_.SetSAMPBridge(&bridge_);

        crosshairSwitcher_.ApplySettings(settingsManager_.Get());
        squareMap_.ApplySettings(settingsManager_.Get());
        hudSpriteManager_.ApplySettings(settingsManager_.Get());
        weaponIconManager_.ApplySettings(settingsManager_.Get());

        hitmarker_.ApplySettings(settingsManager_.Get());

        hitsoundPlayer_.ApplySettings(settingsManager_.Get());

        executor_.SetSAMPBridge(&bridge_);
        executor_.SetGameDataProvider(&bridge_);
        inputManager_.SetKeybindManager(&keybindManager_);
        inputManager_.SetActionExecutor(&executor_);
        inputManager_.SetSAMPBridge(&bridge_);
        inputManager_.SetImGuiOverlay(&overlay_);
        inputManager_.SetQuickMapVisibleCallback([&]() {
            return quickMap_.IsVisible() || interfaceEditor_.IsElementMoveModeActive();
        });
        overlay_.SetEarlyKeybindHandler([&](int key, bool keyDown) {
            return inputManager_.HandleEarlyKeyEvent(key, keyDown);
        });
        overlay_.SetFocusLostHandler([&]() {
            inputManager_.ClearTransientInputState();
        });

        mainWindowUI_.SetKeybindManager(&keybindManager_);
        mainWindowUI_.SetSettingsManager(&settingsManager_);
        mainWindowUI_.SetSAMPBridge(&bridge_);
        bridge_.SetEngineInputBlockedCallback([&]() {
            return overlay_.IsVisible() || quickMap_.IsVisible()
                || onlineOverlayRenderer_.IsPersistentOverlayMoveModeActive()
                || interfaceEditor_.IsElementMoveModeActive();
        });
        mainWindowUI_.SetHitmarker(&hitmarker_);
        mainWindowUI_.SetHitsoundPlayer(&hitsoundPlayer_);
        mainWindowUI_.SetCrosshairSwitcher(&crosshairSwitcher_);
        mainWindowUI_.SetSquareMap(&squareMap_);
        mainWindowUI_.SetHudSpriteManager(&hudSpriteManager_);
        mainWindowUI_.SetWeaponIconManager(&weaponIconManager_);
        mainWindowUI_.SetInterfaceEditor(&interfaceEditor_);
        mainWindowUI_.SetInterfaceMoveRequestedCallback([&]() {
            onlineOverlayRenderer_.SetPersistentOverlayMoveMode(false);
            quickMap_.Hide();
            overlay_.Hide();
        });
        mainWindowUI_.SetRssFeedManager(&rssFeedManager_);
        mainWindowUI_.SetOnlineDataManager(&onlineDataManager_);
        mainWindowUI_.SetOnlineOverlayController(&onlineOverlayController_);
        mainWindowUI_.SetOnlineOverlayRenderer(&onlineOverlayRenderer_);
        mainWindowUI_.SetOnlineEventDetector(&onlineEventDetector_);
        mainWindowUI_.SetSpeedometerManager(&speedometerManager_);
        mainWindowUI_.SetTextureManager(&textureManager_);
        mainWindowUI_.SetDeviceGetter([&]() {
            return overlay_.GetDevice();
        });
        mainWindowUI_.OnKeybindsChanged([this]() {
            keybindManager_.Save(&settingsManager_);
        });
        overlay_.SetMainWindowUI(&mainWindowUI_);

        bridge_.SetHitmarkerDamageCallback([&](const killtracker::DamageEvent& event) {
            hitmarker_.OnDamageEvent(event);
        });
        bridge_.SetHitsoundDamageCallback([&](const killtracker::DamageEvent& event) {
            hitsoundPlayer_.OnDamageEvent(event);
        });
        bridge_.SetHitsoundIncomingCallback([&]() {
            hitsoundPlayer_.OnIncomingDamage();
        });
        bridge_.SetHitmarkerKillCallback([&](const killtracker::KillEvent& event) {
            hitmarker_.OnKillEvent(event);
        });

        overlay_.SetHitmarkerRenderCallback([&]() {
            hitmarker_.Update();
            hitmarkerRenderer_.RenderMarkers(hitmarker_.GetActiveMarkers(), &textureManager_,
                                             overlay_.GetDevice());
            hitsoundPlayer_.Update();
        });
        overlay_.SetToastRenderCallback([&]() {
            toastNotification_.Render(ImGui::GetForegroundDrawList(), overlay_.GetCurrentScale());
        });
        overlay_.SetQuickMapRenderCallback([&]() {
            quickMap_.Render();
        });
        overlay_.SetQuickMapWheelCaptureCallback([&]() {
            return quickMap_.WantsMouseWheelCapture();
        });
        overlay_.SetQuickMapWheelHandlerCallback([&](float delta) {
            quickMap_.OnMouseWheel(static_cast<int>(delta));
        });
        overlay_.SetVitalStatsRenderCallback([&]() {
            interfaceEditor_.RenderVitalStatsNumbers(ImGui::GetForegroundDrawList(), overlay_.IsVisible(), overlay_.GetCurrentScale());
            interfaceEditor_.RenderElementMoveMode(ImGui::GetForegroundDrawList(), overlay_.GetCurrentScale());
        });
        overlay_.SetSpeedometerRenderCallback([&]() {
            const keybinder::Settings& settings = settingsManager_.Get();
            if (!keybinder::speedometer::kCustomTachoAvailable || !settings.speedometer_enabled) {
                return;
            }
            const keybinder::speedometer::SpeedometerStyle style = keybinder::speedometer::ParseSpeedometerStyle(settings.speedometer_style);
            speedometerUI_.Render(speedometerManager_.GetState(), style);
        });

        onlineOverlayRenderer_.Initialize(&settingsManager_, &onlineDataManager_);
        onlineOverlayRenderer_.SetQuickMapVisibleCallback([&]() {
            return quickMap_.IsVisible();
        });
        onlineOverlayRenderer_.SetOverlayVisibleCallback([&]() {
            return overlay_.IsVisible();
        });
        onlineOverlayRenderer_.SetInputGateCallback([&]() {
            return bridge_.IsChatOpen() || bridge_.IsDialogOpen() || bridge_.IsCursorActive();
        });
        overlay_.SetOnlineQuickOverlayRenderCallback([&]() {
            onlineOverlayRenderer_.Render();
            onlineOverlayRenderer_.RenderPersistentOverlays();
        });
        onlineOverlayRenderer_.SetSettingsPersistCallback([&]() {
            keybindManager_.Save(&settingsManager_);
        });
        overlay_.SetAuxiliaryInteractionActiveCallback([&]() {
            return onlineOverlayRenderer_.IsPersistentOverlayMoveModeActive()
                || interfaceEditor_.IsElementMoveModeActive();
        });
        overlay_.SetAuxiliaryInteractionCancelCallback([&]() {
            onlineOverlayRenderer_.SetPersistentOverlayMoveMode(false);
            interfaceEditor_.FinishElementMove();
        });

        onlineEventDetector_.Initialize(&settingsManager_);
        onlineOverlayController_.Initialize(&settingsManager_, &onlineDataManager_);
        onlineOverlayController_.SetGameStateProvider([&]() {
            return keybinder::OnlineOverlayController::CaptureGameState(&bridge_);
        });
        onlineOverlayRenderer_.SetOverlayController(&onlineOverlayController_);
        bridge_.SetIncomingChatCallback([&](const std::string& text) {
            onlineEventDetector_.OnChatLine(text);
        });

        rssFeedManager_.Initialize();
        onlinePlayerInfoService_.Initialize();
        onlineDataManager_.Initialize();
        // SyncOnlineRuntimeContext will be called in drawHudEvent once bridge is ready

        bridge_.RegisterCommand("info", [&](const std::string& params) {
            const std::string username = keybinder::OnlinePlayerInfoService::NormalizeLookupName(params);
            if (username.empty()) {
                bridge_.SendLocalMessage("{33CCFF}[NLBinder]{FFFFFF} Nutzung: /info <username>");
                return;
            }

            onlinePlayerInfoService_.QueueLookup(username);
        });

        bridge_.RegisterCommand("reconnect", [&](const std::string& params) {
            if (params.find_first_not_of(" \t\r\n") != std::string::npos) {
                bridge_.SendLocalMessage("{33CCFF}[NL-BINDER]{FFFFFF} Nutzung: /reconnect");
                return;
            }
            std::string error;
            if (!keybinder::RequestReconnect(settingsManager_.Get(), bridge_, [&]() {
                    ExportKillTrackerStateToSettings();
                    return keybindManager_.Save(&settingsManager_);
                }, error)) {
                bridge_.SendLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} " + error);
            }
        });

        bridge_.RegisterCommand("speed", [&](const std::string& params) {
            std::string error;
            if (!bridge_.RequestSpeedTicket(params, error))
                bridge_.SendLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} " + error);
        });

        auto runTextDrawDump = [&](const std::string&) {
            const std::string outputPath = BuildTextDrawDumpPath();
            std::size_t count = 0;
            if (textDrawAdapter_.DumpAllTextDrawsToFile(outputPath, &count)) {
                const std::string message = "{33CCFF}[NLBinder]{FFFFFF} TextDraw-Dump gespeichert: "
                    + outputPath
                    + " ("
                    + std::to_string(count)
                    + " Einträge)";
                bridge_.SendLocalMessage(TruncateForSampChat(message, 144));
                return;
            }

            keybinder::Logger::Warning("[Main] TextDraw dump failed for path '%s'", outputPath.c_str());
            bridge_.SendLocalMessage("{FF3333}[NLBinder]{FFFFFF} TextDraw-Dump fehlgeschlagen.");
        };

        bridge_.RegisterCommand("textdrawdump", runTextDrawDump);
        bridge_.RegisterCommand("tddump", runTextDrawDump);

        auto runTextDrawDiagnostics = [&](const std::string& params) {
            const std::string mode = NormalizeCommandArgument(params);
            if (mode == "on" || mode == "start" || mode == "1" || mode == "true") {
                textDrawAdapter_.SetDiagnosticsEnabled(true);
                bridge_.SendLocalMessage("{33CCFF}[NLBinder]{FFFFFF} TextDraw-Diagnose aktiviert. Reproduzieren, dann /tddiag off.");
                return;
            }

            if (mode == "off" || mode == "stop" || mode == "0" || mode == "false") {
                const unsigned long count = textDrawAdapter_.GetDiagnosticsEventCount();
                textDrawAdapter_.SetDiagnosticsEnabled(false);
                bridge_.SendLocalMessage("{33CCFF}[NLBinder]{FFFFFF} TextDraw-Diagnose deaktiviert. Ereignisse: " + std::to_string(count));
                return;
            }

            const std::string status = textDrawAdapter_.IsDiagnosticsEnabled() ? "an" : "aus";
            bridge_.SendLocalMessage("{33CCFF}[NLBinder]{FFFFFF} Nutzung: /tddiag on|off (Status: " + status + ")");
        };

        bridge_.RegisterCommand("textdrawdiag", runTextDrawDiagnostics);
        bridge_.RegisterCommand("tddiag", runTextDrawDiagnostics);

        bridge_.SetSlashCommandInterceptor([&](const std::string& text) -> bool {
            keybinder::Logger::Info("[Main] Slash command interceptor called with text: '%s'", text.c_str());
            
            std::string command = text;
            if (!command.empty() && command[0] == '/') {
                command = command.substr(1);
            }
            
            std::size_t spacePos = command.find(' ');
            if (spacePos != std::string::npos) {
                command = command.substr(0, spacePos);
            }
            
            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t\n\r");
                if (start == std::string::npos) { s = ""; return; }
                size_t end = s.find_last_not_of(" \t\n\r");
                s = s.substr(start, end - start + 1);
            };
            trim(command);
            std::transform(command.begin(), command.end(), command.begin(), ::tolower);
            
            if (command.empty()) {
                return false;
            }

            if (MatchesConfiguredUiOpenCommand(settingsManager_.Get(), command)) {
                quickMap_.Hide();
                overlay_.Show();
                return true;
            }

            if (command == "info" || command == "reconnect" || command == "speed") {
                return false;
            }
            
            const std::vector<size_t> matches = keybindManager_.GetBySlashCommand(command);
            if (matches.empty()) {
                return false;
            }

            std::vector<keybinder::Action> allActions;
            for (size_t index : matches) {
                const keybinder::Keybind* keybind = keybindManager_.Get(index);
                if (keybind == nullptr || !keybind->enabled) {
                    continue;
                }
                
                keybinder::Logger::Info("[Main] Slash command '/%s' triggered keybind '%s' (%zu actions)",
                    command.c_str(), keybind->name.c_str(), keybind->actions.size());
                
                if (!keybind->actions.empty()) {
                    allActions.insert(allActions.end(), keybind->actions.begin(), keybind->actions.end());
                }
            }
            
            if (!allActions.empty()) {
                executor_.Execute(allActions);
                return true;
            }
            
            return false;
        });
        
        auto RegisterKeybindSlashCommands = [&]() {
            std::vector<std::string> slashCommands;
            for (size_t i = 0; i < keybindManager_.Count(); ++i) {
                const auto* kb = keybindManager_.Get(i);
                if (kb != nullptr && !kb->slashCommand.empty()) {
                    slashCommands.push_back(kb->slashCommand);
                    keybinder::Logger::Info("[Main] Found slash command in keybind '%s': /%s", 
                        kb->name.c_str(), kb->slashCommand.c_str());
                }
            }
            bridge_.RegisterSlashCommands(slashCommands);
        };

        auto SyncOnlineRuntimeContext = [&]() {
            const std::string localPlayerName = bridge_.GetLocalPlayerName();
            onlineDataManager_.UpdateSettings(settingsManager_.Get().online_enabled, localPlayerName);
            mainWindowUI_.SetOnlineLocalPlayerName(localPlayerName);
        };

        Events::initRwEvent += [] {
            overlay_.Initialize();
            crosshairSwitcher_.Initialize();
            squareMap_.Initialize();
            hudSpriteManager_.Initialize();
            weaponIconManager_.Initialize();
            if (!nativeHudEditor_.Initialize()) {
                keybinder::Logger::Error("Interface hooks failed: %s", nativeHudEditor_.GetLastError().c_str());
            }
            if (!interfaceEditor_.Initialize()) {
                keybinder::Logger::Error("InterfaceEditor hooks failed to initialize; custom zone text may not work");
            }

            auto pngFiles = textureManager_.ScanDirectory("hitmarkers");
            auto crosshairFiles = textureManager_.ScanDirectory("crosshairs");
            if (auto* device = overlay_.GetDevice()) {
                quickMap_.OnDeviceReset(device);
                for (const auto& path : pngFiles) {
                    textureManager_.LoadTexture(path, device);
                }
                for (const auto& path : crosshairFiles) {
                    textureManager_.LoadTexture(path, device);
                }
            }
            crosshairSwitcher_.OnDeviceReset();
            squareMap_.OnDeviceReset();
            hudSpriteManager_.OnDeviceReset();
            weaponIconManager_.OnDeviceReset();
        };
        Events::drawHudEvent += [&]() {
            if (g_shutdown_phase.load() != 0) {
                return;
            }

            bridge_.Initialize();
            bridge_.Update();
            speedometerManager_.Update();
            SyncOnlineRuntimeContext();
            inputManager_.Initialize();
            inputManager_.Update();
            interfaceEditor_.ApplyCurrentPreset();
            if (const auto* preset = interfaceEditor_.GetPreset(interfaceEditor_.GetActivePreset())) {
                nativeHudEditor_.SetLayout(preset->layout);
            }

            static bool overlayWasVisible = false;
            static bool quickMapWasVisible = false;
            static bool onlineQuickWasVisible = false;

            if (quickMap_.IsVisible() || settingsManager_.Get().quickmap_enabled) {
                quickMap_.Update();
            }

            onlineOverlayRenderer_.Update();
            const auto onlineUpdateTime = std::chrono::steady_clock::now();
            static auto previousOnlineUpdateTime = onlineUpdateTime;
            const float onlineDelta = std::chrono::duration<float>(onlineUpdateTime - previousOnlineUpdateTime).count();
            previousOnlineUpdateTime = onlineUpdateTime;
            onlineOverlayController_.Update(onlineDelta);

            bool overlayVisible = overlay_.IsVisible();
            bool quickMapVisible = quickMap_.IsVisible();
            bool onlineQuickVisible = onlineOverlayRenderer_.IsVisible();

            // Mutual exclusivity: main overlay, QuickMap, and online quick overlay
            if (overlayVisible && !overlayWasVisible && (quickMapVisible || onlineQuickVisible)) {
                quickMap_.Hide();
                onlineOverlayRenderer_.Hide();
                quickMapVisible = false;
                onlineQuickVisible = false;
            } else if ((quickMapVisible && !quickMapWasVisible && overlayVisible) ||
                       (onlineQuickVisible && !onlineQuickWasVisible && overlayVisible)) {
                overlay_.Hide();
                overlayVisible = false;
            } else if (overlayVisible && (quickMapVisible || onlineQuickVisible)) {
                overlay_.Hide();
                overlayVisible = false;
            }

            // QuickMap and online quick overlay also mutually exclusive
            if (quickMapVisible && onlineQuickVisible) {
                onlineOverlayRenderer_.Hide();
                onlineQuickVisible = false;
            }

            overlayWasVisible = overlayVisible;
            quickMapWasVisible = quickMapVisible;
            onlineQuickWasVisible = onlineQuickVisible;

            if (quickMap_.IsVisible()) {
                quickMap_.SetRenderData(quickMap_.ExtractRenderData(&bridge_));
            }

            std::vector<keybinder::RssToastData> newItems = rssFeedManager_.ConsumeNewItems();
            for (const keybinder::RssToastData& item : newItems) {
                if (settingsManager_.Get().rss_notification_mode == keybinder::RssNotificationMode::Chat) {
                    bridge_.QueueLocalMessage(BuildRssChatNotification(item));
                } else {
                    toastNotification_.Enqueue(item.feedName, item.title, item.author);
                }
            }

            std::vector<keybinder::OnlinePlayerLookupResult> completedLookups = onlinePlayerInfoService_.ConsumeCompletedLookups();
            for (std::size_t i = 0; i < completedLookups.size(); ++i) {
                bridge_.SendLocalMessage(completedLookups[i].chatMessage);
            }
            toastNotification_.Update();
            
            static bool slashCommandsRegistered = false;
            if (!slashCommandsRegistered) {
                RegisterKeybindSlashCommands();
                slashCommandsRegistered = true;
            }
        };
        Events::drawFontsEvent += [&]() {
            if (g_shutdown_phase.load() != 0) {
                return;
            }

            // Draw the overlay after the HUD pass so the ImGui UI sits on top of
            // SA-MP chat and other late HUD elements instead of underneath them.
            overlay_.Render();

        };
        Events::gameProcessEvent += [&]() {
            if (g_shutdown_phase.load() != 0) return;
            std::string reconnectError;
            if (keybinder::UpdateReconnect(reconnectError)) {
                bridge_.QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} " + reconnectError);
            }
            inputManager_.ApplyEarlySuppressedKeys();

            if (overlay_.IsVisible() || interfaceEditor_.IsElementMoveModeActive()) {
                // Suppress mouse-look before the game consumes per-frame input.
                // The later HUD/render passes are too late to reliably stop camera
                // movement, but we intentionally keep this suppression scoped to
                // mouse/camera state so player and vehicle momentum continue.
                keybinder::SuppressGameMouseInput();
            }
        };
        Events::d3dLostEvent += [] {
            hudSpriteManager_.OnDeviceLost();
            weaponIconManager_.OnDeviceLost();
            squareMap_.OnDeviceLost();
            crosshairSwitcher_.OnDeviceLost();
            quickMap_.OnDeviceLost();
            overlay_.OnDeviceLost();
            textureManager_.ReleaseAll();
        };
        Events::d3dResetEvent += [] {
            overlay_.OnDeviceReset();

            textureManager_.ReloadAll(overlay_.GetDevice());
            quickMap_.OnDeviceReset(overlay_.GetDevice());
            crosshairSwitcher_.OnDeviceReset();
            squareMap_.OnDeviceReset();
            hudSpriteManager_.OnDeviceReset();
            weaponIconManager_.OnDeviceReset();
        };
        Events::shutdownRwEvent += [] {
            g_shutdown_phase.store(2);
            keybinder::Logger::Info("=== NLBinder shutdown initiated ===");

            // Export kill tracker data to unified config before saving.
            // JobTracker persistence is flushed by bridge_.Shutdown() into jobs_data.json.
            ExportKillTrackerStateToSettings();

            rssFeedManager_.Shutdown();
            onlinePlayerInfoService_.Shutdown();

            keybinder::Logger::Info("Phase 1: Saving %zu keybind(s) and settings to unified config", keybindManager_.Count());
            if (!keybindManager_.Save(&settingsManager_)) {
                keybinder::Logger::Error("Failed to save unified config on shutdown");
            }
            crosshairSwitcher_.Shutdown();
            squareMap_.Shutdown();
            hudSpriteManager_.Shutdown();
            weaponIconManager_.Shutdown();
            onlineOverlayRenderer_.Shutdown();
            quickMap_.Shutdown();
            speedometerManager_.Shutdown();
            textDrawAdapter_.Shutdown();
            nativeHudEditor_.Shutdown();
            interfaceEditor_.Shutdown();
            interfaceEditor_.RestoreOriginalValues();
            keybinder::Logger::Info("Phase 2: Shutting down SAMPBridge");
            hitsoundPlayer_.Shutdown();
            textureManager_.ReleaseAll();
            bridge_.Shutdown();
            keybinder::Logger::Info("Phase 3: Shutting down ImGuiOverlay");
            overlay_.Shutdown();
            keybinder::Logger::Info("Phase 4: Shutting down Logger");
            keybinder::Logger::Info("NLBinder shutdown complete");
            g_shutdown_phase.store(3);
            keybinder::Logger::Shutdown();
        };
    }
    
    ~Keybinder() {
        int phase = g_shutdown_phase.load();

        if (phase == 0) {
            rssFeedManager_.Shutdown();
            onlinePlayerInfoService_.Shutdown();
        }

        if (phase == 3) {
            OutputDebugStringA("[NLBinder] Clean shutdown already complete");
            return;
        }
        
        // Emergency exit (e.g., /q command) — do NOTHING except mark phase.
        // Unpatching hooks doesn't always work during teardown; instead just
        // mark phase so destructors know to skip all cleanup.
        OutputDebugStringA("[NLBinder] Emergency exit detected - skipping cleanup");
        
        g_shutdown_phase.store(1);
    }

private:
    static keybinder::ImGuiOverlay overlay_;
    static SAMPBridge bridge_;
    static keybinder::KeybindManager keybindManager_;
    static keybinder::ActionExecutor executor_;
    static keybinder::InputManager inputManager_;
    static keybinder::MainWindowUI mainWindowUI_;
    static keybinder::SettingsManager settingsManager_;
    static keybinder::RssFeedManager rssFeedManager_;
    static keybinder::OnlinePlayerInfoService onlinePlayerInfoService_;
    static keybinder::OnlineDataManager onlineDataManager_;
    static keybinder::ToastNotification toastNotification_;
    static keybinder::Hitmarker hitmarker_;
    static keybinder::HitmarkerRenderer hitmarkerRenderer_;
    static keybinder::HitsoundPlayer hitsoundPlayer_;
    static keybinder::CrosshairSwitcher crosshairSwitcher_;
    static keybinder::SquareMap squareMap_;
    static keybinder::HudSpriteManager hudSpriteManager_;
    static keybinder::WeaponIconManager weaponIconManager_;
    static keybinder::InterfaceEditor interfaceEditor_;
    static keybinder::NativeHudEditor nativeHudEditor_;
    static keybinder::TextureManager textureManager_;
    static keybinder::QuickMap quickMap_;
    static keybinder::OnlineOverlayRenderer onlineOverlayRenderer_;
    static keybinder::OnlineEventDetector onlineEventDetector_;
    static keybinder::OnlineOverlayController onlineOverlayController_;
    static keybinder::TextDrawAdapter textDrawAdapter_;
    static keybinder::SpeedometerManager speedometerManager_;
    static keybinder::SpeedometerUI speedometerUI_;
};

keybinder::ImGuiOverlay Keybinder::overlay_{};
SAMPBridge Keybinder::bridge_{};
keybinder::KeybindManager Keybinder::keybindManager_{};
keybinder::ActionExecutor Keybinder::executor_{};
keybinder::InputManager Keybinder::inputManager_{};
keybinder::MainWindowUI Keybinder::mainWindowUI_{};
keybinder::SettingsManager Keybinder::settingsManager_{};
keybinder::RssFeedManager Keybinder::rssFeedManager_{Keybinder::settingsManager_};
keybinder::OnlinePlayerInfoService Keybinder::onlinePlayerInfoService_{};
keybinder::OnlineDataManager Keybinder::onlineDataManager_{};
keybinder::ToastNotification Keybinder::toastNotification_{};
keybinder::Hitmarker Keybinder::hitmarker_{};
keybinder::HitmarkerRenderer Keybinder::hitmarkerRenderer_{};
keybinder::HitsoundPlayer Keybinder::hitsoundPlayer_{};
keybinder::CrosshairSwitcher Keybinder::crosshairSwitcher_{};
keybinder::SquareMap Keybinder::squareMap_{};
keybinder::HudSpriteManager Keybinder::hudSpriteManager_{};
keybinder::WeaponIconManager Keybinder::weaponIconManager_{};
keybinder::InterfaceEditor Keybinder::interfaceEditor_{};
keybinder::NativeHudEditor Keybinder::nativeHudEditor_{};
keybinder::TextureManager Keybinder::textureManager_{};
keybinder::QuickMap Keybinder::quickMap_{};
keybinder::OnlineOverlayRenderer Keybinder::onlineOverlayRenderer_{};
keybinder::OnlineEventDetector Keybinder::onlineEventDetector_{};
keybinder::OnlineOverlayController Keybinder::onlineOverlayController_{};
keybinder::TextDrawAdapter Keybinder::textDrawAdapter_{};
keybinder::SpeedometerManager Keybinder::speedometerManager_{};
keybinder::SpeedometerUI Keybinder::speedometerUI_{};
Keybinder plugin_instance{};

// Definition of DoEmergencyShutdown - must be after Keybinder class definition
// so EmergencyShutdown() can access the static members.
static void DoEmergencyShutdown() {
    Keybinder::EmergencyShutdown();
}

// DllMain CRT sequence on DLL_PROCESS_DETACH:
//   1. _DllMainCRTStartup receives DLL_PROCESS_DETACH
//   2. Calls DllMain(DLL_PROCESS_DETACH) — THIS FUNCTION
//   3. Runs _cexit() -> atexit handlers -> static destructors
// We set g_shutdown_phase here so destructors know to skip cleanup.
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_DETACH) {
        // lpReserved != NULL means process is terminating (ExitProcess/TerminateProcess)
        // lpReserved == NULL means DLL is being unloaded via FreeLibrary
        const bool process_terminating = (lpReserved != nullptr);
        
        char buf[256];
        wsprintfA(buf, "DllMain DLL_PROCESS_DETACH: process_terminating=%d g_shutdown_phase=%d tid=%lu",
            process_terminating ? 1 : 0,
            g_shutdown_phase.load(),
            GetCurrentThreadId());
        OutputDebugStringA(buf);
        
        if (g_shutdown_phase.load() == 0) {
            // shutdownRwEvent never fired - mark emergency exit so destructors
            // know to skip all cleanup (no hook removal, no CRT usage).
            g_shutdown_phase.store(1);
        }
        
        if (process_terminating) {
            // Process is dying. Static destructors are about to run but CRT
            // may be partially torn down. The destructors already check
            // g_shutdown_phase and skip cleanup when != 2/3.
        }
    }
    else if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("[NLBinder] DLL_PROCESS_ATTACH");
    }
    return TRUE;
}
