#pragma once

#include "GpsManager.h"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace keybinder {

struct SprayDistance {
    unsigned int id = 0;
    const char* place = "";
    float distance = 0.0f;
};

class SpraysNavigator {
public:
    using Notice = std::function<void(const std::string&)>;
    using PositionProvider = GpsManager::PositionProvider;
    // The caller verifies the current live police role and returns a host /
    // port / account identity, or an empty string when access is not permitted.
    using SessionProvider = std::function<std::string()>;
    using AccessProvider = SessionProvider;
    static constexpr std::size_t kRowsPerPage = 34;

    void Initialize(GpsManager* gps, Notice notice, PositionProvider position, SessionProvider session);
    void Shutdown();
    bool Open();
    bool Toggle();
    void Close() { open_ = false; }
    bool IsOpen() const { return open_; }
    void Update();
    void Render();
    bool Select(unsigned int id);
    void SetPage(std::size_t page);
    std::size_t Page() const { return page_; }
    std::size_t PageCount() const;
    const std::vector<SprayDistance>& Rows() const { return rows_; }
    static std::vector<SprayDistance> BuildRows(const GpsPosition& position);

private:
    bool EnsureAccess(bool notify);
    bool ReadPosition(GpsPosition& position) const;
    bool OwnsActiveRoute() const;
    void StopOwnedRoute();
    void Say(const std::string& text) const;
    GpsManager* gps_ = nullptr;
    Notice notice_;
    PositionProvider positionProvider_;
    SessionProvider sessionProvider_;
    std::string session_;
    std::vector<SprayDistance> rows_;
    unsigned int ownedDestinationId_ = 0;
    std::size_t page_ = 0;
    bool open_ = false;
    bool resetScroll_ = false;
};

} // namespace keybinder
