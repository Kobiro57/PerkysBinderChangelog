/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    RSS Tracker UI - Feed management tab for MainWindowUI
*/
#pragma once

#include <string>
#include <vector>
#include <functional>
#include "RssTypes.h"

namespace keybinder {

class SettingsManager;

// Forward declaration - RssFeedManager is provided by a sibling task
class RssFeedManager;

class RssTrackerUI {
public:
    RssTrackerUI() = default;
    ~RssTrackerUI() = default;

    // Main render function (called from MainWindowUI content area)
    void Render();

    // Dependency injection (matches KillTrackerUI / HitmarkerUI Set* pattern)
    void SetSettingsManager(SettingsManager* manager) { settingsManager_ = manager; }
    void SetRssFeedManager(RssFeedManager* manager) { feedManager_ = manager; }

    // Callback when settings change and should be persisted
    std::function<void()> OnSettingsChanged;

    // --- Test hooks for programmatic unit testing ---
    bool TestHook_AddFeed(const RssFeedConfig& feed);
    bool TestHook_RemoveFeed(int index);
    bool TestHook_ToggleFeed(int index);
    int TestHook_GetFeedCount() const;

private:
    SettingsManager* settingsManager_ = nullptr;
    RssFeedManager* feedManager_ = nullptr;

    // --- UI state ---
    bool globalEnabled_ = false;
    RssNotificationMode notificationMode_ = RssNotificationMode::Toast;
    int selectedFeedIndex_ = -1;
    int editingFeedIndex_ = -2;  // -2 = form closed, -1 = adding new, >= 0 = editing existing

    // Inline form buffers
    static constexpr std::size_t kUrlBufSize = 512;
    static constexpr std::size_t kLabelBufSize = 128;
    char urlBuf_[kUrlBufSize] = {};
    char labelBuf_[kLabelBufSize] = {};
    int intervalMinutes_ = 5;       // 1-60 min
    bool formEnabled_ = true;

    // Test-feed result
    bool testFeedPending_ = false;
    bool testFeedSucceeded_ = false;
    int testFeedItemCount_ = 0;
    std::string testFeedError_;
    float testFeedResultAge_ = 0.0f; // seconds since result

    // Delete confirmation
    int pendingDeleteIndex_ = -1;

    // --- Rendering helpers ---
    void RenderHeader();
    void RenderFeedList();
    void RenderFeedRow(const RssFeedConfig& feed, int index);
    void RenderAddEditForm();
    void RenderDeleteConfirmation();
    void RenderTestFeedResult();

    // --- Data helpers ---
    std::vector<RssFeedConfig>& GetFeeds();
    const std::vector<RssFeedConfig>& GetFeeds() const;
    void SyncFromSettings();
    void PersistFeeds();
    void OpenAddForm();
    void OpenEditForm(int index);
    void ResetForm();
    void SaveFormToFeed(RssFeedConfig& feed);
    std::string TruncateUrl(const std::string& url, std::size_t maxChars) const;
};

} // namespace keybinder
