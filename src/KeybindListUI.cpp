#include "../include/KeybindListUI.h"
#include "../include/AnimationHelper.h"
#include "../include/KeyInputUtils.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/imgui-1.92.7/imgui_internal.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <windows.h>

namespace keybinder {

namespace {

ThemeManager& Theme() {
    return ThemeManager::Instance();
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string TruncateWithEllipsis(const std::string& value, size_t maxChars) {
    if (value.size() <= maxChars) {
        return value;
    }

    if (maxChars <= 1) {
        return "…";
    }

    return value.substr(0, maxChars - 1) + "…";
}

ImVec4 BlendColor(const ImVec4& from, const ImVec4& to, float t) {
    return ImVec4(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t,
        from.w + (to.w - from.w) * t);
}

std::string NormalizeCategoryForDisplay(const std::string& category) {
    if (category.empty() || category == "Default") {
        return "Standard";
    }

    return category;
}

std::string StatusLabel(bool enabled) {
    return enabled ? "Aktiv" : "Inaktiv";
}
bool DrawKeybindRow(
    const char* id,
    const std::string& name,
    const std::string& trigger,
    const std::string& category,
    const std::string& status,
    bool selected) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID itemId = window->GetID(id);
    auto& theme = Theme();

    const float padX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float padY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.78f;
    const float rowGap = theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.18f;

    const std::string title = TruncateWithEllipsis(name, 34);
    const std::string triggerPreview = TruncateWithEllipsis(trigger, 30);
    const std::string categoryPreview = TruncateWithEllipsis(category, 14);

    const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
    const ImVec2 metaSize = ImGui::CalcTextSize(triggerPreview.c_str());
    const float rowHeight = titleSize.y + metaSize.y + padY * 2.0f + rowGap;
    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(ImGui::GetContentRegionAvail().x, rowHeight);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, itemId)) {
        return false;
    }

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, itemId, &hovered, &held);

    char progressKey[64];
    snprintf(progressKey, sizeof(progressKey), "row_progress_%llu", static_cast<unsigned long long>(itemId));
    float* pProgress = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID(progressKey), 0.0f);

    const float targetProgress = selected ? 1.0f : (hovered ? 0.45f : 0.0f);
    const float duration = AnimationHelper::GetDefaultDuration(AnimationHelper::AnimationType::Hover);
    const float step = ImGui::GetIO().DeltaTime / duration;

    if (*pProgress < targetProgress) {
        *pProgress = (std::min)(*pProgress + step, targetProgress);
    } else if (*pProgress > targetProgress) {
        *pProgress = (std::max)(*pProgress - step, targetProgress);
    }

    const float t = AnimationHelper::EaseInOut(*pProgress);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = theme.GetRadius(ThemeRadiusRole::Child);

    if (t > 0.0f) {
        if (selected) {
            ImVec4 bgCol = theme.GetColor(ThemeColorRole::SurfaceElevated);
            bgCol = BlendColor(bgCol, theme.GetColor(ThemeColorRole::Accent), 0.18f);
            bgCol.w = 0.94f;
            drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bgCol), radius);

            ImVec4 borderCol = theme.GetColor(ThemeColorRole::Accent);
            borderCol.w = 0.32f;
            drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(borderCol), radius, 0, 1.0f);
        } else {
            ImVec4 bgCol = BlendColor(
                theme.GetColor(ThemeColorRole::SurfaceElevated),
                theme.GetColor(ThemeColorRole::Hover), t);
            bgCol.w = 0.55f * t;
            drawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bgCol), radius);

            ImVec4 borderCol = theme.GetColor(ThemeColorRole::Hover);
            borderCol.w = 0.08f * t;
            drawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(borderCol), radius, 0, 1.0f);
        }
    }

    const float statusPadX = padX * 0.75f;
    const float statusPadY = padY * 0.32f;
    const ImVec2 statusSize = ImGui::CalcTextSize(status.c_str());
    const float statusWidth = statusSize.x + statusPadX * 2.0f;
    const float statusHeight = statusSize.y + statusPadY * 2.0f;
    const ImVec2 statusMin(bb.Max.x - padX - statusWidth, bb.Min.y + padY - 1.0f);
    const ImVec2 statusMax(statusMin.x + statusWidth, statusMin.y + statusHeight);

    ImVec4 statusFill = status == "Aktiv" ? theme.GetColor(ThemeColorRole::Success) : theme.GetColor(ThemeColorRole::TextMuted);
    statusFill.w = status == "Aktiv" ? 0.16f : 0.12f;
    ImVec4 statusBorder = status == "Aktiv" ? theme.GetColor(ThemeColorRole::Success) : theme.GetColor(ThemeColorRole::BorderStrong);
    statusBorder.w = status == "Aktiv" ? 0.26f : 0.18f;
    const ImVec4 statusTextCol = status == "Aktiv" ? theme.GetColor(ThemeColorRole::Success) : theme.GetColor(ThemeColorRole::TextSecondary);

    drawList->AddRectFilled(statusMin, statusMax, ImGui::GetColorU32(statusFill), statusHeight * 0.5f);
    drawList->AddRect(statusMin, statusMax, ImGui::GetColorU32(statusBorder), statusHeight * 0.5f, 0, 1.0f);
    drawList->AddText(
        ImVec2(
            (statusMin.x + statusMax.x - statusSize.x) * 0.5f,
            (statusMin.y + statusMax.y - statusSize.y) * 0.5f),
        ImGui::GetColorU32(statusTextCol), status.c_str());

    const ImVec2 titlePos(bb.Min.x + padX, bb.Min.y + padY - 1.0f);
    const float titleClipMaxX = statusMin.x - padX;
    drawList->PushClipRect(titlePos, ImVec2(titleClipMaxX, bb.Max.y), true);
    drawList->AddText(titlePos, ImGui::GetColorU32(theme.GetColor(ThemeColorRole::TextPrimary)), title.c_str());
    drawList->PopClipRect();

    const ImVec2 metaPos(bb.Min.x + padX, titlePos.y + titleSize.y + rowGap);
    drawList->AddText(metaPos, ImGui::GetColorU32(theme.GetColor(ThemeColorRole::TextSecondary)), triggerPreview.c_str());

    const float chipPadX = padX * 0.65f;
    const float chipPadY = padY * 0.16f;
    const ImVec2 categorySize = ImGui::CalcTextSize(categoryPreview.c_str());
    const float chipWidth = categorySize.x + chipPadX * 2.0f;
    const float chipHeight = categorySize.y + chipPadY * 2.0f;
    const float chipX = metaPos.x + metaSize.x + padX * 0.85f;
    const ImVec2 chipMin(chipX, metaPos.y - chipPadY + 1.0f);
    const ImVec2 chipMax(chipX + chipWidth, chipMin.y + chipHeight);

    ImVec4 chipFill = theme.GetColor(ThemeColorRole::Accent);
    chipFill.w = 0.10f;
    ImVec4 chipBorder = theme.GetColor(ThemeColorRole::Accent);
    chipBorder.w = 0.18f;
    drawList->AddRectFilled(chipMin, chipMax, ImGui::GetColorU32(chipFill), chipHeight * 0.45f);
    drawList->AddRect(chipMin, chipMax, ImGui::GetColorU32(chipBorder), chipHeight * 0.45f, 0, 1.0f);
    drawList->AddText(ImVec2(chipMin.x + chipPadX, chipMin.y + chipPadY - 1.0f), ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Accent)), categoryPreview.c_str());

    return pressed;
}

