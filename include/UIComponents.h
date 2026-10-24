#pragma once

#include "imgui-1.92.7/imgui.h"
#include <string>
#include <utility>

namespace keybinder {

struct TextureInfo;

class UIComponents {
public:
    static void BeginCard(const char* id, float width = 0.0f, bool withAccentBorder = false);
    static void BeginCardFillHeight(const char* id, float width = 0.0f, bool withAccentBorder = false);
    static void EndCard();
    
    static void CardHeader(const char* title, const char* subtitle = nullptr);
    static void CardFooter();

    static void SectionHeader(const char* title, const char* subtitle = nullptr, const char* icon = nullptr);
    static void Separator(const char* text = nullptr);

    static void InfoRow(const char* label, const char* value, const ImVec4* valueColor = nullptr);
    static void MetricCard(
        const char* id,
        const char* title,
        const std::string& value,
        const std::string& note,
        const ImVec4* valueColor,
        const ImVec4* noteColor,
        float width,
        float valueScale = 1.80f,
        bool withAccentBorder = true);
    static std::pair<std::string, bool> TruncateTextToPixelWidth(const char* text, float maxWidth);

    static float ThumbnailSize();
    static float ThumbnailCardWidth();
    static float GetThumbnailLayoutWidth(float blockWidth = 0.0f);
    static void CenterNextItem(float width);
    static bool ThumbnailButton(const char* id, const ImVec2& size, bool selected);
    static void DrawFittedTexturePreview(const TextureInfo& textureInfo, float innerPadding = 6.0f);
    static void DrawThumbnailTextOverlay(const char* text, ImU32 color);
    static void DrawThumbnailCaption(const char* label, bool selected);

    static void AddSoftGlowRect(
        ImDrawList* drawList,
        const ImVec2& min,
        const ImVec2& max,
        ImU32 color,
        float rounding,
        float glowSize,
        float intensity = 1.0f,
        int layers = 24,
        ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
        bool bypassClip = true);
    static void AddSoftGlowLine(
        ImDrawList* drawList,
        const ImVec2& start,
        const ImVec2& end,
        ImU32 color,
        float coreThickness,
        float glowSize,
        float intensity = 1.0f,
        int layers = 18,
        bool bypassClip = true);
    static void AddSoftGlowArc(
        ImDrawList* drawList,
        const ImVec2& center,
        float radius,
        float startAngle,
        float endAngle,
        ImU32 color,
        float coreThickness,
        float glowSize,
        float intensity = 1.0f,
        int layers = 18,
        int segments = 0,
        bool bypassClip = true);

    static bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool disabled = false);
    static bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool disabled = false);
    static bool GhostButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool disabled = false);
    static bool AccentOutlineButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool disabled = false);
    static bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool disabled = false);

    static bool Toggle(const char* label, bool* v, bool rightAlign = false);

    static void BeginTabs(const char* id);
    static bool TabItem(const char* label, bool active);
    static void EndTabs();

    static void BeginSubTabs(const char* id);
    static bool SubTabItem(const char* label, bool active, bool disabled = false);
    static void EndSubTabs();

    static bool SelectableItem(const char* id, const char* label, bool selected, const char* icon = nullptr);

    static bool InputText(const char* label, std::string* str, const char* placeholder = nullptr, float width = 0.0f);
    static bool InputInt(const char* label, int* v, const char* placeholder = nullptr, float width = 0.0f);
    static bool InputFloat(const char* label, float* v, const char* placeholder = nullptr, float width = 0.0f);
    static bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f", float width = 0.0f);

    static bool Combo(const char* label, int* current_item, const char* const items[], int items_count, float width = 0.0f);

    static bool ColorPicker(const char* label, ImVec4* color, const char* description = nullptr);

    static void ShowDemoWindow(bool* p_open = nullptr);
};

} // namespace keybinder
