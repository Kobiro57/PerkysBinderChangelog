@echo off
setlocal
pushd "%~dp0.." || exit /b 2
if not exist build mkdir build
if not defined NLBINDER_PYTHON set "NLBINDER_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if /i "%~1"=="tests" goto tests
if /i "%~1"=="build" goto build
if /i "%~1"=="worker" goto worker
if /i "%~1"=="document" goto document
if /i "%~1"=="package" goto package
if /i "%~1"=="prepare-publication" goto preparepublication
if /i "%~1"=="publish" goto publish
if /i "%~1"=="verify-public" goto verifypublic
if /i "%~1"=="export" goto export
if /i "%~1"=="install" goto install
set "NLBINDER_RESULT=2"
goto finish
:tests
call tools\run_tests.bat > build\revision21-tests.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
"%NLBINDER_PYTHON%" tools\report_revision21_log.py tests
goto finish
:build
call tools\build_release.bat > build\revision21-build.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
>>build\revision21-build.log echo RELEASE_BUILD_EXIT_CODE=%NLBINDER_RESULT%
"%NLBINDER_PYTHON%" tools\report_revision21_log.py build
goto finish
:worker
"%NLBINDER_PYTHON%" tools\test_reconnect_worker.py > build\revision21-reconnect.log 2>&1
set "NLBINDER_RESULT=%ERRORLEVEL%"
type build\revision21-reconnect.log
goto finish
:document
"%NLBINDER_PYTHON%" tools\document_revision21.py
if errorlevel 1 goto failed
"%NLBINDER_PYTHON%" tools\export_full_changelog.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:package
"%NLBINDER_PYTHON%" tools\verify_revision21_artifact.py
if errorlevel 1 goto failed
"%NLBINDER_PYTHON%" tools\document_revision21.py
if errorlevel 1 goto failed
"%NLBINDER_PYTHON%" tools\export_full_changelog.py
if errorlevel 1 goto failed
"%NLBINDER_PYTHON%" tools\package_revision21.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:preparepublication
"%NLBINDER_PYTHON%" tools\publish_revision21.py --prepare
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:publish
"%NLBINDER_PYTHON%" tools\publish_revision21.py --publish
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:export
"%NLBINDER_PYTHON%" tools\finalize_revision21.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:verifypublic
"%NLBINDER_PYTHON%" tools\verify_revision21_public.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:install
"%NLBINDER_PYTHON%" tools\install_revision21.py
set "NLBINDER_RESULT=%ERRORLEVEL%"
goto finish
:failed
set "NLBINDER_RESULT=2"
:finish
popd
exit /b %NLBINDER_RESULT%
