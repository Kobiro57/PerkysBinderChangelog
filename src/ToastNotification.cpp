/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    ToastNotification - lightweight overlay toast notifications
*/
#include "../include/ToastNotification.h"
#include "../include/ThemeManager.h"

#include <algorithm>
#include <cmath>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace keybinder {

namespace {

ImU32 MakeAlphaColor(const ImVec4& color, float alpha) {
    const float a = (std::max)(0.0f, (std::min)(1.0f, alpha * color.w));
    return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, a));
}

ImU32 MakeAlphaColorU32(ImU32 baseColor, float alpha) {
    ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(baseColor);
    rgba.w *= (std::max)(0.0f, (std::min)(1.0f, alpha));
    return ImGui::ColorConvertFloat4ToU32(rgba);
}

} // namespace

void ToastNotification::Enqueue(const std::string& feedName, const std::string& title, const std::string& author) {
    Toast toast;
    toast.feedName = feedName;
    toast.title = TruncateMessage(title, kMaxTitleLength);
    toast.author = author;
    toast.startTime = ImGui::GetTime();
    toast.duration = kDefaultDuration;
    toast.alpha = 1.0f;

    if (static_cast<int>(toasts_.size()) >= kMaxToasts) {
        toasts_.erase(toasts_.begin());
    }

    toasts_.push_back(std::move(toast));
}

