"""Actual CEF keyboard regression with production-translated synthetic input.

Only a disposable anonymous profile and local DOM fixture are used. The
normal forum page is loaded anonymously once to establish its renderer;
then network requests are blocked. No server form is submitted, and the user's
clipboard or Windows keyboard state is never changed. Chromium/CEF binaries
are the already installed, pinned cef156-r1 binaries; nothing is rebuilt here.
"""
from pathlib import Path
import base64
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
import socket
import struct
import subprocess
import time
import urllib.parse
import urllib.request
import uuid

from test_forum_host import Command, Shared, COMMANDS

ROOT = Path(__file__).resolve().parents[1]
PROOF = ROOT / 'build/revision24-native-forum-keyboard.json'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


class DevTools:
    """Small stdlib-only WebSocket client, restricted to this loopback host."""
    def __init__(self, url):
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme != 'ws' or parsed.hostname not in {'127.0.0.1', 'localhost'}:
            raise ValueError('The test only accepts its own loopback DevTools endpoint')
        self.socket = socket.create_connection((parsed.hostname, parsed.port), timeout=10)
        nonce = base64.b64encode(os.urandom(16)).decode('ascii')
        self.socket.sendall((f'GET {parsed.path} HTTP/1.1\r\nHost: {parsed.hostname}:{parsed.port}\r\n'
            f'Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {nonce}\r\n'
            'Sec-WebSocket-Version: 13\r\n\r\n').encode('ascii'))
        headers = bytearray()
        while not headers.endswith(b'\r\n\r\n'):
            headers += self.exact(1)
            if len(headers) > 16384:
                raise ValueError('Unexpected WebSocket handshake')
        if not headers.startswith(b'HTTP/1.1 101 '):
            raise RuntimeError('DevTools WebSocket upgrade failed')
        self.sequence = 0

    def exact(self, size):
        data = bytearray()
        while len(data) < size:
            piece = self.socket.recv(size - len(data))
            if not piece:
                raise RuntimeError('DevTools disconnected')
            data += piece
        return bytes(data)

    def frame(self, opcode, payload):
        length = len(payload)
        header = bytes([0x80 | opcode])
        if length < 126:
            header += bytes([0x80 | length])
        elif length <= 65535:
            header += bytes([0x80 | 126]) + struct.pack('!H', length)
        else:
            header += bytes([0x80 | 127]) + struct.pack('!Q', length)
        mask = os.urandom(4)
        masked = bytes(ch ^ mask[i % 4] for i, ch in enumerate(payload))
        self.socket.sendall(header + mask + masked)

    def receive(self):
        fragments = bytearray()
        while True:
            first, second = self.exact(2)
            length = second & 0x7F
            if length == 126:
                length = struct.unpack('!H', self.exact(2))[0]
            elif length == 127:
                length = struct.unpack('!Q', self.exact(8))[0]
            if length > 20 * 1024 * 1024:
                raise RuntimeError('Unexpectedly large DevTools frame')
            mask = self.exact(4) if second & 0x80 else None
            body = self.exact(length)
            if mask:
                body = bytes(ch ^ mask[i % 4] for i, ch in enumerate(body))
            opcode = first & 15
            if opcode == 8:
                raise RuntimeError('DevTools closed its connection')
            if opcode == 9:
                self.frame(10, body)
                continue
            if opcode == 10:
                continue
            if opcode not in {0, 1}:
                raise RuntimeError('Unexpected DevTools data opcode')
            fragments += body
            if first & 0x80:
                return json.loads(fragments.decode('utf-8'))

    def call(self, method, params=None):
        self.sequence += 1
        request_id = self.sequence
        self.frame(1, json.dumps({'id': request_id, 'method': method, 'params': params or {}}).encode('utf-8'))
        while True:
            response = self.receive()
            if response.get('id') != request_id:
                continue
            if 'error' in response:
                raise RuntimeError('DevTools command failed: ' + json.dumps(response['error']))
            return response.get('result', {})

    def evaluate(self, expression):
        response = self.call('Runtime.evaluate', {'expression': expression, 'returnByValue': True, 'awaitPromise': True})
        if 'exceptionDetails' in response:
            raise RuntimeError('Local DOM fixture failed')
        return response.get('result', {}).get('value')

    def close(self):
        self.socket.close()


