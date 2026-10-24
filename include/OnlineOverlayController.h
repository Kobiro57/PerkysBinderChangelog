#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

class SAMPBridge;

namespace keybinder {

class SettingsManager;
class OnlineDataManager;
struct OnlineBucketSummary;
struct OnlineSnapshot;

// ---------------------------------------------------------------------------
// Overlay anchor positions
// ---------------------------------------------------------------------------
enum class OnlineOverlayAnchor {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

enum class PersistentOverlayKind {
    LocalAffiliation,
    OwnGang,
    GangsSummary,
    Wanted,
    Location,
    Transport,
    Pedslots,
    FriendsRadar,
    EnemiesRadar,
};

struct OnlineMarkerPlayer {
    int id = -1;
    std::string name;
};

struct OnlineGameOverlayState {
    bool connected = false;
    int streamedPlayers = 0;
    std::string location;
    std::string transport;
    std::vector<OnlineMarkerPlayer> players;
};

struct OnlineMarkerState {
    std::string name;
    std::string affiliation;
    bool friendly = false;
    bool online = false;
    int playerId = -1;
};

// ---------------------------------------------------------------------------
// Overlay style presets
// ---------------------------------------------------------------------------
enum class OnlineOverlayStyle {
    Card,    // solid bg + header + accent
    Minimal, // translucent bg, no header
    Dense,   // card style + 0.9x text scale
};

// ---------------------------------------------------------------------------
// Render data for a single persistent overlay surface
// ---------------------------------------------------------------------------
struct PersistentOverlayCountRow {
    std::string label;
    int count = 0;
};

struct PersistentOverlayData {
    bool visible = false;
    PersistentOverlayKind kind = PersistentOverlayKind::LocalAffiliation;
    OnlineOverlayAnchor anchor = OnlineOverlayAnchor::TopRight;
    float offsetX = 12.0f;
    float offsetY = 12.0f;
    OnlineOverlayStyle style = OnlineOverlayStyle::Card;
    float opacity = 0.85f;  // clamped 0.35-1.0
    bool backgroundEnabled = true;
    float scale = 1.0f; // clamped 0.75-1.50
    bool customTextColorEnabled = false;
    float textColorR = 1.0f;
    float textColorG = 1.0f;
    float textColorB = 1.0f;
    float textColorA = 1.0f;
    std::string title;      // e.g. "LSPD" or "Gangs Online"
    std::vector<std::string> playerNames; // deduped by id, sorted by name
    std::vector<PersistentOverlayCountRow> countRows; // summary rows: label + online count
    int playerCount = 0;
    bool showPlayerCount = true;
    float minimumWidth = 0.0f;
};

// ---------------------------------------------------------------------------
// OnlineOverlayController — manages persistent local affiliation and gang
// summary overlay state, feeds render data to the renderer.
// ---------------------------------------------------------------------------
class OnlineOverlayController {
public:
    OnlineOverlayController();
    ~OnlineOverlayController();

    void Initialize(SettingsManager* settings, OnlineDataManager* dataManager);
    void Shutdown();

    // Called every frame to refresh persistent overlay state.
    void Update(float deltaTimeSeconds);

    // Get current render data for both persistent overlays.
    const PersistentOverlayData& GetLocalOverlayData() const { return localOverlay_; }
    const PersistentOverlayData& GetGangsOverlayData() const { return gangsOverlay_; }
    const PersistentOverlayData& GetWantedOverlayData() const { return wantedOverlay_; }
    const PersistentOverlayData& GetOwnGangOverlayData() const { return ownGangOverlay_; }
    const std::vector<PersistentOverlayData>& GetGameOverlays() const { return gameOverlays_; }
    const std::vector<OnlineMarkerState>& GetMarkerStates() const { return markerStates_; }
    const OnlineGameOverlayState& GetGameState() const { return gameState_; }
    void SetGameStateProvider(std::function<OnlineGameOverlayState()> provider);
    static OnlineGameOverlayState CaptureGameState(SAMPBridge* bridge);
    static std::string NormalizeMarkerName(const std::string& name);
    static std::string ResolveMarkerAffiliation(const OnlineSnapshot& snapshot, const std::string& name);

    // Static helpers
    static OnlineOverlayAnchor ParseAnchor(const std::string& value);
    static OnlineOverlayStyle ParseStyle(const std::string& value);
    static float ClampOpacity(float opacity);
    static float ClampOverlayScale(float scale);
    static int ComputeColumnCount(int playerCount);

private:
    void RefreshSettings();
    void UpdateLocalOverlay();
    void UpdateOwnGangOverlay();
    void UpdateGangsOverlay();
    void UpdateWantedOverlay();
    void UpdateGameOverlays(float deltaTimeSeconds);

    // From the snapshot, find the bucket matching the local player's affiliation.
    const OnlineBucketSummary* FindLocalBucket(const OnlineSnapshot& snapshot) const;

    SettingsManager* settingsManager_ = nullptr;
    OnlineDataManager* dataManager_ = nullptr;
    PersistentOverlayData localOverlay_;
    PersistentOverlayData ownGangOverlay_;
    PersistentOverlayData gangsOverlay_;
    PersistentOverlayData wantedOverlay_;
    std::vector<PersistentOverlayData> gameOverlays_;
    OnlineGameOverlayState gameState_;
    std::map<std::string, std::size_t> gamePlayerIndices_;
    std::vector<OnlineMarkerState> markerStates_;
    std::function<OnlineGameOverlayState()> gameStateProvider_;
    float gameRefreshSeconds_ = 0.0f;

    // Cached settings
    bool localOverlayEnabled_ = false;
    bool ownGangOverlayEnabled_ = false;
    bool gangsOverlayEnabled_ = false;
    bool wantedOverlayEnabled_ = false;
};

} // namespace keybinder
