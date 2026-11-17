#pragma once

#include "GpsManager.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace keybinder {

// A local nearest-next navigation tour. The caller supplies a verified role /
// session identity and current configured points; no server actions are sent.
class WaypointTour {
public:
    using Notice = GpsManager::Notice;
    using PositionProvider = GpsManager::PositionProvider;
    using SessionProvider = std::function<std::string()>;
    // The referenced list must remain valid for the duration of each call.
    using PointsProvider = std::function<const std::vector<GpsEntry>&()>;

    void Initialize(GpsManager* gps, std::string title, std::string stopCommand,
        std::string deniedMessage, Notice notice, PositionProvider position,
        SessionProvider session, PointsProvider points);
    bool Start();
    void Stop(bool notify = true);
    bool IsActive() const { return active_; }
    void Update();
    void OnArrival(const GpsEntry& destination, std::uint64_t generation);
    void Shutdown();

private:
    bool ReadOutsidePosition(GpsPosition& position) const;
    bool OwnsRoute() const;
    bool CheckSession();
    bool NextPoint(const GpsPosition& position, GpsEntry& result) const;
    bool NavigateTo(const GpsEntry& point);
    void Advance();
    void ClearState();
    void EndForReplacement();
    void Say(const std::string& text) const;

    GpsManager* gps_ = nullptr;
    std::string title_, stopCommand_, deniedMessage_;
    Notice notice_;
    PositionProvider positionProvider_;
    SessionProvider sessionProvider_;
    PointsProvider pointsProvider_;
    std::string session_;
    std::unordered_set<unsigned int> visited_;
    unsigned int sourceId_ = 0, destinationId_ = 0;
    std::uint64_t generation_ = 0;
    bool active_ = false, pendingNext_ = false;
};

} // namespace keybinder
