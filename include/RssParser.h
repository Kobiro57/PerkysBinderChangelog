#pragma once

#include "RssTypes.h"

#include <ctime>
#include <string>
#include <vector>

namespace keybinder {

class RssParser {
public:
    std::vector<RssFeedItem> Parse(const std::string& xmlContent);
    static std::time_t ParseRfc822Date(const std::string& dateString);
};

} // namespace keybinder
