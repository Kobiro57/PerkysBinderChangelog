@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cl.exe /nologo /EHsc /O2 /MT /utf-8 /std:c++17 /DUNICODE /D_UNICODE ^
    /Fo"%~dp0..\build\ReconnectHelper.obj" /Fe"%~dp0..\build\ReconnectHelper.exe" ^
    "%~dp0ReconnectHelper.cpp" /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib ole32.lib oleaut32.lib shell32.lib uiautomationcore.lib uuid.lib
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
rem Harmless native GUI/process fixture; never opens an installed launcher.
cl.exe /nologo /EHsc /O2 /MT /utf-8 /std:c++17 /DUNICODE /D_UNICODE ^
    /Fo"%~dp0..\build\ReconnectWorkerFixture.obj" /Fe"%~dp0..\build\ReconnectWorkerFixture.exe" ^
    "%~dp0..\tests\ReconnectWorkerFixture.cpp" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib
exit /b %ERRORLEVEL%
