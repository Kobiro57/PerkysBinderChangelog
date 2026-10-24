#include "../include/RssFeedManager.h"

#include "../include/Logger.h"
#include "../include/SettingsManager.h"

#include <algorithm>
#include <ctime>
#include <exception>
#include <unordered_map>
#include <utility>

namespace keybinder {

namespace {

constexpr std::size_t kMaxSeenGuidsPerFeed = 100;

using FeedClock = std::chrono::steady_clock;
using FeedSeconds = std::chrono::seconds;

FeedSeconds NormalizePollIntervalSeconds(int pollIntervalSeconds) {
    return FeedSeconds((std::max)(1, pollIntervalSeconds));
}

std::string MakeFeedKey(const RssFeedConfig& feed) {
    return feed.url;
}

} // namespace

RssFeedManager::RssFeedManager(SettingsManager& settingsManager)
    : settingsManager_(settingsManager) {
}

RssFeedManager::~RssFeedManager() {
    Shutdown();
}

void RssFeedManager::Initialize() {
    Shutdown();

    {
        std::lock_guard<std::mutex> lock(feedsMutex_);
        rssEnabled_ = settingsManager_.Get().rss_enabled;
        feeds_.clear();
        feeds_.reserve(settingsManager_.GetRssFeeds().size());

        for (const RssFeedConfig& feed : settingsManager_.GetRssFeeds()) {
            FeedRuntimeState state;
            state.config = feed;
            TrimSeenGuids(state.config);
            feeds_.push_back(std::move(state));
        }
    }

    {
        std::lock_guard<std::mutex> lock(newItemsMutex_);
        newItems_.clear();
    }

    running_.store(true);
    workerThread_ = std::thread(&RssFeedManager::WorkerLoop, this);
    Logger::Info("[RssFeedManager] Initialized with %zu feed(s), rss_enabled=%s",
                 settingsManager_.GetRssFeeds().size(),
                 settingsManager_.Get().rss_enabled ? "true" : "false");
}

void RssFeedManager::Shutdown() {
    const bool wasRunning = running_.exchange(false);
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    // Only persist runtime feed state back to settings when the worker
    // thread was actually running. When Shutdown() is called from
    // Initialize() (before the thread starts), feeds_ is empty and
    // writing it back would wipe the user's loaded RSS feeds.
    if (wasRunning) {
        std::vector<RssFeedConfig> feedsToPersist;
        {
            std::lock_guard<std::mutex> lock(feedsMutex_);
            feedsToPersist.reserve(feeds_.size());
            for (const FeedRuntimeState& feed : feeds_) {
                feedsToPersist.push_back(feed.config);
            }
        }

        if (!feedsToPersist.empty() || !settingsManager_.GetRssFeeds().empty()) {
            settingsManager_.SetRssFeeds(feedsToPersist);
        }

        Logger::Info("[RssFeedManager] Shutdown complete");
    }
}

void RssFeedManager::PollFeeds() {
    std::vector<std::pair<std::size_t, RssFeedConfig>> feedsToPoll;
    const FeedClock::time_point now = FeedClock::now();

    {
        std::lock_guard<std::mutex> lock(feedsMutex_);
        if (!rssEnabled_) {
            return;
        }

        feedsToPoll.reserve(feeds_.size());
        for (std::size_t i = 0; i < feeds_.size(); ++i) {
            FeedRuntimeState& state = feeds_[i];
            if (!state.config.enabled || state.config.url.empty()) {
                continue;
            }

            const FeedSeconds interval = NormalizePollIntervalSeconds(state.config.pollIntervalSeconds);
            if (state.hasPolled && (now - state.lastPollTime) < interval) {
                continue;
            }

            state.lastPollTime = now;
            state.hasPolled = true;
            feedsToPoll.emplace_back(i, state.config);
        }
    }

    for (const auto& pendingFeed : feedsToPoll) {
        const std::size_t feedIndex = pendingFeed.first;
        const RssFeedConfig& feedConfig = pendingFeed.second;

        std::string body;
        std::string lastModified;
        std::string etag;
        unsigned long statusCode = 0;
        if (!httpClient_.Fetch(feedConfig.url,
                               body,
                               feedConfig.lastModified,
                               feedConfig.etag,
                               lastModified,
                               etag,
                               statusCode)) {
            Logger::Warning("[RssFeedManager] Fetch failed for feed '%s' (%s)",
                            feedConfig.label.c_str(),
                            feedConfig.url.c_str());
            continue;
        }

        if (statusCode == 304) {
            std::lock_guard<std::mutex> lock(feedsMutex_);
            if (feedIndex < feeds_.size() && feeds_[feedIndex].config.url == feedConfig.url) {
                if (!lastModified.empty()) {
                    feeds_[feedIndex].config.lastModified = lastModified;
                }
                if (!etag.empty()) {
                    feeds_[feedIndex].config.etag = etag;
                }
            }
            continue;
        }

        if (statusCode != 200) {
            Logger::Warning("[RssFeedManager] Feed '%s' returned HTTP %lu",
                            feedConfig.url.c_str(),
                            statusCode);
            continue;
        }

        std::vector<RssFeedItem> items = parser_.Parse(body);
        if (items.empty()) {
            std::lock_guard<std::mutex> lock(feedsMutex_);
            if (feedIndex < feeds_.size() && feeds_[feedIndex].config.url == feedConfig.url) {
                feeds_[feedIndex].config.lastModified = lastModified;
                feeds_[feedIndex].config.etag = etag;
            }
            continue;
        }

        std::vector<RssToastData> newMessages;
        {
            std::lock_guard<std::mutex> lock(feedsMutex_);
            if (feedIndex >= feeds_.size() || feeds_[feedIndex].config.url != feedConfig.url) {
                continue;
            }

            RssFeedConfig& liveFeed = feeds_[feedIndex].config;
            liveFeed.lastModified = lastModified;
            liveFeed.etag = etag;

            std::size_t skippedSeenCount = 0;
            for (const RssFeedItem& item : items) {
                const std::string baseKey = MakeItemBaseKey(item);
                const std::string guidKey = MakeItemGuidKey(item);
                if (baseKey.empty() || guidKey.empty()) {
                    Logger::Warning("[RssFeedManager] Skipping RSS item without GUID/link/title for feed '%s'",
                                    liveFeed.url.c_str());
                    continue;
                }

                const auto seenIt = std::find_if(liveFeed.seen_guids.begin(),
                                                 liveFeed.seen_guids.end(),
                                                 [&](const RssSeenGuid& seen) {
                                                     return seen.guid == guidKey;
                                                 });
                if (seenIt != liveFeed.seen_guids.end()) {
                    ++skippedSeenCount;
                    continue;
                }

                const auto legacySeenIt = std::find_if(liveFeed.seen_guids.begin(),
                                                       liveFeed.seen_guids.end(),
                                                       [&](const RssSeenGuid& seen) {
                                                           return seen.guid == baseKey;
                                                       });
                const bool legacyAlreadyCoversThisVersion =
                    legacySeenIt != liveFeed.seen_guids.end()
                    && (item.pubDateEpoch == 0
                        || legacySeenIt->seenAt == 0
                        || item.pubDateEpoch <= legacySeenIt->seenAt);

                RssSeenGuid seenGuid;
                seenGuid.guid = guidKey;
                seenGuid.seenAt = std::time(nullptr);
                liveFeed.seen_guids.push_back(std::move(seenGuid));
                TrimSeenGuids(liveFeed);

                if (legacyAlreadyCoversThisVersion) {
                    ++skippedSeenCount;
                    continue;
                }

                RssToastData toastData;
                toastData.feedName = liveFeed.label.empty() ? "RSS Feed" : liveFeed.label;
                toastData.title = !item.title.empty() ? item.title : (!item.link.empty() ? item.link : item.guid);
                toastData.author = item.author;
                newMessages.push_back(std::move(toastData));
            }

            Logger::Info("[RssFeedManager] Feed '%s' poll complete: HTTP %lu, parsed=%zu, new=%zu, seen=%zu",
                         liveFeed.label.empty() ? liveFeed.url.c_str() : liveFeed.label.c_str(),
                         statusCode,
                         items.size(),
                         newMessages.size(),
                         skippedSeenCount);
        }

        if (!newMessages.empty()) {
            std::lock_guard<std::mutex> lock(newItemsMutex_);
            for (RssToastData& message : newMessages) {
                newItems_.push_back(std::move(message));
            }
        }
    }
}

std::vector<RssToastData> RssFeedManager::ConsumeNewItems() {
    std::lock_guard<std::mutex> lock(newItemsMutex_);
    std::vector<RssToastData> items;
    items.swap(newItems_);
    return items;
}

void RssFeedManager::UpdateSettings() {
    std::unordered_map<std::string, FeedRuntimeState> existingFeeds;
    {
        std::lock_guard<std::mutex> lock(feedsMutex_);
        existingFeeds.reserve(feeds_.size());
        for (FeedRuntimeState& feed : feeds_) {
            existingFeeds.emplace(MakeFeedKey(feed.config), std::move(feed));
        }
        feeds_.clear();

        rssEnabled_ = settingsManager_.Get().rss_enabled;
        feeds_.reserve(settingsManager_.GetRssFeeds().size());
        for (const RssFeedConfig& settingsFeed : settingsManager_.GetRssFeeds()) {
            FeedRuntimeState updatedFeed;
            auto existingIt = existingFeeds.find(MakeFeedKey(settingsFeed));
            if (existingIt != existingFeeds.end()) {
                updatedFeed = std::move(existingIt->second);
                updatedFeed.config.url = settingsFeed.url;
                updatedFeed.config.label = settingsFeed.label;
                updatedFeed.config.pollIntervalSeconds = settingsFeed.pollIntervalSeconds;
                updatedFeed.config.enabled = settingsFeed.enabled;
            } else {
                updatedFeed.config = settingsFeed;
            }

            TrimSeenGuids(updatedFeed.config);
            feeds_.push_back(std::move(updatedFeed));
        }
    }

    Logger::Info("[RssFeedManager] Settings updated with %zu feed(s), rss_enabled=%s",
                 settingsManager_.GetRssFeeds().size(),
                 settingsManager_.Get().rss_enabled ? "true" : "false");
}

#ifdef KEYBINDER_TESTING
void RssFeedManager::TestHook_ForceFeedEligible(std::size_t feedIndex) {
    std::lock_guard<std::mutex> lock(feedsMutex_);
    if (feedIndex >= feeds_.size()) {
        return;
    }

    feeds_[feedIndex].hasPolled = false;
    feeds_[feedIndex].lastPollTime = FeedClock::time_point{};
}

std::vector<RssFeedConfig> RssFeedManager::TestHook_CopyFeedConfigs() const {
    std::lock_guard<std::mutex> lock(feedsMutex_);

    std::vector<RssFeedConfig> configs;
    configs.reserve(feeds_.size());
    for (const FeedRuntimeState& feed : feeds_) {
        configs.push_back(feed.config);
    }

    return configs;
}
#endif

void RssFeedManager::WorkerLoop() {
    while (running_.load()) {
        try {
            PollFeeds();
        } catch (const std::exception& ex) {
            Logger::Error("[RssFeedManager] PollFeeds exception: %s", ex.what());
        } catch (...) {
            Logger::Error("[RssFeedManager] PollFeeds exception: unknown error");
        }

        for (int i = 0; i < 10 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

std::string RssFeedManager::MakeItemBaseKey(const RssFeedItem& item) {
    if (!item.guid.empty()) {
        return item.guid;
    }
    if (!item.link.empty()) {
        return item.link;
    }
    return item.title;
}

std::string RssFeedManager::MakeItemGuidKey(const RssFeedItem& item) {
    const std::string baseKey = MakeItemBaseKey(item);
    if (baseKey.empty()) {
        return {};
    }

    if (item.pubDateEpoch > 0) {
        return baseKey + "#pub=" + std::to_string(static_cast<long long>(item.pubDateEpoch));
    }
    if (!item.pubDate.empty()) {
        return baseKey + "#date=" + item.pubDate;
    }
    if (!item.slashComments.empty()) {
        return baseKey + "#comments=" + item.slashComments;
    }
    return baseKey;
}

std::string RssFeedManager::BuildToastMessage(const RssFeedConfig& feed, const RssFeedItem& item) {
    // Format: "Title by Author" as specified in plan
    const std::string itemTitle = !item.title.empty() ? item.title : (!item.link.empty() ? item.link : item.guid);
    const std::string itemAuthor = !item.author.empty() ? item.author : "";

    if (itemTitle.empty() && itemAuthor.empty()) {
        return "New post";
    }

    if (itemAuthor.empty()) {
        return itemTitle;
    }

    if (itemTitle.empty()) {
        return "New post by " + itemAuthor;
    }

    return itemTitle + " by " + itemAuthor;
}

void RssFeedManager::TrimSeenGuids(RssFeedConfig& feed) {
    if (feed.seen_guids.size() <= kMaxSeenGuidsPerFeed) {
        return;
    }

    const std::size_t overflow = feed.seen_guids.size() - kMaxSeenGuidsPerFeed;
    feed.seen_guids.erase(feed.seen_guids.begin(), feed.seen_guids.begin() + static_cast<std::ptrdiff_t>(overflow));
}

} // namespace keybinder
