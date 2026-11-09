"""Prepare/publish only the authorized r19 update to Kobiro57/PerkysBinderChangelog.

Uses the existing Git credential manager in memory; never logs or stores tokens.
Large Chromium binaries belong in the release asset, not Git history.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import urllib.request
import urllib.error

ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT/'build/github-publication/repository'
URL='https://github.com/Kobiro57/PerkysBinderChangelog'
API='https://api.github.com/repos/Kobiro57/PerkysBinderChangelog'
TAG='v0.1.1-r19'
PREVIOUS='b3ca24e43a8603d98215ca6b559adad16f3d41fe'
PACKAGE='dist/NLBinder-0.1.1-r19'
BASE='original-archive-92ce219'
GENERATED={'docs/review/SOURCE-COMPARISON.json','docs/review/DATEIVERGLEICH.md',
 'docs/review/DIFFSTAT.txt','docs/review/first-party-changes.patch',
 'docs/review/REVISION19-DIFFSTAT.txt','docs/review/revision19-changes.patch'}
PRIVATE_DIRS={'.git','.vs','__pycache__','output','debug','release','obj','cmakefiles','.codex','.agents','.aws'}
PRIVATE_NAMES={'keybinder.json','jobs_data.json','imgui.ini','gps.xml','.env','.git-credentials','nul'}
PRIVATE_SUFFIXES={'.obj','.pdb','.ilk','.exp','.log','.pyc','.suo','.user','.o'}
PROGRAM=['src','browser','tests','tools','res','include/*.h','Keybinder.rc','Keybinder.vcxproj','Keybinder.sln']

def require(ok,message):
    if not ok:raise ValueError(message)

def digest(data):return hashlib.sha256(data).hexdigest()

def filehash(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def git(*args,input=None):
    p=subprocess.run(['git',*args],cwd=REPO,input=input,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if p.returncode:raise RuntimeError('Git failed: '+str(args[:3])+': '+p.stderr.decode('utf-8','replace')[:1500])
    return p.stdout

def write(relative,data):
    p=REPO/relative;p.parent.mkdir(parents=True,exist_ok=True)
    p.write_bytes(data.encode('utf-8') if isinstance(data,str) else data)

def paths(revision):
    entries={}
    if revision==':':
        records=git('ls-files','--stage','-z').split(b'\0')
        for record in records:
            if not record:continue
            meta,name=record.split(b'\t',1);mode,oid,stage=meta.split()
            require(stage==b'0' and mode in {b'100644',b'100755'},'Unmerged/link entry')
            entries[name.decode()]=oid
    else:
        for record in git('ls-tree','-r','-z',revision).split(b'\0'):
            if not record:continue
            meta,name=record.split(b'\t',1);mode,kind,oid=meta.split()
            require(kind==b'blob' and mode in {b'100644',b'100755'},'Submodule/link entry')
            entries[name.decode()]=oid
    return entries

def blobs(entries):
    oids=sorted(set(entries.values()))
    process=subprocess.Popen(['git','cat-file','--batch'],cwd=REPO,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    raw,err=process.communicate(b'\n'.join(oids)+b'\n')
    require(process.returncode==0,'Cannot read source blobs')
    values={};at=0
    for oid in oids:
        end=raw.index(b'\n',at);header=raw[at:end].split()
        require(header[0]==oid and header[1]==b'blob','Invalid blob batch')
        size=int(header[2]);at=end+1;values[oid]=raw[at:at+size];at+=size+1
    require(at==len(raw),'Trailing source bytes')
    return {name:values[oid] for name,oid in entries.items()}

def allowed(relative):
    p=PurePosixPath(relative)
    return not (p.is_absolute() or '..' in p.parts or ':' in relative or '\\' in relative
      or any(x.lower() in PRIVATE_DIRS for x in p.parts[:-1])
      or p.name.lower() in PRIVATE_NAMES or p.suffix.lower() in PRIVATE_SUFFIXES)

def historical(relative):
    return bool(re.fullmatch(r'docs/review/(REVISION(?:[4-9]|1[0-8])-DIFFSTAT\.txt|revision(?:[4-9]|1[0-8])-changes\.patch)',relative))

def stage():
    require(git('remote','get-url','origin').decode().strip().removesuffix('.git')==URL,'Wrong target repository')
    require(git('rev-parse','HEAD').decode().strip()==PREVIOUS,'Publication must start on the known r18 commit')
    if git('status','--porcelain'):
        # Resume only an explicitly recorded, inspected preparation tree.
        checkpoint=json.loads((ROOT/'build/revision19-staged-owned.json').read_text())
        require(git('write-tree').decode().strip()==checkpoint['staged_tree'] and not git('diff','--name-only'),
                'Publication checkout changed outside the recorded preparation')
    proof=json.loads((ROOT/'build/revision19-package.json').read_text())
    require(proof['revision']==TAG and filehash(ROOT/'build/Release/NLBinder.asi')==proof['asi_sha256'],'Package proof stale')
    original_entries=paths(BASE)
    previous_entries=paths('HEAD')
    for start in ['src','tests','tools','docs','res','browser','third_party/miniz']:
        for parent,dirs,names in os.walk(ROOT/start):
            dirs[:]=[x for x in dirs if x.lower() not in PRIVATE_DIRS]
            for name in names:
                path=Path(parent)/name;relative=path.relative_to(ROOT).as_posix()
                if relative in GENERATED or historical(relative) or not allowed(relative):continue
                require(not path.is_symlink() and path.stat().st_size<90*1024*1024,'Unexpected source link/large binary: '+relative)
                require(path.suffix.lower() not in {'.asi','.exe','.dll','.lib'},'Binary in first-party source: '+relative)
                write(relative,path.read_bytes())
    for path in (ROOT/'include').glob('*.h'):write(path.relative_to(ROOT).as_posix(),path.read_bytes())
    for relative in previous_entries:
        if relative.startswith(('include/','third_party/')) and len(PurePosixPath(relative).parts)>2:
            source=ROOT/relative
            require(source.is_file() and allowed(relative),'Missing required dependency '+relative)
            write(relative,source.read_bytes())
    for name in ['.gitignore','.gitattributes','Keybinder.rc','Keybinder.sln','Keybinder.vcxproj']:
        write(name,(ROOT/name).read_bytes())
    write('README.md',(ROOT/'build/README-GITHUB-r19.md').read_bytes())
    for path in (ROOT/PACKAGE).iterdir():
        if path.is_file():write(PACKAGE+'/'+path.name,path.read_bytes())
    publication=json.loads((ROOT/'docs/review/PUBLICATION.json').read_text())
    publication.update(release=TAG,previous_release='v0.1.1-r18',previous_published_commit=PREVIOUS,
      asi_sha256=proof['asi_sha256'],history_note='Original and dependency imports remain intact. r19 is a subsequent update; no history is rewritten.',
      complete_runtime_asset=proof['archive'],complete_runtime_sha256=proof['archive_sha256'])
    write('docs/review/PUBLICATION.json',json.dumps(publication,indent=2)+'\n')
    git('add','--all')
    # Patches describe program changes and avoid recursively including their own bytes.
    write('docs/review/first-party-changes.patch',git('diff','--cached','--binary','build-dependencies-baseline','--',*PROGRAM))
    write('docs/review/DIFFSTAT.txt',git('diff','--cached','--stat','build-dependencies-baseline','--',*PROGRAM))
    write('docs/review/revision19-changes.patch',git('diff','--cached','--binary',PREVIOUS,'--',*PROGRAM))
    write('docs/review/REVISION19-DIFFSTAT.txt',git('diff','--cached','--stat',PREVIOUS,'--',*PROGRAM))
    git('add','--all')
    current_entries=paths(':')
    selected=lambda name:not name.startswith('dist/') and name not in GENERATED and not historical(name)
    before=blobs({p:o for p,o in original_entries.items() if selected(p)})
    after=blobs({p:o for p,o in current_entries.items() if selected(p)})
    changes=[]
    for name in sorted(before.keys()|after.keys()):
        b=before.get(name);a=after.get(name)
        if a==b:continue
        changes.append({'path':name,'status':'added' if b is None else 'deleted' if a is None else 'modified',
          'dependency':name.startswith(('include/plugin-sdk/','include/RakHook/','third_party/miniz/')),
          'line_endings_only':a is not None and b is not None and a.replace(b'\r\n',b'\n')==b.replace(b'\r\n',b'\n'),
          'before_sha256':digest(b) if b is not None else None,'after_sha256':digest(a) if a is not None else None})
    record={'original_archive':'NLBinder-master.zip','original_sha256':'a328e78eea63420e9bfe9c084883100177fe0fd9c2d62c4fcaef38c0a44801e9',
      'original_archive_commit_comment':'92ce21971bee2d166d830e23ad20950f1dc98921','release':TAG,'release_asi_sha256':proof['asi_sha256'],
      'baseline_tag':BASE,'baseline_commit':git('rev-parse',BASE+'^{commit}').decode().strip(),
      'hash_scope':'Exact Git blob bytes in original tag versus staged r19 source; generated comparisons and release binaries excluded.',
      'generated_paths_excluded':sorted(GENERATED),'historical_revision_patches_excluded':True,
      'baseline_files':len(before),'current_files':len(after),'changes':changes}
    write('docs/review/SOURCE-COMPARISON.json',json.dumps(record,indent=2,ensure_ascii=False)+'\n')
    relevant=[x for x in changes if not x['dependency'] and not x['line_endings_only']]
    lines=['# Dateivergleich zum Original','',
      'Verglichen werden die unveränderlichen Git-Blobs des Tags `original-archive-92ce219` mit den gestagten r19-Quellen.',
      'Die Archivherkunft bleibt dokumentiert; diese Hashes beziehen sich ausdrücklich auf Git-Blobs, nicht neu eingelesene ZIP-Rohbytes.',
      'Generierte Vergleichsdateien, alte Revisionspatches und Binärpakete sind aus diesem rekursionsfreien Vergleich ausgeschlossen.',
      'Alle betrachteten Änderungen stehen in [SOURCE-COMPARISON.json](SOURCE-COMPARISON.json).','',
      '| Status | Datei |','| --- | --- |']
    for item in relevant:lines.append('| '+item['status']+' | ['+item['path']+'](../../'+item['path']+') |')
    write('docs/review/DATEIVERGLEICH.md','\n'.join(lines)+'\n')
    git('add','--all')
    verify_index(previous_entries)
    result={'prepared':True,'previous_commit':PREVIOUS,'staged_tree':git('write-tree').decode().strip(),
      'source_changes':len(changes),'first_party_content_changes':len(relevant),'asi_sha256':proof['asi_sha256'],
      'changed_paths':len(git('diff','--cached','--name-only').decode().splitlines())}
    (ROOT/'build/revision19-publication-ready.json').write_text(json.dumps(result,indent=2)+'\n')
    (ROOT/'build/revision19-staged-owned.json').write_text(json.dumps({'staged_tree':result['staged_tree'],
        'reason':'Successfully prepared and verified r19 source tree; safe resume requires this exact tree.'},indent=2)+'\n')
    print(json.dumps(result,indent=2))

def verify_index(previous_entries=None):
    entries=paths(':')
    previous_entries=previous_entries or paths(PREVIOUS)
    for name in entries:
        require(allowed(name),'Private or generated build data in publication: '+name)
        require(not name.startswith('NLBinder/Browser/'),'Loose Chromium runtime must not enter Git')
        if historical(name):require(entries[name]==previous_entries[name],'Historical review patch changed: '+name)
    proof=json.loads((ROOT/'build/revision19-package.json').read_text())
    require(digest(git('show',':'+PACKAGE+'/NLBinder.asi'))==proof['asi_sha256'],'Published ASI differs')
    require(git('show',':docs/CHANGELOG-GESAMT.txt')==(ROOT/'docs/CHANGELOG-GESAMT.txt').read_bytes(),'Published full changelog differs')
    for name in ['include/DrugUseTracker.h','tests/DrugUseTrackerTests.cpp','src/Main.cpp',
                 'include/SettingsManager.h','src/SettingsManager.cpp','include/OnlineOverlayController.h',
                 'src/OnlineOverlayController.cpp','src/OnlineOverlayRenderer.cpp','src/OnlineUI.cpp',
                 'tests/OnlineOverlayRendererTests.cpp','tests/OnlineSettingsTests.cpp']:
        require(git('show',':'+name)==(ROOT/name).read_bytes().replace(b'\r\n',b'\n'),'Published source differs: '+name)
    # The pinned upstream miniz sources retain their original formatting/hash.
    git('diff','--cached','--check','--','.',':(exclude)*.patch',':(exclude)third_party/miniz')
    require(not git('ls-files','--others','--exclude-standard'),'Unexpected untracked publication file')

def credential():
    env=dict(os.environ,GCM_INTERACTIVE='Never',GIT_TERMINAL_PROMPT='0')
    p=subprocess.run(['git','credential','fill'],cwd=REPO,input=b'protocol=https\nhost=github.com\n\n',
        stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env)
    require(p.returncode==0,'Existing GitHub credential not available')
    fields=dict(line.split('=',1) for line in p.stdout.decode().splitlines() if '=' in line)
    require(fields.get('password'),'Credential manager returned no token')
    return fields['password']

def api(token,url,method='GET',data=None,content_type='application/json'):
    require(url.startswith(('https://api.github.com/','https://uploads.github.com/repos/Kobiro57/PerkysBinderChangelog/')),'Unexpected API host')
    body=json.dumps(data).encode() if data is not None and content_type=='application/json' else data
    req=urllib.request.Request(url,data=body,method=method,headers={'Authorization':'Bearer '+token,
      'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','User-Agent':'NLBinder-authorized-r19-publication','Content-Type':content_type})
    try:
        with urllib.request.urlopen(req,timeout=600) as response:return json.load(response)
    except urllib.error.HTTPError as e:
        raise RuntimeError('GitHub API status '+str(e.code)+' for '+url.split('?')[0]) from None

def publish():
    proof=json.loads((ROOT/'build/revision19-package.json').read_text())
    prepared=json.loads((ROOT/'build/revision19-publication-ready.json').read_text())
    token=credential()
    user=api(token,'https://api.github.com/user')
    require(user['login'].casefold()=='kobiro57','Wrong GitHub account')
    head=git('rev-parse','HEAD').decode().strip()
    if head==PREVIOUS:
        require(git('write-tree').decode().strip()==prepared['staged_tree'],'Staged source changed after review')
        verify_index()
        email=str(user['id'])+'+'+user['login']+'@users.noreply.github.com'
        git('-c','user.name='+user['login'],'-c','user.email='+email,'commit','-m',
          'r19: add own drug use cooldown and aftereffect overlay with saved move position')
        head=git('rev-parse','HEAD').decode().strip()
    require(git('rev-parse','HEAD^').decode().strip()==PREVIOUS,'Unexpected parent commit')
    require(not git('status','--porcelain'),'Publication checkout not clean')
    tag_exists=subprocess.run(['git','rev-parse','--verify','refs/tags/'+TAG],cwd=REPO,stdout=subprocess.PIPE,stderr=subprocess.PIPE).returncode==0
    if not tag_exists:git('tag',TAG,head)
    require(git('rev-parse',TAG+'^{commit}').decode().strip()==head,'Release tag points elsewhere')
    git('push','--atomic','origin','HEAD:main','refs/tags/'+TAG)
    require(api(token,API+'/commits/main')['sha']==head,'Remote main commit differs')
    releases=api(token,API+'/releases?per_page=100')
    release=next((r for r in releases if r['tag_name']==TAG),None)
    body='NLBinder v.0.1.1 by KOBiRO – r19\n\n- Neuer Schalter Online > Overlay-Einstellungen > Dauerhaft > Spielinformationen:\n  Use Cooldown & Nachwirkung anzeigen, direkt unter Ausdauer.\n- Ganze eigene Konsumzeile anhand des aktuellen lokalen Namens erkennen.\n- Sieben Drogen mit Cooldown und paralleler Abklingzeit, alle Werte in Sekunden.\n- Kokain 120/120, Heroin 120/60, Speed 180/180, Ecstasy 180/60,\n  Cannabis ohne Cooldown/40, Adrenalin 45/ohne Abklingzeit, Morphium 45/45.\n- Mehrere Drogen im selben Panel untereinander; lokale einmalige Bereit-Meldung.\n- Gleicher Stil und Maus-Move-Modus mit ESC-Speichern und eigener Position.\n- Ohne aktive Einnahme unsichtbar; im Move-Modus erscheint eine Vorschau.\n- Deaktivieren, Disconnect und Spielerwechsel setzen die laufenden Timer zurueck.\n\nNur NLBinder.asi bei vollstaendig beendetem GTA ersetzen und neu starten.\nBrowserdateien werden weiterhin automatisch unter Dokumente/GTA San Andreas\nUser Files/NLBinder/Browser eingerichtet.\n\nGesamttests, gezielte Timer-/Settings-/Overlay-/Move-Pruefungen und Release-Build\nsowie nativer Reconnect-Worker geprueft. Browser-/GPS-/Helferquellen gegen r18\nunveraendert geprueft. Historische Browser-/GPS-Pruefungen werden nicht als neu\nausgefuehrte r19-Tests dargestellt. Kein r19-GTA-Live-Test behauptet.\n\nVollstaendiger Quellcode, Gesamt-Changelog und Pruefberichte enthalten.\nOriginalimport und saemtliche fruehere Revisionspatches/Historie bleiben erhalten.\n'
    body+='\nASI SHA-256: `'+proof['asi_sha256']+'`\n\nZIP SHA-256: `'+proof['archive_sha256']+'`\n'
    if release is None:release=api(token,API+'/releases','POST',{'tag_name':TAG,'target_commitish':head,'name':'NLBinder v0.1.1 by KOBiRO – r19','body':body,'draft':False,'prerelease':False})
    archive=ROOT/'dist'/proof['archive']
    require(filehash(archive)==proof['archive_sha256'],'Release ZIP changed')
    assets=api(token,API+'/releases/'+str(release['id'])+'/assets')
    asset=next((a for a in assets if a['name']==archive.name),None)
    if asset is None:
        upload='https://uploads.github.com/repos/Kobiro57/PerkysBinderChangelog/releases/'+str(release['id'])+'/assets?name='+archive.name
        asset=api(token,upload,'POST',archive.read_bytes(),'application/zip')
    require(asset['state']=='uploaded' and asset['size']==archive.stat().st_size,'Release asset incomplete')
    remote_digest=asset.get('digest')
    require(remote_digest=='sha256:'+proof['archive_sha256'],'GitHub release asset digest differs')
    result={'published':True,'repository':URL,'commit':head,'commit_url':URL+'/commit/'+head,
      'tag':TAG,'release_url':release['html_url'],'download_url':asset['browser_download_url'],
      'asset_bytes':asset['size'],'asset_sha256_verified':True,'asi_sha256':proof['asi_sha256']}
    (ROOT/'build/revision19-published.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    modes=parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--prepare',action='store_true');modes.add_argument('--publish',action='store_true')
    args=parser.parse_args()
    if args.prepare:stage()
    else:publish()
