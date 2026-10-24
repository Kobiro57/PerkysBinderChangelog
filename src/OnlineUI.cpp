/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Online UI - Online overview and overlay settings tabs
*/
#include "../include/OnlineUI.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/KeyInputUtils.h"
#include "../include/ThemeManager.h"
#include "../include/UIComponents.h"
#include "../include/SettingsManager.h"
#include "../include/KeybindManager.h"
#include "../include/OnlineOverlayController.h"
#include "../include/OnlineOverlayRenderer.h"
#include "../include/OnlineEventDetector.h"
#include "../include/Logger.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace keybinder {

static inline float GetLayoutScale() {
    return ThemeManager::Instance().GetAppliedScale();
}

static inline float Scale(float value) {
    return value * GetLayoutScale();
}

// Bucket cards switch to an internal scroll region beyond this many rows; the
// same cap bounds the height proxy used for balancing the card columns.
constexpr int kBucketVisibleRows = 12;

// ---------------------------------------------------------------------------
// UTF-8 literals for German text
// ---------------------------------------------------------------------------
// (inline escape sequences used directly in string literals)

// ---------------------------------------------------------------------------
// Free helper — must be inside namespace for ADL and type resolution
// ---------------------------------------------------------------------------

const OnlineBucketSummary* FindBucketByName(const std::vector<OnlineBucketSummary>& buckets, const std::string& name) {
    for (const auto& bucket : buckets) {
        if (bucket.bucketName == name) {
            return &bucket;
        }
    }
    return nullptr;
}

const OnlinePlayer* FindBucketPlayer(const OnlineSnapshot& snapshot,
                                     const std::string& playerName,
                                     const OnlineBucketSummary& bucket) {
    for (const auto& player : snapshot.players) {
        if (player.name != playerName || player.affiliation != bucket.bucketName) {
            continue;
        }

        if ((bucket.isFaction && player.isFaction) || (!bucket.isFaction && player.isGang)) {
            return &player;
        }
    }

    return nullptr;
}

ImVec4 WithAlpha(ImVec4 color, float alpha) {
    color.w = alpha;
    return color;
}

void RenderTruncatedText(const std::string& text, const ImVec4& color, float maxWidth) {
    const auto [displayText, wasTruncated] = UIComponents::TruncateTextToPixelWidth(text.c_str(), (std::max)(maxWidth, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(displayText.c_str());
    ImGui::PopStyleColor();

    if (wasTruncated && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text.c_str());
    }
}

void RenderTruncatedText(const std::string& text, const ImVec4& color) {
    RenderTruncatedText(text, color, ImGui::GetContentRegionAvail().x);
}

void ComputePlayerColumns(float width, float padX, float gap, bool showRank, float& nameWidth, float& levelWidth, float& rankWidth) {
    levelWidth = Scale(42.0f);
    rankWidth = showRank ? (std::max)(Scale(72.0f), (std::min)(Scale(120.0f), width * 0.30f)) : 0.0f;

    const float usedWidth = padX * 2.0f + levelWidth + (showRank ? rankWidth + gap * 2.0f : gap);
    nameWidth = (std::max)(width - usedWidth, Scale(40.0f));
}

void RestoreCursorAfterAbsoluteLayout(const ImVec2& cursorPos, float width) {
    ImGui::SetCursorScreenPos(cursorPos);
    ImGui::Dummy(ImVec2((std::max)(width, 0.0f), 0.0f));
}

void RenderOnlineBucketCardHeader(const char* displayName, int playerCount) {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float padX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float padY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.72f;
    const float radius = theme.GetRadius(ThemeRadiusRole::Frame);
    const float lineHeight = ImGui::GetTextLineHeight();
    const float height = lineHeight + padY * 2.0f;
    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::Dummy(ImVec2(width, height));
    const ImVec2 afterHeader = ImGui::GetCursorScreenPos();
    const ImVec2 end(start.x + width, start.y + height);

    drawList->AddRectFilled(start, end, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::SurfaceElevated), 0.50f)), radius);
    drawList->AddRect(start, end, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::BorderStrong), 0.18f)), radius, 0, 1.0f);
    drawList->AddRectFilled(
        start,
        ImVec2(start.x + Scale(3.0f), end.y),
        ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.78f)),
        radius,
        ImDrawFlags_RoundCornersLeft);

    const std::string countText = std::to_string(playerCount) + " online";
    const ImVec2 countTextSize = ImGui::CalcTextSize(countText.c_str());
    const float pillPadX = padX * 0.78f;
    const float pillPadY = padY * 0.38f;
    const float pillWidth = countTextSize.x + pillPadX * 2.0f;
    const float pillHeight = countTextSize.y + pillPadY * 2.0f;
    const ImVec2 pillMin(end.x - padX - pillWidth, start.y + (height - pillHeight) * 0.5f);
    const ImVec2 pillMax(pillMin.x + pillWidth, pillMin.y + pillHeight);

    drawList->AddRectFilled(pillMin, pillMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.14f)), pillHeight * 0.5f);
    drawList->AddRect(pillMin, pillMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.24f)), pillHeight * 0.5f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(start.x + padX + Scale(5.0f), start.y + padY));
    RenderTruncatedText(displayName != nullptr ? displayName : "", theme.GetColor(ThemeColorRole::TextPrimary), pillMin.x - start.x - padX * 2.0f - Scale(5.0f));

    ImGui::SetCursorScreenPos(ImVec2(pillMin.x + pillPadX, pillMin.y + pillPadY - Scale(1.0f)));
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Accent));
    ImGui::TextUnformatted(countText.c_str());
    ImGui::PopStyleColor();

    RestoreCursorAfterAbsoluteLayout(afterHeader, width);
}

