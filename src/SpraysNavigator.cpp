#include "../include/SpraysNavigator.h"
#include "../include/SpraySpots.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace keybinder {

void SpraysNavigator::Initialize(GpsManager* gps, Notice notice, PositionProvider position, SessionProvider session) {
    Shutdown();
    gps_ = gps;
    notice_ = std::move(notice);
    positionProvider_ = std::move(position);
    sessionProvider_ = std::move(session);
}

void SpraysNavigator::Shutdown() {
    StopOwnedRoute();
    open_ = false;
    resetScroll_ = false;
    page_ = 0;
    rows_.clear();
    session_.clear();
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
    return ownedDestinationId_ != 0 && destination && destination->id == ownedDestinationId_;
}

void SpraysNavigator::StopOwnedRoute() {
    if (OwnsActiveRoute()) gps_->StopNavigation();
    ownedDestinationId_ = 0;
}

bool SpraysNavigator::EnsureAccess(bool notify) {
    const std::string current = sessionProvider_ ? sessionProvider_() : std::string{};
    if (current.empty() || (!session_.empty() && current != session_)) {
        Close();
        StopOwnedRoute();
        rows_.clear();
        page_ = 0;
    }
    session_ = current;
    if (current.empty()) {
        if (notify) Say("Du bist kein Polizist.");
        return false;
    }
    return true;
}

bool SpraysNavigator::ReadPosition(GpsPosition& position) const {
    return positionProvider_ && positionProvider_(position) && GpsManager::IsValidPosition(position);
}

std::vector<SprayDistance> SpraysNavigator::BuildRows(const GpsPosition& position) {
    if (!GpsManager::IsValidPosition(position) || position.interior != 0) return {};
    std::vector<SprayDistance> rows;
    rows.reserve(kSpraySpots.size());
    for (const auto& point : kSpraySpots) {
        const double dx = static_cast<double>(position.x) - point.position.x;
        const double dy = static_cast<double>(position.y) - point.position.y;
        const double dz = static_cast<double>(position.z) - point.position.z;
        rows.push_back({point.id, point.place, static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz))});
    }
    std::sort(rows.begin(), rows.end(), [](const auto& left, const auto& right) {
        return left.distance == right.distance ? left.id < right.id : left.distance < right.distance;
    });
    return rows;
}

bool SpraysNavigator::Open() {
    if (!EnsureAccess(true)) return false;
    GpsPosition position;
    if (!gps_ || !ReadPosition(position)) {
        Close();
        rows_.clear();
        Say("Deine aktuelle Spielposition ist noch nicht verf\xC3\xBC" "gbar.");
        return false;
    }
    if (position.interior != 0) {
        Close();
        rows_.clear();
        Say("Spray-Navigation ist nur au\xC3\x9F" "erhalb von Innenr\xC3\xA4" "umen verf\xC3\xBC" "gbar.");
        return false;
    }
    rows_ = BuildRows(position);
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
    return (rows_.size() + kRowsPerPage - 1) / kRowsPerPage;
}

void SpraysNavigator::SetPage(std::size_t page) {
    const auto count = PageCount();
    const auto next = count ? (std::min)(page, count - 1) : 0;
    if (next != page_) resetScroll_ = true;
    page_ = next;
}

void SpraysNavigator::Update() {
    if (!EnsureAccess(false)) return;
    if (ownedDestinationId_ && !OwnsActiveRoute()) ownedDestinationId_ = 0;
    if (!open_) return;
    GpsPosition position;
    const bool available = ReadPosition(position);
    if (available && position.interior != 0) Close();
    rows_ = available ? BuildRows(position) : std::vector<SprayDistance>{};
    SetPage(page_);
}

