/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Regression tests: Logger lifecycle, config round-trip with logging, corrupt config recovery
*/
#ifndef KEYBINDER_TESTING
#define KEYBINDER_TESTING
#endif

#include "KeybindTests.h"
#include "../include/Logger.h"
#include "../include/PersistentPaths.h"
#include "../include/SettingsManager.h"
#include "../include/SpeedometerManager.h"
#include "../include/SpeedometerUI.h"
#include "../include/TextDrawAdapter.h"
#include "../include/TextDrawRPC.h"
#include "../include/RakHook/include/RakNet/BitStream.h"
#include "../include/RssTypes.h"

#include <windows.h>
#include <shlobj.h>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <locale>
#include <string>
#include <vector>

namespace {

struct CapturedLog {
    keybinder::LogLevel level;
    std::string message;
};

std::vector<CapturedLog> g_capturedLogs;

void TestLogSink(keybinder::LogLevel level, const char* message) {
    g_capturedLogs.push_back({level, message});
}

void ClearCapturedLogs() {
    g_capturedLogs.clear();
}

void EnableCapturedLogSink() {
#ifdef KEYBINDER_TESTING
    keybinder::Logger::SetTestSink(TestLogSink);
#endif
}

void DisableCapturedLogSink() {
#ifdef KEYBINDER_TESTING
    keybinder::Logger::ResetTestSink();
#endif
}

bool HasLogMatching(keybinder::LogLevel level, const std::string& substring) {
    for (const auto& entry : g_capturedLogs) {
        if (entry.level == level && entry.message.find(substring) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void WriteString16(RakNet::BitStream& bitStream, const std::string& text) {
    bitStream.Write(static_cast<unsigned short>(text.size()));
    if (!text.empty()) {
        bitStream.Write(text.c_str(), static_cast<int>(text.size()));
    }
}

void WriteTextDrawSetStringRpc(RakNet::BitStream& bitStream, int textDrawId, const std::string& text) {
    bitStream.Write(static_cast<unsigned short>(textDrawId));
    WriteString16(bitStream, text);
}

void WriteShowTextDrawRpc(RakNet::BitStream& bitStream, int textDrawId, const std::string& text) {
    bitStream.Write(static_cast<unsigned short>(textDrawId));
    bitStream.Write(static_cast<unsigned char>(0));
    bitStream.Write(0.5f);
    bitStream.Write(0.8f);
    bitStream.Write(static_cast<std::int32_t>(0xFFFFFFFF));
    bitStream.Write(1280.0f);
    bitStream.Write(1280.0f);
    bitStream.Write(static_cast<std::int32_t>(0));
    bitStream.Write(static_cast<unsigned char>(0));
    bitStream.Write(static_cast<unsigned char>(0));
    bitStream.Write(static_cast<std::int32_t>(0));
    bitStream.Write(static_cast<unsigned char>(2));
    bitStream.Write(static_cast<unsigned char>(0));
    bitStream.Write(535.0f);
    bitStream.Write(430.0f);
    bitStream.Write(static_cast<unsigned short>(0));
    bitStream.Write(0.0f);
    bitStream.Write(0.0f);
    bitStream.Write(0.0f);
    bitStream.Write(1.0f);
    bitStream.Write(static_cast<short>(-1));
    bitStream.Write(static_cast<short>(-1));
    WriteString16(bitStream, text);
}

bool ShouldSuppressSpeedometerShowForTest(
    int rpcId,
    bool speedometerEnabled = true,
    bool inVehicleContext = true,
    float posX = 520.0f,
    float posY = 430.0f,
    int style = 1,
    int box = 1,
    int center = 0,
    float boxSizeX = 625.0f,
    float boxSizeY = 440.0f,
    float letterWidth = 0.5f,
    float letterHeight = 0.8f,
    const std::string& rawText = "_") {
    return keybinder::TextDrawAdapter::ShouldSuppressShowTextDrawForTesting(
        rpcId,
        speedometerEnabled,
        inVehicleContext,
        posX,
        posY,
        style,
        box,
        center,
        boxSizeX,
        boxSizeY,
        letterWidth,
        letterHeight,
        rawText);
}

bool DirectoryExists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool RemoveDirectoryRecursive(const std::string& path) {
    if (!DirectoryExists(path)) {
        return true;
    }

    WIN32_FIND_DATAA findData = {};
    const std::string searchPattern = path + "\\*";
    HANDLE findHandle = FindFirstFileA(searchPattern.c_str(), &findData);
    if (findHandle != INVALID_HANDLE_VALUE) {
        do {
            if (std::strcmp(findData.cFileName, ".") == 0 || std::strcmp(findData.cFileName, "..") == 0) {
                continue;
            }

            const std::string entryPath = path + "\\" + findData.cFileName;
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (!RemoveDirectoryRecursive(entryPath)) {
                    FindClose(findHandle);
                    return false;
                }
            } else if (DeleteFileA(entryPath.c_str()) == 0) {
                FindClose(findHandle);
                return false;
            }
        } while (FindNextFileA(findHandle, &findData) != 0);

        FindClose(findHandle);
    }

    return RemoveDirectoryA(path.c_str()) != 0 || !DirectoryExists(path);
}

std::string ReadEntireFile(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return {};
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    if (size <= 0) {
        std::fclose(file);
        return {};
    }

    std::rewind(file);
    std::string contents(static_cast<std::size_t>(size), '\0');
    const std::size_t read = std::fread(&contents[0], 1, contents.size(), file);
    contents.resize(read);
    std::fclose(file);
    return contents;
}

std::string RoundTripQuickMapZoomLevel(const std::string& value) {
    keybinder::SettingsManager settingsManager;
    const std::string json = std::string("{\n")
        + "  \"version\": 1,\n"
        + "  \"quickmap_zoom_level\": \"" + value + "\"\n"
        + "}";

    if (!settingsManager.LoadFromJson(json)) {
        return {};
    }

    keybinder::SettingsManager roundTripManager;
    if (!roundTripManager.LoadFromJson(settingsManager.GetSettingsJson())) {
        return {};
    }

    return roundTripManager.Get().quickmap_zoom_level;
}

std::string RoundTripQuickMapLastZoomLevel(const std::string& value) {
    keybinder::SettingsManager settingsManager;
    const std::string json = std::string("{\n")
        + "  \"version\": 1,\n"
        + "  \"quickmap_last_zoom_level\": \"" + value + "\"\n"
        + "}";

    if (!settingsManager.LoadFromJson(json)) {
        return {};
    }

    keybinder::SettingsManager roundTripManager;
    if (!roundTripManager.LoadFromJson(settingsManager.GetSettingsJson())) {
        return {};
    }

    return roundTripManager.Get().quickmap_last_zoom_level;
}

} // namespace

bool TestLoggerLifecycle() {
    keybinder::Logger::Initialize("test_logger.log");
    keybinder::Logger::Info("Lifecycle test message");
    keybinder::Logger::Shutdown();

    keybinder::Logger::Initialize("test_logger.log");
    keybinder::Logger::Warning("Second cycle message");
    keybinder::Logger::Shutdown();

    std::remove("test_logger.log");
    ASSERT_TRUE(true);
    return true;
}

bool TestLoggerTestSink() {
    ClearCapturedLogs();

    keybinder::Logger::Initialize("test_sink.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    keybinder::Logger::Debug("debug msg");
    keybinder::Logger::Info("info msg");
    keybinder::Logger::Warning("warn msg");
    keybinder::Logger::Error("error msg");

    ASSERT_EQ(g_capturedLogs.size(), 4);
    ASSERT_EQ(g_capturedLogs[0].level, keybinder::LogLevel::Debug);
    ASSERT_TRUE(g_capturedLogs[0].message.find("debug msg") != std::string::npos);
    ASSERT_EQ(g_capturedLogs[1].level, keybinder::LogLevel::Info);
    ASSERT_EQ(g_capturedLogs[2].level, keybinder::LogLevel::Warning);
    ASSERT_EQ(g_capturedLogs[3].level, keybinder::LogLevel::Error);

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_sink.log");
    ClearCapturedLogs();
    return true;
}

bool TestLoggerMinLevel() {
    ClearCapturedLogs();

    keybinder::Logger::Initialize("test_level.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Warning);

    keybinder::Logger::Debug("should not appear");
    keybinder::Logger::Info("should not appear");
    keybinder::Logger::Warning("should appear");
    keybinder::Logger::Error("should appear");

    ASSERT_EQ(g_capturedLogs.size(), 2);
    ASSERT_EQ(g_capturedLogs[0].level, keybinder::LogLevel::Warning);
    ASSERT_EQ(g_capturedLogs[1].level, keybinder::LogLevel::Error);

    DisableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Info);
    keybinder::Logger::Shutdown();
    std::remove("test_level.log");
    ClearCapturedLogs();
    return true;
}

bool TestConfigRoundTripWithLogger() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_rt_log.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    ASSERT_TRUE(keybinder::PersistentPaths::ConfigFile().find("NLBinder") != std::string::npos);

    keybinder::KeybindManager mgr("test_rt_config.json");

    Keybind kb;
    kb.name = "Regression Bind";
    kb.key = 0x52;
    kb.category = "Regression";
    kb.enabled = true;
    kb.passthrough = false;
    kb.actions.push_back(Action(ActionType::Chat, "Regression test"));
    kb.actions.push_back(Action(ActionType::Delay, "100"));

    mgr.Add(kb);
    ASSERT_TRUE(mgr.Save());

    keybinder::KeybindManager mgr2("test_rt_config.json");
    ASSERT_TRUE(mgr2.Load());
    ASSERT_EQ(mgr2.Count(), 1);

    const Keybind* loaded = mgr2.Get(0);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->name, "Regression Bind");
    ASSERT_EQ(loaded->key, 0x52);
    ASSERT_EQ(loaded->actions.size(), 2);

    ASSERT_TRUE(HasLogMatching(keybinder::LogLevel::Info, "Loaded 1 keybind(s)"));

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_rt_config.json");
    std::remove("test_rt_log.log");
    ClearCapturedLogs();
    return true;
}

bool TestEmptyConfigWithLogger() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_empty_log.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    keybinder::KeybindManager mgr("test_nonexistent_config_xyz.json");
    ASSERT_TRUE(mgr.Load());
    ASSERT_EQ(mgr.Count(), 0);

    ASSERT_TRUE(HasLogMatching(keybinder::LogLevel::Info, "No config file"));

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_empty_log.log");
    ClearCapturedLogs();
    return true;
}

