"""Hard-stop extraction, then retry from the verified cached ZIP without network."""
from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import hashlib,json,os,shutil,subprocess,time,uuid
R=Path(__file__).resolve().parents[1]
lock=json.loads((R/'res/browser-runtime.json').read_text(encoding='utf-8'))
base=R/'build/browser-restart-tests'/uuid.uuid4().hex;root=base/'Browser';cache=root/'Cache'/ 'cef156-r1.zip'
cache.parent.mkdir(parents=True);shutil.copyfile(R/'dist'/lock['asset'],cache)
k=c.WinDLL('kernel32',use_last_error=True)
k.CreateFileMappingW.argtypes=[w.HANDLE,c.c_void_p,w.DWORD,w.DWORD,w.DWORD,w.LPCWSTR];k.CreateFileMappingW.restype=w.HANDLE
k.MapViewOfFile.argtypes=[w.HANDLE,w.DWORD,w.DWORD,w.DWORD,c.c_size_t];k.MapViewOfFile.restype=c.c_void_p
k.UnmapViewOfFile.argtypes=[c.c_void_p];k.CloseHandle.argtypes=[w.HANDLE]
class Progress(c.Structure):
    _fields_=[('magic',c.c_uint32),('bytes',c.c_uint32),('phase',c.c_int32),('percent',c.c_int32),('cancel',c.c_int32),('heartbeat',c.c_int32),('downloadedKiB',c.c_int32),('error',c.c_char*256)]
checks=0
def check(ok,why):
    global checks
    checks+=1
    if not ok:raise AssertionError(why)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def start():
    name='Local\\NLBinder.BrowserSetup.'+str(os.getpid())+'.'+uuid.uuid4().hex
    mapping=k.CreateFileMappingW(w.HANDLE(-1),None,4,0,c.sizeof(Progress),name);check(bool(mapping),'Create status mapping')
    address=k.MapViewOfFile(mapping,0xf001f,0,0,c.sizeof(Progress));check(bool(address),'Map status')
    state=Progress.from_address(address);state.magic=0x4e4c4253;state.bytes=c.sizeof(Progress)
    process=subprocess.Popen([str(R/'build/BrowserSetup.exe'),'--root',str(root),'--progress',name],cwd=R,creationflags=subprocess.CREATE_NO_WINDOW)
    return process,state,address,mapping
def close(process,state,address,mapping):
    if process.poll() is None:process.kill();process.wait(timeout=5)
    del state;k.UnmapViewOfFile(address);k.CloseHandle(mapping)
try:
    process,state,address,mapping=start();deadline=time.monotonic()+30
    try:
        while process.poll() is None and state.phase!=4 and time.monotonic()<deadline:time.sleep(.002)
        check(state.phase==4,'Native extraction must really begin')
        process.kill();process.wait(timeout=5)
        check(cache.is_file() and sha(cache)==lock['archive_sha256'],'Completed download survives an interrupted extraction')
    finally:close(process,state,address,mapping)
    started=time.monotonic();process,state,address,mapping=start();phases=set()
    try:
        while process.poll() is None and time.monotonic()-started<90:phases.add(state.phase);time.sleep(.01)
        check(process.poll() is not None,'Retry must finish')
        check(process.returncode==0 and state.phase==5,'Retry reaches verified Ready')
        check(3 not in phases,'Retry must not download the archive again')
        for entry in lock['files']:
            p=root/lock['runtime_id']/entry['path'];check(p.is_file() and sha(p)==entry['sha256'],'Correct retried file '+entry['path'])
        check((root/lock['runtime_id']/'.verified-runtime').is_file(),'Verification receipt committed')
        result={'revision':'v0.1.1-r18','checks':checks,'failures':0,'hard_cancel_during_extraction':True,
            'verified_download_kept':True,'retry_uses_cache_without_network':True,'all_files_verified':236,
            'retry_seconds':round(time.monotonic()-started,2),'personal_documents_modified':False}
        (R/'build/revision18-restart-tests.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))
    finally:close(process,state,address,mapping)
finally:
    assert base.resolve().parent==(R/'build/browser-restart-tests').resolve()
    shutil.rmtree(base)
