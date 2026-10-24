#include "../include/OnlineOverlayController.h"
#include "../include/OnlineDataManager.h"
#include "../include/SettingsManager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#ifndef KEYBINDER_TESTING
#include "../include/SAMPBridge.h"
#include "CPlayerPed.h"
#include "CTheZones.h"
#include "common.h"
#endif

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
    gameOverlays_.clear();
    markerStates_.clear();
    gameState_ = {};
    gamePlayerIndices_.clear();
    gameStateProvider_ = nullptr;
    gameRefreshSeconds_ = 0.0f;
    settingsManager_ = nullptr;
    dataManager_ = nullptr;
}

void OnlineOverlayController::Update(float deltaTimeSeconds) {
    if (settingsManager_ == nullptr || dataManager_ == nullptr) {
        return;
    }

    RefreshSettings();
    UpdateLocalOverlay();
    UpdateOwnGangOverlay();
    UpdateGangsOverlay();
    UpdateWantedOverlay();
    UpdateGameOverlays(deltaTimeSeconds);
}

void OnlineOverlayController::SetGameStateProvider(std::function<OnlineGameOverlayState()> provider) {
    gameStateProvider_ = std::move(provider);
    gameRefreshSeconds_ = 0.0f;
}

std::string OnlineOverlayController::NormalizeMarkerName(const std::string& name) {
    const std::size_t first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = name.find_last_not_of(" \t\r\n");
    std::string normalized = name.substr(first, last - first + 1);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized;
}

std::string OnlineOverlayController::ResolveMarkerAffiliation(const OnlineSnapshot& snapshot, const std::string& name) {
    if (!snapshot.IsUsable()) return "Unbekannt";
    const std::string normalizedName = NormalizeMarkerName(name);
    for (const auto& player : snapshot.players) {
        if (NormalizeMarkerName(player.name) == normalizedName) {
            return player.affiliation == "Zivilisten" ? "Zivilist"
                : (player.affiliation.empty() ? "Unbekannt" : player.affiliation);
        }
    }
    return "Unbekannt";
}

OnlineGameOverlayState OnlineOverlayController::CaptureGameState(SAMPBridge* bridge) {
    OnlineGameOverlayState state;
#ifndef KEYBINDER_TESTING
    if (bridge == nullptr || bridge->GetVersion() != SAMPBridge::Version::V03DLR1) return state;
    auto* netGame = bridge->GetNetGame();
    if (netGame == nullptr || netGame->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED) return state;
    auto* pool = netGame->GetPlayerPool();
    if (pool == nullptr) return state;
    state.connected = true;

    // Read the native ped only after SA-MP has a current local ped. The
    // connected state can already be set while the player is still spawning.
    auto* localPlayer = pool->pLocalPlayer;
    ::CPed* localPed = localPlayer != nullptr && localPlayer->m_pPed != nullptr
        ? localPlayer->m_pPed->m_pGamePed : nullptr;
    const CVector localPosition = localPed != nullptr ? localPed->GetPosition() : CVector();
    const bool hasLocalPosition = localPed != nullptr && std::isfinite(localPosition.x)
        && std::isfinite(localPosition.y) && std::isfinite(localPosition.z);
    if (hasLocalPosition) {
        const char* city = "San Andreas";
        switch (CTheZones::GetLevelFromPosition(&localPosition)) {
        case LEVEL_NAME_LOS_SANTOS: city = "Los Santos"; break;
        case LEVEL_NAME_SAN_FIERRO: city = "San Fierro"; break;
        case LEVEL_NAME_LAS_VENTURAS: city = "Las Venturas"; break;
        default: city = "San Andreas"; break;
        }
        state.location = std::string(city) + " | " + bridge->GetCurrentZone();
        state.transport = localPed->bInVehicle && localPed->m_pVehicle != nullptr
            ? bridge->GetVehicleName() : "Zu Fu\xC3\x9F";
    }

    for (int id = 0; id < sampapi::v03dl::CPlayerPool::MAX_PLAYERS; ++id) {
        if (id == pool->m_nLocalPlayerId || !pool->m_bNotEmpty[id]) continue;
        auto* info = pool->m_pObject[id];
        if (info == nullptr || info->m_szNick.empty()) continue;
        OnlineMarkerPlayer player;
        player.id = id;
        player.name = info->m_szNick;
        auto* remote = info->m_pPlayer;
        if (remote != nullptr && remote->m_pPed != nullptr && remote->m_pPed->m_pGamePed != nullptr) {
            ++state.streamedPlayers;
        }
        state.players.push_back(std::move(player));
    }
#endif
    return state;
}

