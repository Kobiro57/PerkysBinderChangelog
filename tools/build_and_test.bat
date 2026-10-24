@echo off
setlocal

echo === Building Keybinder.asi (Release) ===
call "%~dp0build_release.bat"
if %ERRORLEVEL% NEQ 0 (
    echo BUILD FAILED with error %ERRORLEVEL%
    exit /b 1
)
echo BUILD SUCCEEDED

echo.
echo === Running Standalone Test Harness ===
call "%~dp0run_tests.bat"
exit /b %ERRORLEVEL%
