@echo off
setlocal
call "%~dp0setup_msvc.bat" || exit /b 1
pushd "%~dp0.." || exit /b 1
cl.exe /nologo /EHsc /O2 /Gy /MT /utf-8 /std:c++latest /DWIN32 /D_CRT_SECURE_NO_WARNINGS /DGTASA /DPLUGIN_SGV_10US /DRW /D_DX9_SDK_INSTALLED ^
 /Iinclude /Iinclude\plugin-sdk\shared /Iinclude\plugin-sdk\shared\game /Iinclude\plugin-sdk\plugin_sa /Iinclude\plugin-sdk\plugin_sa\game_sa ^
 /Iinclude\plugin-sdk\plugin_sa\game_sa\enums /Iinclude\plugin-sdk\plugin_sa\game_sa\rw /Iinclude\plugin-sdk\injector /Iinclude\plugin-sdk\shared\dxsdk ^
 /Iinclude\SAMP-API\include /Iinclude\RakHook\include /Iinclude\imgui-1.92.7 ^
 /Fo"build\\" /Fe"build\GpsNativeAdapterTests.exe" tests\GpsNativeAdapterTests.cpp include\plugin-sdk\shared\game\CVector.cpp ^
 /link /OPT:REF
if errorlevel 1 (popd&exit /b 2)
build\GpsNativeAdapterTests.exe
set "NLBINDER_TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %NLBINDER_TEST_RESULT%
