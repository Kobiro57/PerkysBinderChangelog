#pragma once

#include <vector>
#include <string>
#include <functional>
#include "Keybind.h"

namespace keybinder {

class KeybindListUI {
public:
    KeybindListUI() = default;
    ~KeybindListUI() = default;

    // Render the list screen
    void Render();
    
    // Update displayed keybinds
    void SetKeybinds(const std::vector<Keybind>& keybinds);
    
    // Get currently selected keybind
    const Keybind* GetSelectedKeybind() const;
    
    // Set category filter
    void SetFilterCategory(const std::string& category);
    
    // Set text search filter
    void SetFilterSearch(const std::string& search);
    
    // Callbacks
    std::function<void()> OnCreateClicked;
    std::function<void(const Keybind&)> OnKeybindSelected;

private:
    std::vector<Keybind> keybinds_;
    
    // Filters
    std::string filterCategory_ = "";
    std::string filterSearch_ = "";
    
    // Internal state for ImGui widgets
    char searchBuf_[256] = {0};
    int selectedIndex_ = -1; // Index in the original keybinds_ array
    
    // Helpers
    std::string FormatKeyString(const Keybind& kb) const;
    std::vector<std::string> GetUniqueCategories() const;
};

} // namespace keybinder
