@echo off
setlocal
pushd "%~dp0.." || exit /b 2
if not defined NLBINDER_PYTHON set "NLBINDER_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if /i "%~1"=="tests" goto tests
if /i "%~1"=="build" goto build
if /i "%~1"=="smoke" goto smoke
if /i "%~1"=="repository" goto repository
if /i "%~1"=="native-smoke" goto nativesmoke
if /i "%~1"=="worker" goto worker
if /i "%~1"=="document" goto document
if /i "%~1"=="package" goto package
if /i "%~1"=="prepare-publication" goto preparepublication
if /i "%~1"=="publish" goto publish
if /i "%~1"=="export" goto export
echo Usage: tools\run_revision16.bat tests^|build^|smoke^|repository
popd
exit /b 2
:tests
call tools\run_tests.bat > build\revision16-tests.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
"%NLBINDER_PYTHON%" tools\report_revision16_log.py tests
goto finish
:build
call tools\build_forum_browser.bat > build\revision16-browser-build.log 2>&1
if errorlevel 1 (set "NLBINDER_RESULT=2"&"%NLBINDER_PYTHON%" tools\report_revision16_log.py browser-build&goto finish)
call tools\build_release.bat > build\revision16-build.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
>>build\revision16-build.log echo RELEASE_BUILD_EXIT_CODE=%NLBINDER_RESULT%
"%NLBINDER_PYTHON%" tools\report_revision16_log.py build
goto finish
:smoke
"%NLBINDER_PYTHON%" tools\test_forum_host.py > build\revision16-browser-smoke.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
"%NLBINDER_PYTHON%" tools\report_revision16_log.py browser-smoke
goto finish
:nativesmoke
"%NLBINDER_PYTHON%" tools\prepare_browser_runtime.py
if errorlevel 1 (set "NLBINDER_RESULT=2"&goto finish)
build\KeybinderTests.exe --native-forum-smoke > build\revision16-native-smoke.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
"%NLBINDER_PYTHON%" tools\report_revision16_log.py native-smoke
goto finish
:repository
if not exist build\github-publication mkdir build\github-publication
if not exist build\github-publication\repository\.git (
    git clone https://github.com/Kobiro57/PerkysBinderChangelog.git build\github-publication\repository
    if errorlevel 1 (set "NLBINDER_RESULT=2"&goto finish)
)
git -C build\github-publication\repository fetch origin --tags
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:worker
"%NLBINDER_PYTHON%" tools\test_reconnect_worker.py > build\revision16-reconnect.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
type build\revision16-reconnect.log
goto finish
:document
"%NLBINDER_PYTHON%" tools\document_revision16.py
if errorlevel 1 (set "NLBINDER_RESULT=2"&goto finish)
"%NLBINDER_PYTHON%" tools\document_publication_revision16.py
if errorlevel 1 (set "NLBINDER_RESULT=2"&goto finish)
"%NLBINDER_PYTHON%" tools\export_full_changelog.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:package
"%NLBINDER_PYTHON%" tools\verify_revision16_artifact.py
if errorlevel 1 (set "NLBINDER_RESULT=2"&goto finish)
"%NLBINDER_PYTHON%" tools\package_revision16.py > build\revision16-packaging.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
type build\revision16-packaging.log
goto finish
:preparepublication
"%NLBINDER_PYTHON%" tools\publish_revision16.py --prepare
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:publish
"%NLBINDER_PYTHON%" tools\publish_revision16.py --publish
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:export
"%NLBINDER_PYTHON%" tools\finalize_revision16.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:finish
popd
exit /b %NLBINDER_RESULT%
