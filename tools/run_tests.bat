@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1

echo === Compiling Tests ===
rc.exe /nologo /fo "%~dp0..\build\KeybinderTests.res" "%~dp0..\Keybinder.rc"
if %ERRORLEVEL% NEQ 0 (
    echo TEST RESOURCE COMPILATION FAILED with error %ERRORLEVEL%
    exit /b 2
)

set "KEYBINDER_TEST_RSP=%~dp0..\build\KeybinderTests.rsp"
(
    echo /nologo /EHsc /std:c++latest /W3 /MT /DKEYBINDER_TESTING /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DGTASA /DPLUGIN_SGV_10US /DRW /D_DX9_SDK_INSTALLED
    echo /I"%~dp0..\include"
    echo /I"%~dp0..\src"
    echo /I"%~dp0..\tests"
    echo /I"%~dp0..\include\plugin-sdk\shared"
    echo /I"%~dp0..\include\plugin-sdk\shared\game"
    echo /I"%~dp0..\include\plugin-sdk\plugin_sa"
    echo /I"%~dp0..\include\plugin-sdk\plugin_sa\game_sa"
    echo /I"%~dp0..\include\plugin-sdk\plugin_sa\game_sa\rw"
    echo /I"%~dp0..\include\plugin-sdk\hooking"
    echo /I"%~dp0..\include\plugin-sdk\injector"
    echo /I"%~dp0..\include\plugin-sdk\safetyhook"
    echo /I"%~dp0..\include\plugin-sdk\shared\dxsdk"
    echo /I"%~dp0..\include\imgui-1.92.7"
    echo /I"%~dp0..\include\imgui-1.92.7\backends"
    echo /I"%~dp0..\include\SAMP-API\include"
    echo /I"%~dp0..\include\SAMP-API\src"
    echo /I"%~dp0..\include\RakHook\include"
    echo /I"%~dp0..\include\pugixml"
    echo /Fe:"%~dp0..\build\KeybinderTests.exe"
    echo "%~dp0..\tests\KeybindTests.cpp"
    echo "%~dp0..\tests\ActionExecutorTests.cpp"
    echo "%~dp0..\tests\InterfaceEditorTests.cpp"
    echo "%~dp0..\tests\InputManagerTests.cpp"
    echo "%~dp0..\tests\RegressionTests.cpp"
    echo "%~dp0..\tests\SAMPBridgeTests.cpp"
    echo "%~dp0..\tests\ImGuiOverlayTests.cpp"
    echo "%~dp0..\tests\HitmarkerTests.cpp"
    echo "%~dp0..\tests\HitsoundTests.cpp"
    echo "%~dp0..\tests\ThemeManagerTests.cpp"
    echo "%~dp0..\tests\QuickMapTests.cpp"
    echo "%~dp0..\tests\JobRegistryTests.cpp"
    echo "%~dp0..\tests\JobChatAdapterTests.cpp"
    echo "%~dp0..\tests\JobEventStoreTests.cpp"
    echo "%~dp0..\tests\JobParserTests.cpp"
    echo "%~dp0..\tests\JobTrackerLifecycleTests.cpp"
    echo "%~dp0..\tests\MainWindowJobsNavigationTests.cpp"
    echo "%~dp0..\tests\JobsUITests.cpp"
    echo "%~dp0..\tests\JobsCorrectionTests.cpp"
    echo "%~dp0..\tests\JobAdjustmentTests.cpp"
    echo "%~dp0..\tests\OnlineUITests.cpp"
    echo "%~dp0..\tests\OnlineDataManagerTests.cpp"
    echo "%~dp0..\tests\OnlineSettingsTests.cpp"
    echo "%~dp0..\tests\OnlineOverlayRendererTests.cpp"
    echo "%~dp0..\tests\OnlineEventDetectorTests.cpp"
    echo "%~dp0..\tests\UIComponentsTests.cpp"
    echo "%~dp0..\tests\SquareMapTests.cpp"
    echo "%~dp0..\tests\HudSpriteManagerTests.cpp"
    echo "%~dp0..\tests\SourceContractTests.cpp"
    echo "%~dp0..\tests\ImGuiFreeTypeStubs.cpp"
    echo "%~dp0..\tests\TestStubs.cpp"
    echo "%~dp0..\\src\\Logger.cpp"
    echo "%~dp0..\\src\\ActionExecutor.cpp"
    echo "%~dp0..\\src\\KeybindManager.cpp"
    echo "%~dp0..\\src\\PersistentPaths.cpp"
    echo "%~dp0..\\src\\SAMPBridge.cpp"
    echo "%~dp0..\\src\\InputManager.cpp"
    echo "%~dp0..\\src\\ImGuiOverlay.cpp"
    echo "%~dp0..\\src\\SettingsManager.cpp"
    echo "%~dp0..\\src\\Hitmarker.cpp"
    echo "%~dp0..\\src\\HitsoundPlayer.cpp"
    echo "%~dp0..\\src\\HitsoundUI.cpp"
    echo "%~dp0..\\src\\HitmarkerRenderer.cpp"
    echo "%~dp0..\\src\\HitmarkerUI.cpp"
    echo "%~dp0..\\src\\InterfaceEditor.cpp"
    echo "%~dp0..\\src\\InterfaceEditorUI.cpp"
    echo "%~dp0..\\src\\CrosshairSwitcher.cpp"
    echo "%~dp0..\\src\\CrosshairUI.cpp"
    echo "%~dp0..\\src\\SquareMap.cpp"
    echo "%~dp0..\\src\\SquareMapUI.cpp"
    echo "%~dp0..\\src\\EmbeddedRwTexture.cpp"
    echo "%~dp0..\\src\\HudSpriteManager.cpp"
    echo "%~dp0..\\src\\HudSpriteUI.cpp"
    echo "%~dp0..\\src\\TextureManager.cpp"
    echo "%~dp0..\\src\\MainWindowUI.cpp"
    echo "%~dp0..\\src\\ThemeManager.cpp"
    echo "%~dp0..\\src\\QuickMap.cpp"
    echo "%~dp0..\\src\\TextDrawAdapter.cpp"
    echo "%~dp0..\\src\\SpeedometerManager.cpp"
    echo "%~dp0..\\src\\SpeedometerUI.cpp"
    echo "%~dp0..\\src\\UIComponents.cpp"
    echo "%~dp0..\\src\\KeybindEditorUI.cpp"
    echo "%~dp0..\\src\\KeybindListUI.cpp"
    echo "%~dp0..\\src\\KillTracker.cpp"
    echo "%~dp0..\\src\\WeaponNames.cpp"
    echo "%~dp0..\\src\\StatsStorage.cpp"
    echo "%~dp0..\\src\\JobRegistry.cpp"
    echo "%~dp0..\\src\\JobChatAdapter.cpp"
    echo "%~dp0..\\src\\JobEventStore.cpp"
    echo "%~dp0..\\src\\JobParser.cpp"
    echo "%~dp0..\\src\\JobTracker.cpp"
    echo "%~dp0..\\src\\KillfeedAdapter.cpp"
    echo "%~dp0..\\src\\DamageRPCAdapter.cpp"
    echo "%~dp0..\\src\\DeathMessageRPCAdapter.cpp"
    echo "%~dp0..\\src\\KillTrackerUI.cpp"
    echo "%~dp0..\\src\\JobsUI.cpp"
    echo "%~dp0..\\src\\OnlineUI.cpp"
    echo "%~dp0..\\src\\OnlineDataManager.cpp"
    echo "%~dp0..\\src\\OnlineOverlayController.cpp"
    echo "%~dp0..\\src\\OnlineOverlayRenderer.cpp"
    echo "%~dp0..\\src\\OnlineEventDetector.cpp"
    echo "%~dp0..\\src\\OnlinePlayerInfoService.cpp"
    echo "%~dp0..\\src\\RssFeedManager.cpp"
    echo "%~dp0..\\src\\RssTrackerUI.cpp"
    echo "%~dp0..\\src\\RssHttpClient.cpp"
    echo "%~dp0..\\src\\RssParser.cpp"
    echo "%~dp0..\\include\\pugixml\\pugixml.cpp"
    echo "%~dp0..\\include\\plugin-sdk\\shared\\Other.cpp"
    echo "%~dp0..\\include\\SAMP-API\\src\\sampapi\\sampapi.cpp"
    echo "%~dp0..\\include\\RakHook\\src\\RakNet\\BitStream.cpp"
    echo "%~dp0..\\include\\imgui-1.92.7\\imgui.cpp"
    echo "%~dp0..\include\imgui-1.92.7\imgui_draw.cpp"
    echo "%~dp0..\include\imgui-1.92.7\imgui_tables.cpp"
    echo "%~dp0..\include\imgui-1.92.7\imgui_widgets.cpp"
    echo "%~dp0..\include\imgui-1.92.7\backends\imgui_impl_dx9.cpp"
    echo "%~dp0..\include\imgui-1.92.7\backends\imgui_impl_win32.cpp"
    echo "%~dp0..\include\imgui-1.92.7\misc\cpp\imgui_stdlib.cpp"
    echo "%~dp0..\build\KeybinderTests.res" /link user32.lib winhttp.lib xaudio2.lib ole32.lib
) > "%KEYBINDER_TEST_RSP%"
cl.exe @"%KEYBINDER_TEST_RSP%"
if %ERRORLEVEL% NEQ 0 (
    echo TEST COMPILATION FAILED with error %ERRORLEVEL%
    exit /b 2
)
echo TEST COMPILATION SUCCEEDED

