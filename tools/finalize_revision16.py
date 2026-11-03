"""Export the verified r16 source and runtime to the user's requested Desktop folder.

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
    prepared=desktop/'NLBinder-masterNEW-r16-prepared-20261103'
    backup=desktop/'NLBinder-masterNEW-r15-backup-20261103'
    # Verify every computed absolute path before any recursive rename or export.
    for path in [target,prepared,backup]:
        require(path.parent.resolve(strict=True)==desktop.resolve(strict=True) and path.resolve().parent==desktop.resolve(strict=True),
                'Folder must be directly inside the intended Desktop')
        require(not path.is_symlink() and not path.is_junction(),'Refusing redirected Desktop folder')
    require(target.is_dir() and (target/'Keybinder.vcxproj').is_file(),'Original latest source folder missing')
    require(not prepared.exists() and not backup.exists(),'A previous export/backup exists; refusing overwrite')
    require(sha(target/'NLBinder.asi')=='9d2139f9889fe5368030f505e515575ef0acbb83d374046d42dddd3aff929018',
            'The user-supplied source folder changed; original remains untouched')
    publication=json.loads((ROOT/'build/revision16-published.json').read_text())
    proof=json.loads((ROOT/'build/revision16-package.json').read_text())
    require(publication['published'] and publication['asi_sha256']==proof['asi_sha256'],'Verified publication missing')
    subprocess.run([sys.executable,str(ROOT/'tools/export_source_bundle.py'),'--destination',str(prepared),
        '--revision',publication['commit']],cwd=ROOT,check=True)
    require(sha(prepared/'NLBinder.asi')==proof['asi_sha256'],'Prepared ASI differs')
    require(sha(prepared/'CHANGELOG-GESAMT.txt')==proof['changelog_sha256'],'Prepared changelog differs')
    runtime=json.loads((prepared/'BROWSER-RUNTIME.json').read_text())
    for entry in runtime['files']:
        path=prepared/'NLBinder/Browser'/entry['path']
        require(path.resolve().is_relative_to(prepared.resolve()) and sha(path)==entry['sha256'],'Prepared runtime differs')
    # Atomic directory renames preserve the whole supplied r15 tree as a backup.
    require(target.resolve().parent==desktop.resolve() and backup.resolve().parent==desktop.resolve(),'Unsafe final rename')
    target.rename(backup)
    prepared.rename(target)
    result={'revision':'v0.1.1-r16','desktop_source':str(target),'original_backup':str(backup),
      'source_commit':publication['commit'],'asi_sha256':sha(target/'NLBinder.asi'),
      'all_source_files_verified':True,'browser_runtime_files_verified':len(runtime['files']),
      'game_installation_modified':False}
    (ROOT/'build/revision16-desktop-export.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
