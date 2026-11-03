#pragma once

#include <functional>
#include <memory>
#include <string>
#include <windows.h>

struct IDirect3DDevice9;
namespace keybinder {
class SettingsManager;
class ForumBrowser {
public:
    ForumBrowser();
    ~ForumBrowser();
    void Initialize(SettingsManager* settings, std::function<void(const std::string&)> notice,
                    std::function<void()> persist);
    void Toggle();
    void Close();
    void Shutdown();
    void Update();
    void Render(IDirect3DDevice9* device);
    void OnDeviceLost();
    bool IsOpen() const;
    bool HandleWindowMessage(HWND window, UINT message, WPARAM key, LPARAM native);
private:
#ifdef KEYBINDER_TESTING
    friend int RunNativeForumSmokeTest();
#endif
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace keybinder
