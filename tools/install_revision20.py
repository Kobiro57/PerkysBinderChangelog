"""Install only the verified ASI into the explicitly named GTA folder when closed."""
from pathlib import Path
from ctypes import wintypes as w
import ctypes as c
import hashlib,json,os,shutil,uuid
R=Path(__file__).resolve().parents[1]
GAME=Path.home()/'Desktop/Grand Theft Auto (OPENMP)'
def require(ok,text):
    if not ok:raise ValueError(text)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def running_game_pids():
    k=c.WinDLL('kernel32',use_last_error=True)
    class Process(c.Structure):
        _fields_=[('size',w.DWORD),('usage',w.DWORD),('pid',w.DWORD),('heap',c.c_size_t),('module',w.DWORD),('threads',w.DWORD),('parent',w.DWORD),('priority',w.LONG),('flags',w.DWORD),('name',w.WCHAR*260)]
    k.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD];k.CreateToolhelp32Snapshot.restype=w.HANDLE
    k.Process32FirstW.argtypes=[w.HANDLE,c.POINTER(Process)];k.Process32NextW.argtypes=[w.HANDLE,c.POINTER(Process)];k.CloseHandle.argtypes=[w.HANDLE]
    handle=k.CreateToolhelp32Snapshot(2,0);require(handle and handle!=w.HANDLE(-1).value,'Cannot verify whether GTA is closed')
    entry=Process();entry.size=c.sizeof(entry);pids=[]
    try:
        require(bool(k.Process32FirstW(handle,c.byref(entry))),'Cannot read game process list')
        while True:
            if entry.name.casefold()=='gta_sa.exe':pids.append(entry.pid)
            if not k.Process32NextW(handle,c.byref(entry)):break
    finally:k.CloseHandle(handle)
    return pids
def main():
    pids=running_game_pids()
    if pids:
        print(json.dumps({'installed':False,'reason':'GTA is still running; no game file changed','processes':len(pids)}));return
    require(GAME.is_dir() and GAME.resolve()==GAME.absolute() and not GAME.is_junction(),'Unexpected GTA target')
    source=R/'build/Release/NLBinder.asi';target=GAME/'NLBinder.asi';temporary=GAME/'NLBinder.asi.r20.tmp'
    for path in [target,temporary]:require(path.resolve().parent==GAME.resolve() and not path.is_symlink(),'Redirected game file refused')
    proof=json.loads((R/'build/revision20-artifact.json').read_text(encoding='utf-8'))
    require(proof['revision']=='v0.1.2-r20' and sha(source)==proof['sha256'] and proof['embedded_browser_setup_matches'],'ASI proof does not match')
    backup=R/'build/game-asi-backups'/uuid.uuid4().hex/'NLBinder.asi'
    if target.exists():
        old=sha(target)
        require(old in {'4eec175d23397a51d4da28877b8f1109044cce8523cbcb06c7a70aa6cbf837b9',
            '638f889bb6ef5bb13c9382d7add3eba4e25bcacbec39c815379a195e844ed2b1',proof['sha256']},'Unknown installed ASI; left untouched')
        backup.parent.mkdir(parents=True);shutil.copy2(target,backup)
    require(not temporary.exists(),'A previous temporary game file exists')
    shutil.copyfile(source,temporary);require(sha(temporary)==proof['sha256'],'Copied ASI differs')
    require(not running_game_pids(),'GTA started during preparation; installation stopped')
    os.replace(temporary,target);require(sha(target)==proof['sha256'],'Installed ASI differs')
    result={'revision':'v0.1.2-r20','installed':True,'asi_sha256':proof['sha256'],'game_file':str(target),'previous_backup':str(backup),'game_was_closed':True}
    (R/'build/revision20-installed.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
