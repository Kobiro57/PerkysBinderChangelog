"""Export the verified r17 source and automatic-browser manifest to the user's requested Desktop folder.

The previous folder is retained intact. No GTA process is stopped or restarted.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def require(ok,message):
    if not ok:raise ValueError(message)

def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    desktop=Path.home()/'Desktop'
    require(desktop.resolve(strict=True)==ROOT.parent.resolve(strict=True),'Unexpected Desktop location')
    target=desktop/'NLBinder-masterNEW'
    prepared=desktop/'NLBinder-masterNEW-r17-prepared-20261103'
    backup=desktop/'NLBinder-masterNEW-r16-backup-20261103'
    # Verify every computed absolute path before any recursive rename or export.
    for path in [target,prepared,backup]:
        require(path.parent.resolve(strict=True)==desktop.resolve(strict=True) and path.resolve().parent==desktop.resolve(strict=True),
                'Folder must be directly inside the intended Desktop')
        require(not path.is_symlink() and not path.is_junction(),'Refusing redirected Desktop folder')
    require(target.is_dir() and (target/'Keybinder.vcxproj').is_file(),'Original latest source folder missing')
    require(not prepared.exists() and not backup.exists(),'A previous export/backup exists; refusing overwrite')
    require(sha(target/'NLBinder.asi')=='69ae76195feb01223f2e10290391ff45d7e35d7f1b4747a5c555ecf6d02423e4',
            'The user-supplied source folder changed; original remains untouched')
    publication=json.loads((ROOT/'build/revision17-published.json').read_text())
    proof=json.loads((ROOT/'build/revision17-package.json').read_text())
    require(publication['published'] and publication['asi_sha256']==proof['asi_sha256'],'Verified publication missing')
    subprocess.run([sys.executable,str(ROOT/'tools/export_source_bundle.py'),'--destination',str(prepared),
        '--revision',publication['commit']],cwd=ROOT,check=True)
    require(sha(prepared/'NLBinder.asi')==proof['asi_sha256'],'Prepared ASI differs')
    require(sha(prepared/'CHANGELOG-GESAMT.txt')==proof['changelog_sha256'],'Prepared changelog differs')
    runtime=json.loads((prepared/'BROWSER-RUNTIME.json').read_text())
    require(runtime['runtime_id']=='cef156-r1' and runtime['archive_sha256']=='01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45', 'Automatic browser manifest differs')
    # Atomic directory renames preserve the whole supplied r16 tree as a backup.
    require(target.resolve().parent==desktop.resolve() and backup.resolve().parent==desktop.resolve(),'Unsafe final rename')
    target.rename(backup)
    prepared.rename(target)
    result={'revision':'v0.1.1-r17','desktop_source':str(target),'original_backup':str(backup),
      'source_commit':publication['commit'],'asi_sha256':sha(target/'NLBinder.asi'),
      'all_source_files_verified':True,'browser_automatic_download_manifest_verified':True,
      'game_installation_modified':False}
    (ROOT/'build/revision17-desktop-export.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
