// Started only by /reconnect: wait for old exit, use Nova's normal play button,
// and require the new binder to confirm that its local player has spawned.
#include "../include/ReconnectManager.h"
#include "../include/NovaReconnectFlow.h"
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <UIAutomation.h>
#include <string>
#include <vector>

namespace {
struct WorkerOptions {
    DWORD parentId = 0;
    std::wstring readyEvent, mode, launcher, directory, gameExecutable, cancelEvent;
    std::vector<std::wstring> arguments;
    ULONGLONG timeout = 90000, retry = 2500;
    bool quiet = false;
};
bool Number(const std::wstring& value, unsigned long& result) {
    if (value.empty() || value.find_first_not_of(L"0123456789") != std::wstring::npos) return false;
    wchar_t* end = nullptr; result = wcstoul(value.c_str(), &end, 10);
    return end && !*end;
}
bool ParseOptions(int count, wchar_t** arguments, WorkerOptions& options) {
    int index = 1;
    while (index < count && wcslen(arguments[index]) >= 2 && arguments[index][0] == L'-' && arguments[index][1] == L'-') {
        const std::wstring key = arguments[index++];
        if (index >= count) return false;
        const std::wstring value = arguments[index++];
        if (key == L"--parent") { unsigned long id = 0; if (!Number(value,id) || !id) return false; options.parentId=id; }
        else if (key == L"--ready") options.readyEvent = value;
        else if (key == L"--mode") options.mode = value;
        else if (key == L"--launcher") options.launcher = value;
        else if (key == L"--working-directory") options.directory = value;
        else if (key == L"--game-executable") options.gameExecutable = value;
        else if (key == L"--cancel") options.cancelEvent = value;
        else if (key == L"--timeout-ms") { unsigned long v=0; if (!Number(value,v) || v<1000 || v>120000) return false; options.timeout=v; }
        else if (key == L"--retry-ms") { unsigned long v=0; if (!Number(value,v) || v<1000 || v>10000) return false; options.retry=v; }
        else if (key == L"--quiet-errors") { if(value!=L"true") return false; options.quiet=true; }
        else return false;
    }
    for (; index < count; ++index) options.arguments.emplace_back(arguments[index]);
    return options.parentId && !options.readyEvent.empty() && !options.launcher.empty()
        && (options.mode == L"nova" || options.mode == L"openmp")
        && (options.mode != L"nova" || !options.gameExecutable.empty());
}
void Trace(const std::wstring& message) {
    wchar_t path[32768]{}; const DWORD length = GetModuleFileNameW(nullptr,path,32768);
    if (!length || length>=32768) return;
    std::wstring file(path,length); const auto slash=file.find_last_of(L"\\/");
    if(slash==std::wstring::npos)return; file.resize(slash+1); file+=L"NLBinderReconnect.log";
    HANDLE log=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log==INVALID_HANDLE_VALUE)return;
    const auto text=std::to_wstring(GetTickCount64())+L" "+message+L"\r\n";
    DWORD written=0; WriteFile(log,text.data(),static_cast<DWORD>(text.size()*sizeof(wchar_t)),&written,nullptr); CloseHandle(log);
}
void NotifyFailure(const WorkerOptions& options,const wchar_t* text) {
    Trace(text); if(!options.quiet)MessageBoxW(nullptr,text,L"NL-BINDER – Reconnect",MB_OK|MB_ICONINFORMATION);
}
std::wstring FullPath(const std::wstring& input) {
    wchar_t path[32768]{}; const DWORD length=GetFullPathNameW(input.c_str(),32768,path,nullptr);
    std::wstring result=length && length<32768?std::wstring(path,length):input;
    for(auto& ch:result)if(ch==L'/')ch=L'\\'; return result;
}
bool ProcessMatches(DWORD id,const std::wstring& executable) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,id);
    if(!process)return false;
    wchar_t path[32768]{}; DWORD length=32768;
    const bool result=WaitForSingleObject(process,0)==WAIT_TIMEOUT
        &&QueryFullProcessImageNameW(process,0,path,&length)&&_wcsicmp(FullPath(std::wstring(path,length)).c_str(),executable.c_str())==0;
    CloseHandle(process); return result;
}
DWORD FindGame(const WorkerOptions& options) {
    const auto executable=FullPath(options.gameExecutable); const auto slash=executable.find_last_of(L"\\/");
    const auto name=executable.substr(slash==std::wstring::npos?0:slash+1);
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W item{}; item.dwSize=sizeof(item); DWORD found=0;
    if(Process32FirstW(snapshot,&item))do {
        if(item.th32ProcessID!=options.parentId&&_wcsicmp(item.szExeFile,name.c_str())==0&&ProcessMatches(item.th32ProcessID,executable)){found=item.th32ProcessID;break;}
    }while(Process32NextW(snapshot,&item));
    CloseHandle(snapshot); return found;
}
bool Launch(const WorkerOptions& options) {
    std::wstring command=keybinder::ReconnectCommandLine(options.launcher,options.arguments);
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    if(CreateProcessW(options.launcher.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
        options.directory.empty()?nullptr:options.directory.c_str(),&startup,&process)) {
        CloseHandle(process.hThread); CloseHandle(process.hProcess); return true;
    }
    if(GetLastError()!=ERROR_ELEVATION_REQUIRED)return false;
    std::wstring parameters;
    for(const auto& argument:options.arguments){if(!parameters.empty())parameters+=L' ';parameters+=keybinder::QuoteReconnectArgument(argument);}
    SHELLEXECUTEINFOW shell{}; shell.cbSize=sizeof(shell); shell.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;
    shell.lpVerb=L"runas"; shell.lpFile=options.launcher.c_str(); shell.lpParameters=parameters.c_str();
    shell.lpDirectory=options.directory.empty()?nullptr:options.directory.c_str(); shell.nShow=SW_SHOWNORMAL;
    const bool result=ShellExecuteExW(&shell)!=FALSE;
    if(shell.hProcess)CloseHandle(shell.hProcess); return result;
}
struct WindowSearch { const std::wstring* executable; std::vector<HWND> windows; };
BOOL CALLBACK FindNovaWindow(HWND window,LPARAM data) {
    auto& search=*reinterpret_cast<WindowSearch*>(data);
    if(GetWindow(window,GW_OWNER)||GetWindowTextLengthW(window)==0||!IsWindowVisible(window))return TRUE;
    DWORD processId=0; GetWindowThreadProcessId(window,&processId);
    if(ProcessMatches(processId,*search.executable))search.windows.push_back(window);
    return TRUE;
}
// Own only the uniquely identified enabled play button. Never activate kill,
// authentication, update, permission or anti-cheat controls.
IUIAutomationElement* FindPlayButton(IUIAutomation* automation,const std::vector<HWND>& windows) {
    VARIANT type; VariantInit(&type); type.vt=VT_I4; type.lVal=UIA_ButtonControlTypeId;
    IUIAutomationCondition* condition=nullptr;
    if(FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId,type,&condition))||!condition)return nullptr;
    IUIAutomationElement* selected=nullptr; bool ambiguous=false;
    for(HWND window:windows) {
        if(IsIconic(window))ShowWindow(window,SW_RESTORE);
        IUIAutomationElement* root=nullptr;
        if(FAILED(automation->ElementFromHandle(window,&root))||!root)continue;
        IUIAutomationElementArray* buttons=nullptr; root->FindAll(TreeScope_Descendants,condition,&buttons); root->Release();
        int count=0; if(buttons)buttons->get_Length(&count);
        for(int i=0;i<count&&i<4096;++i) {
            IUIAutomationElement* button=nullptr; buttons->GetElement(i,&button); if(!button)continue;
            BSTR name=nullptr; BOOL enabled=FALSE;
            button->get_CurrentName(&name); button->get_CurrentIsEnabled(&enabled);
            const bool match=enabled&&name&&keybinder::NormalizeNovaPlayLabel(name)==L"spiel starten";
            if(name)SysFreeString(name);
            if(match) { if(selected)ambiguous=true; else {selected=button;button=nullptr;} }
            if(button)button->Release();
        }
        if(buttons)buttons->Release();
    }
    condition->Release();
    if(ambiguous&&selected){selected->Release();selected=nullptr;} return selected;
}
bool InvokePlay(IUIAutomationElement* button) {
    IUIAutomationInvokePattern* invoke=nullptr;
    if(SUCCEEDED(button->GetCurrentPatternAs(UIA_InvokePatternId,IID_PPV_ARGS(&invoke)))&&invoke) {
        const bool result=SUCCEEDED(invoke->Invoke()); invoke->Release(); if(result)return true;
    }
    IUIAutomationLegacyIAccessiblePattern* legacy=nullptr;
    if(SUCCEEDED(button->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId,IID_PPV_ARGS(&legacy)))&&legacy) {
        const bool result=SUCCEEDED(legacy->DoDefaultAction()); legacy->Release(); return result;
    }
    return false;
}
bool StartNovaGame(const WorkerOptions& options) {
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); if(FAILED(initialized))return false;
    IUIAutomation* automation=nullptr;
    if(FAILED(CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))||!automation){CoUninitialize();return false;}
    IUIAutomation2* timed=nullptr;
    if(SUCCEEDED(automation->QueryInterface(IID_PPV_ARGS(&timed)))&&timed){timed->put_ConnectionTimeout(2000);timed->put_TransactionTimeout(2000);timed->Release();}
    const auto executable=FullPath(options.launcher);
    keybinder::NovaReconnectFlow flow(GetTickCount64(),options.timeout,options.retry);
    DWORD awaitedPid=0; HANDLE entered=nullptr; bool success=false;
    for(;;) {
        const DWORD game=FindGame(options);
        if(game!=awaitedPid) {
            if(entered){CloseHandle(entered);entered=nullptr;} awaitedPid=game;
            if(game) {
                const auto name=keybinder::ReconnectEnteredEventName(FullPath(options.gameExecutable),game);
                entered=CreateEventW(nullptr,TRUE,FALSE,name.c_str());
                if(entered&&GetLastError()==ERROR_ALREADY_EXISTS)ResetEvent(entered);
                Trace(L"game-detected-pid="+std::to_wstring(game));
            }
        }
        const bool confirmed=entered&&WaitForSingleObject(entered,0)==WAIT_OBJECT_0;
        IUIAutomationElement* play=nullptr;
        if(!game) {
            WindowSearch search{&executable}; EnumWindows(FindNovaWindow,reinterpret_cast<LPARAM>(&search));
            play=FindPlayButton(automation,search.windows);
        }
        const auto action=flow.Update(GetTickCount64(),game!=0,confirmed,play!=nullptr);
        if(action==keybinder::NovaReconnectFlow::Action::Complete){success=true;Trace(L"game-entered-confirmed");}
        else if(action==keybinder::NovaReconnectFlow::Action::InvokePlay) {
            if(!FindGame(options))Trace(std::wstring(InvokePlay(play)?L"play-invoked-attempt=":L"play-invoke-failed-attempt=")+std::to_wstring(flow.Attempts()));
        }
        if(play)play->Release();
        if(success||action==keybinder::NovaReconnectFlow::Action::TimedOut) {
            if(!success)Trace(game?L"timeout-game-not-entered":L"timeout-no-game"); break;
        }
        Sleep(250);
    }
    if(entered)CloseHandle(entered); automation->Release(); CoUninitialize(); return success;
}
} // namespace

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int count=0; wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    WorkerOptions options; const bool valid=arguments&&ParseOptions(count,arguments,options);
    if(arguments)LocalFree(arguments); if(!valid)return 2;
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,options.parentId);
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,options.readyEvent.c_str());
    HANDLE cancel=options.cancelEvent.empty()?nullptr:OpenEventW(SYNCHRONIZE,FALSE,options.cancelEvent.c_str());
    const DWORD attributes=GetFileAttributesW(options.launcher.c_str());
    if(!parent||!ready||(!options.cancelEvent.empty()&&!cancel)||attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY)) {
        if(parent)CloseHandle(parent); if(ready)CloseHandle(ready); if(cancel)CloseHandle(cancel); return 3;
    }
    if(options.mode==L"nova"&&FindGame(options)) {
        Trace(L"another-game-already-running; readiness refused");
        CloseHandle(parent); CloseHandle(ready); if(cancel)CloseHandle(cancel); return 7;
    }
    Trace(L"waiting-for-parent-exit"); SetEvent(ready); CloseHandle(ready);
    HANDLE waitHandles[]={parent,cancel};
    const DWORD wait=cancel?WaitForMultipleObjects(2,waitHandles,FALSE,60000):WaitForSingleObject(parent,60000);
    const bool cancelled=cancel&&WaitForSingleObject(cancel,0)==WAIT_OBJECT_0;
    CloseHandle(parent); if(cancel)CloseHandle(cancel);
    if(cancelled||wait==WAIT_OBJECT_0+1){Trace(L"cancelled-before-launch");return 8;}
    if(wait!=WAIT_OBJECT_0)return 4;
    const auto launcher=FullPath(options.launcher); WindowSearch existing{&launcher};
    if(options.mode==L"nova")EnumWindows(FindNovaWindow,reinterpret_cast<LPARAM>(&existing));
    if(existing.windows.empty()&&!Launch(options)) {
        NotifyFailure(options,L"Der ausgewählte Launcher konnte nicht gestartet werden. Bitte Launcher-Pfad und Windows-Freigabe prüfen."); return 5;
    }
    Trace(existing.windows.empty()?L"launcher-started":L"existing-launcher-reused");
    if(options.mode==L"nova"&&!StartNovaGame(options)) {
        NotifyFailure(options,L"Nova-Reconnect konnte den Spielbeitritt innerhalb von 90 Sekunden nicht bestätigen. Bitte Anmeldung, Launcher-Meldungen oder den Spielstart prüfen. Ein laufendes Spiel wird nicht erneut gestartet oder beendet."); return 6;
    }
    return 0;
}
