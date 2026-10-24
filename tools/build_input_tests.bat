@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 || exit /b 1
cl /nologo /std:c++latest /EHsc /MTd /DINPUT_MANAGER_TEST_STANDALONE /Iinclude /Iinclude\SAMP-API\include /Iinclude\SAMP-API\src /Iinclude\RakHook\include /Iinclude\imgui-1.92.7 /Iinclude\imgui-1.92.7\backends /Iplugin-sdk\shared /Iplugin-sdk\shared\extensions /Iplugin-sdk\plugin_sa /Iplugin-sdk\plugin_sa\game_sa /Iplugin-sdk\plugin_sa\game_sa\rw /Febuild\InputManagerTests.exe tests\InputManagerTests.cpp
