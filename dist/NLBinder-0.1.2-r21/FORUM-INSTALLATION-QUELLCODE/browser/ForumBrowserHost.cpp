#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cctype>
#include <string>
#include <thread>
#include <vector>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_download_handler.h"
#include "include/cef_parser.h"
#include "include/cef_render_handler.h"
#include "include/cef_sandbox_win.h"
#include "include/wrapper/cef_closure_task.h"
#include "ForumBrowserProtocol.h"

namespace {
namespace ipc = keybinder::forum;
LONG Read(volatile std::int32_t& value) { return InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&value), 0, 0); }
void Write(volatile std::int32_t& value, LONG updated) { InterlockedExchange(reinterpret_cast<volatile LONG*>(&value), updated); }

bool Allowed(const CefString& url) {
    CefURLParts parts;
    if (!CefParseURL(url, parts)) return false;
    auto host = CefString(&parts.host).ToString();
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto scheme = CefString(&parts.scheme).ToString();
    const auto port = CefString(&parts.port).ToString();
    return scheme == "https" && (port.empty() || port == "443")
        && (host == "nova-legacy.de" || (host.size() > 15 && host.ends_with(".nova-legacy.de")));
}

class ForumClient final : public CefClient, public CefLifeSpanHandler, public CefRenderHandler,
    public CefDisplayHandler, public CefLoadHandler, public CefRequestHandler,
    public CefDownloadHandler, public CefContextMenuHandler {
public:
    explicit ForumClient(ipc::SharedState* shared) : shared_(shared) {}
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        browser_ = browser;
        Write(shared_->state, static_cast<LONG>(ipc::State::Ready));
        browser->GetHost()->SetFocus(true);
    }
    void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        browser_ = nullptr;
        Write(shared_->state, static_cast<LONG>(ipc::State::Closed));
        CefQuitMessageLoop();
    }
    void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override { rect = CefRect(0, 0, width_, height_); }
    bool GetScreenPoint(CefRefPtr<CefBrowser>, int viewX, int viewY, int& x, int& y) override {
        x = Read(shared_->screenX) + viewX; y = Read(shared_->screenY) + viewY; return true;
    }
    bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
        info.device_scale_factor = 1.0f; info.depth = 32; info.depth_per_component = 8;
        info.rect = info.available_rect = CefRect(0, 0, width_, height_); return true;
    }
    void OnPopupShow(CefRefPtr<CefBrowser>, bool show) override { popupShown_ = show; if (!show) { popup_.clear(); Publish(); } }
    void OnPopupSize(CefRefPtr<CefBrowser>, const CefRect& rect) override { popupRect_ = rect; }
    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const void* buffer, int width, int height) override {
        if (width < 1 || height < 1 || width > ipc::kMaximumWidth || height > ipc::kMaximumHeight) return;
        auto& pixels = type == PET_VIEW ? view_ : popup_;
        pixels.resize(static_cast<std::size_t>(width) * height * 4);
        std::memcpy(pixels.data(), buffer, pixels.size());
        if (type == PET_VIEW) { paintedWidth_ = width; paintedHeight_ = height; }
        else { popupWidth_ = width; popupHeight_ = height; }
        Publish();
    }
    void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& value) override { CopyText(shared_->title, value.ToString()); }
    void OnAddressChange(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, const CefString& url) override {
        if (frame->IsMain()) CopyText(shared_->url, url.ToString());
    }
    void OnLoadingStateChange(CefRefPtr<CefBrowser>, bool loading, bool back, bool forward) override {
        Write(shared_->loading, loading); Write(shared_->canGoBack, back); Write(shared_->canGoForward, forward);
    }
    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code,
                     const CefString&, const CefString&) override {
        if (frame->IsMain() && code != ERR_ABORTED) Write(shared_->errorCode, static_cast<LONG>(code));
    }
    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool, bool) override {
        if (!frame->IsMain() || Allowed(request->GetURL())) return false;
        Write(shared_->blockedLink, 1); return true;
    }
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>, int,
        const CefString& target, const CefString&, cef_window_open_disposition_t, bool,
        const CefPopupFeatures&, CefWindowInfo&, CefRefPtr<CefClient>&,
        CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*) override {
        if (Allowed(target)) browser->GetMainFrame()->LoadURL(target);
        else Write(shared_->blockedLink, 1);
        return true;
    }
    bool CanDownload(CefRefPtr<CefBrowser>, const CefString&, const CefString&) override {
        Write(shared_->blockedLink, 2); return false;
    }
    void OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefContextMenuParams>, CefRefPtr<CefMenuModel> model) override {
        // Copy/paste is provided by the in-game menu and normal keyboard
        // shortcuts. No native popup window is placed behind exclusive GTA.
        model->Clear();
    }

    void Dispatch(const ipc::Command& command) {
        if (!browser_) return;
        auto host = browser_->GetHost();
        CefMouseEvent mouse;
        mouse.x = command.a; mouse.y = command.b; mouse.modifiers = Modifiers(command.modifiers);
        switch (command.type) {
        case ipc::CommandType::Resize:
            width_ = std::clamp(command.a, 320, ipc::kMaximumWidth);
            height_ = std::clamp(command.b, 240, ipc::kMaximumHeight);
            popup_.clear(); popupShown_ = false; host->WasResized(); break;
        case ipc::CommandType::MouseMove: host->SendMouseMoveEvent(mouse, command.c != 0); break;
        case ipc::CommandType::MouseButton:
            host->SendMouseClickEvent(mouse, command.c == 1 ? MBT_RIGHT : (command.c == 2 ? MBT_MIDDLE : MBT_LEFT), command.d != 0, std::clamp(command.e, 1, 2)); break;
        case ipc::CommandType::MouseWheel: host->SendMouseWheelEvent(mouse, command.c, command.d); break;
        case ipc::CommandType::Key: {
            CefKeyEvent key;
            key.type = command.a == static_cast<int>(ipc::KeyType::Character) ? KEYEVENT_CHAR
                : (command.a == static_cast<int>(ipc::KeyType::Up) ? KEYEVENT_KEYUP : KEYEVENT_RAWKEYDOWN);
            key.windows_key_code = command.b; key.native_key_code = command.c;
            key.modifiers = Modifiers(command.modifiers); key.is_system_key = command.d != 0;
            if (key.type == KEYEVENT_CHAR) key.character = key.unmodified_character = static_cast<char16_t>(command.b);
            host->SendKeyEvent(key); break;
        }
        case ipc::CommandType::Focus: host->SetFocus(command.a != 0); break;
        case ipc::CommandType::Back: if (browser_->CanGoBack()) browser_->GoBack(); break;
        case ipc::CommandType::Forward: if (browser_->CanGoForward()) browser_->GoForward(); break;
        case ipc::CommandType::Reload: Write(shared_->errorCode, 0); browser_->Reload(); break;
        case ipc::CommandType::Home: Write(shared_->errorCode, 0); browser_->GetMainFrame()->LoadURL(ipc::kHome); break;
        case ipc::CommandType::Close: Write(shared_->state, static_cast<LONG>(ipc::State::Closing)); host->CloseBrowser(true); break;
        default: {
            auto frame = browser_->GetFocusedFrame(); if (!frame) frame = browser_->GetMainFrame();
            if (command.type == ipc::CommandType::Copy) frame->Copy();
            else if (command.type == ipc::CommandType::Paste) frame->Paste();
            else if (command.type == ipc::CommandType::Cut) frame->Cut();
            else if (command.type == ipc::CommandType::SelectAll) frame->SelectAll();
            break;
        }
        }
    }