bool TestCorruptConfigWithLogger() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_corrupt_log.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    {
        FILE* f = std::fopen("test_corrupt_regression.json", "w");
        if (f) {
            std::fputs("{ totally broken json !!!", f);
            std::fclose(f);
        }
    }

    keybinder::KeybindManager mgr("test_corrupt_regression.json");
    bool result = mgr.Load();
    ASSERT_TRUE(result == false);
    ASSERT_EQ(mgr.Count(), 0);

    ASSERT_TRUE(HasLogMatching(keybinder::LogLevel::Error, "Parse error"));

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_corrupt_regression.json");
    std::remove("test_corrupt_log.log");
    ClearCapturedLogs();
    return true;
}

bool TestLoggerDoubleInitShutdown() {
    keybinder::Logger::Initialize("test_double.log");
    keybinder::Logger::Initialize("test_double.log");
    keybinder::Logger::Info("After double init");
    keybinder::Logger::Shutdown();
    keybinder::Logger::Shutdown();

    std::remove("test_double.log");
    ASSERT_TRUE(true);
    return true;
}

bool TestLoggerTruncatesPreviousSession() {
    keybinder::Logger::Initialize("test_truncate.log");
    keybinder::Logger::Info("First session message");
    keybinder::Logger::Shutdown();

    keybinder::Logger::Initialize("test_truncate.log");
    keybinder::Logger::Info("Second session message");
    keybinder::Logger::Shutdown();

    const std::string contents = ReadEntireFile("test_truncate.log");
    ASSERT_TRUE(!contents.empty());
    ASSERT_TRUE(contents.find("First session message") == std::string::npos);
    ASSERT_TRUE(contents.find("Second session message") != std::string::npos);

    std::remove("test_truncate.log");
    return true;
}

bool TestPersistentPathsRootContainsNLBinder() {
    const std::string root = keybinder::PersistentPaths::RootDir();
    ASSERT_TRUE(!root.empty());
    ASSERT_TRUE(root.find("NLBinder") != std::string::npos);
    return true;
}

bool TestPersistentPathsConfigFileEndsWithNLBinderKeybinderJson() {
    const std::string configFile = keybinder::PersistentPaths::ConfigFile();
    ASSERT_TRUE(configFile.find("NLBinder") != std::string::npos);
    ASSERT_TRUE(EndsWith(configFile, "NLBinder\\keybinder.json"));
    return true;
}

bool TestPersistentPathsLogFileEndsWithNLBinderKeybinderLog() {
    const std::string logFile = keybinder::PersistentPaths::LogFile();
    ASSERT_TRUE(logFile.find("NLBinder") != std::string::npos);
    ASSERT_TRUE(EndsWith(logFile, "NLBinder\\keybinder.log"));
    return true;
}

bool TestPersistentPathsImGuiIniEndsWithNLBinderImguiIni() {
    const std::string imguiIniFile = keybinder::PersistentPaths::ImGuiIniFile();
    ASSERT_TRUE(imguiIniFile.find("NLBinder") != std::string::npos);
    ASSERT_TRUE(EndsWith(imguiIniFile, "NLBinder\\imgui.ini"));
    return true;
}

bool TestPersistentPathsEnsureLayoutCreatesNestedDirectories() {
    const std::string rootDir = keybinder::PersistentPaths::EnsureLayout();
    ASSERT_TRUE(!rootDir.empty());
    ASSERT_TRUE(rootDir.find("NLBinder") != std::string::npos);
    ASSERT_TRUE(DirectoryExists(rootDir));
    ASSERT_TRUE(DirectoryExists(keybinder::PersistentPaths::HitmarkersDir()));
    ASSERT_TRUE(DirectoryExists(keybinder::PersistentPaths::CrosshairsDir()));
    return true;
}

bool TestPersistentPathsRecursiveHitmarkerTempDirectoryCreation() {
    ASSERT_TRUE(!keybinder::PersistentPaths::EnsureLayout().empty());

    const std::string tempDir = keybinder::PersistentPaths::ResolveAssetPath("hitmarkers\\test_temp");
    const std::string nestedDir = keybinder::PersistentPaths::ResolveAssetPath("hitmarkers\\test_temp\\nested_child");
    ASSERT_TRUE(tempDir.find("NLBinder\\hitmarkers\\test_temp") != std::string::npos);

    ASSERT_TRUE(RemoveDirectoryRecursive(tempDir));

    const int createResult = SHCreateDirectoryExA(nullptr, nestedDir.c_str(), nullptr);
    ASSERT_TRUE(createResult == ERROR_SUCCESS || createResult == ERROR_ALREADY_EXISTS);
    ASSERT_TRUE(DirectoryExists(tempDir));
    ASSERT_TRUE(DirectoryExists(nestedDir));

    ASSERT_TRUE(RemoveDirectoryRecursive(tempDir));
    ASSERT_TRUE(!DirectoryExists(tempDir));
    return true;
}

bool TestSettingsExtraction() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_settings.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    {
        FILE* f = std::fopen("test_settings_extract.json", "w");
        if (f) {
            std::fputs("{\n", f);
            std::fputs("  \"version\": 1,\n", f);
            std::fputs("  \"settings\": {\n", f);
            std::fputs("    \"version\": 1,\n", f);
            std::fputs("    \"dpi_scale\": 2.5,\n", f);
            std::fputs("    \"auto_detect_dpi\": false,\n", f);
            std::fputs("    \"ui_open_command_primary\": \"menu\",\n", f);
            std::fputs("    \"ui_open_command_secondary\": \"binder\",\n", f);
            std::fputs("    \"ui_toggle_hotkey\": 75\n", f);
            std::fputs("  },\n", f);
            std::fputs("  \"keybinds\": []\n", f);
            std::fputs("}\n", f);
            std::fclose(f);
        }
    }

    keybinder::KeybindManager mgr("test_settings_extract.json");
    keybinder::SettingsManager settingsMgr;
    ASSERT_TRUE(mgr.Load(&settingsMgr));

    ASSERT_TRUE(settingsMgr.GetDpiScale() > 2.4f && settingsMgr.GetDpiScale() < 2.6f);

    ASSERT_TRUE(settingsMgr.Get().auto_detect_dpi == false);
    ASSERT_TRUE(settingsMgr.Get().auto_hudscalefix);
    ASSERT_EQ(settingsMgr.Get().ui_open_command_primary, std::string("menu"));
    ASSERT_EQ(settingsMgr.Get().ui_open_command_secondary, std::string("binder"));
    ASSERT_EQ(settingsMgr.Get().ui_toggle_hotkey, 'K');

    ASSERT_TRUE(HasLogMatching(keybinder::LogLevel::Info, "Loaded settings"));

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_settings_extract.json");
    std::remove("test_settings.log");
    ClearCapturedLogs();
    return true;
}

