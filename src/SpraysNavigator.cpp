#include "../include/SpraysNavigator.h"
#include "../include/SpraySpots.h"
#include "../include/UIComponents.h"
#include "../include/ThemeManager.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace keybinder {

void SpraysNavigator::Initialize(GpsManager* gps, Notice notice, PositionProvider position, SessionProvider session, std::string storagePath) {
    Shutdown();
    gps_ = gps;
    notice_ = std::move(notice);
    positionProvider_ = std::move(position);
    sessionProvider_ = std::move(session);
    if (!catalog_.Initialize(std::move(storagePath))) Say(catalog_.LastError());
}

void SpraysNavigator::Shutdown() {
    StopOwnedRoute();
    open_ = false;
    resetScroll_ = false;
    page_ = 0;
    rows_.clear();
    session_.clear();
    newPointName_.clear();
    gps_ = nullptr;
    notice_ = {};
    positionProvider_ = {};
    sessionProvider_ = {};
}

void SpraysNavigator::Say(const std::string& text) const {
    if (notice_) notice_("{33CCFF}[NL-BINDER]{FFFFFF} " + text);
}

bool SpraysNavigator::OwnsActiveRoute() const {
    const auto* destination = gps_ ? gps_->ActiveDestination() : nullptr;
    return ownedDestinationId_ != 0 && ownedDestinationGeneration_ != 0 && destination
        && destination->id == ownedDestinationId_
        && gps_->ActiveNavigationGeneration() == ownedDestinationGeneration_;
}

void SpraysNavigator::StopOwnedRoute() {
    if (OwnsActiveRoute()) gps_->StopNavigation();
    ownedDestinationId_ = 0;
    ownedDestinationGeneration_ = 0;
}

bool SpraysNavigator::EnsureAccess(bool notify) {
    const std::string current = sessionProvider_ ? sessionProvider_() : std::string{};
    if (current.empty() || (!session_.empty() && current != session_)) {
        Close();
        StopOwnedRoute();
        rows_.clear();
        page_ = 0;
        newPointName_.clear();
    }
    session_ = current;
    if (current.empty()) {
        if (notify) Say("F\xC3\xBC" "r diese Funktion ben\xC3\xB6" "tigst du eine Fraktion oder Gang.");
        return false;
    }
    return true;
}

bool SpraysNavigator::ReadPosition(GpsPosition& position) const {
    return positionProvider_ && positionProvider_(position) && GpsManager::IsValidPosition(position);
}

std::vector<SprayDistance> SpraysNavigator::BuildRows(const GpsPosition& position) {
    return BuildRows(position, SprayCatalog::DefaultEntries());
}

std::vector<SprayDistance> SpraysNavigator::BuildRows(const GpsPosition& position, const std::vector<GpsEntry>& points) {
    if (!GpsManager::IsValidPosition(position) || position.interior != 0) return {};
    std::vector<SprayDistance> rows;
    rows.reserve(points.size());
    for (const auto& point : points) {
        const double dx = static_cast<double>(position.x) - point.position.x;
        const double dy = static_cast<double>(position.y) - point.position.y;
        const double dz = static_cast<double>(position.z) - point.position.z;
        rows.push_back({point.id, point.name, static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz))});
    }
    std::sort(rows.begin(), rows.end(), [](const auto& left, const auto& right) {
        return left.distance == right.distance ? left.id < right.id : left.distance < right.distance;
    });
    return rows;
}

bool SpraysNavigator::ReadOutsidePosition(GpsPosition& position) {
    if (!ReadPosition(position)) {
        Say("Deine aktuelle Spielposition ist noch nicht verf\xC3\xBC" "gbar.");
        return false;
    }
    if (position.interior != 0) {
        Close();
        Say("Spray-Navigation ist nur au\xC3\x9F" "erhalb von Innenr\xC3\xA4" "umen verf\xC3\xBC" "gbar.");
        return false;
    }
    return true;
}

void SpraysNavigator::RefreshRows(const GpsPosition& position) {
    rows_ = BuildRows(position, catalog_.Entries());
    SetPage(page_);
}

bool SpraysNavigator::Open() {
    if (!EnsureAccess(true)) return false;
    GpsPosition position;
    if (!gps_ || !ReadOutsidePosition(position)) {
        Close();
        rows_.clear();
        return false;
    }
    RefreshRows(position);
    page_ = 0;
    resetScroll_ = true;
    open_ = true;
    return true;
}

bool SpraysNavigator::Toggle() {
    if (open_) { Close(); return true; }
    return Open();
}

std::size_t SpraysNavigator::PageCount() const {
    return (catalog_.Entries().size() + kRowsPerPage - 1) / kRowsPerPage;
}