private:
    template<std::size_t N> void CopyText(char (&target)[N], const std::string& text) {
        if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&shared_->frameLock), 1, 0) != 0) return;
        std::memcpy(target, text.data(), (std::min)(text.size(), N - 1)); target[(std::min)(text.size(), N - 1)] = 0;
        Write(shared_->frameLock, 0);
    }
    static unsigned int Modifiers(unsigned int flags) {
        unsigned int result = 0;
        if (flags & ipc::Shift) result |= EVENTFLAG_SHIFT_DOWN;
        if (flags & ipc::Control) result |= EVENTFLAG_CONTROL_DOWN;
        if (flags & ipc::Alt) result |= EVENTFLAG_ALT_DOWN;
        if (flags & ipc::LeftButton) result |= EVENTFLAG_LEFT_MOUSE_BUTTON;
        if (flags & ipc::MiddleButton) result |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
        if (flags & ipc::RightButton) result |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
        if (flags & ipc::CapsLock) result |= EVENTFLAG_CAPS_LOCK_ON;
        if (flags & ipc::NumLock) result |= EVENTFLAG_NUM_LOCK_ON;
        if (flags & ipc::Repeat) result |= EVENTFLAG_IS_REPEAT;
        return result;
    }
    void Publish() {
        if (view_.empty()) return;
        if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&shared_->frameLock), 1, 0) != 0) {
            if (browser_) browser_->GetHost()->Invalidate(PET_VIEW);
            return;
        }
        const std::size_t bytes = static_cast<std::size_t>(paintedWidth_) * paintedHeight_ * 4;
        std::memcpy(shared_->pixels, view_.data(), bytes);
        if (popupShown_ && !popup_.empty()) {
            for (int y = 0; y < popupHeight_; ++y) {
                const int targetY = popupRect_.y + y;
                if (targetY < 0 || targetY >= paintedHeight_) continue;
                for (int x = 0; x < popupWidth_; ++x) {
                    const int targetX = popupRect_.x + x;
                    if (targetX >= 0 && targetX < paintedWidth_)
                        std::memcpy(shared_->pixels + (targetY * paintedWidth_ + targetX) * 4,
                            popup_.data() + (y * popupWidth_ + x) * 4, 4);
                }
            }
        }
        Write(shared_->width, paintedWidth_); Write(shared_->height, paintedHeight_);
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared_->frameSequence));
        Write(shared_->frameLock, 0);
    }
    ipc::SharedState* shared_;
    CefRefPtr<CefBrowser> browser_;
    int width_ = 1100, height_ = 680, paintedWidth_ = 0, paintedHeight_ = 0;
    int popupWidth_ = 0, popupHeight_ = 0;
    bool popupShown_ = false;
    CefRect popupRect_;
    std::vector<std::uint8_t> view_, popup_;
    IMPLEMENT_REFCOUNTING(ForumClient);
};

