// Native, separate installer process. Only the pinned browser archive/files
// are accepted. No shell, Python, PowerShell, elevation or user profiles.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "../include/BrowserSetupProtocol.h"
#include "../include/BrowserRuntimeLock.h"
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
extern "C" {
#include "../third_party/miniz/miniz.c"
#include "../third_party/miniz/miniz_tinfl.c"
#include "../third_party/miniz/miniz_zip.c"
}

namespace fs=std::filesystem;
namespace cfg=keybinder::browser_setup;
namespace {
struct NativeHandle { HANDLE value=nullptr; ~NativeHandle(){ if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value); } };
struct HttpHandle { HINTERNET value=nullptr; ~HttpHandle(){ if(value)WinHttpCloseHandle(value); } };
cfg::SetupProgress* progress=nullptr;
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void CancelPoint(){if(progress && InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&progress->cancel),0,0))throw std::runtime_error("Installation abgebrochen.");}
void Phase(cfg::Phase phase,int percent=0){if(progress){InterlockedExchange(reinterpret_cast<volatile LONG*>(&progress->percent),percent);InterlockedExchange(reinterpret_cast<volatile LONG*>(&progress->phase),static_cast<LONG>(phase));}}
std::wstring Wide(const char* value){const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,nullptr,0);Check(size>0,"Ungueltiger Dateiname.");std::wstring out(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,out.data(),size);out.pop_back();return out;}
bool PlainDirectory(const fs::path& path){const auto a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY) && !(a&FILE_ATTRIBUTE_REPARSE_POINT);}
void CheckParent(const fs::path& path,const fs::path& root){Check(path.lexically_normal().parent_path()==root.lexically_normal(),"Ungueltiger Installationspfad.");Check(!fs::exists(path)||!(GetFileAttributesW(path.c_str())&FILE_ATTRIBUTE_REPARSE_POINT),"Umgeleitete Browserordner werden nicht ersetzt.");}
struct Sha256 {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    Sha256(){Check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA-256 nicht verfuegbar.");Check(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0,"SHA-256 konnte nicht gestartet werden.");}
    ~Sha256(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    void Add(const void* data,DWORD size){Check(BCryptHashData(hash,const_cast<PUCHAR>(static_cast<const UCHAR*>(data)),size,0)>=0,"SHA-256 fehlgeschlagen.");}
    std::string Finish(){unsigned char bytes[32]{};Check(BCryptFinishHash(hash,bytes,32,0)>=0,"SHA-256 fehlgeschlagen.");std::string text;const char* h="0123456789abcdef";for(auto b:bytes){text+=h[b>>4];text+=h[b&15];}return text;}
};
std::string Hash(const fs::path& path){
    NativeHandle file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE)return {};
    Sha256 hash;std::array<unsigned char,65536> buffer{};DWORD n=0;
    for(;;){CancelPoint();if(!ReadFile(file.value,buffer.data(),static_cast<DWORD>(buffer.size()),&n,nullptr))return {};if(!n)break;hash.Add(buffer.data(),n);}
    return hash.Finish();
}
bool ValidRuntime(const fs::path& root){
    if(!PlainDirectory(root))return false;
    std::size_t index=0;
    for(const auto& entry:cfg::kRuntimeFiles){
        CancelPoint();const fs::path file=root/Wide(entry.name);std::error_code ec;
        if(!fs::is_regular_file(file,ec)||GetFileAttributesW(file.c_str())&FILE_ATTRIBUTE_REPARSE_POINT
            ||fs::file_size(file,ec)!=entry.bytes||ec||Hash(file)!=entry.sha256)return false;
        Phase(cfg::Phase::Checking,static_cast<int>(++index*100/std::size(cfg::kRuntimeFiles)));
    }
    return true;
}
bool AllowedHost(const std::wstring& host){return host==L"github.com"||host==L"release-assets.githubusercontent.com"||host==L"objects.githubusercontent.com";}
std::wstring Header(HINTERNET request,DWORD query){DWORD bytes=0;WinHttpQueryHeaders(request,query,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&bytes,WINHTTP_NO_HEADER_INDEX);if(!bytes)return {};std::wstring value(bytes/sizeof(wchar_t),L'\0');Check(WinHttpQueryHeaders(request,query,WINHTTP_HEADER_NAME_BY_INDEX,value.data(),&bytes,WINHTTP_NO_HEADER_INDEX)!=FALSE,"HTTP-Antwort ungueltig.");value.resize(bytes/sizeof(wchar_t));while(!value.empty()&&value.back()==L'\0')value.pop_back();return value;}
void Download(const fs::path& archive){
    HttpHandle session{WinHttpOpen(L"NLBinder-BrowserSetup/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    Check(session.value!=nullptr,"Keine Internetverbindung fuer die Browserinstallation.");
    Check(WinHttpSetTimeouts(session.value,10000,10000,15000,15000)!=FALSE,"Netzwerk konnte nicht vorbereitet werden.");
    std::wstring url=cfg::kDownloadUrl;
    for(int redirect=0;redirect<6;++redirect){
        CancelPoint();URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=static_cast<DWORD>(-1);
        Check(WinHttpCrackUrl(url.c_str(),0,0,&parts)!=FALSE&&parts.nScheme==INTERNET_SCHEME_HTTPS&&parts.nPort==INTERNET_DEFAULT_HTTPS_PORT,"Nur HTTPS ist erlaubt.");
        std::wstring host(parts.lpszHostName,parts.dwHostNameLength);Check(AllowedHost(host),"Unerwartete Downloadquelle.");
        std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        HttpHandle connection{WinHttpConnect(session.value,host.c_str(),parts.nPort,0)};Check(connection.value!=nullptr,"Downloadverbindung fehlgeschlagen.");
        HttpHandle request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
        Check(request.value!=nullptr,"Download konnte nicht gestartet werden.");DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        Check(WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))!=FALSE,"Download konnte nicht abgesichert werden.");
        Check(WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)!=FALSE
          &&WinHttpReceiveResponse(request.value,nullptr)!=FALSE,"Download fehlgeschlagen. Bitte Internetverbindung pruefen und /forum erneut aufrufen.");
        DWORD status=0,size=sizeof(status);Check(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=FALSE,"HTTP-Status fehlt.");
        if(status==301||status==302||status==303||status==307||status==308){url=Header(request.value,WINHTTP_QUERY_LOCATION);Check(!url.empty(),"Downloadweiterleitung fehlt.");continue;}
        Check(status==200,"Browserdownload momentan nicht verfuegbar. Bitte /forum spaeter erneut aufrufen.");
        NativeHandle file{CreateFileW(archive.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
        Check(file.value!=INVALID_HANDLE_VALUE,"Browserdateien konnten nicht gespeichert werden.");
        Sha256 hash;std::array<unsigned char,65536> buffer{};std::uint64_t total=0;
        for(;;){CancelPoint();DWORD n=0,written=0;Check(WinHttpReadData(request.value,buffer.data(),static_cast<DWORD>(buffer.size()),&n)!=FALSE,"Browserdownload unterbrochen.");if(!n)break;
            total+=n;Check(total<=cfg::kArchiveBytes,"Browserdownload hat eine unerwartete Groesse.");
            Check(WriteFile(file.value,buffer.data(),n,&written,nullptr)!=FALSE&&written==n,"Nicht genug Speicherplatz fuer den Browser.");hash.Add(buffer.data(),n);
            Phase(cfg::Phase::Downloading,static_cast<int>(total*100/cfg::kArchiveBytes));}
        Check(total==cfg::kArchiveBytes&&hash.Finish()==cfg::kArchiveSha256,"Browserdownload stimmt nicht mit der geprueften Version ueberein.");return;
    }
    throw std::runtime_error("Zu viele Downloadweiterleitungen.");
}
bool SafeEntry(const std::string& name){
    if(name.empty()||name.front()=='/'||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos)return false;
    std::size_t begin=0;while(begin<name.size()){auto end=name.find('/',begin);if(end==std::string::npos)end=name.size();const auto part=name.substr(begin,end-begin);if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;begin=end+1;}return name.back()!='/';
}
struct ExtractSink { HANDLE file; std::uint64_t written=0; };
size_t ExtractWrite(void* opaque,mz_uint64 offset,const void* data,size_t size){
    auto& sink=*static_cast<ExtractSink*>(opaque);DWORD written=0;
    if((progress&&progress->cancel)||offset!=sink.written||size>MAXDWORD||!WriteFile(sink.file,data,static_cast<DWORD>(size),&written,nullptr)||written!=size)return 0;
    sink.written+=written;return written;
}
void Extract(const fs::path& archive,const fs::path& target){
    FILE* stream=_wfopen(archive.c_str(),L"rb");Check(stream!=nullptr,"Browserarchiv konnte nicht geoeffnet werden.");
    mz_zip_archive zip{};
    if(!mz_zip_reader_init_cfile(&zip,stream,0,0)){fclose(stream);throw std::runtime_error("Browserarchiv ist beschaedigt.");}
    try {
        Check(mz_zip_reader_get_num_files(&zip)==std::size(cfg::kRuntimeFiles),"Browserarchiv ist unvollstaendig.");
        std::map<std::string,const cfg::RuntimeFile*> expected;for(const auto& e:cfg::kRuntimeFiles)expected.emplace(e.name,&e);
        for(mz_uint i=0;i<mz_zip_reader_get_num_files(&zip);++i){
            CancelPoint();mz_zip_archive_file_stat stat{};Check(mz_zip_reader_file_stat(&zip,i,&stat)!=0,"Browserarchiv ist beschaedigt.");
            const std::string name(stat.m_filename);auto found=expected.find(name);
            Check(SafeEntry(name)&&found!=expected.end()&&!stat.m_is_directory&&!stat.m_is_encrypted
              &&(stat.m_method==0||stat.m_method==8)&&stat.m_uncomp_size==found->second->bytes,"Unerwarteter Inhalt im Browserarchiv.");
            const auto unixType=(stat.m_external_attr>>16)&0170000;
            Check(unixType==0||unixType==0100000,"Verknuepfungen im Browserarchiv sind nicht erlaubt.");
            const fs::path file=target/Wide(name.c_str());fs::create_directories(file.parent_path());
            NativeHandle output{CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
            Check(output.value!=INVALID_HANDLE_VALUE,"Browserdatei konnte nicht erstellt werden.");ExtractSink sink{output.value};
            Check(mz_zip_reader_extract_to_callback(&zip,i,ExtractWrite,&sink,0)!=0&&sink.written==found->second->bytes,"Browserdatei konnte nicht vollstaendig entpackt werden.");
            CloseHandle(output.value);output.value=nullptr;
            Check(Hash(file)==found->second->sha256,"Entpackte Browserdatei stimmt nicht mit der geprueften Version ueberein.");
            expected.erase(found);Phase(cfg::Phase::Extracting,static_cast<int>((i+1)*100/std::size(cfg::kRuntimeFiles)));
        }
        Check(expected.empty(),"Browserdateien fehlen.");
    }catch(...){mz_zip_reader_end(&zip);fclose(stream);throw;}
    mz_zip_reader_end(&zip);fclose(stream);
}
void Install(const fs::path& root,const fs::path& source,const fs::path& localArchive){
    Check(root.is_absolute()&&root.filename()==L"Browser","Ungueltiger Browserordner.");
    fs::create_directories(root);Check(PlainDirectory(root),"Der Browserordner ist nicht verfuegbar.");
    const fs::path final=root/cfg::kRuntimeId;CheckParent(final,root);
    Phase(cfg::Phase::Checking);if(ValidRuntime(final)){Phase(cfg::Phase::Ready,100);return;}
    GUID guid{};Check(SUCCEEDED(CoCreateGuid(&guid)),"Installation konnte nicht vorbereitet werden.");wchar_t value[64]{};StringFromGUID2(guid,value,64);
    const fs::path stage=root/(std::wstring(L".setup-")+value);CheckParent(stage,root);Check(CreateDirectoryW(stage.c_str(),nullptr)!=FALSE,"Installationsordner konnte nicht angelegt werden.");
    try{
        const fs::path files=stage/L"Files";fs::create_directory(files);bool copied=false;
        if(!source.empty()&&ValidRuntime(source)){
            copied=true;int i=0;
            for(const auto& e:cfg::kRuntimeFiles){CancelPoint();const auto name=Wide(e.name);fs::create_directories((files/name).parent_path());
                Check(CopyFileW((source/name).c_str(),(files/name).c_str(),TRUE)!=FALSE,"Vorhandener Browser konnte nicht uebernommen werden.");Phase(cfg::Phase::Copying,++i*100/static_cast<int>(std::size(cfg::kRuntimeFiles)));}
        }
        if(!copied){const fs::path archive=localArchive.empty()?stage/L"download.zip":localArchive;
            if(localArchive.empty())Download(archive);
            Check(fs::file_size(archive)==cfg::kArchiveBytes&&Hash(archive)==cfg::kArchiveSha256,"Browserarchiv hat eine falsche Pruefsumme.");
            Phase(cfg::Phase::Extracting);Extract(archive,files);}
        Check(ValidRuntime(files),"Die Browserinstallation ist unvollstaendig.");CancelPoint();
        if(fs::exists(final)){const fs::path old=root/(std::wstring(cfg::kRuntimeId)+L".previous-"+value);CheckParent(old,root);CheckParent(final,root);fs::rename(final,old);}
        CheckParent(final,root);fs::rename(files,final);
        CheckParent(stage,root);fs::remove_all(stage);Phase(cfg::Phase::Ready,100);
    }catch(...){CheckParent(stage,root);if(PlainDirectory(stage))fs::remove_all(stage);throw;}
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){
    int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 2;
    std::map<std::wstring,std::wstring> options;
    for(int i=1;i+1<count;i+=2)options.emplace(args[i],args[i+1]);LocalFree(args);
    NativeHandle mapping;int result=0;
    try{
        if(options.count(L"--progress")){
            const auto& name=options[L"--progress"];Check(name.rfind(L"Local\\NLBinder.BrowserSetup.",0)==0&&name.size()<180,"Ungueltige Installationskennung.");
            mapping.value=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name.c_str());Check(mapping.value!=nullptr,"Installationsstatus nicht verfuegbar.");
            progress=static_cast<cfg::SetupProgress*>(MapViewOfFile(mapping.value,FILE_MAP_ALL_ACCESS,0,0,sizeof(cfg::SetupProgress)));
            Check(progress&&progress->magic==cfg::kSetupMagic&&progress->bytes==sizeof(cfg::SetupProgress),"Ungueltiger Installationsstatus.");
        }
        Check(options.count(L"--root")!=0,"Browserordner fehlt.");
        const fs::path root=fs::absolute(options[L"--root"]).lexically_normal();
        // One installer per destination, including separate GTA instances.
        Sha256 id;const auto key=root.wstring();id.Add(key.data(),static_cast<DWORD>(key.size()*sizeof(wchar_t)));
        NativeHandle mutex{CreateMutexW(nullptr,TRUE,(L"Local\\NLBinder.BrowserInstall."+Wide(id.Finish().c_str())).c_str())};
        Check(mutex.value&&GetLastError()!=ERROR_ALREADY_EXISTS,"Eine Browserinstallation ist bereits aktiv. Bitte /forum gleich erneut aufrufen.");
        Install(root,options[L"--source"],options[L"--archive"]);
    }catch(const std::exception& error){
        result=1;if(progress){strncpy_s(progress->error,error.what(),_TRUNCATE);Phase(progress->cancel?cfg::Phase::Cancelled:cfg::Phase::Failed);}
        OutputDebugStringA(error.what());
    }
    if(progress){UnmapViewOfFile(progress);progress=nullptr;}
    return result;
}
