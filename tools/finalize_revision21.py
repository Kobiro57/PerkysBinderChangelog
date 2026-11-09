"""Export r21, retaining the previous Desktop folder and all user modifications."""
from pathlib import Path
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
PREVIOUS='9a728abaf27b1784cf3b6a65beff4746add2120e'

def require(ok,message):
    if not ok:raise ValueError(message)

def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def unchanged_previous(target):
    if not target.is_dir():return False
    manifest=ROOT/'build/source-export-r20.json'
    if not manifest.is_file():return False
    old=json.loads(manifest.read_text(encoding='utf-8-sig'))
    if old.get('source_commit')!=PREVIOUS:return False
    actual=set()
    for path in target.rglob('*'):
        if path.is_symlink() or path.is_junction():return False
        if path.is_file():actual.add(path.relative_to(target).as_posix())
    expected=set(old['files'])
    if not (target/'NLBinder.asi').exists():expected.discard('NLBinder.asi')
    if actual!=expected:return False
    for name,digest in old['files'].items():
        path=target/name
        if name=='NLBinder.asi' and not path.exists():continue
        if not path.resolve().is_relative_to(target.resolve()) or path.is_symlink() or not path.is_file() or sha(path)!=digest:
            return False
    return True

def main():
    desktop=Path.home()/'Desktop'
    require(desktop.resolve(strict=True)==ROOT.parent.resolve(strict=True),'Unexpected Desktop location')
    latest=desktop/'NLBinder-v0.1.2'
    fallback=desktop/'NLBinder-v0.1.2-r21'
    prepared=desktop/'NLBinder-v0.1.2-r21-prepared-20261109'
    backup=desktop/'NLBinder-v0.1.2-r20-backup-20261109'
    for path in (latest,fallback,prepared,backup):
        require(path.parent.resolve(strict=True)==desktop.resolve(strict=True) and path.resolve().parent==desktop.resolve(strict=True),
                'Export/backup must be directly inside the intended Desktop')
        require(not path.is_symlink() and not path.is_junction(),'Redirected Desktop path refused')
    require(not prepared.exists(),'Previous prepared export is preserved')
    replace=latest.exists() and unchanged_previous(latest)
    target=latest if replace or not latest.exists() else fallback
    require(not target.exists() or replace,'Existing unexpected export is preserved')
    if replace:require(not backup.exists(),'Previous backup is preserved')
    publication=json.loads((ROOT/'build/revision21-published.json').read_text(encoding='utf-8'))
    proof=json.loads((ROOT/'build/revision21-package.json').read_text(encoding='utf-8'))
    require(publication['published'] and publication['tag']=='v0.1.2-r21' and publication['asi_sha256']==proof['asi_sha256'],
            'Verified r21 publication missing')
    subprocess.run([sys.executable,str(ROOT/'tools/export_source_bundle_revision21.py'),
        '--destination',str(prepared),'--revision',publication['commit']],cwd=ROOT,check=True)
    for name,key in [('NLBinder.asi','asi_sha256'),('CHANGELOG-GESAMT.txt','changelog_sha256'),
                     ('FORUM-CHANGELOG.txt','forum_changelog_sha256'),('FORUM-INSTALLATION.txt','forum_installation_sha256'),
                     ('FORUM-DATEILISTE.txt','forum_file_list_sha256')]:
        require(sha(prepared/name)==proof[key],'Prepared export differs: '+name)
    focus=prepared/'FORUM-INSTALLATION-QUELLCODE'
    actual={p.relative_to(focus).as_posix():p for p in focus.rglob('*') if p.is_file()}
    require(set(actual)==set(proof['forum_review_files']),'Focused installer source set differs')
    for name,digest in proof['forum_review_files'].items():
        require(actual[name].resolve().is_relative_to(focus.resolve()) and sha(actual[name])==digest,'Focused source differs: '+name)
    if replace:
        require(unchanged_previous(latest),'User changed the previous source during export; nothing replaced')
        require(latest.resolve().parent==desktop.resolve() and backup.resolve().parent==desktop.resolve(),'Unsafe backup rename')
        latest.rename(backup)
    require(not target.exists() and prepared.resolve().parent==desktop.resolve() and target.resolve().parent==desktop.resolve(),'Unsafe final rename')
    prepared.rename(target)
    result={'revision':'v0.1.2-r21','desktop_source':str(target),'original_backup':str(backup) if replace else None,
        'source_commit':publication['commit'],'asi_sha256':sha(target/'NLBinder.asi'),'all_source_files_verified':True,
        'focused_forum_source_files_verified':len(actual),'unexpected_user_changes_preserved':True,
        'postable_forum_changelog_verified':True,'game_installation_modified':False}
    (ROOT/'build/revision21-desktop-export.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
