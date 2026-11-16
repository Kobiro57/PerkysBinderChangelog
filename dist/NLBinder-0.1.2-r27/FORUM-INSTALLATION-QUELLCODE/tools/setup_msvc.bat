@echo off
rem Locate the installed C++ toolchain, including custom installation paths.
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" exit /b 0
set "NLBINDER_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%NLBINDER_VSWHERE%" (
    echo Visual Studio Installer was not found. Install the Visual Studio C++ Build Tools.
    exit /b 1
)
set "NLBINDER_VS_PATH="
for /f "usebackq delims=" %%I in (`"%NLBINDER_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "NLBINDER_VS_PATH=%%I"
if not defined NLBINDER_VS_PATH (
    echo No Visual Studio installation with the x86 C++ toolchain was found.
    exit /b 1
)
call "%NLBINDER_VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
exit /b %ERRORLEVEL%
