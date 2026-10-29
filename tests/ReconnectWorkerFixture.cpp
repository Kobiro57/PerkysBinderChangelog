// Harmless Nova/play-button, game-entry and waiting-parent fixture. All GUI
// windows are tool windows placed off screen; no installed program is used.
#include <windows.h>
#include <shellapi.h>
#include <string>
#include "../include/ReconnectManager.h"
#include "../include/NovaReconnectFlow.h"

namespace {
std::wstring Env(const wchar_t* key) { wchar_t text[32768]{}; const auto length=GetEnvironmentVariableW(key,text,32768); return length&&length<32768?std::wstring(text,length):std::wstring(); }
unsigned EnvNumber(const wchar_t* key,unsigned fallback=0) { const auto value=Env(key); return value.empty()?fallback:static_cast<unsigned>(wcstoul(value.c_str(),nullptr,10)); }
void Log(const std::string& text) {
    const auto path=Env(L"NLBINDER_TEST_NOVA_LOG"); if(path.empty())return;
    HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return; DWORD written=0; WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr); CloseHandle(file);
}
HANDLE exitEvent=nullptr,gameProcess=nullptr;
HWND button=nullptr;
ULONGLONG started=0,launchAt=0;
unsigned clicks=0,launches=0;
bool gameMode=false,entrySent=false;
unsigned crashAfter=0;
std::wstring PlayLabel() { auto name=Env(L"NLBINDER_TEST_NOVA_LABEL"); return name.empty()?L"Spiel starten":name; }
void LaunchGame() {
    launchAt=0; const auto executable=Env(L"NLBINDER_TEST_GAME_EXE");
    std::wstring command=keybinder::ReconnectCommandLine(executable,{L"--fixture-game",launches?L"stable":L"first"});
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) {
        ++launches;gameProcess=process.hProcess;CloseHandle(process.hThread);Log("game-launched\n");SetWindowTextW(button,L"GTA beenden");
    } else Log("game-launch-failed\n");
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM word,LPARAM param) {
    if(message==WM_COMMAND&&LOWORD(word)==100) {
        wchar_t label[256]{};GetWindowTextW(button,label,256);
        if(keybinder::NormalizeNovaPlayLabel(label)!=L"spiel starten") {Log("unexpected-kill\n");return 0;}
        ++clicks;Log("play\n");
        if(clicks>=EnvNumber(L"NLBINDER_TEST_REQUIRED_CLICKS",1)) {
            const auto delay=EnvNumber(L"NLBINDER_TEST_LAUNCH_DELAY_MS");
            if(delay) {SetWindowTextW(button,L"Wird gestartet...");launchAt=GetTickCount64()+delay;}
            else LaunchGame();
        }
        return 0;
    }
    if(message==WM_TIMER) {
        if(exitEvent&&WaitForSingleObject(exitEvent,0)==WAIT_OBJECT_0){DestroyWindow(window);return 0;}
        const auto elapsed=GetTickCount64()-started;
        if(gameMode) {
            if(crashAfter&&elapsed>=crashAfter){Log("game-crashed\n");DestroyWindow(window);return 0;}
            if(!entrySent&&!EnvNumber(L"NLBINDER_TEST_NO_ENTRY")&&elapsed>=EnvNumber(L"NLBINDER_TEST_ENTRY_DELAY_MS",500)) {
                wchar_t path[32768]{};const auto length=GetModuleFileNameW(nullptr,path,32768);
                const auto name=keybinder::ReconnectEnteredEventName(std::wstring(path,length),GetCurrentProcessId());
                HANDLE entered=OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str());
                if(entered){entrySent=SetEvent(entered)!=FALSE;CloseHandle(entered);if(entrySent)Log("game-entered\n");}
            }
        } else {
            EnableWindow(button,elapsed>=EnvNumber(L"NLBINDER_TEST_BUTTON_DELAY_MS"));
            if(launchAt&&GetTickCount64()>=launchAt)LaunchGame();
            if(gameProcess&&WaitForSingleObject(gameProcess,0)==WAIT_OBJECT_0) {
                CloseHandle(gameProcess);gameProcess=nullptr;SetWindowTextW(button,PlayLabel().c_str());Log("game-gone\n");
            }
        }
        return 0;
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,word,param);
}
int RunGui(bool game,bool first) {
    gameMode=game;started=GetTickCount64();
    if(game&&first)crashAfter=EnvNumber(L"NLBINDER_TEST_FIRST_GAME_CRASH_MS");
    const auto eventName=Env(L"NLBINDER_TEST_FIXTURE_EXIT_EVENT");
    exitEvent=OpenEventW(SYNCHRONIZE,FALSE,eventName.c_str());if(!exitEvent)return 16;
    WNDCLASSW definition{};definition.lpfnWndProc=WindowProc;definition.hInstance=GetModuleHandleW(nullptr);definition.lpszClassName=L"NLBinderReconnectFixture";
    RegisterClassW(&definition);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,definition.lpszClassName,game?L"Harmless game fixture":L"Harmless Nova fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,-25000,-25000,400,150,nullptr,nullptr,definition.hInstance,nullptr);
    if(!window){CloseHandle(exitEvent);return 17;}
    if(!game) {
        button=CreateWindowW(L"BUTTON",PlayLabel().c_str(),WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,10,10,300,50,window,reinterpret_cast<HMENU>(100),definition.hInstance,nullptr);
        EnableWindow(button,EnvNumber(L"NLBINDER_TEST_BUTTON_DELAY_MS")==0);
    }
    Log(game?"game-window\n":"nova-window\n");SetTimer(window,1,50,nullptr);
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    if(gameProcess)CloseHandle(gameProcess);CloseHandle(exitEvent);return 0;
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int count=0;wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&count);if(!arguments)return 10;
    if(count==3&&std::wstring(arguments[1])==L"--fixture-parent") {
        HANDLE event=OpenEventW(SYNCHRONIZE,FALSE,arguments[2]);LocalFree(arguments);if(!event)return 11;
        const auto result=WaitForSingleObject(event,30000);CloseHandle(event);return result==WAIT_OBJECT_0?0:12;
    }
    if(count>=2&&std::wstring(arguments[1])==L"--fixture-game") {
        const bool first=count>=3&&std::wstring(arguments[2])==L"first";LocalFree(arguments);return RunGui(true,first);
    }
    if(Env(L"NLBINDER_TEST_ROLE")==L"nova") {LocalFree(arguments);return RunGui(false,false);}
    const auto destination=Env(L"NLBINDER_TEST_LAUNCH_LOG");if(destination.empty()){LocalFree(arguments);return 13;}
    std::wstring output(1,L'\xFEFF');for(int index=0;index<count;++index){output+=arguments[index];output+=L'\n';}LocalFree(arguments);
    HANDLE file=CreateFileW(destination.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return 14;
    DWORD written=0;const DWORD size=static_cast<DWORD>(output.size()*sizeof(wchar_t));const bool success=WriteFile(file,output.data(),size,&written,nullptr)&&written==size;CloseHandle(file);return success?0:15;
}
