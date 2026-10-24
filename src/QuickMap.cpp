#include "../include/QuickMap.h"

#include "../include/RadarSpriteResources.h"
#include "../include/ResourceIds.h"
#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/TextureManager.h"
#include "../include/UIComponents.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CRadar.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <type_traits>
#include <utility>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::size_t kRadarTraceCapacity = 175;
constexpr float kLocalPlayerMarkerSize = 12.0f;

struct ZoomLevelInfo {
    keybinder::QuickMapZoomLevel level;
    const char* label;
    float multiplier;
};

static const ZoomLevelInfo kZoomLevels[] = {
    { keybinder::QuickMapZoomLevel::Far, "Far", 1.0f },
    { keybinder::QuickMapZoomLevel::Medium, "Medium", 1.2f },
    { keybinder::QuickMapZoomLevel::Close, "Close", 1.45f },
    { keybinder::QuickMapZoomLevel::VeryClose, "Very Close", 1.75f },
    { keybinder::QuickMapZoomLevel::Ultra, "Ultra", 2.1f },
};
static constexpr int kZoomLevelCount = sizeof(kZoomLevels) / sizeof(kZoomLevels[0]);
static constexpr float kMaxEffectiveZoomFactor = 4.5f;

float Clamp01(float value) {
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

float ClampFloat(float value, float minValue, float maxValue) {
    return (std::max)(minValue, (std::min)(value, maxValue));
}

ImU32 MakeOutlineColor(ImU32 color, float alphaScale = 1.0f) {
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    rgba.x = 0.02f;
    rgba.y = 0.04f;
    rgba.z = 0.06f;
    rgba.w = Clamp01(rgba.w * alphaScale);
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

ImU32 ColorFromAbgr(unsigned int abgr, unsigned char fallbackAlpha = 255) {
    unsigned int alpha = (abgr >> 24) & 0xFF;
    if (alpha == 0) {
        alpha = fallbackAlpha;
    }

    return IM_COL32(
        static_cast<int>(abgr & 0xFF),
        static_cast<int>((abgr >> 8) & 0xFF),
        static_cast<int>((abgr >> 16) & 0xFF),
        static_cast<int>(alpha));
}

#ifndef KEYBINDER_TESTING
constexpr uintptr_t kRadarTraceArrayAddress = 0xBA86F0;
constexpr uintptr_t kGetActualBlipArrayIndexAddress = 0x582870;
constexpr uintptr_t kGetRadarTraceColourAddress = 0x584770;

tRadarTrace* GetRadarTraceArray() {
    return reinterpret_cast<tRadarTrace*>(kRadarTraceArrayAddress);
}

int GetActualBlipArrayIndex(int blipIndex) {
    return reinterpret_cast<int(__cdecl*)(int)>(kGetActualBlipArrayIndexAddress)(blipIndex);
}

unsigned int GetRadarTraceColour(unsigned int color, unsigned char bright, unsigned char friendly) {
    return reinterpret_cast<unsigned int(__cdecl*)(unsigned int, unsigned char, unsigned char)>(kGetRadarTraceColourAddress)(color, bright, friendly);
}
#else
tRadarTrace* GetRadarTraceArray() {
    return nullptr;
}

int GetActualBlipArrayIndex(int blipIndex) {
    return blipIndex;
}

unsigned int GetRadarTraceColour(unsigned int color, unsigned char, unsigned char) {
    return color;
}
#endif

ImVec2 RotatePoint(const ImVec2& point, float radians) {
    const float cosAngle = std::cos(radians);
    const float sinAngle = std::sin(radians);
    return ImVec2(
        point.x * cosAngle - point.y * sinAngle,
        point.x * sinAngle + point.y * cosAngle);
}

float HeadingFromOnFootQuaternion(const float* quaternion) {
    const float w = quaternion[0];
    const float x = quaternion[1];
    const float y = quaternion[2];
    const float z = quaternion[3];
    return std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
}

float ClampMapTransparency(float value) {
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

bool HasMeaningfulWorldPosition(float x, float y) {
    constexpr float kMinimumMagnitude = 0.01f;
    return std::fabs(x) > kMinimumMagnitude
        || std::fabs(y) > kMinimumMagnitude;
}

bool IsWorldPositionInsideQuickMapBounds(float x, float y) {
    constexpr float kWorldBoundsPadding = 64.0f;
    return x >= (keybinder::QuickMap::kWorldMin - kWorldBoundsPadding)
        && x <= (keybinder::QuickMap::kWorldMax + kWorldBoundsPadding)
        && y >= (keybinder::QuickMap::kWorldMin - kWorldBoundsPadding)
        && y <= (keybinder::QuickMap::kWorldMax + kWorldBoundsPadding);
}

bool IsCoordStyleRadarTrace(const tRadarTrace& radarTrace) {
    return radarTrace.m_nRadarSprite == RADAR_SPRITE_NONE
        && (radarTrace.m_nBlipType == BLIP_COORD || radarTrace.m_nBlipType == BLIP_CONTACTPOINT);
}


bool HasCheckpointExtent(const sampapi::CVector& size) {
    constexpr float kMinimumExtent = 0.01f;
    return std::fabs(size.x) > kMinimumExtent
        || std::fabs(size.y) > kMinimumExtent
        || std::fabs(size.z) > kMinimumExtent;
}


bool EqualsIgnoreCase(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const unsigned char l = static_cast<unsigned char>(lhs[i]);
        const unsigned char r = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(l) != std::tolower(r)) {
            return false;
        }
    }

    return true;
}

const ZoomLevelInfo& GetZoomLevelInfo(keybinder::QuickMapZoomLevel level) {
    for (const ZoomLevelInfo& info : kZoomLevels) {
        if (info.level == level) {
            return info;
        }
    }

    return kZoomLevels[0];
}

int GetZoomLevelIndex(keybinder::QuickMapZoomLevel level) {
    for (int index = 0; index < kZoomLevelCount; ++index) {
        if (kZoomLevels[index].level == level) {
            return index;
        }
    }

    return 0;
}

const char* ZoomLevelToString(keybinder::QuickMapZoomLevel level) {
    switch (level) {
        case keybinder::QuickMapZoomLevel::Far:
            return "far";
        case keybinder::QuickMapZoomLevel::Medium:
            return "medium";
        case keybinder::QuickMapZoomLevel::Close:
            return "close";
        case keybinder::QuickMapZoomLevel::VeryClose:
            return "very_close";
        case keybinder::QuickMapZoomLevel::Ultra:
            return "ultra";
    }

    return "far";
}

keybinder::QuickMapZoomLevel ParseZoomLevel(const std::string& value) {
    if (EqualsIgnoreCase(value, "medium")) {
        return keybinder::QuickMapZoomLevel::Medium;
    }

    if (EqualsIgnoreCase(value, "close")) {
        return keybinder::QuickMapZoomLevel::Close;
    }

    if (EqualsIgnoreCase(value, "very_close") || EqualsIgnoreCase(value, "very close")) {
        return keybinder::QuickMapZoomLevel::VeryClose;
    }

    if (EqualsIgnoreCase(value, "ultra")) {
        return keybinder::QuickMapZoomLevel::Ultra;
    }

    return keybinder::QuickMapZoomLevel::Far;
}

template <typename T, typename = void>
struct HasQuickMapZoomLevel : std::false_type {};

template <typename T>
struct HasQuickMapZoomLevel<T, std::void_t<decltype(std::declval<T&>().quickmap_zoom_level)>> : std::true_type {};

template <typename T, typename = void>
struct HasQuickMapLastZoomLevel : std::false_type {};

template <typename T>
struct HasQuickMapLastZoomLevel<T, std::void_t<decltype(std::declval<T&>().quickmap_last_zoom_level)>> : std::true_type {};

template <typename T>
bool TryReadQuickMapZoomLevelString(const T& settings, std::string& value) {
    if constexpr (HasQuickMapZoomLevel<T>::value) {
        value = settings.quickmap_zoom_level;
        return true;
    }

    (void)settings;
    value.clear();
    return false;
}

template <typename T>
void WriteQuickMapZoomLevelString(T& settings, const char* value) {
    if constexpr (HasQuickMapZoomLevel<T>::value) {
        settings.quickmap_zoom_level = value;
    } else {
        (void)settings;
        (void)value;
    }
}

template <typename T>
bool TryReadQuickMapLastZoomLevelString(const T& settings, std::string& value) {
    if constexpr (HasQuickMapLastZoomLevel<T>::value) {
        value = settings.quickmap_last_zoom_level;
        return true;
    }

    (void)settings;
    value.clear();
    return false;
}

template <typename T>
void WriteQuickMapLastZoomLevelString(T& settings, const char* value) {
    if constexpr (HasQuickMapLastZoomLevel<T>::value) {
        settings.quickmap_last_zoom_level = value;
    } else {
        (void)settings;
        (void)value;
    }
}

int CountActiveRedMarkers(const keybinder::QuickMapRenderData& renderData) {
    int count = 0;
    if (renderData.hasCheckpoint) {
        ++count;
    }
    if (renderData.hasRaceCheckpoint) {
        ++count;
    }
    return count;
}

const keybinder::QuickMapMarker* GetSingleActiveRedMarker(const keybinder::QuickMapRenderData& renderData) {
    if (CountActiveRedMarkers(renderData) != 1) {
        return nullptr;
    }

    if (renderData.hasCheckpoint) {
        return &renderData.checkpoint;
    }

    if (renderData.hasRaceCheckpoint) {
        return &renderData.raceCheckpoint;
    }

    return nullptr;
}

bool IsWorldPointInsideView(float worldX, float worldY, float minWorldX, float maxWorldX, float minWorldY, float maxWorldY) {
    return worldX >= minWorldX
        && worldX <= maxWorldX
        && worldY >= minWorldY
        && worldY <= maxWorldY;
}

bool DoesWorldRectIntersectView(
    float minWorldX,
    float maxWorldX,
    float minWorldY,
    float maxWorldY,
    float viewMinWorldX,
    float viewMaxWorldX,
    float viewMinWorldY,
    float viewMaxWorldY) {
    return maxWorldX >= viewMinWorldX
        && minWorldX <= viewMaxWorldX
        && maxWorldY >= viewMinWorldY
        && minWorldY <= viewMaxWorldY;
}

ImVec2 WorldToMapForView(
    float worldX,
    float worldY,
    float minWorldX,
    float maxWorldX,
    float minWorldY,
    float maxWorldY,
    const ImVec2& displayPos,
    const ImVec2& displaySize) {
    const float spanX = (std::max)(maxWorldX - minWorldX, 1.0f);
    const float spanY = (std::max)(maxWorldY - minWorldY, 1.0f);
    const float normalizedX = Clamp01((worldX - minWorldX) / spanX);
    const float normalizedY = Clamp01((maxWorldY - worldY) / spanY);
    return ImVec2(
        displayPos.x + normalizedX * displaySize.x,
        displayPos.y + normalizedY * displaySize.y);
}

void AddQuickMapRedMarkerGlow(ImDrawList* drawList, const ImVec2& rectMin, const ImVec2& rectMax, float rounding) {
    constexpr ImU32 kGlowColor = IM_COL32(255, 38, 38, 230);
    const float markerWidth = (std::max)(rectMax.x - rectMin.x, 1.0f);
    const float pulse = 1.08f + 0.42f * (0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.6f));

    keybinder::UIComponents::AddSoftGlowRect(
        drawList,
        rectMin,
        rectMax,
        kGlowColor,
        rounding + 2.0f,
        (std::max)(14.0f, markerWidth * 0.9f),
        pulse,
        32);
}

} // namespace

