@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
cl.exe /nologo /EHsc /O2 /MT /utf-8 /std:c++17 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
 /Fo"%~dp0..\build\BrowserSetupUnitTests.obj" /Fe"%~dp0..\build\BrowserSetupUnitTests.exe" ^
 "%~dp0..\tests\BrowserSetupUnitTests.cpp" /link /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" winhttp.lib bcrypt.lib shell32.lib ole32.lib advapi32.lib
exit /b %ERRORLEVEL%
