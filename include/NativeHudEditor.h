#pragma once

#include "InterfaceLayout.h"
#include <array>
#include <cstdint>
#include <string>

namespace keybinder {

// Transforms the game's own HUD draw calls. No parallel HUD, gameplay values,
// server textdraws, chat or global HUD palette entries are changed.
class NativeHudEditor {
public:
    bool Initialize();
    void Shutdown();
    void SetLayout(const HudLayout& layout);
    const std::string& GetLastError() const { return last_error_; }
    bool GetElementBounds(HudElement element, float& x, float& y, float& width, float& height) const;

    // Shared by native callbacks and focused tests, using ARGB vertex colors.
    void TransformPoint(HudElement element, float& x, float& y, float width, float height) const;
    std::uint32_t TransformColor(HudElement element, std::uint32_t argb, bool text = false) const;
    static HudElement ClassifyPlayerInfoText(const char* text);

private:
    friend struct NativeHudHooks;
    struct Bounds { bool valid = false; float left = 0, top = 0, right = 0, bottom = 0; };
    void ResetBounds(HudElement element);
    void RecordPoint(HudElement element, float x, float y);
    HudLayout layout_{};
    std::array<Bounds, kHudElementCount> bounds_{};
    std::string last_error_;
    bool initialized_ = false;
    int viewport_width_ = 0;
    int viewport_height_ = 0;
};

} // namespace keybinder
