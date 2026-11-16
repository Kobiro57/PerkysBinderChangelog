#include "../include/PoliceLocateTracker.h"

#include <algorithm>
#include <cmath>

namespace keybinder {
namespace {

bool IsHex(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

std::string Trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string NormalizeLine(const std::string& line) {
    std::string text;
    text.reserve(line.size());
    for (std::size_t at = 0; at < line.size();) {
        if (line[at] == '{') {
            const auto end = line.find('}', at + 1);
            if (end != std::string::npos && (end - at == 7 || end - at == 9)) {
                bool hex = true;
                for (auto digit = at + 1; digit < end; ++digit) hex = hex && IsHex(line[digit]);
                if (hex) { at = end + 1; continue; }
            }
        }
        text.push_back(line[at++]);
    }
    text = Trim(text);
    // A log-style timestamp is allowed only at the beginning. Never match
    // detective text quoted inside another player's chat or a radio message.
    if (text.size() >= 11 && text[0] == '[' && text[3] == ':' && text[6] == ':'
        && text[9] == ']' && text[10] == ' ') {
        bool digits = true;
        for (int pos : {1, 2, 4, 5, 7, 8}) digits = digits && text[pos] >= '0' && text[pos] <= '9';
        if (digits) text = Trim(text.substr(11));
    }
    return text;
}

bool ValidTarget(const std::string& target) {
    if (target.empty() || target.size() > 24) return false;
    for (unsigned char ch : target) {
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
            || (ch >= '0' && ch <= '9') || ch == '_' || ch == '[' || ch == ']'
            || ch == '(' || ch == ')' || ch == '.' || ch == '$' || ch == '@'
            || ch == '=' || ch == '-')) return false;
    }
    return true;
}

bool ParseStart(const std::string& line, std::string& target, std::uint32_t& durationMs) {
    const std::string prefix = "Detektiv: Das Handy und die Ortung von ";
    const std::string separator = " wurde gestartet und dauert ";
    const std::string suffix = " Sekunden.";
    if (line.compare(0, prefix.size(), prefix) != 0 || line.size() <= prefix.size() + suffix.size()
        || line.compare(line.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
    const auto separatorAt = line.find(separator, prefix.size());
    if (separatorAt == std::string::npos) return false;
    target = line.substr(prefix.size(), separatorAt - prefix.size());
    if (!ValidTarget(target)) return false;
    const auto durationAt = separatorAt + separator.size();
    const auto durationSize = line.size() - suffix.size() - durationAt;
    if (durationSize == 0 || durationSize > 3) return false;
    std::uint32_t seconds = 0;
    for (std::size_t at = durationAt; at < durationAt + durationSize; ++at) {
        if (line[at] < '0' || line[at] > '9') return false;
        seconds = seconds * 10 + static_cast<std::uint32_t>(line[at] - '0');
    }
    if (seconds == 0 || seconds * 1000 > PoliceLocateTracker::kOperationTimeoutMs) return false;
    durationMs = seconds * 1000;
    return true;
}

bool ValidCheckpoint(const LocateCheckpoint& checkpoint) {
    // GTA's world is well within these limits, including supported interiors.
    // Reject corrupt network floats rather than resolving a misleading zone.
    return std::isfinite(checkpoint.x) && std::isfinite(checkpoint.y)
        && std::isfinite(checkpoint.z) && std::isfinite(checkpoint.radius)
        && std::fabs(checkpoint.x) <= 20000.0f && std::fabs(checkpoint.y) <= 20000.0f
        && std::fabs(checkpoint.z) <= 20000.0f
        && checkpoint.radius > 0.0f && checkpoint.radius <= 1000.0f;
}

bool WithinWindow(std::uint32_t first, std::uint32_t second) {
    return static_cast<std::uint32_t>(first - second) <= PoliceLocateTracker::kCompletionWindowMs
        || static_cast<std::uint32_t>(second - first) <= PoliceLocateTracker::kCompletionWindowMs;
}

std::string SafeDistrict(const std::string& value) {
    const std::string district = Trim(value);
    if (district.empty() || district.size() > 100 || district == "Unknown" || district == "unknown") return {};
    for (unsigned char ch : district) {
        if (ch < 0x20 || ch == 0x7f || ch == '{' || ch == '}' || ch == '[' || ch == ']') return {};
    }
    return district;
}

} // namespace

void PoliceLocateTracker::SetSession(bool policeAllowed, const std::string& connectionIdentity) {
    const bool permitted = policeAllowed && !connectionIdentity.empty();
    if (!permitted || !allowed_ || connectionIdentity != identity_) ClearOperation();
    allowed_ = permitted;
    identity_ = permitted ? connectionIdentity : std::string{};
}

void PoliceLocateTracker::Reset() {
    ClearOperation();
    allowed_ = false;
    identity_.clear();
}

void PoliceLocateTracker::ClearOperation() {
    pending_ = false;
    target_.clear();
    startedAt_ = durationMs_ = 0;
    successObserved_ = markerObserved_ = false;
    successAt_ = markerAt_ = 0;
    marker_ = {};
}

bool PoliceLocateTracker::ValidatePending(std::uint32_t nowMs) {
    if (!allowed_ || !pending_) return false;
    if (static_cast<std::uint32_t>(nowMs - startedAt_) > kOperationTimeoutMs
        || (successObserved_ && static_cast<std::uint32_t>(nowMs - successAt_) > kCompletionWindowMs)) {
        ClearOperation();
        return false;
    }
    return true;
}

bool PoliceLocateTracker::ObserveChat(const std::string& line, std::uint32_t nowMs) {
    if (!allowed_) return false;
    const std::string normalized = NormalizeLine(line);
    std::string target;
    std::uint32_t durationMs = 0;
    if (ParseStart(normalized, target, durationMs)) {
        if (ValidatePending(nowMs) && target_ == target && durationMs_ == durationMs
            && static_cast<std::uint32_t>(nowMs - startedAt_) < 1000) return false;
        ClearOperation();
        pending_ = true;
        target_ = std::move(target);
        startedAt_ = nowMs;
        durationMs_ = durationMs;
        return true;
    }
    if (normalized != "Detektiv: Das Ziel wurde geortet und wird dir nun auf der Karte angezeigt."
        || !ValidatePending(nowMs) || successObserved_) return false;
    // The server confirms completion. Its announced duration is informative;
    // packet delivery or a skill change must not invalidate an actual success.
    successObserved_ = true;
    successAt_ = nowMs;
    if (markerObserved_ && !WithinWindow(markerAt_, successAt_)) markerObserved_ = false;
    return true;
}

bool PoliceLocateTracker::ObserveCheckpoint(const LocateCheckpoint& checkpoint, std::uint32_t nowMs) {
    if (!ValidatePending(nowMs) || !ValidCheckpoint(checkpoint)) return false;
    if (successObserved_ && !WithinWindow(nowMs, successAt_)) return false;
    marker_ = checkpoint;
    markerAt_ = nowMs;
    markerObserved_ = true;
    return true;
}

void PoliceLocateTracker::ObserveCheckpointDisabled() {
    markerObserved_ = false;
    marker_ = {};
    markerAt_ = 0;
}

bool PoliceLocateTracker::ReadyForCurrentCheckpoint(std::uint32_t nowMs) {
    return ValidatePending(nowMs) && successObserved_
        && static_cast<std::uint32_t>(nowMs - successAt_) >= kNativeCheckpointSettleMs;
}

std::string PoliceLocateTracker::TakeRadioMessage(std::uint32_t nowMs, const DistrictResolver& resolveDistrict) {
    if (!ValidatePending(nowMs) || !successObserved_ || !markerObserved_
        || !WithinWindow(markerAt_, successAt_) || !resolveDistrict) return {};
    std::string district;
    try { district = SafeDistrict(resolveDistrict(marker_.x, marker_.y, marker_.z)); }
    catch (...) { return {}; }
    if (district.empty()) return {};
    const std::string message = "/r [ORTUNG] Gesuchter " + target_
        + " wurde zuletzt in [" + district + "] geortet.";
    ClearOperation();
    return message;
}

} // namespace keybinder
