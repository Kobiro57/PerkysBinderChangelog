#pragma once

#include <d3d9.h>

#include <functional>
#include <string>
#include <vector>

#include "imgui-1.92.7/imgui.h"

class SAMPBridge;

namespace keybinder {

class SettingsManager;
class TextureManager;
struct TextureInfo;

enum class QuickMapMode {
    Toggle,
    Hold,
};
enum class RadarSpritePack;

enum class QuickMapMarkerType {
    LocalPlayer,
    Checkpoint,
    RaceCheckpoint,
    MapIcon,
};

enum class QuickMapZoomLevel {
    Far,
    Medium,
    Close,
    VeryClose,
    Ultra,
};

struct QuickMapMarker {
    QuickMapMarkerType type = QuickMapMarkerType::MapIcon;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float headingRadians = 0.0f;
    ImU32 color = IM_COL32(255, 255, 255, 255);
    float size = 10.0f;
    bool filled = true;
    bool emphasize = false;
    unsigned char radarSprite = 0;
    unsigned char blipType = 0;
};

struct QuickMapGangZone {
    float minWorldX = 0.0f;
    float minWorldY = 0.0f;
    float maxWorldX = 0.0f;
    float maxWorldY = 0.0f;
    ImU32 color = IM_COL32(255, 0, 0, 64);
    ImU32 altColor = 0;  // alpha == 0 means: not flashing
    float outlineThickness = 1.0f;
};

struct MapMarker {
    ImVec2 position;
    ImU32 color = IM_COL32(255, 255, 255, 255);
    float size = 10.0f;
    QuickMapMarkerType type = QuickMapMarkerType::MapIcon;
    float headingRadians = 0.0f;
    bool filled = true;
    bool emphasize = false;
    unsigned char radarSprite = 0;
    unsigned char blipType = 0;
    bool visible = true;
};

struct GangZoneRect {
    ImVec2 min;
    ImVec2 max;
    ImU32 color = IM_COL32(255, 0, 0, 64);
    float outlineThickness = 1.0f;
    bool visible = true;
};

struct QuickMapRenderData {
    bool hasLocalPlayer = false;
    QuickMapMarker localPlayer;
    bool hasCheckpoint = false;
    QuickMapMarker checkpoint;
    bool hasRaceCheckpoint = false;
    QuickMapMarker raceCheckpoint;
    std::vector<QuickMapMarker> mapIcons;
    std::vector<QuickMapGangZone> gangZones;
};

class QuickMap {
public:
    struct TestingHooks {
        std::function<bool(int)> isKeyDown;
        std::function<bool()> isInputGated;
    };

    static constexpr float kWorldMin = -3000.0f;
    static constexpr float kWorldMax = 3000.0f;
    static constexpr float kWorldSpan = kWorldMax - kWorldMin;
    static constexpr float kClassicMapImageWidth = 1918.0f;
    static constexpr float kClassicMapImageHeight = 1919.0f;
    static constexpr float kSatelliteMapImageWidth = 2047.0f;
    static constexpr float kSatelliteMapImageHeight = 2047.0f;

    QuickMap();

    void Initialize(SettingsManager* settings, TextureManager* textures, ::SAMPBridge* bridge = nullptr);
    void Shutdown();
    void OnDeviceLost();
    void OnDeviceReset(IDirect3DDevice9* device);

    void Update();
    bool IsVisible() const { return visible_; }
    QuickMapMode GetMode() const { return mode_; }
    int GetHotkey() const { return hotkey_; }
    QuickMapZoomLevel GetZoomLevel() const;
    void SetZoomLevel(QuickMapZoomLevel level);
    void OnMouseWheel(int direction);
    bool WantsMouseWheelCapture() const;
    const char* GetZoomLevelLabel() const;
    float GetActiveMapAspectRatio() const;
    void Show();
    void Hide();
    void Toggle();

    void SetRenderCallback(std::function<void()> callback);
    void Render();
    void Render(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize);