void RenderOnlineTableHeader(bool showRank) {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float padX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float gap = theme.GetSpacing(ThemeSpacingRole::InnerSpacingX);
    const float textHeight = ImGui::GetTextLineHeight();
    const float height = textHeight + theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.75f;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    float nameWidth = 0.0f;
    float levelWidth = 0.0f;
    float rankWidth = 0.0f;
    ComputePlayerColumns(width, padX, gap, showRank, nameWidth, levelWidth, rankWidth);

    ImGui::Dummy(ImVec2(width, height));
    const ImVec2 afterHeader = ImGui::GetCursorScreenPos();
    const float textY = start.y + (height - textHeight) * 0.5f - Scale(1.0f);
    const ImVec4 muted = theme.GetColor(ThemeColorRole::TextMuted);

    ImGui::SetCursorScreenPos(ImVec2(start.x + padX, textY));
    RenderTruncatedText("Name", muted, nameWidth);
    ImGui::SetCursorScreenPos(ImVec2(start.x + padX + nameWidth + gap, textY));
    RenderTruncatedText("Lv.", muted, levelWidth);
    if (showRank) {
        ImGui::SetCursorScreenPos(ImVec2(start.x + padX + nameWidth + gap + levelWidth + gap, textY));
        RenderTruncatedText("Rang", muted, rankWidth);
    }

    drawList->AddLine(
        ImVec2(start.x + padX, start.y + height - Scale(1.0f)),
        ImVec2(start.x + width - padX, start.y + height - Scale(1.0f)),
        ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Border), 0.34f)),
        Scale(1.0f));

    RestoreCursorAfterAbsoluteLayout(afterHeader, width);
}

void RenderOnlinePlayerRow(const std::string& name,
                           const std::string& levelText,
                           const std::string& rankText,
                           bool showRank,
                           int rowIndex) {
    auto& theme = ThemeManager::Instance();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    const float padX = theme.GetSpacing(ThemeSpacingRole::FramePaddingX);
    const float padY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.54f;
    const float gap = theme.GetSpacing(ThemeSpacingRole::InnerSpacingX);
    const float textHeight = ImGui::GetTextLineHeight();
    const float rowHeight = (std::max)(Scale(29.0f), textHeight + padY * 2.0f);
    const float radius = theme.GetRadius(ThemeRadiusRole::Frame);
    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::Dummy(ImVec2(width, rowHeight));
    const ImVec2 afterRow = ImGui::GetCursorScreenPos();
    const ImVec2 end(start.x + width, start.y + rowHeight);

    ImVec4 rowFill = (rowIndex % 2 == 0)
        ? WithAlpha(theme.GetColor(ThemeColorRole::SurfaceElevated), 0.24f)
        : WithAlpha(theme.GetColor(ThemeColorRole::Surface), 0.16f);
    drawList->AddRectFilled(start, end, ImGui::GetColorU32(rowFill), radius);

    float nameWidth = 0.0f;
    float levelWidth = 0.0f;
    float rankWidth = 0.0f;
    ComputePlayerColumns(width, padX, gap, showRank, nameWidth, levelWidth, rankWidth);
    const float textY = start.y + (rowHeight - textHeight) * 0.5f - Scale(1.0f);

    ImGui::SetCursorScreenPos(ImVec2(start.x + padX, textY));
    RenderTruncatedText(name, theme.GetColor(ThemeColorRole::TextPrimary), nameWidth);

    const float levelX = start.x + padX + nameWidth + gap;
    const ImVec2 levelSize = ImGui::CalcTextSize(levelText.c_str());
    const float chipPadX = padX * 0.48f;
    const float chipPadY = padY * 0.28f;
    const float chipWidth = (std::min)(levelWidth, levelSize.x + chipPadX * 2.0f);
    const float chipHeight = levelSize.y + chipPadY * 2.0f;
    const ImVec2 chipMin(levelX + (levelWidth - chipWidth) * 0.5f, start.y + (rowHeight - chipHeight) * 0.5f);
    const ImVec2 chipMax(chipMin.x + chipWidth, chipMin.y + chipHeight);
    drawList->AddRectFilled(chipMin, chipMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.10f)), chipHeight * 0.45f);
    drawList->AddRect(chipMin, chipMax, ImGui::GetColorU32(WithAlpha(theme.GetColor(ThemeColorRole::Accent), 0.16f)), chipHeight * 0.45f, 0, 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(chipMin.x + (chipWidth - levelSize.x) * 0.5f, chipMin.y + chipPadY - Scale(1.0f)));
    ImGui::PushStyleColor(ImGuiCol_Text, levelText == "-" ? theme.GetColor(ThemeColorRole::TextMuted) : theme.GetColor(ThemeColorRole::Accent));
    ImGui::TextUnformatted(levelText.c_str());
    ImGui::PopStyleColor();

    if (showRank) {
        const float rankX = levelX + levelWidth + gap;
        const bool hasRank = !rankText.empty() && rankText != "-";
        ImGui::SetCursorScreenPos(ImVec2(rankX, textY));
        RenderTruncatedText(hasRank ? rankText : "-", hasRank
            ? theme.GetColor(ThemeColorRole::TextSecondary)
            : theme.GetColor(ThemeColorRole::TextMuted), rankWidth);
    }

    RestoreCursorAfterAbsoluteLayout(afterRow, width);
}

void RenderOnlineBucketCard(const char* id,
                            const OnlineSnapshot& snapshot,
                            const OnlineBucketSummary* bucket,
                            const char* displayName,
                            float width,
                            bool showRank,
                            const char* emptyMessage) {
    auto& theme = ThemeManager::Instance();
    const int playerCount = bucket != nullptr ? bucket->playerCount : 0;

    UIComponents::BeginCard(id, width, true);
    RenderOnlineBucketCardHeader(displayName, playerCount);
    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.45f));

    if (bucket == nullptr || bucket->playerCount <= 0 || bucket->playerNames.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("%s", emptyMessage);
        ImGui::PopStyleColor();
        UIComponents::EndCard();
        return;
    }

    RenderOnlineTableHeader(showRank);
    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.25f));

    const int totalRows = static_cast<int>(bucket->playerNames.size());
    const bool useScrollRegion = totalRows > kBucketVisibleRows;

    if (useScrollRegion) {
        const float padY = theme.GetSpacing(ThemeSpacingRole::FramePaddingY) * 0.54f;
        const float rowHeight = (std::max)(Scale(29.0f), ImGui::GetTextLineHeight() + padY * 2.0f);
        const float interGap = theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.22f
                             + ImGui::GetStyle().ItemSpacing.y * 2.0f;
        const float listHeight = static_cast<float>(kBucketVisibleRows) * rowHeight
                               + static_cast<float>(kBucketVisibleRows - 1) * interGap
                               + Scale(2.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::Text("%d Spieler \xE2\x80\x94 scrollen", totalRows);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.2f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("##BucketRowsScroll", ImVec2(0.0f, listHeight),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
    }

    int rowIndex = 0;
    for (const auto& name : bucket->playerNames) {
        const OnlinePlayer* player = FindBucketPlayer(snapshot, name, *bucket);
        const std::string levelText = player != nullptr ? std::to_string(player->level) : "-";
        const std::string rankText = player != nullptr && player->hasTeamRank && !player->teamRank.empty()
            ? player->teamRank
            : "-";

        RenderOnlinePlayerRow(name, levelText, rankText, showRank, rowIndex++);
        if (rowIndex < static_cast<int>(bucket->playerNames.size())) {
            ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::ItemSpacingY) * 0.22f));
        }
    }

    if (useScrollRegion) {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    UIComponents::EndCard();
}

