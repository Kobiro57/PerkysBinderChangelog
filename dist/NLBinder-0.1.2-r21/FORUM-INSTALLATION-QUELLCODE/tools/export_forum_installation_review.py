"""Export an auditable, deterministic source-only view of the /forum installer.

This never downloads, executes browser code, or reads a user browser profile.
Text line endings are normalized to LF so a working tree and a Git-blob source
export produce identical files. The full binder tree is a separate deliverable.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    'tools/BrowserSetup.cpp', 'tools/build_browser_setup.bat',
    'tools/build_forum_browser.bat', 'tools/setup_msvc.bat',
    'tools/fetch_cef_runtime.py', 'tools/prepare_browser_runtime.py',
    'tools/prepare_browser_autoinstall.py', 'tools/cef-runtime.lock.json',
    'tools/export_forum_installation_review.py',
    'src/ForumBrowser.cpp', 'src/PersistentPaths.cpp', 'src/Main.cpp',
    'include/ForumBrowser.h', 'include/ForumBrowserProtocol.h',
    'include/BrowserSetupProtocol.h', 'include/BrowserRuntimeLock.h',
    'include/ForumAfkSession.h', 'include/PersistentPaths.h', 'include/ResourceIds.h', 'include/Version.h',
    'browser/ForumBrowserHost.cpp', 'browser/CMakeLists.txt',
    'res/browser-runtime.json', 'Keybinder.rc', 'Keybinder.vcxproj',
    'docs/FORUM-INSTALLATION-DE.md', 'docs/FORUM-DATEILISTE.txt',
)
MINIZ_FILES = (
    'LICENSE', 'NLBINDER-PROVENANCE.json', 'miniz.h', 'miniz.c',
    'miniz_common.h', 'miniz_export.h', 'miniz_tdef.h',
    'miniz_tinfl.h', 'miniz_tinfl.c', 'miniz_zip.h', 'miniz_zip.c',
)
README = '''# Quellcode zur automatischen /forum-Einrichtung

Dieser Ordner ist ein gezielter Prüfauszug aus dem mitgelieferten vollständigen
NLBinder-Quellcode. Er enthält keine Browserprogramme, DLLs, privaten Profile,
Cookies, Zugangsdaten oder persönlichen Chatlogs. Textdateien sind für einen
stabilen Vergleich auf LF-Zeilenenden vereinheitlicht.

## Zuerst lesen

- [Ablauf, Downloadquelle, Installationspfade und lokale Browserdaten](docs/FORUM-INSTALLATION-DE.md)
- [Alle 236 Runtime-Dateien mit Größe und SHA-256](docs/FORUM-DATEILISTE.txt)
- [Originales maschinenlesbares Runtime-Manifest](res/browser-runtime.json)
- `SOURCE-MANIFEST.json`: Größen und SHA-256 der enthaltenen Prüfauszugsdateien.
- `SHA256SUMS.txt`: Prüfsummen aller Nutzdateien einschließlich des Quellmanifests.

## Der eigentliche Code

`tools/BrowserSetup.cpp` enthält den nativen x86-Installer. Er verwendet Windows
WinHTTP/BCrypt und die enthaltenen miniz-Quellen unter `third_party/miniz/`.
`src/ForumBrowser.cpp` steuert ihn aus der ASI und verbindet Maus, Tastatur und Bild.
`browser/ForumBrowserHost.cpp` ist der selbst gebaute x64-CEF-Host. `src/Main.cpp`
zeigt die Registrierung von `/forum` und die AFK-/Back-Anbindung. Protokolle,
Ressourceneinbettung und Prüfmanifeste liegen in den jeweiligen Unterordnern bei.

Zum Bauen des nativen Installers: Visual Studio C++ Build Tools v143 und Windows
SDK, dann `tools/build_browser_setup.bat`. Das erzeugt `build/BrowserSetup.exe`.
Zum Bauen des Hosts: Python nur als Entwicklerwerkzeug für den gepinnten SDK-Abruf,
CMake, Windows-x64-C++-Toolchain und das passende CEF-SDK aus `tools/cef-runtime.lock.json`.
`tools/fetch_cef_runtime.py` lädt das SDK; `tools/build_forum_browser.bat` baut den Host.
Diese Skripte werden durch das Lesen/Entpacken dieses Ordners nicht ausgeführt.
Der Prüfauszug enthält nicht alle Abhängigkeiten/Ressourcen zum Bauen der ASI;
dafür den zusätzlich mitgelieferten vollständigen Binder-Quellbaum verwenden.

Der eigene Installer und der eigene Host liegen als tatsächlicher Quellcode bei.
Die komplette Chromium-/CEF-Engine liegt nicht als Quellbaum in diesem Paket.
Die installierte Runtime enthält vorcompilierte Drittanbieterdateien. Offizielle
Originalquellen und Lizenzen sind in der Installationsbeschreibung verlinkt;
die genaue verwendete Version und Binärdatei-Hashes stehen im Runtime-Manifest.
Eine veröffentlichte Prüfsumme beweist Dateigleichheit, nicht automatisch einen
reproduzierbaren Build oder die Fehlerfreiheit eines Browsers.
'''


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def normalized(path):
    require(path.is_file() and not path.is_symlink() and not path.is_junction(),
            'Missing or redirected source: ' + str(path))
    require(path.resolve().is_relative_to(ROOT.resolve()), 'Source escaped the project root')
    return path.read_bytes().replace(b'\r\n', b'\n')


def file_list_text(manifest):
    entries = manifest['files']
    require(len(entries) == 236 and sum(row['bytes'] for row in entries) == 429252102,
            'Unexpected runtime file set; review documentation before updating')
    require(manifest['runtime_id'] == 'cef156-r1'
            and manifest['archive_sha256'] == '01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45',
            'Runtime pin changed; update the reviewed documentation first')
    lines = [
        'NLBinder /forum – vollständiger fester Runtime-Dateibestand',
        '=' * 64, '',
        'Runtime-ID: ' + manifest['runtime_id'],
        'CEF-Version: ' + manifest['cef_version'],
        'Download: ' + manifest['download_url'],
        'Archiv: ' + manifest['asset'],
        'Archivgröße (Bytes): ' + str(manifest['archive_bytes']),
        'Archiv-SHA256: ' + manifest['archive_sha256'],
        'Dateien: ' + str(len(entries)),
        'Entpackte Summe (Bytes): ' + str(sum(row['bytes'] for row in entries)), '',
        'Alle folgenden Pfade relativ zu:',
        '<Windows-Dokumente>\\GTA San Andreas User Files\\NLBinder\\Browser\\cef156-r1\\', '',
        'Dateipfad | Größe in Bytes | SHA-256',
    ]
    seen = set()
    for row in sorted(entries, key=lambda item: item['path']):
        path = row['path']
        require(path not in seen and not Path(path).is_absolute() and '..' not in Path(path).parts,
                'Invalid or duplicate runtime path')
        require(isinstance(row['bytes'], int) and row['bytes'] >= 0
                and re.fullmatch('[0-9a-f]{64}', row['sha256']) is not None,
                'Invalid runtime entry')
        seen.add(path)
        lines.append(f"{path} | {row['bytes']} | {row['sha256']}")
    lines += ['',
        'Diese Liste erfasst exakt die 236 Dateien des heruntergeladenen ZIPs.',
        'Zusätzlich erzeugte Dateien (z.B. .verified-runtime, Installationscache,',
        'temporäre Helfer und Browserprofil/Cookies/Webcache) sind keine ZIP-Einträge.',
        'Ihre Orte und ihre Verwendung stehen in FORUM-INSTALLATION-DE.md.',
        'Der Quellcode-Prüfauszug enthält diese Programme nicht als Binärdateien.',
    ]
    return '\n'.join(lines) + '\n'


def build_files():
    manifest = json.loads((ROOT / 'res/browser-runtime.json').read_text(encoding='utf-8-sig'))
    expected_list = file_list_text(manifest).encode('utf-8')
    require(normalized(ROOT / 'docs/FORUM-DATEILISTE.txt') == expected_list,
            'FORUM-DATEILISTE.txt is stale; regenerate it from the runtime manifest')
    paths = list(SOURCES) + ['third_party/miniz/' + name for name in MINIZ_FILES]
    files = {name: normalized(ROOT / name) for name in paths}
    files['README.md'] = README.encode('utf-8')
    proof = {
        'format_version': 1,
        'scope': 'Own forum installer/host/controller source, pinned runtime manifest and miniz; no browser binaries or private runtime data.',
        'line_endings': 'LF',
        'runtime_manifest_sha256': sha(files['res/browser-runtime.json']),
        'files': {name: {'bytes': len(data), 'sha256': sha(data)} for name, data in sorted(files.items())},
    }
    files['SOURCE-MANIFEST.json'] = (json.dumps(proof, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    files['SHA256SUMS.txt'] = ''.join(sha(data) + '  ' + name + '\n'
                                      for name, data in sorted(files.items())).encode('utf-8')
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', required=True, type=Path)
    args = parser.parse_args()
    destination = args.destination.absolute()
    require(destination.name == 'FORUM-INSTALLATION-QUELLCODE', 'Expected the dedicated review folder name')
    require(destination.resolve() == destination and not destination.is_symlink() and not destination.is_junction(),
            'Redirected review destination refused')
    require(destination.parent.is_dir(), 'Destination parent must exist')
    files = build_files()
    if destination.exists():
        require(destination.is_dir(), 'Review destination is not a directory')
        require(not any(path.is_symlink() or path.is_junction() for path in destination.rglob('*')),
                'Redirected existing review contents refused')
        actual = {path.relative_to(destination).as_posix(): path for path in destination.rglob('*') if path.is_file()}
        require(set(actual) in (set(), set(files)), 'Refusing to overwrite foreign or partial review files')
        for name, path in actual.items():
            require(not path.is_symlink() and path.read_bytes() == files[name], 'Existing review file differs: ' + name)
    destination.mkdir(exist_ok=True)
    for name, data in sorted(files.items()):
        target = destination / name
        require(target.resolve().is_relative_to(destination), 'Review file escaped the destination')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    actual = {path.relative_to(destination).as_posix(): path for path in destination.rglob('*') if path.is_file()}
    require(set(actual) == set(files), 'Review file set differs')
    for name, data in files.items():
        require(actual[name].read_bytes() == data and not actual[name].is_symlink(), 'Review copy differs: ' + name)
    print(json.dumps({'review_files': len(files), 'review_bytes': sum(map(len, files.values())),
        'runtime_files_documented': 236, 'binary_runtime_files_included': 0, 'private_profiles_included': 0,
        'all_review_files_verified': True, 'source_manifest_sha256': sha(files['SOURCE-MANIFEST.json'])}, indent=2))


if __name__ == '__main__':
    main()
