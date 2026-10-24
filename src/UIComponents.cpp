#include "../include/UIComponents.h"
#include "../include/ThemeManager.h"
#include "../include/AnimationHelper.h"
#include "../include/TextureManager.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/imgui-1.92.7/imgui_internal.h"
#include "../include/imgui-1.92.7/misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>


namespace {

int ClampUtf8PrefixLength(const char* text, int length) {
    while (length > 0) {
        const unsigned char byte = static_cast<unsigned char>(text[length]);
        if ((byte & 0xC0) != 0x80) {
            break;
        }
        --length;
    }
    return length;
}

} // anonymous namespace
namespace keybinder {

std::pair<std::string, bool> UIComponents::TruncateTextToPixelWidth(const char* text, float maxWidth) {
    const char* safeText = text != nullptr ? text : "";
    if (safeText[0] == '\0') {
        return {"", false};
    }

    if (maxWidth <= 0.0f) {
        return {"...", true};
    }

    const ImVec2 textSize = ImGui::CalcTextSize(safeText, nullptr, true);
    if (textSize.x <= maxWidth) {
        return {std::string(safeText), false};
    }

    const ImVec2 ellipsisSize = ImGui::CalcTextSize("...", nullptr, true);
    const float targetWidth = maxWidth - ellipsisSize.x;
    if (targetWidth <= 0.0f) {
        return {"...", true};
    }

    const int textLength = static_cast<int>(std::strlen(safeText));
    int lo = 0;
    int hi = textLength;
    int bestLength = 0;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        const int prefixLength = ClampUtf8PrefixLength(safeText, mid);
        const ImVec2 prefixSize = ImGui::CalcTextSize(safeText, safeText + prefixLength, true);
        if (prefixSize.x <= targetWidth) {
            bestLength = prefixLength;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return {std::string(safeText, bestLength) + "...", true};
}

enum class ButtonVariant {
    Primary,
    Secondary,
    Ghost,
    AccentOutline,
    Danger
};

static ImVec4 WithAlpha(const ImVec4& color, float alpha) {
    ImVec4 adjusted = color;
    adjusted.w = alpha;
    return adjusted;
}

static ImVec4 ScaleAlpha(const ImVec4& color, float factor) {
    ImVec4 adjusted = color;
    adjusted.w *= factor;
    return adjusted;
}

static float ClampFinite(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        return minValue;
    }
    return (std::max)(minValue, (std::min)(maxValue, value));
}

static bool IsFiniteVec2(const ImVec2& value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

static bool PushBoundedEffectClipRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float effectExtent) {
    if (drawList == nullptr || !IsFiniteVec2(min) || !IsFiniteVec2(max) || !std::isfinite(effectExtent)) {
        return false;
    }

    const float padding = (std::max)(effectExtent, 0.0f) + 1.0f;
    ImVec2 displayMin(0.0f, 0.0f);
    ImVec2 displayMax = ImGui::GetIO().DisplaySize;
    if (ImGuiViewport* viewport = ImGui::GetMainViewport()) {
        displayMin = viewport->Pos;
        displayMax = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    }

    if (!IsFiniteVec2(displayMin) || !IsFiniteVec2(displayMax) || displayMax.x <= displayMin.x || displayMax.y <= displayMin.y) {
        return false;
    }

    ImVec2 clipMin(
        (std::max)(displayMin.x, min.x - padding),
        (std::max)(displayMin.y, min.y - padding));
    ImVec2 clipMax(
        (std::min)(displayMax.x, max.x + padding),
        (std::min)(displayMax.y, max.y + padding));

    if (drawList->_ClipRectStack.Size > 0) {
        const ImVec2 currentClipMin = drawList->GetClipRectMin();
        const ImVec2 currentClipMax = drawList->GetClipRectMax();
        if (IsFiniteVec2(currentClipMin) && IsFiniteVec2(currentClipMax) &&
            currentClipMax.x > currentClipMin.x && currentClipMax.y > currentClipMin.y) {
            clipMin.x = (std::max)(clipMin.x, currentClipMin.x);
            clipMin.y = (std::max)(clipMin.y, currentClipMin.y);
            clipMax.x = (std::min)(clipMax.x, currentClipMax.x);
            clipMax.y = (std::min)(clipMax.y, currentClipMax.y);
        }
    }

    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
        return false;
    }

    drawList->PushClipRect(clipMin, clipMax, false);
    return true;
}

