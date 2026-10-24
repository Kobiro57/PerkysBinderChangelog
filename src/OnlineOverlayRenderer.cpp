#include "../include/OnlineOverlayRenderer.h"
#include "../include/OnlineDataManager.h"
#include "../include/OnlineOverlayController.h"
#include "../include/SettingsManager.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/Logger.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <utility>
#include <windows.h>

namespace keybinder {

namespace {

ImVec4 WithAlpha(const ImVec4& color, float alpha) {
    ImVec4 adjusted = color;
    adjusted.w = alpha;
    return adjusted;
}

ImVec4 ResolveOverlayTextColor(const PersistentOverlayData& data, const ImVec4& fallback) {
    ImVec4 color = data.customTextColorEnabled
        ? ImVec4(data.textColorR, data.textColorG, data.textColorB, data.textColorA)
        : fallback;
    color.w *= data.opacity;
    return color;
}

void RenderTruncatedOverlayText(const std::string& text, const ImVec4& color, float maxWidth) {
    const auto truncated = UIComponents::TruncateTextToPixelWidth(text.c_str(), (std::max)(0.0f, maxWidth));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(truncated.first.c_str());
    ImGui::PopStyleColor();

    if (truncated.second && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text.c_str());
    }
}

} // namespace

OnlineQuickOverlayMode OnlineOverlayRenderer::ParseMode(const std::string& value) {
    if (value == "hold") {
        return OnlineQuickOverlayMode::Hold;
    }
    return OnlineQuickOverlayMode::Toggle;
}

OnlineOverlayRenderer::OnlineOverlayRenderer() = default;

OnlineOverlayRenderer::~OnlineOverlayRenderer() {
    Shutdown();
}

void OnlineOverlayRenderer::Initialize(SettingsManager* settings, OnlineDataManager* dataManager) {
    settingsManager_ = settings;
    dataManager_ = dataManager;
    RefreshSettings();
}

void OnlineOverlayRenderer::Shutdown() {
    visible_ = false;
    hotkeyWasDown_ = false;
    inputWasGated_ = false;
    settingsManager_ = nullptr;
    dataManager_ = nullptr;
    quickMapVisibleCallback_ = nullptr;
    overlayVisibleCallback_ = nullptr;
    inputGateCallback_ = nullptr;
#ifdef KEYBINDER_TESTING
    testingHooks_ = {};
#endif
}

void OnlineOverlayRenderer::RefreshSettings() {
    if (settingsManager_ == nullptr) {
        return;
    }

    const Settings& s = settingsManager_->Get();
    mode_ = ParseMode(s.online_quick_overlay_mode);
    hotkey_ = s.online_quick_overlay_hotkey;
    showLspd_ = s.online_quick_overlay_show_lspd;
    showArmy_ = s.online_quick_overlay_show_army;
    showFireDepartment_ = s.online_quick_overlay_show_fire_department;
    showZivilisten_ = s.online_quick_overlay_show_zivilisten;
    showAllGangs_ = s.online_quick_overlay_show_all_gangs;
    selectedGangs_ = s.online_quick_overlay_selected_gangs;
}

bool OnlineOverlayRenderer::IsFeatureEnabled() const {
    if (settingsManager_ == nullptr) {
        return false;
    }
    return settingsManager_->Get().online_quick_overlay_enabled;
}

bool OnlineOverlayRenderer::IsHotkeyBound() const {
    return hotkey_ != 0;
}

bool OnlineOverlayRenderer::IsHotkeyDown() const {
    if (hotkey_ <= 0) {
        return false;
    }

#ifdef KEYBINDER_TESTING
    if (testingHooks_.isKeyDown) {
        return testingHooks_.isKeyDown(hotkey_);
    }
#endif

    return (GetAsyncKeyState(hotkey_) & 0x8000) != 0;
}

