#pragma once

#include <functional>
#include <string>
#include <vector>

class SAMPBridge;
namespace keybinder {
struct GpsPosition {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    int interior = 0;
};
struct GpsEntry {
    unsigned int id = 0;
    std::string name;
    GpsPosition position;
};

class GpsManager {
public:
    static constexpr std::size_t kMaximumEntries = 20;
    using Notice = std::function<void(const std::string&)>;
    using PositionProvider = std::function<bool(GpsPosition&)>;
    void Initialize(std::string path, Notice notice, PositionProvider provider);
    void Shutdown();
    bool Set(const std::string& name);
    bool Remove(unsigned int id);
    bool RemoveAll();
    bool Navigate(unsigned int id);
    void StopNavigation();
    void ShowList() { listOpen_ = !listOpen_; }
    void CloseList() { listOpen_ = false; }
    bool IsListOpen() const { return listOpen_; }
    void ShowHelp() const;
    void Update();
    void Render();
    const std::vector<GpsEntry>& Entries() const { return entries_; }
    const GpsEntry* ActiveDestination() const;
    static bool IsValidPosition(const GpsPosition& position);
    static bool HasArrived(const GpsPosition& current, const GpsPosition& target);
private:
    bool Load();
    bool Save() const;
    void Say(const std::string& text) const;
    std::string path_;
    Notice notice_;
    PositionProvider positionProvider_;
    std::vector<GpsEntry> entries_;
    unsigned int nextId_ = 1;
    unsigned int activeId_ = 0;
    unsigned int selectedId_ = 0;
    bool listOpen_ = false;
};

// Local GTA marker uses a private identifier and never replaces the server's
// SA-MP checkpoints, job targets or another mod's waypoint.
bool ReadGpsPlayerPosition(::SAMPBridge* bridge, GpsPosition& position);
void RenderGpsDestination(const GpsEntry* destination);
void ClearGpsDestinationMarker();
} // namespace keybinder