void SpraysNavigator::SetPage(std::size_t page) {
    const auto count = PageCount();
    const auto next = count ? (std::min)(page, count - 1) : 0;
    if (next != page_) resetScroll_ = true;
    page_ = next;
}

void SpraysNavigator::Update() {
    if (!EnsureAccess(false)) return;
    if (ownedDestinationId_ && !OwnsActiveRoute()) {
        ownedDestinationId_ = 0;
        ownedDestinationGeneration_ = 0;
    }
    if (!open_) return;
    GpsPosition position;
    const bool available = ReadPosition(position);
    if (available && position.interior != 0) Close();
    rows_ = available ? BuildRows(position, catalog_.Entries()) : std::vector<SprayDistance>{};
    SetPage(page_);
}

bool SpraysNavigator::Select(unsigned int id) {
    if (!EnsureAccess(true) || !open_ || !gps_) return false;
    const auto* found = catalog_.Find(id);
    if (!found) return false;
    GpsPosition current;
    if (!ReadOutsidePosition(current)) return false;
    const unsigned int destinationId = GpsManager::kTemporaryIdMask | found->id;
    if (!gps_->NavigateTemporary({destinationId, found->name, found->position})) return false;
    ownedDestinationId_ = destinationId;
    ownedDestinationGeneration_ = gps_->ActiveNavigationGeneration();
    Close();
    return true;
}

bool SpraysNavigator::AddCurrent(const std::string& name) {
    if (!EnsureAccess(true) || !open_) return false;
    GpsPosition position;
    if (!ReadOutsidePosition(position)) return false;
    if (!catalog_.Add(name, position)) {
        Say(catalog_.LastError());
        return false;
    }
    RefreshRows(position);
    SetPage(0);
    resetScroll_ = true;
    newPointName_.clear();
    Say("Spraypunkt an deiner aktuellen Position gespeichert.");
    return true;
}

bool SpraysNavigator::Remove(unsigned int id) {
    if (!EnsureAccess(true) || !open_) return false;
    GpsPosition position;
    if (!ReadOutsidePosition(position)) return false;
    if (!catalog_.Remove(id)) {
        Say(catalog_.LastError());
        return false;
    }
    if (ownedDestinationId_ == (GpsManager::kTemporaryIdMask | id)) StopOwnedRoute();
    RefreshRows(position);
    Say("Spraypunkt gel\xC3\xB6" "scht.");
    return true;
}

