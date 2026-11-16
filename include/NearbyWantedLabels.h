#pragma once

#include <limits>
#include <optional>
#include <string>

class SAMPBridge;

namespace keybinder {

class OnlineDataManager;
struct OnlineSnapshot;

// Deliberately fixed: this is a close-range annotation, never a player radar.
constexpr float kNearbyWantedLabelMaximumDistance = 5.0f;

struct NearbyWantedLabelState {
    bool overlayEnabled = false;
    bool policeMember = false;
    bool connected = false;
    bool localActive = false;
    bool localAlive = false;
    bool streamed = false;
    bool targetAlive = false;
    bool targetVisible = false;
    bool sameInterior = false;
    bool serverAllowsLabels = false;
    bool remoteAllowsLabels = false;
    bool cameraLineOfSight = false;
    bool localLineOfSight = false;
    bool projectedOnScreen = false;
    int localPlayerId = -1;
    int playerId = -1;
    int wanted = 0;
    float distanceSquared = std::numeric_limits<float>::infinity();
    float serverDrawDistance = 0.0f;
};

bool ShouldShowNearbyWantedLabel(const NearbyWantedLabelState& state);
std::optional<int> FindLiveWantedForName(const OnlineSnapshot& snapshot, const std::string& name);
std::string BuildNearbyWantedLabel(int wanted);

class NearbyWantedLabels {
public:
    // Called inside the existing ImGui HUD frame. No labels or positions persist
    // between frames and this never modifies native nametags, blips or entities.
    static void Render(SAMPBridge& bridge, const OnlineDataManager& online, bool wantedOverlayEnabled);
};

} // namespace keybinder
