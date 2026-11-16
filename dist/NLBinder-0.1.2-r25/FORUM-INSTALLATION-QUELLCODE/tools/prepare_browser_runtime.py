"""Assemble the real CEF runtime; binaries stay outside the source repository."""
from pathlib import Path
import hashlib
import json
import os
import shutil

R=Path(__file__).resolve().parents[1]
source=R/'build/cef-host/Release'
target=R/'build/NLBinder/Browser'
target.mkdir(parents=True,exist_ok=True)
allowed={'.dll','.pak','.dat','.bin','.json'}
files=[]
for path in sorted(source.rglob('*')):
    if not path.is_file():continue
    relative=path.relative_to(source)
    if path.name!='NLBinderBrowser.exe' and path.suffix.lower() not in allowed:continue
    destination=target/relative
    destination.parent.mkdir(parents=True,exist_ok=True)
    if destination.exists() and destination.stat().st_size==path.stat().st_size and os.path.samefile(path,destination):pass
    else:
        if destination.exists():destination.unlink()
        try:os.link(path,destination)
        except OSError:shutil.copyfile(path,destination)
    files.append(relative.as_posix())
cef=Path((R/'build/cef-root.txt').read_text(encoding='utf-8'))
shutil.copyfile(cef/'LICENSE.txt',target/'CEF-LICENSE.txt')
if (cef/'README.txt').exists():shutil.copyfile(cef/'README.txt',target/'CEF-README.txt')
lock=json.loads((R/'tools/cef-runtime.lock.json').read_text(encoding='utf-8'))
record={'cef_version':lock['version'],'architecture':'x64','sandbox':True,'files':[]}
for path in sorted(target.rglob('*')):
    if path.is_file():record['files'].append({'path':path.relative_to(target).as_posix(),'bytes':path.stat().st_size,'sha256':hashlib.file_digest(path.open('rb'),'sha256').hexdigest()})
(R/'build/revision16-browser-runtime.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'runtime_files':len(record['files']),'runtime_bytes':sum(x['bytes'] for x in record['files'])},indent=2))