static ImU32 WithAlphaMultiplier(ImU32 color, float multiplier) {
    if (multiplier <= 0.0f || !std::isfinite(multiplier)) {
        return color & ~IM_COL32_A_MASK;
    }

    ImVec4 adjusted = ImGui::ColorConvertU32ToFloat4(color);
    adjusted.w = ClampFinite(adjusted.w * multiplier, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(adjusted);
}

static ImVec4 BlendColor(const ImVec4& from, const ImVec4& to, float t) {
    return ImVec4(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t,
        from.w + (to.w - from.w) * t);
}

static void AddHorizontalFadeRect(
    ImDrawList* drawList,
    const ImVec2& min,
    const ImVec2& max,
    const ImVec4& startColor,
    const ImVec4& endColor) {
    drawList->AddRectFilledMultiColor(
        min,
        max,
        ImGui::GetColorU32(startColor),
        ImGui::GetColorU32(endColor),
        ImGui::GetColorU32(endColor),
        ImGui::GetColorU32(startColor));
}

void UIComponents::AddSoftGlowRect(
    ImDrawList* drawList,
    const ImVec2& min,
    const ImVec2& max,
    ImU32 color,
    float rounding,
    float glowSize,
    float intensity,
    int layers,
    ImDrawFlags flags,
    bool bypassClip) {
    if (drawList == nullptr || layers <= 0 || glowSize <= 0.0f || intensity <= 0.0f ||
        !IsFiniteVec2(min) || !IsFiniteVec2(max) || !std::isfinite(rounding) || !std::isfinite(glowSize) ||
        max.x <= min.x || max.y <= min.y || (color & IM_COL32_A_MASK) == 0) {
        return;
    }

    const int layerCount = (std::min)(layers, 32);
    const float safeIntensity = ClampFinite(intensity, 0.0f, 1.5f);
    if (safeIntensity <= 0.0f) {
        return;
    }

    const ImDrawListFlags oldFlags = drawList->Flags;
    drawList->Flags |= ImDrawListFlags_AntiAliasedFill;

    bool pushedEffectClip = false;
    if (bypassClip) {
        pushedEffectClip = PushBoundedEffectClipRect(drawList, min, max, glowSize);
        if (!pushedEffectClip) {
            drawList->Flags = oldFlags;
            return;
        }
    }

    for (int layer = layerCount; layer >= 1; --layer) {
        const float spreadT = static_cast<float>(layer) / static_cast<float>(layerCount);
        const float alphaT = 1.0f - ((static_cast<float>(layer) - 0.5f) / static_cast<float>(layerCount));
        const float smoothFalloff = alphaT * alphaT * (3.0f - 2.0f * alphaT);
        const float spread = glowSize * spreadT;
        const ImU32 layerColor = WithAlphaMultiplier(color, 0.052f * safeIntensity * smoothFalloff);
        if ((layerColor & IM_COL32_A_MASK) == 0) {
            continue;
        }

        drawList->AddRectFilled(
            ImVec2(min.x - spread, min.y - spread),
            ImVec2(max.x + spread, max.y + spread),
            layerColor,
            (std::max)(rounding + spread, 0.0f),
            flags);
    }

    if (pushedEffectClip) {
        drawList->PopClipRect();
    }
    drawList->Flags = oldFlags;
}

void UIComponents::AddSoftGlowLine(
    ImDrawList* drawList,
    const ImVec2& start,
    const ImVec2& end,
    ImU32 color,
    float coreThickness,
    float glowSize,
    float intensity,
    int layers,
    bool bypassClip) {
    if (drawList == nullptr || layers <= 0 || coreThickness <= 0.0f || glowSize <= 0.0f || intensity <= 0.0f ||
        !IsFiniteVec2(start) || !IsFiniteVec2(end) || !std::isfinite(coreThickness) || !std::isfinite(glowSize) ||
        (color & IM_COL32_A_MASK) == 0) {
        return;
    }

    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float lengthSq = dx * dx + dy * dy;
    if (lengthSq <= 0.01f) {
        return;
    }

    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    const ImVec2 direction(dx * inverseLength, dy * inverseLength);

    const int layerCount = (std::min)(layers, 28);
    const float safeIntensity = ClampFinite(intensity, 0.0f, 1.5f);
    if (safeIntensity <= 0.0f) {
        return;
    }

    const ImDrawListFlags oldFlags = drawList->Flags;
    drawList->Flags |= ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill;

    bool pushedEffectClip = false;
    if (bypassClip) {
        const ImVec2 clipMin((std::min)(start.x, end.x), (std::min)(start.y, end.y));
        const ImVec2 clipMax((std::max)(start.x, end.x), (std::max)(start.y, end.y));
        pushedEffectClip = PushBoundedEffectClipRect(drawList, clipMin, clipMax, coreThickness + glowSize);
        if (!pushedEffectClip) {
            drawList->Flags = oldFlags;
            return;
        }
    }

    for (int layer = layerCount; layer >= 1; --layer) {
        const float spreadT = static_cast<float>(layer) / static_cast<float>(layerCount);
        const float alphaT = 1.0f - ((static_cast<float>(layer) - 0.5f) / static_cast<float>(layerCount));
        const float smoothFalloff = alphaT * alphaT * (3.0f - 2.0f * alphaT);
        const float spread = glowSize * spreadT;
        const float thickness = coreThickness + spread;
        const float extension = thickness * 0.80f;
        const ImVec2 layerStart(start.x - direction.x * extension, start.y - direction.y * extension);
        const ImVec2 layerEnd(end.x + direction.x * extension, end.y + direction.y * extension);
        const ImU32 layerColor = WithAlphaMultiplier(color, 0.070f * safeIntensity * smoothFalloff);
        const ImU32 capColor = WithAlphaMultiplier(color, 0.016f * safeIntensity * smoothFalloff);
        if ((layerColor & IM_COL32_A_MASK) == 0) {
            continue;
        }

        drawList->AddLine(layerStart, layerEnd, layerColor, thickness);
        if ((capColor & IM_COL32_A_MASK) != 0) {
            drawList->AddCircleFilled(layerStart, thickness * 0.42f, capColor, 24);
            drawList->AddCircleFilled(layerEnd, thickness * 0.42f, capColor, 24);
        }
    }

    if (pushedEffectClip) {
        drawList->PopClipRect();
    }
    drawList->Flags = oldFlags;
}

void UIComponents::AddSoftGlowArc(
    ImDrawList* drawList,
    const ImVec2& center,
    float radius,
    float startAngle,
    float endAngle,
    ImU32 color,
    float coreThickness,
    float glowSize,
    float intensity,
    int layers,
    int segments,
    bool bypassClip) {
    if (drawList == nullptr || layers <= 0 || radius <= 0.0f || coreThickness <= 0.0f ||
        glowSize <= 0.0f || intensity <= 0.0f || endAngle <= startAngle || !IsFiniteVec2(center) ||
        !std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(endAngle) ||
        !std::isfinite(coreThickness) || !std::isfinite(glowSize) || (color & IM_COL32_A_MASK) == 0) {
        return;
    }

    const int layerCount = (std::min)(layers, 28);
    const int safeSegments = segments > 0 ? (std::min)(segments, 256) : 0;
    const float safeIntensity = ClampFinite(intensity, 0.0f, 1.5f);
    if (safeIntensity <= 0.0f) {
        return;
    }

    const ImDrawListFlags oldFlags = drawList->Flags;
    drawList->Flags |= ImDrawListFlags_AntiAliasedLines;

    bool pushedEffectClip = false;
    if (bypassClip) {
        const float effectRadius = radius + coreThickness + glowSize;
        pushedEffectClip = PushBoundedEffectClipRect(
            drawList,
            ImVec2(center.x - effectRadius, center.y - effectRadius),
            ImVec2(center.x + effectRadius, center.y + effectRadius),
            0.0f);
        if (!pushedEffectClip) {
            drawList->Flags = oldFlags;
            return;
        }
    }

    for (int layer = layerCount; layer >= 1; --layer) {
        const float spreadT = static_cast<float>(layer) / static_cast<float>(layerCount);
        const float alphaT = 1.0f - ((static_cast<float>(layer) - 0.5f) / static_cast<float>(layerCount));
        const float smoothFalloff = alphaT * alphaT * (3.0f - 2.0f * alphaT);
        const float spread = glowSize * spreadT;
        const ImU32 layerColor = WithAlphaMultiplier(color, 0.074f * safeIntensity * smoothFalloff);
        if ((layerColor & IM_COL32_A_MASK) == 0) {
            continue;
        }

        drawList->PathArcTo(center, radius + spread * 0.5f, startAngle, endAngle, safeSegments);
        drawList->PathStroke(layerColor, 0, coreThickness + spread);
    }

    if (pushedEffectClip) {
        drawList->PopClipRect();
    }
    drawList->Flags = oldFlags;
}

static void DrawPanelDecor(const ImRect& bb, float radius, bool withAccentBorder) {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float uiScale = (std::max)(theme.GetAppliedScale(), 1.0f);
    const float opacityScale = 1.0f - theme.GetAppliedTransparency();

    const ImVec4 surfaceAlt = theme.GetColor(ThemeColorRole::SurfaceAlt);
    const ImVec4 elevated = theme.GetColor(ThemeColorRole::SurfaceElevated);
    const ImVec4 accent = theme.GetColor(ThemeColorRole::Accent);
    const ImVec4 text = theme.GetColor(ThemeColorRole::TextPrimary);

    // ---------- 1. Outer drop shadow (gives the card lift over the backdrop). ----------
    const float shadowSize = (std::max)(radius * 1.4f, 14.0f * uiScale);
    UIComponents::AddSoftGlowRect(
        drawList,
        bb.Min,
        bb.Max,
        IM_COL32(0, 0, 0, 110),
        radius,
        shadowSize,
        0.55f * opacityScale,
        22,
        ImDrawFlags_RoundCornersAll,
        true);

    // ---------- 2. Card body fill, honoring theme alpha. ----------
    ImVec4 base = BlendColor(surfaceAlt, elevated, 0.32f);
    base.w = ((surfaceAlt.w + elevated.w) * 0.5f) * opacityScale;
    drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(base), radius);

    // ---------- 3. Accent rail glow (only when requested). ----------
    const float accentRailWidth = (std::max)(2.0f * uiScale, 2.0f);
    const float accentRailInsetY = (std::min)((std::max)(radius * 0.62f, 2.0f * uiScale), (bb.Max.y - bb.Min.y) * 0.25f);
    const ImVec2 accentRailMin(bb.Min.x + uiScale, bb.Min.y + accentRailInsetY);
    const ImVec2 accentRailMax(bb.Min.x + uiScale + accentRailWidth, bb.Max.y - accentRailInsetY);
    if (withAccentBorder) {
        const float cardHeightRail = bb.Max.y - bb.Min.y;
        const float glowInsetY = (std::min)(radius * 0.12f, cardHeightRail * 0.08f);
        const ImVec2 glowMin(accentRailMin.x, accentRailMin.y + glowInsetY);
        const ImVec2 glowMax(accentRailMax.x, accentRailMax.y - glowInsetY);
        UIComponents::AddSoftGlowRect(
            drawList,
            glowMin,
            glowMax,
            ImGui::GetColorU32(WithAlpha(accent, 0.56f * opacityScale)),
            (std::max)(radius - uiScale, accentRailWidth * 0.5f),
            34.0f * uiScale,
            0.34f,
            28,
            ImDrawFlags_RoundCornersAll,
            true);
    }

    // ---------- 4. Top sheen as a vertical gradient. ----------
    const float cardHeight = bb.Max.y - bb.Min.y;
    const float sheenHeight = (std::min)((std::max)(cardHeight * 0.28f, 18.0f * uiScale), 96.0f * uiScale);
    const ImU32 sheenTop = ImGui::GetColorU32(WithAlpha(text, 0.07f * opacityScale));
    const ImU32 sheenBottom = ImGui::GetColorU32(WithAlpha(text, 0.0f));
    drawList->AddRectFilledMultiColor(
        bb.Min,
        ImVec2(bb.Max.x, bb.Min.y + sheenHeight),
        sheenTop,
        sheenTop,
        sheenBottom,
        sheenBottom);

    // ---------- 5. Outline wrap (matches SubTab bar): rounded outer stroke
    //              followed by an inner top hairline inset by `radius` so the
    //              corner curve is owned by the border rather than the sheen.
    //              This kills the bright corner hotspot without truncating the
    //              sheen itself.
    const ImVec4 outlineCol = WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.34f * opacityScale);
    drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(outlineCol), radius, 0, 1.05f);
    if (bb.Max.x - bb.Min.x > radius * 2.0f) {
        drawList->AddLine(
            ImVec2(bb.Min.x + radius, bb.Min.y + 1.0f),
            ImVec2(bb.Max.x - radius, bb.Min.y + 1.0f),
            ImGui::GetColorU32(WithAlpha(text, 0.12f * opacityScale)),
            1.0f);
    }

    if (withAccentBorder) {
        drawList->AddRectFilled(
            accentRailMin,
            accentRailMax,
            ImGui::GetColorU32(WithAlpha(accent, 0.46f * opacityScale)),
            accentRailWidth * 0.5f,
            ImDrawFlags_RoundCornersAll);
    }
}