class DispatchTask final : public CefTask {
public:
    DispatchTask(CefRefPtr<ForumClient> client, ipc::Command command) : client_(client), command_(command) {}
    void Execute() override { client_->Dispatch(command_); }
private:
    CefRefPtr<ForumClient> client_;
    ipc::Command command_;
    IMPLEMENT_REFCOUNTING(DispatchTask);
};

class ForumApp final : public CefApp, public CefBrowserProcessHandler {
public:
    explicit ForumApp(ipc::SharedState* shared) : shared_(shared), client_(new ForumClient(shared)) {}
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnContextInitialized() override {
        CefWindowInfo info;
        info.SetAsWindowless(reinterpret_cast<HWND>(static_cast<std::uintptr_t>(shared_->parentWindow)));
        CefBrowserSettings settings;
        settings.windowless_frame_rate = 30;
        if (!CefBrowserHost::CreateBrowser(info, client_, ipc::kHome, settings, nullptr, nullptr)) {
            Write(shared_->state, static_cast<LONG>(ipc::State::Error)); CefQuitMessageLoop();
        }
    }
    CefRefPtr<ForumClient> Client() { return client_; }
private:
    ipc::SharedState* shared_;
    CefRefPtr<ForumClient> client_;
    IMPLEMENT_REFCOUNTING(ForumApp);
};