void ToastNotification::Render(ImDrawList* drawList, float uiScale) {
    if (drawList == nullptr || toasts_.empty()) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f) {
        return;
    }

    const float scale = (std::max)(uiScale, 1.0f);
    const float gap = kToastGap * scale;
    const float padX = kToastPaddingX * scale;
    const float padY = kToastPaddingY * scale;
    const float marginRight = kToastMarginRight * scale;
    const float marginTop = kToastMarginTop * scale;
    const float minWidth = kToastMinWidth * scale;
    const float maxWidth = kToastMaxWidth * scale;
    const float radius = kToastRadius * scale;

    ImFont* font = ImGui::GetFont();
    if (font == nullptr) {
        return;
    }

    auto& theme = ThemeManager::Instance();
    const float captionSize = theme.GetTypography(ThemeTypographyRole::Caption).size * scale;
    const float bodySize = theme.GetTypography(ThemeTypographyRole::Body).size * scale;

    const ImVec4& bgColor = theme.GetColor(ThemeColorRole::SurfaceElevated);
    const ImVec4& borderColor = theme.GetColor(ThemeColorRole::Border);
    const ImVec4& accentColor = theme.GetColor(ThemeColorRole::Accent);
    const ImVec4& textPrimary = theme.GetColor(ThemeColorRole::TextPrimary);
    const ImVec4& textSecondary = theme.GetColor(ThemeColorRole::TextSecondary);
    const ImVec4& textMuted = theme.GetColor(ThemeColorRole::TextMuted);

    float cursorY = viewport->Pos.y + marginTop;

    for (int i = 0; i < static_cast<int>(toasts_.size()); ++i) {
        Toast& toast = toasts_[static_cast<std::size_t>(i)];
        const float alpha = toast.alpha;

        // Build the three-line layout:
        // Line 1: "Neuer Post in: [FeedName]" (accent color for header, muted for label)
        // Line 2: Post Title (primary color, larger)
        // Line 3: "von [Author]" (secondary color)
        
        const std::string headerPrefix = "Neuer Post in: ";
        const std::string authorPrefix = "von ";
        
        // Measure each component
        const ImVec2 headerPrefixSize = font->CalcTextSizeA(captionSize, FLT_MAX, 0.0f, headerPrefix.c_str());
        const ImVec2 feedNameSize = font->CalcTextSizeA(captionSize, maxWidth - padX * 2.0f, 0.0f, toast.feedName.c_str());
        const ImVec2 titleSize = font->CalcTextSizeA(bodySize, maxWidth - padX * 2.0f, 0.0f, toast.title.c_str());
        const ImVec2 authorPrefixSize = font->CalcTextSizeA(captionSize, FLT_MAX, 0.0f, authorPrefix.c_str());
        const ImVec2 authorSize = font->CalcTextSizeA(captionSize, maxWidth - padX * 2.0f, 0.0f, toast.author.c_str());
        
        const float lineHeight = headerPrefixSize.y;
        const float lineSpacing = 2.0f * scale;
        
        // Calculate total height and width
        const float contentWidth = (std::max)({
            headerPrefixSize.x + feedNameSize.x,
            titleSize.x,
            authorPrefixSize.x + authorSize.x
        });
        const float toastWidth = (std::max)(minWidth, (std::min)(maxWidth, contentWidth + padX * 2.0f));
        const float toastHeight = (lineHeight * 3.0f) + (lineSpacing * 2.0f) + padY * 2.0f;

        const float toastX = viewport->Pos.x + viewport->Size.x - toastWidth - marginRight;
        const float toastY = cursorY;

        const ImVec2 toastMin(toastX, toastY);
        const ImVec2 toastMax(toastX + toastWidth, toastY + toastHeight);

        // Background: semi-transparent dark with rounded corners
        drawList->AddRectFilled(toastMin, toastMax, MakeAlphaColor(bgColor, alpha), radius);


        // Subtle outer border
        drawList->AddRect(toastMin, toastMax, MakeAlphaColor(borderColor, alpha), radius, 0, 1.0f * scale);

        // Render the three lines
        float textY = toastY + padY;
        const float textX = toastX + padX;

        // Line 1: "Neuer Post in: " (muted) + FeedName (accent)
        drawList->AddText(font, captionSize, ImVec2(textX, textY), 
            MakeAlphaColor(textMuted, alpha), headerPrefix.c_str());
        drawList->AddText(font, captionSize, ImVec2(textX + headerPrefixSize.x, textY), 
            MakeAlphaColor(accentColor, alpha), toast.feedName.c_str());
        textY += lineHeight + lineSpacing;

        // Line 2: Post Title (primary, body size)
        drawList->AddText(font, bodySize, ImVec2(textX, textY), 
            MakeAlphaColor(textPrimary, alpha), toast.title.c_str());
        textY += lineHeight + lineSpacing;

        // Line 3: "von " (secondary) + Author (secondary, slightly emphasized)
        drawList->AddText(font, captionSize, ImVec2(textX, textY), 
            MakeAlphaColor(textSecondary, alpha), authorPrefix.c_str());
        drawList->AddText(font, captionSize, ImVec2(textX + authorPrefixSize.x, textY), 
            MakeAlphaColor(textSecondary, alpha), toast.author.c_str());

        // Click detection for dismiss (entire toast area)
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec2 mousePos = ImGui::GetIO().MousePos;
            if (mousePos.x >= toastMin.x && mousePos.x <= toastMax.x &&
                mousePos.y >= toastMin.y && mousePos.y <= toastMax.y) {
                Dismiss(i);
                break;
            }
        }

        cursorY += toastHeight + gap;
    }
}

void ToastNotification::Update() {
    const double currentTime = ImGui::GetTime();

    for (auto it = toasts_.begin(); it != toasts_.end(); ) {
        const float elapsed = static_cast<float>(currentTime - it->startTime);
        const float remaining = it->duration - elapsed;

        if (remaining <= 0.0f) {
            it = toasts_.erase(it);
            continue;
        }

        // Fade out during the last kFadeDuration seconds
        if (remaining < kFadeDuration) {
            it->alpha = remaining / kFadeDuration;
        } else {
            it->alpha = 1.0f;
        }

        ++it;
    }
}

void ToastNotification::Dismiss(int index) {
    if (index >= 0 && index < static_cast<int>(toasts_.size())) {
        toasts_.erase(toasts_.begin() + index);
    }
}

std::string ToastNotification::TruncateMessage(const std::string& message, std::size_t maxLength) const {
    if (message.length() <= maxLength) {
        return message;
    }

    return message.substr(0, maxLength - 3) + "...";
}

ImU32 ToastNotification::ApplyAlpha(ImU32 color, float alpha) const {
    return MakeAlphaColorU32(color, alpha);
}

} // namespace keybinder