static bool DrawCustomButton(const char* label, const ImVec2& size_arg, bool disabled, ButtonVariant variant) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    auto& theme = ThemeManager::Instance();
    const float height = theme.GetControlHeight(ThemeControlHeightRole::Button);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, height);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false;
    bool held = false;
    bool pressed = disabled ? false : ImGui::ButtonBehavior(bb, id, &hovered, &held);

    float* pProgress = ImGui::GetStateStorage()->GetFloatRef(id, 0.0f);
    float targetProgress = (hovered && !disabled) ? 1.0f : 0.0f;
    
    if (held && !disabled) targetProgress = 1.0f;

    float duration = theme.GetMotion(ThemeMotionRole::Fast).durationSeconds;
    float step = ImGui::GetIO().DeltaTime / duration;
    
    if (*pProgress < targetProgress) {
        *pProgress = (std::min)(*pProgress + step, 1.0f);
    } else if (*pProgress > targetProgress) {
        *pProgress = (std::max)(*pProgress - step, 0.0f);
    }

    float t = AnimationHelper::EaseInOut(*pProgress);

    ImVec4 baseBg, hoverBg, activeBg, textCol, borderCol;
    float borderThickness = 0.0f;

    if (variant == ButtonVariant::Primary) {
        baseBg = BlendColor(theme.GetColor(ThemeColorRole::Accent), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.08f);
        baseBg.w = 0.88f;
        hoverBg = BlendColor(theme.GetColor(ThemeColorRole::AccentHover), theme.GetColor(ThemeColorRole::TextPrimary), 0.03f);
        hoverBg.w = 0.95f;
        activeBg = theme.GetColor(ThemeColorRole::AccentActive);
        activeBg.w = 0.95f;
        textCol = ThemeManager::DeriveTextColor(baseBg, baseBg);
        borderCol = WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), 0.10f);
        borderThickness = 1.0f;
    } else if (variant == ButtonVariant::Secondary) {
        baseBg = WithAlpha(BlendColor(theme.GetColor(ThemeColorRole::SurfaceAlt), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.38f), 0.70f);
        hoverBg = WithAlpha(BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), theme.GetColor(ThemeColorRole::TextPrimary), 0.05f), 0.82f);
        activeBg = BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), theme.GetColor(ThemeColorRole::Active), 0.65f);
        textCol = theme.GetColor(ThemeColorRole::TextPrimary);
        borderCol = WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.40f);
        borderThickness = 1.15f;
    } else if (variant == ButtonVariant::Ghost) {
        baseBg = WithAlpha(theme.GetColor(ThemeColorRole::SurfaceElevated), 0.28f);
        hoverBg = WithAlpha(BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), theme.GetColor(ThemeColorRole::TextPrimary), 0.04f), 0.70f);
        activeBg = BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), theme.GetColor(ThemeColorRole::Active), 0.55f);
        textCol = theme.GetColor(ThemeColorRole::TextPrimary);
        borderCol = WithAlpha(theme.GetColor(ThemeColorRole::Border), 0.34f);
        borderThickness = 1.0f;
    } else if (variant == ButtonVariant::AccentOutline) {
        baseBg = WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.0f);
        hoverBg = WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.10f);
        activeBg = WithAlpha(theme.GetColor(ThemeColorRole::AccentActive), 0.18f);
        textCol = theme.GetColor(ThemeColorRole::Accent);
        borderCol = WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.78f);
        borderThickness = 1.25f;
    } else if (variant == ButtonVariant::Danger) {
        baseBg = theme.GetColor(ThemeColorRole::Danger);
        hoverBg = theme.GetColor(ThemeColorRole::DangerHover);
        activeBg = theme.GetColor(ThemeColorRole::Danger);
        textCol = ThemeManager::DeriveTextColor(baseBg, baseBg);
        borderCol = WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), 0.10f);
        borderThickness = 1.0f;
    }

    if (disabled) {
        // Keep the variant's silhouette; just thin its alpha and recolor text. A hard color
        // swap to Disabled looked like a different widget on glass surfaces.
        baseBg.w *= 0.45f;
        hoverBg = baseBg;
        activeBg = baseBg;
        textCol = theme.GetColor(ThemeColorRole::TextDisabled);
        borderCol = WithAlpha(theme.GetColor(ThemeColorRole::Border), 0.18f);
        borderThickness = 1.0f;
    }

    ImVec4 currentBg;
    if (held && !disabled) {
        currentBg = activeBg;
    } else {
        currentBg.x = AnimationHelper::Lerp(baseBg.x, hoverBg.x, t);
        currentBg.y = AnimationHelper::Lerp(baseBg.y, hoverBg.y, t);
        currentBg.z = AnimationHelper::Lerp(baseBg.z, hoverBg.z, t);
        currentBg.w = AnimationHelper::Lerp(baseBg.w, hoverBg.w, t);
    }

    const float radius = theme.GetRadius(ThemeRadiusRole::Frame);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Outer rim glow. Filled accent buttons (Primary/Danger) keep a low-intensity
    // resting glow so they read as lit even at rest; the muted variants only light up
    // on hover but no longer sit perfectly flat next to glowing toggles/inputs.
    if (!disabled) {
        const bool isFilledAccent = (variant == ButtonVariant::Primary || variant == ButtonVariant::Danger);
        const bool isOutlineAccent = (variant == ButtonVariant::AccentOutline);
        const bool isMuted = (variant == ButtonVariant::Secondary || variant == ButtonVariant::Ghost);

        const float restIntensity = isFilledAccent ? 0.22f : 0.0f;
        const float hoverIntensity = isMuted ? 0.32f : 0.62f;
        const float glowIntensity = restIntensity + (hoverIntensity - restIntensity) * t;

        if (glowIntensity > 0.0f) {
            ImVec4 glowSource;
            if (variant == ButtonVariant::Danger) {
                glowSource = theme.GetColor(ThemeColorRole::Danger);
            } else if (isMuted) {
                // Muted variants borrow the accent on hover so the lift feels intentional
                // rather than a generic white halo.
                glowSource = theme.GetColor(ThemeColorRole::Accent);
            } else {
                glowSource = theme.GetColor(ThemeColorRole::Accent);
            }
            const float glowAlpha = isFilledAccent ? 0.55f : (isOutlineAccent ? 0.50f : 0.38f);
            const float glowSize = (isFilledAccent ? 16.0f : 13.0f) * theme.GetAppliedScale();
            UIComponents::AddSoftGlowRect(
                drawList,
                bb.Min,
                bb.Max,
                ImGui::GetColorU32(WithAlpha(glowSource, glowAlpha)),
                radius,
                glowSize,
                glowIntensity,
                18,
                ImDrawFlags_RoundCornersAll,
                true);
        }
    }

    if (currentBg.w > 0.0f) {
        drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(currentBg), radius);
    }

    // Persistent top gloss + bottom inner shadow on filled variants. Gives them the
    // same subtle bevel the rest of the surface chrome already has, instead of reading
    // as a single flat slab.
    const bool hasBevel = !disabled && (variant == ButtonVariant::Primary ||
                                        variant == ButtonVariant::Secondary ||
                                        variant == ButtonVariant::Danger);
    if (hasBevel) {
        const float highlightAlpha = 0.10f + 0.12f * t;
        drawList->AddLine(
            ImVec2(bb.Min.x + radius, bb.Min.y + 1.0f),
            ImVec2(bb.Max.x - radius, bb.Min.y + 1.0f),
            ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), highlightAlpha)),
            1.0f);
        drawList->AddLine(
            ImVec2(bb.Min.x + radius, bb.Max.y - 1.0f),
            ImVec2(bb.Max.x - radius, bb.Max.y - 1.0f),
            IM_COL32(0, 0, 0, 55),
            1.0f);
    } else if (!disabled && (hovered || held) && variant == ButtonVariant::Ghost) {
        // Ghost has no fill at rest; only emit the top gloss on hover so it doesn't
        // float a stray line over empty pixels.
        drawList->AddLine(
            ImVec2(bb.Min.x + radius, bb.Min.y + 1.0f),
            ImVec2(bb.Max.x - radius, bb.Min.y + 1.0f),
            ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), 0.10f)),
            1.0f);
    }

    if (borderThickness > 0.0f && borderCol.w > 0.0f) {
        drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(borderCol), radius, 0, borderThickness);
    }

    const char* text_end = ImGui::FindRenderedTextEnd(label);
    float textX = bb.Min.x + (size.x - label_size.x) * 0.5f;
    float textY = bb.Min.y + (size.y - label_size.y) * 0.5f;
    const size_t visibleLen = static_cast<size_t>(text_end - label);
    if (visibleLen == 1 && size.y > height * 0.5f) {
        textY += (std::max)(theme.GetAppliedScale(), 1.0f) * 0.6f;
    }
    const ImVec2 textPos(textX, textY);
    drawList->AddText(textPos, ImGui::GetColorU32(textCol), label, text_end);

    return pressed;
}

bool UIComponents::PrimaryButton(const char* label, const ImVec2& size, bool disabled) {
    return DrawCustomButton(label, size, disabled, ButtonVariant::Primary);
}

bool UIComponents::SecondaryButton(const char* label, const ImVec2& size, bool disabled) {
    return DrawCustomButton(label, size, disabled, ButtonVariant::Secondary);
}

bool UIComponents::GhostButton(const char* label, const ImVec2& size, bool disabled) {
    return DrawCustomButton(label, size, disabled, ButtonVariant::Ghost);
}

bool UIComponents::AccentOutlineButton(const char* label, const ImVec2& size, bool disabled) {
    return DrawCustomButton(label, size, disabled, ButtonVariant::AccentOutline);
}

bool UIComponents::DangerButton(const char* label, const ImVec2& size, bool disabled) {
    return DrawCustomButton(label, size, disabled, ButtonVariant::Danger);
}

