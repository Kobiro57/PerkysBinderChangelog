@echo off
setlocal

:: Use VS 2022 BuildTools for MSBuild (matches v143 PlatformToolset and plugin.lib)
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
call "%VCVARS%" x86 >nul 2>&1

echo === Building Keybinder.asi (Release) ===
MSBuild "%~dp0..\Keybinder.vcxproj" /p:Configuration=Release /p:Platform=Win32 /m /nologo /v:minimal
if %ERRORLEVEL% NEQ 0 (
    echo BUILD FAILED with error %ERRORLEVEL%
    exit /b 1
)
echo BUILD SUCCEEDED

echo.
echo === Running Standalone Test Harness ===
call "%~dp0run_tests.bat"
exit /b %ERRORLEVEL%