bool OnlineOverlayRenderer::IsInputGated() const {
#ifdef KEYBINDER_TESTING
    if (testingHooks_.isInputGated) {
        return testingHooks_.isInputGated();
    }
#endif

    return inputGateCallback_ != nullptr && inputGateCallback_();
}

bool OnlineOverlayRenderer::ShouldBeVisible() const {
    if (!IsFeatureEnabled() || !IsHotkeyBound()) {
        return false;
    }

    // Mutual exclusivity with QuickMap
    if (quickMapVisibleCallback_ && quickMapVisibleCallback_()) {
        return false;
    }

    // Mutual exclusivity with main overlay
    if (overlayVisibleCallback_ && overlayVisibleCallback_()) {
        return false;
    }

    if (mode_ == OnlineQuickOverlayMode::Hold) {
        return !IsInputGated() && IsHotkeyDown();
    }

    // Toggle mode: visibility managed by Show/Hide/Toggle
    return !IsInputGated() && visible_;
}

void OnlineOverlayRenderer::Show() {
    if (!IsFeatureEnabled() || !IsHotkeyBound()) {
        return;
    }
    // Don't show if QuickMap or main overlay is visible
    if (quickMapVisibleCallback_ && quickMapVisibleCallback_()) {
        return;
    }
    if (overlayVisibleCallback_ && overlayVisibleCallback_()) {
        return;
    }
    if (IsInputGated()) {
        return;
    }
    visible_ = true;
}

void OnlineOverlayRenderer::Hide() {
    visible_ = false;
}

void OnlineOverlayRenderer::Toggle() {
    if (visible_) {
        Hide();
    } else {
        Show();
    }
}

void OnlineOverlayRenderer::Update() {
    RefreshSettings();

    const bool inputGated = IsInputGated();

    if (inputGated && !inputWasGated_) {
        visible_ = false;
    }

    // Hold mode: visibility driven entirely by key state unless gameplay input is gated.
    if (mode_ == OnlineQuickOverlayMode::Hold) {
        const bool keyDown = IsHotkeyDown();
        visible_ = !inputGated && keyDown && IsFeatureEnabled() && IsHotkeyBound();
        hotkeyWasDown_ = keyDown;
        inputWasGated_ = inputGated;
        return;
    }

    // Toggle mode: detect key press edge to flip visibility
    if (!IsFeatureEnabled() || !IsHotkeyBound()) {
        visible_ = false;
        hotkeyWasDown_ = false;
        inputWasGated_ = inputGated;
        return;
    }

    bool keyDown = IsHotkeyDown();
    if (!inputGated && keyDown && !hotkeyWasDown_) {
        // Key just pressed — toggle
        if (quickMapVisibleCallback_ && quickMapVisibleCallback_()) {
            // QuickMap visible, don't toggle on
        } else if (overlayVisibleCallback_ && overlayVisibleCallback_()) {
            // Main overlay visible, don't toggle on
        } else {
            visible_ = !visible_;
        }
    }
    hotkeyWasDown_ = keyDown;
    inputWasGated_ = inputGated;
}

void OnlineOverlayRenderer::SetQuickMapVisibleCallback(std::function<bool()> callback) {
    quickMapVisibleCallback_ = std::move(callback);
}

void OnlineOverlayRenderer::SetOverlayVisibleCallback(std::function<bool()> callback) {
    overlayVisibleCallback_ = std::move(callback);
}

void OnlineOverlayRenderer::SetInputGateCallback(std::function<bool()> callback) {
    inputGateCallback_ = std::move(callback);
}

#ifdef KEYBINDER_TESTING
void OnlineOverlayRenderer::SetTestingHooks(const TestingHooks& hooks) {
    testingHooks_ = hooks;
}
#endif

bool OnlineOverlayRenderer::ShouldShowFaction(const std::string& factionName) const {
    if (factionName == "LSPD") {
        return showLspd_;
    }
    if (factionName == "Army") {
        return showArmy_;
    }
    if (factionName == "Fire Department") {
        return showFireDepartment_;
    }
    return true;
}

