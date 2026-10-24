"""Exercise only the reconnect helper plus a harmless fixture executable.

The fixture is a fake waiting parent or argv logger, never a game launcher.
Build tests/ReconnectWorkerFixture.cpp as build/ReconnectWorkerFixture.exe first.
"""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "build/ReconnectHelper.exe"
FIXTURE = ROOT / "build/ReconnectWorkerFixture.exe"
OUTPUT = ROOT / "build/reconnect-worker-tests"
OUTPUT.mkdir(exist_ok=True)

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateEventW.argtypes = (wintypes.LPVOID, wintypes.BOOL, wintypes.BOOL, wintypes.LPCWSTR)
kernel.CreateEventW.restype = wintypes.HANDLE
kernel.SetEvent.argtypes = (wintypes.HANDLE,)
kernel.SetEvent.restype = wintypes.BOOL
kernel.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
kernel.WaitForSingleObject.restype = wintypes.DWORD
kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
kernel.CloseHandle.restype = wintypes.BOOL

checks = 0


def check(value, message):
    global checks
    checks += 1
    if not value:
        raise AssertionError(message)


def event(name):
    handle = kernel.CreateEventW(None, True, False, name)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    return handle


def spawn(arguments, **kwargs):
    return subprocess.Popen([str(value) for value in arguments],
                            creationflags=subprocess.CREATE_NO_WINDOW, **kwargs)


def stop_owned(process):
    if process is not None and process.poll() is None:
        process.terminate()
        process.wait(timeout=5)


def run_case(ready_exists):
    suffix = uuid.uuid4().hex
    ready_name = "Local\\NLBinderWorkerTestReady-" + suffix
    exit_name = "Local\\NLBinderWorkerTestExit-" + suffix
    ready = event(ready_name) if ready_exists else None
    parent_exit = event(exit_name)
    log = OUTPUT / (suffix + ".argv.txt")
    parent = worker = None
    try:
        parent = spawn([FIXTURE, "--fixture-parent", exit_name])
        check(parent.poll() is None, "Fixture parent must be alive")
        expected_arguments = ["-h", "127.0.0.1", "-p", "7777", "-n", "[RCB]Fixture",
                              "-g", "C:\\Harmless Test Game Path\\", 'quoted\\"argument']
        environment = dict(os.environ, NLBINDER_TEST_LAUNCH_LOG=str(log))
        worker = spawn([HELPER, "--parent", parent.pid, "--ready", ready_name,
                        "--mode", "openmp", "--launcher", FIXTURE,
                        "--working-directory", OUTPUT, *expected_arguments], env=environment)
        if ready_exists:
            check(kernel.WaitForSingleObject(ready, 4000) == 0, "Helper must signal readiness")
            check(parent.poll() is None, "Readiness must precede parent exit")
            time.sleep(0.3)
            check(worker.poll() is None, "Helper must wait while parent is alive")
            check(not log.exists(), "Launcher must not start before parent exit")
        else:
            check(worker.wait(timeout=5) == 3, "Missing ready event must fail closed")
            check(not log.exists(), "Readiness failure must not launch fixture")

        check(bool(kernel.SetEvent(parent_exit)), "Fixture exit event must be signaled")
        check(parent.wait(timeout=5) == 0, "Fixture parent must exit normally")
        if ready_exists:
            check(worker.wait(timeout=5) == 0, "Ready worker must launch after parent exits")
            deadline = time.monotonic() + 5
            while not log.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            check(log.exists(), "Dummy launcher must write argv log")
            # Fixture writes and closes a small UTF-16 file immediately.
            deadline = time.monotonic() + 2
            actual_arguments = []
            while time.monotonic() < deadline:
                try:
                    actual_arguments = log.read_text(encoding="utf-16").splitlines()
                    if actual_arguments:
                        break
                except (OSError, UnicodeError):
                    pass
                time.sleep(0.01)
            check(actual_arguments == [str(FIXTURE), *expected_arguments], "Launcher argv must round-trip exactly")
        else:
            time.sleep(0.1)
            check(not log.exists(), "Failed readiness must remain cancelled after parent exit")
    finally:
        kernel.SetEvent(parent_exit)
        stop_owned(worker)
        stop_owned(parent)
        if ready:
            kernel.CloseHandle(ready)
        kernel.CloseHandle(parent_exit)


check(HELPER.is_file() and FIXTURE.is_file(), "Built helper and fixture are required")
invalid = spawn([HELPER])
check(invalid.wait(timeout=5) == 2, "Invalid helper arguments must return 2")
run_case(True)
run_case(False)
result = {"checks": checks, "failures": 0,
          "scope": "Harmless fixture processes only; no game or installed launcher executed"}
(OUTPUT / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
