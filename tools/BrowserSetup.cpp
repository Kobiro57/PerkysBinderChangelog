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
void Touch(){if(progress)InterlockedExchange(reinterpret_cast<volatile LONG*>(&progress->heartbeat),static_cast<LONG>(GetTickCount()));}
void CancelPoint(){Touch();if(progress && InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&progress->cancel),0,0))throw std::runtime_error("Installation abgebrochen.");}
void Phase(cfg::Phase phase,int percent=0){Touch();if(progress){
    int overall=percent;
    if(phase==cfg::Phase::Checking)overall=percent/20;
    if(phase==cfg::Phase::Copying)overall=5+percent*90/100;
    if(phase==cfg::Phase::Downloading)overall=5+percent*55/100;
    if(phase==cfg::Phase::Extracting)overall=60+percent*35/100;
    if(phase==cfg::Phase::Committing)overall=99;
    if(phase==cfg::Phase::Ready)overall=100;else overall=std::min(overall,99);
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&progress->percent),overall);InterlockedExchange(reinterpret_cast<volatile LONG*>(&progress->phase),static_cast<LONG>(phase));}}
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
struct FileStamp { std::uint64_t bytes=0, modified=0; };
struct Receipt {
    std::uint32_t magic=0x4E4C4252, version=1;
    char archiveSha[64]{};
    std::array<FileStamp,std::size(cfg::kRuntimeFiles)> files{};
};
bool Stamp(const fs::path& path,FileStamp& stamp){
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info)
        ||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return false;
    stamp.bytes=(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    stamp.modified=(static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
    return true;
}
bool ReceiptMatches(const fs::path& root){
    NativeHandle file{CreateFileW((root/L".verified-runtime").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE)return false;
    Receipt receipt;DWORD size=0;LARGE_INTEGER bytes{};
    if(!GetFileSizeEx(file.value,&bytes)||bytes.QuadPart!=sizeof(receipt)
        ||!ReadFile(file.value,&receipt,sizeof(receipt),&size,nullptr)||size!=sizeof(receipt)
        ||receipt.magic!=0x4E4C4252||receipt.version!=1||memcmp(receipt.archiveSha,cfg::kArchiveSha256,64))return false;
    for(std::size_t i=0;i<std::size(cfg::kRuntimeFiles);++i){CancelPoint();FileStamp actual;
        if(!Stamp(root/Wide(cfg::kRuntimeFiles[i].name),actual)||actual.bytes!=cfg::kRuntimeFiles[i].bytes
            ||actual.bytes!=receipt.files[i].bytes||actual.modified!=receipt.files[i].modified)return false;}
    return true;
}
void SaveReceipt(const fs::path& root){
    Receipt receipt;memcpy(receipt.archiveSha,cfg::kArchiveSha256,64);
    for(std::size_t i=0;i<std::size(cfg::kRuntimeFiles);++i)
        Check(Stamp(root/Wide(cfg::kRuntimeFiles[i].name),receipt.files[i])&&receipt.files[i].bytes==cfg::kRuntimeFiles[i].bytes,"Browserdatei fehlt nach der Installation.");
    NativeHandle file{CreateFileW((root/L".verified-runtime").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    DWORD written=0;Check(file.value!=INVALID_HANDLE_VALUE&&WriteFile(file.value,&receipt,sizeof(receipt),&written,nullptr)
        &&written==sizeof(receipt)&&FlushFileBuffers(file.value),"Browserpruefung konnte nicht gespeichert werden.");
}
bool ValidRuntime(const fs::path& root,bool cached=false){
    if(!PlainDirectory(root))return false;
    if(cached&&ReceiptMatches(root))return true;
    std::size_t index=0;
    for(const auto& entry:cfg::kRuntimeFiles){
        CancelPoint();const fs::path file=root/Wide(entry.name);std::error_code ec;
        if(!fs::is_regular_file(file,ec)||GetFileAttributesW(file.c_str())&FILE_ATTRIBUTE_REPARSE_POINT
            ||fs::file_size(file,ec)!=entry.bytes||ec||Hash(file)!=entry.sha256)return false;
        Phase(cfg::Phase::Checking,static_cast<int>(++index*100/std::size(cfg::kRuntimeFiles)));
    }
    if(cached)SaveReceipt(root);
    return true;
}
bool AllowedHost(const std::wstring& host){return host==L"github.com"||host==L"release-assets.githubusercontent.com"||host==L"objects.githubusercontent.com";}
std::wstring Header(HINTERNET request,DWORD query){DWORD bytes=0;WinHttpQueryHeaders(request,query,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&bytes,WINHTTP_NO_HEADER_INDEX);if(!bytes)return {};std::wstring value(bytes/sizeof(wchar_t),L'\0');Check(WinHttpQueryHeaders(request,query,WINHTTP_HEADER_NAME_BY_INDEX,value.data(),&bytes,WINHTTP_NO_HEADER_INDEX)!=FALSE,"HTTP-Antwort ungueltig.");value.resize(bytes/sizeof(wchar_t));while(!value.empty()&&value.back()==L'\0')value.pop_back();return value;}
void Download(const fs::path& archive){
    Phase(cfg::Phase::Downloading);
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
            if(progress)InterlockedExchange(reinterpret_cast<volatile LONG*>(&progress->downloadedKiB),static_cast<LONG>(total/1024));
            Phase(cfg::Phase::Downloading,static_cast<int>(total*100/cfg::kArchiveBytes));}
        Check(total==cfg::kArchiveBytes&&hash.Finish()==cfg::kArchiveSha256,"Browserdownload stimmt nicht mit der geprueften Version ueberein.");return;
    }
    throw std::runtime_error("Zu viele Downloadweiterleitungen.");
}
bool SafeEntry(const std::string& name){
    if(name.empty()||name.front()=='/'||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos)return false;
    std::size_t begin=0;while(begin<name.size()){auto end=name.find('/',begin);if(end==std::string::npos)end=name.size();const auto part=name.substr(begin,end-begin);if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;begin=end+1;}return name.back()!='/';
}
struct ExtractSink { HANDLE file; std::uint64_t written=0; Sha256 hash; bool failed=false; };
size_t ExtractWrite(void* opaque,mz_uint64 offset,const void* data,size_t size){
    auto& sink=*static_cast<ExtractSink*>(opaque);DWORD written=0;
    Touch();
    if((progress&&progress->cancel)||offset!=sink.written||size>MAXDWORD||!WriteFile(sink.file,data,static_cast<DWORD>(size),&written,nullptr)||written!=size)return 0;
    try{sink.hash.Add(data,written);}catch(...){sink.failed=true;return 0;}
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
            Check(!sink.failed&&sink.hash.Finish()==found->second->sha256,"Entpackte Browserdatei stimmt nicht mit der geprueften Version ueberein.");
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
    Phase(cfg::Phase::Checking);if(ValidRuntime(final,true)){Phase(cfg::Phase::Ready,100);return;}
    GUID guid{};Check(SUCCEEDED(CoCreateGuid(&guid)),"Installation konnte nicht vorbereitet werden.");wchar_t value[64]{};StringFromGUID2(guid,value,64);
    const fs::path stage=root/(std::wstring(L".setup-")+value);CheckParent(stage,root);Check(CreateDirectoryW(stage.c_str(),nullptr)!=FALSE,"Installationsordner konnte nicht angelegt werden.");
    try{
        const fs::path files=stage/L"Files";fs::create_directory(files);bool copied=false;
        if(!source.empty()&&ValidRuntime(source)){
            copied=true;int i=0;
            for(const auto& e:cfg::kRuntimeFiles){CancelPoint();const auto name=Wide(e.name);fs::create_directories((files/name).parent_path());
                Check(CopyFileW((source/name).c_str(),(files/name).c_str(),TRUE)!=FALSE,"Vorhandener Browser konnte nicht uebernommen werden.");Phase(cfg::Phase::Copying,++i*100/static_cast<int>(std::size(cfg::kRuntimeFiles)));}
        }
        if(!copied){
            const fs::path cache=root/L"Cache";fs::create_directories(cache);Check(PlainDirectory(cache),"Browsercache ist nicht verfuegbar.");
            const fs::path cached=cache/L"cef156-r1.zip";
            const fs::path archive=localArchive.empty()?cached:localArchive;
            bool verified=false;
            if(fs::exists(archive)&&fs::file_size(archive)==cfg::kArchiveBytes)verified=Hash(archive)==cfg::kArchiveSha256;
            if(!verified&&localArchive.empty()){
                // Never activate partial downloads. Keep a completed verified ZIP
                // across cancellation so extraction can be retried without a download.
                if(fs::exists(cached))fs::remove(cached);
                Download(cached);verified=true;
            }
            Check(verified,"Browserarchiv hat eine falsche Pruefsumme.");
            Phase(cfg::Phase::Extracting);Extract(archive,files);}
        // Extract already hashes every byte while writing. Only migration
        // needs a destination pass; do not read the entire runtime a third time.
        if(copied)Check(ValidRuntime(files),"Die Browserinstallation ist unvollstaendig.");
        SaveReceipt(files);CancelPoint();Phase(cfg::Phase::Committing);
        if(fs::exists(final)){const fs::path old=root/(std::wstring(cfg::kRuntimeId)+L".previous-"+value);CheckParent(old,root);CheckParent(final,root);fs::rename(final,old);}
        CheckParent(final,root);fs::rename(files,final);
        // Ready is published immediately after atomic activation. Deleting
        // temporary files must never delay the game opening the browser.
        Phase(cfg::Phase::Ready,100);
        CheckParent(stage,root);std::error_code cleanup;fs::remove_all(stage,cleanup);
        if(localArchive.empty())fs::remove(root/L"Cache/cef156-r1.zip",cleanup);
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
        const bool existingMutex=GetLastError()==ERROR_ALREADY_EXISTS;
        Check(mutex.value!=nullptr,"Browserinstallation konnte nicht gesperrt werden.");
        if(existingMutex){const auto wait=WaitForSingleObject(mutex.value,1500);Check(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED,"Eine andere Browserinstallation ist noch aktiv.");}
        // Bounded CLI-only fault injection for the real native integration
        // harness; these options are never passed by a production ASI.
        if(options.count(L"--test-delay-start"))Sleep(std::min(15000UL,wcstoul(options[L"--test-delay-start"].c_str(),nullptr,10)));
        Install(root,options[L"--source"],options[L"--archive"]);
        if(options.count(L"--test-delay-exit"))Sleep(std::min(15000UL,wcstoul(options[L"--test-delay-exit"].c_str(),nullptr,10)));
    }catch(const std::exception& error){
        result=1;if(progress){strncpy_s(progress->error,error.what(),_TRUNCATE);Phase(progress->cancel?cfg::Phase::Cancelled:cfg::Phase::Failed);}
        OutputDebugStringA(error.what());
    }
    if(progress){UnmapViewOfFile(progress);progress=nullptr;}
    return result;
}
