"""Export the verified r19 source and automatic-browser manifest to the user's requested Desktop folder.

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
    prepared=desktop/'NLBinder-masterNEW-r19-prepared-20261109'
    backup=desktop/'NLBinder-masterNEW-r18-backup-20261109'
    # Verify every computed absolute path before any recursive rename or export.
    for path in [target,prepared,backup]:
        require(path.parent.resolve(strict=True)==desktop.resolve(strict=True) and path.resolve().parent==desktop.resolve(strict=True),
                'Folder must be directly inside the intended Desktop')
        require(not path.is_symlink() and not path.is_junction(),'Refusing redirected Desktop folder')
    require(target.is_dir() and (target/'Keybinder.vcxproj').is_file(),'Original latest source folder missing')
    require(not prepared.exists() and not backup.exists(),'A previous export/backup exists; refusing overwrite')
    previous=json.loads((target/'PRUEFBERICHT.json').read_text(encoding='utf-8-sig'))
    require(previous['revision']=='v0.1.1-r18' and previous['sha256']=='4eec175d23397a51d4da28877b8f1109044cce8523cbcb06c7a70aa6cbf837b9', 'Unexpected prior snapshot')
    if (target/'NLBinder.asi').exists():
        require(sha(target/'NLBinder.asi')==previous['sha256'], 'Previous ASI has user changes')
    # Users may have moved the loose ASI into GTA. Verify the actual source
    # snapshot independently instead of requiring that moved file to exist.
    import export_source_bundle_revision19 as snapshot
    repository=ROOT/'build/github-publication/repository'
    entries=snapshot.source_entries(repository,'v0.1.1-r18')
    selected={name:oid for name,oid in entries.items() if name.startswith(('src/','tests/','res/'))
        or name.startswith('include/') and len(Path(name).parts)==2
        or name in {'Keybinder.vcxproj','Keybinder.rc','Keybinder.sln'}}
    for name,data in snapshot.read_blobs(repository,selected).items():
        path=target/name
        require(path.is_file(), 'Prior source missing: '+name)
        current=path.read_bytes()
        if path.suffix.lower() in {'.cpp','.h','.rc','.vcxproj','.sln','.json'}:
            current=current.replace(b'\r\n',b'\n');data=data.replace(b'\r\n',b'\n')
        require(current==data,'User source changed: '+name)
    publication=json.loads((ROOT/'build/revision19-published.json').read_text())
    proof=json.loads((ROOT/'build/revision19-package.json').read_text())
    require(publication['published'] and publication['asi_sha256']==proof['asi_sha256'],'Verified publication missing')
    subprocess.run([sys.executable,str(ROOT/'tools/export_source_bundle_revision19.py'),'--destination',str(prepared),
        '--revision',publication['commit']],cwd=ROOT,check=True)
    require(sha(prepared/'NLBinder.asi')==proof['asi_sha256'],'Prepared ASI differs')
    require(sha(prepared/'CHANGELOG-GESAMT.txt')==proof['changelog_sha256'],'Prepared changelog differs')
    runtime=json.loads((prepared/'BROWSER-RUNTIME.json').read_text())
    require(runtime['runtime_id']=='cef156-r1' and runtime['archive_sha256']=='01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45', 'Automatic browser manifest differs')
    # Atomic directory renames preserve the whole supplied r18 tree as a backup.
    require(target.resolve().parent==desktop.resolve() and backup.resolve().parent==desktop.resolve(),'Unsafe final rename')
    target.rename(backup)
    prepared.rename(target)
    result={'revision':'v0.1.1-r19','desktop_source':str(target),'original_backup':str(backup),
      'source_commit':publication['commit'],'asi_sha256':sha(target/'NLBinder.asi'),
      'all_source_files_verified':True,'browser_automatic_download_manifest_verified':True,
      'game_installation_modified':False}
    (ROOT/'build/revision19-desktop-export.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