bool UIComponents::Toggle(const char* label, bool* v, bool rightAlign) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    auto& theme = ThemeManager::Instance();
    const float height = theme.GetControlHeight(ThemeControlHeightRole::Toggle);
    const float width = height * 1.8f;
    const float radius = height * 0.5f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 total_size = ImVec2(width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), height);
    
    if (rightAlign) {
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > total_size.x) pos.x += avail - total_size.x;
    }

    const ImRect bb(pos, ImVec2(pos.x + total_size.x, pos.y + total_size.y));
    const ImRect toggle_bb(pos, ImVec2(pos.x + width, pos.y + height));

    ImGui::ItemSize(total_size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) {
        *v = !*v;
        ImGui::MarkItemEdited(id);
    }

    float* pProgress = ImGui::GetStateStorage()->GetFloatRef(id, *v ? 1.0f : 0.0f);
    float targetProgress = *v ? 1.0f : 0.0f;
    
    float duration = theme.GetMotion(ThemeMotionRole::Normal).durationSeconds;
    float step = ImGui::GetIO().DeltaTime / duration;
    
    if (*pProgress < targetProgress) {
        *pProgress = (std::min)(*pProgress + step, targetProgress);
    } else if (*pProgress > targetProgress) {
        *pProgress = (std::max)(*pProgress - step, targetProgress);
    }

    float t = AnimationHelper::SmoothStep(0.0f, 1.0f, *pProgress);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float controlOpacity = std::clamp(1.0f - theme.GetAppliedTransparency() * 0.35f, 0.76f, 1.0f);
    ImVec4 bgColOff = BlendColor(theme.GetColor(ThemeColorRole::Disabled), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.20f);
    bgColOff.w = (std::max)(bgColOff.w, 0.58f * controlOpacity);
    ImVec4 bgColOn = theme.GetColor(ThemeColorRole::Accent);
    bgColOn.w = (std::max)(bgColOn.w, 0.86f * controlOpacity);

    ImVec4 currentBg;
    currentBg.x = AnimationHelper::Lerp(bgColOff.x, bgColOn.x, t);
    currentBg.y = AnimationHelper::Lerp(bgColOff.y, bgColOn.y, t);
    currentBg.z = AnimationHelper::Lerp(bgColOff.z, bgColOn.z, t);
    currentBg.w = AnimationHelper::Lerp(bgColOff.w, bgColOn.w, t);

    if (hovered) {
        currentBg.x = (std::min)(currentBg.x + 0.05f, 1.0f);
        currentBg.y = (std::min)(currentBg.y + 0.05f, 1.0f);
        currentBg.z = (std::min)(currentBg.z + 0.05f, 1.0f);
    }

    // Outer accent glow when ON (and a small lift on hover when OFF).
    if (t > 0.0f) {
        const ImVec4 accentColor = theme.GetColor(ThemeColorRole::Accent);
        UIComponents::AddSoftGlowRect(
            drawList,
            toggle_bb.Min,
            toggle_bb.Max,
            ImGui::GetColorU32(WithAlpha(accentColor, 0.6f)),
            radius,
            height * 0.7f,
            0.55f * t,
            18,
            ImDrawFlags_RoundCornersAll,
            true);
    }

    drawList->AddRectFilled(toggle_bb.Min, toggle_bb.Max, ImGui::GetColorU32(currentBg), radius);
    drawList->AddRect(
        toggle_bb.Min,
        toggle_bb.Max,
        ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.24f * controlOpacity)),
        radius,
        0,
        (std::max)(1.0f * theme.GetAppliedScale(), 1.0f));

    float knob_radius = radius - 2.0f;
    float min_x = toggle_bb.Min.x + radius;
    float max_x = toggle_bb.Max.x - radius;
    float knob_x = AnimationHelper::Lerp(min_x, max_x, t);

    ImVec4 knobCol = BlendColor(theme.GetColor(ThemeColorRole::Background), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.28f);
    knobCol.w = (std::max)(knobCol.w, 0.94f * controlOpacity);
    // Drop shadow under the knob to imply elevation off the rail.
    const float knobShadowOffset = (std::max)(1.0f * theme.GetAppliedScale(), 1.0f);
    drawList->AddCircleFilled(
        ImVec2(knob_x, toggle_bb.Min.y + radius + knobShadowOffset),
        knob_radius,
        IM_COL32(0, 0, 0, 70));
    drawList->AddCircleFilled(ImVec2(knob_x, toggle_bb.Min.y + radius), knob_radius, ImGui::GetColorU32(knobCol));

    if (label_size.x > 0.0f) {
        const float label_y = toggle_bb.Min.y + (height - label_size.y) * 0.5f;
        ImGui::RenderText(ImVec2(toggle_bb.Max.x + style.ItemInnerSpacing.x, label_y), label);
    }

    return pressed;
}

void UIComponents::BeginTabs(const char* id) {
    ImGui::PushID(id);
    ImGui::BeginGroup();
}

bool UIComponents::TabItem(const char* label, bool active) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    auto& theme = ThemeManager::Instance();
    const float height = theme.GetControlHeight(ThemeControlHeightRole::Tab);
    const float pad_x = theme.GetSpacing(ThemeSpacingRole::FramePaddingX) * 2.25f;
    
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(label_size.x + pad_x, height);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    float* pProgress = ImGui::GetStateStorage()->GetFloatRef(id, active ? 1.0f : 0.0f);
    float targetProgress = active ? 1.0f : (hovered ? 0.5f : 0.0f);

    float duration = theme.GetMotion(ThemeMotionRole::Slow).durationSeconds;
    float step = ImGui::GetIO().DeltaTime / duration;

    if (*pProgress < targetProgress) {
        *pProgress = (std::min)(*pProgress + step, targetProgress);
    } else if (*pProgress > targetProgress) {
        *pProgress = (std::max)(*pProgress - step, targetProgress);
    }

    float t = AnimationHelper::EaseInOut(*pProgress);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    ImVec4 bgBase = theme.GetColor(ThemeColorRole::SurfaceElevated);
    bgBase.w = 0.92f;
    ImVec4 bgHover = BlendColor(
        theme.GetColor(ThemeColorRole::SurfaceElevated),
        theme.GetColor(ThemeColorRole::Hover),
        0.75f);
    ImVec4 bgActive = theme.GetColor(ThemeColorRole::Accent);

    ImVec4 bgCol;
    if (active) {
        bgCol = bgActive;
    } else {
        bgCol = BlendColor(bgBase, bgHover, t);
    }

    const float tabRadius = theme.GetRadius(ThemeRadiusRole::Tab);
    if (bgCol.w > 0.0f) {
        drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bgCol), tabRadius);
    }
    
    // Hover-only outline. Active state is signaled by fill + glowy underline; a third
    // border was visual noise.
    if (hovered && !active) {
        drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.45f)), tabRadius, 0, 1.15f);
    }

    ImVec4 textBase = theme.GetColor(ThemeColorRole::TextSecondary);
    ImVec4 textHover = theme.GetColor(ThemeColorRole::TextPrimary);
    ImVec4 textActive = ThemeManager::DeriveTextColor(theme.GetColor(ThemeColorRole::Accent), bgActive);
    ImVec4 textCol = active
        ? textActive
        : BlendColor(textBase, textHover, hovered ? (std::min)(t * 2.0f, 1.0f) : 0.0f);

    const char* text_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 textPos(
        bb.Min.x + (size.x - label_size.x) * 0.5f,
        bb.Min.y + (size.y - label_size.y) * 0.5f);
    drawList->AddText(textPos, ImGui::GetColorU32(textCol), label, text_end);

    if (t > 0.0f) {
        const float underlineHeight = (std::max)(2.0f * theme.GetAppliedScale(), 2.0f);
        const float line_w = size.x * t;
        const float line_x = bb.Min.x + (size.x - line_w) * 0.5f;
        const ImVec2 underlineMin(line_x, bb.Max.y - underlineHeight);
        const ImVec2 underlineMax(line_x + line_w, bb.Max.y);
        const ImU32 underlineCol = ImGui::GetColorU32(textActive);

        UIComponents::AddSoftGlowRect(
            drawList,
            underlineMin,
            underlineMax,
            underlineCol,
            underlineHeight * 0.5f,
            underlineHeight * 3.5f,
            0.6f * t,
            14,
            ImDrawFlags_RoundCornersAll,
            true);
        drawList->AddRectFilled(underlineMin, underlineMax, underlineCol, underlineHeight * 0.5f);
    }

    ImGui::SameLine(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingX));
    return pressed;
}

void UIComponents::EndTabs() {
    ImGui::NewLine();
    ImGui::EndGroup();
    auto& theme = ThemeManager::Instance();
    const ImVec4 borderCol = theme.GetBorder(ThemeBorderRole::Tab).color;
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        p, ImVec2(p.x + w, p.y), ImGui::GetColorU32(borderCol)
    );
    ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY)));
    ImGui::PopID();
}

// ── Sub-tab bar (glass navigation strip) ───────────────────────────
static float GetSubTabHeight() {
    auto& theme = ThemeManager::Instance();
    return (std::max)(theme.GetControlHeight(ThemeControlHeightRole::Tab) + 2.0f * theme.GetAppliedScale(), 12.0f);
}

static float GetSubTabBarPaddingX() {
    auto& theme = ThemeManager::Instance();
    return theme.GetSpacing(ThemeSpacingRole::FramePaddingX) * 0.70f;
}

static float GetSubTabBarPaddingY() {
    auto& theme = ThemeManager::Instance();
    return (std::max)(theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.72f, 6.0f * theme.GetAppliedScale());
}

void UIComponents::BeginSubTabs(const char* id) {
    ImGui::PushID(id);
    ImGui::BeginGroup();

    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float padX = GetSubTabBarPaddingX();
    const float padY = GetSubTabBarPaddingY();
    const float barHeight = GetSubTabHeight() + padY * 2.0f;
    const float barWidth = ImGui::GetContentRegionAvail().x;
    const float radius = theme.GetRadius(ThemeRadiusRole::Tab) + padY * 0.45f;
    const ImVec2 barMin = ImGui::GetCursorScreenPos();
    const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);

    if (barWidth > 0.0f && !ImGui::GetCurrentWindow()->SkipItems) {
        const ImVec4 base = theme.GetColor(ThemeColorRole::Surface);
        const ImVec4 elevated = theme.GetColor(ThemeColorRole::SurfaceElevated);
        const ImVec4 accent = theme.GetColor(ThemeColorRole::Accent);
        const ImVec4 text = theme.GetColor(ThemeColorRole::TextPrimary);

        // Single vertical gradient: subtle sheen on top fading to clear.
        ImVec4 barTop = BlendColor(elevated, text, 0.06f);
        barTop.w = 0.55f;
        ImVec4 barBottom = BlendColor(base, theme.GetColor(ThemeColorRole::Background), 0.10f);
        barBottom.w = 0.42f;
        drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(barBottom), radius);
        drawList->AddRectFilledMultiColor(
            barMin,
            barMax,
            ImGui::GetColorU32(barTop),
            ImGui::GetColorU32(barTop),
            ImGui::GetColorU32(WithAlpha(barTop, 0.0f)),
            ImGui::GetColorU32(WithAlpha(barTop, 0.0f)));

        drawList->AddRect(
            barMin,
            barMax,
            ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.34f)),
            radius,
            0,
            1.05f);

        drawList->AddLine(
            ImVec2(barMin.x + radius, barMin.y + 1.0f),
            ImVec2(barMax.x - radius, barMin.y + 1.0f),
            ImGui::GetColorU32(WithAlpha(text, 0.12f)),
            1.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(barMin.x + padX, barMin.y + padY));
}