bool OnlineOverlayRenderer::ShouldShowGang(const std::string& gangName) const {
    if (gangName == "Zivilisten") {
        return showZivilisten_;
    }
    if (showAllGangs_) {
        return true;
    }
    // Check selected gangs list
    for (const auto& selected : selectedGangs_) {
        if (selected == gangName) {
            return true;
        }
    }
    return false;
}

int OnlineOverlayRenderer::CountFilteredPlayers(const OnlineSnapshot& snapshot) const {
    int count = 0;
    for (const auto& bucket : snapshot.factionBuckets) {
        if (ShouldShowFaction(bucket.bucketName)) {
            count += bucket.playerCount;
        }
    }
    for (const auto& bucket : snapshot.gangBuckets) {
        if (ShouldShowGang(bucket.bucketName)) {
            count += bucket.playerCount;
        }
    }
    return count;
}

void OnlineOverlayRenderer::Render() {
    if (!ShouldBeVisible()) {
        return;
    }
    if (dataManager_ == nullptr) {
        return;
    }

    const OnlineSnapshot snapshot = dataManager_->GetSnapshot();

    // If snapshot is offline and we've never had data, just skip rendering
    if (!snapshot.IsUsable() && snapshot.totalPlayers == 0) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    const float scale = ImGui::GetIO().FontGlobalScale;
    const float panelWidth = (std::min)(760.0f * scale, viewport->Size.x * 0.6f);
    const float panelMaxHeight = viewport->Size.y * 0.4f;

    // Center the panel
    const ImVec2 panelPos(
        viewport->Pos.x + (viewport->Size.x - panelWidth) * 0.5f,
        viewport->Pos.y + (viewport->Size.y * 0.5f) - (panelMaxHeight * 0.5f));

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, -1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(panelWidth, 0.0f), ImVec2(panelWidth, panelMaxHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavInputs
        | ImGuiWindowFlags_NoMouseInputs;

   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale, 12.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    auto& theme = ThemeManager::Instance();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, WithAlpha(theme.GetColor(ThemeColorRole::Surface), 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.50f));

    if (ImGui::Begin("##OnlineQuickOverlay", nullptr, flags)) {
        RenderHeader(snapshot, panelWidth, scale);
        ImGui::Separator();
        RenderSummaryLine(snapshot, panelWidth, scale);
        ImGui::Separator();

        // Render filtered buckets
        int visibleCount = 0;

        for (const auto& bucket : snapshot.factionBuckets) {
            if (!ShouldShowFaction(bucket.bucketName)) {
                continue;
            }
            if (bucket.playerCount == 0) {
                continue;
            }
            RenderBucketList(bucket, panelWidth, scale);
            visibleCount++;
        }

        for (const auto& bucket : snapshot.gangBuckets) {
            if (!ShouldShowGang(bucket.bucketName)) {
                continue;
            }
            if (bucket.playerCount == 0) {
                continue;
            }
            RenderBucketList(bucket, panelWidth, scale);
            visibleCount++;
        }

        if (visibleCount == 0) {
            RenderEmptyState(panelWidth, scale);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void OnlineOverlayRenderer::RenderHeader(const OnlineSnapshot& snapshot, float panelWidth, float scale) {
    // Total count + fetch status
    auto& theme = ThemeManager::Instance();
    const char* statusText = "Offline";
    ImU32 statusColor = ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Danger));
    if (snapshot.fetchStatus == OnlineFetchStatus::Live) {
        statusText = "Live";
        statusColor = ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Success));
    } else if (snapshot.fetchStatus == OnlineFetchStatus::Stale) {
        statusText = "Stale";
        statusColor = ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Warning));
    }

    const int filteredCount = CountFilteredPlayers(snapshot);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
    ImGui::Text("Online: %d", filteredCount);
    ImGui::SameLine();

    float statusTextWidth = ImGui::CalcTextSize(statusText).x;
    float availableWidth = panelWidth - ImGui::GetCursorPosX() - ImGui::GetStyle().WindowPadding.x;
    if (availableWidth > statusTextWidth + 4.0f * scale) {
        ImGui::SameLine(ImGui::GetCursorPosX() + availableWidth - statusTextWidth);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
        ((statusColor >> 0) & 0xFF) / 255.0f,
        ((statusColor >> 8) & 0xFF) / 255.0f,
        ((statusColor >> 16) & 0xFF) / 255.0f,
        1.0f));
    ImGui::Text("%s", statusText);
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

