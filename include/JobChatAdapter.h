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

struct JobChatAdapterState {
    bool isAlive = false;
    JobChatEventCallback callback;
};

class JobChatAdapter {
public:
    JobChatAdapter();
    ~JobChatAdapter();

    bool Initialize();
    void Shutdown();

    void SetCallback(JobChatEventCallback callback);
    bool IsInitialized() const { return initialized_; }

private:
    bool initialized_;
    std::shared_ptr<JobChatAdapterState> state_;
};

} // namespace jobs