    QuickMapRenderData ExtractRenderData(::SAMPBridge* bridge) const;
    void SetRenderData(const QuickMapRenderData& renderData);
    const QuickMapRenderData& GetRenderData() const { return renderData_; }
    void SetInputGateCallback(std::function<bool()> callback);
    void SetTestingHooks(const TestingHooks& hooks);

#ifdef KEYBINDER_TESTING
    float GetCurrentZoomFactorForTesting() const { return currentZoomFactor_; }
    ImVec2 GetCurrentFocusWorldForTesting() const { return currentFocusWorld_; }
    QuickMapZoomLevel GetZoomLevelForTesting() const { return zoomLevel_; }
    float GetEffectiveZoomFactorForTesting() const { return currentZoomFactor_; }
    float GetCurrentViewSpanXForTesting() const { return viewMaxWorldX_ - viewMinWorldX_; }
    float GetCurrentViewSpanYForTesting() const { return viewMaxWorldY_ - viewMinWorldY_; }
    void SetDisplaySizeForTesting(const ImVec2& displaySize) {
        lastDisplaySize_ = displaySize;
        UpdateViewState();
    }
#endif

    static QuickMapMode ParseMode(const std::string& value);
    static ImVec2 WorldToNormalized(float worldX, float worldY);
    static ImVec2 WorldToMap(float worldX, float worldY, const ImVec2& displayPos, const ImVec2& displaySize);
    static MapMarker ProjectMarker(const QuickMapMarker& marker, const ImVec2& displayPos, const ImVec2& displaySize);
    static GangZoneRect ProjectGangZone(const QuickMapGangZone& zone, const ImVec2& displayPos, const ImVec2& displaySize);
    static ImU32 ResolveGangZoneFlashColor(ImU32 color, ImU32 altColor, double timeSeconds, bool flashEnabled);
    static ImU32 MakeGangZoneOutlineColor(ImU32 color);

private:
    void RefreshSettings();
    void ApplyConfiguredZoomLevel();
    void UpdateViewState();
    float ResolveViewAspectRatio() const;
    bool IsFeatureEnabled() const;
    bool IsAutoZoomEnabled() const;
    bool IsFollowPlayerEnabled() const;
    bool IsFocusMarkerEnabled() const;
    bool IsInputGated() const;
    bool IsHotkeyDown() const;
    WORD GetActiveMapResourceId() const;
    bool EnsureQuickMapTexturesLoaded(IDirect3DDevice9* device);
    void DrawMapBackdrop(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) const;
    void DrawGangZones(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) const;
    void DrawMarker(ImDrawList* drawList, const MapMarker& marker) const;
    RadarSpritePack GetActiveRadarSpritePack() const;
    TextureInfo* GetRadarSpriteTexture(unsigned char radarSprite) const;
    void DrawRadarSpriteMarker(ImDrawList* drawList, const MapMarker& marker, TextureInfo* texture) const;
    void DrawZoomIndicator(ImDrawList* drawList, const ImVec2& displayPos, const ImVec2& displaySize) const;
    MapMarker ProjectMarkerForCurrentView(const QuickMapMarker& marker, const ImVec2& displayPos, const ImVec2& displaySize) const;
    GangZoneRect ProjectGangZoneForCurrentView(const QuickMapGangZone& zone, const ImVec2& displayPos, const ImVec2& displaySize) const;

    SettingsManager* settingsManager_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    ::SAMPBridge* bridge_ = nullptr;
    QuickMapRenderData renderData_{};
    ImVec2 lastDisplayPos_{};
    ImVec2 lastDisplaySize_{};
    bool visible_ = false;
    QuickMapMode mode_ = QuickMapMode::Toggle;
    int hotkey_ = 'M';
    QuickMapZoomLevel zoomLevel_ = QuickMapZoomLevel::Far;
    QuickMapZoomLevel manualZoomLevel_ = QuickMapZoomLevel::Far;
    QuickMapZoomLevel autoZoomLevel_ = QuickMapZoomLevel::Far;
    bool autoZoomEnabled_ = false;
    bool hasSessionZoomOverride_ = false;
    bool hotkeyWasDown_ = false;
    float currentZoomFactor_ = 1.0f;
    ImVec2 currentFocusWorld_{};
    float viewMinWorldX_ = kWorldMin;
    float viewMaxWorldX_ = kWorldMax;
    float viewMinWorldY_ = kWorldMin;
    float viewMaxWorldY_ = kWorldMax;
    std::function<void()> renderCallback_;
    std::function<bool()> inputGateCallback_;
    TestingHooks testingHooks_{};
};

} // namespace keybinder
