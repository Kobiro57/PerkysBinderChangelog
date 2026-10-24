#include "../include/SpeedometerUI.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace keybinder {

namespace {
float g_speedometerRenderScale = 0.0f;

float Clamp01(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

float ClampNonNegative(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return (std::max)(0.0f, value);
}

float Lerp(float from, float to, float pct) {
    return from + (to - from) * Clamp01(pct);
}

ImVec4 WithAlpha(const ImVec4& color, float alpha) {
    ImVec4 adjusted = color;
    adjusted.w = alpha;
    return adjusted;
}

ImU32 ColorU32(ImVec4 color) {
    return ImGui::ColorConvertFloat4ToU32(color);
}

float UiScale() {
    return g_speedometerRenderScale > 0.0f ? g_speedometerRenderScale
        : (std::max)(1.0f, ThemeManager::Instance().GetAppliedScale());
}

float Scale(float value) {
    return value * UiScale();
}

ImVec2 PointOnArc(const ImVec2& center, float radius, float angle) {
    return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
}

int ArcSegmentCount(float radius, float startAngle, float endAngle) {
    const float arcLength = (std::max)(0.0f, endAngle - startAngle) * (std::max)(1.0f, radius);
    return (std::max)(48, (std::min)(160, static_cast<int>(arcLength / Scale(3.0f))));
}

void StrokeArc(ImDrawList* drawList, const ImVec2& center, float radius, float startAngle, float endAngle, ImU32 color, float thickness) {
    if (drawList == nullptr || endAngle <= startAngle || thickness <= 0.0f) {
        return;
    }

    drawList->PathArcTo(center, radius, startAngle, endAngle, ArcSegmentCount(radius, startAngle, endAngle));
    drawList->PathStroke(color, 0, thickness);
}


void StrokeArcSmooth(ImDrawList* drawList, const ImVec2& center, float radius, float startAngle, float endAngle, ImU32 color, float thickness) {
    if (drawList == nullptr || endAngle <= startAngle || thickness <= 0.0f || radius <= 0.0f) {
        return;
    }

    drawList->PathArcTo(center, radius, startAngle, endAngle, ArcSegmentCount(radius, startAngle, endAngle));
    drawList->PathStroke(color, 0, thickness);
}

void StrokeGaugeArc(ImDrawList* drawList, const ImVec2& center, float radius, float startAngle, float endAngle, const ImVec4& coreColor, float coreThickness, float coreAlpha = 1.0f) {
    if (drawList == nullptr || endAngle <= startAngle || radius <= 0.0f) {
        return;
    }

    StrokeArcSmooth(drawList, center, radius, startAngle, endAngle, ColorU32(WithAlpha(coreColor, coreAlpha)), coreThickness);
}


void DrawCenteredText(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& center, ImU32 color, const char* text) {
    if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    drawList->AddText(font, fontSize, ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f), color, text);
}

void DrawCenteredTextShadow(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& center, ImU32 color, ImU32 shadowColor, const char* text) {
    DrawCenteredText(drawList, font, fontSize, ImVec2(center.x + Scale(1.6f), center.y + Scale(2.0f)), shadowColor, text);
    DrawCenteredText(drawList, font, fontSize, center, color, text);
}

void DrawCenteredTextShadowGlow(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& center, ImU32 color, ImU32 shadowColor, ImU32 glowColor, const char* text) {
    DrawCenteredText(drawList, font, fontSize, ImVec2(center.x + Scale(1.7f), center.y + Scale(2.2f)), shadowColor, text);
    DrawCenteredText(drawList, font, fontSize, ImVec2(center.x, center.y - Scale(0.8f)), glowColor, text);
    DrawCenteredText(drawList, font, fontSize, center, color, text);
}

void DrawTextShadow(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& pos, ImU32 color, ImU32 shadowColor, const char* text) {
    if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    drawList->AddText(font, fontSize, ImVec2(pos.x + Scale(1.8f), pos.y + Scale(2.2f)), shadowColor, text);
    drawList->AddText(font, fontSize, pos, color, text);
}

void DrawTextShadowGlow(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& pos, ImU32 color, ImU32 shadowColor, ImU32 glowColor, const char* text) {
    if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    drawList->AddText(font, fontSize, ImVec2(pos.x + Scale(1.9f), pos.y + Scale(2.4f)), shadowColor, text);
    drawList->AddText(font, fontSize, ImVec2(pos.x, pos.y - Scale(0.8f)), glowColor, text);
    drawList->AddText(font, fontSize, pos, color, text);
}

void DrawRightAlignedTextShadow(ImDrawList* drawList, ImFont* font, float fontSize, const ImVec2& topRight, ImU32 color, ImU32 shadowColor, const char* text) {
    if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    DrawTextShadow(drawList, font, fontSize, ImVec2(topRight.x - size.x, topRight.y), color, shadowColor, text);
}

void DrawRightAlignedCenteredTextShadow(ImDrawList* drawList, ImFont* font, float fontSize, float rightX, float centerY, ImU32 color, ImU32 shadowColor, const char* text) {
    if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    DrawTextShadow(drawList, font, fontSize, ImVec2(rightX - size.x, centerY - size.y * 0.5f), color, shadowColor, text);
}

void DrawSlantedBlock(ImDrawList* drawList, const ImVec2& min, const ImVec2& size, float slant, ImU32 color) {
    if (drawList == nullptr || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const ImVec2 a(min.x + slant, min.y);
    const ImVec2 b(min.x + size.x + slant, min.y);
    const ImVec2 c(min.x + size.x - slant, min.y + size.y);
    const ImVec2 d(min.x - slant, min.y + size.y);
    drawList->AddQuadFilled(a, b, c, d, color);
}

void DrawSlantedSegmentedBar(
    ImDrawList* drawList,
    const ImVec2& min,
    const ImVec2& size,
    float pct,
    int segments,
    float gap,
    float slant,
    ImU32 fillColor,
    ImU32 inactiveColor) {
    if (drawList == nullptr || segments <= 0 || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const float segmentGap = (std::max)(0.0f, gap);
    const float segmentWidth = (size.x - segmentGap * static_cast<float>(segments - 1)) / static_cast<float>(segments);
    if (segmentWidth <= 0.0f) {
        return;
    }

    const float clampedPct = Clamp01(pct);
    const float activeUnits = clampedPct * static_cast<float>(segments);
    for (int segment = 0; segment < segments; ++segment) {
        const float x = min.x + static_cast<float>(segment) * (segmentWidth + segmentGap);
        const ImVec2 segmentMin(x, min.y);
        const ImVec2 segmentSize(segmentWidth, size.y);
        DrawSlantedBlock(
            drawList,
            segmentMin,
            segmentSize,
            slant,
            inactiveColor);

        const float segmentFill = Clamp01(activeUnits - static_cast<float>(segment));
        if (segmentFill <= 0.0f) {
            continue;
        }

        DrawSlantedBlock(
            drawList,
            segmentMin,
            ImVec2(segmentWidth * segmentFill, size.y),
            slant,
            fillColor);
    }
}

void DrawMinimalEngineIcon(ImDrawList* drawList, const ImVec2& center, ImU32 color, float sizeScale = 1.0f) {
    if (drawList == nullptr) {
        return;
    }

    const float scale = UiScale() * 0.62f * sizeScale;
    const float thickness = 2.5f * scale;
    const ImVec2 bodyMin(center.x - 17.0f * scale, center.y - 7.0f * scale);
    const ImVec2 bodyMax(center.x + 18.0f * scale, center.y + 13.0f * scale);

    drawList->AddRect(bodyMin, bodyMax, color, 2.0f * scale, 0, thickness);
    drawList->AddLine(ImVec2(center.x - 9.0f * scale, center.y - 7.0f * scale), ImVec2(center.x - 9.0f * scale, center.y - 15.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(center.x - 9.0f * scale, center.y - 15.0f * scale), ImVec2(center.x + 8.0f * scale, center.y - 15.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(center.x + 8.0f * scale, center.y - 15.0f * scale), ImVec2(center.x + 8.0f * scale, center.y - 7.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(center.x - 3.0f * scale, center.y - 18.0f * scale), ImVec2(center.x + 3.0f * scale, center.y - 18.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(center.x, center.y - 18.0f * scale), ImVec2(center.x, center.y - 15.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(bodyMin.x, center.y + 1.0f * scale), ImVec2(bodyMin.x - 7.0f * scale, center.y + 1.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(bodyMin.x - 7.0f * scale, center.y + 1.0f * scale), ImVec2(bodyMin.x - 7.0f * scale, center.y + 10.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(bodyMax.x, center.y + 2.0f * scale), ImVec2(bodyMax.x + 5.0f * scale, center.y + 2.0f * scale), color, thickness);
    drawList->AddLine(ImVec2(bodyMax.x + 5.0f * scale, center.y + 2.0f * scale), ImVec2(bodyMax.x + 5.0f * scale, center.y - 5.0f * scale), color, thickness);
}


void DrawSegmentedBar(ImDrawList* drawList, const ImVec2& min, const ImVec2& size, float pct, int segments, ImU32 fillColor, ImU32 inactiveColor, ImU32 outlineColor) {
    if (drawList == nullptr || segments <= 0 || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const float gap = Scale(3.0f);
    const float segmentWidth = (size.x - gap * static_cast<float>(segments - 1)) / static_cast<float>(segments);
    const int activeSegments = static_cast<int>(std::ceil(Clamp01(pct) * static_cast<float>(segments)));

    for (int segment = 0; segment < segments; ++segment) {
        const float x = min.x + static_cast<float>(segment) * (segmentWidth + gap);
        const ImVec2 segmentMin(x, min.y);
        const ImVec2 segmentMax(x + segmentWidth, min.y + size.y);
        drawList->AddRectFilled(segmentMin, segmentMax, segment < activeSegments ? fillColor : inactiveColor, Scale(1.5f));
    }

    drawList->AddRect(min, ImVec2(min.x + size.x, min.y + size.y), outlineColor, Scale(2.0f), 0, Scale(1.0f));
}

void DrawMinimalBar(ImDrawList* drawList, const ImVec2& min, const ImVec2& size, float pct, ImU32 fillColor, ImU32 trackColor, ImU32 outlineColor) {
    if (drawList == nullptr || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const float rounding = Scale(4.0f);
    const ImVec2 max(min.x + size.x, min.y + size.y);
    drawList->AddRectFilled(min, max, trackColor, rounding);

    const float fillWidth = size.x * Clamp01(pct);
    if (fillWidth > Scale(1.0f)) {
        drawList->AddRectFilled(min, ImVec2(min.x + fillWidth, max.y), fillColor, rounding);
    }

    drawList->AddRect(min, max, outlineColor, rounding, 0, Scale(1.0f));
}

void DrawFuelIcon(ImDrawList* drawList, const ImVec2& center, ImU32 color, float sizeScale = 1.0f) {
    if (drawList == nullptr) {
        return;
    }

    const float width = Scale(15.0f * sizeScale);
    const float height = Scale(21.0f * sizeScale);
    const ImVec2 bodyMin(center.x - width * 0.5f, center.y - height * 0.5f);
    const ImVec2 bodyMax(center.x + width * 0.35f, center.y + height * 0.5f);
    drawList->AddRect(bodyMin, bodyMax, color, Scale(2.0f * sizeScale), 0, Scale(2.0f * sizeScale));
    drawList->AddLine(ImVec2(bodyMin.x + Scale(3.0f * sizeScale), bodyMin.y + Scale(5.0f * sizeScale)), ImVec2(bodyMax.x - Scale(3.0f * sizeScale), bodyMin.y + Scale(5.0f * sizeScale)), color, Scale(1.5f * sizeScale));
    drawList->AddLine(ImVec2(bodyMax.x, bodyMin.y + Scale(5.0f * sizeScale)), ImVec2(center.x + width * 0.55f, center.y - Scale(2.0f * sizeScale)), color, Scale(2.0f * sizeScale));
    drawList->AddLine(ImVec2(center.x + width * 0.55f, center.y - Scale(2.0f * sizeScale)), ImVec2(center.x + width * 0.55f, center.y + Scale(7.0f * sizeScale)), color, Scale(2.0f * sizeScale));
}

void DrawCarIcon(ImDrawList* drawList, const ImVec2& center, ImU32 color) {
    if (drawList == nullptr) {
        return;
    }

    const float width = Scale(25.0f);
    const float height = Scale(15.0f);
    const ImVec2 bodyMin(center.x - width * 0.5f, center.y - height * 0.15f);
    const ImVec2 bodyMax(center.x + width * 0.5f, center.y + height * 0.5f);
    drawList->AddRect(bodyMin, bodyMax, color, Scale(3.0f), 0, Scale(2.0f));
    drawList->AddLine(ImVec2(center.x - width * 0.25f, bodyMin.y), ImVec2(center.x - width * 0.10f, center.y - height * 0.5f), color, Scale(2.0f));
    drawList->AddLine(ImVec2(center.x - width * 0.10f, center.y - height * 0.5f), ImVec2(center.x + width * 0.22f, center.y - height * 0.5f), color, Scale(2.0f));
    drawList->AddLine(ImVec2(center.x + width * 0.22f, center.y - height * 0.5f), ImVec2(center.x + width * 0.36f, bodyMin.y), color, Scale(2.0f));
    drawList->AddCircleFilled(ImVec2(center.x - width * 0.28f, bodyMax.y), Scale(3.0f), color, 12);
    drawList->AddCircleFilled(ImVec2(center.x + width * 0.28f, bodyMax.y), Scale(3.0f), color, 12);
}

} // namespace

ImVec4 SpeedometerUI::GetHealthColor(float healthPct) {
    auto& theme = ThemeManager::Instance();
    if (healthPct <= 0.25f) {
        return theme.GetColor(ThemeColorRole::Danger);
    }
    if (healthPct <= 0.50f) {
        return theme.GetColor(ThemeColorRole::Warning);
    }
    return theme.GetColor(ThemeColorRole::Success);
}

ImVec4 SpeedometerUI::GetFuelColor(float fuelPct) {
    auto& theme = ThemeManager::Instance();
    if (fuelPct <= 0.15f) {
        return theme.GetColor(ThemeColorRole::Danger);
    }
    if (fuelPct <= 0.35f) {
        return theme.GetColor(ThemeColorRole::Warning);
    }
    return theme.GetColor(ThemeColorRole::Success);
}

void SpeedometerUI::Render(const speedometer::SpeedometerState& state, speedometer::SpeedometerStyle style) {
    if (!state.isInVehicle) {
        displayedArcPct_ = 0.0f;
        displayedMinimalFuelPct_ = 0.0f;
        displayedMinimalHealthPct_ = 0.0f;
        return;
    }
    if (style != lastStyle_) displayedArcPct_ = 0.0f;
    lastStyle_ = style;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) return;
    const bool minimal = style == speedometer::SpeedometerStyle::MinimalDigital;
    const float baseWidth = minimal ? 632.0f : 434.0f;
    const float baseHeight = minimal ? 190.0f : 356.0f;
    const float scale = (std::max)(0.1f, (std::min)((std::max)(1.0f, ThemeManager::Instance().GetAppliedScale()),
        (std::min)(viewport->Size.x / baseWidth, viewport->Size.y / baseHeight)));
    struct RestoreScale {
        float previous;
        ~RestoreScale() { g_speedometerRenderScale = previous; }
    } restoreScale{g_speedometerRenderScale};
    g_speedometerRenderScale = scale;
    switch (style) {
    case speedometer::SpeedometerStyle::TachometerArc:
        RenderTachometerArc(state);
        return;
    case speedometer::SpeedometerStyle::MinimalDigital:
        RenderMinimalDigital(state);
        return;
    case speedometer::SpeedometerStyle::AnalogArc:
    default:
        RenderAnalogArc(state);
        return;
    }
}


void SpeedometerUI::RenderTachometerArc(const speedometer::SpeedometerState& state) {
    RenderArcGauge(state, false);
}

void SpeedometerUI::RenderAnalogArc(const speedometer::SpeedometerState& state) {
    RenderArcGauge(state, true);
}

void SpeedometerUI::RenderMinimalDigital(const speedometer::SpeedometerState& state) {
    if (!state.isInVehicle) {
        displayedMinimalFuelPct_ = 0.0f;
        displayedMinimalHealthPct_ = 0.0f;
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        displayedMinimalFuelPct_ = 0.0f;
        displayedMinimalHealthPct_ = 0.0f;
        return;
    }

    const speedometer::VehicleTelemetry& telemetry = state.telemetry;
    const bool hasFuel = telemetry.hasFuel;
    auto& theme = ThemeManager::Instance();
    const float width = Scale(588.0f);
    const float height = Scale(156.0f);
    const ImVec2 anchor(viewport->Pos.x + viewport->Size.x - Scale(44.0f), viewport->Pos.y + viewport->Size.y - Scale(34.0f));

    ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##SpeedometerMinimalDigital", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec2 origin = ImGui::GetCursorScreenPos();

        const ImVec4 pureWhite(0.96f, 0.98f, 1.00f, 1.0f);
        const ImVec4 mutedWhite(0.62f, 0.68f, 0.70f, 1.0f);
        const ImVec4 deepTrack(0.08f, 0.09f, 0.10f, 1.0f);
        const ImVec4 alertRed(1.00f, 0.08f, 0.05f, 1.0f);
        const ImVec4 accentCyan(0.00f, 0.92f, 1.00f, 1.0f);
        const ImVec4 dividerGray(0.36f, 0.40f, 0.42f, 1.0f);
        const ImVec4 background = theme.GetColor(ThemeColorRole::Background);
        const ImU32 textColor = ColorU32(pureWhite);
        const ImU32 mutedTextColor = ColorU32(WithAlpha(mutedWhite, 0.82f));
        const ImU32 inactiveColor = ColorU32(WithAlpha(deepTrack, 0.86f));
        const ImU32 dividerColor = ColorU32(WithAlpha(dividerGray, 0.78f));
        const ImU32 shadowTextColor = ColorU32(WithAlpha(background, 0.88f));
        const ImU32 speedGlowColor = ColorU32(WithAlpha(accentCyan, 0.18f));
        const ImU32 redTextColor = ColorU32(WithAlpha(alertRed, 0.86f));

        DrawSlantedBlock(
            drawList,
            ImVec2(origin.x + Scale(20.0f), origin.y + Scale(22.0f)),
            ImVec2(Scale(10.0f), Scale(104.0f)),
            Scale(8.5f),
            dividerColor);
        const int speed = static_cast<int>(std::round(ClampNonNegative(telemetry.speedKmh)));
        char speedText[16] = {};
        std::snprintf(speedText, sizeof(speedText), "%d", speed);
        DrawTextShadowGlow(drawList, font, Scale(90.0f), ImVec2(origin.x + Scale(62.0f), origin.y + Scale(10.0f)), textColor, shadowTextColor, speedGlowColor, speedText);
        DrawTextShadow(drawList, font, Scale(25.0f), ImVec2(origin.x + Scale(68.0f), origin.y + Scale(99.0f)), mutedTextColor, shadowTextColor, "KMH");

        const float targetFuelPct = hasFuel ? Clamp01(telemetry.fuelPct) : 0.0f;
        const float targetHealthPct = Clamp01(telemetry.dlPct);
        const float deltaTime = (std::max)(ImGui::GetIO().DeltaTime, 1.0f / 120.0f);
        const float response = 1.0f - std::exp(-deltaTime * 12.0f);
        displayedMinimalFuelPct_ = Lerp(displayedMinimalFuelPct_, targetFuelPct, response);
        displayedMinimalHealthPct_ = Lerp(displayedMinimalHealthPct_, targetHealthPct, response);

        const ImU32 fuelBarColor = hasFuel ? ColorU32(WithAlpha(pureWhite, 0.90f)) : mutedTextColor;
        const ImU32 healthBarColor = ColorU32(WithAlpha(pureWhite, 0.90f));
        const ImU32 fuelIconColor = hasFuel ? ColorU32(WithAlpha(pureWhite, 0.86f)) : mutedTextColor;
        const ImVec4 engineColor = telemetry.engineOn ? theme.GetColor(ThemeColorRole::Success) : theme.GetColor(ThemeColorRole::Danger);
        const ImU32 engineIconColor = ColorU32(WithAlpha(engineColor, 0.82f));

        const float rightPanelX = origin.x + Scale(236.0f);
        const float barX = origin.x + Scale(286.0f);
        const float fuelBarY = origin.y + Scale(44.0f);
        const float barWidth = Scale(202.0f);
        const float barHeight = Scale(16.0f);
        const float healthBarY = origin.y + Scale(106.0f);
        const ImVec4 fuelStatusColor = hasFuel ? GetFuelColor(displayedMinimalFuelPct_) : mutedWhite;
        const ImVec4 healthStatusColor = GetHealthColor(displayedMinimalHealthPct_);

        // Soft halo hugs only the lit portion of the bar -- no glow leaking around the icon
        // or past the empty segments, so the row stays clean.
        const float fillGlowHPad = Scale(3.0f);
        const float fillGlowVPad = Scale(4.0f);
        const float fillGlowRounding = Scale(6.0f);
        const float fillGlowSize = Scale(10.0f);

        const float fuelFillWidth = barWidth * displayedMinimalFuelPct_;
        const float fuelGlowIntensity = hasFuel ? 0.30f + 0.18f * displayedMinimalFuelPct_ : 0.0f;
        if (fuelGlowIntensity > 0.0f && fuelFillWidth > Scale(2.0f)) {
            UIComponents::AddSoftGlowRect(
                drawList,
                ImVec2(barX - fillGlowHPad, fuelBarY - fillGlowVPad),
                ImVec2(barX + fuelFillWidth + fillGlowHPad, fuelBarY + barHeight + fillGlowVPad),
                ColorU32(WithAlpha(fuelStatusColor, 0.85f)),
                fillGlowRounding,
                fillGlowSize,
                fuelGlowIntensity,
                8);
        }
        DrawFuelIcon(drawList, ImVec2(rightPanelX, fuelBarY + barHeight * 0.5f), fuelIconColor, 1.08f);
        DrawSlantedSegmentedBar(
            drawList,
            ImVec2(barX, fuelBarY),
            ImVec2(barWidth, barHeight),
            displayedMinimalFuelPct_,
            10,
            Scale(4.0f),
            Scale(4.0f),
            fuelBarColor,
            inactiveColor);
        const float fuelScaleLineY = fuelBarY + barHeight + Scale(8.0f);
        const float fuelEmptyLabelX = barX - Scale(10.0f);
        const float fuelFullLabelRightX = barX + barWidth + Scale(4.0f);
        drawList->AddLine(
            ImVec2(fuelEmptyLabelX, fuelScaleLineY),
            ImVec2(fuelFullLabelRightX, fuelScaleLineY),
            ColorU32(WithAlpha(deepTrack, 0.68f)),
            Scale(1.6f));
        DrawTextShadow(drawList, font, Scale(22.0f), ImVec2(fuelEmptyLabelX, fuelBarY + Scale(24.0f)), redTextColor, shadowTextColor, "E");
        DrawRightAlignedTextShadow(drawList, font, Scale(22.0f), ImVec2(fuelFullLabelRightX, fuelBarY + Scale(24.0f)), fuelBarColor, shadowTextColor, "F");
        DrawRightAlignedCenteredTextShadow(
            drawList,
            font,
            Scale(28.0f),
            origin.x + width - Scale(10.0f),
            fuelBarY + barHeight * 0.5f,
            fuelBarColor,
            shadowTextColor,
            hasFuel ? state.fuelDisplayText : "N/A");

        const float healthFillWidth = barWidth * displayedMinimalHealthPct_;
        const float healthGlowIntensity = 0.30f + 0.18f * displayedMinimalHealthPct_;
        if (healthFillWidth > Scale(2.0f)) {
            UIComponents::AddSoftGlowRect(
                drawList,
                ImVec2(barX - fillGlowHPad, healthBarY - fillGlowVPad),
                ImVec2(barX + healthFillWidth + fillGlowHPad, healthBarY + barHeight + fillGlowVPad),
                ColorU32(WithAlpha(healthStatusColor, 0.85f)),
                fillGlowRounding,
                fillGlowSize,
                healthGlowIntensity,
                8);
        }
        DrawMinimalEngineIcon(drawList, ImVec2(rightPanelX, healthBarY + barHeight * 0.5f), engineIconColor);
        DrawSlantedSegmentedBar(
            drawList,
            ImVec2(barX, healthBarY),
            ImVec2(barWidth, barHeight),
            displayedMinimalHealthPct_,
            18,
            Scale(3.5f),
            Scale(4.0f),
            healthBarColor,
            inactiveColor);
        DrawRightAlignedCenteredTextShadow(
            drawList,
            font,
            Scale(28.0f),
            origin.x + width - Scale(10.0f),
            healthBarY + barHeight * 0.5f,
            healthBarColor,
            shadowTextColor,
            state.healthText);

        ImGui::Dummy(ImVec2(width, height));
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void SpeedometerUI::RenderArcGauge(const speedometer::SpeedometerState& state, bool analogVariant) {
    if (!state.isInVehicle) {
        displayedArcPct_ = 0.0f;
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        displayedArcPct_ = 0.0f;
        return;
    }

    const speedometer::VehicleTelemetry& telemetry = state.telemetry;
    const bool hasFuel = telemetry.hasFuel;
    auto& theme = ThemeManager::Instance();
    const float width = Scale(390.0f);
    const float height = Scale(328.0f);
    const ImVec2 anchor(viewport->Pos.x + viewport->Size.x - Scale(44.0f), viewport->Pos.y + viewport->Size.y - Scale(28.0f));

    ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    const char* windowId = analogVariant ? "##SpeedometerAnalogArc" : "##SpeedometerTachometerArc";
    if (ImGui::Begin(windowId, nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 center(origin.x + width * 0.5f, origin.y + Scale(172.0f));
        const float radius = Scale(142.0f);
        const float arcThickness = Scale(8.0f);
        constexpr float kPi = 3.14159265358979323846f;
        const float startAngle = kPi * 0.86f;
        const float endAngle = kPi * 2.14f;
        const float sweepAngle = endAngle - startAngle;

        const float deltaTime = (std::max)(ImGui::GetIO().DeltaTime, 1.0f / 120.0f);
        const float currentSpeed = ClampNonNegative(telemetry.speedKmh);
        const float targetArcPct = analogVariant ? Clamp01(telemetry.speedPct) : Clamp01(telemetry.estimatedRpmPct);
        const float response = 1.0f - std::exp(-deltaTime * (targetArcPct > displayedArcPct_ ? 12.0f : 6.0f));
        displayedArcPct_ = Lerp(displayedArcPct_, targetArcPct, response);

        const ImVec4 accentCyan(0.00f, 0.92f, 1.00f, 1.0f);
        const ImVec4 indicatorOrange(1.00f, 0.48f, 0.08f, 1.0f);
        const ImVec4 glassWhite(0.92f, 0.98f, 1.00f, 1.0f);
        const ImVec4 alertRed(1.00f, 0.04f, 0.06f, 1.0f);
        const ImU32 inactiveArcColor = ColorU32(WithAlpha(glassWhite, analogVariant ? 0.12f : 0.16f));
        const ImU32 redlineColor = ColorU32(WithAlpha(alertRed, 0.88f));
        const ImU32 tickColor = ColorU32(WithAlpha(glassWhite, 0.88f));
        const ImU32 minorTickColor = ColorU32(WithAlpha(glassWhite, analogVariant ? 0.22f : 0.30f));
        const ImU32 textColor = ColorU32(WithAlpha(glassWhite, 0.98f));
        const ImU32 shadowTextColor = ColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Background), 0.82f));
        const ImU32 mutedTextColor = ColorU32(WithAlpha(glassWhite, 0.56f));
        const ImU32 statusMutedTextColor = ColorU32(WithAlpha(glassWhite, 0.46f));
        const ImU32 inactiveBarColor = ColorU32(WithAlpha(glassWhite, 0.12f));
        const ImU32 outlineColor = ColorU32(WithAlpha(indicatorOrange, 0.24f));
        const ImU32 speedGlowColor = ColorU32(WithAlpha(accentCyan, analogVariant ? 0.16f : 0.22f));

        StrokeArcSmooth(drawList, center, radius, startAngle, endAngle, inactiveArcColor, arcThickness);
        const float redlineStartAngle = startAngle + sweepAngle * (6.0f / 8.0f);
        // Redline marker sits on a slimmer inner band so it never stacks thickness against the
        // active stroke -- avoids the hard cutoff the previous design produced at the boundary.
        const float redlineMarkerRadius = radius - arcThickness * 0.5f - Scale(4.0f);
        if (!analogVariant) {
            StrokeGaugeArc(drawList, center, redlineMarkerRadius, redlineStartAngle, endAngle, alertRed, Scale(1.6f), 0.55f);
        }

        const float activeEndAngle = startAngle + sweepAngle * displayedArcPct_;
        if (activeEndAngle > startAngle + 0.01f) {
            if (!analogVariant) {
                // Blend orange -> red as the needle pushes into the redline. One color, one stroke,
                // one underglow -- no overlapping cap glows, no thickness steps at the boundary.
                const float redlineMix = Clamp01((displayedArcPct_ - 0.75f) / 0.18f);
                const ImVec4 activeColor(
                    indicatorOrange.x + (alertRed.x - indicatorOrange.x) * redlineMix,
                    indicatorOrange.y + (alertRed.y - indicatorOrange.y) * redlineMix,
                    indicatorOrange.z + (alertRed.z - indicatorOrange.z) * redlineMix,
                    1.0f);
                const float coreThickness = Scale(4.8f);

                UIComponents::AddSoftGlowArc(
                    drawList,
                    center,
                    radius,
                    startAngle,
                    activeEndAngle,
                    ColorU32(WithAlpha(activeColor, 0.78f + 0.10f * redlineMix)),
                    coreThickness,
                    Scale(26.0f),
                    0.46f + 0.10f * redlineMix,
                    20);

                StrokeGaugeArc(drawList, center, radius, startAngle, activeEndAngle, activeColor, coreThickness, 0.96f);

                if (activeEndAngle > redlineStartAngle + 0.01f) {
                    StrokeGaugeArc(
                        drawList,
                        center,
                        redlineMarkerRadius,
                        redlineStartAngle,
                        activeEndAngle,
                        alertRed,
                        Scale(1.8f),
                        0.92f);
                }

                // Leading-edge cap -- a short feathered arc that tapers slightly past the head
                // plus a tiny radial halo so the tip fades smoothly instead of cutting off hard.
                const float capGlowBack = (std::min)(sweepAngle * 0.030f, 0.090f);
                const float capGlowFwd = (std::min)(sweepAngle * 0.014f, 0.040f);
                const float capStart = (std::max)(startAngle, activeEndAngle - capGlowBack);
                const float capEnd = (std::min)(endAngle, activeEndAngle + capGlowFwd);
                if (capEnd - capStart > 0.001f) {
                    UIComponents::AddSoftGlowArc(
                        drawList,
                        center,
                        radius,
                        capStart,
                        capEnd,
                        ColorU32(WithAlpha(activeColor, 0.62f + 0.18f * redlineMix)),
                        coreThickness,
                        Scale(22.0f),
                        0.36f + 0.10f * redlineMix,
                        14);
                }

                const ImVec2 tip = PointOnArc(center, radius, activeEndAngle);
                drawList->AddCircleFilled(tip, Scale(7.5f), ColorU32(WithAlpha(activeColor, 0.16f)), 24);
                drawList->AddCircleFilled(tip, Scale(4.6f), ColorU32(WithAlpha(activeColor, 0.34f)), 22);
                drawList->AddCircleFilled(tip, Scale(2.6f), ColorU32(WithAlpha(activeColor, 0.78f)), 20);
            } else {
                StrokeGaugeArc(drawList, center, radius, startAngle, activeEndAngle, glassWhite, Scale(2.8f), 0.72f);
            }
        }

        const int totalTicks = analogVariant ? 30 : 40;
        const int majorTickStep = 5;
        for (int tick = 0; tick <= totalTicks; ++tick) {
            const bool majorTick = tick % majorTickStep == 0;
            const float tickPct = static_cast<float>(tick) / static_cast<float>(totalTicks);
            const float angle = startAngle + sweepAngle * tickPct;
            const float innerOffset = majorTick ? Scale(18.0f) : Scale(8.0f);
            const float outerOffset = majorTick ? Scale(7.0f) : Scale(2.0f);
            const ImVec2 inner = PointOnArc(center, radius - innerOffset, angle);
            const ImVec2 outer = PointOnArc(center, radius + outerOffset, angle);
            drawList->AddLine(inner, outer, majorTick ? tickColor : minorTickColor, majorTick ? Scale(2.5f) : Scale(1.0f));

            if (majorTick) {
                const int label = analogVariant ? tick * 10 : tick / majorTickStep;
                char tickText[4] = {};
                std::snprintf(tickText, sizeof(tickText), "%d", label);
                const ImU32 labelColor = (!analogVariant && label >= 6) ? redlineColor : tickColor;
                const ImVec2 labelCenter = PointOnArc(center, radius - Scale(39.0f), angle);
                DrawCenteredText(drawList, font, Scale(18.0f), ImVec2(labelCenter.x + Scale(1.0f), labelCenter.y + Scale(1.3f)), shadowTextColor, tickText);
                DrawCenteredText(drawList, font, Scale(18.0f), labelCenter, labelColor, tickText);
            }
        }

        if (analogVariant) {
            const float needleLength = radius - Scale(30.0f);
            const float tailLength = Scale(16.0f);
            const float baseHalfWidth = Scale(5.0f);
            const float tipInset = Scale(5.5f);
            const float tipShoulderInset = Scale(7.0f);
            const float tipHalfWidth = Scale(1.65f);
            const ImVec2 direction(std::cos(activeEndAngle), std::sin(activeEndAngle));
            const ImVec2 normal(-direction.y, direction.x);
            const ImVec2 tip(center.x + direction.x * (needleLength - tipInset), center.y + direction.y * (needleLength - tipInset));
            const ImVec2 tipShoulder(tip.x - direction.x * tipShoulderInset, tip.y - direction.y * tipShoulderInset);
            const ImVec2 leftTip(tipShoulder.x + normal.x * tipHalfWidth, tipShoulder.y + normal.y * tipHalfWidth);
            const ImVec2 rightTip(tipShoulder.x - normal.x * tipHalfWidth, tipShoulder.y - normal.y * tipHalfWidth);
            const ImVec2 tail(center.x - direction.x * tailLength, center.y - direction.y * tailLength);
            const ImVec2 leftBase(center.x + normal.x * baseHalfWidth, center.y + normal.y * baseHalfWidth);
            const ImVec2 rightBase(center.x - normal.x * baseHalfWidth, center.y - normal.y * baseHalfWidth);
            const ImVec2 shadowOffset(Scale(1.4f), Scale(2.0f));
            const ImVec2 glowStart(center.x + direction.x * Scale(13.0f), center.y + direction.y * Scale(13.0f));
            const ImVec2 glowEnd(tip.x - direction.x * Scale(8.0f), tip.y - direction.y * Scale(8.0f));
            const ImVec2 spineStart(center.x + direction.x * Scale(7.0f), center.y + direction.y * Scale(7.0f));
            const ImVec2 spineEnd(tipShoulder.x - direction.x * Scale(1.5f), tipShoulder.y - direction.y * Scale(1.5f));
            const ImU32 needleCore = ColorU32(WithAlpha(accentCyan, 0.86f));
            const ImU32 needleSpine = ColorU32(WithAlpha(accentCyan, 0.62f));
            const ImU32 needleGlow = ColorU32(WithAlpha(accentCyan, 0.42f));
            const ImU32 needleShadow = ColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Background), 0.52f));

            UIComponents::AddSoftGlowLine(
                drawList,
                glowStart,
                glowEnd,
                needleGlow,
                Scale(1.7f),
                Scale(18.0f),
                0.46f,
                14);
            drawList->AddCircleFilled(center, Scale(12.5f), ColorU32(WithAlpha(accentCyan, 0.10f)), 36);

            const ImVec2 shadowNeedle[6] = {
                ImVec2(tail.x + shadowOffset.x, tail.y + shadowOffset.y),
                ImVec2(rightBase.x + shadowOffset.x, rightBase.y + shadowOffset.y),
                ImVec2(rightTip.x + shadowOffset.x, rightTip.y + shadowOffset.y),
                ImVec2(tip.x + shadowOffset.x, tip.y + shadowOffset.y),
                ImVec2(leftTip.x + shadowOffset.x, leftTip.y + shadowOffset.y),
                ImVec2(leftBase.x + shadowOffset.x, leftBase.y + shadowOffset.y),
            };
            drawList->AddConvexPolyFilled(shadowNeedle, 6, needleShadow);

            const ImVec2 needleBody[6] = {
                tail,
                rightBase,
                rightTip,
                tip,
                leftTip,
                leftBase,
            };
            drawList->AddConvexPolyFilled(needleBody, 6, needleCore);
            drawList->AddLine(spineStart, spineEnd, needleSpine, Scale(1.15f));
            drawList->AddCircleFilled(center, Scale(9.4f), ColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Background), 0.90f)), 36);
            drawList->AddCircle(center, Scale(9.4f), ColorU32(WithAlpha(accentCyan, 0.68f)), 36, Scale(1.45f));
            drawList->AddCircleFilled(center, Scale(3.3f), ColorU32(WithAlpha(accentCyan, 0.78f)), 28);
        }

        char speedText[16] = {};
        std::snprintf(speedText, sizeof(speedText), "%d", static_cast<int>(std::round(currentSpeed)));
        const float speedValueY = analogVariant ? center.y + Scale(42.0f) : center.y - Scale(2.0f);
        const float speedUnitY = analogVariant ? center.y + Scale(88.0f) : center.y + Scale(50.0f);
        DrawCenteredTextShadowGlow(drawList, font, Scale(76.0f), ImVec2(center.x, speedValueY), textColor, shadowTextColor, speedGlowColor, speedText);
        DrawCenteredTextShadow(drawList, font, Scale(23.0f), ImVec2(center.x, speedUnitY), mutedTextColor, shadowTextColor, "km/h");
        if (!analogVariant) {
            // Caption sits above the speed digit so it cannot crowd the GEAR value below.
            DrawCenteredTextShadow(drawList, font, Scale(13.0f), ImVec2(center.x, center.y - Scale(58.0f)), statusMutedTextColor, shadowTextColor, "RPM \xC3\x97 1000");
        }

        const float statusLabelY = center.y + Scale(68.0f);
        const float statusValueY = center.y + Scale(88.0f);
        const float statusOffsetX = Scale(78.0f);
        DrawCenteredTextShadow(drawList, font, Scale(11.0f), ImVec2(center.x - statusOffsetX, statusLabelY), statusMutedTextColor, shadowTextColor, "ENG");
        DrawCenteredTextShadow(drawList, font, Scale(17.0f), ImVec2(center.x - statusOffsetX, statusValueY), ColorU32(WithAlpha(telemetry.engineOn ? theme.GetColor(ThemeColorRole::Success) : theme.GetColor(ThemeColorRole::Danger), 0.88f)), shadowTextColor, telemetry.engineOn ? "ON" : "OFF");
        DrawCenteredTextShadow(drawList, font, Scale(11.0f), ImVec2(center.x + statusOffsetX, statusLabelY), statusMutedTextColor, shadowTextColor, "GEAR");
        DrawCenteredTextShadow(drawList, font, Scale(21.0f), ImVec2(center.x + statusOffsetX, statusValueY + Scale(1.0f)), ColorU32(WithAlpha(glassWhite, 0.86f)), shadowTextColor, telemetry.gearText);

        const float fuelPct = hasFuel ? Clamp01(telemetry.fuelPct) : 0.0f;
        const float healthPct = Clamp01(telemetry.dlPct);
        const ImVec4 fuelColor = hasFuel ? GetFuelColor(fuelPct) : theme.GetColor(ThemeColorRole::TextMuted);
        const ImVec4 healthColor = GetHealthColor(healthPct);
        const ImU32 fuelBarColor = ColorU32(WithAlpha(fuelColor, hasFuel ? 0.86f : 0.38f));
        const ImU32 healthBarColor = ColorU32(WithAlpha(healthColor, 0.86f));
        const float barWidth = Scale(112.0f);
        const float barHeight = Scale(12.0f);
        const float barY = origin.y + Scale(286.0f);
        const float leftX = center.x - Scale(150.0f);
        const float rightX = center.x + Scale(42.0f);

        DrawFuelIcon(drawList, ImVec2(leftX - Scale(20.0f), barY + barHeight * 0.5f), hasFuel ? fuelBarColor : mutedTextColor);
        const float fuelFillWidth = barWidth * fuelPct;
        if (fuelFillWidth > Scale(1.0f)) {
            UIComponents::AddSoftGlowRect(
                drawList,
                ImVec2(leftX, barY),
                ImVec2(leftX + fuelFillWidth, barY + barHeight),
                ColorU32(WithAlpha(fuelColor, hasFuel ? 0.42f : 0.0f)),
                Scale(4.0f),
                Scale(14.0f),
                hasFuel ? 0.26f : 0.0f,
                14);
        }
        DrawSegmentedBar(drawList, ImVec2(leftX, barY), ImVec2(barWidth, barHeight), fuelPct, 10, fuelBarColor, inactiveBarColor, outlineColor);

        DrawCarIcon(drawList, ImVec2(rightX - Scale(24.0f), barY + barHeight * 0.5f), healthBarColor);
        const float healthFillWidth = barWidth * healthPct;
        if (healthFillWidth > Scale(1.0f)) {
            UIComponents::AddSoftGlowRect(
                drawList,
                ImVec2(rightX, barY),
                ImVec2(rightX + healthFillWidth, barY + barHeight),
                ColorU32(WithAlpha(healthColor, 0.42f)),
                Scale(4.0f),
                Scale(14.0f),
                0.28f,
                14);
        }
        DrawSegmentedBar(drawList, ImVec2(rightX, barY), ImVec2(barWidth, barHeight), healthPct, 10, healthBarColor, inactiveBarColor, outlineColor);

        ImGui::Dummy(ImVec2(width, height));
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

} // namespace keybinder
