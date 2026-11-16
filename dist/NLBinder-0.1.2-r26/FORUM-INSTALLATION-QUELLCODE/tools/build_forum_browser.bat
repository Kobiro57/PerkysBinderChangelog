@echo off
setlocal
set "NLBINDER_PROJECT=%~dp0.."
if not exist "%NLBINDER_PROJECT%\build\cef-root.txt" (
    echo CEF is missing. Run python tools/fetch_cef_runtime.py first.
    exit /b 2
)
for /f "usebackq delims=" %%I in ("%NLBINDER_PROJECT%\build\cef-root.txt") do set "NLBINDER_CEF_ROOT=%%I"
set "NLBINDER_CMAKE="
set "NLBINDER_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%NLBINDER_VSWHERE%" for /f "usebackq delims=" %%I in (`"%NLBINDER_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do if exist "%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "NLBINDER_CMAKE=%%I\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined NLBINDER_CMAKE for /f "delims=" %%I in ('where cmake.exe 2^>nul') do set "NLBINDER_CMAKE=%%I"
if not defined NLBINDER_CMAKE (
    echo CMake was not found. Install the Visual Studio CMake component.
    exit /b 2
)
"%NLBINDER_CMAKE%" -S "%NLBINDER_PROJECT%\browser" -B "%NLBINDER_PROJECT%\build\cef-host" -G "Visual Studio 17 2022" -A x64 -DCEF_SDK_ROOT:PATH="%NLBINDER_CEF_ROOT%"
if errorlevel 1 exit /b 2
"%NLBINDER_CMAKE%" --build "%NLBINDER_PROJECT%\build\cef-host" --config Release --parallel 4
if errorlevel 1 exit /b 2
copy /y "%NLBINDER_CEF_ROOT%\LICENSE.txt" "%NLBINDER_PROJECT%\build\cef-host\Release\CEF-LICENSE.txt" >nul
exit /b 0