bool TestSettingsRoundTrip() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_settings_rt.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    keybinder::KeybindManager mgr("test_settings_roundtrip.json");
    keybinder::SettingsManager settingsMgr;

    settingsMgr.GetMutable().ui_open_command_primary = "/menu";
    settingsMgr.GetMutable().ui_open_command_secondary = "binder";
    settingsMgr.GetMutable().ui_toggle_hotkey = 'K';
    settingsMgr.GetMutable().ui_window_width = 1320.0f;
    settingsMgr.GetMutable().ui_window_height = 880.0f;
    settingsMgr.GetMutable().quickmap_enabled = false;
    settingsMgr.GetMutable().damage_log_enabled = true;
    settingsMgr.GetMutable().quickmap_hotkey = 'Q';
    settingsMgr.GetMutable().quickmap_mode = "hold";
    settingsMgr.GetMutable().theme_name = "Dark";
    settingsMgr.GetMutable().theme_custom_accent = "#12ab34";
    settingsMgr.GetMutable().ui_transparency = 0.35f;
    settingsMgr.GetMutable().squaremap_enabled = true;
    settingsMgr.GetMutable().quickmap_radar_pack = "radar2";
    settingsMgr.GetMutable().fist_texture_path = "fists\\custom.png";
    settingsMgr.GetMutable().hitmarker_active_preset_index = 2;
    settingsMgr.GetMutable().hitmarker_saved_presets.resize(5);
    settingsMgr.GetMutable().hitmarker_saved_presets[2].name = "Meine Desert Eagle";
    settingsMgr.GetMutable().hitmarker_saved_presets[2].shape = keybinder::HitmarkerShape::Circle;
    settingsMgr.GetMutable().hitmarker_saved_presets[2].size = 28.0f;
    settingsMgr.GetMutable().hitmarker_saved_presets[2].opacity = 0.85f;
    settingsMgr.GetMutable().hitmarker_saved_presets[2].duration = 0.7f;
    settingsMgr.GetMutable().hitmarker_saved_presets[2].fadeOut = true;
    settingsMgr.GetMutable().hitmarker_saved_presets[2].color = keybinder::HitmarkerColor{0.9f, 0.3f, 0.3f, 1.0f};
    settingsMgr.GetMutable().hitmarker_saved_presets[2].killConfirmedColor = keybinder::HitmarkerColor{1.0f, 0.85f, 0.2f, 1.0f};
    settingsMgr.GetMutable().hitmarker_saved_presets[2].customImagePath = "hitmarkers\\custom.png";
    settingsMgr.GetMutable().interface_editor_saved_presets.resize(5);
    settingsMgr.GetMutable().interface_editor_active_preset_index = 1;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].name = "Vitals";
    settingsMgr.GetMutable().interface_editor_saved_presets[1].show_hp_number = true;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].show_armor_number = true;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].hp_number_pos_x = -0.08f;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].hp_number_pos_y = 0.04f;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].armor_number_pos_x = 0.03f;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].armor_number_pos_y = -0.02f;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].hp_number_scale = 1.15f;
    settingsMgr.GetMutable().interface_editor_saved_presets[1].armor_number_scale = 0.90f;

    ASSERT_TRUE(mgr.Save(&settingsMgr));

    keybinder::KeybindManager mgr2("test_settings_roundtrip.json");
    keybinder::SettingsManager settingsMgr2;
    ASSERT_TRUE(mgr2.Load(&settingsMgr2));

    const std::string savedJson = settingsMgr.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"dpi_scale\"") == std::string::npos);
    ASSERT_TRUE(savedJson.find("\"auto_detect_dpi\"") == std::string::npos);
    ASSERT_TRUE(savedJson.find("\"ui_window_width\": ") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"ui_window_height\": ") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_enabled\": false") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"damage_log_enabled\": true") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_hotkey\": 81") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_mode\": \"hold\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"ui_transparency\": 0.35") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"show_hp_number\": true") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"show_armor_number\": true") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"hp_number_pos_x\": -0.08") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"hp_number_pos_y\": 0.04") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"armor_number_pos_x\": 0.03") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"armor_number_pos_y\": -0.02") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"hp_number_scale\": 1.15") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"armor_number_scale\": 0.9") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"custom_image_path\": \"hitmarkers\\\\custom.png\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("quickmap_map_image_path") == std::string::npos);
    ASSERT_TRUE(savedJson.find("\"squaremap_enabled\": true") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_radar_pack\": \"radar2\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("hd_sprites_enabled") == std::string::npos);
    ASSERT_TRUE(savedJson.find("hd_radar_pack") == std::string::npos);
    ASSERT_TRUE(savedJson.find("\"fist_texture_path\": \"fists\\\\custom.png\"") != std::string::npos);
    ASSERT_EQ(settingsMgr2.Get().ui_open_command_primary, std::string("menu"));
    ASSERT_EQ(settingsMgr2.Get().ui_open_command_secondary, std::string("binder"));
    ASSERT_EQ(settingsMgr2.Get().ui_toggle_hotkey, 'K');
    ASSERT_TRUE(settingsMgr2.Get().ui_window_width > 1319.9f && settingsMgr2.Get().ui_window_width < 1320.1f);
    ASSERT_TRUE(settingsMgr2.Get().ui_window_height > 879.9f && settingsMgr2.Get().ui_window_height < 880.1f);
    ASSERT_EQ(settingsMgr2.Get().quickmap_enabled, false);
    ASSERT_TRUE(settingsMgr2.Get().damage_log_enabled);
    ASSERT_EQ(settingsMgr2.Get().quickmap_hotkey, 'Q');
    ASSERT_EQ(settingsMgr2.Get().quickmap_mode, std::string("hold"));
    ASSERT_EQ(settingsMgr2.Get().theme_name, std::string("Dark"));
    ASSERT_EQ(settingsMgr2.Get().theme_custom_accent, std::string("#12AB34"));
    ASSERT_TRUE(settingsMgr2.Get().ui_transparency > 0.349f && settingsMgr2.Get().ui_transparency < 0.351f);
    ASSERT_EQ(settingsMgr2.Get().squaremap_enabled, true);
    ASSERT_EQ(settingsMgr2.Get().quickmap_radar_pack, std::string("radar2"));
    ASSERT_EQ(settingsMgr2.Get().fist_texture_path, std::string("fists\\custom.png"));
    ASSERT_EQ(settingsMgr2.Get().hitmarker_active_preset_index, 2);
    ASSERT_EQ(settingsMgr2.Get().hitmarker_saved_presets.size(), static_cast<std::size_t>(5));
    ASSERT_EQ(settingsMgr2.Get().hitmarker_saved_presets[2].name, std::string("Meine Desert Eagle"));
    ASSERT_EQ(settingsMgr2.Get().hitmarker_saved_presets[2].shape, keybinder::HitmarkerShape::Circle);
    ASSERT_EQ(settingsMgr2.Get().hitmarker_saved_presets[2].customImagePath, std::string("hitmarkers\\custom.png"));
    ASSERT_TRUE(settingsMgr2.Get().hitmarker_saved_presets[2].size > 27.9f && settingsMgr2.Get().hitmarker_saved_presets[2].size < 28.1f);
    ASSERT_EQ(settingsMgr2.Get().interface_editor_active_preset_index, 1);
    ASSERT_EQ(settingsMgr2.Get().interface_editor_saved_presets.size(), static_cast<std::size_t>(5));
    ASSERT_EQ(settingsMgr2.Get().interface_editor_saved_presets[1].name, std::string("Vitals"));
    ASSERT_EQ(settingsMgr2.Get().interface_editor_saved_presets[1].show_hp_number, true);
    ASSERT_EQ(settingsMgr2.Get().interface_editor_saved_presets[1].show_armor_number, true);
    ASSERT_TRUE(settingsMgr2.Get().interface_editor_saved_presets[1].hp_number_pos_x > -0.081f && settingsMgr2.Get().interface_editor_saved_presets[1].hp_number_pos_x < -0.079f);
    ASSERT_TRUE(settingsMgr2.Get().interface_editor_saved_presets[1].hp_number_pos_y > 0.039f && settingsMgr2.Get().interface_editor_saved_presets[1].hp_number_pos_y < 0.041f);
    ASSERT_TRUE(settingsMgr2.Get().interface_editor_saved_presets[1].armor_number_pos_x > 0.029f && settingsMgr2.Get().interface_editor_saved_presets[1].armor_number_pos_x < 0.031f);
    ASSERT_TRUE(settingsMgr2.Get().interface_editor_saved_presets[1].armor_number_pos_y > -0.021f && settingsMgr2.Get().interface_editor_saved_presets[1].armor_number_pos_y < -0.019f);
    ASSERT_TRUE(settingsMgr2.Get().interface_editor_saved_presets[1].hp_number_scale > 1.149f && settingsMgr2.Get().interface_editor_saved_presets[1].hp_number_scale < 1.151f);
    ASSERT_TRUE(settingsMgr2.Get().interface_editor_saved_presets[1].armor_number_scale > 0.899f && settingsMgr2.Get().interface_editor_saved_presets[1].armor_number_scale < 0.901f);

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_settings_roundtrip.json");
    std::remove("test_settings_rt.log");
    ClearCapturedLogs();
    return true;
}

bool TestDamageLogSettingsDefaultAndRoundTrip() {
    keybinder::SettingsManager defaults;
    ASSERT_TRUE(!defaults.Get().damage_log_enabled);
    ASSERT_TRUE(defaults.LoadFromJson(R"({"auto_engine_start":true})"));
    ASSERT_TRUE(!defaults.Get().damage_log_enabled);

    keybinder::SettingsManager enabled;
    ASSERT_TRUE(enabled.LoadFromJson(R"({"damage_log_enabled":true,"auto_engine_start":false,"kill_tracker_enabled":false})"));
    ASSERT_TRUE(enabled.Get().damage_log_enabled);
    ASSERT_TRUE(!enabled.Get().auto_engine_start);
    ASSERT_TRUE(!enabled.Get().kill_tracker_enabled);
    ASSERT_TRUE(enabled.GetSettingsJson().find("\"damage_log_enabled\": true") != std::string::npos);
    keybinder::SettingsManager reloaded;
    ASSERT_TRUE(reloaded.LoadFromJson(enabled.GetSettingsJson()));
    ASSERT_TRUE(reloaded.Get().damage_log_enabled);

    enabled.GetMutable().damage_log_enabled = false;
    ASSERT_TRUE(enabled.GetSettingsJson().find("\"damage_log_enabled\": false") != std::string::npos);
    ASSERT_TRUE(reloaded.LoadFromJson(enabled.GetSettingsJson()));
    ASSERT_TRUE(!reloaded.Get().damage_log_enabled);
    ASSERT_TRUE(reloaded.LoadFromJson(R"({"damage_log_enabled":"yes"})"));
    ASSERT_TRUE(!reloaded.Get().damage_log_enabled);
    return true;
}

bool TestSquareMapFistAndQuickMapRadarPackRoundTrip() {
    keybinder::SettingsManager missingManager;
    ASSERT_TRUE(missingManager.LoadFromJson(R"json({
  "version": 1,
  "crosshair_enabled": true
})json"));
    ASSERT_EQ(missingManager.Get().squaremap_enabled, false);
    ASSERT_EQ(missingManager.Get().quickmap_radar_pack, std::string("radar1"));
    ASSERT_EQ(missingManager.Get().fist_texture_path, std::string(""));

    keybinder::SettingsManager roundTripManager;
    roundTripManager.GetMutable().squaremap_enabled = true;
    roundTripManager.GetMutable().quickmap_radar_pack = "radar2";
    roundTripManager.GetMutable().fist_texture_path = "fists\\custom.png";
    const std::string savedJson = roundTripManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"squaremap_enabled\": true") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_radar_pack\": \"radar2\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("hd_sprites_enabled") == std::string::npos);
    ASSERT_TRUE(savedJson.find("hd_radar_pack") == std::string::npos);
    ASSERT_TRUE(savedJson.find("\"fist_texture_path\": \"fists\\\\custom.png\"") != std::string::npos);

    keybinder::SettingsManager reloaded;
    ASSERT_TRUE(reloaded.LoadFromJson(savedJson));
    ASSERT_EQ(reloaded.Get().squaremap_enabled, true);
    ASSERT_EQ(reloaded.Get().quickmap_radar_pack, std::string("radar2"));
    ASSERT_EQ(reloaded.Get().fist_texture_path, std::string("fists\\custom.png"));
    return true;
}

bool TestThemeNameSavedToJson() {
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().theme_name = "dark";

    const std::string json = settingsManager.GetSettingsJson();
    ASSERT_TRUE(json.find("\"theme_name\": \"Dark\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"theme_custom_accent\": \"\"") != std::string::npos);
    return true;
}

bool TestUiTransparencyClampsAndRoundTrips() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "ui_transparency": 0.95
})json"));
    ASSERT_TRUE(settingsManager.Get().ui_transparency > 0.699f && settingsManager.Get().ui_transparency < 0.701f);

    keybinder::SettingsManager negativeManager;
    ASSERT_TRUE(negativeManager.LoadFromJson(R"json({
  "version": 1,
  "ui_transparency": -0.25
})json"));
    ASSERT_TRUE(negativeManager.Get().ui_transparency > -0.001f && negativeManager.Get().ui_transparency < 0.001f);

    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"ui_transparency\": 0.7") != std::string::npos);
    return true;
}


bool TestThemeNameLoadedFromJson() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "theme_name": "classic"
})json"));

    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Dark"));
    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    return true;
}

bool TestThemeNameRecognizesAllBuiltIns() {
    keybinder::SettingsManager settingsManager;
    settingsManager.GetMutable().theme_name = "ocean";
    ASSERT_TRUE(settingsManager.GetSettingsJson().find("\"theme_name\": \"Ocean\"") != std::string::npos);

    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "theme_name": "forest"
})json"));
    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Forest"));

    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "theme_name": "ember"
})json"));
    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Ember"));
    return true;
}

bool TestMissingThemeNameDefaultsToDark() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "quickmap_mode": "toggle"
})json"));

    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Dark"));
    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    return true;
}

