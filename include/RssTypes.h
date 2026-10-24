#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace keybinder {

enum class RssNotificationMode {
    Toast = 0,
    Chat = 1
};

struct RssSeenGuid {
    std::string guid;
    std::time_t seenAt = 0;
};

struct RssFeedItem {
    std::string guid;
    std::string title;
    std::string link;
    std::string author;
    std::string pubDate;
    std::string category;
    std::string contentEncoded;
    std::string slashComments;
    std::time_t pubDateEpoch = 0;
};

struct RssFeedConfig {
    std::string url;
    std::string label;
    std::string lastModified;
    std::string etag;
    int pollIntervalSeconds = 300;
    bool enabled = true;
    std::vector<RssSeenGuid> seen_guids;
};

} // namespace keybinder
