@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cl.exe /nologo /EHsc /std:c++17 /W4 /MT /DWIN32 /D_CRT_SECURE_NO_WARNINGS /Fo"%~dp0..\build\ForumKeyboardProbe.obj" /Fe"%~dp0..\build\ForumKeyboardProbe.exe" "%~dp0..\tests\ForumKeyboardProbe.cpp" /link user32.lib
exit /b %ERRORLEVEL%
