#include "../include/SprayCatalog.h"
#include "../include/SpraySpots.h"
#include "../include/pugixml/pugixml.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <utility>
#include <windows.h>

namespace keybinder {
namespace {
constexpr DWORD kMaximumFileBytes = 256 * 1024;

std::wstring WideSprayPath(const std::string& text) {
    if (text.empty() || text.size() > 32760 || text.find('\0') != std::string::npos) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string CleanSprayName(const std::string& text) {
    if (text.empty() || text.size() > 256) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    std::size_t characters = wide.size(); // Match the GPS name's UTF-16 limit.
    for (std::size_t index = 0; index < wide.size(); ++index) {
        const unsigned int ch = static_cast<unsigned int>(wide[index]);
        if (ch < 0x20 || (ch >= 0x7f && ch <= 0x9f)) return {};
    }
    const auto first = text.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(' ');
    const auto name = text.substr(first, last - first + 1);
    // Spaces removed at the edges do not consume the user-facing name limit.
    characters -= first + text.size() - last - 1;
    return characters >= 1 && characters <= 64 ? name : std::string{};
}

bool UnsignedAttribute(const pugi::xml_attribute& attribute, unsigned int& value) {
    const char* begin = attribute.as_string();
    const char* end = begin + std::strlen(begin);
    if (begin == end) return false;
    const auto parsed = std::from_chars(begin, end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool FloatAttribute(const pugi::xml_attribute& attribute, float& value) {
    const char* begin = attribute.as_string();
    const char* end = begin + std::strlen(begin);
    if (begin == end) return false;
    const auto parsed = std::from_chars(begin, end, value, std::chars_format::general);
    return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(value);
}

bool ValidSprayPosition(const GpsPosition& position) {
    return GpsManager::IsValidPosition(position) && position.interior == 0;
}
} // namespace

std::vector<GpsEntry> SprayCatalog::DefaultEntries() {
    std::vector<GpsEntry> defaults;
    defaults.reserve(kSpraySpots.size());
    for (const auto& point : kSpraySpots) defaults.push_back({point.id, point.place, point.position});
    return defaults;
}

bool SprayCatalog::Fail(const std::string& error) { lastError_ = error; return false; }

bool SprayCatalog::Initialize(std::string path) {
    path_ = std::move(path);
    entries_ = DefaultEntries();
    nextId_ = 101;
    writable_ = false;
    lastError_.clear();
    const auto wide = WideSprayPath(path_);
    if (wide.empty()) return Fail("Der Pfad der Spray-Datei ist ung\xC3\xBC" "ltig.");
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &attributes)) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) { writable_ = true; return true; }
        return Fail("Die Spray-Datei konnte nicht gelesen werden. Deine vorhandenen Daten bleiben erhalten.");
    }
    if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || attributes.nFileSizeHigh
        || attributes.nFileSizeLow > kMaximumFileBytes)
        return Fail("Die Spray-Datei ist ung\xC3\xBC" "ltig oder zu gro\xC3\x9F. Deine vorhandenen Daten bleiben erhalten.");
    pugi::xml_document document;
    if (!document.load_file(wide.c_str(), pugi::parse_default, pugi::encoding_utf8))
        return Fail("Die Spray-Datei ist besch\xC3\xA4" "digt. Deine vorhandenen Daten bleiben erhalten.");
    const auto root = document.document_element();
    unsigned int version = 0, nextId = 0;
    if (std::strcmp(root.name(), "nlbinder-sprays") != 0
        || !UnsignedAttribute(root.attribute("version"), version) || version != 1
        || !UnsignedAttribute(root.attribute("next-id"), nextId) || nextId < 101 || nextId > GpsManager::kTemporaryIdMask)
        return Fail("Das Format der Spray-Datei ist ung\xC3\xBC" "ltig. Deine vorhandenen Daten bleiben erhalten.");
    for (const auto node : document.children())
        if (node.type() == pugi::node_element && node != root)
            return Fail("Das Format der Spray-Datei ist ung\xC3\xBC" "ltig. Deine vorhandenen Daten bleiben erhalten.");
    std::vector<GpsEntry> loaded;
    std::set<unsigned int> ids;
    unsigned int maximumId = 0;
    for (const auto node : root.children()) {
        if (node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata)
            return Fail("Das Format der Spray-Datei ist ung\xC3\xBC" "ltig. Deine vorhandenen Daten bleiben erhalten.");
        if (node.type() != pugi::node_element) continue;
        GpsEntry entry;
        unsigned int interior = 0;
        entry.name = CleanSprayName(node.attribute("name").as_string());
        if (std::strcmp(node.name(), "point") != 0 || loaded.size() >= kMaximumEntries
            || !UnsignedAttribute(node.attribute("id"), entry.id) || entry.id == 0
            || entry.id >= GpsManager::kTemporaryIdMask || !ids.insert(entry.id).second
            || entry.name.empty() || !FloatAttribute(node.attribute("x"), entry.position.x)
            || !FloatAttribute(node.attribute("y"), entry.position.y) || !FloatAttribute(node.attribute("z"), entry.position.z)
            || !UnsignedAttribute(node.attribute("interior"), interior) || interior != 0
            || !ValidSprayPosition(entry.position))
            return Fail("Die Spray-Datei enth\xC3\xA4" "lt ung\xC3\xBC" "ltige Punkte. Deine vorhandenen Daten bleiben erhalten.");
        maximumId = (std::max)(maximumId, entry.id);
        loaded.push_back(std::move(entry));
    }
    if (nextId <= maximumId)
        return Fail("Die Punktnummern in der Spray-Datei sind ung\xC3\xBC" "ltig. Deine vorhandenen Daten bleiben erhalten.");
    entries_ = std::move(loaded);
    nextId_ = nextId;
    writable_ = true;
    return true;
}