void OnlineOverlayController::UpdateGameOverlays(float deltaTimeSeconds) {
    const float elapsed = std::isfinite(deltaTimeSeconds) ? (std::max)(0.0f, deltaTimeSeconds) : 0.0f;
    gameOverlays_.clear();
    markerStates_.clear();
    const Settings& settings = settingsManager_->Get();
    if (!settings.online_enabled) {
        gameState_ = {};
        gamePlayerIndices_.clear();
        gameRefreshSeconds_ = 0.0f;
        return;
    }
    gameRefreshSeconds_ -= elapsed;
    if (gameStateProvider_ && gameRefreshSeconds_ <= 0.0f) {
        gameState_ = gameStateProvider_();
        gamePlayerIndices_.clear();
        for (std::size_t i = 0; i < gameState_.players.size(); ++i) {
            gamePlayerIndices_.emplace(NormalizeMarkerName(gameState_.players[i].name), i);
        }
        gameRefreshSeconds_ = 0.5f;
    }

    if (!gameState_.connected) {
        return;
    }

    auto addOverlay = [&](PersistentOverlayKind kind, const char* title, bool enabled,
                          const std::string& anchor, float x, float y, std::vector<std::string> lines) {
        if (!enabled) return;
        PersistentOverlayData data = localOverlay_;
        data.kind = kind;
        data.visible = true;
        data.title = title;
        data.anchor = ParseAnchor(anchor);
        data.offsetX = x;
        data.offsetY = y;
        data.playerNames = std::move(lines);
        data.countRows.clear();
        data.playerCount = static_cast<int>(data.playerNames.size());
        data.showPlayerCount = false;
        data.minimumWidth = 240.0f;
        gameOverlays_.push_back(std::move(data));
    };

    addOverlay(PersistentOverlayKind::Location, "Ort", settings.online_location_overlay_enabled,
        settings.online_location_overlay_anchor, settings.online_location_overlay_offset_x,
        settings.online_location_overlay_offset_y,
        {gameState_.location.empty() ? "Position nicht verf\xC3\xBC" "gbar" : gameState_.location});
    addOverlay(PersistentOverlayKind::Transport, "Fortbewegungsmittel", settings.online_transport_overlay_enabled,
        settings.online_transport_overlay_anchor, settings.online_transport_overlay_offset_x,
        settings.online_transport_overlay_offset_y,
        {gameState_.transport.empty() ? "Nicht verf\xC3\xBC" "gbar" : gameState_.transport});
    addOverlay(PersistentOverlayKind::Pedslots, "Pedslots", settings.online_pedslots_overlay_enabled,
        settings.online_pedslots_overlay_anchor, settings.online_pedslots_overlay_offset_x,
        settings.online_pedslots_overlay_offset_y,
        {std::to_string(gameState_.streamedPlayers) + " Spieler im Stream"});

    std::vector<std::string> savedNames;
    std::vector<std::string> friends;
    std::vector<std::string> enemies;
    const OnlineSnapshot snapshot = dataManager_->GetSnapshot();
    for (const auto& marker : settings.online_player_markers) {
        const std::string normalizedName = NormalizeMarkerName(marker.name);
        if (normalizedName.empty() || std::find(savedNames.begin(), savedNames.end(), normalizedName) != savedNames.end() || markerStates_.size() >= 20) continue;
        savedNames.push_back(normalizedName);
        OnlineMarkerState state;
        state.name = marker.name;
        state.affiliation = ResolveMarkerAffiliation(snapshot, marker.name);
        state.friendly = marker.friendly;
        const auto playerIndex = gamePlayerIndices_.find(normalizedName);
        if (playerIndex != gamePlayerIndices_.end()) {
            const auto& player = gameState_.players[playerIndex->second];
            state.online = true;
            state.name = player.name;
            state.playerId = player.id;
        }
        const bool showCategory = state.friendly ? settings.online_friends_overlay_enabled : settings.online_enemies_overlay_enabled;
        if (state.online && showCategory) {
            std::string line = std::string(state.friendly ? "Freund: " : "Feind: ") + state.name;
            line += " [" + state.affiliation + "] | ID " + std::to_string(state.playerId);
            (state.friendly ? friends : enemies).push_back(std::move(line));
        }
        markerStates_.push_back(std::move(state));
    }
    auto addRadar = [&](PersistentOverlayKind kind, const char* title, bool enabled,
                        const std::string& anchor, float x, float y, std::vector<std::string> lines,
                        const char* emptyMessage) {
        if (!enabled) return;
        const int onlineCount = static_cast<int>(lines.size());
        if (lines.empty()) lines.push_back(emptyMessage);
        addOverlay(kind, title, true, anchor, x, y, std::move(lines));
        gameOverlays_.back().playerCount = onlineCount;
        gameOverlays_.back().showPlayerCount = true;
        gameOverlays_.back().minimumWidth = 360.0f;
    };
    addRadar(PersistentOverlayKind::FriendsRadar, "Freunde-Radar", settings.online_friends_overlay_enabled,
        settings.online_friends_overlay_anchor, settings.online_friends_overlay_offset_x,
        settings.online_friends_overlay_offset_y, std::move(friends), "Keine gespeicherten Freunde online");
    addRadar(PersistentOverlayKind::EnemiesRadar, "Feinde-Radar", settings.online_enemies_overlay_enabled,
        settings.online_enemies_overlay_anchor, settings.online_enemies_overlay_offset_x,
        settings.online_enemies_overlay_offset_y, std::move(enemies), "Keine gespeicherten Feinde online");
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