void SpraysNavigator::Render() {
    Update();
    if (!open_) return;
    const auto display = ImGui::GetIO().DisplaySize;
    const float maximumWidth = (std::max)(1.0f, display.x - 20.0f);
    const float maximumHeight = (std::max)(1.0f, display.y - 20.0f);
    const float width = (std::min)(800.0f, maximumWidth);
    const float height = (std::min)(850.0f, maximumHeight);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2((std::min)(480.0f, maximumWidth), (std::min)(400.0f, maximumHeight)),
        ImVec2(maximumWidth, maximumHeight));
    if (ImGui::Begin("Graffiti-Navigation##NLBinderSprays", &open_, ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const auto& style = ImGui::GetStyle();
        const float buttonHeight = (std::max)(ImGui::GetFrameHeight(),
            ThemeManager::Instance().GetControlHeight(ThemeControlHeightRole::Button));
        const char* previousLabel = "Zur\xC3\xBC" "ck";
        const char* nextLabel = "Weiter";
        const char* closeLabel = "Schlie\xC3\x9F" "en";
        const char* stopLabel = "Stopp";
        const float previousWidth = (std::max)(82.0f, ImGui::CalcTextSize(previousLabel).x + 2 * style.FramePadding.x);
        const float nextWidth = (std::max)(82.0f, ImGui::CalcTextSize(nextLabel).x + 2 * style.FramePadding.x);
        char pageLabel[48];
        std::snprintf(pageLabel, sizeof(pageLabel), "Seite %u / %u", static_cast<unsigned int>(page_ + 1),
            static_cast<unsigned int>((std::max)(std::size_t(1), PageCount())));
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const bool pageFits = previousWidth + nextWidth + ImGui::CalcTextSize(pageLabel).x
            + 2 * style.ItemSpacing.x <= availableWidth;
        const bool actionFits = !OwnsActiveRoute() || ImGui::CalcTextSize(closeLabel).x
            + ImGui::CalcTextSize(stopLabel).x + 4 * style.FramePadding.x + style.ItemSpacing.x <= availableWidth;
        const int footerRows = (pageFits ? 1 : 2) + (actionFits ? 1 : 2);
        const float footerHeight = footerRows * (buttonHeight + style.ItemSpacing.y) + style.ItemSpacing.y;
        unsigned int removeId = 0, navigateId = 0;
        // The body can scroll at a large font size; page controls and Close
        // always occupy a separate fixed area at the bottom of this window.
        if (ImGui::BeginChild("##sprays-body", ImVec2(0, (std::max)(1.0f, ImGui::GetContentRegionAvail().y - footerHeight)))) {
        UIComponents::CardHeader("Graffitipunkte");
        ImGui::TextDisabled("%u / %u Punkte", static_cast<unsigned int>(catalog_.Entries().size()),
            static_cast<unsigned int>(SprayCatalog::kMaximumEntries));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.9f, 0.45f, 1.0f));
        ImGui::TextWrapped("Gr\xC3\xBC" "n: n\xC3\xA4" "chster Punkt | Meter (Luftlinie)");
        ImGui::PopStyleColor();
        const bool writable = catalog_.IsWritable();
        if (!writable) ImGui::TextWrapped("Die Spraydatei ist schreibgesch\xC3\xBC" "tzt.");
        ImGui::BeginDisabled(!writable);
        UIComponents::InputText("##spray-new-name", &newPointName_, "Name des neuen Punkts (aktuelle Position)");
        ImGui::EndDisabled();
        if (UIComponents::PrimaryButton("Aktuelle Position hinzuf\xC3\xBC" "gen", ImVec2(0, 0),
            !writable || catalog_.Entries().size() >= SprayCatalog::kMaximumEntries || newPointName_.empty())) AddCurrent(newPointName_);

        if (catalog_.Entries().empty()) ImGui::TextWrapped("Keine Spraypunkte gespeichert. F\xC3\xBC" "ge oben einen Punkt hinzu.");
        else if (rows_.empty()) ImGui::TextWrapped("Deine aktuelle Position im Freien ist noch nicht verf\xC3\xBC" "gbar.");

        const float tableHeight = (std::max)(1.0f, ImGui::GetContentRegionAvail().y);
        const auto begin = page_ * kRowsPerPage;
        const auto end = (std::min)(begin + kRowsPerPage, rows_.size());
        if (ImGui::BeginTable("##spray-points", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
            | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
            ImGui::TableSetupColumn("Nr.", ImGuiTableColumnFlags_WidthFixed,
                (std::max)(45.0f, ImGui::CalcTextSize("000").x + 2.0f * style.CellPadding.x));
            ImGui::TableSetupColumn("Stadtteil / Ort", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Entfernung", ImGuiTableColumnFlags_WidthFixed,
                (std::max)(85.0f, ImGui::CalcTextSize("99999 m").x + 2.0f * style.CellPadding.x));
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                (std::max)(80.0f, ImGui::CalcTextSize("L\xC3\xB6" "schen").x + 2.0f * style.FramePadding.x));
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            if (resetScroll_) {
                ImGui::SetScrollY(0.0f);
                resetScroll_ = false;
            }
            for (std::size_t index = begin; index < end; ++index) {
                const auto& row = rows_[index];
                const bool nearest = index == 0;
                ImGui::PushID(static_cast<int>(row.id));
                if (nearest) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.9f, 0.45f, 1.0f));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                char id[16];
                std::snprintf(id, sizeof(id), "%03u", row.id);
                if (ImGui::Selectable(id)) navigateId = row.id;
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.place.c_str());
                if (ImGui::IsItemClicked()) navigateId = row.id;
                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::SetTooltip("%s", row.place.c_str());
                }
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.0f m", row.distance);
                if (nearest) ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(3);
                if (UIComponents::GhostButton("L\xC3\xB6" "schen", ImVec2(0, ImGui::GetTextLineHeightWithSpacing()), !writable)) removeId = row.id;
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        }
        ImGui::EndChild();
        // Apply mutations after table iteration. Deleting a row must never also
        // navigate to it, and owning strings keep rendered labels valid.
        if (removeId) Remove(removeId);
        else if (navigateId) Select(navigateId);
        if (UIComponents::SecondaryButton(previousLabel, ImVec2(previousWidth, buttonHeight), page_ == 0)) SetPage(page_ - 1);
        ImGui::SameLine();
        if (pageFits) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(pageLabel);
            ImGui::SameLine();
        }
        if (UIComponents::SecondaryButton(nextLabel, ImVec2(nextWidth, buttonHeight), page_ + 1 >= PageCount())) SetPage(page_ + 1);
        if (!pageFits) ImGui::TextUnformatted(pageLabel);
        if (UIComponents::SecondaryButton(closeLabel, ImVec2(0, buttonHeight))) Close();
        if (OwnsActiveRoute()) {
            if (actionFits) ImGui::SameLine();
            if (UIComponents::SecondaryButton(stopLabel, ImVec2(0, buttonHeight))) StopOwnedRoute();
        }
    }
    ImGui::End();
}

} // namespace keybinder
