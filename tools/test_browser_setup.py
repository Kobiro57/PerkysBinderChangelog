"""Exercise the native installer with an actual public download and private fixtures."""
from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import hashlib,json,shutil,subprocess,time,uuid

R=Path(__file__).resolve().parents[1]
LOCK=json.loads((R/'res/browser-runtime.json').read_text())
BASE=R/'build/browser-setup-tests'/uuid.uuid4().hex
BASE.mkdir(parents=True)
kernel=c.WinDLL('kernel32',use_last_error=True)
kernel.CreateFileMappingW.argtypes=[w.HANDLE,c.c_void_p,w.DWORD,w.DWORD,w.DWORD,w.LPCWSTR];kernel.CreateFileMappingW.restype=w.HANDLE
kernel.MapViewOfFile.argtypes=[w.HANDLE,w.DWORD,w.DWORD,w.DWORD,c.c_size_t];kernel.MapViewOfFile.restype=c.c_void_p
kernel.UnmapViewOfFile.argtypes=[c.c_void_p];kernel.CloseHandle.argtypes=[w.HANDLE]
class Progress(c.Structure):
    _fields_=[('magic',c.c_uint32),('bytes',c.c_uint32),('phase',c.c_int32),('percent',c.c_int32),('cancel',c.c_int32),('heartbeat',c.c_int32),('downloadedKiB',c.c_int32),('error',c.c_char*256)]
assert c.sizeof(Progress)==284
checks=0;cases=[]
def check(ok,text):
    global checks
    checks+=1
    if not ok:raise AssertionError(text)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(label,root,extra=(),success=True,cancel=False):
    name='Local\\NLBinder.BrowserSetup.'+str(subprocess.os.getpid())+'.'+uuid.uuid4().hex
    mapping=kernel.CreateFileMappingW(w.HANDLE(-1),None,4,0,c.sizeof(Progress),name)
    check(bool(mapping),'Create installer progress mapping')
    address=kernel.MapViewOfFile(mapping,0xf001f,0,0,c.sizeof(Progress));check(bool(address),'Map installer progress')
    p=Progress.from_address(address);p.magic=0x4e4c4253;p.bytes=c.sizeof(Progress);p.phase=1;p.cancel=int(cancel)
    process=None;phases=set();started=time.monotonic()
    print('Checking native browser setup:',label,flush=True)
    try:
        process=subprocess.Popen([str(R/'build/BrowserSetup.exe'),'--root',str(root),'--progress',name,*map(str,extra)],cwd=R,creationflags=subprocess.CREATE_NO_WINDOW)
        while process.poll() is None and time.monotonic()-started<240:
            phases.add(p.phase);time.sleep(.03)
        if process.poll() is None:process.kill();raise AssertionError('Native installer timeout: '+label)
        error=bytes(p.error).split(b'\0')[0].decode('utf-8','replace')
        check((process.returncode==0)==success,label+': '+error)
        check(p.phase==(5 if success else 7 if cancel else 6),label+': wrong terminal phase '+str(p.phase))
        check(not list(root.glob('.setup-*')),label+': leftover staging directory')
        if success:
            for entry in LOCK['files']:
                file=root/LOCK['runtime_id']/entry['path']
                check(file.is_file() and file.stat().st_size==entry['bytes'] and sha(file)==entry['sha256'],label+': '+entry['path'])
        else:check(bool(error),label+': missing local failure reason')
        cases.append({'case':label,'success_expected':success,'exit_code':process.returncode,'phases':sorted(phases),'seconds':round(time.monotonic()-started,2)})
        return phases
    finally:
        if process and process.poll() is None:process.kill();process.wait()
        del p;kernel.UnmapViewOfFile(address);kernel.CloseHandle(mapping)

subprocess.run([str(R/'build/BrowserSetupUnitTests.exe')],cwd=R,check=True)
try:
    root=BASE/'Frischer Browser mit Umlaut ä'/ 'Browser'
    phases=run('real pinned HTTPS download',root)
    check(3 in phases and 4 in phases,'Real download and extraction must be exercised')
    runtime=root/LOCK['runtime_id'];stamp=(runtime/'NLBinderBrowser.exe').stat().st_mtime_ns
    phases=run('already installed without a download',root,['--archive',BASE/'does-not-exist.zip'])
    check(3 not in phases and (runtime/'NLBinderBrowser.exe').stat().st_mtime_ns==stamp,'Existing good files must be reused')
    check((runtime/'.verified-runtime').is_file(),'Cached verification receipt missing')
    file=runtime/'CEF-README.txt';data=file.read_bytes();file.write_bytes(bytes([data[0]^1])+data[1:])
    run('repair same-size damaged file',root,['--archive',R/'dist'/LOCK['asset']])
    check(bool(list(root.glob(LOCK['runtime_id']+'.previous-*'))),'Damaged old runtime retained as backup')
    migration=BASE/'migration'/'Browser'
    phases=run('migrate supplied runtime without download',migration,['--source',runtime])
    check(2 in phases and 3 not in phases,'Verified local source must be copied without network')
    invalid=BASE/'invalid.zip';invalid.write_bytes(b'invalid')
    rejected=BASE/'bad-archive'/'Browser';run('reject incorrect archive',rejected,['--archive',invalid],success=False)
    check(not (rejected/LOCK['runtime_id']).exists(),'Bad archive must not become an active runtime')
    cancelled=BASE/'cancelled'/'Browser';run('cancel before installation',cancelled,['--archive',R/'dist'/LOCK['asset']],success=False,cancel=True)
    check(not (cancelled/LOCK['runtime_id']).exists(),'Cancelled setup must not become active')
    result={'revision':'v0.1.1-r18','checks':checks,'failures':0,'real_public_https_download':True,'archive_sha256':LOCK['archive_sha256'],
        'native_installer':True,'unicode_and_spaces_in_paths':True,'all_runtime_files_verified':len(LOCK['files']),
        'cases':cases,'personal_documents_modified':False,'login_or_post_submitted':False}
    (R/'build/revision18-setup-tests.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)
finally:
    # Only the uniquely generated fixture directory inside this build root.
    assert BASE.resolve().parent==(R/'build/browser-setup-tests').resolve()
    shutil.rmtree(BASE)
