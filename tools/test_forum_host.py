"""Real CEF offscreen integration test using a fresh anonymous test profile.

No login, posts, messages, downloads or user browser profiles are used.
"""
from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
import subprocess
import time
import uuid

ROOT=Path(__file__).resolve().parents[1]
MAX_W,MAX_H,COMMANDS=1920,1080,1024
class Command(C.Structure):
    _fields_=[('type',W.DWORD),('a',C.c_int32),('b',C.c_int32),('c',C.c_int32),('d',C.c_int32),('e',C.c_int32),('modifiers',W.DWORD)]
class Shared(C.Structure):
    _fields_=[('magic',W.DWORD),('version',W.DWORD),('bytes',W.DWORD),('reserved',W.DWORD),
      ('commandWrite',C.c_int32),('commandRead',C.c_int32),('frameLock',C.c_int32),('frameSequence',C.c_int32),
      ('state',C.c_int32),('width',C.c_int32),('height',C.c_int32),('canGoBack',C.c_int32),('canGoForward',C.c_int32),('loading',C.c_int32),
      ('errorCode',C.c_int32),('blockedLink',C.c_int32),('parentWindow',C.c_uint64),('screenX',C.c_int32),('screenY',C.c_int32),
      ('title',C.c_char*512),('url',C.c_char*2048),('commands',Command*COMMANDS),('pixels',C.c_ubyte*(MAX_W*MAX_H*4))]

def main():
    assert C.sizeof(Command)==28 and Shared.parentWindow.offset==64
    kernel=C.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileMappingW.argtypes=[W.HANDLE,C.c_void_p,W.DWORD,W.DWORD,W.DWORD,W.LPCWSTR];kernel.CreateFileMappingW.restype=W.HANDLE
    kernel.MapViewOfFile.argtypes=[W.HANDLE,W.DWORD,W.DWORD,W.DWORD,C.c_size_t];kernel.MapViewOfFile.restype=C.c_void_p
    kernel.CreateEventW.argtypes=[C.c_void_p,W.BOOL,W.BOOL,W.LPCWSTR];kernel.CreateEventW.restype=W.HANDLE
    kernel.SetEvent.argtypes=[W.HANDLE];kernel.CloseHandle.argtypes=[W.HANDLE]
    kernel.UnmapViewOfFile.argtypes=[C.c_void_p]
    base=ROOT/'build/forum-smoke'/uuid.uuid4().hex
    base.mkdir(parents=True)
    name='Local\\NLBinder.Forum.smoke.'+uuid.uuid4().hex
    mapping=kernel.CreateFileMappingW(W.HANDLE(-1),None,0x04,0,C.sizeof(Shared),name)
    if not mapping:raise OSError(C.get_last_error(),'CreateFileMapping')
    pointer=kernel.MapViewOfFile(mapping,0xF001F,0,0,C.sizeof(Shared))
    event=kernel.CreateEventW(None,False,False,name+'.input')
    if not pointer or not event:raise OSError(C.get_last_error(),'mapping/event')
    state=Shared.from_address(pointer);state.magic=0x4E4C4346;state.version=1;state.bytes=C.sizeof(Shared)
    executable=ROOT/'build/cef-host/Release/NLBinderBrowser.exe'
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    process=subprocess.Popen([str(executable),'--nlbinder-map='+name,'--nlbinder-smoke-profile='+str(base/'profile')],
        cwd=executable.parent,startupinfo=startup,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    def send(kind,a=0,b=0,c=0,d=0,e=0,modifiers=0):
        position=state.commandWrite & 0xffffffff
        if (position-(state.commandRead & 0xffffffff))&0xffffffff>=COMMANDS:raise RuntimeError('input ring full')
        state.commands[position%COMMANDS]=Command(kind,a,b,c,d,e,modifiers)
        state.commandWrite=C.c_int32(position+1).value;kernel.SetEvent(event)
    def wait_for(predicate,seconds=35):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError('CEF host exited: '+str(process.returncode))
            if predicate():return
            time.sleep(.05)
        raise RuntimeError('CEF timeout: '+json.dumps({'state':state.state,'frames':state.frameSequence,'error':state.errorCode,'title':state.title.decode('utf-8','replace')}))
    def pixels():
        for _ in range(100):
            if state.frameLock==0:
                sequence=state.frameSequence
                width,height=state.width,state.height
                value=C.string_at(pointer+Shared.pixels.offset,width*height*4)
                if not state.frameLock and state.frameSequence==sequence:return width,height,value
            time.sleep(.01)
        raise RuntimeError('pixel lock unavailable')
    try:
        wait_for(lambda:state.state==1)
        send(0,1280,800)
        wait_for(lambda:state.frameSequence>2 and state.width==1280 and state.height==800
                 and not state.loading and b'Nova Legacy' in state.title,60)
        w,h,first=pixels()
        if len(set(first[::4]))<10:raise RuntimeError('Forum did not render real page content')
        before=hashlib.sha256(first).hexdigest()
        send(1,640,400);send(3,640,400,0,-720)
        time.sleep(1.0)
        _,_,after=pixels()
        scroll_changed=hashlib.sha256(after).hexdigest()!=before
        if not scroll_changed:raise RuntimeError('Real browser scroll input did not change the page')
        # Save only private diagnostic pixels, never include them in publication.
        import struct
        row=w*4
        bitmap=struct.pack('<2sIHHI',b'BM',54+len(after),0,0,54)+struct.pack('<IiiHHIIiiII',40,w,-h,1,32,0,len(after),0,0,0,0)+after
        (base/'forum-private.bmp').write_bytes(bitmap)
        result={'cef_version':json.loads((ROOT/'tools/cef-runtime.lock.json').read_text())['version'],
          'real_forum_rendered':True,'real_forum_title':state.title.decode('utf-8','replace'),
          'viewport':[w,h],'offscreen_frames':state.frameSequence,'mouse_scroll_changed_real_page':scroll_changed,
          'anonymous_isolated_profile':True,'login_or_post_submitted':False,'sandbox_bootstrap_used':True}
        send(10)
        process.wait(timeout=10)
        result['clean_exit_code']=process.returncode
        if process.returncode!=0:raise RuntimeError('Browser shutdown failed')
        (ROOT/'build/revision16-browser-smoke.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(result,indent=2))
    finally:
        if process.poll() is None:
            try:send(10);process.wait(timeout=5)
            except Exception:process.terminate();process.wait(timeout=5)
        kernel.UnmapViewOfFile(pointer);kernel.CloseHandle(event);kernel.CloseHandle(mapping)

if __name__=='__main__':main()
