#include "../include/WaypointTour.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <windows.h>

namespace keybinder {
namespace {
bool ValidTourPoint(const GpsEntry& point) {
    if (point.id == 0 || point.id >= GpsManager::kTemporaryIdMask
        || !GpsManager::IsValidPosition(point.position) || point.position.interior != 0
        || point.name.empty() || point.name.size() > 256
        || point.name.find_first_not_of(' ') == std::string::npos) return false;
    const int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, point.name.data(),
        static_cast<int>(point.name.size()), nullptr, 0);
    if (characters < 1 || characters > 64) return false;
    return std::none_of(point.name.begin(), point.name.end(), [](unsigned char ch) {
        return ch < 0x20 || ch == 0x7f;
    });
}

bool SamePosition(const GpsPosition& left, const GpsPosition& right) {
    return left.x == right.x && left.y == right.y && left.z == right.z && left.interior == right.interior;
}
} // namespace

void WaypointTour::Initialize(GpsManager* gps, std::string title, std::string stopCommand,
    std::string deniedMessage, Notice notice, PositionProvider position,
    SessionProvider session, PointsProvider points) {
    Shutdown();
    gps_ = gps;
    title_ = std::move(title);
    stopCommand_ = std::move(stopCommand);
    deniedMessage_ = std::move(deniedMessage);
    notice_ = std::move(notice);
    positionProvider_ = std::move(position);
    sessionProvider_ = std::move(session);
    pointsProvider_ = std::move(points);
}

void WaypointTour::Say(const std::string& text) const {
    if (notice_) notice_("{33CCFF}[NL-BINDER]{FFFFFF} " + text);
}

bool WaypointTour::ReadOutsidePosition(GpsPosition& position) const {
    return positionProvider_ && positionProvider_(position)
        && GpsManager::IsValidPosition(position) && position.interior == 0;
}

bool WaypointTour::OwnsRoute() const {
    const auto* destination = gps_ ? gps_->ActiveDestination() : nullptr;
    return destination && destinationId_ && generation_
        && destination->id == destinationId_ && gps_->ActiveNavigationGeneration() == generation_;
}

void WaypointTour::ClearState() {
    active_ = pendingNext_ = false;
    sourceId_ = destinationId_ = 0;
    generation_ = 0;
    session_.clear();
    visited_.clear();
}

void WaypointTour::Stop(bool notify) {
    const bool wasActive = active_;
    if (OwnsRoute()) gps_->StopNavigation();
    ClearState();
    if (notify) Say(wasActive ? title_ + " abgebrochen." : "Keine " + title_ + " aktiv.");
}

void WaypointTour::Shutdown() {
    Stop(false);
    gps_ = nullptr;
    title_.clear(); stopCommand_.clear(); deniedMessage_.clear();
    notice_ = {}; positionProvider_ = {}; sessionProvider_ = {}; pointsProvider_ = {};
}

bool WaypointTour::CheckSession() {
    if (!active_) return false;
    const std::string current = sessionProvider_ ? sessionProvider_() : std::string{};
    if (!current.empty() && current == session_) return true;
    Stop(false);
    Say(title_ + " beendet: Deine Fraktion oder Sitzung hat sich ge\xC3\xA4" "ndert.");
    return false;
}

bool WaypointTour::NextPoint(const GpsPosition& position, GpsEntry& result) const {
    if (!pointsProvider_) return false;
    bool found = false;
    double closest = std::numeric_limits<double>::infinity();
    for (const auto& point : pointsProvider_()) {
        if (!ValidTourPoint(point) || visited_.count(point.id)) continue;
        const double dx = static_cast<double>(position.x) - point.position.x;
        const double dy = static_cast<double>(position.y) - point.position.y;
        const double dz = static_cast<double>(position.z) - point.position.z;
        const double distance = dx * dx + dy * dy + dz * dz;
        if (!found || distance < closest || (distance == closest && point.id < result.id)) {
            result = point;
            closest = distance;
            found = true;
        }
    }
    return found;
}