std::wstring ProfilePath(CefRefPtr<CefCommandLine> args) {
    // Test profiles are explicitly isolated and never reuse the user's login.
    if (args->HasSwitch("nlbinder-smoke-profile")) return args->GetSwitchValue("nlbinder-smoke-profile").ToWString();
    PWSTR root = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &root))) return {};
    std::wstring result = root; CoTaskMemFree(root);
    return result + L"\\NLBinder\\Browser\\Forum";
}
} // namespace

// Official sandbox bootstrap remains byte-for-byte unchanged. It supplies
// the sandbox from the executable as required by modern CEF (M138+).
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE instance, LPWSTR, int, void* sandboxInfo, cef_version_info_t*) {
    CefMainArgs mainArgs(instance);
    const int child = CefExecuteProcess(mainArgs, nullptr, sandboxInfo);
    if (child >= 0) return child;
    if (!sandboxInfo) return 20;
    HANDLE token = nullptr; TOKEN_ELEVATION elevation{}; DWORD bytes = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytes); CloseHandle(token);
    }
    if (elevation.TokenIsElevated) return 21; // Never run web content as administrator.
    auto args = CefCommandLine::CreateCommandLine(); args->InitFromString(GetCommandLineW());
    const auto mapName = args->GetSwitchValue("nlbinder-map").ToWString();
    if (!mapName.starts_with(L"Local\\NLBinder.Forum.") || mapName.size() > 160) return 22;
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName.c_str());
    if (!mapping) return 23;
    auto* shared = static_cast<ipc::SharedState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ipc::SharedState)));
    if (!shared || shared->magic != ipc::kMagic || shared->version != ipc::kVersion || shared->bytes != sizeof(ipc::SharedState)) {
        if (shared) UnmapViewOfFile(shared); CloseHandle(mapping); return 24;
    }
    HANDLE input = OpenEventW(SYNCHRONIZE, FALSE, (mapName + L".input").c_str());
    const auto profile = ProfilePath(args);
    if (!input || profile.empty()) { if (input) CloseHandle(input); UnmapViewOfFile(shared); CloseHandle(mapping); return 25; }
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.persist_session_cookies = true;
    settings.log_severity = LOGSEVERITY_DISABLE;
    CefString(&settings.root_cache_path) = profile;
    CefString(&settings.cache_path) = profile + L"\\Cache";
    CefString(&settings.locale) = "de";
    CefString(&settings.accept_language_list) = "de-DE,de,en-US,en";
    CefRefPtr<ForumApp> app(new ForumApp(shared));
    if (!CefInitialize(mainArgs, settings, app, sandboxInfo)) {
        Write(shared->state, static_cast<LONG>(ipc::State::Error));
        CloseHandle(input); UnmapViewOfFile(shared); CloseHandle(mapping); return CefGetExitCode();
    }
    std::atomic<bool> stop{false};
    std::thread commands([&]() {
        while (!stop.load()) {
            WaitForSingleObject(input, 100);
            while (!stop.load()) {
                const auto read = static_cast<std::uint32_t>(Read(shared->commandRead));
                const auto write = static_cast<std::uint32_t>(Read(shared->commandWrite));
                if (read == write || write - read > ipc::kCommandCount) break;
                const ipc::Command command = shared->commands[read % ipc::kCommandCount];
                SecureZeroMemory(&shared->commands[read % ipc::kCommandCount], sizeof(ipc::Command));
                Write(shared->commandRead, static_cast<LONG>(read + 1));
                CefPostTask(TID_UI, new DispatchTask(app->Client(), command));
            }
        }
    });
    CefRunMessageLoop();
    stop.store(true); commands.join();
    app = nullptr; CefShutdown();
    CloseHandle(input); UnmapViewOfFile(shared); CloseHandle(mapping);
    return 0;
}
