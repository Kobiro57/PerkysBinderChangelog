/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Online UI - Online overview and overlay settings tabs
*/
#pragma once

#include "OnlineDataManager.h"

#include <string>
#include <vector>

namespace keybinder {

class SettingsManager;
class KeybindManager;
class OnlineOverlayController;
class OnlineOverlayRenderer;
class OnlineEventDetector;

enum class OnlineSubtab {
    Overview = 0,
    OverlaySettings = 1,
};

enum class OnlineOverlaySection {
    Quick = 0,
    Persistent = 1,
    Appearance = 2,
};

enum class OnlinePersistentStyleTarget {
    GangsOverview = 0,
    OwnGang = 1,
    Wanted = 2,
};

class OnlineUI {
public:
    OnlineUI();
    ~OnlineUI() = default;

    // Main render entry — called from MainWindowUI::RenderOnlineView
    void Render();

    // Dependency injection — called by MainWindowUI after construction
    void SetOnlineDataManager(OnlineDataManager* manager) { onlineDataManager_ = manager; }
    void SetSettingsManager(SettingsManager* manager) { settingsManager_ = manager; }
    void SetKeybindManager(KeybindManager* manager) { keybindManager_ = manager; }
    void SetOnlineOverlayController(OnlineOverlayController* controller) { overlayController_ = controller; }
    void SetOnlineOverlayRenderer(OnlineOverlayRenderer* renderer) { overlayRenderer_ = renderer; }
    void SetOnlineEventDetector(OnlineEventDetector* detector) { eventDetector_ = detector; }
    void SetLocalPlayerName(const std::string& localPlayerName) { localPlayerName_ = localPlayerName; }

    // Expose current tab for testing
    OnlineSubtab GetCurrentSubtab() const { return currentSubtab_; }
    void SetCurrentSubtab(OnlineSubtab tab) { currentSubtab_ = tab; }

    // Balance bucket cards across `columnCount` vertical columns. Walks the
    // weights in order and drops each card into the currently shortest column,
    // returning the card indices grouped per column. Pure helper so the
    // overview's masonry packing can be exercised without an ImGui context.
    static std::vector<std::vector<int>> BalanceCardColumns(const std::vector<int>& cardWeights, int columnCount);

private:
    OnlineDataManager* onlineDataManager_ = nullptr;
    SettingsManager* settingsManager_ = nullptr;
    KeybindManager* keybindManager_ = nullptr;
    OnlineOverlayController* overlayController_ = nullptr;
    OnlineOverlayRenderer* overlayRenderer_ = nullptr;
    OnlineEventDetector* eventDetector_ = nullptr;
    std::string localPlayerName_;
    OnlineSubtab currentSubtab_ = OnlineSubtab::Overview;
    OnlineOverlaySection currentOverlaySection_ = OnlineOverlaySection::Quick;
    OnlinePersistentStyleTarget currentStyleTarget_ = OnlinePersistentStyleTarget::GangsOverview;

    // Hotkey capture state
    bool hotkeyCaptureActive_ = false;
    int hotkeyCaptureFrame_ = -1;
    std::vector<int> hotkeyIgnoreKeys_;

    // Cached gang list from snapshot (preserves offline selections)
    std::vector<std::string> cachedGangList_;

    // Sub-tab rendering
    void RenderOverviewTab();
    void RenderOverlaySettingsTab();

    // Overlay settings helpers
    void RenderQuickOverlaySettings();
    void RenderPersistentOverlaySettings();
    void RenderPersistentOverlayStyleControls(
        std::string& style,
        float& opacity,
        bool& backgroundEnabled,
        float& overlayScale,
        bool& customTextColorEnabled,
        float& textColorR,
        float& textColorG,
        float& textColorB,
        float& textColorA);
    void RenderFactionFilterGrid();
    void RenderGangSelectionGrid();
    void StartHotkeyCapture();
    void RenderHotkeyCaptureUI();
    void PersistSettings();
    void ApplyLiveSettings();
    std::string GetHotkeyLabel(int vk) const;
    void RefreshCachedGangList();

    // Overview helpers
    void RenderStatusHeader(const OnlineSnapshot& snapshot);
    void RenderFactionSection(const OnlineSnapshot& snapshot);
    void RenderGangsSection(const OnlineSnapshot& snapshot);
};

} // namespace keybinder
