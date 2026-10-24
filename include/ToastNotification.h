/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    ToastNotification - lightweight overlay toast notifications
*/
#pragma once

#include <string>
#include <vector>

#include "imgui-1.92.7/imgui.h"

namespace keybinder {

struct Toast {
    std::string feedName;      // "ForumName" for "Neuer Post in: ForumName"
    std::string title;         // Post title
    std::string author;        // Creator name for "von CreatorName"
    double startTime = 0.0;
    float duration = 5.0f;
    float alpha = 1.0f;
};

class ToastNotification {
public:
    static constexpr int kMaxToasts = 3;
    static constexpr float kDefaultDuration = 5.0f;
    static constexpr float kFadeDuration = 1.0f;
    static constexpr float kToastGap = 8.0f;
    static constexpr float kToastPaddingX = 14.0f;
    static constexpr float kToastPaddingY = 10.0f;
    static constexpr float kToastMarginRight = 16.0f;
    static constexpr float kToastMarginTop = 16.0f;
    static constexpr float kToastMinWidth = 280.0f;
    static constexpr float kToastMaxWidth = 380.0f;
    static constexpr std::size_t kMaxTitleLength = 60;
    static constexpr float kToastRadius = 8.0f;

    ToastNotification() = default;

    void Enqueue(const std::string& feedName, const std::string& title, const std::string& author);
    void Render(ImDrawList* drawList, float uiScale);
    void Update();
    void Dismiss(int index);

    bool IsEmpty() const { return toasts_.empty(); }
    int GetToastCount() const { return static_cast<int>(toasts_.size()); }

private:
    std::vector<Toast> toasts_;

    std::string TruncateMessage(const std::string& message, std::size_t maxLength) const;
    ImU32 ApplyAlpha(ImU32 color, float alpha) const;
};

} // namespace keybinder