namespace keybinder {

QuickMap::QuickMap() = default;

void QuickMap::Initialize(SettingsManager* settings, TextureManager* textures, SAMPBridge* bridge) {
    settingsManager_ = settings;
    textureManager_ = textures;
    bridge_ = bridge;
    visible_ = false;
    hotkeyWasDown_ = false;
    UpdateViewState();
    RefreshSettings();
}

void QuickMap::Shutdown() {
    visible_ = false;
    hotkeyWasDown_ = false;
    zoomLevel_ = QuickMapZoomLevel::Far;
    manualZoomLevel_ = QuickMapZoomLevel::Far;
    autoZoomLevel_ = QuickMapZoomLevel::Far;
    autoZoomEnabled_ = false;
    hasSessionZoomOverride_ = false;
    renderData_ = {};
    UpdateViewState();
    settingsManager_ = nullptr;
    textureManager_ = nullptr;
    bridge_ = nullptr;
    renderCallback_ = nullptr;
    inputGateCallback_ = nullptr;
    testingHooks_ = {};
}

void QuickMap::OnDeviceLost() {
    hotkeyWasDown_ = false;
}

void QuickMap::OnDeviceReset(IDirect3DDevice9* device) {
    RefreshSettings();
    EnsureQuickMapTexturesLoaded(device);
}

void QuickMap::Update() {
    RefreshSettings();

    if (!IsFeatureEnabled()) {
        Hide();
        hotkeyWasDown_ = false;
        UpdateViewState();
        return;
    }

    const bool hotkeyDown = IsHotkeyDown();
    const bool inputGated = IsInputGated();

    switch (mode_) {
        case QuickMapMode::Toggle:
            if (!inputGated && hotkeyDown && !hotkeyWasDown_) {
                Toggle();
            }
            break;

        case QuickMapMode::Hold:
            if (!inputGated && hotkeyDown) {
                Show();
            } else {
                Hide();
            }
            break;
    }

    UpdateViewState();

    hotkeyWasDown_ = hotkeyDown;
}

float QuickMap::GetActiveMapAspectRatio() const {
    const WORD resourceId = GetActiveMapResourceId();
    if (textureManager_ != nullptr) {
        const std::string textureKey = TextureManager::GetResourceTextureKey(resourceId);
        if (TextureInfo* texture = textureManager_->GetTexture(textureKey)) {
            if (texture->width > 0 && texture->height > 0) {
                return static_cast<float>(texture->width) / static_cast<float>(texture->height);
            }
        }
    }

    if (resourceId == IDR_GTASA_SATELLITE_MAP) {
        return kSatelliteMapImageWidth / kSatelliteMapImageHeight;
    }

    return kClassicMapImageWidth / kClassicMapImageHeight;
}

QuickMapZoomLevel QuickMap::GetZoomLevel() const {
    return zoomLevel_;
}

void QuickMap::SetZoomLevel(QuickMapZoomLevel level) {
    zoomLevel_ = GetZoomLevelInfo(level).level;

    if (!IsAutoZoomEnabled()) {
        manualZoomLevel_ = zoomLevel_;
        hasSessionZoomOverride_ = false;
    } else {
        hasSessionZoomOverride_ = true;
    }

    if (settingsManager_ != nullptr && !IsAutoZoomEnabled()) {
        Settings& settings = settingsManager_->GetMutable();
        WriteQuickMapLastZoomLevelString(settings, ZoomLevelToString(zoomLevel_));
    }

    UpdateViewState();
}

void QuickMap::OnMouseWheel(int direction) {
    if (direction == 0) {
        return;
    }

    const int step = direction > 0 ? 1 : -1;
    const int currentIndex = GetZoomLevelIndex(zoomLevel_);
    const int targetIndex = (std::max)(0, (std::min)(kZoomLevelCount - 1, currentIndex + step));
    SetZoomLevel(kZoomLevels[targetIndex].level);
}

bool QuickMap::WantsMouseWheelCapture() const {
    return visible_ && !IsInputGated();
}

const char* QuickMap::GetZoomLevelLabel() const {
    return GetZoomLevelInfo(zoomLevel_).label;
}

void QuickMap::Show() {
    if (!IsFeatureEnabled()) {
        visible_ = false;
        return;
    }

    if (!autoZoomEnabled_ || !hasSessionZoomOverride_) {
        ApplyConfiguredZoomLevel();
    }
    visible_ = true;
    UpdateViewState();
}

void QuickMap::Hide() {
    if (autoZoomEnabled_) {
        hasSessionZoomOverride_ = false;
        ApplyConfiguredZoomLevel();
    }

    visible_ = false;
    UpdateViewState();
}

void QuickMap::Toggle() {
    if (visible_) {
        Hide();
    } else {
        Show();
    }
}

void QuickMap::SetRenderCallback(std::function<void()> callback) {
    renderCallback_ = std::move(callback);
}

void QuickMap::Render() {
    if (!visible_ || !renderCallback_) {
        return;
    }

    renderCallback_();
}

void QuickMap::Render(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) {
    if (!visible_ || drawList == nullptr || displaySize.x <= 0.0f || displaySize.y <= 0.0f) {
        return;
    }

    lastDisplayPos_ = displayPos;
    lastDisplaySize_ = displaySize;
    UpdateViewState();

    DrawMapBackdrop(drawList, displayPos, displaySize);
    DrawGangZones(drawList, displayPos, displaySize);

    if (renderData_.hasCheckpoint) {
        DrawMarker(drawList, ProjectMarkerForCurrentView(renderData_.checkpoint, displayPos, displaySize));
    }

    if (renderData_.hasRaceCheckpoint) {
        DrawMarker(drawList, ProjectMarkerForCurrentView(renderData_.raceCheckpoint, displayPos, displaySize));
    }

    for (const QuickMapMarker& icon : renderData_.mapIcons) {
        DrawMarker(drawList, ProjectMarkerForCurrentView(icon, displayPos, displaySize));
    }

    if (renderData_.hasLocalPlayer) {
        DrawMarker(drawList, ProjectMarkerForCurrentView(renderData_.localPlayer, displayPos, displaySize));
    }

    DrawZoomIndicator(drawList, displayPos, displaySize);
}

QuickMapRenderData QuickMap::ExtractRenderData(SAMPBridge* bridge) const {
    QuickMapRenderData renderData;

    SAMPBridge* const sourceBridge = bridge != nullptr ? bridge : bridge_;
    if (sourceBridge == nullptr) {
        return renderData;
    }

    if (sampapi::v03dl::CLocalPlayer* localPlayer = sourceBridge->GetLocalPlayer()) {
        if (localPlayer->m_pPed != nullptr) {
            QuickMapMarker localPlayerMarker;
            localPlayerMarker.type = QuickMapMarkerType::LocalPlayer;
            localPlayerMarker.color = IM_COL32(255, 255, 255, 255);
            localPlayerMarker.size = kLocalPlayerMarkerSize;
            localPlayerMarker.filled = true;

            constexpr sampapi::ID kInvalidEntityId = static_cast<sampapi::ID>(-1);
            float headingRadians = HeadingFromOnFootQuaternion(localPlayer->m_onfootData.m_fQuaternion);
            if (localPlayer->m_nCurrentVehicle != kInvalidEntityId) {
                headingRadians = HeadingFromOnFootQuaternion(localPlayer->m_incarData.m_fQuaternion);
            }

            if (localPlayer->m_pPed->m_pGamePed != nullptr) {
                const CVector& position = localPlayer->m_pPed->m_pGamePed->GetPosition();
                localPlayerMarker.worldX = position.x;
                localPlayerMarker.worldY = position.y;
                localPlayerMarker.headingRadians = headingRadians;
                renderData.hasLocalPlayer = true;
                renderData.localPlayer = localPlayerMarker;
            } else {
                localPlayerMarker.worldX = localPlayer->m_onfootData.m_position.x;
                localPlayerMarker.worldY = localPlayer->m_onfootData.m_position.y;
                localPlayerMarker.headingRadians = headingRadians;
                renderData.hasLocalPlayer = true;
                renderData.localPlayer = localPlayerMarker;
            }
        }
    }

    if (sampapi::v03dl::CGame* game = sourceBridge->GetGame()) {
        if (game->m_checkpoint.m_bEnabled
            && HasMeaningfulWorldPosition(
                game->m_checkpoint.m_position.x,
                game->m_checkpoint.m_position.y)
            && IsWorldPositionInsideQuickMapBounds(
                game->m_checkpoint.m_position.x,
                game->m_checkpoint.m_position.y)) {
            renderData.hasCheckpoint = true;
            renderData.checkpoint.type = QuickMapMarkerType::Checkpoint;
            renderData.checkpoint.worldX = game->m_checkpoint.m_position.x;
            renderData.checkpoint.worldY = game->m_checkpoint.m_position.y;
            renderData.checkpoint.color = IM_COL32(255, 70, 70, 255);
            renderData.checkpoint.size = 24.0f;
            renderData.checkpoint.filled = true;
        }

        if (game->m_racingCheckpoint.m_bEnabled
            && HasMeaningfulWorldPosition(
                game->m_racingCheckpoint.m_currentPosition.x,
                game->m_racingCheckpoint.m_currentPosition.y)
            && IsWorldPositionInsideQuickMapBounds(
                game->m_racingCheckpoint.m_currentPosition.x,
                game->m_racingCheckpoint.m_currentPosition.y)) {
            renderData.hasRaceCheckpoint = true;
            renderData.raceCheckpoint.type = QuickMapMarkerType::RaceCheckpoint;
            renderData.raceCheckpoint.worldX = game->m_racingCheckpoint.m_currentPosition.x;
            renderData.raceCheckpoint.worldY = game->m_racingCheckpoint.m_currentPosition.y;
            renderData.raceCheckpoint.color = IM_COL32(255, 70, 70, 255);
            renderData.raceCheckpoint.size = 24.0f;
            renderData.raceCheckpoint.filled = true;
        }
    }

    if (sampapi::v03dl::CNetGame* netGame = sourceBridge->GetNetGame()) {
        if (tRadarTrace* const radarTraceArray = GetRadarTraceArray()) {
            renderData.mapIcons.reserve(100);
            std::vector<bool> trackedRadarTraceIndices(kRadarTraceCapacity, false);

            for (const auto mapIconHandle : netGame->m_aMapIcons) {
                if (mapIconHandle == 0) {
                    continue;
                }

                const int radarTraceIndex = GetActualBlipArrayIndex(static_cast<int>(mapIconHandle));
                if (radarTraceIndex < 0) {
                    continue;
                }

                if (radarTraceIndex < static_cast<int>(trackedRadarTraceIndices.size())) {
                    trackedRadarTraceIndices[radarTraceIndex] = true;
                }

                const tRadarTrace& radarTrace = radarTraceArray[radarTraceIndex];
                if (!radarTrace.m_bInUse) {
                    continue;
                }

                if (radarTrace.m_nRadarSprite == RADAR_SPRITE_CENTRE
                    || radarTrace.m_nRadarSprite == RADAR_SPRITE_MAP_HERE
                    || radarTrace.m_nRadarSprite == RADAR_SPRITE_NORTH) {
                    continue;
                }

                if (!HasMeaningfulWorldPosition(radarTrace.m_vecPos.x, radarTrace.m_vecPos.y)) {
                    continue;
                }

                if (!IsWorldPositionInsideQuickMapBounds(radarTrace.m_vecPos.x, radarTrace.m_vecPos.y)) {
                    continue;
                }

                QuickMapMarker mapIconMarker;
                mapIconMarker.type = QuickMapMarkerType::MapIcon;
                mapIconMarker.worldX = radarTrace.m_vecPos.x;
                mapIconMarker.worldY = radarTrace.m_vecPos.y;
                mapIconMarker.color = ColorFromAbgr(
                    GetRadarTraceColour(
                        radarTrace.m_nColour,
                        radarTrace.m_bBright ? 1 : 0,
                        radarTrace.m_bFriendly ? 1 : 0));
                mapIconMarker.radarSprite = radarTrace.m_nRadarSprite;
                mapIconMarker.blipType = radarTrace.m_nBlipType;
                mapIconMarker.emphasize = IsCoordStyleRadarTrace(radarTrace);
                mapIconMarker.size = mapIconMarker.emphasize
                    ? 20.0f
                    : (radarTrace.m_nRadarSprite == RADAR_SPRITE_WAYPOINT ? 12.0f : 10.0f);
                mapIconMarker.filled = true;
                renderData.mapIcons.push_back(mapIconMarker);
            }

            for (std::size_t radarTraceIndex = 0; radarTraceIndex < kRadarTraceCapacity; ++radarTraceIndex) {
                if (radarTraceIndex < trackedRadarTraceIndices.size() && trackedRadarTraceIndices[radarTraceIndex]) {
                    continue;
                }

                const tRadarTrace& radarTrace = radarTraceArray[radarTraceIndex];
                const bool isDynamicCoordMarker = IsCoordStyleRadarTrace(radarTrace);
                const bool isDynamicSpriteMarker = radarTrace.m_nRadarSprite != RADAR_SPRITE_NONE
                    && radarTrace.m_nRadarSprite != RADAR_SPRITE_CENTRE
                    && radarTrace.m_nRadarSprite != RADAR_SPRITE_MAP_HERE
                    && radarTrace.m_nRadarSprite != RADAR_SPRITE_NORTH;

                if (!radarTrace.m_bInUse || (!isDynamicCoordMarker && !isDynamicSpriteMarker)) {
                    continue;
                }

                if (radarTrace.m_nBlipDisplay != BLIP_DISPLAY_MARKER_ONLY
                    && radarTrace.m_nBlipDisplay != BLIP_DISPLAY_BOTH) {
                    continue;
                }

                if (!HasMeaningfulWorldPosition(radarTrace.m_vecPos.x, radarTrace.m_vecPos.y)) {
                    continue;
                }

                if (!IsWorldPositionInsideQuickMapBounds(radarTrace.m_vecPos.x, radarTrace.m_vecPos.y)) {
                    continue;
                }

                QuickMapMarker coordMarker;
                coordMarker.type = QuickMapMarkerType::MapIcon;
                coordMarker.worldX = radarTrace.m_vecPos.x;
                coordMarker.worldY = radarTrace.m_vecPos.y;
                coordMarker.color = ColorFromAbgr(
                    GetRadarTraceColour(
                        radarTrace.m_nColour,
                        radarTrace.m_bBright ? 1 : 0,
                        radarTrace.m_bFriendly ? 1 : 0));
                coordMarker.radarSprite = radarTrace.m_nRadarSprite;
                coordMarker.blipType = radarTrace.m_nBlipType;
                coordMarker.emphasize = isDynamicCoordMarker;
                coordMarker.size = isDynamicCoordMarker ? 20.0f : 10.0f;
                coordMarker.filled = true;
                renderData.mapIcons.push_back(coordMarker);
            }
        }

        if (netGame->m_pPools != nullptr && netGame->m_pPools->m_pGangZone != nullptr) {
            sampapi::v03dl::CGangZonePool* const gangZonePool = netGame->m_pPools->m_pGangZone;
            renderData.gangZones.reserve(sampapi::v03dl::CGangZonePool::MAX_GANGZONES);

            for (int index = 0; index < sampapi::v03dl::CGangZonePool::MAX_GANGZONES; ++index) {
                if (!gangZonePool->m_bNotEmpty[index] || gangZonePool->m_pObject[index] == nullptr) {
                    continue;
                }

                const sampapi::v03dl::GangZone* const gangZone = gangZonePool->m_pObject[index];
                QuickMapGangZone renderZone;
                renderZone.minWorldX = gangZone->m_rect.left;
                renderZone.minWorldY = gangZone->m_rect.bottom;
                renderZone.maxWorldX = gangZone->m_rect.right;
                renderZone.maxWorldY = gangZone->m_rect.top;
                renderZone.color = ColorFromAbgr(static_cast<unsigned int>(gangZone->m_color), 64);
                renderZone.altColor = ColorFromAbgr(static_cast<unsigned int>(gangZone->m_altColor), 0);
                renderZone.outlineThickness = 1.0f;
                renderData.gangZones.push_back(renderZone);
            }
        }
    }

    return renderData;
}

void QuickMap::SetRenderData(const QuickMapRenderData& renderData) {
    renderData_ = renderData;
    UpdateViewState();
}

void QuickMap::SetInputGateCallback(std::function<bool()> callback) {
    inputGateCallback_ = std::move(callback);
}

void QuickMap::SetTestingHooks(const TestingHooks& hooks) {
    testingHooks_ = hooks;
}

QuickMapMode QuickMap::ParseMode(const std::string& value) {
    if (EqualsIgnoreCase(value, "hold")) {
        return QuickMapMode::Hold;
    }

    return QuickMapMode::Toggle;
}

ImVec2 QuickMap::WorldToNormalized(float worldX, float worldY) {
    const float normalizedX = Clamp01((worldX - kWorldMin) / kWorldSpan);
    const float normalizedY = Clamp01((kWorldMax - worldY) / kWorldSpan);
    return ImVec2(normalizedX, normalizedY);
}

ImVec2 QuickMap::WorldToMap(float worldX, float worldY, const ImVec2& displayPos, const ImVec2& displaySize) {
    const ImVec2 normalized = WorldToNormalized(worldX, worldY);
    return ImVec2(
        displayPos.x + normalized.x * displaySize.x,
        displayPos.y + normalized.y * displaySize.y);
}

MapMarker QuickMap::ProjectMarker(const QuickMapMarker& marker, const ImVec2& displayPos, const ImVec2& displaySize) {
    MapMarker projected;
    projected.position = WorldToMap(marker.worldX, marker.worldY, displayPos, displaySize);
    projected.color = marker.color;
    projected.size = marker.size;
    projected.type = marker.type;
    projected.headingRadians = marker.headingRadians;
    projected.filled = marker.filled;
    projected.emphasize = marker.emphasize;
    projected.radarSprite = marker.radarSprite;
    projected.blipType = marker.blipType;
    return projected;
}

GangZoneRect QuickMap::ProjectGangZone(const QuickMapGangZone& zone, const ImVec2& displayPos, const ImVec2& displaySize) {
    const float minWorldX = (std::min)(zone.minWorldX, zone.maxWorldX);
    const float maxWorldX = (std::max)(zone.minWorldX, zone.maxWorldX);
    const float minWorldY = (std::min)(zone.minWorldY, zone.maxWorldY);
    const float maxWorldY = (std::max)(zone.minWorldY, zone.maxWorldY);

    const ImVec2 p1 = WorldToMap(minWorldX, minWorldY, displayPos, displaySize);
    const ImVec2 p2 = WorldToMap(maxWorldX, maxWorldY, displayPos, displaySize);

    GangZoneRect rect;
    rect.min = ImVec2((std::min)(p1.x, p2.x), (std::min)(p1.y, p2.y));
    rect.max = ImVec2((std::max)(p1.x, p2.x), (std::max)(p1.y, p2.y));
    rect.color = zone.color;
    rect.outlineThickness = zone.outlineThickness;
    return rect;
}

void QuickMap::RefreshSettings() {
    if (settingsManager_ == nullptr) {
        mode_ = QuickMapMode::Toggle;
        hotkey_ = 'M';
        manualZoomLevel_ = QuickMapZoomLevel::Far;
        autoZoomLevel_ = QuickMapZoomLevel::Far;
        autoZoomEnabled_ = false;
        hasSessionZoomOverride_ = false;
        ApplyConfiguredZoomLevel();
        UpdateViewState();
        return;
    }

    const Settings& settings = settingsManager_->Get();
    const bool previousAutoZoomEnabled = autoZoomEnabled_;
    const QuickMapZoomLevel previousManualZoomLevel = manualZoomLevel_;
    const QuickMapZoomLevel previousAutoZoomLevel = autoZoomLevel_;
    mode_ = ParseMode(settings.quickmap_mode);
    hotkey_ = settings.quickmap_hotkey;

    std::string zoomLevelValue;
    if (TryReadQuickMapZoomLevelString(settings, zoomLevelValue)) {
        autoZoomLevel_ = ParseZoomLevel(zoomLevelValue);
    } else {
        autoZoomLevel_ = QuickMapZoomLevel::Far;
    }

    std::string lastZoomLevelValue;
    if (TryReadQuickMapLastZoomLevelString(settings, lastZoomLevelValue)) {
        manualZoomLevel_ = ParseZoomLevel(lastZoomLevelValue);
    } else {
        manualZoomLevel_ = autoZoomLevel_;
    }

    autoZoomEnabled_ = settings.quickmap_auto_zoom;

    const bool configuredZoomSourceChanged = previousAutoZoomEnabled != autoZoomEnabled_
        || (!autoZoomEnabled_ && previousManualZoomLevel != manualZoomLevel_)
        || (autoZoomEnabled_ && previousAutoZoomLevel != autoZoomLevel_);

    if (configuredZoomSourceChanged) {
        hasSessionZoomOverride_ = false;
        ApplyConfiguredZoomLevel();
    } else if (!visible_ && (!autoZoomEnabled_ || !hasSessionZoomOverride_)) {
        ApplyConfiguredZoomLevel();
    }

    if (!settings.quickmap_enabled) {
        visible_ = false;
    }

    UpdateViewState();
}

void QuickMap::ApplyConfiguredZoomLevel() {
    zoomLevel_ = autoZoomEnabled_ ? autoZoomLevel_ : manualZoomLevel_;
}

bool QuickMap::IsFeatureEnabled() const {
    return settingsManager_ == nullptr || settingsManager_->Get().quickmap_enabled;
}

bool QuickMap::IsAutoZoomEnabled() const {
    return autoZoomEnabled_;
}

bool QuickMap::IsFollowPlayerEnabled() const {
    return settingsManager_ == nullptr || settingsManager_->Get().quickmap_follow_player;
}

bool QuickMap::IsFocusMarkerEnabled() const {
    return settingsManager_ == nullptr || settingsManager_->Get().quickmap_focus_marker;
}

float QuickMap::ResolveViewAspectRatio() const {
    if (lastDisplaySize_.x > 0.0f && lastDisplaySize_.y > 0.0f) {
        return ClampFloat(lastDisplaySize_.x / lastDisplaySize_.y, 0.25f, 4.0f);
    }

    return ClampFloat(GetActiveMapAspectRatio(), 0.25f, 4.0f);
}

void QuickMap::UpdateViewState() {
    currentZoomFactor_ = 1.0f;
    viewMinWorldX_ = kWorldMin;
    viewMaxWorldX_ = kWorldMax;
    viewMinWorldY_ = kWorldMin;
    viewMaxWorldY_ = kWorldMax;
    currentFocusWorld_ = renderData_.hasLocalPlayer
        ? ImVec2(renderData_.localPlayer.worldX, renderData_.localPlayer.worldY)
        : ImVec2(0.0f, 0.0f);

    if (!visible_) {
        return;
    }

    const float manualMultiplier = GetZoomLevelInfo(zoomLevel_).multiplier;
    currentZoomFactor_ = ClampFloat(1.0f * manualMultiplier, 1.0f, kMaxEffectiveZoomFactor);

    if (renderData_.hasLocalPlayer) {
        const QuickMapMarker& player = renderData_.localPlayer;
        currentFocusWorld_ = ImVec2(player.worldX, player.worldY);
        const QuickMapMarker* singleRedMarker = nullptr;

        if (IsFollowPlayerEnabled() && IsFocusMarkerEnabled()) {
            singleRedMarker = GetSingleActiveRedMarker(renderData_);
        }

        if (singleRedMarker != nullptr) {
            currentFocusWorld_ = ImVec2(
                (player.worldX + singleRedMarker->worldX) * 0.5f,
                (player.worldY + singleRedMarker->worldY) * 0.5f);

            const float markerDistanceX = std::fabs(singleRedMarker->worldX - player.worldX);
            const float markerDistanceY = std::fabs(singleRedMarker->worldY - player.worldY);
            const float requiredViewSpan = (std::max)(
                1800.0f,
                (std::max)(markerDistanceX, markerDistanceY) + 650.0f * 2.0f);
            const float contextFactor = ClampFloat(
                QuickMap::kWorldSpan / requiredViewSpan,
                1.15f,
                2.05f);
            currentZoomFactor_ = ClampFloat(contextFactor * manualMultiplier, 1.0f, kMaxEffectiveZoomFactor);
        } else if (IsAutoZoomEnabled()) {
            currentZoomFactor_ = ClampFloat(1.35f * manualMultiplier, 1.0f, kMaxEffectiveZoomFactor);
        }
    }

    const float baseViewSpan = QuickMap::kWorldSpan / currentZoomFactor_;
    float viewSpanX = baseViewSpan;
    float viewSpanY = baseViewSpan;
    const float viewAspectRatio = ResolveViewAspectRatio();

    if (viewAspectRatio >= 1.0f) {
        viewSpanY /= viewAspectRatio;
    } else {
        viewSpanX *= viewAspectRatio;
    }

    const float halfViewSpanX = viewSpanX * 0.5f;
    const float halfViewSpanY = viewSpanY * 0.5f;
    const float clampedFocusX = ClampFloat(currentFocusWorld_.x, QuickMap::kWorldMin + halfViewSpanX, QuickMap::kWorldMax - halfViewSpanX);
    const float clampedFocusY = ClampFloat(currentFocusWorld_.y, QuickMap::kWorldMin + halfViewSpanY, QuickMap::kWorldMax - halfViewSpanY);

    viewMinWorldX_ = clampedFocusX - halfViewSpanX;
    viewMaxWorldX_ = clampedFocusX + halfViewSpanX;
    viewMinWorldY_ = clampedFocusY - halfViewSpanY;
    viewMaxWorldY_ = clampedFocusY + halfViewSpanY;
}

bool QuickMap::IsInputGated() const {
    if (testingHooks_.isInputGated) {
        return testingHooks_.isInputGated();
    }

    bool inputGated = false;
    if (inputGateCallback_) {
        inputGated = inputGateCallback_();
    }

    if (bridge_ != nullptr) {
        inputGated = inputGated || bridge_->IsChatOpen() || bridge_->IsDialogOpen() || bridge_->IsCursorActive();
    }

    return inputGated;
}

bool QuickMap::IsHotkeyDown() const {
    if (hotkey_ <= 0) {
        return false;
    }

    if (testingHooks_.isKeyDown) {
        return testingHooks_.isKeyDown(hotkey_);
    }

    return (GetAsyncKeyState(hotkey_) & 0x8000) != 0;
}

WORD QuickMap::GetActiveMapResourceId() const {
    if (settingsManager_ != nullptr && settingsManager_->Get().quickmap_map_variant == "satellite") {
        return IDR_GTASA_SATELLITE_MAP;
    }

    return IDR_GTASA_MAP;
}

bool QuickMap::EnsureQuickMapTexturesLoaded(IDirect3DDevice9* device) {
    if (textureManager_ == nullptr || device == nullptr) {
        return false;
    }

    bool loadedAll = true;
    const WORD baseResources[] = {
        IDR_GTASA_MAP,
        IDR_GTASA_SATELLITE_MAP,
    };

    for (const WORD resourceId : baseResources) {
        const std::string textureKey = TextureManager::GetResourceTextureKey(resourceId);
        loadedAll = (textureManager_->HasTexture(textureKey)
            || textureManager_->LoadTextureFromResource(resourceId, device)) && loadedAll;
    }

    for (const RadarSpriteResource& resource : kRadarSpriteResources) {
        const int resourceIds[] = {
            ResolveRadarSpriteResourceId(resource, RadarSpritePack::Radar1),
            ResolveRadarSpriteResourceId(resource, RadarSpritePack::Radar2),
        };

        for (const int resourceId : resourceIds) {
            const WORD wordResourceId = static_cast<WORD>(resourceId);
            const std::string textureKey = TextureManager::GetResourceTextureKey(wordResourceId);
            loadedAll = (textureManager_->HasTexture(textureKey)
                || textureManager_->LoadTextureFromResource(wordResourceId, device)) && loadedAll;
        }
    }

    return loadedAll;
}

void QuickMap::DrawMapBackdrop(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) const {
    const ImVec2 mapMax(displayPos.x + displaySize.x, displayPos.y + displaySize.y);
    drawList->AddRectFilled(displayPos, mapMax, IM_COL32(16, 24, 34, 230), 6.0f);

    TextureInfo* mapTexture = nullptr;
    if (textureManager_ != nullptr) {
        const std::string textureKey = TextureManager::GetResourceTextureKey(GetActiveMapResourceId());
        mapTexture = textureManager_->GetTexture(textureKey);
    }

    const float mapTransparency = settingsManager_ != nullptr
        ? ClampMapTransparency(settingsManager_->Get().quickmap_map_transparency)
        : 1.0f;
    const int mapAlpha = static_cast<int>(mapTransparency * 255.0f);

    if (mapTexture != nullptr && mapTexture->texture != nullptr) {
        const ImVec2 uvMin = WorldToNormalized(viewMinWorldX_, viewMaxWorldY_);
        const ImVec2 uvMax = WorldToNormalized(viewMaxWorldX_, viewMinWorldY_);
        drawList->AddImage(
            reinterpret_cast<ImTextureID>(mapTexture->texture),
            displayPos,
            mapMax,
            uvMin,
            uvMax,
            IM_COL32(255, 255, 255, mapAlpha));
    }

    const float gridX1 = displayPos.x + displaySize.x * 0.33333334f;
    const float gridX2 = displayPos.x + displaySize.x * 0.66666669f;
    const float gridY1 = displayPos.y + displaySize.y * 0.33333334f;
    const float gridY2 = displayPos.y + displaySize.y * 0.66666669f;
    const ImU32 gridColor = IM_COL32(255, 255, 255, 36);

    drawList->AddLine(ImVec2(gridX1, displayPos.y), ImVec2(gridX1, mapMax.y), gridColor, 1.0f);
    drawList->AddLine(ImVec2(gridX2, displayPos.y), ImVec2(gridX2, mapMax.y), gridColor, 1.0f);
    drawList->AddLine(ImVec2(displayPos.x, gridY1), ImVec2(mapMax.x, gridY1), gridColor, 1.0f);
    drawList->AddLine(ImVec2(displayPos.x, gridY2), ImVec2(mapMax.x, gridY2), gridColor, 1.0f);
    drawList->AddRect(displayPos, mapMax, IM_COL32(255, 255, 255, 190), 6.0f, 0, 2.0f);
}

void QuickMap::DrawGangZones(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) const {
    if (renderData_.gangZones.empty()) {
        return;
    }

    const Settings* const settings = settingsManager_ != nullptr ? &settingsManager_->Get() : nullptr;
    const float outlineThicknessSetting = settings != nullptr
        ? ClampFloat(settings->quickmap_gangzone_outline_thickness, 1.0f, 6.0f)
        : 2.5f;
    const bool flashEnabled = settings == nullptr || settings->quickmap_gangzone_flash_enabled;
    const double now = ImGui::GetTime();

    for (const QuickMapGangZone& zone : renderData_.gangZones) {
        const GangZoneRect rect = ProjectGangZoneForCurrentView(zone, displayPos, displaySize);
        if (!rect.visible) {
            continue;
        }
        const ImU32 fillColor = ResolveGangZoneFlashColor(zone.color, zone.altColor, now, flashEnabled);
        const float thickness = (std::max)(outlineThicknessSetting, zone.outlineThickness);
        drawList->AddRectFilled(rect.min, rect.max, fillColor, 0.0f);
        drawList->AddRect(rect.min, rect.max, MakeGangZoneOutlineColor(fillColor), 0.0f, 0, thickness);
    }
}

ImU32 QuickMap::ResolveGangZoneFlashColor(ImU32 color, ImU32 altColor, double timeSeconds, bool flashEnabled) {
    if (!flashEnabled) {
        return color;
    }
    const unsigned int altAlpha = (altColor >> IM_COL32_A_SHIFT) & 0xFFu;
    if (altAlpha == 0u || altColor == color) {
        return color;
    }
    // 1Hz square wave: 0.5s primary, 0.5s alt.
    double t = timeSeconds;
    if (!(t == t)) {  // NaN guard
        t = 0.0;
    }
    const double phase = t - std::floor(t);
    return phase < 0.5 ? color : altColor;
}

ImU32 QuickMap::MakeGangZoneOutlineColor(ImU32 color) {
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    const float maxChannel = (std::max)(rgba.x, (std::max)(rgba.y, rgba.z));
    if (maxChannel <= 0.0f) {
        // Pure black fill: fall back to a bright white outline so the zone stays visible.
        return IM_COL32(255, 255, 255, 230);
    }
    // Boost a dim color so the outline reads against the map, but keep saturation intact.
    if (maxChannel < 0.85f) {
        const float scale = 0.85f / maxChannel;
        rgba.x = Clamp01(rgba.x * scale);
        rgba.y = Clamp01(rgba.y * scale);
        rgba.z = Clamp01(rgba.z * scale);
    }
    rgba.w = 0.95f;
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

RadarSpritePack QuickMap::GetActiveRadarSpritePack() const {
    return ResolveActiveRadarSpritePack(settingsManager_ != nullptr ? &settingsManager_->Get() : nullptr);
}

TextureInfo* QuickMap::GetRadarSpriteTexture(unsigned char radarSprite) const {
    if (textureManager_ == nullptr) {
        return nullptr;
    }

    const RadarSpriteResource* const resource = FindRadarSpriteResource(radarSprite);
    if (resource == nullptr) {
        return nullptr;
    }

    const int resourceId = ResolveRadarSpriteResourceId(*resource, GetActiveRadarSpritePack());
    const std::string textureKey = TextureManager::GetResourceTextureKey(static_cast<WORD>(resourceId));
    return textureManager_->GetTexture(textureKey);
}

void QuickMap::DrawRadarSpriteMarker(ImDrawList* drawList, const MapMarker& marker, TextureInfo* texture) const {
    if (drawList == nullptr || texture == nullptr || texture->texture == nullptr) {
        return;
    }

    const float halfHeight = (std::max)(6.0f, marker.size * 0.8f);
    const float textureAspect = texture->height > 0
        ? static_cast<float>(texture->width) / static_cast<float>(texture->height)
        : 1.0f;
    const float halfWidth = halfHeight * textureAspect;
    const ImVec2 rectMin(marker.position.x - halfWidth, marker.position.y - halfHeight);
    const ImVec2 rectMax(marker.position.x + halfWidth, marker.position.y + halfHeight);
    drawList->AddImage(
        reinterpret_cast<ImTextureID>(texture->texture),
        rectMin,
        rectMax,
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        IM_COL32(255, 255, 255, 255));
}

void QuickMap::DrawMarker(ImDrawList* drawList, const MapMarker& marker) const {
    if (!marker.visible) {
        return;
    }

    const ImU32 outline = MakeOutlineColor(marker.color);

    switch (marker.type) {
        case QuickMapMarkerType::LocalPlayer: {
            const ImVec2 center = marker.position;
            const float angle = marker.headingRadians;
            TextureInfo* localPlayerTexture = GetRadarSpriteTexture(RADAR_SPRITE_CENTRE);

            if (localPlayerTexture != nullptr && localPlayerTexture->texture != nullptr) {
                const float halfHeight = (std::max)(12.0f, marker.size);
                const float textureAspect = localPlayerTexture->height > 0
                    ? static_cast<float>(localPlayerTexture->width) / static_cast<float>(localPlayerTexture->height)
                    : 1.0f;
                const float halfWidth = halfHeight * textureAspect;

                const ImVec2 localTopLeft = RotatePoint(ImVec2(-halfWidth, -halfHeight), angle);
                const ImVec2 localTopRight = RotatePoint(ImVec2(halfWidth, -halfHeight), angle);
                const ImVec2 localBottomRight = RotatePoint(ImVec2(halfWidth, halfHeight), angle);
                const ImVec2 localBottomLeft = RotatePoint(ImVec2(-halfWidth, halfHeight), angle);

                drawList->AddImageQuad(
                    reinterpret_cast<ImTextureID>(localPlayerTexture->texture),
                    ImVec2(center.x + localTopLeft.x, center.y + localTopLeft.y),
                    ImVec2(center.x + localTopRight.x, center.y + localTopRight.y),
                    ImVec2(center.x + localBottomRight.x, center.y + localBottomRight.y),
                    ImVec2(center.x + localBottomLeft.x, center.y + localBottomLeft.y),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    ImVec2(0.0f, 1.0f),
                    IM_COL32(255, 255, 255, 255));
            } else {
                const float radius = (std::max)(7.0f, marker.size * 0.6f);
                const ImVec2 tipOffset = RotatePoint(ImVec2(0.0f, -radius * 1.35f), angle);
                const ImVec2 leftOffset = RotatePoint(ImVec2(-radius * 0.7f, radius * 0.95f), angle);
                const ImVec2 rightOffset = RotatePoint(ImVec2(radius * 0.7f, radius * 0.95f), angle);
                const ImVec2 tip(center.x + tipOffset.x, center.y + tipOffset.y);
                const ImVec2 left(center.x + leftOffset.x, center.y + leftOffset.y);
                const ImVec2 right(center.x + rightOffset.x, center.y + rightOffset.y);
                drawList->AddTriangleFilled(tip, left, right, outline);
                drawList->AddTriangleFilled(
                    tip,
                    ImVec2((center.x + left.x) * 0.5f, (center.y + left.y) * 0.5f),
                    ImVec2((center.x + right.x) * 0.5f, (center.y + right.y) * 0.5f),
                    marker.color);
            }
            break;
        }

        case QuickMapMarkerType::Checkpoint: {
            [[fallthrough]];
        }

        case QuickMapMarkerType::RaceCheckpoint: {
            const float half = (std::max)(8.0f, marker.size * 0.5f);
            const float rounding = 3.0f;
            const ImVec2 rectMin(marker.position.x - half, marker.position.y - half);
            const ImVec2 rectMax(marker.position.x + half, marker.position.y + half);
            AddQuickMapRedMarkerGlow(drawList, rectMin, rectMax, rounding);
            drawList->AddRectFilled(rectMin, rectMax, outline, rounding);
            drawList->AddRectFilled(
                ImVec2(marker.position.x - half * 0.72f, marker.position.y - half * 0.72f),
                ImVec2(marker.position.x + half * 0.72f, marker.position.y + half * 0.72f),
                marker.color,
                rounding);
            break;
        }

        case QuickMapMarkerType::MapIcon: {
            if (marker.emphasize) {
                const float half = (std::max)(7.0f, marker.size * 0.5f);
                const float rounding = 3.0f;
                const ImVec2 rectMin(marker.position.x - half, marker.position.y - half);
                const ImVec2 rectMax(marker.position.x + half, marker.position.y + half);
                AddQuickMapRedMarkerGlow(drawList, rectMin, rectMax, rounding);
                drawList->AddRectFilled(rectMin, rectMax, outline, rounding);
                drawList->AddRectFilled(
                    ImVec2(marker.position.x - half * 0.72f, marker.position.y - half * 0.72f),
                    ImVec2(marker.position.x + half * 0.72f, marker.position.y + half * 0.72f),
                    marker.color,
                    rounding);
                break;
            }

            if (TextureInfo* radarSpriteTexture = GetRadarSpriteTexture(marker.radarSprite)) {
                DrawRadarSpriteMarker(drawList, marker, radarSpriteTexture);
                break;
            }

            const float radius = marker.radarSprite == RADAR_SPRITE_WAYPOINT ? 7.0f : (std::max)(4.5f, marker.size * 0.45f);
            drawList->AddCircleFilled(marker.position, radius + 2.0f, outline, 24);
            drawList->AddCircleFilled(marker.position, radius, marker.color, 24);
            drawList->AddCircleFilled(marker.position, (std::max)(1.5f, radius * 0.28f), IM_COL32(255, 255, 255, 210), 16);
            break;
        }
    }
}

void QuickMap::DrawZoomIndicator(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) const {
    if (drawList == nullptr) {
        return;
    }

    std::string label = "Zoom: ";
    label += GetZoomLevelLabel();

    if (IsFollowPlayerEnabled() && IsFocusMarkerEnabled()) {
        const QuickMapMarker* const singleRedMarker = GetSingleActiveRedMarker(renderData_);
        if (singleRedMarker != nullptr) {
            label += " (Focus)";
        } else {
            label += " (Follow)";
        }
    } else if (IsFollowPlayerEnabled()) {
        label += " (Follow)";
    } else if (IsAutoZoomEnabled()) {
        label += " (Auto)";
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * 0.8f;
    const ImVec2 textSize = font != nullptr
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str())
        : ImGui::CalcTextSize(label.c_str());

    const ImVec2 padding(8.0f, 5.0f);
    const ImVec2 textPos(displayPos.x + 12.0f, displayPos.y + displaySize.y - textSize.y - padding.y * 2.0f - 12.0f);
    const ImVec2 bgMin(textPos.x - padding.x, textPos.y - padding.y);
    const ImVec2 bgMax(textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y);

    drawList->AddRectFilled(bgMin, bgMax, IM_COL32(8, 12, 18, 180), 4.0f);
    drawList->AddText(font, fontSize, textPos, IM_COL32(255, 255, 255, 235), label.c_str());
}

MapMarker QuickMap::ProjectMarkerForCurrentView(const QuickMapMarker& marker, const ImVec2& displayPos, const ImVec2& displaySize) const {
    MapMarker projected;
    projected.color = marker.color;
    projected.size = marker.size;
    projected.type = marker.type;
    projected.headingRadians = marker.headingRadians;
    projected.filled = marker.filled;
    projected.emphasize = marker.emphasize;
    projected.radarSprite = marker.radarSprite;
    projected.blipType = marker.blipType;
    projected.visible = IsWorldPointInsideView(
        marker.worldX,
        marker.worldY,
        viewMinWorldX_,
        viewMaxWorldX_,
        viewMinWorldY_,
        viewMaxWorldY_);

    if (!projected.visible) {
        return projected;
    }

    projected.position = WorldToMapForView(
        marker.worldX,
        marker.worldY,
        viewMinWorldX_,
        viewMaxWorldX_,
        viewMinWorldY_,
        viewMaxWorldY_,
        displayPos,
        displaySize);
    return projected;
}

GangZoneRect QuickMap::ProjectGangZoneForCurrentView(const QuickMapGangZone& zone, const ImVec2& displayPos, const ImVec2& displaySize) const {
    const float minWorldX = (std::min)(zone.minWorldX, zone.maxWorldX);
    const float maxWorldX = (std::max)(zone.minWorldX, zone.maxWorldX);
    const float minWorldY = (std::min)(zone.minWorldY, zone.maxWorldY);
    const float maxWorldY = (std::max)(zone.minWorldY, zone.maxWorldY);

    GangZoneRect rect;
    rect.color = zone.color;
    rect.outlineThickness = zone.outlineThickness;
    rect.visible = DoesWorldRectIntersectView(
        minWorldX,
        maxWorldX,
        minWorldY,
        maxWorldY,
        viewMinWorldX_,
        viewMaxWorldX_,
        viewMinWorldY_,
        viewMaxWorldY_);

    if (!rect.visible) {
        return rect;
    }

    const float clippedMinWorldX = (std::max)(minWorldX, viewMinWorldX_);
    const float clippedMaxWorldX = (std::min)(maxWorldX, viewMaxWorldX_);
    const float clippedMinWorldY = (std::max)(minWorldY, viewMinWorldY_);
    const float clippedMaxWorldY = (std::min)(maxWorldY, viewMaxWorldY_);

    const ImVec2 p1 = WorldToMapForView(
        clippedMinWorldX,
        clippedMinWorldY,
        viewMinWorldX_,
        viewMaxWorldX_,
        viewMinWorldY_,
        viewMaxWorldY_,
        displayPos,
        displaySize);
    const ImVec2 p2 = WorldToMapForView(
        clippedMaxWorldX,
        clippedMaxWorldY,
        viewMinWorldX_,
        viewMaxWorldX_,
        viewMinWorldY_,
        viewMaxWorldY_,
        displayPos,
        displaySize);

    rect.min = ImVec2((std::min)(p1.x, p2.x), (std::min)(p1.y, p2.y));
    rect.max = ImVec2((std::max)(p1.x, p2.x), (std::max)(p1.y, p2.y));
    return rect;
}

} // namespace keybinder