bool UIComponents::SubTabItem(const char* label, bool active, bool disabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    auto& theme = ThemeManager::Instance();
    const float height = GetSubTabHeight();
    const float padX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX) * 1.25f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(label_size.x + padX * 2.0f, height);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false;
    bool held = false;
    bool pressed = disabled ? false : ImGui::ButtonBehavior(bb, id, &hovered, &held);

    float* pProgress = ImGui::GetStateStorage()->GetFloatRef(id, active ? 1.0f : 0.0f);
    float targetProgress = disabled ? 0.0f : (active ? 1.0f : (hovered ? 0.65f : 0.0f));

    float duration = theme.GetMotion(ThemeMotionRole::Slow).durationSeconds;
    float step = ImGui::GetIO().DeltaTime / duration;
    if (*pProgress < targetProgress) {
        *pProgress = (std::min)(*pProgress + step, targetProgress);
    } else if (*pProgress > targetProgress) {
        *pProgress = (std::max)(*pProgress - step, targetProgress);
    }

    float t = AnimationHelper::EaseInOut(*pProgress);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = (std::max)(theme.GetRadius(ThemeRadiusRole::Tab) - 2.0f, 2.0f);
    const ImVec4 accent = theme.GetColor(ThemeColorRole::Accent);

    if (!disabled && (hovered || active || t > 0.0f)) {
        ImVec4 bg = active
            ? BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), accent, 0.22f)
            : BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), theme.GetColor(ThemeColorRole::Hover), 0.55f);
        bg.w = active ? 0.62f : 0.34f * t;
        drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bg), radius);
    }

    if (active && !disabled) {
        const float underlineHeight = (std::max)(2.0f * theme.GetAppliedScale(), 2.0f);
        const float underlineInset = (std::max)(padX * 0.62f, 8.0f * theme.GetAppliedScale());
        const ImVec2 underlineMin(bb.Min.x + underlineInset, bb.Max.y - underlineHeight - 2.0f);
        const ImVec2 underlineMax(bb.Max.x - underlineInset, bb.Max.y - 2.0f);
        const ImU32 accentU32 = ImGui::GetColorU32(accent);

        UIComponents::AddSoftGlowRect(
            drawList,
            underlineMin,
            underlineMax,
            accentU32,
            underlineHeight * 0.5f,
            underlineHeight * 4.0f,
            0.55f,
            14,
            ImDrawFlags_RoundCornersAll,
            true);
        drawList->AddRectFilled(underlineMin, underlineMax, accentU32, underlineHeight * 0.5f);
    } else if (hovered && !disabled) {
        drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.32f)), radius, 0, 1.0f);
    }

    ImVec4 textCol = disabled
        ? theme.GetColor(ThemeColorRole::TextDisabled)
        : (active
            ? theme.GetColor(ThemeColorRole::TextPrimary)
            : BlendColor(theme.GetColor(ThemeColorRole::TextSecondary), theme.GetColor(ThemeColorRole::TextPrimary), hovered ? (std::min)(t * 1.5f, 1.0f) : 0.0f));

    const char* text_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 textPos(
        bb.Min.x + (size.x - label_size.x) * 0.5f,
        bb.Min.y + (size.y - label_size.y) * 0.5f);
    drawList->AddText(textPos, ImGui::GetColorU32(textCol), label, text_end);

    ImGui::SameLine(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingX) * 0.45f);
    return pressed && !disabled;
}

void UIComponents::EndSubTabs() {
    ImGui::NewLine();
    ImGui::Dummy(ImVec2(0.0f, GetSubTabBarPaddingY()));
    ImGui::EndGroup();

    auto& theme = ThemeManager::Instance();
    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.75f));
    ImGui::PopID();
}

bool UIComponents::SelectableItem(const char* id_str, const char* label, bool selected, const char* icon) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(id_str);
    
    auto& theme = ThemeManager::Instance();
    const float pad_x = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float pad_y = theme.GetSpacing(ThemeSpacingRole::FramePaddingY);
    const float minRowHeight = 58.0f * theme.GetAppliedScale();
    
    ImFont* currentFont = ImGui::GetFont();
    const float navTextSize = ImGui::GetFontSize() * 1.12f;
    const float iconTextSize = navTextSize * 1.3f;
    ImVec2 label_size = currentFont->CalcTextSizeA(navTextSize, 10000.0f, 0.0f, label);
    if (icon) {
        ImVec2 icon_size = currentFont->CalcTextSizeA(iconTextSize, 10000.0f, 0.0f, icon);
        label_size.x += icon_size.x + style.ItemInnerSpacing.x;
        label_size.y = (std::max)(label_size.y, icon_size.y);
    }

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, (std::max)(label_size.y + pad_y * 2.0f, minRowHeight));

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    float* pProgress = ImGui::GetStateStorage()->GetFloatRef(id, selected ? 1.0f : 0.0f);
    float targetProgress = selected ? 1.0f : (hovered ? 1.0f : 0.0f);
    
    float duration = theme.GetMotion(ThemeMotionRole::Fast).durationSeconds;
    float step = ImGui::GetIO().DeltaTime / duration;
    
    if (*pProgress < targetProgress) {
        *pProgress = (std::min)(*pProgress + step, 1.0f);
    } else if (*pProgress > targetProgress) {
        *pProgress = (std::max)(*pProgress - step, 0.0f);
    }

    float t = AnimationHelper::EaseInOut(*pProgress);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = theme.GetRadius(ThemeRadiusRole::Child);

    if (t > 0.0f) {
        if (selected) {
            // Lighter selected fill so the row reads as a row in glass nav, not a pasted block.
            ImVec4 bgCol = BlendColor(
                theme.GetColor(ThemeColorRole::SurfaceElevated),
                theme.GetColor(ThemeColorRole::Accent),
                0.10f);
            bgCol.w = 0.32f;
            drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bgCol), radius);

            // Subtle outline keeps the silhouette readable on busy backdrops.
            ImVec4 borderCol = WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.24f);
            drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(borderCol), radius, 0, 1.0f);

            // Bottom underline: a wide centered bar that signals the active row
            // without competing with the row's left edge.
            const float scale = theme.GetAppliedScale();
            const float underlineHeight = (std::max)(3.0f * scale, 3.0f);
            const float underlineInsetX = (std::max)(pad_x * 0.55f, 18.0f * scale);
            const float underlineBottomGap = (std::max)(6.0f * scale, 6.0f);
            const float underlineLeft = bb.Min.x + underlineInsetX;
            const float underlineRight = bb.Max.x - underlineInsetX;
            if (underlineRight > underlineLeft + 8.0f * scale) {
                const ImVec2 underlineMin(underlineLeft, bb.Max.y - underlineBottomGap - underlineHeight);
                const ImVec2 underlineMax(underlineRight, bb.Max.y - underlineBottomGap);
                const ImU32 accentU32 = ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Accent));

                UIComponents::AddSoftGlowRect(
                    drawList,
                    underlineMin,
                    underlineMax,
                    accentU32,
                    underlineHeight * 0.5f,
                    underlineHeight * 5.0f,
                    0.55f,
                    18,
                    ImDrawFlags_RoundCornersAll,
                    true);
                drawList->AddRectFilled(underlineMin, underlineMax, accentU32, underlineHeight * 0.5f);
            }
        } else {
            ImVec4 hoverBg = BlendColor(
                WithAlpha(theme.GetColor(ThemeColorRole::Active), 0.0f),
                theme.GetColor(ThemeColorRole::Active), t);
            drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(hoverBg), radius);
        }
    }

    ImVec4 textCol = selected 
        ? theme.GetColor(ThemeColorRole::TextPrimary) 
        : (hovered
            ? BlendColor(theme.GetColor(ThemeColorRole::TextSecondary), theme.GetColor(ThemeColorRole::TextPrimary), 0.58f)
            : theme.GetColor(ThemeColorRole::TextSecondary));

    float contentX = bb.Min.x + pad_x;
    const char* text_end = ImGui::FindRenderedTextEnd(label);
    ImVec2 textLabelSize = currentFont->CalcTextSizeA(navTextSize, 10000.0f, 0.0f, label, text_end);
    ImVec2 iconSize = icon
        ? currentFont->CalcTextSizeA(iconTextSize, 10000.0f, 0.0f, icon)
        : ImVec2(0.0f, 0.0f);

    ImFontBaked* navBaked = currentFont->GetFontBaked(navTextSize);
    const float groupCenterY = bb.Min.y + bb.GetHeight() * 0.5f;
    const float navBaseline = groupCenterY + (navBaked->Ascent + navBaked->Descent) * 0.5f;
    const float labelX = contentX + (icon ? iconSize.x + style.ItemInnerSpacing.x : 0.0f);
    ImVec2 labelTextPos = ImVec2(labelX, navBaseline - navBaked->Ascent);

    if (icon) {
        const float labelCenterY = labelTextPos.y + textLabelSize.y * 0.5f;
        const float iconOpticalOffsetY = 2.0f * theme.GetAppliedScale();
        ImVec2 iconPos = ImVec2(
            contentX,
            labelCenterY - iconSize.y * 0.5f + iconOpticalOffsetY);
        drawList->AddText(currentFont, iconTextSize, iconPos, ImGui::GetColorU32(textCol), icon);
    }

    drawList->AddText(currentFont, navTextSize, labelTextPos, ImGui::GetColorU32(textCol), label, text_end);

    return pressed;
}

