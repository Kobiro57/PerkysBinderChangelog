/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    RSS Tracker UI Implementation
*/
#include "../include/RssTrackerUI.h"
#include "../include/SettingsManager.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/RssHttpClient.h"
#include "../include/RssParser.h"

#include "../include/imgui-1.92.7/imgui.h"
#include <algorithm>
#include <cstring>

namespace keybinder {

namespace {

// DPI-aware value scaling (matches MainWindowUI pattern)
inline float Scale(float value) {
    return value * ThemeManager::Instance().GetAppliedScale();
}


} // anonymous namespace

// ---------------------------------------------------------------------------
// Data helpers
// ---------------------------------------------------------------------------

std::vector<RssFeedConfig>& RssTrackerUI::GetFeeds() {
    if (settingsManager_ != nullptr) {
        return settingsManager_->GetMutable().rss_feeds;
    }
    static std::vector<RssFeedConfig> empty;
    return empty;
}

const std::vector<RssFeedConfig>& RssTrackerUI::GetFeeds() const {
    if (settingsManager_ != nullptr) {
        return settingsManager_->Get().rss_feeds;
    }
    static const std::vector<RssFeedConfig> empty;
    return empty;
}

void RssTrackerUI::SyncFromSettings() {
    if (settingsManager_ != nullptr) {
        globalEnabled_ = settingsManager_->Get().rss_enabled;
        notificationMode_ = settingsManager_->Get().rss_notification_mode;
    }
}

void RssTrackerUI::PersistFeeds() {
    if (settingsManager_ != nullptr) {
        settingsManager_->GetMutable().rss_enabled = globalEnabled_;
        settingsManager_->GetMutable().rss_notification_mode = notificationMode_;
    }
    if (OnSettingsChanged) {
        OnSettingsChanged();
    }
}

void RssTrackerUI::OpenAddForm() {
    editingFeedIndex_ = -1;
    ResetForm();
}

void RssTrackerUI::OpenEditForm(int index) {
    const auto& feeds = GetFeeds();
    if (index < 0 || index >= static_cast<int>(feeds.size())) {
        return;
    }

    editingFeedIndex_ = index;
    const RssFeedConfig& feed = feeds[index];
    std::strncpy(urlBuf_, feed.url.c_str(), kUrlBufSize - 1);
    urlBuf_[kUrlBufSize - 1] = '\0';
    std::strncpy(labelBuf_, feed.label.c_str(), kLabelBufSize - 1);
    labelBuf_[kLabelBufSize - 1] = '\0';
    intervalMinutes_ = (std::max)(1, (std::min)(60, feed.pollIntervalSeconds / 60));
    formEnabled_ = feed.enabled;
}

void RssTrackerUI::ResetForm() {
    std::memset(urlBuf_, 0, kUrlBufSize);
    std::memset(labelBuf_, 0, kLabelBufSize);
    intervalMinutes_ = 5;
    formEnabled_ = true;
}

void RssTrackerUI::SaveFormToFeed(RssFeedConfig& feed) {
    feed.url = urlBuf_;
    feed.label = labelBuf_;
    feed.pollIntervalSeconds = intervalMinutes_ * 60;
    feed.enabled = formEnabled_;
}

std::string RssTrackerUI::TruncateUrl(const std::string& url, std::size_t maxChars) const {
    if (url.size() <= maxChars) {
        return url;
    }
    if (maxChars <= 1) {
        return "...";
    }
    return url.substr(0, maxChars - 3) + "...";
}

// ---------------------------------------------------------------------------
// Test hooks
// ---------------------------------------------------------------------------

bool RssTrackerUI::TestHook_AddFeed(const RssFeedConfig& feed) {
    auto& feeds = GetFeeds();
    // Reject duplicate URLs
    for (const auto& f : feeds) {
        if (f.url == feed.url) {
            return false;
        }
    }
    feeds.push_back(feed);
    PersistFeeds();
    return true;
}

bool RssTrackerUI::TestHook_RemoveFeed(int index) {
    auto& feeds = GetFeeds();
    if (index < 0 || index >= static_cast<int>(feeds.size())) {
        return false;
    }
    feeds.erase(feeds.begin() + index);
    if (selectedFeedIndex_ == index) {
        selectedFeedIndex_ = -1;
    } else if (selectedFeedIndex_ > index) {
        --selectedFeedIndex_;
    }
    PersistFeeds();
    return true;
}

bool RssTrackerUI::TestHook_ToggleFeed(int index) {
    auto& feeds = GetFeeds();
    if (index < 0 || index >= static_cast<int>(feeds.size())) {
        return false;
    }
    feeds[index].enabled = !feeds[index].enabled;
    PersistFeeds();
    return true;
}

int RssTrackerUI::TestHook_GetFeedCount() const {
    return static_cast<int>(GetFeeds().size());
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void RssTrackerUI::Render() {
    SyncFromSettings();
    RenderHeader();
    ImGui::Spacing();
    RenderFeedList();
    ImGui::Spacing();

    if (editingFeedIndex_ >= -1) {
        // -1 = add form open, >= 0 = edit form open; render when active
        if (editingFeedIndex_ != -2) {
            RenderAddEditForm();
            ImGui::Spacing();
        }
    }

    RenderDeleteConfirmation();
    RenderTestFeedResult();
}

void RssTrackerUI::RenderHeader() {
    UIComponents::SectionHeader("Forum RSS Tracker", "RSS-Feeds konfigurieren und neue Foren-Beiträge empfangen");

    // Global enable/disable toggle
    if (UIComponents::Toggle("RSS-Tracker aktivieren", &globalEnabled_)) {
        PersistFeeds();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("RSS-Tracking global ein-/ausschalten");
    }

    const char* notificationModeItems[] = { "Toast", "Chat" };
    int notificationModeIndex = notificationMode_ == RssNotificationMode::Chat ? 1 : 0;
    if (UIComponents::Combo("Benachrichtigung", &notificationModeIndex, notificationModeItems, IM_ARRAYSIZE(notificationModeItems))) {
        notificationMode_ = notificationModeIndex == 1 ? RssNotificationMode::Chat : RssNotificationMode::Toast;
        PersistFeeds();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Neue RSS-Posts als Overlay-Toast oder als lokale Chat-Nachricht anzeigen");
    }
}

void RssTrackerUI::RenderFeedList() {
    auto& theme = ThemeManager::Instance();
    const auto& feeds = GetFeeds();

    // "Add feed" button
    if (UIComponents::PrimaryButton("+ Feed hinzufügen")) {
        OpenAddForm();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Neuen RSS-Feed hinzufügen");
    }

    ImGui::Spacing();

    if (feeds.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextWrapped("Keine RSS-Feeds konfiguriert. Klicke auf '+ Feed hinzufügen', um einen Feed anzulegen.");
        ImGui::PopStyleColor();
        return;
    }

    for (int i = 0; i < static_cast<int>(feeds.size()); ++i) {
        const bool isSelected = (selectedFeedIndex_ == i);
        std::string cardId = "FeedRow_" + std::to_string(i);
        UIComponents::BeginCard(cardId.c_str(), 0.0f, isSelected);

        ImGui::PushID(i);
        RenderFeedRow(feeds[i], i);
        ImGui::PopID();

        UIComponents::EndCard();
    }
}

void RssTrackerUI::RenderFeedRow(const RssFeedConfig& feed, int index) {
    auto& theme = ThemeManager::Instance();
    const bool isSelected = (selectedFeedIndex_ == index);

    // --- Layout constants ---
    const float actionStripWidth = Scale(160.0f);
    const float innerSpacingX = theme.GetSpacing(ThemeSpacingRole::InnerSpacingX);
    const float itemSpacingX = theme.GetSpacing(ThemeSpacingRole::ItemSpacingX);
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float textLaneWidth = (std::max)(availableWidth - actionStripWidth - innerSpacingX, 0.0f);

    const float toggleHeight = theme.GetControlHeight(ThemeControlHeightRole::Toggle);
    const float toggleWidth = toggleHeight * 1.8f;
    const float btnSize = Scale(52.0f);
    const float deleteBtnSize = Scale(24.0f);

    // Row background on selection
    if (isSelected) {
        ImVec4 selBg = theme.GetColor(ThemeColorRole::Accent);
        selBg.w = 0.10f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2(rowWidth, 0.0f));
        const float rowHeight = ImGui::GetItemRectSize().y;
        ImGui::SetCursorScreenPos(rowMin);
    }

    // --- Line 1: Label (or URL fallback) with pixel-width truncation ---
    const std::string fullPrimaryText = feed.label.empty() ? feed.url : feed.label;
    const auto [displayPrimary, primaryTruncated] = UIComponents::TruncateTextToPixelWidth(fullPrimaryText.c_str(), textLaneWidth);
    ImGui::Text("%s", displayPrimary.c_str());
    if (primaryTruncated && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", fullPrimaryText.c_str());
    }

    // --- Action strip (right-aligned, same line as primary text) ---
    const float rightAnchorX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

    // Compact unlabeled toggle
    {
        bool feedEnabled = feed.enabled;
        const float toggleStartX = rightAnchorX - deleteBtnSize - itemSpacingX - btnSize - itemSpacingX - toggleWidth;
        ImGui::SameLine(toggleStartX);
        ImGui::PushID("toggle");
        if (UIComponents::Toggle("##feed_enabled", &feedEnabled)) {
            GetFeeds()[index].enabled = feedEnabled;
            PersistFeeds();
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", feed.enabled ? "Feed deaktivieren" : "Feed aktivieren");
        }
    }

    // Edit button
    {
        const float editStartX = rightAnchorX - deleteBtnSize - itemSpacingX - btnSize;
        ImGui::SameLine(editStartX);
        ImGui::PushID("edit");
        if (UIComponents::GhostButton("Bearb.", ImVec2(btnSize, toggleHeight))) {
            selectedFeedIndex_ = index;
            OpenEditForm(index);
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Feed bearbeiten");
        }
    }

    // Icon-only destructive delete button
    {
        const float deleteStartX = rightAnchorX - deleteBtnSize;
        ImGui::SameLine(deleteStartX);
        ImGui::PushID("delete");
        if (UIComponents::DangerButton("X", ImVec2(deleteBtnSize, toggleHeight))) {
            pendingDeleteIndex_ = index;
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Feed entfernen");
        }
    }

    // --- Line 2: URL + interval badge (only when label is set) ---
    if (!feed.label.empty()) {
        char intervalBuf[32];
        std::snprintf(intervalBuf, sizeof(intervalBuf), "[%d min]", (std::max)(1, feed.pollIntervalSeconds / 60));
        const ImVec2 intervalSize = ImGui::CalcTextSize(intervalBuf);
        const float urlMaxWidth = (std::max)(textLaneWidth - intervalSize.x - itemSpacingX, 0.0f);

        const auto [displayUrl, urlTruncated] = UIComponents::TruncateTextToPixelWidth(feed.url.c_str(), urlMaxWidth);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::Text("%s", displayUrl.c_str());
        ImGui::PopStyleColor();
        if (urlTruncated && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", feed.url.c_str());
        }

        // Interval badge inline with URL
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", intervalBuf);
        ImGui::PopStyleColor();
    }

    // Click row to edit (selectable via invisible button over the label area)
    ImGui::PushID("rowclick");
    if (ImGui::InvisibleButton("##row", ImVec2(-1.0f, 0.0f))) {
        selectedFeedIndex_ = index;
        OpenEditForm(index);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Klicken zum Bearbeiten");
    }
    ImGui::PopID();
}

void RssTrackerUI::RenderAddEditForm() {
    auto& theme = ThemeManager::Instance();
    const bool isEditing = (editingFeedIndex_ >= 0);
    const char* formTitle = isEditing ? "Feed bearbeiten" : "Neuen Feed hinzufügen";

    UIComponents::BeginCard("RssFeedForm", 0.0f, true);
    UIComponents::CardHeader(formTitle, "URL, Bezeichnung und Abrufintervall festlegen");

    // URL input
    std::string urlStr(urlBuf_);
    if (UIComponents::InputText("URL", &urlStr, "https://forum.example.com/feed")) {
        std::strncpy(urlBuf_, urlStr.c_str(), kUrlBufSize - 1);
        urlBuf_[kUrlBufSize - 1] = '\0';
    }

    // Label input
    std::string labelStr(labelBuf_);
    if (UIComponents::InputText("Bezeichnung", &labelStr, "Optionaler Name für diesen Feed")) {
        std::strncpy(labelBuf_, labelStr.c_str(), kLabelBufSize - 1);
        labelBuf_[kLabelBufSize - 1] = '\0';
    }

    // Interval slider (1-60 minutes)
    if (ImGui::SliderInt("Intervall (Minuten)", &intervalMinutes_, 1, 60, "%d min")) {
        intervalMinutes_ = (std::max)(1, (std::min)(60, intervalMinutes_));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Abrufintervall in Minuten (1-60)");
    }

    // Enabled toggle
    UIComponents::Toggle("Aktiv", &formEnabled_);

    UIComponents::Separator();

    // Action buttons
    const bool hasUrl = std::strlen(urlBuf_) > 0;

    if (UIComponents::PrimaryButton("Speichern", ImVec2(0.0f, 0.0f), !hasUrl)) {
        if (hasUrl) {
            if (isEditing) {
                auto& feeds = GetFeeds();
                if (editingFeedIndex_ < static_cast<int>(feeds.size())) {
                    SaveFormToFeed(feeds[editingFeedIndex_]);
                    PersistFeeds();
                }
            } else {
                RssFeedConfig newFeed;
                SaveFormToFeed(newFeed);
                GetFeeds().push_back(newFeed);
                PersistFeeds();
            }
            editingFeedIndex_ = -2; // close form
            ResetForm();
        }
    }

    ImGui::SameLine();

    if (UIComponents::SecondaryButton("Abbrechen")) {
        editingFeedIndex_ = -2; // close form
        ResetForm();
    }

    // Test Feed button (only if URL is non-empty)
    if (hasUrl) {
        ImGui::SameLine();
        if (UIComponents::GhostButton("Feed testen")) {
            testFeedPending_ = true;
            testFeedSucceeded_ = false;
            testFeedItemCount_ = 0;
            testFeedError_.clear();
            testFeedResultAge_ = 0.0f;

            // Synchronous HTTP fetch (explicit UI-thread exception per task spec)
            RssHttpClient client;
            std::string body;
            std::string outLastModified;
            std::string outEtag;
            unsigned long statusCode = 0;

            bool fetched = client.Fetch(urlBuf_, body, "", "", outLastModified, outEtag, statusCode);

            if (fetched && statusCode == 200 && !body.empty()) {
                RssParser parser;
                auto items = parser.Parse(body);
                testFeedSucceeded_ = true;
                testFeedItemCount_ = static_cast<int>(items.size());
            } else {
                testFeedSucceeded_ = false;
                char errBuf[64];
                std::snprintf(errBuf, sizeof(errBuf), "HTTP %lu", statusCode);
                testFeedError_ = fetched ? errBuf : "Verbindung fehlgeschlagen";
            }
            testFeedResultAge_ = 0.0f;
            testFeedPending_ = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Feed-URL einmalig abrufen und Ergebnis anzeigen");
        }
    }

    UIComponents::EndCard();
}

void RssTrackerUI::RenderDeleteConfirmation() {
    if (pendingDeleteIndex_ < 0) {
        return;
    }

    auto& theme = ThemeManager::Instance();
    const auto& feeds = GetFeeds();
    if (pendingDeleteIndex_ >= static_cast<int>(feeds.size())) {
        pendingDeleteIndex_ = -1;
        return;
    }

    const std::string displayName = feeds[pendingDeleteIndex_].label.empty()
        ? TruncateUrl(feeds[pendingDeleteIndex_].url, 40)
        : feeds[pendingDeleteIndex_].label;

    if (ImGui::BeginPopupModal("Feed entfernen?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Möchtest du den Feed wirklich entfernen?");
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Warning), "%s", displayName.c_str());
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Warning),
            "Diese Aktion kann nicht rückgängig gemacht werden!");
        UIComponents::Separator();

        if (UIComponents::DangerButton("Ja, entfernen", ImVec2(120, 0))) {
            TestHook_RemoveFeed(pendingDeleteIndex_);
            pendingDeleteIndex_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(120, 0))) {
            pendingDeleteIndex_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        // Auto-open the modal on first render
        ImGui::OpenPopup("Feed entfernen?");
    }
}

void RssTrackerUI::RenderTestFeedResult() {
    if (testFeedPending_) {
        ImGui::TextColored(ThemeManager::Instance().GetColor(ThemeColorRole::Info),
            "Feed wird abgerufen...");
        return;
    }

    if (testFeedError_.empty() && testFeedItemCount_ == 0 && !testFeedSucceeded_) {
        return; // no result to show
    }

    auto& theme = ThemeManager::Instance();

    // Auto-dismiss after 8 seconds
    testFeedResultAge_ += ImGui::GetIO().DeltaTime;
    if (testFeedResultAge_ > 8.0f) {
        testFeedSucceeded_ = false;
        testFeedItemCount_ = 0;
        testFeedError_.clear();
        return;
    }

    if (testFeedSucceeded_) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Success),
            "Feed erfolgreich abgerufen! %d Einträge gefunden.", testFeedItemCount_);
    } else {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::Danger),
            "Fehler beim Abruf: %s", testFeedError_.c_str());
    }
}

} // namespace keybinder
