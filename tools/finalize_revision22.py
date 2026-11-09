"""Export v0.1.2 to a new Desktop folder, preserving every previous export."""
from pathlib import Path
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]

def require(ok,message):
    if not ok:raise ValueError(message)

def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    desktop=Path.home()/'Desktop'
    require(desktop.resolve(strict=True)==ROOT.parent.resolve(strict=True),'Unexpected Desktop location')
    target=desktop/'NLBinder-v0.1.2-r22'
    prepared=desktop/'NLBinder-v0.1.2-r22-prepared-20261109'
    for path in [target,prepared]:
        require(path.parent.resolve(strict=True)==desktop.resolve(strict=True)
                and path.resolve().parent==desktop.resolve(strict=True),'Unexpected export path')
        require(not path.is_symlink() and not path.is_junction(),'Redirected Desktop folder refused')
        require(not path.exists(),'Existing export is preserved: '+path.name)
    publication=json.loads((ROOT/'build/revision22-published.json').read_text(encoding='utf-8'))
    proof=json.loads((ROOT/'build/revision22-package.json').read_text(encoding='utf-8'))
    require(publication['published'] and publication['tag']=='v0.1.2-r22'
            and publication['asi_sha256']==proof['asi_sha256'],'Verified publication missing')
    subprocess.run([sys.executable,str(ROOT/'tools/export_source_bundle_revision22.py'),
        '--destination',str(prepared),'--revision',publication['commit']],cwd=ROOT,check=True)
    require(sha(prepared/'NLBinder.asi')==proof['asi_sha256'],'Prepared ASI differs')
    require(sha(prepared/'CHANGELOG-GESAMT.txt')==proof['changelog_sha256'],'Complete changelog differs')
    require(sha(prepared/'FORUM-CHANGELOG.txt')==proof['forum_changelog_sha256'],'Postable forum changelog differs')
    require(sha(prepared/'FORUM-INSTALLATION.txt')==proof['forum_installation_sha256'],'Installer disclosure differs')
    require(sha(prepared/'FORUM-DATEILISTE.txt')==proof['forum_file_list_sha256'],'Runtime file list differs')
    focus=prepared/'FORUM-INSTALLATION-QUELLCODE'
    actual={p.relative_to(focus).as_posix():p for p in focus.rglob('*') if p.is_file()}
    require(set(actual)==set(proof['forum_review_files']),'Focused installer source set differs')
    for name,digest in proof['forum_review_files'].items():
        require(actual[name].resolve().is_relative_to(focus.resolve()) and sha(actual[name])==digest,
                'Focused installer file differs: '+name)
    runtime=json.loads((prepared/'BROWSER-RUNTIME.json').read_text(encoding='utf-8'))
    require(runtime['runtime_id']=='cef156-r1'
            and runtime['archive_sha256']=='01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45',
            'Unchanged automatic browser manifest differs')
    require(prepared.resolve().parent==desktop.resolve() and target.resolve().parent==desktop.resolve(),
            'Unsafe final rename')
    prepared.rename(target)
    result={'revision':'v0.1.2-r22','desktop_source':str(target),'source_commit':publication['commit'],
        'asi_sha256':sha(target/'NLBinder.asi'),'all_source_files_verified':True,
        'focused_forum_source_files_verified':len(actual),'forum_installation_paths_documented':True,
        'postable_forum_changelog_verified':True,'previous_desktop_exports_modified':False,
        'game_installation_modified':False}
    (ROOT/'build/revision22-desktop-export.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