void OnlineOverlayRenderer::RenderSummaryLine(const OnlineSnapshot& snapshot, float panelWidth, float scale) {
    // Count per faction/gang category
    int lspd = 0, army = 0, fd = 0, gangs = 0, ziv = 0;

    for (const auto& bucket : snapshot.factionBuckets) {
        if (bucket.bucketName == "LSPD") lspd = bucket.playerCount;
        else if (bucket.bucketName == "Army") army = bucket.playerCount;
        else if (bucket.bucketName == "Fire Department") fd = bucket.playerCount;
    }

    for (const auto& bucket : snapshot.gangBuckets) {
        if (bucket.bucketName == "Zivilisten") {
            ziv = bucket.playerCount;
        } else {
            gangs += bucket.playerCount;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeManager::Instance().GetColor(ThemeColorRole::TextSecondary));
    ImGui::Text("LSPD: %d | Army: %d | FD: %d | Gangs: %d | Ziv: %d", lspd, army, fd, gangs, ziv);
    ImGui::PopStyleColor();
}

void OnlineOverlayRenderer::RenderBucketList(const OnlineBucketSummary& bucket, float panelWidth, float scale) {
    // Compact group header with count
    auto& theme = ThemeManager::Instance();
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::Text("%s (%d)", bucket.bucketName.c_str(), bucket.playerCount);
    ImGui::PopStyleColor();

    // Compact 1-2 column name list
    if (bucket.playerNames.empty()) {
        return;
    }

    const float indent = 8.0f * scale;
    ImGui::Indent(indent);

    const float nameLineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float availWidth = panelWidth - indent * 2.0f - ImGui::GetStyle().WindowPadding.x * 2.0f;
    const float colWidth = availWidth * 0.5f;

    // Use columns if enough names
    const bool useColumns = bucket.playerNames.size() > 3 && availWidth > 300.0f * scale;

    if (useColumns) {
        int col = 0;
        for (const auto& name : bucket.playerNames) {
            if (col == 1) {
                ImGui::SameLine(colWidth + indent);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), 0.9f));
            ImGui::Text("%s", name.c_str());
            ImGui::PopStyleColor();
            col = 1 - col;
        }
    } else {
        for (const auto& name : bucket.playerNames) {
            ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), 0.9f));
            ImGui::Text("%s", name.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::Unindent(indent);
}

void OnlineOverlayRenderer::RenderEmptyState(float panelWidth, float scale) {
    ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(ThemeManager::Instance().GetColor(ThemeColorRole::TextMuted), 0.8f));
    ImGui::Text("No groups selected or no players online");
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Persistent overlay rendering
// ---------------------------------------------------------------------------

void OnlineOverlayRenderer::SetOverlayController(OnlineOverlayController* controller) {
    overlayController_ = controller;
}

void OnlineOverlayRenderer::SetPersistentOverlayMoveMode(bool enabled) {
    if (persistentOverlayMoveMode_ == enabled) {
        return;
    }

    persistentOverlayMoveMode_ = enabled;
    if (!persistentOverlayMoveMode_ && persistentOverlayMovedSinceSave_) {
        if (settingsPersistCallback_) {
            settingsPersistCallback_();
        }
        persistentOverlayMovedSinceSave_ = false;
    }
}

void OnlineOverlayRenderer::TogglePersistentOverlayMoveMode() {
    SetPersistentOverlayMoveMode(!persistentOverlayMoveMode_);
}