bool TestInvalidThemeNameDefaultsToDark() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "theme_name": "obsidian"
})json"));

    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Dark"));
    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    return true;
}

bool TestLegacyCustomAccentOnlyDefaultsToDark() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "theme_custom_accent": "#12ab34"
})json"));

    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Dark"));
    ASSERT_EQ(settingsManager.Get().theme_custom_accent, std::string("#12AB34"));

    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"theme_custom_accent\": \"#12AB34\"") != std::string::npos);
    return true;
}

bool TestSettingsIgnoreDeprecatedThemeFields() {
    keybinder::SettingsManager settingsManager;
    const std::string json = R"json({
  "version": 1,
  "theme_selected": "obsidian",
  "theme_accent_mode": "preset",
  "theme_accent_preset": "Electric Blue",
  "theme_custom_accent": "#12ab34"
})json";

    ASSERT_TRUE(settingsManager.LoadFromJson(json));
    ASSERT_EQ(settingsManager.Get().theme_name, std::string("Dark"));
    ASSERT_EQ(settingsManager.Get().theme_custom_accent, std::string("#12AB34"));

    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"theme_custom_accent\": \"#12AB34\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("theme_selected") == std::string::npos);
    ASSERT_TRUE(savedJson.find("theme_accent_mode") == std::string::npos);
    ASSERT_TRUE(savedJson.find("theme_accent_preset") == std::string::npos);
    return true;
}

bool TestSettingsRoundTripWithRssFeeds() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_rss_rt.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    keybinder::KeybindManager mgr("test_rss_roundtrip.json");
    keybinder::SettingsManager settingsMgr;

    settingsMgr.GetMutable().rss_enabled = true;
    settingsMgr.GetMutable().rss_notification_mode = keybinder::RssNotificationMode::Chat;
    keybinder::RssFeedConfig feed;
    feed.url = "https://forum.example.com/feed";
    feed.label = "My Forum";
    feed.pollIntervalSeconds = 600;
    feed.enabled = true;
    feed.lastModified = "Wed, 01 Jan 2025 00:00:00 GMT";
    feed.etag = "\"abc123\"";
    keybinder::RssSeenGuid seenGuid;
    seenGuid.guid = "post-42";
    seenGuid.seenAt = 1704067200;
    feed.seen_guids.push_back(seenGuid);
    settingsMgr.GetMutable().rss_feeds.push_back(feed);

    ASSERT_TRUE(mgr.Save(&settingsMgr));

    keybinder::KeybindManager mgr2("test_rss_roundtrip.json");
    keybinder::SettingsManager settingsMgr2;
    ASSERT_TRUE(mgr2.Load(&settingsMgr2));

    ASSERT_EQ(settingsMgr2.Get().rss_enabled, true);
    ASSERT_EQ(settingsMgr2.Get().rss_notification_mode, keybinder::RssNotificationMode::Chat);
    ASSERT_EQ(settingsMgr2.Get().rss_feeds.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(settingsMgr2.Get().rss_feeds[0].url, std::string("https://forum.example.com/feed"));
    ASSERT_EQ(settingsMgr2.Get().rss_feeds[0].label, std::string("My Forum"));
    ASSERT_EQ(settingsMgr2.Get().rss_feeds[0].pollIntervalSeconds, 600);
    ASSERT_EQ(settingsMgr2.Get().rss_feeds[0].enabled, true);
    ASSERT_EQ(settingsMgr2.Get().rss_feeds[0].seen_guids.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(settingsMgr2.Get().rss_feeds[0].seen_guids[0].guid, std::string("post-42"));

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_rss_roundtrip.json");
    std::remove("test_rss_rt.log");
    ClearCapturedLogs();
    return true;
}

bool TestSettingsQuickMapInvalidHotkeyFallsBackToM() {
    keybinder::SettingsManager settingsManager;
    const std::string json = R"json({
  "version": 1,
  "quickmap_enabled": false,
  "quickmap_hotkey": 0,
  "quickmap_mode": "toggle",
  "quickmap_map_image_path": "gtasamap.jpg"
})json";

    ASSERT_TRUE(settingsManager.LoadFromJson(json));
    ASSERT_EQ(settingsManager.Get().quickmap_enabled, false);
    ASSERT_EQ(settingsManager.Get().quickmap_hotkey, 'M');
    ASSERT_EQ(settingsManager.Get().quickmap_mode, std::string("toggle"));

    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"quickmap_hotkey\": 77") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_mode\": \"toggle\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("quickmap_map_image_path") == std::string::npos);
    return true;
}

bool TestSettingsQuickMapFollowPlayerDefaultsAndRoundTrips() {
    keybinder::SettingsManager defaultsManager;
    ASSERT_TRUE(defaultsManager.LoadFromJson(R"json({
  "version": 1,
  "quickmap_enabled": true,
  "quickmap_hotkey": 77,
  "quickmap_mode": "toggle"
})json"));
    ASSERT_EQ(defaultsManager.Get().quickmap_auto_zoom, false);
    ASSERT_EQ(defaultsManager.Get().quickmap_follow_player, true);
    ASSERT_EQ(defaultsManager.Get().quickmap_focus_marker, true);

    keybinder::SettingsManager configuredManager;
    ASSERT_TRUE(configuredManager.LoadFromJson(R"json({
  "version": 1,
  "quickmap_enabled": true,
  "quickmap_hotkey": 77,
  "quickmap_mode": "toggle",
  "quickmap_auto_zoom": true,
  "quickmap_follow_player": false,
  "quickmap_focus_marker": false
})json"));
    ASSERT_EQ(configuredManager.Get().quickmap_auto_zoom, true);
    ASSERT_EQ(configuredManager.Get().quickmap_follow_player, false);
    ASSERT_EQ(configuredManager.Get().quickmap_focus_marker, false);

    const std::string savedJson = configuredManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"quickmap_auto_zoom\": true") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_follow_player\": false") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_focus_marker\": false") != std::string::npos);
    return true;
}

bool TestSettingsQuickMapZoomLevelDefaultsToFar() {
    keybinder::SettingsManager settingsManager;
    ASSERT_EQ(settingsManager.Get().quickmap_zoom_level, std::string("far"));
    ASSERT_EQ(settingsManager.Get().quickmap_last_zoom_level, std::string("far"));

    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "quickmap_mode": "toggle"
})json"));
    ASSERT_EQ(settingsManager.Get().quickmap_zoom_level, std::string("far"));
    ASSERT_EQ(settingsManager.Get().quickmap_last_zoom_level, std::string("far"));

    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"quickmap_zoom_level\": \"far\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_last_zoom_level\": \"far\"") != std::string::npos);
    return true;
}

bool TestSettingsQuickMapLastZoomLevelDefaultsToConfiguredZoomLevelForLegacyConfig() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "quickmap_zoom_level": "close"
})json"));

    ASSERT_EQ(settingsManager.Get().quickmap_zoom_level, std::string("close"));
    ASSERT_EQ(settingsManager.Get().quickmap_last_zoom_level, std::string("close"));
    return true;
}

bool TestSettingsQuickMapLastZoomLevelRoundTripsIndependently() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "quickmap_zoom_level": "medium",
  "quickmap_last_zoom_level": "ultra"
})json"));

    ASSERT_EQ(settingsManager.Get().quickmap_zoom_level, std::string("medium"));
    ASSERT_EQ(settingsManager.Get().quickmap_last_zoom_level, std::string("ultra"));

    const std::string savedJson = settingsManager.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"quickmap_zoom_level\": \"medium\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"quickmap_last_zoom_level\": \"ultra\"") != std::string::npos);
    ASSERT_EQ(RoundTripQuickMapLastZoomLevel("very_close"), std::string("very_close"));
    return true;
}

bool TestSettingsQuickMapZoomLevelMediumRoundTrips() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel("medium"), std::string("medium"));
    return true;
}

bool TestSettingsQuickMapZoomLevelCloseRoundTrips() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel("close"), std::string("close"));
    return true;
}

bool TestSettingsQuickMapZoomLevelVeryCloseRoundTrips() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel("very_close"), std::string("very_close"));
    return true;
}

bool TestSettingsQuickMapZoomLevelUltraRoundTrips() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel("ultra"), std::string("ultra"));
    return true;
}

bool TestSettingsQuickMapZoomLevelInvalidFallsBackToFar() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel("super_zoom"), std::string("far"));
    return true;
}

bool TestSettingsQuickMapZoomLevelEmptyFallsBackToFar() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel(""), std::string("far"));
    return true;
}

bool TestSettingsQuickMapZoomLevelIsCaseInsensitive() {
    ASSERT_EQ(RoundTripQuickMapZoomLevel("MEDIUM"), std::string("medium"));
    return true;
}

bool TestSettingsHitsoundRoundTrip() {
    keybinder::SettingsManager settingsMgr;

    settingsMgr.GetMutable().hitsound_outgoing_enabled = false;
    settingsMgr.GetMutable().hitsound_outgoing_path = "hitsounds\\outgoing.wav";
    settingsMgr.GetMutable().hitsound_outgoing_volume = 0.35f;
    settingsMgr.GetMutable().hitsound_incoming_enabled = true;
    settingsMgr.GetMutable().hitsound_incoming_path = "hitsounds\\incoming.wav";
    settingsMgr.GetMutable().hitsound_incoming_volume = 0.60f;

    const std::string json = settingsMgr.GetSettingsJson();

    ASSERT_TRUE(json.find("\"hitsound_outgoing_enabled\": false") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_outgoing_path\": \"hitsounds\\\\outgoing.wav\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_outgoing_volume\": 0.35") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_incoming_enabled\": true") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_incoming_path\": \"hitsounds\\\\incoming.wav\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_incoming_volume\": 0.6") != std::string::npos);

    keybinder::SettingsManager settingsMgr2;
    ASSERT_TRUE(settingsMgr2.LoadFromJson(json));

    ASSERT_EQ(settingsMgr2.Get().hitsound_outgoing_enabled, false);
    ASSERT_EQ(settingsMgr2.Get().hitsound_outgoing_path, std::string("hitsounds\\outgoing.wav"));
    ASSERT_EQ(settingsMgr2.Get().hitsound_outgoing_volume, 0.35f);
    ASSERT_EQ(settingsMgr2.Get().hitsound_incoming_enabled, true);
    ASSERT_EQ(settingsMgr2.Get().hitsound_incoming_path, std::string("hitsounds\\incoming.wav"));
    ASSERT_EQ(settingsMgr2.Get().hitsound_incoming_volume, 0.60f);

    return true;
}

