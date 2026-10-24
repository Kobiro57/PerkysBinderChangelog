#include "../include/OnlineOverlayController.h"
#include "../include/OnlineDataManager.h"
#include "../include/SettingsManager.h"
#include <algorithm>
#include <utility>

namespace keybinder {

namespace {

const OnlineBucketSummary* FindBucketByName(const std::vector<OnlineBucketSummary>& buckets,
                                            const std::string& bucketName) {
    for (const auto& bucket : buckets) {
        if (bucket.bucketName == bucketName) {
            return &bucket;
        }
    }

    return nullptr;
}

} // namespace

OnlineOverlayController::OnlineOverlayController() = default;

OnlineOverlayController::~OnlineOverlayController() {
    Shutdown();
}

void OnlineOverlayController::Initialize(SettingsManager* settings, OnlineDataManager* dataManager) {
    settingsManager_ = settings;
    dataManager_ = dataManager;
    RefreshSettings();
    localOverlay_.kind = PersistentOverlayKind::LocalAffiliation;
    ownGangOverlay_.kind = PersistentOverlayKind::OwnGang;
    gangsOverlay_.kind = PersistentOverlayKind::GangsSummary;
    wantedOverlay_.kind = PersistentOverlayKind::Wanted;
}

void OnlineOverlayController::Shutdown() {
    localOverlay_.visible = false;
    ownGangOverlay_.visible = false;
    gangsOverlay_.visible = false;
    wantedOverlay_.visible = false;
    settingsManager_ = nullptr;
    dataManager_ = nullptr;
}

void OnlineOverlayController::Update(float deltaTimeSeconds) {
    (void)deltaTimeSeconds;

    if (settingsManager_ == nullptr || dataManager_ == nullptr) {
        return;
    }

    RefreshSettings();
    UpdateLocalOverlay();
    UpdateOwnGangOverlay();
    UpdateGangsOverlay();
    UpdateWantedOverlay();
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

OnlineOverlayAnchor OnlineOverlayController::ParseAnchor(const std::string& value) {
    if (value == "top_left") return OnlineOverlayAnchor::TopLeft;
    if (value == "bottom_left") return OnlineOverlayAnchor::BottomLeft;
    if (value == "bottom_right") return OnlineOverlayAnchor::BottomRight;
    return OnlineOverlayAnchor::TopRight; // default
}

OnlineOverlayStyle OnlineOverlayController::ParseStyle(const std::string& value) {
    if (value == "minimal") return OnlineOverlayStyle::Minimal;
    if (value == "dense") return OnlineOverlayStyle::Dense;
    return OnlineOverlayStyle::Card; // default
}

float OnlineOverlayController::ClampOpacity(float opacity) {
    if (opacity < 0.35f) return 0.35f;
    if (opacity > 1.0f) return 1.0f;
    return opacity;
}

float OnlineOverlayController::ClampOverlayScale(float scale) {
    if (scale < 0.75f) return 0.75f;
    if (scale > 1.50f) return 1.50f;
    return scale;
}

int OnlineOverlayController::ComputeColumnCount(int playerCount) {
    if (playerCount <= 10) return 1;
    if (playerCount <= 24) return 2;
    return 3;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void OnlineOverlayController::RefreshSettings() {
    if (settingsManager_ == nullptr) return;

    const Settings& s = settingsManager_->Get();
    localOverlayEnabled_ = s.online_local_overlay_enabled;
    ownGangOverlayEnabled_ = s.online_own_gang_overlay_enabled;
    gangsOverlayEnabled_ = s.online_gangs_overlay_enabled;
    wantedOverlayEnabled_ = s.online_wanted_overlay_enabled;

    const OnlineOverlayStyle style = ParseStyle(s.online_overlay_style);
    const float opacity = ClampOpacity(s.online_overlay_opacity);
    const bool backgroundEnabled = s.online_overlay_background_enabled;
    const float overlayScale = ClampOverlayScale(s.online_overlay_scale);
    const bool customTextColorEnabled = s.online_overlay_custom_text_color_enabled;
    const float offsetX = s.online_overlay_offset_x;
    const float offsetY = s.online_overlay_offset_y;

    localOverlay_.kind = PersistentOverlayKind::LocalAffiliation;
    localOverlay_.anchor = ParseAnchor(s.online_local_overlay_anchor);
    localOverlay_.style = style;
    localOverlay_.opacity = opacity;
    localOverlay_.offsetX = offsetX;
    localOverlay_.offsetY = offsetY;
    localOverlay_.backgroundEnabled = backgroundEnabled;
    localOverlay_.scale = overlayScale;
    localOverlay_.customTextColorEnabled = customTextColorEnabled;
    localOverlay_.textColorR = s.online_overlay_text_color_r;
    localOverlay_.textColorG = s.online_overlay_text_color_g;
    localOverlay_.textColorB = s.online_overlay_text_color_b;
    localOverlay_.textColorA = s.online_overlay_text_color_a;

    ownGangOverlay_.kind = PersistentOverlayKind::OwnGang;
    ownGangOverlay_.anchor = ParseAnchor(s.online_own_gang_overlay_anchor);
    ownGangOverlay_.style = ParseStyle(s.online_own_gang_overlay_style);
    ownGangOverlay_.opacity = ClampOpacity(s.online_own_gang_overlay_opacity);
    ownGangOverlay_.offsetX = s.online_own_gang_overlay_offset_x;
    ownGangOverlay_.offsetY = s.online_own_gang_overlay_offset_y;
    ownGangOverlay_.backgroundEnabled = s.online_own_gang_overlay_background_enabled;
    ownGangOverlay_.scale = ClampOverlayScale(s.online_own_gang_overlay_scale);
    ownGangOverlay_.customTextColorEnabled = s.online_own_gang_overlay_custom_text_color_enabled;
    ownGangOverlay_.textColorR = s.online_own_gang_overlay_text_color_r;
    ownGangOverlay_.textColorG = s.online_own_gang_overlay_text_color_g;
    ownGangOverlay_.textColorB = s.online_own_gang_overlay_text_color_b;
    ownGangOverlay_.textColorA = s.online_own_gang_overlay_text_color_a;

    gangsOverlay_.kind = PersistentOverlayKind::GangsSummary;
    gangsOverlay_.anchor = ParseAnchor(s.online_gangs_overlay_anchor);
    gangsOverlay_.style = style;
    gangsOverlay_.opacity = opacity;
    gangsOverlay_.offsetX = offsetX;
    gangsOverlay_.offsetY = offsetY;
    gangsOverlay_.backgroundEnabled = backgroundEnabled;
    gangsOverlay_.scale = overlayScale;
    gangsOverlay_.customTextColorEnabled = customTextColorEnabled;
    gangsOverlay_.textColorR = s.online_overlay_text_color_r;
    gangsOverlay_.textColorG = s.online_overlay_text_color_g;
    gangsOverlay_.textColorB = s.online_overlay_text_color_b;
    gangsOverlay_.textColorA = s.online_overlay_text_color_a;

    wantedOverlay_.kind = PersistentOverlayKind::Wanted;
    wantedOverlay_.anchor = ParseAnchor(s.online_wanted_overlay_anchor);
    wantedOverlay_.style = ParseStyle(s.online_wanted_overlay_style);
    wantedOverlay_.opacity = ClampOpacity(s.online_wanted_overlay_opacity);
    wantedOverlay_.offsetX = s.online_wanted_overlay_offset_x;
    wantedOverlay_.offsetY = s.online_wanted_overlay_offset_y;
    wantedOverlay_.backgroundEnabled = s.online_wanted_overlay_background_enabled;
    wantedOverlay_.scale = ClampOverlayScale(s.online_wanted_overlay_scale);
    wantedOverlay_.customTextColorEnabled = s.online_wanted_overlay_custom_text_color_enabled;
    wantedOverlay_.textColorR = s.online_wanted_overlay_text_color_r;
    wantedOverlay_.textColorG = s.online_wanted_overlay_text_color_g;
    wantedOverlay_.textColorB = s.online_wanted_overlay_text_color_b;
    wantedOverlay_.textColorA = s.online_wanted_overlay_text_color_a;
}

void OnlineOverlayController::UpdateLocalOverlay() {
    if (!localOverlayEnabled_ || dataManager_ == nullptr) {
        localOverlay_.visible = false;
        return;
    }

    const OnlineSnapshot snapshot = dataManager_->GetSnapshot();
    if (!snapshot.IsUsable() && snapshot.totalPlayers == 0) {
        localOverlay_.visible = false;
        return;
    }

    if (!snapshot.localAffiliation.isFaction) {
        localOverlay_.visible = false;
        return;
    }

    const OnlineBucketSummary* localBucket = FindLocalBucket(snapshot);
    if (localBucket == nullptr) {
        localOverlay_.visible = false;
        return;
    }

    localOverlay_.visible = true;
    localOverlay_.title = localBucket->bucketName;
    localOverlay_.playerNames = localBucket->playerNames;
    localOverlay_.countRows.clear();
    localOverlay_.playerCount = localBucket->playerCount;
}

void OnlineOverlayController::UpdateOwnGangOverlay() {
    if (!ownGangOverlayEnabled_ || dataManager_ == nullptr) {
        ownGangOverlay_.visible = false;
        return;
    }

    const OnlineSnapshot snapshot = dataManager_->GetSnapshot();
    if (!snapshot.IsUsable() && snapshot.totalPlayers == 0) {
        ownGangOverlay_.visible = false;
        return;
    }

    const OnlineAffiliation& local = snapshot.localAffiliation;
    if (local.name.empty() || local.isFaction) {
        ownGangOverlay_.visible = false;
        return;
    }

    const OnlineBucketSummary* bucket = FindBucketByName(snapshot.gangBuckets, local.name);
    if (bucket == nullptr) {
        ownGangOverlay_.visible = false;
        return;
    }

    ownGangOverlay_.visible = true;
    ownGangOverlay_.title = bucket->bucketName;
    ownGangOverlay_.playerNames = bucket->playerNames;
    ownGangOverlay_.countRows.clear();
    ownGangOverlay_.playerCount = bucket->playerCount;
}

void OnlineOverlayController::UpdateGangsOverlay() {
    if (!gangsOverlayEnabled_ || dataManager_ == nullptr) {
        gangsOverlay_.visible = false;
        return;
    }

    const OnlineSnapshot snapshot = dataManager_->GetSnapshot();
    if (!snapshot.IsUsable() && snapshot.totalPlayers == 0) {
        gangsOverlay_.visible = false;
        return;
    }

    std::vector<PersistentOverlayCountRow> rows;
    int totalGangPlayers = 0;
    for (const auto& bucket : snapshot.gangBuckets) {
        if (bucket.bucketName.empty() || bucket.bucketName == "Zivilisten" || bucket.playerCount <= 0) {
            continue;
        }

        rows.push_back({bucket.bucketName, bucket.playerCount});
        totalGangPlayers += bucket.playerCount;
    }

    if (rows.empty()) {
        gangsOverlay_.visible = false;
        return;
    }

    gangsOverlay_.visible = true;
    gangsOverlay_.title = "Gangs Online";
    gangsOverlay_.playerNames.clear();
    gangsOverlay_.countRows = std::move(rows);
    gangsOverlay_.playerCount = totalGangPlayers;
}

void OnlineOverlayController::UpdateWantedOverlay() {
    if (!wantedOverlayEnabled_ || dataManager_ == nullptr) {
        wantedOverlay_.visible = false;
        return;
    }

    const OnlineSnapshot snapshot = dataManager_->GetSnapshot();
    if (!snapshot.IsUsable() && snapshot.totalPlayers == 0) {
        wantedOverlay_.visible = false;
        return;
    }

    const Settings& s = settingsManager_->Get();
    int minWanted = s.online_wanted_overlay_min;
    int maxWanted = s.online_wanted_overlay_max;
    if (minWanted < 0) minWanted = 0;
    if (maxWanted < 0) maxWanted = 0;
    // max == 0 means "no upper limit"; only swap when both bounds are positive.
    if (maxWanted > 0 && minWanted > maxWanted) {
        const int tmp = minWanted;
        minWanted = maxWanted;
        maxWanted = tmp;
    }

    // Collect players whose wanted falls in [min, max] (upper unbounded if max==0).
    struct WantedEntry {
        std::string name;
        int wanted = 0;
    };
    std::vector<WantedEntry> entries;
    for (const auto& player : snapshot.players) {
        const int w = player.wanted;
        if (w < minWanted) continue;
        if (maxWanted > 0 && w > maxWanted) continue;
        entries.push_back({player.name, w});
    }

    if (entries.empty()) {
        wantedOverlay_.visible = false;
        return;
    }

    // Sort by wanted descending, then by name ascending for stable ordering.
    std::sort(entries.begin(), entries.end(), [](const WantedEntry& a, const WantedEntry& b) {
        if (a.wanted != b.wanted) return a.wanted > b.wanted;
        return a.name < b.name;
    });

    std::vector<PersistentOverlayCountRow> rows;
    rows.reserve(entries.size());
    for (const auto& e : entries) {
        rows.push_back({e.name, e.wanted});
    }

    wantedOverlay_.visible = true;
    wantedOverlay_.title = "Wanted";
    wantedOverlay_.playerNames.clear();
    wantedOverlay_.countRows = std::move(rows);
    wantedOverlay_.playerCount = static_cast<int>(entries.size());
}

const OnlineBucketSummary* OnlineOverlayController::FindLocalBucket(const OnlineSnapshot& snapshot) const {
    const OnlineAffiliation& local = snapshot.localAffiliation;
    if (local.name.empty()) return nullptr;

    if (const OnlineBucketSummary* bucket = FindBucketByName(snapshot.factionBuckets, local.name)) {
        return bucket;
    }

    return FindBucketByName(snapshot.gangBuckets, local.name);
}

} // namespace keybinder