void OnlineOverlayRenderer::SetSettingsPersistCallback(std::function<void()> callback) {
    settingsPersistCallback_ = std::move(callback);
}

void OnlineOverlayRenderer::RenderPersistentOverlays() {
    if (overlayController_ == nullptr || settingsManager_ == nullptr) {
        return;
    }

    // Persistent overlays render even when the main overlay UI is hidden.
    if (!settingsManager_->Get().online_enabled) {
        SetPersistentOverlayMoveMode(false);
        return;
    }

    if (persistentOverlayMoveMode_ && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        SetPersistentOverlayMoveMode(false);
    }

    const float scale = ImGui::GetIO().FontGlobalScale;

    const PersistentOverlayData& localData = overlayController_->GetLocalOverlayData();
    const PersistentOverlayData& gangsData = overlayController_->GetGangsOverlayData();
    const PersistentOverlayData& ownGangData = overlayController_->GetOwnGangOverlayData();
    const PersistentOverlayData& wantedData = overlayController_->GetWantedOverlayData();

    if (persistentOverlayMoveMode_) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport != nullptr) {
            auto& theme = ThemeManager::Instance();
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const char* hint = "Move-Modus: Overlays mit der Maus ziehen, ESC beendet";
            const ImVec2 hintSize = ImGui::CalcTextSize(hint);
            const float pad = 8.0f * scale;
            const ImVec2 hintMin(viewport->Pos.x + (viewport->Size.x - hintSize.x) * 0.5f - pad, viewport->Pos.y + 18.0f * scale);
            const ImVec2 hintMax(hintMin.x + hintSize.x + pad * 2.0f, hintMin.y + hintSize.y + pad * 2.0f);
            drawList->AddRectFilled(hintMin, hintMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Surface), 0.86f)), 6.0f * scale);
            drawList->AddRect(hintMin, hintMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.45f)), 6.0f * scale, 0, 1.0f);
            drawList->AddText(ImVec2(hintMin.x + pad, hintMin.y + pad), ImGui::GetColorU32(theme.GetColor(ThemeColorRole::TextPrimary)), hint);
        }
    }

    if (localData.visible) {
        RenderSinglePersistentOverlay(localData, scale, 0.0f);
    }

    float ownGangStackedOffsetY = 0.0f;
    if (ownGangData.visible) {
        if (localData.visible && localData.anchor == ownGangData.anchor) {
            ownGangStackedOffsetY = 200.0f * ownGangData.scale;
        }
        RenderSinglePersistentOverlay(ownGangData, scale, ownGangStackedOffsetY);
    }
    if (gangsData.visible) {
        float stackedOffsetY = 0.0f;
        if (localData.visible && localData.anchor == gangsData.anchor) {
            stackedOffsetY += 200.0f * gangsData.scale;
        }
        if (ownGangData.visible && ownGangData.anchor == gangsData.anchor) {
            stackedOffsetY += 200.0f * gangsData.scale;
        }
        RenderSinglePersistentOverlay(gangsData, scale, stackedOffsetY);
    }

    if (wantedData.visible) {
        float stackedOffsetY = 0.0f;
        if (localData.visible && localData.anchor == wantedData.anchor) {
            stackedOffsetY += 200.0f * wantedData.scale;
        }
        if (ownGangData.visible && ownGangData.anchor == wantedData.anchor) {
            stackedOffsetY += 200.0f * wantedData.scale;
        }
        if (gangsData.visible && gangsData.anchor == wantedData.anchor) {
            stackedOffsetY += 200.0f * wantedData.scale;
        }
        RenderSinglePersistentOverlay(wantedData, scale, stackedOffsetY);
    }

    if (persistentOverlayMovedSinceSave_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (settingsPersistCallback_) {
            settingsPersistCallback_();
        }
        persistentOverlayMovedSinceSave_ = false;
    }
}

