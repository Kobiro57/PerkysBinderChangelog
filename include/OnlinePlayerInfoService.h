#pragma once

#include "RssHttpClient.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace keybinder {

struct OnlinePlayerInfo {
    std::string id;
    std::string name;
    int level = 0;
    bool hasFaction = false;
    std::string faction;
    bool hasGang = false;
    std::string gang;
    int skinId = 0;
    bool hasTeamRank = false;
    std::string teamRank;
    int wanted = 0;  // optional API field; defaults to 0 when absent or null
};

struct OnlinePlayerLookupResult {
    enum class Status {
        Success,
        NotFound,
        NetworkError,
        ParseError,
    };

    Status status = Status::NetworkError;
    std::string requestedName;
    std::string chatMessage;
};

class OnlinePlayerInfoService {
public:
    OnlinePlayerInfoService();
    ~OnlinePlayerInfoService();

    void Initialize();
    void Shutdown();
    void QueueLookup(const std::string& username);
    std::vector<OnlinePlayerLookupResult> ConsumeCompletedLookups();

    OnlinePlayerLookupResult LookupNow(const std::string& username) const;

    static std::string NormalizeLookupName(const std::string& username);
    static bool ParsePlayersResponse(const std::string& response,
                                     std::vector<OnlinePlayerInfo>& outPlayers,
                                     std::string* parseError = nullptr);
    static bool FindPlayerByName(const std::vector<OnlinePlayerInfo>& players,
                                 const std::string& username,
                                 OnlinePlayerInfo& outPlayer);
    static std::string BuildChatMessage(const OnlinePlayerInfo& player);

private:
    void WorkerLoop();
    void PushCompletedResult(const OnlinePlayerLookupResult& result);

    RssHttpClient httpClient_;

    mutable std::mutex requestMutex_;
    std::condition_variable requestCv_;
    std::vector<std::string> pendingUsernames_;
    bool running_ = false;
    std::thread workerThread_;

    mutable std::mutex completedMutex_;
    std::vector<OnlinePlayerLookupResult> completedLookups_;
};

} // namespace keybinder