// ---------------------------------------------------------------------------
// Balanced-column (masonry) layout for bucket cards
// ---------------------------------------------------------------------------
// A row-major grid advances each new row past the *tallest* card in the
// previous row, so a faction/gang with many members online leaves a large
// empty gap beneath every shorter card beside it. Packing the auto-sized
// cards into balanced vertical columns keeps them flush against each other and
// removes the gaps, regardless of how lopsided the member counts are.

struct OnlineBucketCard {
    const OnlineBucketSummary* bucket;  // may be null (e.g. faction with nobody online)
    const char* displayName;
    int weight;
};

// Approximate a card's height in row-units. Exact pixels are unnecessary —
// only the relative magnitude drives how cards are distributed across columns.
int ComputeBucketCardWeight(const OnlineBucketSummary* bucket) {
    constexpr int kCardChromeUnits = 2;  // card header + surrounding spacing
    if (bucket == nullptr || bucket->playerCount <= 0 || bucket->playerNames.empty()) {
        return kCardChromeUnits + 1;  // single empty-state line, no table header
    }
    const int rows = static_cast<int>(bucket->playerNames.size());
    // Cards beyond the cap collapse into a fixed-height scroll region, so their
    // height plateaus (+1 for the "N Spieler" status line above the scroll).
    const int visibleRows = rows > kBucketVisibleRows ? (kBucketVisibleRows + 1) : rows;
    return kCardChromeUnits + 1 /* table header */ + visibleRows;
}

void RenderOnlineBucketColumns(const char* cardId,
                               const OnlineSnapshot& snapshot,
                               const std::vector<OnlineBucketCard>& cards,
                               float availableWidth,
                               float minCardWidth,
                               int maxColumns,
                               bool showRank,
                               const char* emptyMessage) {
    if (cards.empty()) {
        return;
    }

    const float cardGap = ThemeManager::Instance().GetSpacing(ThemeSpacingRole::CardGap);

    int columnCount = 1;
    if (availableWidth > minCardWidth) {
        columnCount = static_cast<int>((availableWidth + cardGap) / (minCardWidth + cardGap));
        columnCount = (std::max)(1, (std::min)(columnCount, maxColumns));
    }
    columnCount = (std::min)(columnCount, static_cast<int>(cards.size()));

    const float cardWidth = columnCount > 1
        ? (availableWidth - cardGap * static_cast<float>(columnCount - 1)) / static_cast<float>(columnCount)
        : 0.0f;  // single column fills the available width

    // Distribute cards across the columns so their heights stay balanced (see
    // OnlineUI::BalanceCardColumns). Rendering each column as a vertical stack
    // of auto-sized cards is what removes the gaps a row-major grid leaves
    // beneath shorter cards.
    std::vector<int> weights;
    weights.reserve(cards.size());
    for (const OnlineBucketCard& card : cards) {
        weights.push_back(card.weight);
    }
    const std::vector<std::vector<int>> columnCards = OnlineUI::BalanceCardColumns(weights, columnCount);

    for (int c = 0; c < columnCount; ++c) {
        if (c > 0) {
            ImGui::SameLine(0.0f, cardGap);
        }
        ImGui::BeginGroup();
        const std::vector<int>& colCards = columnCards[static_cast<std::size_t>(c)];
        for (int j = 0; j < static_cast<int>(colCards.size()); ++j) {
            if (j > 0) {
                ImGui::Dummy(ImVec2(0.0f, cardGap));
            }
            const int cardIndex = colCards[static_cast<std::size_t>(j)];
            const OnlineBucketCard& card = cards[static_cast<std::size_t>(cardIndex)];
            ImGui::PushID(cardIndex);
            RenderOnlineBucketCard(cardId, snapshot, card.bucket, card.displayName, cardWidth, showRank, emptyMessage);
            ImGui::PopID();
        }
        ImGui::EndGroup();
    }
}

// Greedy shortest-column packing. Empty columns are filled left-to-right
// first (equal weights keep the lowest index), so the top row preserves the
// source order while the remaining cards even out the column heights.
std::vector<std::vector<int>> OnlineUI::BalanceCardColumns(const std::vector<int>& cardWeights, int columnCount) {
    if (columnCount < 1) {
        columnCount = 1;
    }

    std::vector<std::vector<int>> columns(static_cast<std::size_t>(columnCount));
    std::vector<long long> columnWeight(static_cast<std::size_t>(columnCount), 0);

    for (int i = 0; i < static_cast<int>(cardWeights.size()); ++i) {
        int target = 0;
        for (int c = 1; c < columnCount; ++c) {
            if (columnWeight[static_cast<std::size_t>(c)] < columnWeight[static_cast<std::size_t>(target)]) {
                target = c;
            }
        }
        columns[static_cast<std::size_t>(target)].push_back(i);
        columnWeight[static_cast<std::size_t>(target)] += cardWeights[static_cast<std::size_t>(i)];
    }

    return columns;
}

OnlineUI::OnlineUI() = default;

void OnlineUI::Render() {
    UIComponents::BeginSubTabs("OnlineTabs");
    if (UIComponents::SubTabItem("\xC3\x9C""bersicht", static_cast<int>(currentSubtab_) == 0)) {
        currentSubtab_ = OnlineSubtab::Overview;
    }
    if (UIComponents::SubTabItem("Overlay-Einstellungen", static_cast<int>(currentSubtab_) == 1)) {
        currentSubtab_ = OnlineSubtab::OverlaySettings;
    }
    UIComponents::EndSubTabs();

    if (currentSubtab_ == OnlineSubtab::Overview) {
        RenderOverviewTab();
    } else {
        RenderOverlaySettingsTab();
    }
}