bool TestSettingsHitsoundDefaultValues() {
    keybinder::SettingsManager settingsMgr;

    ASSERT_EQ(settingsMgr.Get().hitsound_outgoing_enabled, true);
    ASSERT_EQ(settingsMgr.Get().hitsound_outgoing_path, std::string(""));
    ASSERT_EQ(settingsMgr.Get().hitsound_outgoing_volume, 0.08f);
    ASSERT_EQ(settingsMgr.Get().hitsound_incoming_enabled, true);
    ASSERT_EQ(settingsMgr.Get().hitsound_incoming_path, std::string(""));
    ASSERT_EQ(settingsMgr.Get().hitsound_incoming_volume, 0.08f);

    const std::string json = settingsMgr.GetSettingsJson();
    ASSERT_TRUE(json.find("\"hitsound_outgoing_enabled\": true") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_outgoing_path\": \"\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_outgoing_volume\": 0.08") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_incoming_enabled\": true") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_incoming_path\": \"\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"hitsound_incoming_volume\": 0.08") != std::string::npos);

    keybinder::SettingsManager settingsMgr2;
    ASSERT_TRUE(settingsMgr2.LoadFromJson(json));
    ASSERT_EQ(settingsMgr2.Get().hitsound_outgoing_enabled, true);
    ASSERT_EQ(settingsMgr2.Get().hitsound_outgoing_path, std::string(""));
    ASSERT_EQ(settingsMgr2.Get().hitsound_outgoing_volume, 0.08f);
    ASSERT_EQ(settingsMgr2.Get().hitsound_incoming_enabled, true);
    ASSERT_EQ(settingsMgr2.Get().hitsound_incoming_path, std::string(""));
    ASSERT_EQ(settingsMgr2.Get().hitsound_incoming_volume, 0.08f);

    return true;
}

bool TestSettingsHitsoundPathNormalization() {
    keybinder::SettingsManager settingsMgr;

    const std::string json = R"json({
  "version": 1,
  "hitsound_outgoing_path": "hitsounds/outgoing.wav",
  "hitsound_outgoing_volume": 1.75,
  "hitsound_incoming_path": "hitsoundsincoming.wav",
  "hitsound_incoming_volume": -5.0
})json";

    ASSERT_TRUE(settingsMgr.LoadFromJson(json));

    ASSERT_EQ(settingsMgr.Get().hitsound_outgoing_path, std::string("hitsounds\\outgoing.wav"));
    ASSERT_EQ(settingsMgr.Get().hitsound_outgoing_volume, 1.0f);
    ASSERT_EQ(settingsMgr.Get().hitsound_incoming_path, std::string("hitsounds\\incoming.wav"));
    ASSERT_EQ(settingsMgr.Get().hitsound_incoming_volume, 0.0f);

    return true;
}

bool TestSettingsNestedThemeNameExtractionThroughKeybindManager() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_theme_nested.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    {
        FILE* f = std::fopen("test_theme_nested.json", "w");
        if (f) {
            std::fputs("{\n", f);
            std::fputs("  \"settings\": {\n", f);
            std::fputs("    \"version\": 1,\n", f);
            std::fputs("    \"theme_name\": \"classic\",\n", f);
            std::fputs("    \"theme_custom_accent\": \"#7c3aed\"\n", f);
            std::fputs("  },\n", f);
            std::fputs("  \"keybinds\": []\n", f);
            std::fputs("}\n", f);
            std::fclose(f);
        }
    }

    keybinder::KeybindManager mgr("test_theme_nested.json");
    keybinder::SettingsManager settingsMgr;
    ASSERT_TRUE(mgr.Load(&settingsMgr));

    ASSERT_EQ(settingsMgr.Get().theme_name, std::string("Dark"));
    ASSERT_EQ(settingsMgr.Get().theme_custom_accent, std::string("#7C3AED"));

    const std::string savedJson = settingsMgr.GetSettingsJson();
    ASSERT_TRUE(savedJson.find("\"theme_name\": \"Dark\"") != std::string::npos);
    ASSERT_TRUE(savedJson.find("\"theme_custom_accent\": \"#7C3AED\"") != std::string::npos);

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_theme_nested.json");
    std::remove("test_theme_nested.log");
    ClearCapturedLogs();
    return true;
}

bool TestSettingsSpeedometerDefaultsAndRoundTrip() {
    keybinder::SettingsManager settingsManager;
    ASSERT_EQ(settingsManager.Get().speedometer_enabled, false);
    ASSERT_EQ(settingsManager.Get().speedometer_style, std::string("analog_arc"));

    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "speedometer_enabled": true,
  "speedometer_style": "tachometer_arc",
  "speedometer_hide_textdraw_ids": [5, 10, 15],
  "speedometer_fuel_textdraw_ids": [20]
})json"));

    ASSERT_EQ(settingsManager.Get().speedometer_enabled, true);
    ASSERT_EQ(settingsManager.Get().speedometer_style, std::string("tachometer_arc"));
    ASSERT_TRUE(settingsManager.GetSettingsJson().find("\"speedometer_enabled\": true") != std::string::npos);

    keybinder::SettingsManager roundTripManager;
    ASSERT_TRUE(roundTripManager.LoadFromJson(settingsManager.GetSettingsJson()));
    ASSERT_EQ(roundTripManager.Get().speedometer_enabled, true);
    ASSERT_EQ(roundTripManager.Get().speedometer_style, std::string("tachometer_arc"));

    keybinder::SettingsManager minimalStyleManager;
    ASSERT_TRUE(minimalStyleManager.LoadFromJson(R"json({ "version": 1, "speedometer_style": "minimal_digital" })json"));
    ASSERT_EQ(minimalStyleManager.Get().speedometer_style, std::string("minimal_digital"));
    ASSERT_TRUE(minimalStyleManager.GetSettingsJson().find("\"speedometer_style\": \"minimal_digital\"") != std::string::npos);

    const std::string json = keybinder::SettingsManager().GetSettingsJson();
    ASSERT_TRUE(json.find("\"speedometer_enabled\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_style\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_hide_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_fuel_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_debug_log_rpcs\"") == std::string::npos);
    return true;
}

bool TestSettingsIgnoresLegacySpeedometerTextDrawIds() {
    ClearCapturedLogs();
    keybinder::Logger::Initialize("test_speedometer_settings.log");
    EnableCapturedLogSink();
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);

    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "speedometer_hide_textdraw_ids": [5, 9999, -1],
  "speedometer_fuel_textdraw_ids": [2303, 2304]
})json"));

    const std::string json = settingsManager.GetSettingsJson();
    ASSERT_TRUE(json.find("\"speedometer_hide_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_fuel_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(!HasLogMatching(keybinder::LogLevel::Warning, "TextDraw ID"));

    DisableCapturedLogSink();
    keybinder::Logger::Shutdown();
    std::remove("test_speedometer_settings.log");
    ClearCapturedLogs();
    return true;
}

bool TestSettingsSpeedometerLegacyIdFieldsDoNotRoundTrip() {
    keybinder::SettingsManager settingsManager;
    ASSERT_TRUE(settingsManager.LoadFromJson(R"json({
  "version": 1,
  "speedometer_hide_textdraw_ids": [],
  "speedometer_fuel_textdraw_ids": []
})json"));

    const std::string json = settingsManager.GetSettingsJson();
    ASSERT_TRUE(json.find("\"speedometer_hide_textdraw_ids\"") == std::string::npos);
    ASSERT_TRUE(json.find("\"speedometer_fuel_textdraw_ids\"") == std::string::npos);
    return true;
}

bool TestTextDrawRpcConstants() {
    ASSERT_EQ(textdraw::RPC_SCR_SHOW_TEXTDRAW, static_cast<unsigned char>(134));
    ASSERT_EQ(textdraw::RPC_SCR_HIDE_TEXTDRAW, static_cast<unsigned char>(135));
    ASSERT_EQ(textdraw::RPC_SCR_TEXTDRAW_SET_STRING, static_cast<unsigned char>(105));
    return true;
}

bool TestTextDrawFuelParsingWithColorCodes() {
    ASSERT_EQ(keybinder::TextDrawAdapter::StripColorCodesForTesting("~g~75~w~%"), std::string("75%"));
    ASSERT_EQ(keybinder::TextDrawAdapter::StripColorCodesForTesting("{FF0000}Fuel: ~y~42~w~%"), std::string("Fuel: 42%"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("~g~75~w~%"), 75.0f);
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("{FF0000}Fuel: ~y~42~w~%"), 42.0f);
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("100%"), 100.0f);
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting("Fuel unavailable"), -1.0f);
    return true;
}

bool TestTextDrawRpcParsesSetStringFuelText() {
    RakNet::BitStream bitStream;
    WriteTextDrawSetStringRpc(bitStream, 2090, "100%");

    int textDrawId = -1;
    std::string text;
    ASSERT_TRUE(keybinder::TextDrawAdapter::TryParseRpcTextForTesting(textdraw::RPC_SCR_TEXTDRAW_SET_STRING, &bitStream, textDrawId, text));
    ASSERT_EQ(textDrawId, 2090);
    ASSERT_EQ(text, std::string("100%"));
    ASSERT_EQ(bitStream.GetReadOffset(), 0);
    return true;
}

bool TestTextDrawRpcParsesShowTextDrawFuelText() {
    RakNet::BitStream bitStream;
    WriteShowTextDrawRpc(bitStream, 2090, "100%");

    int textDrawId = -1;
    std::string text;
    ASSERT_TRUE(keybinder::TextDrawAdapter::TryParseRpcTextForTesting(textdraw::RPC_SCR_SHOW_TEXTDRAW, &bitStream, textDrawId, text));
    ASSERT_EQ(textDrawId, 2090);
    ASSERT_EQ(text, std::string("100%"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ParseFuelValueForTesting(text), 100.0f);
    ASSERT_EQ(bitStream.GetReadOffset(), 0);
    return true;
}

bool TestTextDrawCachesFuelFromPercentTextDrawOnly() {
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2090, true, true, "100%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(42, true, true, "100%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2092, true, true, "100%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2095, true, true, "25%"));

    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2090, true, true, "_"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2090, true, false, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2090, false, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2087, true, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(39, true, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2098, true, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2099, true, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2089, true, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2094, true, true, "168"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldCacheFuelTextForTesting(2096, true, true, "100%"));
    return true;
}

bool TestTextDrawMatchesHardcodedTachoIdsFromLocalRpcIds() {
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(4));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(21));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(22));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(23));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(36));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(37));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(38));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(39));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(40));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(41));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(42));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(43));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(44));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2052));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2069));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2070));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2071));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2084));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2085));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2086));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2087));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2088));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2089));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2090));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2091));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2092));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2093));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2094));
    ASSERT_TRUE(keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2095));

    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(3));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(18));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(19));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(27));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(32));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(33));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(34));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(35));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(50));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(51));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(52));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(53));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2080));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2081));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2082));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2083));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2098));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2099));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2100));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::TextDrawIdMatchesForTesting(2101));
    return true;
}

