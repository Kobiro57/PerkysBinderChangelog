@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cl.exe /nologo /EHsc /O2 /MT /utf-8 /std:c++17 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
 /Fo"%~dp0..\build\BrowserSetup.obj" /Fe"%~dp0..\build\BrowserSetup.exe" ^
 "%~dp0BrowserSetup.cpp" /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" winhttp.lib bcrypt.lib shell32.lib ole32.lib advapi32.lib
exit /b %ERRORLEVEL%
