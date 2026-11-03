@echo off
setlocal
pushd "%~dp0.." || exit /b 2
if not defined NLBINDER_PYTHON set "NLBINDER_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if /i "%~1"=="browser-asset" goto asset
if /i "%~1"=="setup-tests" goto setuptests
if /i "%~1"=="tests" goto tests
if /i "%~1"=="build" goto build
if /i "%~1"=="native-smoke" goto nativesmoke
if /i "%~1"=="worker" goto worker
if /i "%~1"=="package" goto package
if /i "%~1"=="prepare-publication" goto preparepublication
if /i "%~1"=="publish" goto publish
if /i "%~1"=="export" goto export
set "NLBINDER_RESULT=2"
goto finish
:asset
"%NLBINDER_PYTHON%" build\publish_browser_asset_r17.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:setuptests
call tools\build_browser_setup.bat > build\revision17-setup-build.log 2>&1
if errorlevel 1 (type build\revision17-setup-build.log&set "NLBINDER_RESULT=2"&goto finish)
call tools\build_browser_setup_tests.bat >> build\revision17-setup-build.log 2>&1
if errorlevel 1 (type build\revision17-setup-build.log&set "NLBINDER_RESULT=2"&goto finish)
"%NLBINDER_PYTHON%" tools\test_browser_setup.py > build\revision17-setup-tests.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
type build\revision17-setup-tests.log
goto finish
:tests
call tools\run_tests.bat > build\revision17-tests.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
"%NLBINDER_PYTHON%" tools\report_revision17_log.py tests
goto finish
:build
call tools\build_release.bat > build\revision17-build.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
>>build\revision17-build.log echo RELEASE_BUILD_EXIT_CODE=%NLBINDER_RESULT%
"%NLBINDER_PYTHON%" tools\report_revision17_log.py build
goto finish
:nativesmoke
build\KeybinderTests.exe --native-forum-smoke > build\revision17-native-smoke.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
"%NLBINDER_PYTHON%" tools\report_revision17_log.py native-smoke
goto finish
:worker
"%NLBINDER_PYTHON%" tools\test_reconnect_worker.py > build\revision17-reconnect.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
type build\revision17-reconnect.log
goto finish
:package
"%NLBINDER_PYTHON%" tools\verify_revision17_artifact.py
if errorlevel 1 (set "NLBINDER_RESULT=2"&goto finish)
"%NLBINDER_PYTHON%" tools\package_revision17.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:preparepublication
"%NLBINDER_PYTHON%" tools\publish_revision17.py --prepare
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:publish
"%NLBINDER_PYTHON%" tools\publish_revision17.py --publish
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:export
"%NLBINDER_PYTHON%" tools\finalize_revision17.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:finish
popd
exit /b %NLBINDER_RESULT%