echo.
echo === Compiling RSS Tests ===
cl.exe /nologo /EHsc /std:c++latest /W3 /MT /DKEYBINDER_TESTING /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DGTASA /DPLUGIN_SGV_10US /DRW /D_DX9_SDK_INSTALLED ^
    /I"%~dp0..\include" ^
    /I"%~dp0..\src" ^
    /I"%~dp0..\tests" ^
    /I"%~dp0..\include\plugin-sdk\shared" ^
    /I"%~dp0..\include\plugin-sdk\shared\game" ^
    /I"%~dp0..\include\plugin-sdk\plugin_sa" ^
    /I"%~dp0..\include\plugin-sdk\plugin_sa\game_sa" ^
    /I"%~dp0..\include\plugin-sdk\plugin_sa\game_sa\rw" ^
    /I"%~dp0..\include\plugin-sdk\hooking" ^
    /I"%~dp0..\include\plugin-sdk\injector" ^
    /I"%~dp0..\include\plugin-sdk\safetyhook" ^
    /I"%~dp0..\include\plugin-sdk\shared\dxsdk" ^
    /I"%~dp0..\include\imgui-1.92.7" ^
    /I"%~dp0..\include\imgui-1.92.7\backends" ^
    /I"%~dp0..\include\SAMP-API\include" ^
    /I"%~dp0..\include\SAMP-API\src" ^
    /I"%~dp0..\include\RakHook\include" ^
    /I"%~dp0..\include\pugixml" ^
    /Fe:"%~dp0..\build\RssParserTests.exe" ^
    "%~dp0..\tests\RssParserTests.cpp" ^
    "%~dp0..\src\Logger.cpp" ^
    "%~dp0..\src\SettingsManager.cpp" ^
    "%~dp0..\src\RssFeedManager.cpp" ^
    "%~dp0..\src\RssHttpClient.cpp" ^
    "%~dp0..\src\RssParser.cpp" ^
    "%~dp0..\include\pugixml\pugixml.cpp" /link user32.lib winhttp.lib
