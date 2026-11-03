#include "../include/GpsManager.h"
#include "../include/UIComponents.h"
#include "../include/imgui-1.92.7/imgui.h"
#include "../include/pugixml/pugixml.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <windows.h>

namespace keybinder {
namespace {
std::wstring WideGpsPath(const std::string& value) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}
std::string GpsName(std::string value) {
    if (!value.empty() && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0) <= 0) {
        const int size = MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()), wide.data(), size);
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), size, nullptr, 0, nullptr, nullptr);
        value.assign(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), size, value.data(), bytes, nullptr, nullptr);
    }
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    const int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (characters <= 0 || characters > 64 || std::any_of(value.begin(), value.end(), [](unsigned char ch) { return ch < 32; })) return {};
    return value;
}
} // namespace

void GpsManager::Initialize(std::string path, Notice notice, PositionProvider provider) {
    path_ = std::move(path); notice_ = std::move(notice); positionProvider_ = std::move(provider);
    if (!Load()) Say("Die GPS-Datei konnte nicht gelesen werden. Vorhandene Daten werden nicht automatisch ersetzt.");
}
void GpsManager::Shutdown() { listOpen_ = false; StopNavigation(); }
void GpsManager::Say(const std::string& text) const {
    if (notice_) notice_("{33CCFF}[NL-BINDER]{FFFFFF} " + text);
}
bool GpsManager::IsValidPosition(const GpsPosition& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
        && std::fabs(p.x) <= 1000000.0f && std::fabs(p.y) <= 1000000.0f && std::fabs(p.z) <= 1000000.0f
        && p.interior >= 0 && p.interior <= 255;
}
bool GpsManager::HasArrived(const GpsPosition& current, const GpsPosition& target) {
    if (!IsValidPosition(current) || !IsValidPosition(target) || current.interior != target.interior) return false;
    const float dx = current.x - target.x, dy = current.y - target.y;
    return dx * dx + dy * dy <= 9.0f && std::fabs(current.z - target.z) <= 4.0f;
}
bool GpsManager::Load() {
    entries_.clear(); activeId_ = selectedId_ = 0; nextId_ = 1;
    const auto path = WideGpsPath(path_);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return GetLastError() == ERROR_FILE_NOT_FOUND;
    pugi::xml_document document;
    if (!document.load_file(path.c_str()) || !document.child("nlbinder-gps")) return false;
    for (const auto node : document.child("nlbinder-gps").children("entry")) {
        if (entries_.size() >= kMaximumEntries) break;
        GpsEntry entry;
        entry.id = nextId_++;
        entry.name = GpsName(node.attribute("name").as_string());
        entry.position = {node.attribute("x").as_float(NAN), node.attribute("y").as_float(NAN),
            node.attribute("z").as_float(NAN), node.attribute("interior").as_int(-1)};
        if (!entry.name.empty() && IsValidPosition(entry.position)) entries_.push_back(std::move(entry));
    }
    return true;
}
bool GpsManager::Save() const {
    pugi::xml_document document;
    auto root = document.append_child("nlbinder-gps");
    root.append_attribute("version") = 1;
    for (const auto& entry : entries_) {
        auto node = root.append_child("entry");
        node.append_attribute("name") = entry.name.c_str();
        node.append_attribute("x").set_value(entry.position.x, 9);
        node.append_attribute("y").set_value(entry.position.y, 9);
        node.append_attribute("z").set_value(entry.position.z, 9);
        node.append_attribute("interior") = entry.position.interior;
    }
    const auto destination = WideGpsPath(path_), temporary = destination + L".tmp";
    if (!document.save_file(temporary.c_str(), "  ", pugi::format_default, pugi::encoding_utf8)) return false;
    return MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
bool GpsManager::Set(const std::string& requestedName) {
    const auto name = GpsName(requestedName);
    if (name.empty()) { Say("Nutzung: /setgps (Bemerkung, maximal 64 Zeichen)"); return false; }
    if (entries_.size() >= kMaximumEntries) { Say("Du kannst maximal 20 GPS-Ziele speichern. Entferne zuerst einen Eintrag mit /gpslist."); return false; }
    GpsPosition position;
    if (!positionProvider_ || !positionProvider_(position) || !IsValidPosition(position)) {
        Say("Deine aktuelle Spielposition ist noch nicht verf\xC3\xBC" "gbar."); return false;
    }
    entries_.push_back({nextId_++, name, position});
    if (!Save()) { entries_.pop_back(); Say("Das GPS-Ziel konnte nicht gespeichert werden."); return false; }
    Say("GPS-Ziel gespeichert: " + name + " (" + std::to_string(entries_.size()) + "/20).");
    return true;
}
bool GpsManager::Remove(unsigned int id) {
    const auto before = entries_;
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [id](const auto& entry) { return entry.id == id; }), entries_.end());
    if (entries_.size() == before.size()) return false;
    if (!Save()) { entries_ = before; Say("Das GPS-Ziel konnte nicht gel\xC3\xB6" "scht werden."); return false; }
    if (activeId_ == id) StopNavigation();
    if (selectedId_ == id) selectedId_ = 0;
    Say("GPS-Ziel gel\xC3\xB6" "scht."); return true;
}
bool GpsManager::RemoveAll() {
    const auto before = entries_; entries_.clear();
    if (!Save()) { entries_ = before; Say("Die GPS-Ziele konnten nicht gel\xC3\xB6" "scht werden."); return false; }
    StopNavigation(); selectedId_ = 0; Say("Alle GPS-Eintr\xC3\xA4ge wurden gel\xC3\xB6" "scht."); return true;
}
const GpsEntry* GpsManager::ActiveDestination() const {
    const auto found = std::find_if(entries_.begin(), entries_.end(), [this](const auto& entry) { return entry.id == activeId_; });
    return found == entries_.end() ? nullptr : &*found;
}
bool GpsManager::Navigate(unsigned int id) {
    if (std::none_of(entries_.begin(), entries_.end(), [id](const auto& entry) { return entry.id == id; })) return false;
    activeId_ = id; listOpen_ = false;
    GpsPosition current;
    arrivalArmed_ = positionProvider_ && positionProvider_(current)
        && !HasArrived(current, ActiveDestination()->position);
    Say("Navigation gestartet: " + ActiveDestination()->name + ". Das Ziel ist rot markiert."); return true;
}
void GpsManager::StopNavigation() { activeId_ = 0; arrivalArmed_ = false; }
void GpsManager::ShowHelp() const {
    Say("/setgps (Bemerkung) - Marker speichern | /gpslist - GPS-Liste \xC3\xB6" "ffnen");
    Say("/delgpsall - Alle GPS-Eintr\xC3\xA4ge l\xC3\xB6" "schen | /gps - Diese Hilfe");
}
void GpsManager::Update() {
    const auto* destination = ActiveDestination();
    if (!destination) return;
    GpsPosition current;
    if (!positionProvider_ || !positionProvider_(current) || !IsValidPosition(current)) return;
    const bool inside = HasArrived(current, destination->position);
    if (!inside) { arrivalArmed_ = true; return; }
    // Starting navigation while standing on the saved position must still
    // show the checkpoint. Arrival is armed after first leaving its radius.
    if (arrivalArmed_) {
        const auto name = destination->name;
        StopNavigation(); Say("Du hast dein Ziel erreicht: " + name + ".");
    }
}
void GpsManager::Render() {
    if (!listOpen_) return;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("GPS-Liste##NLBinderGps", &listOpen_, ImGuiWindowFlags_NoCollapse)) {
        UIComponents::CardHeader("Gespeicherte GPS-Ziele", "Ziel ausw\xC3\xA4hlen und navigieren oder l\xC3\xB6" "schen");
        ImGui::Text("%u / 20 Ziele", static_cast<unsigned int>(entries_.size()));
        if (ImGui::BeginChild("##gps-entries", ImVec2(0.0f, -80.0f), ImGuiChildFlags_Borders)) {
            for (const auto& entry : entries_) {
                ImGui::PushID(static_cast<int>(entry.id));
                if (ImGui::Selectable(entry.name.c_str(), selectedId_ == entry.id)) selectedId_ = entry.id;
                ImGui::PopID();
            }
            if (entries_.empty()) ImGui::TextWrapped("Noch keine Ziele gespeichert. Nutze /setgps (Bemerkung).");
        }
        ImGui::EndChild();
        if (UIComponents::PrimaryButton("Navigieren", ImVec2(150, 0), selectedId_ == 0)) Navigate(selectedId_);
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("L\xC3\xB6" "schen", ImVec2(120, 0), selectedId_ == 0)) Remove(selectedId_);
        if (activeId_ && UIComponents::SecondaryButton("Navigation beenden")) StopNavigation();
    }
    ImGui::End();
}
} // namespace keybinder