void OnlineOverlayRenderer::PersistMovedOverlayPosition(const PersistentOverlayData& data, const ImVec2& windowPos, const ImVec2& windowSize, float scale, float /*stackedOffsetY*/) {
    if (settingsManager_ == nullptr || scale <= 0.0f || !std::isfinite(scale)) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    // Auto-pick anchor from the panel's center quadrant. Whichever edge is
    // closer becomes the anchor, so the dragged side stays put as content
    // grows (left-placed panels grow rightward, right-placed panels grow
    // leftward, etc.). Predictable expansion is the whole point.
    const float centerX = windowPos.x + windowSize.x * 0.5f;
    const float centerY = windowPos.y + windowSize.y * 0.5f;
    const float viewportCenterX = viewport->Pos.x + viewport->Size.x * 0.5f;
    const float viewportCenterY = viewport->Pos.y + viewport->Size.y * 0.5f;
    const bool leftSide = centerX < viewportCenterX;
    const bool topSide = centerY < viewportCenterY;
    OnlineOverlayAnchor newAnchor;
    const char* newAnchorString;
    if (leftSide && topSide) {
        newAnchor = OnlineOverlayAnchor::TopLeft;
        newAnchorString = "top_left";
    } else if (!leftSide && topSide) {
        newAnchor = OnlineOverlayAnchor::TopRight;
        newAnchorString = "top_right";
    } else if (leftSide && !topSide) {
        newAnchor = OnlineOverlayAnchor::BottomLeft;
        newAnchorString = "bottom_left";
    } else {
        newAnchor = OnlineOverlayAnchor::BottomRight;
        newAnchorString = "bottom_right";
    }

    // Compute the stack offset under the NEW anchor (since the controller will
    // re-resolve stacking next frame using the new per-overlay anchor). If we
    // used the passed-in stackedOffsetY (which was computed with the old
    // anchor), an anchor flip that breaks a stacking relationship would shift
    // the dropped position by 200 px on the next render.
    const float newStackedY = [&]() -> float {
        if (overlayController_ == nullptr) return 0.0f;
        if (data.kind == PersistentOverlayKind::LocalAffiliation) return 0.0f;
        const PersistentOverlayData& local = overlayController_->GetLocalOverlayData();
        const PersistentOverlayData& own = overlayController_->GetOwnGangOverlayData();
        const PersistentOverlayData& gangs = overlayController_->GetGangsOverlayData();
        if (data.kind == PersistentOverlayKind::OwnGang) {
            return (local.visible && local.anchor == newAnchor) ? 200.0f * data.scale : 0.0f;
        }
        // GangsSummary and Wanted both stack beneath everything above them.
        float y = 0.0f;
        if (local.visible && local.anchor == newAnchor) y += 200.0f * data.scale;
        if (own.visible && own.anchor == newAnchor) y += 200.0f * data.scale;
        if (data.kind == PersistentOverlayKind::Wanted
            && gangs.visible && gangs.anchor == newAnchor) {
            y += 200.0f * data.scale;
        }
        return y;
    }();

    Settings& settings = settingsManager_->GetMutable();
    float& storedOffsetX = data.kind == PersistentOverlayKind::OwnGang
        ? settings.online_own_gang_overlay_offset_x
        : (data.kind == PersistentOverlayKind::Wanted
            ? settings.online_wanted_overlay_offset_x
            : settings.online_overlay_offset_x);
    float& storedOffsetY = data.kind == PersistentOverlayKind::OwnGang
        ? settings.online_own_gang_overlay_offset_y
        : (data.kind == PersistentOverlayKind::Wanted
            ? settings.online_wanted_overlay_offset_y
            : settings.online_overlay_offset_y);
    std::string& storedAnchor = data.kind == PersistentOverlayKind::LocalAffiliation
        ? settings.online_local_overlay_anchor
        : (data.kind == PersistentOverlayKind::OwnGang
            ? settings.online_own_gang_overlay_anchor
            : (data.kind == PersistentOverlayKind::Wanted
                ? settings.online_wanted_overlay_anchor
                : settings.online_gangs_overlay_anchor));
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    switch (newAnchor) {
    case OnlineOverlayAnchor::TopLeft:
        offsetX = (windowPos.x - viewport->Pos.x) / scale;
        offsetY = (windowPos.y - viewport->Pos.y) / scale - newStackedY;
        break;
    case OnlineOverlayAnchor::TopRight:
        offsetX = (viewport->Pos.x + viewport->Size.x - windowPos.x - windowSize.x) / scale;
        offsetY = (windowPos.y - viewport->Pos.y) / scale - newStackedY;
        break;
    case OnlineOverlayAnchor::BottomLeft:
        offsetX = (windowPos.x - viewport->Pos.x) / scale;
        offsetY = (viewport->Pos.y + viewport->Size.y - windowPos.y - windowSize.y) / scale - newStackedY;
        break;
    case OnlineOverlayAnchor::BottomRight:
        offsetX = (viewport->Pos.x + viewport->Size.x - windowPos.x - windowSize.x) / scale;
        offsetY = (viewport->Pos.y + viewport->Size.y - windowPos.y - windowSize.y) / scale - newStackedY;
        break;
    }

    if (!std::isfinite(offsetX) || !std::isfinite(offsetY)) {
        return;
    }

    const bool offsetChanged = std::fabs(storedOffsetX - offsetX) > 0.01f
        || std::fabs(storedOffsetY - offsetY) > 0.01f;
    const bool anchorChanged = storedAnchor != newAnchorString;
    if (offsetChanged || anchorChanged) {
        storedOffsetX = offsetX;
        storedOffsetY = offsetY;
        storedAnchor = newAnchorString;
        persistentOverlayMovedSinceSave_ = true;
    }
}