void DrawCompactDivider() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImVec4 divider = Theme().GetColor(ThemeColorRole::BorderStrong);
    divider.w = 0.10f;
    drawList->AddLine(
        ImVec2(start.x + Theme().GetSpacing(ThemeSpacingRole::FramePaddingX) * 0.5f, start.y),
        ImVec2(start.x + width - Theme().GetSpacing(ThemeSpacingRole::FramePaddingX) * 0.5f, start.y),
        ImGui::GetColorU32(divider),
        1.0f);
    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.45f));
}

}

static inline float GetLayoutScale() {
    return ThemeManager::Instance().GetAppliedScale();
}

static inline float Scale(float value) {
    return value * GetLayoutScale();
}

static std::string GetKeyName(int vk) {
    return GetLocalizedVirtualKeyName(vk);
}

std::string KeybindListUI::FormatKeyString(const Keybind& kb) const {
    std::vector<int> allKeys = kb.GetAllKeys();
    std::string res;

    if (!allKeys.empty()) {
        for (size_t i = 0; i < allKeys.size(); ++i) {
            if (i > 0) res += " + ";
            res += GetKeyName(allKeys[i]);
        }
    }

    if (!kb.slashCommand.empty()) {
        if (!res.empty()) res += " | ";
        res += "/" + kb.slashCommand;
    }

    if (res.empty()) return "Nicht gesetzt";
    return res;
}

std::vector<std::string> KeybindListUI::GetUniqueCategories() const {
    std::set<std::string> cats;
    for (const auto& kb : keybinds_) {
        cats.insert(NormalizeCategoryForDisplay(kb.category));
    }
    std::vector<std::string> res(cats.begin(), cats.end());
    return res;
}

void KeybindListUI::SetKeybinds(const std::vector<Keybind>& keybinds) {
    keybinds_ = keybinds;
    if (selectedIndex_ >= (int)keybinds_.size()) {
        selectedIndex_ = -1;
    }
}

const Keybind* KeybindListUI::GetSelectedKeybind() const {
    if (selectedIndex_ >= 0 && selectedIndex_ < (int)keybinds_.size()) {
        return &keybinds_[selectedIndex_];
    }
    return nullptr;
}

void KeybindListUI::SetFilterCategory(const std::string& category) {
    filterCategory_ = category;
}

void KeybindListUI::SetFilterSearch(const std::string& search) {
    filterSearch_ = search;
    strncpy_s(searchBuf_, sizeof(searchBuf_), search.c_str(), _TRUNCATE);
}

