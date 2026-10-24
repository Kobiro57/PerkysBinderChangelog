#pragma once

#include <functional>
#include <string>
#include <vector>

#include "imgui-1.92.7/imgui.h"

namespace keybinder {

class SettingsManager;
class OnlineDataManager;
class OnlineOverlayController;
struct PersistentOverlayData;

enum class OnlineQuickOverlayMode {
    Toggle,
    Hold,
};

class OnlineOverlayRenderer {
public:
    OnlineOverlayRenderer();
    ~OnlineOverlayRenderer();

    void Initialize(SettingsManager* settings, OnlineDataManager* dataManager);
    void Shutdown();
    void Update();
    void Render();

    // Persistent overlays (always-on HUD surfaces)
    void SetOverlayController(OnlineOverlayController* controller);
    void RenderPersistentOverlays();
    void SetPersistentOverlayMoveMode(bool enabled);
    void TogglePersistentOverlayMoveMode();
    bool IsPersistentOverlayMoveModeActive() const { return persistentOverlayMoveMode_; }
    void SetSettingsPersistCallback(std::function<void()> callback);

    bool IsVisible() const { return visible_; }
    void Show();
    void Hide();
    void Toggle();

    void SetQuickMapVisibleCallback(std::function<bool()> callback);
    void SetOverlayVisibleCallback(std::function<bool()> callback);
    void SetInputGateCallback(std::function<bool()> callback);

    static OnlineQuickOverlayMode ParseMode(const std::string& value);
#ifdef KEYBINDER_TESTING
    struct TestingHooks {
        std::function<bool(int)> isKeyDown;
        std::function<bool()> isInputGated;
    };

    void SetTestingHooks(const TestingHooks& hooks);
    int CountFilteredPlayersForTesting(const struct OnlineSnapshot& snapshot) const { return CountFilteredPlayers(snapshot); }
    void PersistMovedOverlayPositionForTesting(const PersistentOverlayData& data, const ImVec2& position, const ImVec2& size) {
        PersistMovedOverlayPosition(data, position, size, 1.0f, 0.0f);
    }
#endif

private:
    void RefreshSettings();
    bool IsFeatureEnabled() const;
    bool IsHotkeyBound() const;
    bool IsHotkeyDown() const;
    bool IsInputGated() const;
    bool ShouldBeVisible() const;

    // Rendering sub-sections
    void RenderHeader(const struct OnlineSnapshot& snapshot, float panelWidth, float scale);
    void RenderSummaryLine(const struct OnlineSnapshot& snapshot, float panelWidth, float scale);
    void RenderBucketList(const struct OnlineBucketSummary& bucket, float panelWidth, float scale);
    void RenderEmptyState(float panelWidth, float scale);

    // Persistent overlay rendering
    void RenderSinglePersistentOverlay(const PersistentOverlayData& data, float scale, float stackedOffsetY);
    void PersistMovedOverlayPosition(const PersistentOverlayData& data, const ImVec2& windowPos, const ImVec2& windowSize, float scale, float stackedOffsetY);

    // Filter logic
    bool ShouldShowFaction(const std::string& factionName) const;
    bool ShouldShowGang(const std::string& gangName) const;
    int CountFilteredPlayers(const struct OnlineSnapshot& snapshot) const;

    SettingsManager* settingsManager_ = nullptr;
    OnlineDataManager* dataManager_ = nullptr;
    OnlineOverlayController* overlayController_ = nullptr;
    std::function<void()> settingsPersistCallback_;

    bool visible_ = false;
    OnlineQuickOverlayMode mode_ = OnlineQuickOverlayMode::Toggle;
    int hotkey_ = 0;
    bool hotkeyWasDown_ = false;
    bool inputWasGated_ = false;

    bool persistentOverlayMoveMode_ = false;
    bool persistentOverlayMovedSinceSave_ = false;

    // Cached settings filters
    bool showLspd_ = true;
    bool showArmy_ = true;
    bool showFireDepartment_ = true;
    bool showRcb_ = true;
    bool showZivilisten_ = true;
    bool showAllGangs_ = true;
    std::vector<std::string> selectedGangs_;

    // Callbacks
    std::function<bool()> quickMapVisibleCallback_;
    std::function<bool()> overlayVisibleCallback_;
    std::function<bool()> inputGateCallback_;
#ifdef KEYBINDER_TESTING
    TestingHooks testingHooks_{};
#endif
};

} // namespace keybinder
