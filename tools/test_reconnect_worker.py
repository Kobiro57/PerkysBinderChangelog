"""Exercise only the reconnect helper plus a harmless fixture executable.

The fixture is a fake waiting parent or argv logger, never a game launcher.
Build tests/ReconnectWorkerFixture.cpp as build/ReconnectWorkerFixture.exe first.
"""
import ctypes
from ctypes import wintypes
import json
import os
import shutil
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


def run_cancel_case():
    suffix = uuid.uuid4().hex
    ready_name = "Local\\NLBinderCancelReady-" + suffix
    exit_name = "Local\\NLBinderCancelParentExit-" + suffix
    cancel_name = "Local\\NLBinderCancel-" + suffix
    ready, parent_exit, cancel = event(ready_name), event(exit_name), event(cancel_name)
    log = OUTPUT / (suffix + ".cancel-argv.txt")
    parent = worker = None
    try:
        parent = spawn([FIXTURE, "--fixture-parent", exit_name])
        worker = spawn([HELPER, "--parent", parent.pid, "--ready", ready_name,
            "--mode", "openmp", "--launcher", FIXTURE, "--working-directory", OUTPUT,
            "--cancel", cancel_name], env=dict(os.environ, NLBINDER_TEST_LAUNCH_LOG=str(log)))
        check(kernel.WaitForSingleObject(ready, 4000) == 0, "Cancellable worker must be ready before parent exit")
        check(parent.poll() is None and not log.exists(), "Cancellable worker must keep waiting without launching")
        check(bool(kernel.SetEvent(cancel)), "Cancellation event must be signalable by the parent")
        check(worker.wait(timeout=5) == 8, "Cancellation must stop the worker without termination privileges")
        check(parent.poll() is None and not log.exists(), "Cancelling must leave old game fixture untouched")
        kernel.SetEvent(parent_exit)
        check(parent.wait(timeout=5) == 0, "Cancelled parent may exit normally later")
        time.sleep(0.1)
        check(not log.exists(), "A later parent exit must not revive a cancelled reconnect")
    finally:
        kernel.SetEvent(parent_exit)
        stop_owned(worker)
        stop_owned(parent)
        for handle in (ready, parent_exit, cancel):
            kernel.CloseHandle(handle)


run_cancel_case()