void KeybindListUI::Render() {
    const bool isEmpty = keybinds_.empty();

    if (isEmpty) {
        UIComponents::CardHeader("Belegungen", "Lege die erste Belegung an, um loszulegen.");
    } else {
        UIComponents::CardHeader("Belegungen", nullptr);
    }

    if (isEmpty) {
        float windowWidth = ImGui::GetContentRegionAvail().x;
        const char* emptyText = "Noch keine Tastenbelegungen vorhanden.";
        float textWidth = ImGui::CalcTextSize(emptyText).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "%s", emptyText);

        ImGui::Spacing();

        const char* hintText = "Erstelle deine erste Tastenbelegung, um loszulegen.";
        float hintWidth = ImGui::CalcTextSize(hintText).x;
        ImGui::SetCursorPosX((windowWidth - hintWidth) * 0.5f);
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextMuted), "%s", hintText);

        ImGui::Spacing();
        ImGui::Spacing();

        const char* btnText = "Erste Tastenbelegung erstellen";
        float btnWidth = ImGui::CalcTextSize(btnText).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX((windowWidth - btnWidth) * 0.5f);
        if (UIComponents::PrimaryButton(btnText)) {
            if (OnCreateClicked) {
                OnCreateClicked();
            }
        }
        return;
    }

    const float buttonSize = Theme().GetControlHeight(ThemeControlHeightRole::Button);

    ImGui::BeginGroup();

    if (UIComponents::PrimaryButton("+", ImVec2(buttonSize, 0.0f))) {
        if (OnCreateClicked) {
            OnCreateClicked();
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Neue Tastenbelegung erstellen");
    }
    
    ImGui::SameLine();
    
    std::string searchStr = searchBuf_;
    if (UIComponents::InputText("##Search", &searchStr, "Suche…")) {
        filterSearch_ = searchStr;
        strncpy_s(searchBuf_, sizeof(searchBuf_), searchStr.c_str(), _TRUNCATE);
    }

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, Theme().GetColor(ThemeColorRole::TextMuted));
    ImGui::TextUnformatted("Kategorie");
    ImGui::PopStyleColor();

    auto cats = GetUniqueCategories();

    std::vector<std::string> filterCats = {"Alle Kategorien"};
    filterCats.insert(filterCats.end(), cats.begin(), cats.end());

    std::string currentCombo = filterCategory_.empty() ? "Alle Kategorien" : filterCategory_;
    int selectedCombo = 0;

    std::vector<const char*> comboItems;
    for (size_t i = 0; i < filterCats.size(); ++i) {
        comboItems.push_back(filterCats[i].c_str());
        if (filterCats[i] == currentCombo) {
            selectedCombo = (int)i;
        }
    }

    if (UIComponents::Combo("##CategoryFilter", &selectedCombo, comboItems.data(), (int)comboItems.size())) {
        std::string selectedCat = filterCats[selectedCombo];
        if (selectedCat == "Alle Kategorien") {
            filterCategory_ = "";
        } else {
            filterCategory_ = selectedCat;
        }
    }

    ImGui::EndGroup();

    ImGui::Spacing();
    UIComponents::Separator("Übersicht");
    ImGui::Dummy(ImVec2(0.0f, Theme().GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.35f));

    bool anyMatches = false;

    const std::string searchLower = ToLowerCopy(filterSearch_);

    bool firstItem = true;
    for (size_t i = 0; i < keybinds_.size(); ++i) {
        const auto& kb = keybinds_[i];
        const std::string label = kb.name.empty() ? "Unbenannt" : kb.name;
        const std::string normalizedCategory = NormalizeCategoryForDisplay(kb.category);
        const std::string fullTriggerDisplay = FormatKeyString(kb);
        const std::string statusText = StatusLabel(kb.enabled);
        
        if (!filterCategory_.empty() && normalizedCategory != filterCategory_) {
            continue;
        }
        
        if (!searchLower.empty()) {
            const std::string searchableText = ToLowerCopy(label + " " + fullTriggerDisplay + " " + normalizedCategory + " " + statusText);
            if (searchableText.find(searchLower) == std::string::npos) {
                continue;
            }
        }
        
        anyMatches = true;
        
        if (!firstItem) {
            DrawCompactDivider();
        }
        firstItem = false;

        bool isSelected = (selectedIndex_ == (int)i);

        std::string selectableId = "KeybindRow##" + std::to_string(i);
        if (DrawKeybindRow(selectableId.c_str(), label, fullTriggerDisplay, normalizedCategory, statusText, isSelected)) {
            selectedIndex_ = (int)i;
            if (OnKeybindSelected) {
                OnKeybindSelected(kb);
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Name: %s\nTrigger: %s\nKategorie: %s\nStatus: %s",
                label.c_str(),
                fullTriggerDisplay.c_str(),
                normalizedCategory.c_str(),
                statusText.c_str());
        }
    }
    
    if (!anyMatches) {
        ImGui::TextColored(Theme().GetColor(ThemeColorRole::TextSecondary), "Keine Tastenbelegungen gefunden.");
    }
}

}