void UIComponents::BeginCard(const char* id, float width, bool withAccentBorder) {
    auto& theme = ThemeManager::Instance();
    const float radius = theme.GetRadius(ThemeRadiusRole::Child);
    const float paddingX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float paddingY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY);
    const ImVec4 bgCol = theme.GetColor(ThemeColorRole::SurfaceAlt);
    const ThemeBorderToken border = theme.GetBorder(ThemeBorderRole::Child);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgCol);
    ImGui::PushStyleColor(ImGuiCol_Border, border.color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(paddingX * 1.6f, paddingY * 1.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, radius);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, border.thickness);

    ImGui::BeginChild(id, ImVec2(width, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    const ImVec2 pMin = ImGui::GetWindowPos();
    const ImVec2 pMax = ImVec2(pMin.x + ImGui::GetWindowSize().x, pMin.y + ImGui::GetWindowSize().y);
    DrawPanelDecor(ImRect(pMin, pMax), radius, withAccentBorder);
}

void UIComponents::BeginCardFillHeight(const char* id, float width, bool withAccentBorder) {
    auto& theme = ThemeManager::Instance();
    const float radius = theme.GetRadius(ThemeRadiusRole::Child);
    const float paddingX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float paddingY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY);
    const ImVec4 bgCol = theme.GetColor(ThemeColorRole::SurfaceAlt);
    const ThemeBorderToken border = theme.GetBorder(ThemeBorderRole::Child);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgCol);
    ImGui::PushStyleColor(ImGuiCol_Border, border.color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(paddingX * 2.0f, paddingY * 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, radius);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, border.thickness);

    ImGui::BeginChild(id, ImVec2(width, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    
    const ImVec2 pMin = ImGui::GetWindowPos();
    const ImVec2 pMax = ImVec2(pMin.x + ImGui::GetWindowSize().x, pMin.y + ImGui::GetWindowSize().y);
    DrawPanelDecor(ImRect(pMin, pMax), radius, withAccentBorder);
}

void UIComponents::EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void UIComponents::CardHeader(const char* title, const char* subtitle) {
    auto& theme = ThemeManager::Instance();
    const ImVec4 titleCol = theme.GetColor(ThemeColorRole::TextPrimary);
    
    ImGui::PushStyleColor(ImGuiCol_Text, titleCol);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (subtitle) {
        const ImVec4 subCol = theme.GetColor(ThemeColorRole::TextSecondary);
        ImGui::PushStyleColor(ImGuiCol_Text, subCol);
        ImGui::TextWrapped("%s", subtitle);
        ImGui::PopStyleColor();
    }
    
    ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 1.15f));
}

void UIComponents::CardFooter() {
    auto& theme = ThemeManager::Instance();
    const float spacingY = theme.GetSpacing(ThemeSpacingRole::ItemSpacingY);
    ImGui::Dummy(ImVec2(0, spacingY * 0.65f));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const ImU32 hairline = ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Border), 0.18f));
    drawList->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y), hairline, 1.0f);

    ImGui::Dummy(ImVec2(0, spacingY * 0.65f));
}

void UIComponents::SectionHeader(const char* title, const char* subtitle, const char* icon) {
    auto& theme = ThemeManager::Instance();

    ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.4f));
    
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    if (icon) {
        ImGui::Text("%s %s", icon, title);
    } else {
        ImGui::TextUnformatted(title);
    }
    ImGui::PopStyleColor();
    
    if (subtitle) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextWrapped("%s", subtitle);
        ImGui::PopStyleColor();
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 lineStart = ImGui::GetCursorScreenPos();
    const float lineWidth = ImGui::GetContentRegionAvail().x;
    const ImVec4 accentColor = theme.GetColor(ThemeColorRole::Accent);
    const ImVec4 accentBright = WithAlpha(accentColor, 0.55f);
    const ImVec4 accentFade = WithAlpha(accentColor, 0.0f);

    // Underglow: a soft horizontal halo behind the bright hairline.
    UIComponents::AddSoftGlowLine(
        drawList,
        ImVec2(lineStart.x, lineStart.y + 0.5f),
        ImVec2(lineStart.x + lineWidth * 0.6f, lineStart.y + 0.5f),
        ImGui::GetColorU32(accentBright),
        1.0f,
        6.0f,
        0.40f,
        12,
        true);

    // Bright crisp 1px hairline that fades horizontally into the layout.
    AddHorizontalFadeRect(
        drawList,
        lineStart,
        ImVec2(lineStart.x + lineWidth, lineStart.y + 1.0f),
        accentBright,
        accentFade);
    ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.75f));
}

void UIComponents::Separator(const char* text) {
    auto& theme = ThemeManager::Instance();
    const ImVec4 borderCol = theme.GetBorder(ThemeBorderRole::Window).color;
    
    ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY)));
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    if (text) {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        float lineW = (w - textSize.x - theme.GetSpacing(ThemeSpacingRole::ItemSpacingX) * 2.0f) * 0.5f;
        
        drawList->AddLine(
            ImVec2(p.x, p.y + textSize.y * 0.5f), 
            ImVec2(p.x + lineW, p.y + textSize.y * 0.5f), 
            ImGui::GetColorU32(borderCol)
        );
        
        ImGui::SetCursorScreenPos(ImVec2(p.x + lineW + theme.GetSpacing(ThemeSpacingRole::ItemSpacingX), p.y));
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        
        drawList->AddLine(
            ImVec2(p.x + lineW + textSize.x + theme.GetSpacing(ThemeSpacingRole::ItemSpacingX) * 2.0f, p.y + textSize.y * 0.5f), 
            ImVec2(p.x + w, p.y + textSize.y * 0.5f), 
            ImGui::GetColorU32(borderCol)
        );
    } else {
        drawList->AddLine(
            ImVec2(p.x, p.y), 
            ImVec2(p.x + w, p.y), 
            ImGui::GetColorU32(borderCol)
        );
        ImGui::Dummy(ImVec2(0, 1.0f));
    }
    
    ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY)));
}

void UIComponents::InfoRow(const char* label, const char* value, const ImVec4* valueColor) {
    auto& theme = ThemeManager::Instance();
    
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    
    ImVec2 valSize = ImGui::CalcTextSize(value);
    float availX = ImGui::GetContentRegionAvail().x;
    if (availX > valSize.x) {
        ImGui::SameLine(ImGui::GetCursorPosX() + availX - valSize.x);
    }
    
    if (valueColor) {
        ImGui::PushStyleColor(ImGuiCol_Text, *valueColor);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    }
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

void UIComponents::MetricCard(
    const char* id,
    const char* title,
    const std::string& value,
    const std::string& note,
    const ImVec4* valueColor,
    const ImVec4* noteColor,
    float width,
    float valueScale,
    bool withAccentBorder) {
    auto& theme = ThemeManager::Instance();

    BeginCard(id, width, withAccentBorder);
    CardHeader(title);

    if (valueColor != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, *valueColor);
    }

    ImGui::SetWindowFontScale(valueScale);
    ImGui::TextUnformatted(value.c_str());
    ImGui::SetWindowFontScale(1.0f);

    if (valueColor != nullptr) {
        ImGui::PopStyleColor();
    }

    if (!note.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, noteColor != nullptr ? *noteColor : theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextWrapped("%s", note.c_str());
        ImGui::PopStyleColor();
    }

    EndCard();
}

float UIComponents::ThumbnailSize() {
    return 72.0f;
}

float UIComponents::ThumbnailCardWidth() {
    return 124.0f;
}

float UIComponents::GetThumbnailLayoutWidth(float blockWidth) {
    const float effectiveBlockWidth = blockWidth > 0.0f ? blockWidth : ThumbnailCardWidth();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float reservedWidth = style.ScrollbarSize + style.ItemSpacing.x;
    return (std::max)(effectiveBlockWidth, ImGui::GetContentRegionAvail().x - reservedWidth);
}

void UIComponents::CenterNextItem(float width) {
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    if (availableWidth > width) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((availableWidth - width) * 0.5f));
    }
}

bool UIComponents::ThumbnailButton(const char* id, const ImVec2& size, bool selected) {
    auto& theme = ThemeManager::Instance();
    const bool clicked = ImGui::InvisibleButton(id, size);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(theme.GetColor(ThemeColorRole::SurfaceAlt)), 6.0f);
    drawList->AddRect(min, max, ImGui::GetColorU32(selected ? theme.GetColor(ThemeColorRole::Accent) : theme.GetColor(ThemeColorRole::Border)), 6.0f);

    return clicked;
}