void OnlineOverlayRenderer::RenderSinglePersistentOverlay(const PersistentOverlayData& data, float scale, float stackedOffsetY) {
    const bool hasPlayerNames = !data.playerNames.empty();
    const bool hasCountRows = !data.countRows.empty();
    if (!data.visible || (!hasPlayerNames && !hasCountRows)) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return;
    }

    const float overlayScale = scale * data.scale;
    const int columns = hasCountRows ? 1 : OnlineOverlayController::ComputeColumnCount(data.playerCount);
    const float textScale = ((data.style == OnlineOverlayStyle::Dense) ? 0.9f : 1.0f) * data.scale;
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing() * textScale;

    const float colWidth = (hasCountRows ? 220.0f : 160.0f) * overlayScale;
    const bool hasHeader = (data.style == OnlineOverlayStyle::Card || data.style == OnlineOverlayStyle::Dense);
    const float headerHeight = hasHeader ? (28.0f * overlayScale) : 0.0f;
    const float padding = 8.0f * overlayScale;

    const int rowCount = hasCountRows ? static_cast<int>(data.countRows.size()) : data.playerCount;
    const float panelWidth = padding * 2.0f + static_cast<float>(columns) * colWidth;
    const int rowsPerCol = (rowCount + columns - 1) / columns;
    const float panelContentHeight = static_cast<float>(rowsPerCol) * rowHeight;
    const float panelHeight = headerHeight + panelContentHeight + padding * 2.0f;

    const float displayOffsetY = data.offsetY + stackedOffsetY;
    float posX = 0.0f;
    float posY = 0.0f;

    switch (data.anchor) {
    case OnlineOverlayAnchor::TopLeft:
        posX = viewport->Pos.x + data.offsetX * scale;
        posY = viewport->Pos.y + displayOffsetY * scale;
        break;
    case OnlineOverlayAnchor::TopRight:
        posX = viewport->Pos.x + viewport->Size.x - panelWidth - data.offsetX * scale;
        posY = viewport->Pos.y + displayOffsetY * scale;
        break;
    case OnlineOverlayAnchor::BottomLeft:
        posX = viewport->Pos.x + data.offsetX * scale;
        posY = viewport->Pos.y + viewport->Size.y - panelHeight - displayOffsetY * scale;
        break;
    case OnlineOverlayAnchor::BottomRight:
        posX = viewport->Pos.x + viewport->Size.x - panelWidth - data.offsetX * scale;
        posY = viewport->Pos.y + viewport->Size.y - panelHeight - displayOffsetY * scale;
        break;
    }

    if (!persistentOverlayMoveMode_) {
        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Appearing);
    }
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavInputs;

    if (!persistentOverlayMoveMode_) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoMouseInputs;
    }

    auto& theme = ThemeManager::Instance();
    const float rounding = 6.0f * overlayScale;
    const ImVec4 rowTextColor = ResolveOverlayTextColor(data, WithAlpha(theme.GetColor(ThemeColorRole::TextPrimary), 0.9f));
    const ImVec4 headerTextColor = ResolveOverlayTextColor(data, theme.GetColor(ThemeColorRole::TextSecondary));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, (data.backgroundEnabled && hasHeader && data.style == OnlineOverlayStyle::Card) ? 1.0f : 0.0f);

    ImVec4 bgColor = WithAlpha(theme.GetColor(ThemeColorRole::Surface), 0.0f);
    if (data.backgroundEnabled) {
        const float bgAlpha = data.opacity;
        bgColor = data.style == OnlineOverlayStyle::Minimal
            ? WithAlpha(theme.GetColor(ThemeColorRole::Surface), bgAlpha * 0.6f)
            : WithAlpha(theme.GetColor(ThemeColorRole::Surface), bgAlpha);
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
    const bool pushedBorder = data.backgroundEnabled && hasHeader;
    if (pushedBorder) {
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.40f));
    }

    char windowId[64];
    snprintf(windowId, sizeof(windowId), "##PersistentOverlay_%s", data.title.c_str());

    if (ImGui::Begin(windowId, nullptr, flags)) {
        if (hasHeader) {
            ImGui::PushStyleColor(ImGuiCol_Text, headerTextColor);
            ImGui::SetWindowFontScale(textScale * 1.1f);
            const std::string headerText = data.title + " (" + std::to_string(data.playerCount) + ")";
            RenderTruncatedOverlayText(headerText, headerTextColor, panelWidth - padding * 2.0f);
            ImGui::SetWindowFontScale(textScale);
            ImGui::PopStyleColor();
        }

        ImGui::SetWindowFontScale(textScale);

        if (hasCountRows) {
            const float countWidth = ImGui::CalcTextSize("999").x + 8.0f * overlayScale;
            const float availWidth = panelWidth - padding * 2.0f;
            const float nameWidth = (std::max)(40.0f * overlayScale, availWidth - countWidth);
            for (const auto& row : data.countRows) {
                RenderTruncatedOverlayText(row.label, rowTextColor, nameWidth - 4.0f * overlayScale);
                ImGui::SameLine(padding + nameWidth);
                ImGui::PushStyleColor(ImGuiCol_Text, rowTextColor);
                ImGui::Text("%d", row.count);
                ImGui::PopStyleColor();
            }
        } else {
            const float availWidth = panelWidth - padding * 2.0f;
            const float colW = availWidth / static_cast<float>(columns);
            const int totalNames = static_cast<int>(data.playerNames.size());

            for (int i = 0; i < totalNames; ++i) {
                const int col = i % columns;
                if (col > 0) {
                    ImGui::SameLine(padding + static_cast<float>(col) * colW);
                }
                RenderTruncatedOverlayText(data.playerNames[static_cast<std::size_t>(i)], rowTextColor, colW - 4.0f * overlayScale);
            }
        }

        if (persistentOverlayMoveMode_ && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            PersistMovedOverlayPosition(data, ImGui::GetWindowPos(), ImGui::GetWindowSize(), scale, stackedOffsetY);
        }

        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();

    if (pushedBorder) {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

} // namespace keybinder
