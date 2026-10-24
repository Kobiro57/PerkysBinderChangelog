@echo off
set PLUGIN_SDK_DIR=C:\Users\Administrator\Desktop\Code\NLBinder\plugin-sdk
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "%PLUGIN_SDK_DIR%\Plugin_SA\Plugin_SA.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
exit /b %ERRORLEVEL%