def run_nova_case(name, *, required=1, delay=0, entry_delay=500, no_entry=False,
                  crash=0, decoy=False, preexisting=False):
    folder = OUTPUT / (name + "-" + uuid.uuid4().hex)
    folder.mkdir()
    launcher = folder / "NovaFixture.exe"
    game = folder / "Game With Spaces" / "gta_sa.exe"
    helper = folder / "Worker.exe"
    game.parent.mkdir()
    for source, target in ((FIXTURE, launcher), (FIXTURE, game), (HELPER, helper)):
        shutil.copyfile(source, target)
    ready_name = "Local\\NLBinderNovaReady-" + uuid.uuid4().hex
    exit_name = "Local\\NLBinderNovaParentExit-" + uuid.uuid4().hex
    gui_exit_name = "Local\\NLBinderNovaFixtureExit-" + uuid.uuid4().hex
    ready, parent_exit, gui_exit = event(ready_name), event(exit_name), event(gui_exit_name)
    log = folder / "fixture.log"
    environment = dict(os.environ,
        NLBINDER_TEST_ROLE="nova", NLBINDER_TEST_FIXTURE_EXIT_EVENT=gui_exit_name,
        NLBINDER_TEST_GAME_EXE=str(game), NLBINDER_TEST_NOVA_LOG=str(log),
        NLBINDER_TEST_REQUIRED_CLICKS=str(required), NLBINDER_TEST_LAUNCH_DELAY_MS=str(delay),
        NLBINDER_TEST_ENTRY_DELAY_MS=str(entry_delay), NLBINDER_TEST_NO_ENTRY=str(int(no_entry)),
        NLBINDER_TEST_FIRST_GAME_CRASH_MS=str(crash),
        NLBINDER_TEST_BUTTON_DELAY_MS="400", NLBINDER_TEST_NOVA_LABEL="  SPIEL  STARTEN  ")
    parent = worker = extra = None
    try:
        parent = spawn([FIXTURE, "--fixture-parent", exit_name])
        if preexisting or decoy:
            other = game
            if decoy:
                other = folder / "Other Game" / "gta_sa.exe"
                other.parent.mkdir()
                shutil.copyfile(FIXTURE, other)
            extra = spawn([other, "--fixture-game", "stable"], env=environment)
            time.sleep(0.2)
            check(extra.poll() is None, "Harmless extra game fixture must be alive")
        worker = spawn([helper, "--parent", parent.pid, "--ready", ready_name,
            "--mode", "nova", "--launcher", launcher, "--working-directory", folder,
            "--game-executable", game, "--timeout-ms", "4500" if no_entry else "14000",
            "--retry-ms", "1000", "--quiet-errors", "true"], env=environment)
        if preexisting:
            check(worker.wait(timeout=5) == 7, "Another same-path game must refuse readiness")
            check(kernel.WaitForSingleObject(ready, 0) != 0, "Duplicate-instance refusal must not signal ready")
            check(parent.poll() is None, "Duplicate-instance refusal must keep old parent alive")
            check(not log.exists() or "play\n" not in log.read_text(), "Duplicate-instance refusal must not press Play")
            return
        check(kernel.WaitForSingleObject(ready, 4000) == 0, "Nova helper must signal readiness")
        check(parent.poll() is None, "Nova readiness must precede parent exit")
        check(not log.exists() or "play\n" not in log.read_text(), "Nova must not invoke Play before parent exits")
        check(bool(kernel.SetEvent(parent_exit)), "Nova parent exit event must be set")
        check(parent.wait(timeout=5) == 0, "Nova parent fixture must exit normally")
        expected_exit = 6 if no_entry else 0
        check(worker.wait(timeout=22) == expected_exit, "Nova helper must confirm entry or explicitly time out")
        lines = log.read_text().splitlines()
        check(lines.count("play") == (required + int(bool(crash))), "Normal Play must retry only as required")
        check(lines.count("game-launched") == (1 + int(bool(crash))), "Only intended harmless game instances may be launched")
        check("unexpected-kill" not in lines, "The running-game button must never be activated")
        trace = (folder / "NLBinderReconnect.log").read_text(encoding="utf-16-le")
        check(("game-entered-confirmed" in trace) != no_entry, "Success must require the new binder's entry handshake")
        if no_entry:
            check("timeout-game-not-entered" in trace, "A live unentered game must not count as successful reconnect")
        if decoy:
            check(extra.poll() is None, "Same-basename game in another folder must remain untouched")
    finally:
        kernel.SetEvent(parent_exit)
        kernel.SetEvent(gui_exit)
        if worker is not None and worker.poll() is None:
            stop_owned(worker)
        stop_owned(parent)
        if extra is not None:
            try:
                extra.wait(timeout=3)
            except subprocess.TimeoutExpired:
                stop_owned(extra)
        # Child fixture windows consume the shared exit event on their timer.
        time.sleep(0.2)
        for handle in (ready, parent_exit, gui_exit):
            kernel.CloseHandle(handle)


run_nova_case("retry-play", required=2)
run_nova_case("busy-delayed-start", delay=1700)
run_nova_case("live-game-not-entered", no_entry=True)
run_nova_case("startup-crash-retry", crash=200)
run_nova_case("different-path-decoy", decoy=True)
run_nova_case("same-path-already-running", preexisting=True)
result = {"checks": checks, "failures": 0,
          "nova_cases": 6,
          "cancellation_after_readiness_checked": True,
          "nova_start_requires_spawn_confirmation": True,
          "scope": "Harmless fixture processes only; no game or installed launcher executed"}
(OUTPUT / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
