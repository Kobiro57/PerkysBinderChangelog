#include "../include/ForumBrowser.h"
#include "../include/ForumBrowserProtocol.h"
#include "../include/ForumAfkSession.h"
#include "../include/SettingsManager.h"
#include "../include/Logger.h"
#include "../include/UIComponents.h"
#include "../include/BrowserSetupProtocol.h"
#include "../include/BrowserRuntimeLock.h"
#include "../include/ResourceIds.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include <new>
#ifdef KEYBINDER_TESTING
#include <filesystem>
#include <fstream>
#endif
#include <d3d9.h>
#include <sddl.h>
#include <aclapi.h>
#include <shlobj.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

namespace keybinder {
namespace {
namespace ipc = forum;
LONG Read(volatile std::int32_t& value) { return InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&value), 0, 0); }
void Write(volatile std::int32_t& value, LONG updated) { InterlockedExchange(reinterpret_cast<volatile LONG*>(&value), updated); }
void CloseNative(HANDLE& handle) { if (handle) { CloseHandle(handle); handle = nullptr; } }
std::wstring BundledRuntimeDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&BundledRuntimeDirectory), &module)) return {};
    wchar_t path[32768]{};
    const auto size = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
    if (!size || size >= std::size(path)) return {};
    const auto directory = std::wstring(path).substr(0, std::wstring(path).find_last_of(L"\\/"));
    return directory + L"\\NLBinder\\Browser";
}
std::wstring BrowserRootDirectory() {
    PWSTR documents = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &documents))) return {};
    const std::wstring root = std::wstring(documents) + L"\\GTA San Andreas User Files\\NLBinder\\Browser";
    CoTaskMemFree(documents);
    if (SHCreateDirectoryExW(nullptr, root.c_str(), nullptr) != ERROR_SUCCESS
        && GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES) return {};
    return root;
}
bool WriteSetupExecutable(const std::wstring& path) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&WriteSetupExecutable), &module)) return false;
    const auto resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_BROWSER_SETUP), MAKEINTRESOURCEW(10));
    const auto size = resource ? SizeofResource(module, resource) : 0;
    const auto loaded = resource ? LoadResource(module, resource) : nullptr;
    const auto* bytes = loaded ? static_cast<const unsigned char*>(LockResource(loaded)) : nullptr;
    if (!bytes || !size) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes, size, &written, nullptr) != FALSE && written == size;
    CloseHandle(file); return ok;
}
std::wstring CurrentUserSid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD size = 0; GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> bytes(size);
    std::wstring sid;
    if (GetTokenInformation(token, TokenUser, bytes.data(), size, &size)) {
        LPWSTR value = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid, &value)) { sid = value; LocalFree(value); }
    }
    CloseHandle(token); return sid;
}
bool IsElevated() {
    HANDLE token = nullptr; TOKEN_ELEVATION value{}; DWORD size = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return true;
    const bool ok = GetTokenInformation(token, TokenElevation, &value, sizeof(value), &size) != FALSE;
    CloseHandle(token); return !ok || value.TokenIsElevated != 0;
}
bool PermitSandboxResourceReads(const std::wstring& directory) {
    // Modern CEF LPAC processes require read/execute access to their own
    // binaries/resources. Grant only this application-package SID and only
    // on the dedicated runtime folder, preserving all existing access rules.
    const auto attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    PSID packageSid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-15-2-2", &packageSid)) return false;
    PACL previous = nullptr, updated = nullptr; PSECURITY_DESCRIPTOR descriptor = nullptr;
    DWORD error = GetNamedSecurityInfoW(const_cast<wchar_t*>(directory.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, &previous, nullptr, &descriptor);
    if (error == ERROR_SUCCESS) {
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
        access.grfAccessMode = GRANT_ACCESS;
        access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        access.Trustee.ptstrName = static_cast<LPWSTR>(packageSid);
        error = SetEntriesInAclW(1, &access, previous, &updated);
        if (error == ERROR_SUCCESS) error = SetNamedSecurityInfoW(const_cast<wchar_t*>(directory.c_str()), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION, nullptr, nullptr, updated, nullptr);
    }
    if (updated) LocalFree(updated);
    if (descriptor) LocalFree(descriptor);
    LocalFree(packageSid); return error == ERROR_SUCCESS;
}
bool StartMediumProcess(const std::wstring& executable, std::wstring command, PROCESS_INFORMATION& process) {
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    const auto cwd = executable.substr(0, executable.find_last_of(L"\\/"));
    if (!IsElevated()) return CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, cwd.c_str(), &startup, &process) != FALSE;

    // Reuse the same user's normal Explorer token. Web content is never
    // launched with GTA's optional administrator privileges.
    DWORD shellPid = 0; GetWindowThreadProcessId(GetShellWindow(), &shellPid);
    HANDLE shell = shellPid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, shellPid) : nullptr;
    HANDLE token = nullptr, primary = nullptr;
    bool result = false;
    if (shell && OpenProcessToken(shell, TOKEN_QUERY | TOKEN_DUPLICATE, &token)) {
        DWORD size = 0; GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<unsigned char> info(size);
        LPWSTR sid = nullptr;
        if (GetTokenInformation(token, TokenUser, info.data(), size, &size)
            && ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(info.data())->User.Sid, &sid)) {
            const bool sameUser = CurrentUserSid() == sid; LocalFree(sid);
            if (sameUser && DuplicateTokenEx(token, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation, TokenPrimary, &primary))
                result = CreateProcessWithTokenW(primary, LOGON_WITH_PROFILE, executable.c_str(), command.data(),
                    CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, cwd.c_str(), &startup, &process) != FALSE;
        }
    }
    CloseNative(primary); CloseNative(token); CloseNative(shell);
    return result;
}
unsigned int CurrentModifiers() {
    unsigned int value = 0;
    if (GetKeyState(VK_SHIFT) < 0) value |= ipc::Shift;
    if (GetKeyState(VK_CONTROL) < 0) value |= ipc::Control;
    if (GetKeyState(VK_MENU) < 0) value |= ipc::Alt;
    if (GetKeyState(VK_LBUTTON) < 0) value |= ipc::LeftButton;
    if (GetKeyState(VK_MBUTTON) < 0) value |= ipc::MiddleButton;
    if (GetKeyState(VK_RBUTTON) < 0) value |= ipc::RightButton;
    if (GetKeyState(VK_CAPITAL) & 1) value |= ipc::CapsLock;
    if (GetKeyState(VK_NUMLOCK) & 1) value |= ipc::NumLock;
    return value;
}
} // namespace

