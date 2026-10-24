#pragma once

#include "Hitmarker.h"
#include "TextureManager.h"

#include "imgui-1.92.7/imgui.h"
#include <vector>

namespace keybinder {

// Returns the screen position of the GTA crosshair, reading from game memory
// or falling back to screen center in test mode.
ImVec2 GetCrosshairScreenPosition(const ImVec2& fallbackDisplaySize);

class HitmarkerRenderer {
public:
    void RenderMarkers(const std::vector<ActiveMarker>& markers, TextureManager* textureMgr,
                      IDirect3DDevice9* device = nullptr);

private:
    void DrawCrossX(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness);
    void DrawPlus(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness);
    void DrawCircle(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness);
    void DrawDiamond(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness);
    void DrawTextureMarker(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, TextureInfo* tex);
};

} // namespace keybinder
