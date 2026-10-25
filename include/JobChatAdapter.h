#pragma once

#include <ctime>
#include <functional>
#include <memory>
#include <string>

namespace jobs {

struct JobChatEvent {
    std::string rawLine;
    std::string normalizedLine;
    std::string timestampFreeLine;
    std::string jobPrefix;
    std::time_t arrivalTime;

    JobChatEvent()
        : arrivalTime(0) {}
};

using JobChatEventCallback = std::function<void(const JobChatEvent&)>;
using JobChatVisibilityFilter = std::function<bool(const JobChatEvent&)>;

struct JobChatAdapterState {
    bool isAlive = false;
    JobChatEventCallback callback;
    JobChatVisibilityFilter visibilityFilter;
};

class JobChatAdapter {
public:
    JobChatAdapter();
    ~JobChatAdapter();

    bool Initialize();
    void Shutdown();

    void SetCallback(JobChatEventCallback callback);
    // Return false only for lines that should not reach the visible chat.
    // The normal event callback still receives hidden, non-empty lines.
    void SetVisibilityFilter(JobChatVisibilityFilter filter);
    bool IsInitialized() const { return initialized_; }

#ifdef KEYBINDER_TESTING
    bool DispatchLineForTesting(const char* prefix, const char* text) const;
#endif

private:
    bool initialized_;
    std::shared_ptr<JobChatAdapterState> state_;
};

} // namespace jobs