// ---------------------------------------------------------------------------
// Status header
// ---------------------------------------------------------------------------

void OnlineUI::RenderStatusHeader(const OnlineSnapshot& snapshot) {
    auto& theme = ThemeManager::Instance();

    const char* statusText = "Offline";
    const ImVec4* statusColor = &theme.GetColor(ThemeColorRole::TextMuted);

    switch (snapshot.fetchStatus) {
        case OnlineFetchStatus::Live:
            statusText = "Live";
            statusColor = &theme.GetColor(ThemeColorRole::Success);
            break;
        case OnlineFetchStatus::Stale:
            statusText = "Veraltet";
            statusColor = &theme.GetColor(ThemeColorRole::Warning);
            break;
        case OnlineFetchStatus::Offline:
            statusText = "Offline";
            statusColor = &theme.GetColor(ThemeColorRole::TextMuted);
            break;
    }

    UIComponents::BeginCard("OnlineStatusHeader", 0.0f, true);

    // Status indicator with colored dot and player count
    ImGui::PushStyleColor(ImGuiCol_Text, *statusColor);
    ImGui::TextUnformatted(statusText);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
    ImGui::TextUnformatted(" \xE2\x80\xA2 ");  // bullet separator
    ImGui::PopStyleColor();

    ImGui::SameLine();
    char countBuf[64];
    std::snprintf(countBuf, sizeof(countBuf), "%d Spieler online", snapshot.totalPlayers);
    ImGui::TextUnformatted(countBuf);

    // Timestamp
    if (snapshot.updatedAtMs > 0) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        char timeBuf[32];
        std::snprintf(timeBuf, sizeof(timeBuf), "Aktualisiert: %ds", snapshot.updatedAtMs / 1000);
        ImGui::TextUnformatted(timeBuf);
        ImGui::PopStyleColor();
    }

    UIComponents::EndCard();
}

// ---------------------------------------------------------------------------
// Faction section using the same responsive table-card layout as gangs
// ---------------------------------------------------------------------------

void OnlineUI::RenderFactionSection(const OnlineSnapshot& snapshot) {
    UIComponents::SectionHeader("Fraktionen", "LSPD, Army, Fire Department und Red County Brotherhood");

    constexpr const char* kFactionNames[] = {"LSPD", "Army", "Fire Department", "Red County Brotherhood"};
    std::vector<OnlineBucketCard> cards;
    cards.reserve(IM_ARRAYSIZE(kFactionNames));
    for (const char* factionName : kFactionNames) {
        const OnlineBucketSummary* bucket = FindBucketByName(snapshot.factionBuckets, factionName);
        cards.push_back(OnlineBucketCard{bucket, factionName, ComputeBucketCardWeight(bucket)});
    }

    RenderOnlineBucketColumns("FactionTableCard", snapshot, cards,
                              ImGui::GetContentRegionAvail().x, Scale(270.0f), 3, true,
                              "Keine Spieler online");
}

// ---------------------------------------------------------------------------
// Gangs section with responsive spreadsheet-style cards
// ---------------------------------------------------------------------------

void OnlineUI::RenderGangsSection(const OnlineSnapshot& snapshot) {
    auto& theme = ThemeManager::Instance();

    UIComponents::SectionHeader("Gangs", "Alle Gruppierungen und Zivilisten auf einen Blick");

    if (snapshot.gangBuckets.empty()) {
        UIComponents::BeginCard("GangsEmpty", 0.0f, false);
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Keine Gangs oder Zivilisten online");
        ImGui::PopStyleColor();
        UIComponents::EndCard();
        return;
    }

    std::vector<OnlineBucketCard> cards;
    cards.reserve(snapshot.gangBuckets.size());
    for (const auto& bucket : snapshot.gangBuckets) {
        cards.push_back(OnlineBucketCard{&bucket, bucket.bucketName.c_str(), ComputeBucketCardWeight(&bucket)});
    }

    RenderOnlineBucketColumns("GangExcelCard", snapshot, cards,
                              ImGui::GetContentRegionAvail().x, Scale(220.0f), 4, false,
                              "Keine Spieler in dieser Gruppierung");
}

// ---------------------------------------------------------------------------
// Main overview tab
// ---------------------------------------------------------------------------

void OnlineUI::RenderOverviewTab() {
    auto& theme = ThemeManager::Instance();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::InnerSpacingY)));

    if (!onlineDataManager_) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::TextMuted),
                           "Online-Dienst nicht verf\xC3\xBC""gbar");
        return;
    }

    // Get current snapshot
    OnlineSnapshot snapshot = onlineDataManager_->GetSnapshot();

    // 1. Status header
    RenderStatusHeader(snapshot);

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.5f));

    // 2. Faction section
    RenderFactionSection(snapshot);

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.5f));

    // 3. Gangs section
    RenderGangsSection(snapshot);
}


// ---------------------------------------------------------------------------
// Settings persistence & live-update helpers
// ---------------------------------------------------------------------------

void OnlineUI::PersistSettings() {
    if (keybindManager_ != nullptr) {
        keybindManager_->Save(settingsManager_);
    }
}

void OnlineUI::ApplyLiveSettings() {
    if (onlineDataManager_ != nullptr && settingsManager_ != nullptr) {
        Settings& settings = settingsManager_->GetMutable();
        onlineDataManager_->UpdateSettings(settings.online_enabled, localPlayerName_);
    }
    // Controller picks up settings changes via RefreshSettings() inside Update()
    // on the next frame — no explicit refresh call needed here.
    if (eventDetector_ != nullptr) {
        eventDetector_->ReloadSettings();
    }
}