struct ForumBrowser::Impl {
    ForumAfkSession afkSession;
    struct VisibilityGuard {
        Impl* owner;
        ~VisibilityGuard() { owner->afkSession.SetVisible(owner->open); }
    };
    SettingsManager* settings = nullptr;
    std::function<void(const std::string&)> notice;
    std::function<void()> persist;
    HANDLE mapping = nullptr, input = nullptr, process = nullptr, job = nullptr;
    ipc::SharedState* shared = nullptr;
    IDirect3DTexture9* texture = nullptr;
    std::mutex commandsMutex;
    bool open = false, stopping = false, reopen = false, focused = true, geometryDirty = false;
    bool mouseInside = false, pressed[3]{};
    bool firstFrame = true;
    int textureWidth = 0, textureHeight = 0, lastSequence = -1;
    int requestedWidth = 0, requestedHeight = 0, mouseX = 0, mouseY = 0;
    ULONGLONG startedAt = 0, stopAt = 0;
    ULONGLONG exitSeenAt = 0;
    bool hardStopRequested = false;
    std::vector<unsigned char> frame;
    HANDLE setupMapping = nullptr, setupProcess = nullptr, setupJob = nullptr;
    browser_setup::SetupProgress* setup = nullptr;
    std::wstring setupExecutable;
    bool runtimeChecked = false;
    ULONGLONG setupCancelAt = 0;
#ifdef KEYBINDER_TESTING
    std::wstring testProfile;
    std::wstring testRuntimeRoot, testArchive;
    unsigned long testSetupStartDelay = 0, testSetupExitDelay = 0;
    bool testReadyBeforeSetupExit = false;
#endif