void UIComponents::DrawFittedTexturePreview(const TextureInfo& textureInfo, float innerPadding) {
    if (textureInfo.texture == nullptr || textureInfo.width <= 0 || textureInfo.height <= 0) {
        return;
    }

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float frameWidth = (std::max)(1.0f, (itemMax.x - itemMin.x) - (innerPadding * 2.0f));
    const float frameHeight = (std::max)(1.0f, (itemMax.y - itemMin.y) - (innerPadding * 2.0f));
    const float widthScale = frameWidth / static_cast<float>(textureInfo.width);
    const float heightScale = frameHeight / static_cast<float>(textureInfo.height);
    const float scale = (std::min)(widthScale, heightScale);
    const ImVec2 imageSize(
        static_cast<float>(textureInfo.width) * scale,
        static_cast<float>(textureInfo.height) * scale);
    const ImVec2 imageMin(
        itemMin.x + ((itemMax.x - itemMin.x - imageSize.x) * 0.5f),
        itemMin.y + ((itemMax.y - itemMin.y - imageSize.y) * 0.5f));
    const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);

    ImGui::GetWindowDrawList()->AddImage(
        reinterpret_cast<ImTextureID>(textureInfo.texture),
        imageMin,
        imageMax,
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f));
}

void UIComponents::DrawThumbnailTextOverlay(const char* text, ImU32 color) {
    const char* safeText = text != nullptr ? text : "";
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const ImVec2 textSize = ImGui::CalcTextSize(safeText);
    const ImVec2 textPos(
        itemMin.x + ((itemMax.x - itemMin.x - textSize.x) * 0.5f),
        itemMin.y + ((itemMax.y - itemMin.y - textSize.y) * 0.5f));
    ImGui::GetWindowDrawList()->AddText(textPos, color, safeText);
}

void UIComponents::DrawThumbnailCaption(const char* label, bool selected) {
    auto& theme = ThemeManager::Instance();
    const char* safeLabel = label != nullptr ? label : "";
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const ImVec2 textSize = ImGui::CalcTextSize(safeLabel, nullptr, true, availableWidth);
    if (textSize.x < availableWidth) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((availableWidth - textSize.x) * 0.5f));
    }

    ImGui::PushStyleColor(ImGuiCol_Text,
        selected ? theme.GetColor(ThemeColorRole::TextPrimary) : theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + availableWidth);
    ImGui::TextWrapped("%s", safeLabel);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

static void ApplyInputStyles() {
    auto& theme = ThemeManager::Instance();
    const float radius = theme.GetRadius(ThemeRadiusRole::Frame);
    const float paddingX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float paddingY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY);
    const ThemeBorderToken border = theme.GetBorder(ThemeBorderRole::Frame);
    ImVec4 frameBg = BlendColor(theme.GetColor(ThemeColorRole::SurfaceAlt), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.42f);
    frameBg.w = 0.30f;
    ImVec4 frameBgHovered = BlendColor(theme.GetColor(ThemeColorRole::SurfaceAlt), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.62f);
    frameBgHovered.w = 0.42f;
    ImVec4 frameBgActive = BlendColor(theme.GetColor(ThemeColorRole::SurfaceAlt), theme.GetColor(ThemeColorRole::SurfaceElevated), 0.78f);
    frameBgActive.w = 0.55f;
    ImVec4 inputBorder = theme.GetColor(ThemeColorRole::Border);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, frameBgActive);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    ImGui::PushStyleColor(ImGuiCol_Border, inputBorder);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(paddingX, paddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, radius);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, border.thickness);
}

static float ResolveControlWidth(float width) {
    if (width > 0.0f) {
        return width;
    }

    auto& theme = ThemeManager::Instance();
    const float edgeInset = (std::max)(2.0f * theme.GetAppliedScale(), 2.0f);
    return (std::max)(ImGui::GetContentRegionAvail().x - edgeInset, 1.0f);
}

struct ParsedImGuiLabel {
    std::string visiblePortion;
    bool hasVisibleLabel;
    const char* fullLabel;
};

static ParsedImGuiLabel ParseImGuiLabel(const char* label) {
    const char* fullLabel = label ? label : "";
    const char* hiddenMarker = std::strstr(fullLabel, "##");

    if (hiddenMarker == nullptr) {
        return { fullLabel, fullLabel[0] != '\0', fullLabel };
    }

    if (hiddenMarker == fullLabel) {
        return { "", false, fullLabel };
    }

    return { std::string(fullLabel, static_cast<size_t>(hiddenMarker - fullLabel)), true, fullLabel };
}

static void PopInputStyles() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
}

// Inset shadow at the top inside edge of the last submitted item. Sells the "see-through"
// frosted-input look by hinting at depth without darkening the typed text.
static void DrawInputInnerShadow() {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 pMin = ImGui::GetItemRectMin();
    const ImVec2 pMax = ImGui::GetItemRectMax();
    if (pMax.x <= pMin.x || pMax.y <= pMin.y) {
        return;
    }
    const float shadowHeight = (std::min)((pMax.y - pMin.y) * 0.32f, 6.0f * theme.GetAppliedScale());
    if (shadowHeight <= 0.0f) {
        return;
    }
    const ImU32 top = IM_COL32(0, 0, 0, 38);
    const ImU32 bottom = IM_COL32(0, 0, 0, 0);
    drawList->AddRectFilledMultiColor(
        ImVec2(pMin.x + 1.0f, pMin.y + 1.0f),
        ImVec2(pMax.x - 1.0f, pMin.y + 1.0f + shadowHeight),
        top,
        top,
        bottom,
        bottom);
}

static void DrawFocusAccent(bool isFocused) {
    if (!isFocused) {
        return;
    }
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 pMin = ImGui::GetItemRectMin();
    const ImVec2 pMax = ImGui::GetItemRectMax();
    const float radius = theme.GetRadius(ThemeRadiusRole::Frame);
    const float borderThickness = theme.GetBorder(ThemeBorderRole::Frame).thickness;
    const ImU32 accentU32 = ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.85f));

    UIComponents::AddSoftGlowRect(
        drawList,
        pMin,
        pMax,
        accentU32,
        radius,
        10.0f * theme.GetAppliedScale(),
        0.55f,
        16,
        ImDrawFlags_RoundCornersAll,
        true);
    drawList->AddRect(pMin, pMax, accentU32, radius, 0, borderThickness + 0.5f);
}

static void DrawComboChevron(bool open, bool hovered, bool focused) {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const float itemHeight = itemMax.y - itemMin.y;
    const float paddingX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float inset = 1.0f;
    const float segmentWidth = (std::max)(itemHeight * 0.92f, 28.0f);
    const float frameRadius = theme.GetRadius(ThemeRadiusRole::Frame);

    const ImVec2 segmentMin(itemMax.x - segmentWidth - inset, itemMin.y + inset);
    const ImVec2 segmentMax(itemMax.x - inset, itemMax.y - inset);

    ImVec4 segmentFill = BlendColor(theme.GetColor(ThemeColorRole::SurfaceElevated), theme.GetColor(ThemeColorRole::SurfaceAlt), hovered ? 0.50f : 0.22f);
    if (focused || open) {
        segmentFill = BlendColor(segmentFill, theme.GetColor(ThemeColorRole::Accent), 0.10f);
    }
    segmentFill.w = hovered || focused || open ? 0.72f : 0.48f;

    ImVec4 dividerCol = BlendColor(theme.GetColor(ThemeColorRole::BorderStrong), theme.GetColor(ThemeColorRole::Accent), focused || open ? 0.40f : 0.15f);
    dividerCol.w = focused || open ? 0.24f : 0.12f;

    drawList->AddRectFilled(segmentMin, segmentMax, ImGui::GetColorU32(segmentFill), (std::max)(frameRadius - inset, 0.0f), ImDrawFlags_RoundCornersRight);
    drawList->AddLine(
        ImVec2(segmentMin.x, segmentMin.y + 3.0f),
        ImVec2(segmentMin.x, segmentMax.y - 3.0f),
        ImGui::GetColorU32(dividerCol),
        1.0f);

    const ImVec4 chevronColor = focused || open
        ? theme.GetColor(ThemeColorRole::Accent)
        : (hovered ? theme.GetColor(ThemeColorRole::TextPrimary) : theme.GetColor(ThemeColorRole::TextSecondary));
    const ImU32 chevronU32 = ImGui::GetColorU32(chevronColor);

    const ImVec2 center((segmentMin.x + segmentMax.x) * 0.5f, (segmentMin.y + segmentMax.y) * 0.5f + (open ? -1.0f : 0.5f));
    const float halfWidth = (std::max)(4.5f * theme.GetAppliedScale(), 4.5f);
    const float halfHeight = (std::max)(3.0f * theme.GetAppliedScale(), 3.0f);

    // Filled triangle reads cleaner than two AA-aliased lines and scales without quirks.
    if (open) {
        drawList->AddTriangleFilled(
            ImVec2(center.x - halfWidth, center.y + halfHeight),
            ImVec2(center.x + halfWidth, center.y + halfHeight),
            ImVec2(center.x, center.y - halfHeight),
            chevronU32);
    } else {
        drawList->AddTriangleFilled(
            ImVec2(center.x - halfWidth, center.y - halfHeight),
            ImVec2(center.x + halfWidth, center.y - halfHeight),
            ImVec2(center.x, center.y + halfHeight),
            chevronU32);
    }
}