bool WaypointTour::NavigateTo(const GpsEntry& point) {
    GpsEntry destination = point;
    destination.id |= GpsManager::kTemporaryIdMask;
    if (!gps_ || !gps_->NavigateTemporary(destination, true)) return false;
    sourceId_ = point.id;
    destinationId_ = destination.id;
    generation_ = gps_->ActiveNavigationGeneration();
    pendingNext_ = false;
    return generation_ != 0;
}

bool WaypointTour::Start() {
    if (active_) {
        if (CheckSession()) Say(title_ + " l\xC3\xA4" "uft bereits. Mit " + stopCommand_ + " kannst du die Tour abbrechen.");
        return false;
    }
    const std::string session = sessionProvider_ ? sessionProvider_() : std::string{};
    if (session.empty()) { Say(deniedMessage_); return false; }
    GpsPosition position;
    if (!gps_ || !ReadOutsidePosition(position)) {
        Say("Deine aktuelle Position im Freien ist noch nicht verf\xC3\xBC" "gbar.");
        return false;
    }
    // Validate against a fresh unvisited set before replacing any active route.
    visited_.clear();
    GpsEntry first;
    if (!NextPoint(position, first)) {
        Say("F\xC3\xBC" "r die " + title_ + " sind keine g\xC3\xBC" "ltigen Punkte vorhanden.");
        return false;
    }
    Stop(false);
    active_ = true;
    session_ = session;
    if (!NavigateTo(first)) {
        ClearState();
        Say(title_ + " konnte nicht gestartet werden.");
        return false;
    }
    Say(title_ + " gestartet. Mit " + stopCommand_ + " kannst du die Tour abbrechen.");
    return true;
}

void WaypointTour::EndForReplacement() {
    ClearState(); // The new destination belongs to another GPS action.
    Say(title_ + " beendet: Das Navigationsziel wurde gewechselt.");
}

void WaypointTour::Advance() {
    if (!CheckSession()) return;
    GpsPosition position;
    if (!ReadOutsidePosition(position)) { pendingNext_ = true; return; }
    GpsEntry next;
    if (!NextPoint(position, next)) {
        Stop(false);
        Say(title_ + " abgeschlossen. Alle verf\xC3\xBC" "gbaren Punkte wurden angefahren.");
        return;
    }
    if (!NavigateTo(next)) {
        Stop(false);
        Say(title_ + " beendet: Das n\xC3\xA4" "chste Ziel konnte nicht gesetzt werden.");
    }
}

void WaypointTour::Update() {
    if (!CheckSession()) return;
    if (pendingNext_) {
        if (gps_ && gps_->ActiveDestination()) { EndForReplacement(); return; }
        Advance();
        return;
    }
    if (!OwnsRoute()) { EndForReplacement(); return; }
    const auto& points = pointsProvider_();
    const auto point = std::find_if(points.begin(), points.end(), [this](const auto& value) {
        return value.id == sourceId_ && ValidTourPoint(value);
    });
    const auto* destination = gps_->ActiveDestination();
    if (point == points.end() || !SamePosition(point->position, destination->position)) {
        // A removed or moved configured point is no longer a useful destination.
        gps_->StopNavigation();
        sourceId_ = destinationId_ = 0; generation_ = 0;
        pendingNext_ = true;
        Advance();
    }
}

void WaypointTour::OnArrival(const GpsEntry& destination, std::uint64_t generation) {
    if (!active_ || pendingNext_ || destination.id != destinationId_ || generation != generation_) return;
    if (!CheckSession()) return;
    // A different arrival subscriber may have selected a replacement meanwhile.
    if (gps_ && gps_->ActiveDestination()) { EndForReplacement(); return; }
    visited_.insert(sourceId_);
    sourceId_ = destinationId_ = 0; generation_ = 0;
    pendingNext_ = true;
    Advance();
}

} // namespace keybinder