    std::wstring RuntimeRoot() const {
#ifdef KEYBINDER_TESTING
        if (!testRuntimeRoot.empty()) return testRuntimeRoot;
#endif
        return BrowserRootDirectory();
    }
    std::wstring RuntimeDirectory() const { return RuntimeRoot() + L"\\" + browser_setup::kRuntimeId; }
    void ReleaseSetup() {
        if (setup) { UnmapViewOfFile(setup); setup = nullptr; }
        CloseNative(setupProcess); CloseNative(setupMapping); CloseNative(setupJob);
        if (!setupExecutable.empty()) DeleteFileW(setupExecutable.c_str());
        setupExecutable.clear(); setupCancelAt = 0;
    }
    bool BeginSetup() {
        const auto root = RuntimeRoot();
        if (root.empty()) { Say("Der Browserordner unter Dokumente konnte nicht erstellt werden."); return false; }
        if (SHCreateDirectoryExW(nullptr, root.c_str(), nullptr) != ERROR_SUCCESS
            && GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        GUID guid{}; if (FAILED(CoCreateGuid(&guid))) return false;
        wchar_t id[64]{}; StringFromGUID2(guid, id, static_cast<int>(std::size(id)));
        const std::wstring name = L"Local\\NLBinder.BrowserSetup." + std::to_wstring(GetCurrentProcessId()) + id;
        setupExecutable = root + L"\\BrowserSetup-" + std::to_wstring(GetCurrentProcessId()) + id + L".exe";
        if (!WriteSetupExecutable(setupExecutable)) { Say("Die Browserinstallation konnte nicht vorbereitet werden."); ReleaseSetup(); return false; }
        const auto sid = CurrentUserSid();
        PSECURITY_DESCRIPTOR security = nullptr;
        const auto sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;" + sid + L")S:(ML;;NW;;;ME)";
        if (sid.empty() || !ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &security, nullptr)) { ReleaseSetup(); return false; }
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), security, FALSE};
        setupMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &attributes, PAGE_READWRITE, 0,
            sizeof(browser_setup::SetupProgress), name.c_str());
        LocalFree(security);
        setup = setupMapping ? static_cast<browser_setup::SetupProgress*>(MapViewOfFile(setupMapping, FILE_MAP_ALL_ACCESS, 0, 0,
            sizeof(browser_setup::SetupProgress))) : nullptr;
        if (!setup) { ReleaseSetup(); return false; }
        new (setup) browser_setup::SetupProgress();
        Write(setup->heartbeat, static_cast<LONG>(GetTickCount()));
        setupJob = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!setupJob || !SetInformationJobObject(setupJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { ReleaseSetup(); return false; }
        std::wstring command = L"\"" + setupExecutable + L"\" --root \"" + root + L"\" --progress \"" + name + L"\"";
#ifdef KEYBINDER_TESTING
        if (!testArchive.empty()) command += L" --archive \"" + testArchive + L"\"";
        else
#endif
        command += L" --source \"" + BundledRuntimeDirectory() + L"\"";
#ifdef KEYBINDER_TESTING
        if (testSetupStartDelay) command += L" --test-delay-start " + std::to_wstring(testSetupStartDelay);
        if (testSetupExitDelay) command += L" --test-delay-exit " + std::to_wstring(testSetupExitDelay);
#endif
        PROCESS_INFORMATION child{};
        if (!StartMediumProcess(setupExecutable, command, child)) { ReleaseSetup(); Say("Die Browserinstallation konnte nicht gestartet werden."); return false; }
        if (!AssignProcessToJobObject(setupJob, child.hProcess)) {
            TerminateProcess(child.hProcess, 1); CloseHandle(child.hThread); CloseHandle(child.hProcess); ReleaseSetup(); return false;
        }
        setupProcess = child.hProcess; ResumeThread(child.hThread); CloseHandle(child.hThread);
        Logger::Info("[ForumBrowser] Browser setup started (pid %lu)", child.dwProcessId);
        open = true; firstFrame = true; setupCancelAt = 0;
        return true;
    }
    void UpdateSetup() {
        if (!setupProcess) return;
        const auto phase = setup ? static_cast<browser_setup::Phase>(Read(setup->phase)) : browser_setup::Phase::Failed;
        // The verified version is already atomically active at Ready. A
        // helper cleaning temporary files (or a delayed process exit) must
        // never leave the UI sitting at 100 percent.
        if (phase == browser_setup::Phase::Ready) {
#ifdef KEYBINDER_TESTING
            testReadyBeforeSetupExit = WaitForSingleObject(setupProcess, 0) == WAIT_TIMEOUT;
#endif
            const bool wanted = open;
            Logger::Info("[ForumBrowser] Setup ready; handing off to Chromium");
            ReleaseSetup(); open = false; runtimeChecked = true;
            if (wanted) Start();
            return;
        }
        const auto wait = WaitForSingleObject(setupProcess, 0);
        const bool terminal = phase == browser_setup::Phase::Failed || phase == browser_setup::Phase::Cancelled;
        if (wait == WAIT_TIMEOUT && !terminal) {
            const auto heartbeat = setup ? static_cast<DWORD>(Read(setup->heartbeat)) : GetTickCount();
            if (GetTickCount() - heartbeat <= 60000) return;
            Logger::Warning("[ForumBrowser] Setup stopped reporting progress");
            ReleaseSetup(); open = false;
            Say("Die Browserinstallation antwortet nicht. Mit /forum kannst du sie erneut starten."); return;
        }
        DWORD exitCode = 1; GetExitCodeProcess(setupProcess, &exitCode);
        const bool wanted = open;
        const bool ready = !exitCode && setup && Read(setup->phase) == static_cast<LONG>(browser_setup::Phase::Ready);
        const std::string error = setup ? std::string(setup->error, strnlen(setup->error, sizeof(setup->error))) : std::string();
        ReleaseSetup(); open = false;
        if (wanted && ready) { runtimeChecked = true; Start(); }
        else if (wanted) Say(error.empty() ? "Browserinstallation fehlgeschlagen. Mit /forum erneut versuchen." : error);
    }

    void Say(const std::string& text) { if (notice) notice("{33CCFF}[NL-BINDER]{FFFFFF} " + text); }
    void ReleaseTexture() {
        if (texture) { texture->Release(); texture = nullptr; }
        textureWidth = textureHeight = 0; lastSequence = -1;
    }
    void Release() {
        ReleaseTexture();
        if (shared) { UnmapViewOfFile(shared); shared = nullptr; }
        CloseNative(process); CloseNative(input); CloseNative(mapping); CloseNative(job);
        std::vector<unsigned char>().swap(frame);
        stopping = false; requestedWidth = requestedHeight = 0;
        exitSeenAt = 0; hardStopRequested = false;
    }
    bool Send(ipc::Command command) {
        std::lock_guard<std::mutex> lock(commandsMutex);
        if (!shared || !input) return false;
        const auto write = static_cast<std::uint32_t>(Read(shared->commandWrite));
        const auto read = static_cast<std::uint32_t>(Read(shared->commandRead));
        if (write - read >= ipc::kCommandCount) return false;
        shared->commands[write % ipc::kCommandCount] = command;
        MemoryBarrier(); Write(shared->commandWrite, static_cast<LONG>(write + 1)); SetEvent(input);
        return true;
    }
    bool Start() {
        if (process || setupProcess) return false;
        if (!runtimeChecked) return BeginSetup();
        const auto directory = RuntimeDirectory();
        const auto executable = directory + L"\\NLBinderBrowser.exe";
        if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES
            || GetFileAttributesW((directory + L"\\NLBinderBrowser.dll").c_str()) == INVALID_FILE_ATTRIBUTES
            || GetFileAttributesW((directory + L"\\libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES) {
            runtimeChecked = false;
            Say("Browserdateien fehlen. Mit /forum wird die Installation erneut gepr\xC3\xBC" "ft und repariert."); return false;
        }
        if (!PermitSandboxResourceReads(directory)) {
            Say("Die CEF-Ressourcen konnten nicht f\xC3\xBC" "r den gesch\xC3\xBC" "tzten Browserprozess freigegeben werden."); return false;
        }
        GUID guid{}; if (FAILED(CoCreateGuid(&guid))) return false;
        wchar_t text[64]{}; StringFromGUID2(guid, text, static_cast<int>(std::size(text)));
        const std::wstring name = L"Local\\NLBinder.Forum." + std::to_wstring(GetCurrentProcessId()) + text;
        const auto sid = CurrentUserSid();
        PSECURITY_DESCRIPTOR security = nullptr;
        const auto sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;" + sid + L")S:(ML;;NW;;;ME)";
        if (sid.empty() || !ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &security, nullptr)) return false;
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), security, FALSE};
        mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &attributes, PAGE_READWRITE, 0, sizeof(ipc::SharedState), name.c_str());
        input = CreateEventW(&attributes, FALSE, FALSE, (name + L".input").c_str());
        LocalFree(security);
        shared = mapping ? static_cast<ipc::SharedState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ipc::SharedState))) : nullptr;
        if (!shared || !input) { Release(); return false; }
        shared->magic = ipc::kMagic; shared->version = ipc::kVersion; shared->bytes = sizeof(ipc::SharedState);
        shared->parentWindow = reinterpret_cast<std::uintptr_t>(GetActiveWindow());
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { Release(); return false; }
        PROCESS_INFORMATION child{};
        std::wstring command = L"\"" + executable + L"\" --nlbinder-map=\"" + name + L"\"";