bool SpraysNavigator::Select(unsigned int id) {
    if (!EnsureAccess(true) || !open_ || !gps_) return false;
    const auto found = std::find_if(kSpraySpots.begin(), kSpraySpots.end(), [id](const auto& point) {
        return point.id == id;
    });
    if (found == kSpraySpots.end()) return false;
    GpsPosition current;
    if (!ReadPosition(current)) {
        Say("Deine aktuelle Spielposition ist noch nicht verf\xC3\xBC" "gbar.");
        return false;
    }
    if (current.interior != 0) {
        Close();
        Say("Spray-Navigation ist nur au\xC3\x9F" "erhalb von Innenr\xC3\xA4" "umen verf\xC3\xBC" "gbar.");
        return false;
    }
    char name[96];
    std::snprintf(name, sizeof(name), "Graffiti %03u - %s", found->id, found->place);
    const unsigned int destinationId = GpsManager::kTemporaryIdMask | found->id;
    if (!gps_->NavigateTemporary({destinationId, name, found->position})) return false;
    ownedDestinationId_ = destinationId;
    Close();
    return true;
}

void SpraysNavigator::Render() {
    Update();
    if (!open_) return;
    const auto display = ImGui::GetIO().DisplaySize;
    const float width = (std::max)(260.0f, (std::min)(680.0f, display.x - 40.0f));
    const float height = (std::max)(280.0f, (std::min)(790.0f, display.y - 60.0f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2((std::min)(400.0f, width), (std::min)(340.0f, height)), ImVec2(width, height));
    if (ImGui::Begin("Graffiti-Navigation##NLBinderSprays", &open_, ImGuiWindowFlags_NoCollapse)) {
        UIComponents::CardHeader("SAPD / LSPD - Graffitipunkte", "Punkt anklicken, um die Navigation zu starten");
        ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f), "Gr\xC3\xBC" "n: n\xC3\xA4" "chster Punkt");
        ImGui::SameLine();
        ImGui::TextDisabled("| Entfernung in Metern (Luftlinie)");
        if (rows_.empty()) ImGui::TextWrapped("Deine aktuelle Position im Freien ist noch nicht verf\xC3\xBC" "gbar.");
        const auto begin = page_ * kRowsPerPage;
        const auto end = (std::min)(begin + kRowsPerPage, rows_.size());
        if (ImGui::BeginTable("##spray-points", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
            | ImGuiTableFlags_ScrollY, ImVec2(0.0f, (std::max)(100.0f, ImGui::GetContentRegionAvail().y - 72.0f)))) {
            ImGui::TableSetupColumn("Nr.", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ImGui::TableSetupColumn("Stadtteil / Ort", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Entfernung", ImGuiTableColumnFlags_WidthFixed, 95.0f);
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
                char id[8];
                std::snprintf(id, sizeof(id), "%03u", row.id);
                const bool chosen = ImGui::Selectable(id, false, ImGuiSelectableFlags_SpanAllColumns);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.place);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.0f m", row.distance);
                if (nearest) ImGui::PopStyleColor();
                ImGui::PopID();
                if (chosen) { Select(row.id); break; }
            }
            ImGui::EndTable();
        }
        if (UIComponents::SecondaryButton("Zur\xC3\xBC" "ck", ImVec2(110.0f, 0.0f), page_ == 0)) SetPage(page_ - 1);
        ImGui::SameLine();
        ImGui::Text("Seite %u / %u | %u Punkte", static_cast<unsigned int>(PageCount() ? page_ + 1 : 0),
            static_cast<unsigned int>(PageCount()), static_cast<unsigned int>(rows_.size()));
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Weiter", ImVec2(110.0f, 0.0f), page_ + 1 >= PageCount())) SetPage(page_ + 1);
        if (UIComponents::SecondaryButton("Schlie\xC3\x9F" "en")) Close();
        if (OwnsActiveRoute()) {
            ImGui::SameLine();
            if (UIComponents::SecondaryButton("Navigation beenden")) StopOwnedRoute();
        }
    }
    ImGui::End();
}

} // namespace keybinder
