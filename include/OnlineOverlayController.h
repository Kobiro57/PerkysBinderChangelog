#pragma once

#include <chrono>
#include <string>
#include <vector>

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

    // From the snapshot, find the bucket matching the local player's affiliation.
    const OnlineBucketSummary* FindLocalBucket(const OnlineSnapshot& snapshot) const;

    SettingsManager* settingsManager_ = nullptr;
    OnlineDataManager* dataManager_ = nullptr;
    PersistentOverlayData localOverlay_;
    PersistentOverlayData ownGangOverlay_;
    PersistentOverlayData gangsOverlay_;
    PersistentOverlayData wantedOverlay_;

    // Cached settings
    bool localOverlayEnabled_ = false;
    bool ownGangOverlayEnabled_ = false;
    bool gangsOverlayEnabled_ = false;
    bool wantedOverlayEnabled_ = false;
};

} // namespace keybinder
