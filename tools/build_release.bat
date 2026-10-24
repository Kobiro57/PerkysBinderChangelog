@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
if not exist "%~dp0..\include\plugin-sdk\output\lib\plugin.lib" (
    call "%~dp0build_plugin_sdk.bat" || exit /b 1
)
MSBuild "%~dp0..\Keybinder.vcxproj" /p:Configuration=Release /p:Platform=Win32 /m /nologo /v:minimal %*
exit /b %ERRORLEVEL%