bool TestTextDrawSuppressesTachoV2ShowByGeometry() {
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(4, true, true, 575.0f, 426.0f, 1, 1, 1, 307.0f, 155.5f, 0.6f, 2.25f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(5, true, true, 595.0f, 424.5f, 1, 0, 1, 400.0f, 107.0f, 0.224996f, 1.15f, "Tankvolumen"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(22, true, true, 535.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "KM/H"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2085, true, true, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(37, true, true, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2086, true, true, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2087, true, true, 561.45f, 438.0f, 1, 0, 0, 558.95f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2087, true, true, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2088, true, true, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2088, true, true, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "NRG-500"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2084, true, true, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2090, true, true, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Bus"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2089, true, true, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "24"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2091, true, true, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2090, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2092, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(42, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    // Live-server packing observed 2026-05-24: speed digit at id 2094,
    // fuel % at id 2095. The classifier matches purely on position now that
    // the dynamic id range is wide enough to reach them.
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2094, true, true, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2094, true, true, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "168"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2095, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "25%"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2095, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));

    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2083, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~~y~NUM2 ~w~Radio bedienen~n~"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2082, true, true, 455.0f, 455.0f, 1, 1, 0, 640.0f, 480.0f, 1.0f, -5.0f, "_"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2081, true, true, 455.0f, 455.0f, 1, 1, 0, 640.0f, 480.0f, 1.0f, -5.0f, "_"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2088, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "Inventar"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2088, true, false, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2090, false, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    return true;
}

bool TestTextDrawPreservesNonTachoPlayerOverlayFromDump() {
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2076, true, true, 104.0f, 306.0f, 1, 0, 0, 251.5f, 17.0f, 0.262499f, 1.1f, " 89"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2075, true, true, 578.0f, 67.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.31f, 0.799999f, "50"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2077, true, true, 77.0f, 306.0f, 1, 0, 0, 251.5f, 17.0f, 0.262499f, 1.1f, " 93"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2078, true, true, 50.0f, 306.0f, 1, 0, 0, 251.5f, 17.0f, 0.262499f, 1.1f, " 90"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2081, true, true, 50.0f, 306.0f, 1, 0, 0, 251.5f, 17.0f, 0.262499f, 1.1f, "100"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2079, true, true, 134.0f, 401.0f, 1, 0, 0, 288.5f, 208.5f, 0.174998f, 1.4f, "~g~Zahltag~w~ von ca. ~g~0$~w~ in 60 Minuten"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2080, true, true, 134.0f, 401.0f, 1, 0, 0, 288.5f, 208.5f, 0.174998f, 1.4f, " 61"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2081, true, true, 320.0f, 320.0f, 1, 0, 1, 300.0f, 300.0f, 0.2f, 1.5f, "Hol den Fahrgast in Prickle Pine ab"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2082, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~~y~NUM2 ~w~Radio bedienen~n~"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2085, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~~y~NUM2 ~w~Radio bedienen~n~"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2083, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~~k~~GROUP_CONTROL_BWD~ ~w~Inventar~n~"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2088, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~~y~NUM2 ~w~Radio bedienen~n~"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2076, true, true, " 89"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2077, true, true, " 93"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2078, true, true, " 90"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2079, true, true, " 66"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2080, true, true, " 61"));
    return true;
}

bool TestTextDrawDoesNotSuppressAlternativeTachoFromDump() {
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(18, true, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(1, true, true, 546.0f, 66.7f, 4, 0, 0, 61.7f, 8.4f, 0.48f, 1.12f, "LD_SPAC:white"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2, true, true, 548.0f, 68.8f, 4, 0, 0, 57.8f, 4.7f, 0.48f, 1.12f, "LD_SPAC:white"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2080, true, true, 548.0f, 68.8f, 4, 0, 0, 28.9f, 4.7f, 0.48f, 1.12f, "LD_SPAC:white"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2081, true, true, 455.0f, 455.0f, 1, 1, 0, 640.0f, 480.0f, 1.0f, -5.0f, "_"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2082, true, true, 456.0f, 416.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.4f, 0.8f, "Clover"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2084, true, true, 456.0f, 431.0f, 2, 0, 0, 1280.0f, 1280.0f, 0.3f, 0.9f, "0 km/h"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2098, true, true, 535.0f, 430.0f, 2, 0, 0, 1280.0f, 1280.0f, 0.5f, 0.8f, "100%"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2100, true, true, 557.5f, 400.5f, 1, 0, 0, 1280.0f, 1280.0f, 0.4f, 0.8f, "3.4KM"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(18, true, true, "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2050, true, true, "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2084, 456.0f, 431.0f, 2, 0, 0, 1280.0f, 1280.0f, 0.3f, 0.9f, "0 km/h",
        2084, true, true, "10 km/h"));
    return true;
}

bool TestTextDrawSuppressesTachoV2FromDiagnostics() {
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2089, true, true, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Sweeper"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2084, true, true, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2090, true, true, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2091, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2085, true, true, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"));
    // Live 2026-05-31 capture: the gauge bar background box arrives at id 2082
    // (two below the dynamic-first vehicle-name id) while the colored fill lands
    // at 2084. Both must be suppressed; before the gauge-candidate floor was
    // lowered, 2082 fell through to Unknown and left an opaque black bar.
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2082, true, true, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2084, true, true, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2086, true, true, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2087, true, true, 561.45f, 438.0f, 1, 0, 0, 558.95f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2087, true, true, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2088, true, true, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(3, true, true, 575.0f, 426.0f, 1, 1, 1, 307.0f, 155.5f, 0.6f, 2.25f, "_"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(4, true, true, 595.0f, 424.5f, 1, 0, 1, 400.0f, 107.0f, 0.224996f, 1.15f, "Tankvolumen"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(22, true, true, 535.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "KM/H"));

    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2083, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~~y~NUM2 ~w~Radio bedienen~n~"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2083, true, true, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~~k~~GROUP_CONTROL_BWD~ ~w~Inventar~n~"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2079, true, true, 134.0f, 401.0f, 1, 0, 0, 288.5f, 208.5f, 0.174998f, 1.4f, " 66"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2080, true, true, 134.0f, 401.0f, 1, 0, 0, 288.5f, 208.5f, 0.174998f, 1.4f, " 61"));
    return true;
}

bool TestTextDrawDoesNotSuppressSetStringRpc() {
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2089, true, true, "Sweeper"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2090, true, true, "0"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2091, true, true, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2083, true, true, "~y~Z ~w~Motor bedienen~n~"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(2080, true, true, " 61"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(21, true, true, "102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(21, true, true, "~g~~h~~h~fuel"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(21, true, false, "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(21, false, true, "~g~~h~~h~102"));
    return true;
}

bool TestTextDrawSuppressesTachoSetStringRpcFromDiagnostics() {
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2089, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Sweeper",
        2089, true, true, "Sweeper"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2090, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0",
        2090, true, true, "24"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2091, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%",
        2091, true, true, "99%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2084, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus",
        2084, true, true, "Turismo"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2090, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Bus",
        2090, true, true, "Bus"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2091, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0",
        2091, true, true, "0"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2092, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%",
        2092, true, true, "100%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2087, 561.45f, 438.0f, 1, 0, 0, 558.95f, 0.0f, 1.0f, 0.55f, "_",
        2087, true, true, "_"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2088, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_",
        2088, true, true, "_"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        39, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_",
        2087, true, true, "_"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2091, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%",
        42, true, true, "99%"));

    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2083, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~",
        2083, true, true, "~y~Z ~w~Motor bedienen~n~"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2084, 456.0f, 431.0f, 2, 0, 0, 1280.0f, 1280.0f, 0.3f, 0.9f, "0 km/h",
        2084, true, true, "10 km/h"));
    return true;
}

bool TestTextDrawDoesNotApplyAbsoluteTachoCacheToServerNumericAliases() {
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2089, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "24",
        41, true, true, "100"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2090, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0",
        42, true, true, "100"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2087, 561.45f, 438.0f, 1, 0, 0, 558.95f, 0.0f, 1.0f, 0.55f, "_",
        39, true, true, "42"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2088, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Reefer",
        40, true, true, "75"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
        2089, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "24",
        35, true, true, "~g~~h~~h~102"));
    return true;
}

bool TestTextDrawDoesNotReuseShowCacheFromInactiveContext() {
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowContextChangeForTesting(
        2088, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "NRG-500",
        true, false,
        2088, true, true, "Infernus"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowContextChangeForTesting(
        2089, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0",
        false, true,
        2089, true, true, "24"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowContextChangeForTesting(
        2088, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "NRG-500",
        true, true,
        2088, true, true, "Infernus"));
    return true;
}

bool TestTextDrawHideClearsCachedShowClassification() {
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowThenHideForTesting(
        2088, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "NRG-500",
        2088, 2088, true, true, "Infernus"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowThenHideForTesting(
        39, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_",
        2087, 39, true, true, "_"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringAfterShowThenHideForTesting(
        2087, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_",
        39, 2087, true, true, "_"));
    return true;
}

bool TestTextDrawSuppressesTachoV2LocalSpeedSetStringRpc() {
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, true, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        21, true, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));

    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, true, false, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, false, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, true, true, 577.0f, 8.0f, 3, 0, 1, 400.0f, 17.0f, 0.266665f, 1.3f, "21.05.2026", "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, true, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "102"));
    return true;
}

