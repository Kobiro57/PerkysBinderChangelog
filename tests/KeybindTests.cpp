/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Keybind domain model unit tests entry point
*/
#include "KeybindTests.h"

int RunActionExecutorTests();
int RunReconnectTests();
int RunKillMessageTestTests();
int RunLocationVariableTests();
int RunServerStatsParserTests();
int RunStartupStatsSyncTests();
int RunStatsDialogRPCAdapterTests();
int RunSpeedTicketFlowTests();
int RunWeaponIconManagerTests();
int RunWeaponIconSettingsTests();
int RunNativeHudEditorTests();
int RunInterfaceLayoutSettingsTests();
int RunJobSkillsSnapshotTests();
int RunJobSkillsImportTests();
int RunSpeedometerFocusedTests();
int RunInterfaceEditorTests();
int RunInputManagerTests();
int RunRegressionTests();
int RunSAMPBridgeTests();
int RunImGuiOverlayTests();
int RunHitmarkerTests();
int RunHitsoundTests();
int RunThemeManagerTests();
int RunQuickMapTests();
int RunJobRegistryTests();
int RunJobChatAdapterTests();
int RunJobEventStoreTests();
int RunJobParserTests();
int RunJobTrackerLifecycleTests();
int RunMainWindowJobsNavigationTests();
int RunJobsUITests();
int RunJobsCorrectionTests();
int RunJobAdjustmentTests();
int RunOnlineUITests();
int RunOnlineDataManagerTests();
int RunOnlineSettingsTests();
int RunOnlineOverlayRendererTests();
int RunOnlineEventDetectorTests();
int RunUIComponentsTests();
int RunSquareMapTests();
int RunHudSpriteManagerTests();
int RunSourceContractTests();

#include <stdio.h>

int main() {
    auto log_line = [](const char* message) {
        printf("%s\n", message);
        fflush(stdout);
    };

    auto run_suite = [&](const char* message, int (*runner)()) {
        log_line(message);
        return runner();
    };

    log_line("Starting tests...");

    const int keybind_failures = run_suite("Running KeybindTests...", RunKeybindTests);
    const int executor_failures = run_suite("Running ActionExecutorTests...", RunActionExecutorTests);
    const int reconnect_failures = run_suite("Running ReconnectTests...", RunReconnectTests);
    const int speedometer_failures = run_suite("Running SpeedometerFocusedTests...", RunSpeedometerFocusedTests);
    const int interface_editor_failures = run_suite("Running InterfaceEditorTests...", RunInterfaceEditorTests);
    const int input_failures = run_suite("Running InputManagerTests...", RunInputManagerTests);
    const int regression_failures = run_suite("Running RegressionTests...", RunRegressionTests);
    const int samp_bridge_failures = run_suite("Running SAMPBridgeTests...", RunSAMPBridgeTests);
    const int imgui_overlay_failures = run_suite("Running ImGuiOverlayTests...", RunImGuiOverlayTests);
    const int hitmarker_failures = run_suite("Running HitmarkerTests...", RunHitmarkerTests);
    const int hitsound_failures = run_suite("Running HitsoundTests...", RunHitsoundTests);
    const int theme_failures = run_suite("Running ThemeManagerTests...", RunThemeManagerTests);
    const int quickmap_failures = run_suite("Running QuickMapTests...", RunQuickMapTests);
    const int job_registry_failures = run_suite("Running JobRegistryTests...", RunJobRegistryTests);
    const int job_chat_adapter_failures = run_suite("Running JobChatAdapterTests...", RunJobChatAdapterTests);
    const int job_event_store_failures = run_suite("Running JobEventStoreTests...", RunJobEventStoreTests);
    const int job_parser_failures = run_suite("Running JobParserTests...", RunJobParserTests);
    const int job_tracker_lifecycle_failures = run_suite("Running JobTrackerLifecycleTests...", RunJobTrackerLifecycleTests);
    const int main_window_jobs_nav_failures = run_suite("Running MainWindowJobsNavigationTests...", RunMainWindowJobsNavigationTests);
    const int jobs_ui_failures = run_suite("Running JobsUITests...", RunJobsUITests);
    const int jobs_correction_failures = run_suite("Running JobsCorrectionTests...", RunJobsCorrectionTests);
    const int job_adjustment_failures = run_suite("Running JobAdjustmentTests...", RunJobAdjustmentTests);
    const int online_ui_failures = run_suite("Running OnlineUITests...", RunOnlineUITests);
    const int online_data_manager_failures = run_suite("Running OnlineDataManagerTests...", RunOnlineDataManagerTests);
    const int online_settings_failures = run_suite("Running OnlineSettingsTests...", RunOnlineSettingsTests);
    const int online_overlay_renderer_failures = run_suite("Running OnlineOverlayRendererTests...", RunOnlineOverlayRendererTests);
    const int online_event_detector_failures = run_suite("Running OnlineEventDetectorTests...", RunOnlineEventDetectorTests);
    const int ui_components_failures = run_suite("Running UIComponentsTests...", RunUIComponentsTests);
    const int squaremap_failures = run_suite("Running SquareMapTests...", RunSquareMapTests);
    const int hud_sprite_failures = run_suite("Running HudSpriteManagerTests...", RunHudSpriteManagerTests);
    const int source_contract_failures = run_suite("Running SourceContractTests...", RunSourceContractTests);

    const int r4_failures = run_suite("Running KillMessageTestTests...", RunKillMessageTestTests)
        + run_suite("Running LocationVariableTests...", RunLocationVariableTests)
        + run_suite("Running ServerStatsParserTests...", RunServerStatsParserTests)
        + run_suite("Running StartupStatsSyncTests...", RunStartupStatsSyncTests)
        + run_suite("Running StatsDialogRPCAdapterTests...", RunStatsDialogRPCAdapterTests)
        + run_suite("Running SpeedTicketFlowTests...", RunSpeedTicketFlowTests)
        + run_suite("Running WeaponIconManagerTests...", RunWeaponIconManagerTests)
        + run_suite("Running WeaponIconSettingsTests...", RunWeaponIconSettingsTests)
        + run_suite("Running NativeHudEditorTests...", RunNativeHudEditorTests)
        + run_suite("Running InterfaceLayoutSettingsTests...", RunInterfaceLayoutSettingsTests)
        + run_suite("Running JobSkillsSnapshotTests...", RunJobSkillsSnapshotTests)
        + run_suite("Running JobSkillsImportTests...", RunJobSkillsImportTests);
    if (r4_failures != 0) return r4_failures;
    log_line("All tests finished.");
    return reconnect_failures + speedometer_failures + keybind_failures + executor_failures + interface_editor_failures + input_failures + regression_failures + samp_bridge_failures + imgui_overlay_failures + hitmarker_failures + hitsound_failures + theme_failures + quickmap_failures + job_registry_failures + job_chat_adapter_failures + job_event_store_failures + job_parser_failures + job_tracker_lifecycle_failures + main_window_jobs_nav_failures + jobs_ui_failures + jobs_correction_failures + job_adjustment_failures + online_ui_failures + online_data_manager_failures + online_settings_failures + online_overlay_renderer_failures + online_event_detector_failures + ui_components_failures + squaremap_failures + hud_sprite_failures + source_contract_failures;
}
