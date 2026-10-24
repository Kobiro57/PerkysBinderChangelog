/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Main Window UI - Sidebar navigation and content area
*/
#pragma once

#include "KeybindListUI.h"
#include "KeybindEditorUI.h"
#include "KillTrackerUI.h"
#include "JobsUI.h"
#include "HitmarkerUI.h"
#include "CrosshairUI.h"
#include "SquareMapUI.h"
#include "HudSpriteUI.h"
#include "HitsoundUI.h"
#include "InterfaceEditorUI.h"
#include "RssTrackerUI.h"
#include "OnlineUI.h"
#include "KeybindManager.h"
#include "SettingsManager.h"
#include "ThemeManager.h"
#include "InterfaceEditor.h"
#include <d3d9.h>
#include <functional>
#include <array>
#include <string>
#include <vector>

class SAMPBridge;

namespace jobs {
class JobTracker;
}

namespace keybinder {

class Hitmarker;
class HitsoundPlayer;
class CrosshairSwitcher;
class SquareMap;
class HudSpriteManager;
class TextureManager;
class RssFeedManager;
class SpeedometerManager;

enum class NavCategory {
    Keybinds = 0,
    KillTracker = 1,
    Jobs = 2,
    Mods = 3,
    Settings = 4,
    Help = 5,
    Online = 6,
};

enum class SettingsSubtab {
    General = 0,
    Appearance = 1,
    QuickMap = 2,
    RssTracker = 3,
};

enum class ModsSubtab {
    Hitmarker = 0,
    Hitsound = 1,
    Hud = 2,
};

enum class HudSpriteLibrarySubtab {
    Crosshair = 0,
    HudIcons = 1,
};

class MainWindowUI {
public:
    MainWindowUI();
    ~MainWindowUI() = default;

    void Render();

    void SetKeybindManager(KeybindManager* manager);

    void OnKeybindsChanged(std::function<void()> callback);

    NavCategory GetCurrentCategory() const { return currentCategory_; }

    void SetCategory(NavCategory category);

    void SetSettingsManager(SettingsManager* manager);
    void SetHitmarker(Hitmarker* hitmarker);
    void SetCrosshairSwitcher(CrosshairSwitcher* switcher);
    void SetSquareMap(SquareMap* squareMap);
    void SetHudSpriteManager(HudSpriteManager* manager);
    void SetHitsoundPlayer(HitsoundPlayer* player);
    void SetInterfaceEditor(InterfaceEditor* editor);
    void SetTextureManager(TextureManager* manager);
    void SetDeviceGetter(std::function<IDirect3DDevice9*()> getter);
    
    void SetSAMPBridge(SAMPBridge* bridge);

    void SetRssFeedManager(RssFeedManager* manager);
    void SetSpeedometerManager(SpeedometerManager* manager);

    void SetOnlineDataManager(OnlineDataManager* manager);
    void SetOnlineOverlayController(OnlineOverlayController* controller);
    void SetOnlineOverlayRenderer(OnlineOverlayRenderer* renderer);
    void SetOnlineEventDetector(OnlineEventDetector* detector);
    void SetOnlineLocalPlayerName(const std::string& localPlayerName);

    jobs::JobTracker* GetJobTracker() const;

    bool IsCapturingKey() const;

    bool WasCloseRequested() const;
    void ResetCloseRequest();
    void PersistWindowState();

private:
    NavCategory currentCategory_ = NavCategory::Keybinds;

    void RenderSidebar();
    bool EnsureSidebarLogoLoaded();
    void RenderSidebarLogo();
    float sidebarWidth_ = 252.0f;

    void RenderContent();

    void RenderKeybindsView();
    void RenderKillTrackerView();
    void RenderJobsView();
    void RenderModsView();
    void RenderModsTabs();
    void RenderHudModsTab();
    void RenderHudSpriteLibraryTabs();
    void RenderSettingsView();
    void RenderSettingsTabs();
    void RenderSettingsGeneralTab();
    void RenderSettingsAppearanceTab();
    void RenderSettingsQuickMapTab();
    void RenderSettingsRssTrackerTab();
    void RenderSettingsTachoCard();
    void RenderSettingsKillCounterCard();
    void SaveKillCounterSettings();
    void SyncKillTrackerSettings();
    void RenderHelpView();
    void RenderOnlineView();
    void SyncUiOpenSettingsBuffers();
    void SaveUiOpenSettings();
    void SaveSpeedometerSettings();
    void StartUiHotkeyCapture();
    void RenderUiHotkeyCaptureUI();
    void StartQuickMapHotkeyCapture();
    void RenderQuickMapHotkeyCaptureUI();
    void ApplyCurrentTheme();

    bool editorOpen_ = false;
    bool creatingNew_ = false;
    int selectedKeybindIndex_ = -1;

    KeybindListUI listUI_;
    KeybindEditorUI editorUI_;
    KillTrackerUI killTrackerUI_;
    JobsUI jobsUI_;
    HitmarkerUI hitmarkerUI_;
    CrosshairUI crosshairUI_;
    SquareMapUI squareMapUI_;
    HudSpriteUI hudSpriteUI_;
    HitsoundUI hitsoundUI_;
    InterfaceEditorUI interfaceEditorUI_;
    RssTrackerUI rssTrackerUI_;
    OnlineUI onlineUI_;

    KeybindManager* keybindManager_ = nullptr;
    SAMPBridge* bridge_ = nullptr;
    RssFeedManager* rssFeedManager_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    SpeedometerManager* speedometerManager_ = nullptr;
    OnlineOverlayController* onlineOverlayController_ = nullptr;
    OnlineEventDetector* onlineEventDetector_ = nullptr;
    std::function<IDirect3DDevice9*()> deviceGetter_;
    bool sidebarLogoLoadAttempted_ = false;

    SettingsManager* settingsManager_ = nullptr;
    ModsSubtab currentModsSubtab_ = ModsSubtab::Hitmarker;
    HudSpriteLibrarySubtab currentHudSpriteLibrarySubtab_ = HudSpriteLibrarySubtab::Crosshair;
    SettingsSubtab currentSettingsSubtab_ = SettingsSubtab::General;
    std::array<char, 64> uiOpenCommandPrimaryBuffer_{};
    std::array<char, 64> uiOpenCommandSecondaryBuffer_{};
    bool uiOpenSettingsDirty_ = false;
    bool uiHotkeyCaptureActive_ = false;
    int uiHotkeyCaptureFrame_ = -1;
    std::vector<int> uiHotkeyIgnoreKeys_;
    bool quickMapHotkeyCaptureActive_ = false;
    int quickMapHotkeyCaptureFrame_ = -1;
    std::vector<int> quickMapHotkeyIgnoreKeys_;

    std::function<void()> onKeybindsChanged_;

    bool closeRequested_ = false;
    bool showKillMessageVariables_ = false;

    void HandleCreateKeybind();
    void HandleEditKeybind(const Keybind& keybind);
    void HandleSaveKeybind(const Keybind& keybind);
    void HandleDeleteKeybind(const std::string& name);
    void RefreshKeybindList();
};

} // namespace keybinder