if %ERRORLEVEL% NEQ 0 (
    echo RSS TEST COMPILATION FAILED with error %ERRORLEVEL%
    exit /b 3
)
echo RSS TEST COMPILATION SUCCEEDED

echo.
echo === Compiling Online Player Info Tests ===
cl.exe /nologo /EHsc /std:c++latest /W3 /MT /DKEYBINDER_TESTING /DONLINE_PLAYER_INFO_TEST_MAIN /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DGTASA /DPLUGIN_SGV_10US /DRW /D_DX9_SDK_INSTALLED ^
    /I"%~dp0..\include" ^
    /I"%~dp0..\src" ^
    /I"%~dp0..\tests" ^
    /Fe:"%~dp0..\build\OnlinePlayerInfoTests.exe" ^
    "%~dp0..\tests\OnlinePlayerInfoTests.cpp" ^
    "%~dp0..\src\Logger.cpp" ^
    "%~dp0..\src\RssHttpClient.cpp" ^
    "%~dp0..\src\OnlinePlayerInfoService.cpp" /link user32.lib winhttp.lib
if %ERRORLEVEL% NEQ 0 (
    echo ONLINE PLAYER INFO TEST COMPILATION FAILED with error %ERRORLEVEL%
    exit /b 4
)
echo ONLINE PLAYER INFO TEST COMPILATION SUCCEEDED

echo.
echo === Compiling Online Data Manager Tests ===
cl.exe /nologo /EHsc /std:c++latest /W3 /MT /DKEYBINDER_TESTING /DONLINE_DATA_MANAGER_TEST_MAIN /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DGTASA /DPLUGIN_SGV_10US /DRW /D_DX9_SDK_INSTALLED ^
    /I"%~dp0..\include" ^
    /I"%~dp0..\src" ^
    /I"%~dp0..\tests" ^
    /Fe:"%~dp0..\build\OnlineDataManagerTests.exe" ^
    "%~dp0..\tests\OnlineDataManagerTests.cpp" ^
    "%~dp0..\src\Logger.cpp" ^
    "%~dp0..\src\RssHttpClient.cpp" ^
    "%~dp0..\src\OnlinePlayerInfoService.cpp" ^
    "%~dp0..\src\OnlineDataManager.cpp" /link user32.lib winhttp.lib
if %ERRORLEVEL% NEQ 0 (
    echo ONLINE DATA MANAGER TEST COMPILATION FAILED with error %ERRORLEVEL%
    exit /b 5
)
echo ONLINE DATA MANAGER TEST COMPILATION SUCCEEDED

echo.
echo === Running Tests ===
"%~dp0..\build\KeybinderTests.exe"
set KEYBINDER_RESULT=%ERRORLEVEL%
echo.
echo KeybinderTests exit code: %KEYBINDER_RESULT%

echo.
echo === Running RSS Tests ===
"%~dp0..\build\RssParserTests.exe"
set RSS_RESULT=%ERRORLEVEL%

echo.
echo === Running Online Player Info Tests ===
"%~dp0..\build\OnlinePlayerInfoTests.exe"
set ONLINE_PLAYER_INFO_RESULT=%ERRORLEVEL%

echo.
echo === Running Online Data Manager Tests ===
"%~dp0..\build\OnlineDataManagerTests.exe"
set ONLINE_DATA_MANAGER_RESULT=%ERRORLEVEL%

set /a TEST_RESULT=%KEYBINDER_RESULT% + %RSS_RESULT% + %ONLINE_PLAYER_INFO_RESULT% + %ONLINE_DATA_MANAGER_RESULT%
echo.
echo Test exit code: %TEST_RESULT%
exit /b %TEST_RESULT%
