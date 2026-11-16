#pragma once

#include <functional>
#include <optional>
#include <string>

class SAMPBridge;

namespace keybinder {

class OnlineDataManager;
struct OnlineSnapshot;

struct NearbyWantedLabelState {
    bool overlayEnabled = false;
    bool policeMember = false;
    bool connected = false;
    bool localActive = false;
    bool localAlive = false;
    bool streamed = false;
    bool targetAlive = false;
    bool serverAllowsLabels = false;
    bool remoteAllowsLabels = false;
    // Set only by the native DrawLabel call that draws this player's name.
    bool nativeNameTagVisible = false;
    int localPlayerId = -1;
    int playerId = -1;
    int wanted = 0;
};

bool ShouldShowNearbyWantedLabel(const NearbyWantedLabelState& state);
std::optional<int> FindLiveWantedForName(const OnlineSnapshot& snapshot, const std::string& name);
std::string BuildNearbyWantedLabel(int wanted);
bool MatchesNativePlayerNameTag(const std::string& text, const std::string& name, int id);
std::string BuildWantedLabelForNativeDraw(NearbyWantedLabelState state, const OnlineSnapshot& snapshot,
                                        const std::string& nativeText, const std::string& playerName);

class NearbyWantedLabels {
public:
    // Refreshes the native name-draw observer; labels are drawn synchronously
    // with their real name, never from retained world positions or an own radar.
    static void Render(SAMPBridge& bridge, const OnlineDataManager& online, bool wantedOverlayEnabled);
    static void SetEnabledProvider(std::function<bool()> provider);
    static void Shutdown(bool removeHook = true);
};

} // namespace keybinder
