#define KEYBINDER_TESTING

#include "../include/Logger.h"
#include "../include/RssFeedManager.h"
#include "../include/RssHttpClient.h"
#include "../include/RssParser.h"
#include "../include/SettingsManager.h"

#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using namespace keybinder;

struct TestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

struct XmlItemData {
    std::string title;
    std::string link;
    std::string guid;
    std::string pubDate;
    std::string author;
    std::string category;
    std::string contentEncoded;
    std::string slashComments;
};

struct MockFetchResponse {
    bool success = true;
    std::string body;
    std::string lastModified;
    std::string etag;
    unsigned long statusCode = 200;
};

TestState g_state;
std::unordered_map<std::string, MockFetchResponse> g_mockResponses;

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        g_state.error = "Assertion failed: " #condition; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define ASSERT_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        g_state.error = "Assertion failed: " #actual " == " #expected; \
        return false; \
    } \
    ++g_state.passed; \
} while (0)

#define RUN_TEST(test_name) do { \
    g_state.error.clear(); \
    if (test_name()) { \
        std::printf("[PASS] %s\n", #test_name); \
    } else { \
        ++g_state.failed; \
        std::printf("[FAIL] %s: %s\n", #test_name, g_state.error.c_str()); \
    } \
} while (0)

bool ContainsText(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool HasSeenGuid(const RssFeedConfig& feed, const std::string& guid) {
    for (const RssSeenGuid& seen : feed.seen_guids) {
        if (seen.guid == guid) {
            return true;
        }
    }

    return false;
}

std::string BuildFeedXml(const std::vector<XmlItemData>& items) {
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<rss version=\"2.0\" "
        "xmlns:content=\"http://purl.org/rss/1.0/modules/content/\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:slash=\"http://purl.org/rss/1.0/modules/slash/\">"
        "<channel><title><![CDATA[Test Feed]]></title>";

    for (const XmlItemData& item : items) {
        xml += "<item>";
        if (!item.title.empty()) {
            xml += "<title><![CDATA[" + item.title + "]]></title>";
        }
        if (!item.link.empty()) {
            xml += "<link>" + item.link + "</link>";
        }
        if (!item.guid.empty()) {
            xml += "<guid isPermaLink=\"false\">" + item.guid + "</guid>";
        }
        if (!item.pubDate.empty()) {
            xml += "<pubDate>" + item.pubDate + "</pubDate>";
        }
        if (!item.author.empty()) {
            xml += "<dc:creator><![CDATA[" + item.author + "]]></dc:creator>";
        }
        if (!item.category.empty()) {
            xml += "<category><![CDATA[" + item.category + "]]></category>";
        }
        if (!item.contentEncoded.empty()) {
            xml += "<content:encoded><![CDATA[" + item.contentEncoded + "]]></content:encoded>";
        }
        if (!item.slashComments.empty()) {
            xml += "<slash:comments>" + item.slashComments + "</slash:comments>";
        }
        xml += "</item>";
    }

    xml += "</channel></rss>";
    return xml;
}

bool MockFetch(const std::string& url,
               std::string& outBody,
               const std::string& ifModifiedSince,
               const std::string& ifNoneMatch,
               std::string& outLastModified,
               std::string& outEtag,
               unsigned long& outStatusCode) {
    (void)ifModifiedSince;
    (void)ifNoneMatch;

    const auto it = g_mockResponses.find(url);
    if (it == g_mockResponses.end()) {
        return false;
    }

    const MockFetchResponse& response = it->second;
    outBody = response.body;
    outLastModified = response.lastModified;
    outEtag = response.etag;
    outStatusCode = response.statusCode;
    return response.success;
}

struct FetchHookScope {
    FetchHookScope() {
        g_mockResponses.clear();
        RssHttpClient::SetTestFetchHandler(&MockFetch);
    }

    ~FetchHookScope() {
        RssHttpClient::ResetTestFetchHandler();
        g_mockResponses.clear();
    }
};

struct LogCapture {
    static LogCapture* active;
    std::vector<std::pair<LogLevel, std::string>> entries;

    static void Sink(LogLevel level, const char* message) {
        if (active == nullptr) {
            return;
        }

        active->entries.emplace_back(level, message != nullptr ? message : "");
    }

    LogCapture() {
        active = this;
        Logger::SetTestSink(&LogCapture::Sink);
    }

    ~LogCapture() {
        Logger::ResetTestSink();
        active = nullptr;
    }
};

LogCapture* LogCapture::active = nullptr;

bool Test_ParseValidWoltLabFeed() {
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<rss version=\"2.0\" "
        "xmlns:content=\"http://purl.org/rss/1.0/modules/content/\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:slash=\"http://purl.org/rss/1.0/modules/slash/\">"
        "<channel>"
        "<title><![CDATA[WoltLab Suite Forum]]></title>"
        "<item>"
        "<title><![CDATA[Release 1.2.3]]></title>"
        "<link>https://community.example.com/thread/123-release-1-2-3/</link>"
        "<guid isPermaLink=\"false\">com.woltlab.wcf.thread-123</guid>"
        "<pubDate>Mon, 24 Mar 2025 14:30:45 GMT</pubDate>"
        "<dc:creator><![CDATA[Alice Admin]]></dc:creator>"
        "<category><![CDATA[Announcements]]></category>"
        "<content:encoded><![CDATA[<p>Patch notes &amp; details.</p>]]></content:encoded>"
        "<slash:comments>5</slash:comments>"
        "</item>"
        "</channel>"
        "</rss>";

    RssParser parser;
    const std::vector<RssFeedItem> items = parser.Parse(xml);

    ASSERT_EQ(items.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(items[0].title, std::string("Release 1.2.3"));
    ASSERT_EQ(items[0].link, std::string("https://community.example.com/thread/123-release-1-2-3/"));
    ASSERT_EQ(items[0].guid, std::string("com.woltlab.wcf.thread-123"));
    ASSERT_EQ(items[0].pubDate, std::string("Mon, 24 Mar 2025 14:30:45 GMT"));
    ASSERT_EQ(items[0].author, std::string("Alice Admin"));
    ASSERT_EQ(items[0].category, std::string("Announcements"));
    ASSERT_EQ(items[0].contentEncoded, std::string("<p>Patch notes &amp; details.</p>"));
    ASSERT_EQ(items[0].slashComments, std::string("5"));
    ASSERT_EQ(items[0].pubDateEpoch, RssParser::ParseRfc822Date("Mon, 24 Mar 2025 14:30:45 GMT"));
    return true;
}

bool Test_ParseMalformedXml() {
    LogCapture logs;
    RssParser parser;
    const std::vector<RssFeedItem> items = parser.Parse("<rss><channel><item><title>Broken</title></channel>");

    ASSERT_TRUE(items.empty());
    ASSERT_TRUE(!logs.entries.empty());
    ASSERT_EQ(logs.entries.back().first, LogLevel::Warning);
    ASSERT_TRUE(ContainsText(logs.entries.back().second, "Failed to parse RSS XML"));
    return true;
}

bool Test_ParseMissingOptionalFields() {
    const std::string xml = BuildFeedXml({
        {
            "No Optional Fields",
            "https://example.com/thread/optional-less",
            "guid-missing-optional",
            "Tue, 25 Mar 2025 10:00:00 GMT",
            "",
            "",
            "",
            ""
        }
    });

    RssParser parser;
    const std::vector<RssFeedItem> items = parser.Parse(xml);

    ASSERT_EQ(items.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(items[0].title, std::string("No Optional Fields"));
    ASSERT_EQ(items[0].author, std::string());
    ASSERT_EQ(items[0].category, std::string());
    ASSERT_EQ(items[0].contentEncoded, std::string());
    ASSERT_EQ(items[0].slashComments, std::string());
    ASSERT_TRUE(items[0].pubDateEpoch != 0);
    return true;
}

bool Test_ParseRfc822Date() {
    const std::time_t canonical = RssParser::ParseRfc822Date("Mon, 24 Mar 2025 14:30:45 GMT");

    ASSERT_TRUE(canonical != 0);
    ASSERT_EQ(canonical, RssParser::ParseRfc822Date("24 Mar 2025 14:30:45 GMT"));
    ASSERT_EQ(canonical, RssParser::ParseRfc822Date("24 Mar 2025 15:30:45 +0100"));
    ASSERT_EQ(canonical, RssParser::ParseRfc822Date("24 Mar 2025 10:30:45 EDT"));
    ASSERT_EQ(RssParser::ParseRfc822Date("24 Mar 2025 14:30 GMT"), RssParser::ParseRfc822Date("24 Mar 2025 14:30:00 GMT"));
    ASSERT_EQ(RssParser::ParseRfc822Date("24 Foo 2025 14:30 GMT"), static_cast<std::time_t>(0));
    return true;
}

bool Test_ParseEmptyFeed() {
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<rss version=\"2.0\"><channel><title>Empty Feed</title></channel></rss>";

    RssParser parser;
    const std::vector<RssFeedItem> items = parser.Parse(xml);

    ASSERT_TRUE(items.empty());
    return true;
}

bool Test_DedupNewItems() {
    FetchHookScope fetchScope;

    SettingsManager settings;
    settings.GetMutable().rss_enabled = true;

    RssFeedConfig feed;
    feed.url = "https://example.com/forum.xml";
    feed.label = "Forum";
    feed.enabled = true;
    feed.pollIntervalSeconds = 300;
    settings.SetRssFeeds({ feed });

    g_mockResponses[feed.url] = {
        true,
        BuildFeedXml({
            { "First thread", "https://example.com/thread/1", "guid-1", "Mon, 24 Mar 2025 14:30:45 GMT", "Alice", "Announcements", "<p>One</p>", "1" },
            { "Second thread", "https://example.com/thread/2", "guid-2", "Mon, 24 Mar 2025 15:30:45 GMT", "Bob", "Announcements", "<p>Two</p>", "2" }
        }),
        "Mon, 24 Mar 2025 16:00:00 GMT",
        "etag-forum",
        200
    };

    RssFeedManager manager(settings);
    manager.UpdateSettings();
    manager.PollFeeds();

    const std::vector<keybinder::RssToastData> firstBatch = manager.ConsumeNewItems();
    ASSERT_EQ(firstBatch.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(firstBatch[0].title, std::string("First thread"));
    ASSERT_EQ(firstBatch[0].author, std::string("Alice"));
    ASSERT_EQ(firstBatch[1].title, std::string("Second thread"));
    ASSERT_EQ(firstBatch[1].author, std::string("Bob"));

    const std::vector<RssFeedConfig> firstConfigs = manager.TestHook_CopyFeedConfigs();
    ASSERT_EQ(firstConfigs.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(firstConfigs[0].seen_guids.size(), static_cast<std::size_t>(2));

    manager.TestHook_ForceFeedEligible(0);
    manager.PollFeeds();

    const std::vector<keybinder::RssToastData> secondBatch = manager.ConsumeNewItems();
    ASSERT_TRUE(secondBatch.empty());

    const std::vector<RssFeedConfig> secondConfigs = manager.TestHook_CopyFeedConfigs();
    ASSERT_EQ(secondConfigs[0].seen_guids.size(), static_cast<std::size_t>(2));
    return true;
}

bool Test_ThreadReplyUpdateDetectedForLegacySeenGuid() {
    FetchHookScope fetchScope;

    SettingsManager settings;
    settings.GetMutable().rss_enabled = true;

    RssFeedConfig feed;
    feed.url = "https://example.com/thread-updates.xml";
    feed.label = "Thread Updates";
    feed.enabled = true;
    feed.pollIntervalSeconds = 300;

    RssSeenGuid legacySeen;
    legacySeen.guid = "https://example.com/thread/56";
    legacySeen.seenAt = RssParser::ParseRfc822Date("Mon, 24 Mar 2025 14:30:45 GMT");
    feed.seen_guids.push_back(legacySeen);

    settings.SetRssFeeds({ feed });

    g_mockResponses[feed.url] = {
        true,
        BuildFeedXml({
            { "Existing thread with new reply", "https://example.com/thread/56", "https://example.com/thread/56", "Mon, 24 Mar 2025 15:30:45 GMT", "Alice", "Announcements", "<p>Reply</p>", "42" }
        }),
        "Mon, 24 Mar 2025 16:00:00 GMT",
        "etag-thread-update",
        200
    };

    RssFeedManager manager(settings);
    manager.UpdateSettings();
    manager.PollFeeds();

    const std::vector<keybinder::RssToastData> newItems = manager.ConsumeNewItems();
    ASSERT_EQ(newItems.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(newItems[0].title, std::string("Existing thread with new reply"));
    ASSERT_EQ(newItems[0].author, std::string("Alice"));

    const std::vector<RssFeedConfig> configs = manager.TestHook_CopyFeedConfigs();
    ASSERT_EQ(configs.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(configs[0].seen_guids.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(HasSeenGuid(configs[0], "https://example.com/thread/56"));
    ASSERT_TRUE(HasSeenGuid(configs[0], std::string("https://example.com/thread/56#pub=") + std::to_string(static_cast<long long>(RssParser::ParseRfc822Date("Mon, 24 Mar 2025 15:30:45 GMT")))));

    return true;
}

bool Test_LegacySeenGuidSuppressesUnchangedThread() {
    FetchHookScope fetchScope;

    SettingsManager settings;
    settings.GetMutable().rss_enabled = true;

    RssFeedConfig feed;
    feed.url = "https://example.com/unchanged-thread.xml";
    feed.label = "Unchanged Thread";
    feed.enabled = true;
    feed.pollIntervalSeconds = 300;

    const std::time_t originalSeenAt = RssParser::ParseRfc822Date("Mon, 24 Mar 2025 15:30:45 GMT");
    RssSeenGuid legacySeen;
    legacySeen.guid = "https://example.com/thread/56";
    legacySeen.seenAt = originalSeenAt;
    feed.seen_guids.push_back(legacySeen);

    settings.SetRssFeeds({ feed });

    g_mockResponses[feed.url] = {
        true,
        BuildFeedXml({
            { "Existing unchanged thread", "https://example.com/thread/56", "https://example.com/thread/56", "Mon, 24 Mar 2025 15:30:45 GMT", "Alice", "Announcements", "<p>Same</p>", "41" }
        }),
        "Mon, 24 Mar 2025 16:00:00 GMT",
        "etag-unchanged-thread",
        200
    };

    RssFeedManager manager(settings);
    manager.UpdateSettings();
    manager.PollFeeds();

    const std::vector<keybinder::RssToastData> newItems = manager.ConsumeNewItems();
    ASSERT_TRUE(newItems.empty());

    const std::vector<RssFeedConfig> configs = manager.TestHook_CopyFeedConfigs();
    ASSERT_EQ(configs.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(configs[0].seen_guids.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(HasSeenGuid(configs[0], std::string("https://example.com/thread/56#pub=") + std::to_string(static_cast<long long>(originalSeenAt))));

    return true;
}

bool Test_DedupGuidCap() {
    FetchHookScope fetchScope;

    SettingsManager settings;
    settings.GetMutable().rss_enabled = true;

    RssFeedConfig feed;
    feed.url = "https://example.com/capped.xml";
    feed.label = "Capped";
    feed.enabled = true;
    feed.pollIntervalSeconds = 300;
    for (int i = 1; i <= 100; ++i) {
        RssSeenGuid seen;
        seen.guid = "guid-" + std::to_string(i);
        seen.seenAt = static_cast<std::time_t>(i);
        feed.seen_guids.push_back(seen);
    }
    settings.SetRssFeeds({ feed });

    g_mockResponses[feed.url] = {
        true,
        BuildFeedXml({
            { "Newest thread", "https://example.com/thread/101", "guid-101", "Mon, 24 Mar 2025 17:30:45 GMT", "Carol", "News", "<p>Newest</p>", "0" }
        }),
        "Mon, 24 Mar 2025 18:00:00 GMT",
        "etag-capped",
        200
    };

    RssFeedManager manager(settings);
    manager.UpdateSettings();
    manager.PollFeeds();

    const std::vector<keybinder::RssToastData> newItems = manager.ConsumeNewItems();
    ASSERT_EQ(newItems.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(newItems[0].title, std::string("Newest thread"));
    ASSERT_EQ(newItems[0].author, std::string("Carol"));

    const std::vector<RssFeedConfig> configs = manager.TestHook_CopyFeedConfigs();
    ASSERT_EQ(configs.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(configs[0].seen_guids.size(), static_cast<std::size_t>(100));
    ASSERT_TRUE(!HasSeenGuid(configs[0], "guid-1"));
    ASSERT_TRUE(HasSeenGuid(configs[0], "guid-2"));
    ASSERT_TRUE(HasSeenGuid(configs[0], std::string("guid-101#pub=") + std::to_string(static_cast<long long>(RssParser::ParseRfc822Date("Mon, 24 Mar 2025 17:30:45 GMT")))));
    return true;
}

bool Test_DedupCrossFeedIsolation() {
    FetchHookScope fetchScope;

    SettingsManager settings;
    settings.GetMutable().rss_enabled = true;

    RssFeedConfig feedA;
    feedA.url = "https://example.com/feed-a.xml";
    feedA.label = "Feed A";
    feedA.enabled = true;
    feedA.pollIntervalSeconds = 300;

    RssFeedConfig feedB;
    feedB.url = "https://example.com/feed-b.xml";
    feedB.label = "Feed B";
    feedB.enabled = true;
    feedB.pollIntervalSeconds = 300;

    settings.SetRssFeeds({ feedA, feedB });

    const std::string sharedXml = BuildFeedXml({
        { "Shared GUID Item", "https://example.com/thread/shared", "shared-guid", "Mon, 24 Mar 2025 19:30:45 GMT", "Dana", "Shared", "<p>Shared</p>", "4" }
    });

    g_mockResponses[feedA.url] = { true, sharedXml, "Mon, 24 Mar 2025 20:00:00 GMT", "etag-a", 200 };
    g_mockResponses[feedB.url] = { true, sharedXml, "Mon, 24 Mar 2025 20:00:00 GMT", "etag-b", 200 };

    RssFeedManager manager(settings);
    manager.UpdateSettings();
    manager.PollFeeds();

    const std::vector<keybinder::RssToastData> newItems = manager.ConsumeNewItems();
    ASSERT_EQ(newItems.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(newItems[0].title, std::string("Shared GUID Item"));
    ASSERT_EQ(newItems[0].author, std::string("Dana"));
    ASSERT_EQ(newItems[1].title, std::string("Shared GUID Item"));
    ASSERT_EQ(newItems[1].author, std::string("Dana"));

    const std::vector<RssFeedConfig> configs = manager.TestHook_CopyFeedConfigs();
    ASSERT_EQ(configs.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(configs[0].seen_guids.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(configs[1].seen_guids.size(), static_cast<std::size_t>(1));
    const std::string sharedVersionKey = std::string("shared-guid#pub=") + std::to_string(static_cast<long long>(RssParser::ParseRfc822Date("Mon, 24 Mar 2025 19:30:45 GMT")));
    ASSERT_TRUE(HasSeenGuid(configs[0], sharedVersionKey));
    ASSERT_TRUE(HasSeenGuid(configs[1], sharedVersionKey));
    return true;
}

bool Test_SettingsJsonRoundTrip() {
    // Simulate: user adds a feed, GetSettingsJson serializes it,
    // then LoadFromJson parses it back. Verify rss_feeds survives the round-trip.
    SettingsManager sm;

    // Build a settings JSON string that includes an rss_feeds entry
    std::string json = R"({
        "version": 1,
        "rss_enabled": true,
        "rss_notification_mode": "chat",
        "rss_feeds": [
            {"url": "https://forum.example.com/feed", "label": "Test Feed", "last_modified": "Wed, 01 Jan 2025 00:00:00 GMT", "etag": "\"abc123\"", "poll_interval_seconds": 300, "enabled": true, "seen_guids": [
                {"guid": "post-42", "seen_at": 1704067200}
            ]}
        ]
    })";

    ASSERT_TRUE(sm.LoadFromJson(json));
    ASSERT_EQ(sm.Get().rss_enabled, true);
    ASSERT_EQ(sm.Get().rss_notification_mode, RssNotificationMode::Chat);
    ASSERT_EQ(sm.Get().rss_feeds.size(), static_cast<std::size_t>(1));

    const RssFeedConfig& feed = sm.Get().rss_feeds[0];
    ASSERT_EQ(feed.url, std::string("https://forum.example.com/feed"));
    ASSERT_EQ(feed.label, std::string("Test Feed"));
    ASSERT_EQ(feed.lastModified, std::string("Wed, 01 Jan 2025 00:00:00 GMT"));
    ASSERT_EQ(feed.etag, std::string("\"abc123\""));
    ASSERT_EQ(feed.pollIntervalSeconds, 300);
    ASSERT_EQ(feed.enabled, true);
    ASSERT_EQ(feed.seen_guids.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(feed.seen_guids[0].guid, std::string("post-42"));
    ASSERT_EQ(feed.seen_guids[0].seenAt, static_cast<std::time_t>(1704067200));

    // Now serialize back to JSON
    std::string outputJson = sm.GetSettingsJson();

    // Parse the output JSON into a NEW SettingsManager
    SettingsManager sm2;
    ASSERT_TRUE(sm2.LoadFromJson(outputJson));
    ASSERT_EQ(sm2.Get().rss_enabled, true);
    ASSERT_EQ(sm2.Get().rss_notification_mode, RssNotificationMode::Chat);
    ASSERT_EQ(sm2.Get().rss_feeds.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(sm2.Get().rss_feeds[0].url, std::string("https://forum.example.com/feed"));
    ASSERT_EQ(sm2.Get().rss_feeds[0].label, std::string("Test Feed"));
    ASSERT_EQ(sm2.Get().rss_feeds[0].pollIntervalSeconds, 300);
    ASSERT_EQ(sm2.Get().rss_feeds[0].enabled, true);
    ASSERT_EQ(sm2.Get().rss_feeds[0].seen_guids.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(sm2.Get().rss_feeds[0].seen_guids[0].guid, std::string("post-42"));

    return true;
}

bool Test_SettingsJsonRoundTripEmptyFeeds() {
    // Verify that empty rss_feeds survives the round-trip
    SettingsManager sm;
    std::string json = R"({
        "version": 1,
        "rss_enabled": true,
        "rss_notification_mode": "chat",
        "rss_feeds": []
    })";

    ASSERT_TRUE(sm.LoadFromJson(json));
    ASSERT_EQ(sm.Get().rss_enabled, true);
    ASSERT_EQ(sm.Get().rss_notification_mode, RssNotificationMode::Chat);
    ASSERT_EQ(sm.Get().rss_feeds.size(), static_cast<std::size_t>(0));

    std::string outputJson = sm.GetSettingsJson();
    // The output should contain rss_enabled: true and rss_feeds: []
    ASSERT_TRUE(outputJson.find("rss_enabled") != std::string::npos);
    ASSERT_TRUE(outputJson.find("rss_feeds") != std::string::npos);

    SettingsManager sm2;
    ASSERT_TRUE(sm2.LoadFromJson(outputJson));
    ASSERT_EQ(sm2.Get().rss_enabled, true);
    ASSERT_EQ(sm2.Get().rss_notification_mode, RssNotificationMode::Chat);
    ASSERT_EQ(sm2.Get().rss_feeds.size(), static_cast<std::size_t>(0));

    return true;
}

bool Test_SettingsJsonRoundTripAddFeed() {
    // Simulate: empty start, add feed programmatically, serialize, reload
    SettingsManager sm;
    sm.LoadFromJson("{}");

    // Add a feed directly (mimicking RssTrackerUI::GetFeeds().push_back())
    RssFeedConfig feed;
    feed.url = "https://forum.example.com/rss";
    feed.label = "My Forum";
    feed.pollIntervalSeconds = 600;
    feed.enabled = true;
    sm.GetMutable().rss_feeds.push_back(feed);
    sm.GetMutable().rss_enabled = true;
    sm.GetMutable().rss_notification_mode = RssNotificationMode::Chat;

    ASSERT_EQ(sm.Get().rss_feeds.size(), static_cast<std::size_t>(1));

    std::string outputJson = sm.GetSettingsJson();

    SettingsManager sm2;
    ASSERT_TRUE(sm2.LoadFromJson(outputJson));
    ASSERT_EQ(sm2.Get().rss_enabled, true);
    ASSERT_EQ(sm2.Get().rss_notification_mode, RssNotificationMode::Chat);
    ASSERT_EQ(sm2.Get().rss_feeds.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(sm2.Get().rss_feeds[0].url, std::string("https://forum.example.com/rss"));
    ASSERT_EQ(sm2.Get().rss_feeds[0].label, std::string("My Forum"));
    ASSERT_EQ(sm2.Get().rss_feeds[0].pollIntervalSeconds, 600);
    ASSERT_EQ(sm2.Get().rss_feeds[0].enabled, true);

    return true;
}

} // namespace

int main() {
    std::printf("=== RssParserTests ===\n");

    RUN_TEST(Test_ParseValidWoltLabFeed);
    RUN_TEST(Test_ParseMalformedXml);
    RUN_TEST(Test_ParseMissingOptionalFields);
    RUN_TEST(Test_ParseRfc822Date);
    RUN_TEST(Test_ParseEmptyFeed);
    RUN_TEST(Test_DedupNewItems);
    RUN_TEST(Test_ThreadReplyUpdateDetectedForLegacySeenGuid);
    RUN_TEST(Test_LegacySeenGuidSuppressesUnchangedThread);
    RUN_TEST(Test_DedupGuidCap);
    RUN_TEST(Test_DedupCrossFeedIsolation);
    RUN_TEST(Test_SettingsJsonRoundTrip);
    RUN_TEST(Test_SettingsJsonRoundTripEmptyFeeds);
    RUN_TEST(Test_SettingsJsonRoundTripAddFeed);

    std::printf("RssParserTests: %d passed, %d failed\n", g_state.passed, g_state.failed);
    return g_state.failed;
}