const GpsEntry* SprayCatalog::Find(unsigned int id) const {
    const auto found = std::find_if(entries_.begin(), entries_.end(), [id](const auto& entry) { return entry.id == id; });
    return found == entries_.end() ? nullptr : &*found;
}

bool SprayCatalog::Save(const std::vector<GpsEntry>& entries, unsigned int nextId) {
    pugi::xml_document document;
    auto root = document.append_child("nlbinder-sprays");
    root.append_attribute("version") = 1;
    root.append_attribute("next-id") = nextId;
    for (const auto& entry : entries) {
        auto point = root.append_child("point");
        point.append_attribute("id") = entry.id;
        point.append_attribute("name") = entry.name.c_str();
        point.append_attribute("x").set_value(entry.position.x, 9);
        point.append_attribute("y").set_value(entry.position.y, 9);
        point.append_attribute("z").set_value(entry.position.z, 9);
        point.append_attribute("interior") = 0;
    }
    std::ostringstream stream;
    document.save(stream, "  ", pugi::format_default, pugi::encoding_utf8);
    const std::string xml = stream.str();
    if (xml.size() > kMaximumFileBytes) return Fail("Die Spray-Datei ist zu gro\xC3\x9F.");
    const auto destination = WideSprayPath(path_);
    static std::atomic<unsigned long> temporaryCounter{0};
    std::wstring temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 8 && file == INVALID_HANDLE_VALUE; ++attempt) {
        temporary = destination + L".tmp-" + std::to_wstring(GetCurrentProcessId())
            + L"-" + std::to_wstring(++temporaryCounter);
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) break;
    }
    if (file == INVALID_HANDLE_VALUE) return Fail("Die Spraypunkte konnten nicht gespeichert werden. Deine bisherigen Punkte bleiben erhalten.");
    DWORD written = 0;
    const bool saved = WriteFile(file, xml.data(), static_cast<DWORD>(xml.size()), &written, nullptr)
        && written == xml.size() && FlushFileBuffers(file);
    CloseHandle(file);
    const bool moved = saved && MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!moved) {
        DeleteFileW(temporary.c_str()); // Only the CREATE_NEW file owned by this save.
        return Fail("Die Spraypunkte konnten nicht gespeichert werden. Deine bisherigen Punkte bleiben erhalten.");
    }
    return true;
}

bool SprayCatalog::Add(const std::string& requestedName, const GpsPosition& position) {
    if (!writable_) return lastError_.empty() ? Fail("Die Spray-Datei ist nicht zum Bearbeiten verf\xC3\xBC" "gbar.") : false;
    if (entries_.size() >= kMaximumEntries) return Fail("Maximal 340 Spraypunkte sind erlaubt (10 Seiten mit je 34 Punkten).");
    const auto name = CleanSprayName(requestedName);
    if (name.empty()) return Fail("Bitte gib einen Namen mit 1 bis 64 Zeichen ohne Steuerzeichen ein.");
    if (!ValidSprayPosition(position)) return Fail("Spraypunkte k\xC3\xB6" "nnen nur an einer g\xC3\xBC" "ltigen Position im Freien gespeichert werden.");
    if (nextId_ >= GpsManager::kTemporaryIdMask) return Fail("Es sind keine freien Spray-Punktnummern mehr verf\xC3\xBC" "gbar.");
    auto changed = entries_;
    changed.push_back({nextId_, name, position});
    if (!Save(changed, nextId_ + 1)) return false;
    entries_ = std::move(changed);
    ++nextId_;
    lastError_.clear();
    return true;
}

bool SprayCatalog::Remove(unsigned int id) {
    if (!writable_) return lastError_.empty() ? Fail("Die Spray-Datei ist nicht zum Bearbeiten verf\xC3\xBC" "gbar.") : false;
    if (!Find(id)) return Fail("Der Spraypunkt wurde nicht gefunden.");
    auto changed = entries_;
    changed.erase(std::remove_if(changed.begin(), changed.end(), [id](const auto& entry) { return entry.id == id; }), changed.end());
    if (!Save(changed, nextId_)) return false;
    entries_ = std::move(changed);
    lastError_.clear();
    return true;
}

} // namespace keybinder
