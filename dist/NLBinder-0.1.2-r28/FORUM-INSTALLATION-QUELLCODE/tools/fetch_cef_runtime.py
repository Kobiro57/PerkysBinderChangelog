"""Fetch the pinned official CEF SDK into build/vendor and verify its checksum."""
from pathlib import Path
import argparse
import hashlib
import json
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
BASE = 'https://cef-builds.spotifycdn.com/'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inspect', action='store_true')
    args = parser.parse_args()
    lock_path = ROOT / 'tools/cef-runtime.lock.json'
    lock = json.loads(lock_path.read_text(encoding='utf-8'))
    if args.inspect:
        print(json.dumps(lock, indent=2)); return
    destination = ROOT / 'build/vendor'
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / lock['archive']
    if not archive.exists():
        temporary = archive.with_suffix(archive.suffix + '.part')
        print('Downloading official CEF SDK:', lock['version'], flush=True)
        with urllib.request.urlopen(BASE + lock['archive'], timeout=60) as response, temporary.open('wb') as output:
            while block := response.read(1024 * 1024): output.write(block)
        temporary.rename(archive)
    digest = hashlib.sha1()
    sha256 = hashlib.sha256()
    with archive.open('rb') as source:
        while block := source.read(1024 * 1024):
            digest.update(block); sha256.update(block)
    if digest.hexdigest() != lock['official_sha1'] or archive.stat().st_size != lock['archive_bytes']:
        raise ValueError('CEF archive differs from the official build index')
    if sha256.hexdigest() != lock['archive_sha256']:
        raise ValueError('CEF archive differs from the pinned SHA-256')
    cef_root = destination / lock['directory']
    marker = cef_root / '.nlbinder-verified-sdk.json'
    if not marker.exists():
        print('Extracting verified CEF SDK', flush=True)
        with tarfile.open(archive) as package:
            for member in package.getmembers():
                relative = Path(member.name)
                if relative.is_absolute() or '..' in relative.parts or not (member.isdir() or member.isfile()):
                    raise ValueError('Unsafe CEF archive entry')
                resolved = (destination / relative).resolve()
                if not resolved.is_relative_to(destination.resolve()): raise ValueError('CEF archive escapes destination')
            package.extractall(destination, filter='data')
        if not (cef_root / 'include/cef_app.h').is_file(): raise ValueError('CEF SDK headers missing')
        marker.write_text(json.dumps({'version':lock['version'], 'archive_sha256':sha256.hexdigest()},indent=2)+'\n',encoding='utf-8')
    (ROOT / 'build/cef-root.txt').write_text(str(cef_root),encoding='utf-8')
    print(json.dumps({'cef_root':str(cef_root),'archive_sha256':sha256.hexdigest()},indent=2),flush=True)

if __name__ == '__main__': main()
