#include "../include/HitmarkerRenderer.h"
#include "../include/ThemeManager.h"

#include <algorithm>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace {

ImU32 MakeOutlineColor(ImU32 color, float alphaScale = 0.45f) {
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
    const ImVec4 bgBase = keybinder::ThemeManager::Instance().GetColor(keybinder::ThemeColorRole::Background);
    rgba.x = bgBase.x;
    rgba.y = bgBase.y;
    rgba.z = bgBase.z;
    rgba.w *= alphaScale;
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

void DrawLineWithOutline(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, ImU32 color, float thickness) {
    drawList->AddLine(start, end, MakeOutlineColor(color), thickness + 2.0f);
    drawList->AddLine(start, end, color, thickness);
}

void DrawSegmentedCrossX(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness) {
    const float scale = size / 20.0f;
    const float innerGap = std::max(1.6f, 4.0f * scale);
    const float armLength = std::max(4.0f, 10.0f * scale);
    const float lineThickness = std::max(1.0f, 2.0f * scale);

    drawList->AddLine(
        ImVec2(center.x - innerGap, center.y - innerGap),
        ImVec2(center.x - innerGap - armLength, center.y - innerGap - armLength),
        color,
        lineThickness);

    drawList->AddLine(
        ImVec2(center.x + innerGap, center.y - innerGap),
        ImVec2(center.x + innerGap + armLength, center.y - innerGap - armLength),
        color,
        lineThickness);

    drawList->AddLine(
        ImVec2(center.x - innerGap, center.y + innerGap),
        ImVec2(center.x - innerGap - armLength, center.y + innerGap + armLength),
        color,
        lineThickness);

    drawList->AddLine(
        ImVec2(center.x + innerGap, center.y + innerGap),
        ImVec2(center.x + innerGap + armLength, center.y + innerGap + armLength),
        color,
        lineThickness);
}

} // namespace

namespace keybinder {

ImVec2 GetCrosshairScreenPosition(const ImVec2& fallbackDisplaySize) {
#ifdef KEYBINDER_TESTING
    return ImVec2(fallbackDisplaySize.x * 0.5f, fallbackDisplaySize.y * 0.5f);
#else
    constexpr uintptr_t kScreenWidthAddress = 0xC17044;
    constexpr uintptr_t kScreenHeightAddress = 0xC17048;
    constexpr uintptr_t kCrosshairMultYAddress = 0xB6EC10;
    constexpr uintptr_t kCrosshairMultXAddress = 0xB6EC14;

    const auto screenWidth = static_cast<float>(*reinterpret_cast<const unsigned int*>(kScreenWidthAddress));
    const auto screenHeight = static_cast<float>(*reinterpret_cast<const unsigned int*>(kScreenHeightAddress));
    const auto crosshairX = *reinterpret_cast<const float*>(kCrosshairMultXAddress);
    const auto crosshairY = *reinterpret_cast<const float*>(kCrosshairMultYAddress);

    const bool validScreen = screenWidth > 0.0f && screenHeight > 0.0f;
    const bool validCrosshair = crosshairX > 0.0f && crosshairX < 1.0f && crosshairY > 0.0f && crosshairY < 1.0f;

    if (validScreen && validCrosshair) {
        return ImVec2(screenWidth * crosshairX, screenHeight * crosshairY);
    }

    return ImVec2(fallbackDisplaySize.x * 0.5f, fallbackDisplaySize.y * 0.5f);
#endif
}

void HitmarkerRenderer::RenderMarkers(const std::vector<ActiveMarker>& markers, TextureManager* textureMgr,
                                      IDirect3DDevice9* device) {
    if (markers.empty()) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 center = GetCrosshairScreenPosition(displaySize);
    float dpiScale = ImGui::GetIO().FontGlobalScale;

    for (const auto& marker : markers) {
        if (marker.preset == nullptr) {
            continue;
        }

        ImVec4 colorVec = marker.isKillConfirmed
            ? ImVec4(marker.preset->killConfirmedColor.r, marker.preset->killConfirmedColor.g,
                     marker.preset->killConfirmedColor.b, marker.currentOpacity)
            : ImVec4(marker.preset->color.r, marker.preset->color.g,
                     marker.preset->color.b, marker.currentOpacity);

        ImU32 color = ImGui::ColorConvertFloat4ToU32(colorVec);
        float size = marker.preset->size * dpiScale;
        float thickness = std::max(2.2f, size / 9.0f);

        switch (marker.preset->shape) {
            case HitmarkerShape::CrossX:
                DrawCrossX(drawList, center, size, color, thickness);
                break;
            case HitmarkerShape::Plus:
                DrawPlus(drawList, center, size, color, thickness);
                break;
            case HitmarkerShape::Circle:
                DrawCircle(drawList, center, size, color, thickness);
                break;
            case HitmarkerShape::Diamond:
                DrawDiamond(drawList, center, size, color, thickness);
                break;
            case HitmarkerShape::CustomPNG:
                if (textureMgr != nullptr && !marker.preset->customImagePath.empty()) {
                    const std::string& imgPath = marker.preset->customImagePath;
                    TextureInfo* tex = textureMgr->GetTexture(imgPath);
                    if (tex == nullptr && device != nullptr) {
                        if (textureMgr->LoadTexture(imgPath, device)) {
                            tex = textureMgr->GetTexture(imgPath);
                        }
                    }
                    if (tex != nullptr) {
                        DrawTextureMarker(drawList, center, size, color, tex);
                    }
                }
                break;
        }
    }
}

void HitmarkerRenderer::DrawCrossX(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness) {
    DrawSegmentedCrossX(drawList, center, size, color, thickness);
}

void HitmarkerRenderer::DrawPlus(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness) {
    const float half = size * 0.5f;
    const float gap = std::max(3.0f, size * 0.18f);

    DrawLineWithOutline(drawList,
        ImVec2(center.x - half, center.y),
        ImVec2(center.x - gap, center.y),
        color, thickness);
    DrawLineWithOutline(drawList,
        ImVec2(center.x + gap, center.y),
        ImVec2(center.x + half, center.y),
        color, thickness);
    DrawLineWithOutline(drawList,
        ImVec2(center.x, center.y - half),
        ImVec2(center.x, center.y - gap),
        color, thickness);
    DrawLineWithOutline(drawList,
        ImVec2(center.x, center.y + gap),
        ImVec2(center.x, center.y + half),
        color, thickness);
}

void HitmarkerRenderer::DrawCircle(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness) {
    const float radius = size * 0.46f;
    drawList->AddCircle(center, radius, MakeOutlineColor(color), 48, thickness + 2.0f);
    drawList->AddCircle(center, radius, color, 48, thickness);
}

void HitmarkerRenderer::DrawDiamond(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness) {
    const float half = size * 0.5f;
    const ImVec2 top(center.x, center.y - half);
    const ImVec2 right(center.x + half, center.y);
    const ImVec2 bottom(center.x, center.y + half);
    const ImVec2 left(center.x - half, center.y);
    DrawLineWithOutline(drawList, top, right, color, thickness);
    DrawLineWithOutline(drawList, right, bottom, color, thickness);
    DrawLineWithOutline(drawList, bottom, left, color, thickness);
    DrawLineWithOutline(drawList, left, top, color, thickness);
}

void HitmarkerRenderer::DrawTextureMarker(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, TextureInfo* tex) {
    float half = size * 0.5f;
    ImVec2 topLeft(center.x - half, center.y - half);
    ImVec2 bottomRight(center.x + half, center.y + half);
    drawList->AddImage(
        static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(tex->texture)),
        topLeft, bottomRight,
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
        color
    );
}

} // namespace keybinder