#ifdef KEYBINDER_TESTING
        if (!testProfile.empty()) command += L" --nlbinder-smoke-profile=\"" + testProfile + L"\"";
#endif
        if (!StartMediumProcess(executable, command, child)) { Release(); Say("Der gesch\xC3\xBC" "tzte Browserprozess konnte nicht gestartet werden."); return false; }
        if (!AssignProcessToJobObject(job, child.hProcess)) {
            TerminateProcess(child.hProcess, 1); CloseHandle(child.hThread); CloseHandle(child.hProcess); Release(); return false;
        }
        process = child.hProcess; ResumeThread(child.hThread); CloseHandle(child.hThread);
        startedAt = GetTickCount64(); stopping = false; open = true; focused = true;
        firstFrame = true; mouseInside = false; std::fill(std::begin(pressed), std::end(pressed), false);
        Logger::Info("[ForumBrowser] Isolated sandboxed browser host started");
        return true;
    }
    void Upload(IDirect3DDevice9* device) {
        if (!shared || !device || device->TestCooperativeLevel() != D3D_OK || Read(shared->frameSequence) == lastSequence) return;
        if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&shared->frameLock), 1, 0) != 0) return;
        const int width = Read(shared->width), height = Read(shared->height);
        const int sequence = Read(shared->frameSequence);
        if (width < 1 || height < 1 || width > ipc::kMaximumWidth || height > ipc::kMaximumHeight) { Write(shared->frameLock, 0); return; }
        try { frame.resize(static_cast<std::size_t>(width) * height * 4); }
        catch (const std::bad_alloc&) { Write(shared->frameLock, 0); return; }
        std::memcpy(frame.data(), shared->pixels, frame.size()); Write(shared->frameLock, 0);
        if (!texture || textureWidth != width || textureHeight != height) {
            ReleaseTexture();
            if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                    D3DPOOL_DEFAULT, &texture, nullptr))) return;
            textureWidth = width; textureHeight = height;
        }
        D3DLOCKED_RECT lock{};
        if (SUCCEEDED(texture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) {
            for (int row = 0; row < height; ++row) std::memcpy(static_cast<unsigned char*>(lock.pBits) + row * lock.Pitch,
                frame.data() + static_cast<std::size_t>(row) * width * 4, width * 4);
            texture->UnlockRect(0); lastSequence = sequence;
        }
    }
};