bool TestTextDrawSuppressionRequiresVehicleContext() {
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2089, true, true, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Sweeper"));
    ASSERT_TRUE(ShouldSuppressSpeedometerShowForTest(2091, true, true, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, true, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));

    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2089, true, false, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Sweeper"));
    ASSERT_TRUE(!ShouldSuppressSpeedometerShowForTest(2091, true, false, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, true, false, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        5, false, true, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~101", "~g~~h~~h~102"));
    return true;
}

bool TestTextDrawClassifiesTachoV2ByIdAndGeometry() {
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        4, 575.0f, 426.0f, 1, 1, 1, 307.0f, 155.5f, 0.6f, 2.25f, "_"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2084, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2088, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2089, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "24"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2091, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2090, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2092, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"));

    ASSERT_TRUE(!keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2102, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "24"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2088, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "Inventar"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2081, 455.0f, 455.0f, 1, 1, 0, 640.0f, 480.0f, 1.0f, -5.0f, "_"));
    ASSERT_TRUE(!keybinder::TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
        2102, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"));
    return true;
}

bool TestTextDrawClassifierRolesFromDiagnostics() {
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        4, 575.0f, 426.0f, 1, 1, 1, 307.0f, 155.5f, 0.6f, 2.25f, "_"), std::string("SpeedometerBox"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        5, 636.0f, 2.0f, 3, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~g~~h~~h~102"), std::string("Unknown"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        4, 595.0f, 424.5f, 1, 0, 1, 400.0f, 107.0f, 0.224996f, 1.15f, "Tankvolumen"), std::string("FuelLabel"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        22, 535.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "KM/H"), std::string("SpeedUnit"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2085, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"), std::string("GaugeSegment"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2086, 559.0f, 437.0f, 1, 1, 0, 638.5f, 0.0f, 1.0f, 0.75f, "_"), std::string("GaugeSegment"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2087, 561.45f, 438.0f, 1, 0, 0, 558.95f, 0.0f, 1.0f, 0.55f, "_"), std::string("GaugeSegment"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2088, 561.45f, 438.0f, 1, 1, 0, 636.05f, 0.0f, 1.0f, 0.55f, "_"), std::string("GaugeSegment"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2084, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus"), std::string("VehicleName"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2089, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Sweeper"), std::string("VehicleName"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2090, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Bus"), std::string("VehicleName"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2090, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0"), std::string("SpeedValue"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2091, 515.0f, 435.0f, 1, 0, 1, 400.0f, 107.0f, 0.237498f, 1.05f, "0"), std::string("SpeedValue"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2091, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"), std::string("FuelValue"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2092, 595.0f, 437.0f, 1, 0, 1, 400.0f, 107.0f, 0.174998f, 0.849995f, "100%"), std::string("FuelValue"));

    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2083, 618.0f, 120.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.3f, 1.5f, "~y~Z ~w~Motor bedienen~n~~y~NUM2 ~w~Radio bedienen~n~"), std::string("Unknown"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2082, 456.0f, 416.0f, 1, 0, 0, 1280.0f, 1280.0f, 0.4f, 0.8f, "Clover"), std::string("Unknown"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2084, 456.0f, 431.0f, 2, 0, 0, 1280.0f, 1280.0f, 0.3f, 0.9f, "0 km/h"), std::string("Unknown"));
    ASSERT_EQ(keybinder::TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
        2098, 535.0f, 430.0f, 2, 0, 0, 1280.0f, 1280.0f, 0.5f, 0.8f, "100%"), std::string("Unknown"));
    ASSERT_TRUE(keybinder::TextDrawAdapter::ClassifyShowTextDrawConfidenceForTesting(
        2088, 527.0f, 424.0f, 1, 0, 1, 400.0f, 107.0f, 0.287499f, 1.25f, "Infernus") >= 85);
    return true;
}

bool TestTextDrawDiagnosticsToggle() {
    keybinder::TextDrawAdapter adapter;
    ASSERT_TRUE(!adapter.IsDiagnosticsEnabled());
    ASSERT_EQ(adapter.GetDiagnosticsEventCount(), static_cast<unsigned long>(0));

    adapter.SetDiagnosticsEnabled(true);
    ASSERT_TRUE(adapter.IsDiagnosticsEnabled());
    ASSERT_EQ(adapter.GetDiagnosticsEventCount(), static_cast<unsigned long>(0));

    adapter.SetDiagnosticsEnabled(false);
    ASSERT_TRUE(!adapter.IsDiagnosticsEnabled());
    ASSERT_EQ(adapter.GetDiagnosticsEventCount(), static_cast<unsigned long>(0));
    return true;
}

bool TestSpeedometerRpmKeepsIdleFloorAtHighGearBandStart() {
    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = 205.0f;
    input.vehicleHealth = 1000.0f;
    input.currentGear = 6;
    input.engineOn = true;

    const auto telemetry = keybinder::SpeedometerManager::BuildTelemetry(input);
    ASSERT_TRUE(telemetry.estimatedRpmPct >= 0.10f);
    ASSERT_TRUE(telemetry.estimatedRpmPct < 0.30f);

    input.engineOn = false;
    const auto movingWithEngineFlagOff = keybinder::SpeedometerManager::BuildTelemetry(input);
    ASSERT_TRUE(movingWithEngineFlagOff.estimatedRpmPct >= 0.10f);

    input.speedKmh = 0.0f;
    const auto stoppedEngineOff = keybinder::SpeedometerManager::BuildTelemetry(input);
    ASSERT_TRUE(stoppedEngineOff.estimatedRpmPct == 0.0f);
    return true;
}

bool TestSpeedometerRpmRisesGraduallyWithinGearBand() {
    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.vehicleHealth = 1000.0f;
    input.currentGear = 6;
    input.engineOn = true;

    input.speedKmh = 220.0f;
    const auto lowTopGear = keybinder::SpeedometerManager::BuildTelemetry(input);

    input.speedKmh = 260.0f;
    const auto highTopGear = keybinder::SpeedometerManager::BuildTelemetry(input);

    // Holding the throttle at a fixed speed must not artificially bump the needle:
    // a real tach is driven by wheel speed through the gear ratio, not by pedal
    // input. Two samples at the same speed/gear must read identically regardless
    // of throttle position.
    input.speedKmh = 220.0f;
    input.hasThrottleValue = true;
    input.throttleValue = 1.0f;
    const auto throttleHeld = keybinder::SpeedometerManager::BuildTelemetry(input);

    ASSERT_TRUE(lowTopGear.estimatedRpmPct > 0.25f);
    ASSERT_TRUE(lowTopGear.estimatedRpmPct < 0.50f);
    ASSERT_TRUE(highTopGear.estimatedRpmPct > lowTopGear.estimatedRpmPct);
    ASSERT_TRUE(highTopGear.estimatedRpmPct <= 0.95f);
    ASSERT_TRUE(std::fabs(throttleHeld.estimatedRpmPct - lowTopGear.estimatedRpmPct) < 0.001f);
    return true;
}


bool TestSpeedometerRpmUsesActualGearShiftReset() {
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 228.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.currentGear = 5;
    previousInput.currentGearMinSpeedKmh = 170.0f;
    previousInput.currentGearMaxSpeedKmh = 230.0f;
    previousInput.engineOn = true;
    const auto beforeShift = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = 230.0f;
    input.vehicleHealth = 1000.0f;
    input.currentGear = 6;
    input.currentGearMinSpeedKmh = 205.0f;
    input.currentGearMaxSpeedKmh = 285.0f;
    input.engineOn = true;
    const auto atActualShift = keybinder::SpeedometerManager::BuildTelemetry(input, &beforeShift, 0.05f);

    input.speedKmh = 245.0f;
    const auto afterActualShift = keybinder::SpeedometerManager::BuildTelemetry(input, &atActualShift, 0.5f);

    ASSERT_TRUE(atActualShift.observedGearStartSpeedKmh >= 229.0f);
    ASSERT_TRUE(atActualShift.estimatedRpmPct >= 0.10f);
    ASSERT_TRUE(atActualShift.estimatedRpmPct < 0.25f);
    ASSERT_TRUE(afterActualShift.estimatedRpmPct > atActualShift.estimatedRpmPct);
    ASSERT_TRUE(afterActualShift.estimatedRpmPct < 0.55f);
    return true;
}


bool TestSpeedometerRpmResetsOnAudioShiftEvent() {
    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = 120.0f;
    input.vehicleHealth = 1000.0f;
    input.currentGear = 3;
    input.currentGearMinSpeedKmh = 70.0f;
    input.currentGearMaxSpeedKmh = 190.0f;
    input.engineOn = true;
    const auto beforeShift = keybinder::SpeedometerManager::BuildTelemetry(input);

    input.speedKmh = 150.0f;
    const auto beforeAudioShift = keybinder::SpeedometerManager::BuildTelemetry(input, &beforeShift, 0.5f);

    input.audioChangingGear = true;
    const auto duringAudioShift = keybinder::SpeedometerManager::BuildTelemetry(input, &beforeAudioShift, 0.05f);

    input.audioChangingGear = false;
    input.speedKmh = 165.0f;
    const auto afterAudioShift = keybinder::SpeedometerManager::BuildTelemetry(input, &duringAudioShift, 0.5f);

    ASSERT_TRUE(duringAudioShift.observedGearStartSpeedKmh >= 149.0f);
    ASSERT_TRUE(duringAudioShift.estimatedRpmPct < beforeAudioShift.estimatedRpmPct);
    ASSERT_TRUE(afterAudioShift.estimatedRpmPct > duringAudioShift.estimatedRpmPct);
    ASSERT_TRUE(afterAudioShift.estimatedRpmPct < beforeAudioShift.estimatedRpmPct);
    return true;
}

bool TestSpeedometerRpmReflectsSpeedInTopGear() {
    // In the upper half of the top gear's speed band the gauge must read well
    // above the idle/low-end range even while the throttle is simply held and the
    // car is coasting (no positive acceleration). The reading is purely a
    // function of wheel speed within the gear band -- throttle and acceleration
    // do not contribute to the needle.
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 240.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.currentGear = 6;
    previousInput.currentGearMinSpeedKmh = 205.0f;
    previousInput.currentGearMaxSpeedKmh = 285.0f;
    previousInput.engineOn = true;
    const auto previous = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input = previousInput;
    input.speedKmh = 232.0f;
    input.hasThrottleValue = true;
    input.throttleValue = 1.0f;

    const auto coastingHighGear = keybinder::SpeedometerManager::BuildTelemetry(input, &previous, 0.5f);

    ASSERT_TRUE(coastingHighGear.accelerationPct == 0.0f);
    ASSERT_TRUE(coastingHighGear.throttlePct >= 0.99f);
    ASSERT_TRUE(coastingHighGear.estimatedRpmPct > (3.0f / 8.0f));
    return true;
}

