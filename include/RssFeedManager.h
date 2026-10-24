#pragma once

#include "RssHttpClient.h"
#include "RssParser.h"
#include "RssTypes.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace keybinder {

class SettingsManager;

struct RssToastData {
    std::string feedName;      // "ForumName" for "Neuer Post in: ForumName"
    std::string title;         // Post title
    std::string author;        // Creator name for "von CreatorName"
};

class RssFeedManager {
public:
    explicit RssFeedManager(SettingsManager& settingsManager);
    ~RssFeedManager();

    void Initialize();
    void Shutdown();
    void PollFeeds();
    std::vector<RssToastData> ConsumeNewItems();
    void UpdateSettings();

#ifdef KEYBINDER_TESTING
    void TestHook_ForceFeedEligible(std::size_t feedIndex);
    std::vector<RssFeedConfig> TestHook_CopyFeedConfigs() const;
#endif

private:
    struct FeedRuntimeState {
        RssFeedConfig config;
        std::chrono::steady_clock::time_point lastPollTime{};
        bool hasPolled = false;
    };

    void WorkerLoop();
    static std::string MakeItemBaseKey(const RssFeedItem& item);
    static std::string MakeItemGuidKey(const RssFeedItem& item);
    static std::string BuildToastMessage(const RssFeedConfig& feed, const RssFeedItem& item);
    static void TrimSeenGuids(RssFeedConfig& feed);

    SettingsManager& settingsManager_;
    RssHttpClient httpClient_;
    RssParser parser_;

    std::atomic<bool> running_{false};
    std::thread workerThread_;

    mutable std::mutex feedsMutex_;
    bool rssEnabled_ = false;
    std::vector<FeedRuntimeState> feeds_;

    mutable std::mutex newItemsMutex_;
    std::vector<RssToastData> newItems_;
};

} // namespace keybinder
