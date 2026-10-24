#pragma once

#include <string>

namespace keybinder {

class RssHttpClient {
public:
#ifdef KEYBINDER_TESTING
    using TestFetchHandler = bool(*)(const std::string& url,
                                     std::string& outBody,
                                     const std::string& ifModifiedSince,
                                     const std::string& ifNoneMatch,
                                     std::string& outLastModified,
                                     std::string& outEtag,
                                     unsigned long& outStatusCode);

    static void SetTestFetchHandler(TestFetchHandler handler);
    static void ResetTestFetchHandler();
#endif

    bool Fetch(const std::string& url,
               std::string& outBody,
               const std::string& ifModifiedSince,
               const std::string& ifNoneMatch,
               std::string& outLastModified,
               std::string& outEtag,
               unsigned long& outStatusCode) const;
};

} // namespace keybinder