ForumBrowser::ForumBrowser() : impl_(std::make_unique<Impl>()) {}
ForumBrowser::~ForumBrowser() {
    // Process-detach may run under the Windows loader lock. Normal game
    // shutdown uses Shutdown(); emergency teardown only closes kernel objects.
    if (impl_) { impl_->texture = nullptr; impl_->Release(); impl_->ReleaseSetup(); }
}
void ForumBrowser::Initialize(SettingsManager* settings, std::function<void(const std::string&)> notice, std::function<void()> persist,
    std::function<bool(const std::string&)> sessionCommand) {
    impl_->settings = settings; impl_->notice = std::move(notice); impl_->persist = std::move(persist);
    impl_->afkSession.SetSender(std::move(sessionCommand));
}
bool ForumBrowser::IsOpen() const { return impl_->open; }
void ForumBrowser::Toggle() {
    Impl::VisibilityGuard visibility{impl_.get()};
    Update();
    if (impl_->open) { Close(); return; }
    if (impl_->setupProcess) impl_->ReleaseSetup(); // Recover any hidden, abandoned setup state.
    if (impl_->stopping) { impl_->reopen = true; impl_->open = true; return; }
    try { impl_->Start(); }
    catch (const std::bad_alloc&) { impl_->open = false; impl_->Release(); impl_->Say("Nicht genug Speicher zum Starten des Forums."); }
}
void ForumBrowser::Close() {
    Impl::VisibilityGuard visibility{impl_.get()};
    if (!impl_->open) return;
    if (impl_->stopping) { impl_->reopen = false; impl_->open = false; return; }
    if (impl_->setup) {
        Write(impl_->setup->cancel, 1);
        // A synchronous network/file read may not observe a cancel flag
        // promptly. Stop only our own installer job and clear the UI state now.
        if (impl_->setupJob) TerminateJobObject(impl_->setupJob, ERROR_CANCELLED);
        if (impl_->setupProcess) WaitForSingleObject(impl_->setupProcess, 100);
        impl_->ReleaseSetup(); impl_->open = false; impl_->reopen = false;
        Logger::Info("[ForumBrowser] Setup cancelled; next /forum may start immediately"); return;
    }
    impl_->open = false; impl_->focused = false;
    if (impl_->geometryDirty && impl_->persist) impl_->persist();
    impl_->geometryDirty = false;
    impl_->Send({ipc::CommandType::Close});
    impl_->stopAt = GetTickCount64(); impl_->stopping = true;
    impl_->ReleaseTexture();
}
void ForumBrowser::Shutdown() {
    if (!impl_) return;
    Impl::VisibilityGuard visibility{impl_.get()};
    if (impl_->setup) Write(impl_->setup->cancel, 1);
    if (impl_->setupProcess) WaitForSingleObject(impl_->setupProcess, 100);
    impl_->ReleaseSetup();
    if (impl_->open && impl_->geometryDirty && impl_->persist) impl_->persist();
    if (impl_->shared) impl_->Send({ipc::CommandType::Close});
    impl_->open = false; impl_->reopen = false;
    if (impl_->process) WaitForSingleObject(impl_->process, 1500);
    impl_->Release(); // Job handle also contains children after an emergency exit.
}
void ForumBrowser::OnDeviceLost() { impl_->ReleaseTexture(); }
void ForumBrowser::Update() {
    Impl::VisibilityGuard visibility{impl_.get()};
    impl_->UpdateSetup();
    if (!impl_->process) return;
    const bool exited = WaitForSingleObject(impl_->process, 0) == WAIT_OBJECT_0;
    if (exited) {
        if (!impl_->exitSeenAt) impl_->exitSeenAt = GetTickCount64();
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        const bool childrenGone = impl_->job && QueryInformationJobObject(impl_->job,
            JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr)
            && accounting.ActiveProcesses == 0;
        if (!childrenGone) {
            // A CEF child may still hold the profile lock after the main host
            // exits. Wait for the owned job to drain before reopening it.
            if (GetTickCount64() - impl_->exitSeenAt > 2000 && !impl_->hardStopRequested) {
                TerminateJobObject(impl_->job, 1); impl_->hardStopRequested = true;
            }
            return;
        }
        DWORD code = 0; GetExitCodeProcess(impl_->process, &code);
        const bool unexpected = impl_->open && !impl_->stopping;
        impl_->open = false; impl_->Release();
        if (unexpected) { Logger::Warning("[ForumBrowser] Host exited (%lu)", code); impl_->Say("Der Browser wurde beendet. Mit /forum kannst du ihn neu starten."); }
        if (impl_->reopen) { impl_->reopen = false; impl_->Start(); }
    } else if (impl_->stopping && GetTickCount64() - impl_->stopAt > 10000 && !impl_->hardStopRequested) {
        TerminateJobObject(impl_->job, 1); impl_->hardStopRequested = true;
    } else if (!impl_->stopping && impl_->shared && Read(impl_->shared->state) == static_cast<LONG>(ipc::State::Starting)
        && GetTickCount64() - impl_->startedAt > 30000) {
        impl_->Say("Der Browser antwortet nicht. Bitte /forum erneut versuchen."); Close();
    }
}
bool ForumBrowser::HandleWindowMessage(HWND, UINT message, WPARAM key, LPARAM native) {
    if (impl_->setupProcess || impl_->stopping) return false;
    if (!impl_->open || !impl_->focused || key == VK_ESCAPE) return false;
    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN && message != WM_KEYUP
        && message != WM_SYSKEYUP && message != WM_CHAR && message != WM_SYSCHAR) return false;
    if ((message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && key == VK_F4) return false;
    ipc::Command command{ipc::CommandType::Key};
    command.a = static_cast<int>((message == WM_CHAR || message == WM_SYSCHAR) ? ipc::KeyType::Character
        : ((message == WM_KEYUP || message == WM_SYSKEYUP) ? ipc::KeyType::Up : ipc::KeyType::Down));
    command.b = static_cast<int>(key); command.c = static_cast<int>(native);
    command.d = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SYSCHAR;
    command.modifiers = CurrentModifiers();
    if (native & (1u << 30)) command.modifiers |= ipc::Repeat;
    impl_->Send(command); return true;
}