def main():
    probe = ROOT / 'build/ForumKeyboardProbe.exe'
    generated = json.loads(subprocess.check_output([str(probe)], cwd=ROOT))
    if generated['protocol_version'] != 1:
        raise ValueError('Unexpected keyboard probe protocol')
    runtime = Path.home() / 'Documents/GTA San Andreas User Files/NLBinder/Browser/cef156-r1'
    executable = runtime / 'NLBinderBrowser.exe'
    host = runtime / 'NLBinderBrowser.dll'
    lock = json.loads((ROOT / 'res/browser-runtime.json').read_text(encoding='utf-8'))
    locked = {item['path']: item for item in lock['files']}
    for file in (executable, host):
        if not file.is_file() or sha(file) != locked[file.name]['sha256']:
            raise ValueError('Installed browser does not match the reviewed cef156-r1 pin')
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileMappingW.argtypes = [W.HANDLE, C.c_void_p, W.DWORD, W.DWORD, W.DWORD, W.LPCWSTR]
    kernel.CreateFileMappingW.restype = W.HANDLE
    kernel.MapViewOfFile.argtypes = [W.HANDLE, W.DWORD, W.DWORD, W.DWORD, C.c_size_t]
    kernel.MapViewOfFile.restype = C.c_void_p
    kernel.CreateEventW.argtypes = [C.c_void_p, W.BOOL, W.BOOL, W.LPCWSTR]
    kernel.CreateEventW.restype = W.HANDLE
    kernel.SetEvent.argtypes = [W.HANDLE]
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.UnmapViewOfFile.argtypes = [C.c_void_p]
    assert C.sizeof(Command) == 28 and Shared.parentWindow.offset == 64
    base = ROOT / 'build/forum-keyboard-tests' / uuid.uuid4().hex
    base.mkdir(parents=True)
    mapping_name = 'Local\\NLBinder.Forum.keyboard.' + uuid.uuid4().hex
    mapping = kernel.CreateFileMappingW(W.HANDLE(-1), None, 0x04, 0, C.sizeof(Shared), mapping_name)
    if not mapping:
        raise OSError(C.get_last_error(), 'CreateFileMapping')
    pointer = kernel.MapViewOfFile(mapping, 0xF001F, 0, 0, C.sizeof(Shared))
    event = kernel.CreateEventW(None, False, False, mapping_name + '.input')
    if not pointer or not event:
        raise OSError(C.get_last_error(), 'MapViewOfFile/CreateEvent')
    state = Shared.from_address(pointer)
    state.magic = 0x4E4C4346
    state.version = 1
    state.bytes = C.sizeof(Shared)
    with socket.socket() as port_socket:
        port_socket.bind(('127.0.0.1', 0))
        port = port_socket.getsockname()[1]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(executable), '--nlbinder-map=' + mapping_name,
        '--nlbinder-smoke-profile=' + str(base / 'profile'), '--remote-debugging-port=' + str(port),
        '--remote-debugging-address=127.0.0.1', '--disable-background-networking'], cwd=runtime, startupinfo=startup,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    devtools = None
    def wait_for(predicate, seconds=30):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('CEF host exited: ' + str(process.returncode))
            if predicate():
                return
            time.sleep(0.03)
        raise RuntimeError('CEF keyboard test timed out: ' + json.dumps({
            'state': state.state, 'frames': state.frameSequence, 'width': state.width,
            'height': state.height, 'loading': state.loading, 'error': state.errorCode,
            'read': state.commandRead, 'write': state.commandWrite}))
    def send(values):
        write = state.commandWrite & 0xFFFFFFFF
        if (write - (state.commandRead & 0xFFFFFFFF)) & 0xFFFFFFFF >= COMMANDS:
            raise RuntimeError('Keyboard input ring full')
        state.commands[write % COMMANDS] = Command(*values)
        state.commandWrite = C.c_int32(write + 1).value
        kernel.SetEvent(event)
    def endpoint():
        try:
            with opener.open(f'http://127.0.0.1:{port}/json/list', timeout=0.5) as response:
                targets = json.load(response)
            return next((target.get('webSocketDebuggerUrl') for target in targets if target.get('type') == 'page'), None)
        except (OSError, ValueError):
            return None
    def paint_barrier():
        # The first resize may precede the first paint, so a fixed frame count
        # is not a readiness condition. Force and acknowledge two different
        # viewport sizes instead; each acknowledgement comes from OnPaint.
        sequence = state.frameSequence
        send([0, 1280, 801, 0, 0, 0, 0])
        wait_for(lambda: state.frameSequence > sequence and state.width == 1280 and state.height == 801)
        sequence = state.frameSequence
        send([0, 1280, 800, 0, 0, 0, 0])
        wait_for(lambda: state.frameSequence > sequence and state.width == 1280 and state.height == 800)
    try:
        wait_for(lambda: state.state == 1)
        wait_for(endpoint)
        devtools = DevTools(endpoint())
        devtools.call('Network.enable')
        devtools.call('Runtime.enable')
        # OnAfterCreated marks the host ready before the renderer widget and
        # first navigation have finished. Sending keys at that point can be
        # discarded even though Runtime.evaluate already has a JS context.
        # Require the actual windowless view before constructing the fixture.
        send([0, 1280, 800, 0, 0, 0, 0])
        wait_for(lambda: state.frameSequence >= 1 and state.width == 1280
            and state.height == 800 and not state.loading and not state.errorCode)
        paint_barrier()
        wait_for(lambda: devtools.evaluate("document.readyState") == 'complete')
        devtools.call('Network.setBlockedURLs', {'urls': ['*']})
        devtools.call('Page.stopLoading')
        devtools.evaluate("document.title='NLBinder local keyboard fixture';document.body.replaceChildren();true")
        initial_frames = state.frameSequence
        results = []
        for test in generated['cases']:
            element = test['element']
            # This fixture exists only in this disposable renderer. It has no
            # form, links, network resources, event handlers or submit action.
            fixture = """(() => {
                document.body.replaceChildren();
                const tag=ELEMENT==='contenteditable'?'div':ELEMENT;
                const field=document.createElement(tag); field.id='keyboard-test';
                field.style='position:fixed;left:20px;top:20px;width:600px;height:160px';
                if(ELEMENT==='contenteditable')field.contentEditable='true';
                document.body.appendChild(field);
                if(PREFILL){if(ELEMENT==='contenteditable')field.textContent='erase me';else field.value='erase me';}
                field.focus(); if(field.setSelectionRange)field.setSelectionRange(field.value.length,field.value.length);
                window.fixture=field;
                window.keyEvents=0;
                field.addEventListener('keydown',()=>++window.keyEvents);
                return document.activeElement===field;
            })()""".replace('ELEMENT', json.dumps(element)).replace('PREFILL', json.dumps(test['prefill']))
            if devtools.evaluate(fixture) is not True:
                raise RuntimeError('Local fixture did not receive focus')
            # A user focuses a forum textbox with a click. Exercise that same
            # IPC path after layout has painted; DOM .focus() by itself does
            # not establish native render-widget input readiness.
            paint_barrier()
            send([5, 0, 0, 0, 0, 0, 0])
            send([5, 1, 0, 0, 0, 0, 0])
            send([1, 60, 60, 0, 0, 0, 0])
            send([2, 60, 60, 0, 0, 1, 0])
            send([2, 60, 60, 0, 1, 1, 0])
            wait_for(lambda: state.commandRead == state.commandWrite)
            wait_for(lambda: devtools.evaluate("document.hasFocus()&&document.activeElement===window.fixture") is True)
            # paint_barrier above acknowledges real compositor output with
            # this textbox; the ring counter only acknowledges task posting.
            readiness = devtools.evaluate("({focus:document.hasFocus(),active:document.activeElement===window.fixture,visibility:document.visibilityState,ready:document.readyState})")
            for command in test['commands']:
                send(command)
            wait_for(lambda: state.commandRead == state.commandWrite)
            expected = ''.join(chr(value) for value in test['expected_scalars'])
            # Wait for the queued keyboard events to reach this same renderer.
            # This is an execution barrier, rather than an arbitrary short sleep.
            expected_keys = sum(command[0] == 4 and command[1] == 0 for command in test['commands'])
            if expected_keys:
                wait_for(lambda: devtools.evaluate('window.keyEvents') >= expected_keys, 5)
            value_expression = "window.fixture.isContentEditable?window.fixture.textContent:window.fixture.value"
            actual = devtools.evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve(" + value_expression + "))))")
            # Character-only input can have no raw keydown to acknowledge. Do
            # not resend any input: give this one queued sequence a bounded
            # renderer completion interval and retain a wrong value as failure.
            if not test['legacy']:
                deadline = time.monotonic() + 3
                while actual != expected and time.monotonic() < deadline:
                    time.sleep(0.03)
                    actual = devtools.evaluate(value_expression)
            results.append({'name': test['name'], 'legacy': test['legacy'], 'expected': expected,
                'actual': actual, 'command_count': len(test['commands']), 'passed': actual == expected,
                'readiness': readiness, 'renderer_keydowns': devtools.evaluate('window.keyEvents')})
        fixed = [item for item in results if not item['legacy']]
        regression = any(item['legacy'] and not item['passed'] for item in results)
        passed = all(item['passed'] for item in fixed)
        devtools.close()
        devtools = None
        send([10, 0, 0, 0, 0, 0, 0])
        process.wait(timeout=10)
        proof = {'passed': passed and process.returncode == 0, 'runtime_id': 'cef156-r1',
            'protocol_version': generated['protocol_version'], 'cef_host_sha256': sha(executable),
            'cef_host_dll_sha256': sha(host), 'probe_sha256': sha(probe),
            'translator_sha256': sha(ROOT / 'include/ForumKeyboardInput.h'),
            'cases': results, 'fixed_cases_passed': sum(item['passed'] for item in fixed),
            'fixed_cases': len(fixed), 'legacy_regression_reproduced': regression,
            'actual_cef_ipc_keyboard_path': True, 'local_dom_fixture': True,
            'initial_offscreen_frames': initial_frames, 'renderer_input_readiness_checked': True,
            'viewport_paint_barriers_checked': True,
            'anonymous_isolated_profile': True, 'user_clipboard_modified': False,
            'network_blocked_during_fixture': True, 'login_or_post_submitted': False,
            'initial_page_loaded_read_only': True,
            'live_gta_gameplay_test_performed': False, 'clean_exit_code': process.returncode}
        PROOF.write_text(json.dumps(proof, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')
        print(json.dumps(proof, indent=2, ensure_ascii=True))
        if not proof['passed']:
            raise RuntimeError('CEF rejected one or more translated keyboard cases')
    finally:
        if devtools:
            devtools.close()
        if process.poll() is None:
            try:
                send([10, 0, 0, 0, 0, 0, 0])
                process.wait(timeout=5)
            except Exception:
                process.terminate()
                process.wait(timeout=5)
        kernel.UnmapViewOfFile(pointer)
        kernel.CloseHandle(event)
        kernel.CloseHandle(mapping)


if __name__ == '__main__':
    main()