void OnlineUI::RefreshCachedGangList() {
    if (onlineDataManager_ != nullptr) {
        OnlineSnapshot snapshot = onlineDataManager_->GetSnapshot();
        cachedGangList_.clear();
        for (const auto& bucket : snapshot.gangBuckets) {
            if (bucket.bucketName != "Zivilisten") {
                cachedGangList_.push_back(bucket.bucketName);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Hotkey capture (supports unset / vk=0)
// ---------------------------------------------------------------------------

void OnlineUI::StartHotkeyCapture() {
    hotkeyCaptureActive_ = true;
    hotkeyCaptureFrame_ = ImGui::GetFrameCount();
    CollectPressedVirtualKeys(hotkeyIgnoreKeys_);
}

void OnlineUI::RenderHotkeyCaptureUI() {
    if (!settingsManager_) return;
    Settings& settings = settingsManager_->GetMutable();

    if (hotkeyCaptureActive_) {
        UIComponents::SecondaryButton("Taste dr\xc3\xbc""cken... (ESC = Abbruch, DEL = Zur\xc3\xbc""cksetzen)",
            ImVec2(Scale(360.0f), 0.0f), true);

        if (ImGui::GetFrameCount() > hotkeyCaptureFrame_ + 1) {
            const int vk = FindPressedVirtualKey(hotkeyIgnoreKeys_);
            if (vk == VK_ESCAPE) {
                hotkeyCaptureActive_ = false;
            } else if (vk == VK_DELETE || vk == VK_BACK) {
                settings.online_quick_overlay_hotkey = 0;
                hotkeyCaptureActive_ = false;
                PersistSettings();
                ApplyLiveSettings();
            } else if (vk != 0) {
                settings.online_quick_overlay_hotkey = vk;
                hotkeyCaptureActive_ = false;
                PersistSettings();
                ApplyLiveSettings();
            }
        }
        return;
    }

    const std::string hotkeyLabel = GetHotkeyLabel(settings.online_quick_overlay_hotkey);
    if (UIComponents::SecondaryButton(hotkeyLabel.c_str(), ImVec2(Scale(220.0f), 0.0f))) {
        StartHotkeyCapture();
    }
}

std::string OnlineUI::GetHotkeyLabel(int vk) const {
    return GetLocalizedVirtualKeyName(vk);
}

// ---------------------------------------------------------------------------
// Gang selection UI (radio: All / Specific + compact grid)
// ---------------------------------------------------------------------------

void OnlineUI::RenderFactionFilterGrid() {
    if (!settingsManager_) return;
    Settings& settings = settingsManager_->GetMutable();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float columnGap = Scale(8.0f);
    const float columnWidth = (availableWidth - columnGap * 4.0f) * 0.2f;

    // Faction filters and civilians share the same compact row.
    ImGui::BeginGroup();
    ImGui::PushItemWidth(columnWidth);
    if (UIComponents::Toggle("LSPD", &settings.online_quick_overlay_show_lspd)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("LSPD-Spieler im Quick-Overlay anzeigen."); }
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, columnGap);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(columnWidth);
    if (UIComponents::Toggle("RCB", &settings.online_quick_overlay_show_rcb)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Red County Brotherhood-Spieler im Quick-Overlay anzeigen."); }
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, columnGap);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(columnWidth);
    if (UIComponents::Toggle("Army", &settings.online_quick_overlay_show_army)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Army-Spieler im Quick-Overlay anzeigen."); }
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, columnGap);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(columnWidth);
    if (UIComponents::Toggle("Fire Dept.", &settings.online_quick_overlay_show_fire_department)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Fire Department-Spieler im Quick-Overlay anzeigen."); }
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, columnGap);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(columnWidth);
    if (UIComponents::Toggle("Zivilisten", &settings.online_quick_overlay_show_zivilisten)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Zivilisten im Quick-Overlay anzeigen."); }
    ImGui::PopItemWidth();
    ImGui::EndGroup();
}

void OnlineUI::RenderGangSelectionGrid() {
    if (!settingsManager_) return;
    Settings& settings = settingsManager_->GetMutable();

    RefreshCachedGangList();

    const bool showAll = settings.online_quick_overlay_show_all_gangs;

    bool allSelected = showAll;
    if (ImGui::RadioButton("Alle Gangs", allSelected)) {
        settings.online_quick_overlay_show_all_gangs = true;
        PersistSettings();
        ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zeigt alle Gangs im Quick-Overlay an.");
    }

    ImGui::SameLine();

    bool specificSelected = !showAll;
    if (ImGui::RadioButton("Bestimmte Gangs", specificSelected)) {
        settings.online_quick_overlay_show_all_gangs = false;
        PersistSettings();
        ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("W\xc3\xa4""hle einzelne Gangs f\xc3\xbc""r das Quick-Overlay aus.");
    }

    if (!settings.online_quick_overlay_show_all_gangs) {
        if (cachedGangList_.empty()) {
            auto& theme = ThemeManager::Instance();
            ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
            ImGui::TextWrapped("Keine Gangs verf\xc3\xbc""gbar (Daten werden beim n\xc3\xa4""chsten Update geladen)");
            ImGui::PopStyleColor();
        } else {
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float columnGap = Scale(12.0f);
            const float columnWidth = (availableWidth - columnGap) * 0.5f;
            int col = 0;
            for (const auto& gangName : cachedGangList_) {
                if (col > 0) {
                    ImGui::SameLine(0.0f, columnGap);
                }
                bool isSelected = false;
                for (const auto& sel : settings.online_quick_overlay_selected_gangs) {
                    if (sel == gangName) { isSelected = true; break; }
                }

                ImGui::PushItemWidth(columnWidth);
                if (UIComponents::Toggle(gangName.c_str(), &isSelected)) {
                    if (isSelected) {
                        bool found = false;
                        for (const auto& sel : settings.online_quick_overlay_selected_gangs) {
                            if (sel == gangName) { found = true; break; }
                        }
                        if (!found) {
                            settings.online_quick_overlay_selected_gangs.push_back(gangName);
                        }
                    } else {
                        settings.online_quick_overlay_selected_gangs.erase(
                            std::remove(settings.online_quick_overlay_selected_gangs.begin(),
                                        settings.online_quick_overlay_selected_gangs.end(),
                                        gangName),
                            settings.online_quick_overlay_selected_gangs.end());
                    }
                    PersistSettings();
                    ApplyLiveSettings();
                }
                ImGui::PopItemWidth();
                col = (col + 1) % 2;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Quick overlay settings card
// ---------------------------------------------------------------------------

void OnlineUI::RenderQuickOverlaySettings() {
    if (!settingsManager_) return;
    Settings& settings = settingsManager_->GetMutable();

    UIComponents::BeginCard("QuickOverlayCard", 0.0f, true);
    UIComponents::CardHeader("Quick Overlay", "Schnelleinblendung der Online-Spieler");

    if (UIComponents::Toggle("Quick Overlay aktivieren", &settings.online_quick_overlay_enabled)) {
        PersistSettings(); ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Aktiviert oder deaktiviert das Quick-Overlay global.");
    }

    UIComponents::Separator();

    UIComponents::InfoRow("Hotkey", GetHotkeyLabel(settings.online_quick_overlay_hotkey).c_str());
    RenderHotkeyCaptureUI();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Klicke auf den Button und dr\xc3\xbc""cke dann die gew\xc3\xbc""nschte Taste. ESC bricht ab, DEL setzt zur\xc3\xbc""ck.");
    }

    const char* modeItems[] = { "Umschalten", "Halten" };
    int modeIndex = settings.online_quick_overlay_mode == "hold" ? 1 : 0;
    if (UIComponents::Combo("Modus", &modeIndex, modeItems, IM_ARRAYSIZE(modeItems))) {
        settings.online_quick_overlay_mode = (modeIndex == 1) ? "hold" : "toggle";
        PersistSettings(); ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Umschalten = einmal dr\xc3\xbc""cken zum Ein-/Ausblenden. Halten = nur anzeigen, solange die Taste gedr\xc3\xbc""ckt bleibt.");
    }

    UIComponents::Separator();

    UIComponents::SectionHeader("Fraktionsfilter", "Welche Fraktionen im Overlay angezeigt werden");
    RenderFactionFilterGrid();

    UIComponents::Separator();

    UIComponents::SectionHeader("Gang-Filter", "Welche Gangs im Overlay angezeigt werden");
    RenderGangSelectionGrid();

    UIComponents::EndCard();
}

// ---------------------------------------------------------------------------
// Persistent overlay settings card
// ---------------------------------------------------------------------------

void OnlineUI::RenderPersistentOverlayStyleControls(
    std::string& style,
    float& opacity,
    bool& backgroundEnabled,
    float& overlayScale,
    bool& customTextColorEnabled,
    float& textColorR,
    float& textColorG,
    float& textColorB,
    float& textColorA) {
    const char* styleItems[] = { "Mit Rahmen", "Minimal", "Kompakt" };
    const char* styleValues[] = { "card", "minimal", "dense" };
    int styleIndex = 0;
    for (int i = 0; i < 3; ++i) {
        if (style == styleValues[i]) { styleIndex = i; break; }
    }
    if (UIComponents::Combo("Stil", &styleIndex, styleItems, IM_ARRAYSIZE(styleItems))) {
        style = styleValues[styleIndex]; PersistSettings(); ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Darstellungsstil: Mit Rahmen = Hintergrund mit Kopfzeile, Minimal = transparent, Kompakt = schmal."); }

    if (UIComponents::Toggle("Hintergrund anzeigen", &backgroundEnabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Deaktivieren, um den Overlay-Hintergrund komplett zu entfernen."); }
    if (!backgroundEnabled) {
        auto& theme = ThemeManager::Instance();
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Ohne Hintergrund bleiben nur Text und Zahlen sichtbar.");
        ImGui::PopStyleColor();
    }

    if (UIComponents::SliderFloat("Gr\xC3\xB6\xC3\x9F" "e", &overlayScale, 0.75f, 1.50f, "%.2fx")) {
        PersistSettings(); ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Skaliert Text, Abst\xC3\xA4" "nde und Overlay-Gr\xC3\xB6\xC3\x9F" "e."); }

    if (UIComponents::Toggle("Eigene Textfarbe", &customTextColorEnabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Aktiviert eine eigene Farbe f\xC3\xBC" "r Titel, Namen und Zahlen."); }
    if (customTextColorEnabled) {
        ImVec4 textColor(textColorR, textColorG, textColorB, textColorA);
        ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_DisplayRGB;
        if (ImGui::ColorEdit4("Textfarbe", &textColor.x, colorFlags)) {
            textColorR = textColor.x;
            textColorG = textColor.y;
            textColorB = textColor.z;
            textColorA = textColor.w;
            PersistSettings(); ApplyLiveSettings();
        }
    }

    if (UIComponents::SliderFloat("Deckkraft", &opacity, 0.35f, 1.0f, "%.2f")) {
        PersistSettings(); ApplyLiveSettings();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Transparenz f\xC3\xBC" "r Text und Hintergrund. 0.35 = sehr transparent, 1.0 = vollst\xC3\xA4" "ndig deckend."); }
}

void OnlineUI::RenderPersistentOverlaySettings() {
    if (!settingsManager_) return;
    Settings& settings = settingsManager_->GetMutable();
    auto& theme = ThemeManager::Instance();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::InnerSpacingY)));

    // --- Toggles + Move mode ---
    UIComponents::BeginCard("PersistentTogglesCard", 0.0f, true);
    UIComponents::CardHeader("Persistente Overlays", "Dauerhafte Anzeigen und Move-Modus");

    if (overlayRenderer_ != nullptr) {
        const bool moveModeActive = overlayRenderer_->IsPersistentOverlayMoveModeActive();
        if (UIComponents::SecondaryButton(moveModeActive ? "Move-Modus beenden" : "Move-Modus starten", ImVec2(Scale(220.0f), 0.0f))) {
            overlayRenderer_->SetPersistentOverlayMoveMode(!moveModeActive);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Blendet dieses Men\xC3\xBC" " aus, zeigt den Mauszeiger und erlaubt das Ziehen der persistenten Overlays. ESC beendet den Move-Modus.");
        }
    }

    UIComponents::Separator();

    if (UIComponents::Toggle("Eigene Fraktion anzeigen", &settings.online_local_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Zeigt deine Fraktion dauerhaft an, wenn du in einer Fraktion bist."); }

    if (UIComponents::Toggle("Gang-\xC3\x9C" "bersicht anzeigen", &settings.online_gangs_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Zeigt alle Gangs mit der Anzahl online befindlicher Spieler an."); }

    if (UIComponents::Toggle("Eigene Gang anzeigen", &settings.online_own_gang_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Zeigt deine eigene Gang als separates Overlay an, wenn du in einer Gang bist."); }

    if (UIComponents::Toggle("Wanted-Liste anzeigen", &settings.online_wanted_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Zeigt Spieler mit Wanted-Level in einem konfigurierbaren Bereich an."); }

    if (settings.online_wanted_overlay_enabled) {
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const float itemWidth = (availWidth - theme.GetSpacing(ThemeSpacingRole::InnerSpacingX)) * 0.5f;

        int minWanted = settings.online_wanted_overlay_min;
        int maxWanted = settings.online_wanted_overlay_max;

        ImGui::BeginGroup();
        ImGui::PushID("wanted_min");
        if (UIComponents::InputInt("Min. Wanted", &minWanted, nullptr, itemWidth)) {
            settings.online_wanted_overlay_min = minWanted < 0 ? 0 : minWanted;
            PersistSettings(); ApplyLiveSettings();
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Untere Grenze (inklusive). Nur Spieler mit mindestens diesem Wanted-Level werden angezeigt."); }
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, theme.GetSpacing(ThemeSpacingRole::InnerSpacingX));

        ImGui::BeginGroup();
        ImGui::PushID("wanted_max");
        if (UIComponents::InputInt("Max. Wanted", &maxWanted, nullptr, itemWidth)) {
            settings.online_wanted_overlay_max = maxWanted < 0 ? 0 : maxWanted;
            PersistSettings(); ApplyLiveSettings();
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Obere Grenze (inklusive). 0 bedeutet keine obere Grenze."); }
        ImGui::EndGroup();
    }

    UIComponents::Separator();
    UIComponents::SectionHeader("Spielinformationen", "Live-Anzeigen aus dem Spiel");
    if (UIComponents::Toggle("Ort anzeigen", &settings.online_location_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (UIComponents::Toggle("Fortbewegungsmittel anzeigen", &settings.online_transport_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (UIComponents::Toggle("Pedslots anzeigen", &settings.online_pedslots_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Anzahl anderer Spieler, die aktuell in deinem Stream geladen sind."); }
    if (UIComponents::Toggle("Freunde anzeigen", &settings.online_friends_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (UIComponents::Toggle("Feinde anzeigen", &settings.online_enemies_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }

    ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
    ImGui::TextWrapped("Darstellung und Position der eingeblendeten Informationen lassen sich unten anpassen.");
    ImGui::PopStyleColor();

    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.5f));

    // --- Shared style editor with target selector ---
    UIComponents::BeginCard("OverlayAppearanceCard", 0.0f, true);
    UIComponents::CardHeader("Darstellung", "Stil-Einstellungen f\xc3\xbc""r persistente Overlays");

    const char* targetItems[] = { "Allgemeine Overlays", "Eigene Gang", "Wanted-Liste" };
    int targetIndex = static_cast<int>(currentStyleTarget_);
    if (UIComponents::Combo("Darstellung anpassen f\xC3\xBC" "r", &targetIndex, targetItems, IM_ARRAYSIZE(targetItems))) {
        currentStyleTarget_ = static_cast<OnlinePersistentStyleTarget>(targetIndex);
    }

    UIComponents::Separator();

    if (currentStyleTarget_ == OnlinePersistentStyleTarget::GeneralOverlays) {
        ImGui::TextWrapped("Gemeinsamer Stil f\xC3\xBC" "r eigene Fraktion, Gang-\xC3\x9C" "bersicht, Ort, Fortbewegungsmittel, Pedslots, Freunde-Radar und Feinde-Radar.");
        RenderPersistentOverlayStyleControls(
            settings.online_overlay_style,
            settings.online_overlay_opacity,
            settings.online_overlay_background_enabled,
            settings.online_overlay_scale,
            settings.online_overlay_custom_text_color_enabled,
            settings.online_overlay_text_color_r,
            settings.online_overlay_text_color_g,
            settings.online_overlay_text_color_b,
            settings.online_overlay_text_color_a);
    } else if (currentStyleTarget_ == OnlinePersistentStyleTarget::OwnGang) {
        RenderPersistentOverlayStyleControls(
            settings.online_own_gang_overlay_style,
            settings.online_own_gang_overlay_opacity,
            settings.online_own_gang_overlay_background_enabled,
            settings.online_own_gang_overlay_scale,
            settings.online_own_gang_overlay_custom_text_color_enabled,
            settings.online_own_gang_overlay_text_color_r,
            settings.online_own_gang_overlay_text_color_g,
            settings.online_own_gang_overlay_text_color_b,
            settings.online_own_gang_overlay_text_color_a);
    } else {
        RenderPersistentOverlayStyleControls(
            settings.online_wanted_overlay_style,
            settings.online_wanted_overlay_opacity,
            settings.online_wanted_overlay_background_enabled,
            settings.online_wanted_overlay_scale,
            settings.online_wanted_overlay_custom_text_color_enabled,
            settings.online_wanted_overlay_text_color_r,
            settings.online_wanted_overlay_text_color_g,
            settings.online_wanted_overlay_text_color_b,
            settings.online_wanted_overlay_text_color_a);
    }

    UIComponents::EndCard();
}

void OnlineUI::RenderPlayerMarkerSettings() {
    if (settingsManager_ == nullptr) return;
    Settings& settings = settingsManager_->GetMutable();
    auto& theme = ThemeManager::Instance();
    UIComponents::BeginCard("PlayerRadarSettings", 0.0f, true);
    UIComponents::CardHeader("Freunde & Feinde", "Gespeicherte Freunde und Feinde im Online-\xC3\x9C" "berblick");
    if (UIComponents::Toggle("Freunde anzeigen", &settings.online_friends_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    if (UIComponents::Toggle("Feinde anzeigen", &settings.online_enemies_overlay_enabled)) { PersistSettings(); ApplyLiveSettings(); }
    ImGui::TextColored(theme.GetColor(ThemeColorRole::TextMuted), "Bis zu 20 gespeicherte Spieler mit Online-Status.");
    ImGui::TextWrapped("Fraktion und Gang werden automatisch ermittelt. Freunde-Radar und Feinde-Radar lassen sich im Move-Modus getrennt verschieben.");
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.5f));
    UIComponents::BeginCard("PlayerRadarEditor", 0.0f, true);
    UIComponents::CardHeader("Spieler hinzuf\xC3\xBC" "gen", "Name eingeben oder aktuelle Spieler-ID verwenden");
    UIComponents::InputText("Name", &markerName_, "Spielername");
    UIComponents::InputInt("Spieler-ID", &markerPlayerId_, "-1 = Name verwenden");
    const OnlineSnapshot snapshot = onlineDataManager_ != nullptr ? onlineDataManager_->GetSnapshot() : OnlineSnapshot{};
    std::string previewName = markerName_;
    if (markerPlayerId_ >= 0) {
        previewName.clear();
        if (overlayController_ != nullptr) {
            for (const auto& player : overlayController_->GetGameState().players) {
                if (player.id == markerPlayerId_) { previewName = player.name; break; }
            }
        }
    }
    const std::string previewAffiliation = OnlineOverlayController::ResolveMarkerAffiliation(snapshot, previewName);
    UIComponents::InfoRow("Fraktion / Gang", previewAffiliation.c_str());
    if (ImGui::RadioButton("Feind", !markerFriendly_)) markerFriendly_ = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("Freund", markerFriendly_)) markerFriendly_ = true;
    if (UIComponents::SecondaryButton("Hinzuf\xC3\xBC" "gen", ImVec2(Scale(160.0f), 0.0f))) {
        std::string name = markerName_;
        markerEditorMessage_.clear();
        if (markerPlayerId_ >= 0) {
            name.clear();
            if (overlayController_ != nullptr) {
                for (const auto& player : overlayController_->GetGameState().players) {
                    if (player.id == markerPlayerId_) { name = player.name; break; }
                }
            }
            if (name.empty()) markerEditorMessage_ = "Diese ID ist aktuell nicht verbunden.";
        }
        const std::size_t first = name.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) name = name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
        else name.clear();
        const std::string normalized = OnlineOverlayController::NormalizeMarkerName(name);
        if (markerEditorMessage_.empty() && (name.empty() || name.size() > 24 || name.find_first_of(" \t\r\n{}") != std::string::npos)) {
            markerEditorMessage_ = "Bitte einen g\xC3\xBC" "ltigen Spielernamen mit maximal 24 Zeichen eingeben.";
        }
        for (const auto& marker : settings.online_player_markers) {
            if (OnlineOverlayController::NormalizeMarkerName(marker.name) == normalized) {
                markerEditorMessage_ = "Dieser Spieler ist bereits gespeichert.";
                break;
            }
        }
        if (markerEditorMessage_.empty() && settings.online_player_markers.size() >= 20) markerEditorMessage_ = "Die Spielerliste ist voll (20 Spieler).";
        if (markerEditorMessage_.empty()) {
            OnlinePlayerMarker marker;
            marker.name = name;
            marker.friendly = markerFriendly_;
            settings.online_player_markers.push_back(std::move(marker));
            markerName_.clear();
            markerPlayerId_ = -1;
            markerEditorMessage_ = name + " gespeichert.";
            PersistSettings(); ApplyLiveSettings();
        }
    }
    if (!markerEditorMessage_.empty()) ImGui::TextWrapped("%s", markerEditorMessage_.c_str());
    UIComponents::EndCard();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::SectionGap) * 0.5f));
    UIComponents::BeginCard("PlayerRadarList", 0.0f, true);
    UIComponents::CardHeader("Gespeicherte Spieler", "Online-Status und Zuordnung");
    if (settings.online_player_markers.empty()) {
        ImGui::TextColored(theme.GetColor(ThemeColorRole::TextMuted), "Noch keine Spieler gespeichert.");
    }
    int deleteIndex = -1;
    for (std::size_t i = 0; i < settings.online_player_markers.size(); ++i) {
        auto& marker = settings.online_player_markers[i];
        ImGui::PushID(static_cast<int>(i));
        const OnlineMarkerState* state = nullptr;
        if (overlayController_ != nullptr) {
            for (const auto& candidate : overlayController_->GetMarkerStates()) {
                if (OnlineOverlayController::NormalizeMarkerName(candidate.name) == OnlineOverlayController::NormalizeMarkerName(marker.name)) { state = &candidate; break; }
            }
        }
        std::string status = state != nullptr && state->online ? "Online | ID " + std::to_string(state->playerId) : "Offline";
        UIComponents::InfoRow(marker.name.c_str(), status.c_str());
        const std::string affiliation = OnlineOverlayController::ResolveMarkerAffiliation(snapshot, marker.name);
        UIComponents::InfoRow("Fraktion / Gang", affiliation.c_str());
        if (UIComponents::Toggle("Freund", &marker.friendly)) { PersistSettings(); ApplyLiveSettings(); }
        if (UIComponents::SecondaryButton("L\xC3\xB6" "schen", ImVec2(Scale(120.0f), 0.0f))) deleteIndex = static_cast<int>(i);
        if (i + 1 < settings.online_player_markers.size()) UIComponents::Separator();
        ImGui::PopID();
    }
    if (deleteIndex >= 0) {
        settings.online_player_markers.erase(settings.online_player_markers.begin() + deleteIndex);
        PersistSettings(); ApplyLiveSettings();
    }
    UIComponents::EndCard();
}

// ---------------------------------------------------------------------------
// Overlay settings tab (main entry)
// ---------------------------------------------------------------------------

void OnlineUI::RenderOverlaySettingsTab() {
    auto& theme = ThemeManager::Instance();
    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::InnerSpacingY)));

    // Inner sub-tabs for overlay settings sections
    UIComponents::BeginSubTabs("OverlaySettingsSections");
    if (UIComponents::SubTabItem("Quick", static_cast<int>(currentOverlaySection_) == 0)) {
        currentOverlaySection_ = OnlineOverlaySection::Quick;
    }
    if (UIComponents::SubTabItem("Dauerhaft", static_cast<int>(currentOverlaySection_) == 1)) {
        currentOverlaySection_ = OnlineOverlaySection::Persistent;
    }
    if (UIComponents::SubTabItem("Freunde & Feinde", currentOverlaySection_ == OnlineOverlaySection::PlayerMarkers)) {
        currentOverlaySection_ = OnlineOverlaySection::PlayerMarkers;
    }
    UIComponents::EndSubTabs();

    switch (currentOverlaySection_) {
        case OnlineOverlaySection::Quick:
            RenderQuickOverlaySettings();
            break;
        case OnlineOverlaySection::Persistent:
            RenderPersistentOverlaySettings();
            break;
        case OnlineOverlaySection::Appearance:
            RenderPersistentOverlaySettings();
            break;
        case OnlineOverlaySection::PlayerMarkers:
            RenderPlayerMarkerSettings();
            break;
    }
}

} // namespace keybinder
