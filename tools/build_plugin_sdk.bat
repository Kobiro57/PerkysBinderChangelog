@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
for %%I in ("%~dp0..\include\plugin-sdk") do set "PLUGIN_SDK_DIR=%%~fI"
if not exist "%PLUGIN_SDK_DIR%\shared\plugin.h" (
    echo Plugin-SDK sources are missing from include\plugin-sdk.
    exit /b 1
)
if not exist "%PLUGIN_SDK_DIR%\plugin_sa\Plugin_SA.vcxproj" (
    pushd "%PLUGIN_SDK_DIR%\tools\premake" || exit /b 1
    premake5.exe --pluginsdkdir="%PLUGIN_SDK_DIR%" vs2022
    if errorlevel 1 (
        popd
        exit /b 1
    )
    popd
)
MSBuild "%PLUGIN_SDK_DIR%\plugin_sa\Plugin_SA.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /m /v:minimal /nologo
exit /b %ERRORLEVEL%