bool UIComponents::InputText(const char* label, std::string* str, const char* placeholder, float width) {
    const ParsedImGuiLabel parsedLabel = ParseImGuiLabel(label);
    auto& theme = ThemeManager::Instance();

    if (parsedLabel.hasVisibleLabel) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextUnformatted(parsedLabel.visiblePortion.c_str());
        ImGui::PopStyleColor();
    }

    ApplyInputStyles();

    ImGui::SetNextItemWidth(ResolveControlWidth(width));
    ImGui::PushID(parsedLabel.fullLabel);

    bool changed = false;
    if (placeholder) {
        changed = ImGui::InputTextWithHint("##input", placeholder, str);
    } else {
        changed = ImGui::InputText("##input", str);
    }

    ImGui::PopID();

    DrawInputInnerShadow();
    DrawFocusAccent(ImGui::IsItemFocused() || ImGui::IsItemActive());

    PopInputStyles();
    return changed;
}

bool UIComponents::InputInt(const char* label, int* v, const char* placeholder, float width) {
    ApplyInputStyles();

    const ParsedImGuiLabel parsedLabel = ParseImGuiLabel(label);
    auto& theme = ThemeManager::Instance();

    if (parsedLabel.hasVisibleLabel) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextUnformatted(parsedLabel.visiblePortion.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetNextItemWidth(ResolveControlWidth(width));

    ImGui::PushID(parsedLabel.fullLabel);
    bool changed = ImGui::InputInt("##input", v);
    ImGui::PopID();

    DrawInputInnerShadow();
    DrawFocusAccent(ImGui::IsItemFocused() || ImGui::IsItemActive());

    PopInputStyles();
    return changed;
}

bool UIComponents::InputFloat(const char* label, float* v, const char* placeholder, float width) {
    ApplyInputStyles();

    const ParsedImGuiLabel parsedLabel = ParseImGuiLabel(label);
    auto& theme = ThemeManager::Instance();

    if (parsedLabel.hasVisibleLabel) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextUnformatted(parsedLabel.visiblePortion.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetNextItemWidth(ResolveControlWidth(width));

    ImGui::PushID(parsedLabel.fullLabel);
    bool changed = ImGui::InputFloat("##input", v, 0.0f, 0.0f, "%.3f");
    ImGui::PopID();

    DrawInputInnerShadow();
    DrawFocusAccent(ImGui::IsItemFocused() || ImGui::IsItemActive());

    PopInputStyles();
    return changed;
}

bool UIComponents::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, float width) {
    const ParsedImGuiLabel parsedLabel = ParseImGuiLabel(label);
    auto& theme = ThemeManager::Instance();

    if (parsedLabel.hasVisibleLabel) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextUnformatted(parsedLabel.visiblePortion.c_str());
        ImGui::PopStyleColor();
    }

    ApplyInputStyles();

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, theme.GetColor(ThemeColorRole::Accent));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, theme.GetColor(ThemeColorRole::AccentHover));

    ImGui::SetNextItemWidth(ResolveControlWidth(width));

    ImGui::PushID(parsedLabel.fullLabel);
    bool changed = ImGui::SliderFloat("##slider", v, v_min, v_max, format);
    ImGui::PopID();

    ImGui::PopStyleColor(2);

    DrawInputInnerShadow();
    DrawFocusAccent(ImGui::IsItemFocused() || ImGui::IsItemActive());

    PopInputStyles();
    return changed;
}

bool UIComponents::Combo(const char* label, int* current_item, const char* const items[], int items_count, float width) {
    auto& theme = ThemeManager::Instance();

    ParsedImGuiLabel parsedLabel = ParseImGuiLabel(label);
    if (parsedLabel.hasVisibleLabel) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextUnformatted(parsedLabel.visiblePortion.c_str());
        ImGui::PopStyleColor();
    }

    ApplyInputStyles();
    
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    } else {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, theme.GetColor(ThemeColorRole::SurfaceElevated));
    ImGui::PushStyleColor(ImGuiCol_Header, theme.GetColor(ThemeColorRole::Selection));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme.GetColor(ThemeColorRole::Hover));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme.GetColor(ThemeColorRole::Active));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, theme.GetRadius(ThemeRadiusRole::Popup));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, theme.GetBorder(ThemeBorderRole::Popup).thickness);

    bool changed = false;
    const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";
    bool open = false;

    if (parsedLabel.hasVisibleLabel) {
        ImGui::PushID(parsedLabel.fullLabel);
    }

    if (ImGui::BeginCombo("##combo", preview_value, ImGuiComboFlags_NoArrowButton)) {
        open = true;
        for (int i = 0; i < items_count; i++) {
            const bool is_selected = (*current_item == i);
            if (ImGui::Selectable(items[i], is_selected)) {
                *current_item = i;
                changed = true;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (parsedLabel.hasVisibleLabel) {
        ImGui::PopID();
    }

    bool isFocused = ImGui::IsItemFocused() || ImGui::IsItemActive();
    bool isHovered = ImGui::IsItemHovered();
    DrawComboChevron(open, isHovered, isFocused);
    DrawInputInnerShadow();
    DrawFocusAccent(isFocused);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    PopInputStyles();
    
    return changed;
}

bool UIComponents::ColorPicker(const char* label, ImVec4* color, const char* description) {
    bool changed = false;
    
    BeginCard(label, 0.0f, false);
    
    if (description) {
        CardHeader(label, description);
    } else {
        CardHeader(label);
    }
    
    ImGui::PushID(label);
    
    ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | 
                                ImGuiColorEditFlags_AlphaPreviewHalf | 
                                ImGuiColorEditFlags_DisplayHex | 
                                ImGuiColorEditFlags_DisplayRGB | 
                                ImGuiColorEditFlags_PickerHueBar;
                                
    ApplyInputStyles();
    
    if (ImGui::ColorPicker4("##picker", (float*)color, flags)) {
        changed = true;
    }
    
    PopInputStyles();
    ImGui::PopID();
    
    EndCard();
    
    return changed;
}

void UIComponents::ShowDemoWindow(bool* p_open) {
    if (p_open && !*p_open) return;
    
    auto& theme = ThemeManager::Instance();
    
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("UI Preview", p_open)) {
        
                SectionHeader("Input Controls", "Text, Numbers, Combos");
        static std::string textVal = "Hello UI";
        static int intVal = 42;
        static float floatVal = 3.14f;
        static int comboItem = 0;
        const char* items[] = { "Option 1", "Option 2", "Option 3" };
        
        InputText("Text Input", &textVal, "Enter some text...");
        InputInt("Integer", &intVal);
        InputFloat("Float", &floatVal);
        Combo("Dropdown", &comboItem, items, 3);
        
        SectionHeader("Color Configuration", "Accent picker");
        static ImVec4 accentColor = theme.GetColor(ThemeColorRole::Accent);
        ColorPicker("Theme Accent", &accentColor, "Select a new accent color");
        
        SectionHeader("Cards & Panels", "Basic content containers", "[#]");
        
        BeginCard("BasicCard");
        CardHeader("Standard Card", "With subtitle and basic content");
        ImGui::TextWrapped("This is a basic card using the UI system. It automatically handles padding, borders, and background colors.");
        CardFooter();
        if (PrimaryButton("Action Button")) {}
        ImGui::SameLine();
        if (SecondaryButton("Cancel")) {}
        EndCard();
        
        ImGui::Dummy(ImVec2(0, theme.GetSpacing(ThemeSpacingRole::CardGap)));
        
        BeginCard("AccentCard", 0.0f, true);
        CardHeader("Accent Card");
        ImGui::TextWrapped("Cards can use a left accent glow to highlight important information.");
        EndCard();
        
        SectionHeader("Interactive Controls", "Buttons, Toggles, Tabs");
        
        BeginTabs("DemoTabs");
        static int activeTab = 0;
        if (TabItem("General", activeTab == 0)) activeTab = 0;
        if (TabItem("Advanced", activeTab == 1)) activeTab = 1;
        if (TabItem("Danger Zone", activeTab == 2)) activeTab = 2;
        EndTabs();

        if (activeTab == 0) {
            PrimaryButton("Primary"); ImGui::SameLine();
            SecondaryButton("Secondary"); ImGui::SameLine();
            GhostButton("Ghost");
            
            ImGui::Spacing();
            static bool toggle1 = true;
            static bool toggle2 = false;
            Toggle("Enable Features", &toggle1);
            Toggle("Right Aligned Toggle", &toggle2, true);
        } else if (activeTab == 1) {
            static int selectedItem = -1;
            SelectableItem("sel1", "List Item 1", selectedItem == 0);
            if (ImGui::IsItemClicked()) selectedItem = 0;
            SelectableItem("sel2", "List Item 2", selectedItem == 1);
            if (ImGui::IsItemClicked()) selectedItem = 1;
            SelectableItem("sel3", "List Item 3", selectedItem == 2, "[*]");
            if (ImGui::IsItemClicked()) selectedItem = 2;
        } else {
            DangerButton("Delete Everything");
        }
        
        SectionHeader("Separators", "Dividing content");
        ImGui::Text("Content above");
        Separator();
        ImGui::Text("Content between");
        Separator("OR");
        ImGui::Text("Content below");
        
        SectionHeader("Info Rows", "Key-value pair display");
        
        BeginCard("StatsCard");
        CardHeader("Player Statistics");
        
        InfoRow("Kills", "1,245");
        InfoRow("Deaths", "842");
        ImVec4 successCol = theme.GetColor(ThemeColorRole::Success);
        InfoRow("K/D Ratio", "1.48", &successCol);
        
        Separator();
        
        InfoRow("Status", "Connected");
        ImVec4 accentCol = theme.GetColor(ThemeColorRole::Accent);
        InfoRow("Rank", "Diamond", &accentCol);
        EndCard();
    }
    ImGui::End();
}

} // namespace keybinder
