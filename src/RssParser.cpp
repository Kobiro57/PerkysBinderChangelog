#include "../include/RssParser.h"

#include "../include/Logger.h"
#include "../include/pugixml/pugixml.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <stdio.h>

namespace keybinder {
namespace {

constexpr std::size_t kMaxRssXmlBytes = 1024 * 1024;
constexpr std::size_t kMaxRssItemCount = 100;
constexpr std::size_t kMaxRssFieldLength = 64 * 1024;

std::string Trim(const std::string& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });

    if (begin == value.end()) {
        return std::string();
    }

    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    return std::string(begin, end);
}

std::string ToUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string GetChildText(const pugi::xml_node& parent, const char* childName) {
    const pugi::xml_node child = parent.child(childName);
    if (!child) {
        return std::string();
    }

    std::string value = child.text().as_string();
    if (value.size() > kMaxRssFieldLength) {
        value.resize(kMaxRssFieldLength);
    }

    return value;
}

int ParseMonth(const std::string& monthToken) {
    const std::string month = ToUpper(monthToken);

    if (month == "JAN") return 0;
    if (month == "FEB") return 1;
    if (month == "MAR") return 2;
    if (month == "APR") return 3;
    if (month == "MAY") return 4;
    if (month == "JUN") return 5;
    if (month == "JUL") return 6;
    if (month == "AUG") return 7;
    if (month == "SEP") return 8;
    if (month == "OCT") return 9;
    if (month == "NOV") return 10;
    if (month == "DEC") return 11;

    return -1;
}

int ParseTimezoneOffsetSeconds(const std::string& timezoneToken) {
    if (timezoneToken.empty()) {
        return 0;
    }

    if (timezoneToken.size() == 5 && (timezoneToken[0] == '+' || timezoneToken[0] == '-')) {
        const bool hasOnlyDigits = std::all_of(timezoneToken.begin() + 1, timezoneToken.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });

        if (!hasOnlyDigits) {
            return 0;
        }

        const int sign = timezoneToken[0] == '-' ? -1 : 1;
        const int hours = ((timezoneToken[1] - '0') * 10) + (timezoneToken[2] - '0');
        const int minutes = ((timezoneToken[3] - '0') * 10) + (timezoneToken[4] - '0');
        return sign * ((hours * 3600) + (minutes * 60));
    }

    const std::string timezone = ToUpper(timezoneToken);

    if (timezone == "UT" || timezone == "UTC" || timezone == "GMT") return 0;
    if (timezone == "EST") return -5 * 3600;
    if (timezone == "EDT") return -4 * 3600;
    if (timezone == "CST") return -6 * 3600;
    if (timezone == "CDT") return -5 * 3600;
    if (timezone == "MST") return -7 * 3600;
    if (timezone == "MDT") return -6 * 3600;
    if (timezone == "PST") return -8 * 3600;
    if (timezone == "PDT") return -7 * 3600;

    return 0;
}

std::time_t MakeUtcTime(std::tm* utcTime) {
#if defined(_WIN32)
    return _mkgmtime(utcTime);
#else
    return timegm(utcTime);
#endif
}

} // namespace

std::vector<RssFeedItem> RssParser::Parse(const std::string& xmlContent) {
    std::vector<RssFeedItem> items;

    if (xmlContent.empty()) {
        Logger::Warning("[RssParser] RSS XML content is empty");
        return items;
    }

    if (xmlContent.size() > kMaxRssXmlBytes) {
        Logger::Warning("[RssParser] RSS XML exceeds size limit (%zu bytes)", xmlContent.size());
        return items;
    }

    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(xmlContent.data(), xmlContent.size());
    if (!result) {
        Logger::Warning("[RssParser] Failed to parse RSS XML: %s", result.description());
        return items;
    }

    const pugi::xml_node rssNode = document.child("rss");
    if (!rssNode) {
        Logger::Warning("[RssParser] Missing rss root element");
        return items;
    }

    const pugi::xml_node channelNode = rssNode.child("channel");
    if (!channelNode) {
        Logger::Warning("[RssParser] Missing channel element");
        return items;
    }

    for (pugi::xml_node itemNode = channelNode.child("item"); itemNode; itemNode = itemNode.next_sibling("item")) {
        if (items.size() >= kMaxRssItemCount) {
            Logger::Warning("[RssParser] RSS item count exceeds limit (%zu)", kMaxRssItemCount);
            break;
        }

        RssFeedItem item;
        item.title = GetChildText(itemNode, "title");
        item.link = GetChildText(itemNode, "link");
        item.guid = GetChildText(itemNode, "guid");
        item.pubDate = GetChildText(itemNode, "pubDate");
        item.author = GetChildText(itemNode, "dc:creator");
        item.category = GetChildText(itemNode, "category");
        item.contentEncoded = GetChildText(itemNode, "content:encoded");
        item.slashComments = GetChildText(itemNode, "slash:comments");
        item.pubDateEpoch = ParseRfc822Date(item.pubDate);
        items.push_back(item);
    }

    return items;
}

std::time_t RssParser::ParseRfc822Date(const std::string& dateString) {
    const std::string trimmed = Trim(dateString);
    if (trimmed.empty()) {
        return 0;
    }

    std::istringstream stream(trimmed);
    std::string dayToken;
    if (!(stream >> dayToken)) {
        return 0;
    }

    if (!dayToken.empty() && dayToken.back() == ',') {
        if (!(stream >> dayToken)) {
            return 0;
        }
    }

    int day = 0;
    if (sscanf_s(dayToken.c_str(), "%d", &day) != 1) {
        return 0;
    }

    std::string monthToken;
    std::string timeToken;
    std::string timezoneToken;
    int year = 0;

    if (!(stream >> monthToken >> year >> timeToken)) {
        return 0;
    }

    if (!(stream >> timezoneToken)) {
        timezoneToken = "GMT";
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    const int parsedTimeParts = sscanf_s(timeToken.c_str(), "%d:%d:%d", &hour, &minute, &second);
    if (parsedTimeParts < 2) {
        return 0;
    }
    if (parsedTimeParts == 2) {
        second = 0;
    }

    const int month = ParseMonth(monthToken);
    if (month < 0) {
        return 0;
    }

    std::tm utcTime = {};
    utcTime.tm_mday = day;
    utcTime.tm_mon = month;
    utcTime.tm_year = year - 1900;
    utcTime.tm_hour = hour;
    utcTime.tm_min = minute;
    utcTime.tm_sec = second;
    utcTime.tm_isdst = -1;

    const std::time_t baseUtc = MakeUtcTime(&utcTime);
    if (baseUtc == static_cast<std::time_t>(-1)) {
        return 0;
    }

    return baseUtc - ParseTimezoneOffsetSeconds(timezoneToken);
}

} // namespace keybinder