bool TestSpeedometerRpmLaunchDoesNotSpikeToRedline() {
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 0.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.currentGear = 1;
    previousInput.currentGearMinSpeedKmh = 0.0f;
    previousInput.currentGearMaxSpeedKmh = 35.0f;
    previousInput.hasThrottleValue = true;
    previousInput.throttleValue = 1.0f;
    previousInput.engineOn = true;
    const auto stopped = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input = previousInput;
    input.speedKmh = 10.0f;

    const auto launch = keybinder::SpeedometerManager::BuildTelemetry(input, &stopped, 0.2f);

    ASSERT_TRUE(stopped.estimatedRpmPct < (3.0f / 8.0f));
    ASSERT_TRUE(launch.estimatedRpmPct > stopped.estimatedRpmPct);
    ASSERT_TRUE(launch.estimatedRpmPct < (4.0f / 8.0f));
    return true;
}

bool TestSpeedometerRpmRetainsObservedGearStartWhenHandlingBandIsAhead() {
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 100.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.currentGear = 3;
    previousInput.currentGearMinSpeedKmh = 70.0f;
    previousInput.currentGearMaxSpeedKmh = 130.0f;
    previousInput.engineOn = true;
    const auto beforeShift = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input;
    input.isInVehicle = true;
    input.speedKmh = 104.0f;
    input.vehicleHealth = 1000.0f;
    input.currentGear = 4;
    input.currentGearMinSpeedKmh = 130.0f;
    input.currentGearMaxSpeedKmh = 185.0f;
    input.engineOn = true;
    const auto atShift = keybinder::SpeedometerManager::BuildTelemetry(input, &beforeShift, 0.1f);

    input.speedKmh = 112.0f;
    const auto afterShift = keybinder::SpeedometerManager::BuildTelemetry(input, &atShift, 0.4f);

    ASSERT_TRUE(atShift.observedGearStartSpeedKmh >= 103.0f);
    ASSERT_TRUE(afterShift.observedGearStartSpeedKmh >= 103.0f);
    ASSERT_TRUE(afterShift.observedGearStartSpeedKmh < 106.0f);
    ASSERT_TRUE(afterShift.estimatedRpmPct > atShift.estimatedRpmPct);
    ASSERT_TRUE(afterShift.estimatedRpmPct > 0.22f);
    ASSERT_TRUE(afterShift.estimatedRpmPct < 0.45f);
    return true;
}

bool TestSpeedometerRpmExpandsTooNarrowLowGearBand() {
    keybinder::speedometer::VehicleTelemetryInput previousInput;
    previousInput.isInVehicle = true;
    previousInput.speedKmh = 0.0f;
    previousInput.vehicleHealth = 1000.0f;
    previousInput.currentGear = 1;
    previousInput.currentGearMinSpeedKmh = 0.0f;
    previousInput.currentGearMaxSpeedKmh = 20.0f;
    previousInput.hasThrottleValue = true;
    previousInput.throttleValue = 1.0f;
    previousInput.engineOn = true;
    const auto stopped = keybinder::SpeedometerManager::BuildTelemetry(previousInput);

    keybinder::speedometer::VehicleTelemetryInput input = previousInput;
    input.speedKmh = 15.0f;
    const auto launch = keybinder::SpeedometerManager::BuildTelemetry(input, &stopped, 0.2f);

    ASSERT_TRUE(launch.estimatedRpmPct > stopped.estimatedRpmPct);
    ASSERT_TRUE(launch.estimatedRpmPct < 0.55f);
    return true;
}



bool TestSpeedometerHealthColorThresholds() {
    const ImVec4 green = keybinder::SpeedometerUI::GetHealthColor(0.80f);
    const ImVec4 yellow = keybinder::SpeedometerUI::GetHealthColor(0.30f);
    const ImVec4 red = keybinder::SpeedometerUI::GetHealthColor(0.10f);

    ASSERT_TRUE(green.y > green.x);
    ASSERT_TRUE(yellow.x > 0.90f && yellow.y > 0.60f);
    ASSERT_TRUE(red.x > red.y);
    return true;
}

int RunRegressionTests() {
    printf("\n=== Regression Tests ===\n\n");

    RUN_TEST(TestLoggerLifecycle);
    RUN_TEST(TestLoggerTestSink);
    RUN_TEST(TestLoggerMinLevel);
    RUN_TEST(TestConfigRoundTripWithLogger);
    RUN_TEST(TestEmptyConfigWithLogger);
    RUN_TEST(TestCorruptConfigWithLogger);
    RUN_TEST(TestLoggerDoubleInitShutdown);
    RUN_TEST(TestLoggerTruncatesPreviousSession);
    RUN_TEST(TestPersistentPathsRootContainsNLBinder);
    RUN_TEST(TestPersistentPathsConfigFileEndsWithNLBinderKeybinderJson);
    RUN_TEST(TestPersistentPathsLogFileEndsWithNLBinderKeybinderLog);
    RUN_TEST(TestPersistentPathsImGuiIniEndsWithNLBinderImguiIni);
    RUN_TEST(TestPersistentPathsEnsureLayoutCreatesNestedDirectories);
    RUN_TEST(TestPersistentPathsRecursiveHitmarkerTempDirectoryCreation);
    RUN_TEST(TestSettingsExtraction);
    RUN_TEST(TestSettingsRoundTrip);
    RUN_TEST(TestDamageLogSettingsDefaultAndRoundTrip);
    RUN_TEST(TestSquareMapFistAndQuickMapRadarPackRoundTrip);
    RUN_TEST(TestThemeNameSavedToJson);
    RUN_TEST(TestUiTransparencyClampsAndRoundTrips);
    RUN_TEST(TestThemeNameLoadedFromJson);
    RUN_TEST(TestThemeNameRecognizesAllBuiltIns);
    RUN_TEST(TestMissingThemeNameDefaultsToDark);
    RUN_TEST(TestInvalidThemeNameDefaultsToDark);
    RUN_TEST(TestLegacyCustomAccentOnlyDefaultsToDark);
    RUN_TEST(TestSettingsIgnoreDeprecatedThemeFields);
    RUN_TEST(TestSettingsNestedThemeNameExtractionThroughKeybindManager);
    RUN_TEST(TestSettingsQuickMapInvalidHotkeyFallsBackToM);
    RUN_TEST(TestSettingsQuickMapFollowPlayerDefaultsAndRoundTrips);
    RUN_TEST(TestSettingsQuickMapZoomLevelDefaultsToFar);
    RUN_TEST(TestSettingsQuickMapLastZoomLevelDefaultsToConfiguredZoomLevelForLegacyConfig);
    RUN_TEST(TestSettingsQuickMapLastZoomLevelRoundTripsIndependently);
    RUN_TEST(TestSettingsQuickMapZoomLevelMediumRoundTrips);
    RUN_TEST(TestSettingsQuickMapZoomLevelCloseRoundTrips);
    RUN_TEST(TestSettingsQuickMapZoomLevelVeryCloseRoundTrips);
    RUN_TEST(TestSettingsQuickMapZoomLevelUltraRoundTrips);
    RUN_TEST(TestSettingsQuickMapZoomLevelInvalidFallsBackToFar);
    RUN_TEST(TestSettingsQuickMapZoomLevelEmptyFallsBackToFar);
    RUN_TEST(TestSettingsQuickMapZoomLevelIsCaseInsensitive);
    RUN_TEST(TestSettingsRoundTripWithRssFeeds);
    RUN_TEST(TestSettingsHitsoundRoundTrip);
    RUN_TEST(TestSettingsHitsoundDefaultValues);
    RUN_TEST(TestSettingsHitsoundPathNormalization);
    RUN_TEST(TestSettingsSpeedometerDefaultsAndRoundTrip);
    RUN_TEST(TestSettingsIgnoresLegacySpeedometerTextDrawIds);
    RUN_TEST(TestSettingsSpeedometerLegacyIdFieldsDoNotRoundTrip);
    RUN_TEST(TestTextDrawRpcConstants);
    RUN_TEST(TestTextDrawFuelParsingWithColorCodes);
    RUN_TEST(TestTextDrawRpcParsesSetStringFuelText);
    RUN_TEST(TestTextDrawRpcParsesShowTextDrawFuelText);
    RUN_TEST(TestTextDrawCachesFuelFromPercentTextDrawOnly);
    RUN_TEST(TestTextDrawMatchesHardcodedTachoIdsFromLocalRpcIds);
    RUN_TEST(TestTextDrawSuppressesTachoV2ShowByGeometry);
    RUN_TEST(TestTextDrawPreservesNonTachoPlayerOverlayFromDump);
    RUN_TEST(TestTextDrawDoesNotSuppressAlternativeTachoFromDump);
    RUN_TEST(TestTextDrawSuppressesTachoV2FromDiagnostics);
    RUN_TEST(TestTextDrawDoesNotSuppressSetStringRpc);
    RUN_TEST(TestTextDrawSuppressesTachoSetStringRpcFromDiagnostics);
    RUN_TEST(TestTextDrawDoesNotApplyAbsoluteTachoCacheToServerNumericAliases);
    RUN_TEST(TestTextDrawDoesNotReuseShowCacheFromInactiveContext);
    RUN_TEST(TestTextDrawHideClearsCachedShowClassification);
    RUN_TEST(TestTextDrawSuppressesTachoV2LocalSpeedSetStringRpc);
    RUN_TEST(TestTextDrawSuppressionRequiresVehicleContext);
    RUN_TEST(TestTextDrawClassifiesTachoV2ByIdAndGeometry);
    RUN_TEST(TestTextDrawClassifierRolesFromDiagnostics);
    RUN_TEST(TestTextDrawDiagnosticsToggle);
    RUN_TEST(TestSpeedometerRpmKeepsIdleFloorAtHighGearBandStart);
    RUN_TEST(TestSpeedometerRpmRisesGraduallyWithinGearBand);
    RUN_TEST(TestSpeedometerRpmUsesActualGearShiftReset);
    RUN_TEST(TestSpeedometerRpmResetsOnAudioShiftEvent);
    RUN_TEST(TestSpeedometerRpmReflectsSpeedInTopGear);
    RUN_TEST(TestSpeedometerRpmLaunchDoesNotSpikeToRedline);
    RUN_TEST(TestSpeedometerRpmRetainsObservedGearStartWhenHandlingBandIsAhead);
    RUN_TEST(TestSpeedometerRpmExpandsTooNarrowLowGearBand);
    RUN_TEST(TestSpeedometerHealthColorThresholds);

    printf("\n");
    return g_results.failed;
}