void ForumBrowser::Render(IDirect3DDevice9* device) {
    // Poll even with a hidden menu/closed window. SA-MP can replace the
    // ordinary game-processing call; the actual rendered overlay is reliable.
    Update();
    if (!impl_->open || !impl_->settings) return;
    if (impl_->stopping) {
        ImGui::SetNextWindowSize(ImVec2(500, 160), ImGuiCond_Appearing);
        bool visible = true;
        if (ImGui::Begin("NLBinder \xE2\x80\x93 Nova Legacy Forum", &visible, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextWrapped("Forum wird neu ge\xC3\xB6" "ffnet ...");
            if (UIComponents::SecondaryButton("Abbrechen")) visible = false;
        }
        ImGui::End(); if (!visible) Close(); return;
    }
    if (impl_->setup) {
        ImGui::SetNextWindowSize(ImVec2(540, 210), ImGuiCond_Appearing);
        bool visible = true;
        if (ImGui::Begin("NLBinder \xE2\x80\x93 Nova Legacy Forum", &visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
            const auto phase = static_cast<browser_setup::Phase>(Read(impl_->setup->phase));
            const char* label = "Browserdateien werden gepr\xC3\xBC" "ft ...";
            if (phase == browser_setup::Phase::Copying) label = "Vorhandener Browser wird nach Dokumente \xC3\xBC" "bernommen ...";
            if (phase == browser_setup::Phase::Downloading) label = "Browser wird einmalig heruntergeladen ...";
            if (phase == browser_setup::Phase::Extracting) label = "Browser wird entpackt und eingerichtet ...";
            if (phase == browser_setup::Phase::Committing) label = "Browser wird fertiggestellt ...";
            UIComponents::CardHeader("Forum vorbereiten", label);
            ImGui::ProgressBar(std::clamp(Read(impl_->setup->percent), 0L, 99L) / 100.0f, ImVec2(-1, 0));
            if (phase == browser_setup::Phase::Downloading)
                ImGui::Text("%.1f / %.1f MB", Read(impl_->setup->downloadedKiB) / 1024.0f, browser_setup::kArchiveBytes / 1048576.0f);
            ImGui::TextWrapped("Die Einrichtung erfolgt im NLBinder-Ordner unter Dokumente. Danach \xC3\xB6" "ffnet sich das Forum automatisch.");
            if (UIComponents::SecondaryButton("Abbrechen")) visible = false;
        }
        ImGui::End(); if (!visible) Close(); return;
    }
    auto& settings = impl_->settings->GetMutable();
    const auto* viewport = ImGui::GetMainViewport();
    if (!viewport) return;
    if (impl_->firstFrame) {
        const float width = std::clamp(settings.forum_window_width, 480.0f, (std::max)(480.0f, viewport->WorkSize.x));
        const float height = std::clamp(settings.forum_window_height, 360.0f, (std::max)(360.0f, viewport->WorkSize.y));
        const float x = settings.forum_window_x < 0 ? viewport->WorkPos.x + (viewport->WorkSize.x - width) * 0.5f
            : std::clamp(settings.forum_window_x, viewport->WorkPos.x, viewport->WorkPos.x + (std::max)(0.0f, viewport->WorkSize.x - width));
        const float y = settings.forum_window_y < 0 ? viewport->WorkPos.y + (viewport->WorkSize.y - height) * 0.5f
            : std::clamp(settings.forum_window_y, viewport->WorkPos.y, viewport->WorkPos.y + (std::max)(0.0f, viewport->WorkSize.y - height));
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        impl_->firstFrame = false;
    }
    if (settings.forum_window_fullscreen) {
        ImGui::SetNextWindowPos(viewport->WorkPos); ImGui::SetNextWindowSize(viewport->WorkSize);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(480, 360), ImVec2((std::max)(480.0f, viewport->WorkSize.x), (std::max)(360.0f, viewport->WorkSize.y)));
    bool visible = true, closeRequested = false;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse
        | (settings.forum_window_fullscreen ? ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize : 0);
    if (ImGui::Begin("NLBinder \xE2\x80\x93 Nova Legacy Forum", &visible, flags)) {
        if (!settings.forum_window_fullscreen) {
            const auto pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
            if (settings.forum_window_x != pos.x || settings.forum_window_y != pos.y
                || settings.forum_window_width != size.x || settings.forum_window_height != size.y) impl_->geometryDirty = true;
            settings.forum_window_x = pos.x; settings.forum_window_y = pos.y;
            settings.forum_window_width = size.x; settings.forum_window_height = size.y;
        }
        const auto send = [&](ipc::CommandType type) { impl_->Send({type}); };
        const bool back = impl_->shared && Read(impl_->shared->canGoBack), forward = impl_->shared && Read(impl_->shared->canGoForward);
        if (UIComponents::SecondaryButton("Zur\xC3\xBC" "ck", ImVec2(0, 0), !back)) send(ipc::CommandType::Back);
        ImGui::SameLine(); if (UIComponents::SecondaryButton("Vorw\xC3\xA4rts", ImVec2(0, 0), !forward)) send(ipc::CommandType::Forward);
        ImGui::SameLine(); if (UIComponents::SecondaryButton("Neu laden")) send(ipc::CommandType::Reload);
        ImGui::SameLine(); if (UIComponents::SecondaryButton("Startseite")) send(ipc::CommandType::Home);
        ImGui::SameLine(); if (UIComponents::SecondaryButton(settings.forum_window_fullscreen ? "Fenster" : "Vollbild")) {
            settings.forum_window_fullscreen = !settings.forum_window_fullscreen;
            impl_->firstFrame = !settings.forum_window_fullscreen; impl_->geometryDirty = true;
        }
        ImGui::SameLine(); if (UIComponents::SecondaryButton("Schlie\xC3\x9F" "en")) closeRequested = true;
        ImGui::Separator();
        ImVec2 size = ImGui::GetContentRegionAvail(); size.y = (std::max)(1.0f, size.y - ImGui::GetTextLineHeightWithSpacing());
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const int width = std::clamp(static_cast<int>(size.x), 320, ipc::kMaximumWidth);
        const int height = std::clamp(static_cast<int>(size.y), 240, ipc::kMaximumHeight);
        if (impl_->shared && Read(impl_->shared->state) == static_cast<LONG>(ipc::State::Ready)
            && (width != impl_->requestedWidth || height != impl_->requestedHeight)) {
            impl_->Send({ipc::CommandType::Resize, width, height}); impl_->requestedWidth = width; impl_->requestedHeight = height;
        }
        if (impl_->shared) {
            const HWND window = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(impl_->shared->parentWindow));
            POINT point{}; ClientToScreen(window, &point);
            Write(impl_->shared->screenX, point.x + static_cast<int>(origin.x)); Write(impl_->shared->screenY, point.y + static_cast<int>(origin.y));
        }
        impl_->Upload(device);
        if (impl_->texture) ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(impl_->texture)), size);
        else {
            ImGui::InvisibleButton("##ForumViewport", size);
            ImGui::GetWindowDrawList()->AddText(ImVec2(origin.x + 20, origin.y + 20), ImGui::GetColorU32(ImGuiCol_Text), "Forum wird geladen ...");
        }
        const bool inside = ImGui::IsItemHovered();
        auto& io = ImGui::GetIO();
        const int x = std::clamp(static_cast<int>((io.MousePos.x - origin.x) * width / (std::max)(1.0f, size.x)), 0, width - 1);
        const int y = std::clamp(static_cast<int>((io.MousePos.y - origin.y) * height / (std::max)(1.0f, size.y)), 0, height - 1);
        if (inside || impl_->mouseInside) {
            ipc::Command move{ipc::CommandType::MouseMove, x, y, inside ? 0 : 1}; move.modifiers = CurrentModifiers(); impl_->Send(move);
        }
        impl_->mouseInside = inside;
        for (int button = 0; button < 3; ++button) {
            const bool down = inside && ImGui::IsMouseClicked(button);
            const bool up = impl_->pressed[button] && ImGui::IsMouseReleased(button);
            if (down || up) {
                ipc::Command click{ipc::CommandType::MouseButton, x, y, button, up ? 1 : 0, ImGui::IsMouseDoubleClicked(button) ? 2 : 1};
                click.modifiers = CurrentModifiers(); impl_->Send(click); impl_->pressed[button] = down;
                if (down) { impl_->focused = true; impl_->Send({ipc::CommandType::Focus, 1}); }
            }
        }
        if (inside && (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)) {
            ipc::Command wheel{ipc::CommandType::MouseWheel, x, y, static_cast<int>(io.MouseWheelH * 120), static_cast<int>(io.MouseWheel * 120)};
            wheel.modifiers = CurrentModifiers(); impl_->Send(wheel);
        }
        if (inside && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) ImGui::OpenPopup("##ForumEdit");
        if (ImGui::BeginPopup("##ForumEdit")) {
            if (ImGui::MenuItem("Kopieren", "Strg+C")) send(ipc::CommandType::Copy);
            if (ImGui::MenuItem("Einf\xC3\xBCgen", "Strg+V")) send(ipc::CommandType::Paste);
            if (ImGui::MenuItem("Alles markieren", "Strg+A")) send(ipc::CommandType::SelectAll);
            ImGui::EndPopup();
        }
        if (impl_->shared && Read(impl_->shared->blockedLink)) ImGui::TextUnformatted("Externe Seiten, Pop-ups au\xC3\x9F" "erhalb von Nova und Downloads sind gesperrt.");
        else if (impl_->shared && Read(impl_->shared->errorCode)) ImGui::TextUnformatted("Ladefehler. Bitte Verbindung pr\xC3\xBC" "fen und Neu laden verwenden.");
        else ImGui::TextUnformatted("nova-legacy.de/forum | ESC schlie\xC3\x9F" "t das Forum");
    }
    ImGui::End();
    if (!visible || closeRequested) Close();
}
#ifdef KEYBINDER_TESTING
int RunNativeForumSmokeTest() {
    const auto root = std::filesystem::path("build/native-forum-smoke") / std::to_string(GetTickCount64());
    std::filesystem::create_directories(root);
    ForumBrowser browser;
    std::vector<std::string> notices;
    std::vector<std::string> sessionCommands;
    browser.Initialize(nullptr, [&](const auto& text) { notices.push_back(text); }, {},
        [&](const std::string& command) { sessionCommands.push_back(command); return true; });
    browser.impl_->testProfile = std::filesystem::absolute(root / "profile").wstring();
    browser.impl_->testRuntimeRoot = std::filesystem::absolute(root / "Browser").wstring();
    browser.impl_->testArchive = std::filesystem::absolute("dist/NLBinder-Browser-cef156-r1.zip").wstring();
    browser.impl_->testSetupStartDelay = 15000;
    browser.Toggle();
    const auto cancelStarted = GetTickCount64();
    browser.Close();
    const bool cancellationReset = !browser.impl_->setupProcess && !browser.IsOpen() && GetTickCount64() - cancelStarted < 1000;
    browser.impl_->testSetupStartDelay = 0;
    browser.impl_->testSetupExitDelay = 15000;
    int frames = 0;
    int successfulCycles = 0;
    auto awaitReady = [&]() {
        const ULONGLONG deadline = GetTickCount64() + 60000;
        while (browser.IsOpen() && GetTickCount64() < deadline) {
            browser.Render(nullptr); // Deliberately no gameProcess/Update call.
            auto* shared = browser.impl_->shared;
            if (!browser.impl_->stopping && shared && Read(shared->state) == static_cast<LONG>(ipc::State::Ready)
                && Read(shared->frameSequence) > 2 && !Read(shared->loading)
                && InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&shared->frameLock), 1, 0) == 0) {
                const std::string title(shared->title, strnlen(shared->title, sizeof(shared->title)));
                frames += Read(shared->frameSequence);
                const bool ready = title.find("Nova Legacy") != std::string::npos;
                Write(shared->frameLock, 0);
                if (ready) { ++successfulCycles; return true; }
            }
            Sleep(50);
        }
        return false;
    };
    auto awaitStopped = [&]() {
        const auto deadline = GetTickCount64() + 16000;
        while ((browser.impl_->process || browser.impl_->setupProcess) && GetTickCount64() < deadline) { browser.Render(nullptr); Sleep(25); }
        return !browser.impl_->process && !browser.impl_->setupProcess && !browser.IsOpen();
    };
    browser.Toggle();
    bool ready = awaitReady();
    browser.Close();
    ready = awaitStopped() && ready;
    if (ready) { browser.Toggle(); ready = awaitReady(); } // Same profile, second normal open.
    if (ready) { browser.Close(); browser.Toggle(); ready = awaitReady(); } // Queue during shutdown.
    if (ready) {
        browser.Close(); ready = awaitStopped();
        if (ready) { browser.Toggle(); browser.Close(); browser.Toggle(); ready = awaitReady(); } // Close before CEF is ready.
    }
    browser.Close();
    const bool closed = awaitStopped();
    bool commandsPaired = sessionCommands.size() >= 8 && sessionCommands.size() % 2 == 0;
    for (std::size_t i=0; i<sessionCommands.size(); ++i)
        commandsPaired = commandsPaired && sessionCommands[i] == (i % 2 ? "/back" : "/afk");
    std::ofstream proof("build/revision18-native-browser-smoke.json");
    proof << "{\n  \"native_x86_controller_launched_x64_cef\": " << (ready ? "true" : "false")
        << ",\n  \"real_forum_rendered\": " << (ready ? "true" : "false")
        << ",\n  \"offscreen_frames\": " << frames
        << ",\n  \"clean_async_shutdown\": " << (closed ? "true" : "false")
        << ",\n  \"same_profile_open_cycles\": " << successfulCycles
        << ",\n  \"automatic_setup_from_embedded_installer\": " << (browser.impl_->runtimeChecked ? "true" : "false")
        << ",\n  \"queued_and_early_reopen_checked\": " << (successfulCycles == 4 ? "true" : "false")
        << ",\n  \"render_only_polling_without_game_process\": true"
        << ",\n  \"ready_handoff_before_delayed_installer_exit\": " << (browser.impl_->testReadyBeforeSetupExit ? "true" : "false")
        << ",\n  \"cancel_then_immediate_retry\": " << (cancellationReset ? "true" : "false")
        << ",\n  \"forum_afk_back_lifecycle_checked\": " << (commandsPaired ? "true" : "false")
        << ",\n  \"forum_afk_back_pairs\": " << sessionCommands.size()/2
        << ",\n  \"isolated_anonymous_profile\": true,\n  \"login_or_post_submitted\": false\n}\n";
    for (const auto& message : notices) std::printf("[NativeForumSmoke] %s\n", message.c_str());
    std::printf("NativeForumSmoke: ready=%d frames=%d closed=%d cycles=%d\n", ready, frames, closed, successfulCycles);
    browser.Shutdown();
    return ready && closed && successfulCycles == 4 && cancellationReset && browser.impl_->testReadyBeforeSetupExit && commandsPaired ? 0 : 1;
}
#endif
} // namespace keybinder
