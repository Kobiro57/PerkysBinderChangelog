#include "../include/SpeedTicketFlow.h"

#include <cctype>
#include <sstream>
#include <string_view>
#include <vector>

namespace keybinder {
namespace {
std::string Trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return std::string(value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1));
}

bool IsColor(const std::string& text, std::size_t at, std::size_t digits) {
    if (at + digits + 1 >= text.size() || text[at] != '{' || text[at + digits + 1] != '}') return false;
    for (std::size_t index = at + 1; index < at + digits + 1; ++index)
        if (!std::isxdigit(static_cast<unsigned char>(text[index]))) return false;
    return true;
}

// Labels tolerate colours, normal spacing and km/h versus km / h. They remain
// exact after normalization, so a different speed threshold is never selected.
std::string Compact(const std::string& text) {
    std::string result;
    for (std::size_t at = 0; at < text.size();) {
        if (IsColor(text, at, 6)) { at += 8; continue; }
        if (IsColor(text, at, 8)) { at += 10; continue; }
        const unsigned char ch = static_cast<unsigned char>(text[at]);
        if (ch < 128) {
            if (std::isalnum(ch)) result += static_cast<char>(std::tolower(ch));
            ++at;
            continue;
        }
        if (at + 1 < text.size() && ch == 0xC3) {
            const unsigned char next = static_cast<unsigned char>(text[at + 1]);
            if (next == 0xA4 || next == 0x84) result += "ae";
            else if (next == 0xB6 || next == 0x96) result += "oe";
            else if (next == 0xBC || next == 0x9C) result += "ue";
            else if (next == 0x9F) result += "ss";
            at += 2;
        } else {
            ++at; // Title separators such as the UTF-8 » carry no identity.
        }
    }
    return result;
}

std::vector<std::string> Rows(const std::string& text) {
    std::vector<std::string> rows;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        rows.push_back(line);
    }
    return rows;
}

bool IsListStyle(int style) { return style == 2 || style == 4 || style == 5; }

std::string SelectedLabel(const SpeedTicketDialog& dialog, int row) {
    const auto rows = Rows(dialog.text);
    const std::size_t index = static_cast<std::size_t>(row) + (dialog.style == 5 ? 1 : 0);
    if (index >= rows.size()) return {};
    const std::string value = rows[index].substr(0, rows[index].find('\t'));
    std::string plain;
    for (std::size_t at = 0; at < value.size();) {
        if (IsColor(value, at, 6)) { at += 8; continue; }
        if (IsColor(value, at, 8)) { at += 10; continue; }
        plain += value[at++];
    }
    return Trim(plain);
}

int FindUniqueRow(const SpeedTicketDialog& dialog, const std::string& wanted, bool requirePoints) {
    const auto rows = Rows(dialog.text);
    const std::size_t first = dialog.style == 5 ? 1 : 0; // Tablist header is not selectable.
    int found = -1;
    for (std::size_t index = first; index < rows.size(); ++index) {
        const auto tab = rows[index].find('\t');
        const std::string label = rows[index].substr(0, tab);
        if (Compact(label) != wanted) continue;
        if (requirePoints && tab != std::string::npos) {
            const auto next = rows[index].find('\t', tab + 1);
            const std::string points = rows[index].substr(tab + 1, next == std::string::npos ? next : next - tab - 1);
            if (Compact(points) != "15") return -1;
        }
        if (found >= 0) return -1; // Ambiguous labels must not choose an arbitrary row.
        found = static_cast<int>(index - first);
    }
    return found;
}

bool SameDialog(const SpeedTicketDialog& first, const SpeedTicketDialog& second) {
    return first.id == second.id && first.style == second.style
        && first.caption == second.caption && first.text == second.text;
}

bool IsExpectedInput(const SpeedTicketDialog& dialog) {
    if (dialog.style != 1 || Compact(dialog.caption) != "wantedsvergeben") return false;
    bool reason = false, points = false, prompt = false;
    for (const auto& row : Rows(dialog.text)) {
        const std::string value = Compact(row);
        reason = reason || value == "grundgeschwindigkeitsueberschreitungab20kmh";
        points = points || value == "wantedpunkte15";
        prompt = prompt || value == "bittespieleridodernameeingeben";
    }
    return reason && points && prompt;
}
} // namespace

bool SpeedTicketFlow::ParsePlayerId(const std::string& arguments, int& id) {
    const std::string value = Trim(arguments);
    if (value.empty()) return false;
    int parsed = 0;
    for (unsigned char ch : value) {
        if (ch < '0' || ch > '9') return false;
        parsed = parsed * 10 + (ch - '0');
        if (parsed > 1003) return false;
    }
    id = parsed;
    return true;
}

bool SpeedTicketFlow::Start(const std::string& arguments, unsigned long now, std::string& error) {
    error.clear();
    if (IsActive()) {
        error = "Ein /speed-Ablauf ist bereits aktiv.";
        return false;
    }
    int playerId = -1;
    if (!ParsePlayerId(arguments, playerId)) {
        error = "Nutzung: /speed Spieler-ID (0 bis 1003).";
        return false;
    }
    Reset();
    playerId_ = playerId;
    stage_ = Stage::Category;
    stageStartedAt_ = now;
    return true;
}

SpeedTicketAction SpeedTicketFlow::OnDialog(const SpeedTicketDialog& dialog, unsigned long now) {
    if (const auto expired = Update(now); expired.kind != SpeedTicketAction::Kind::None) return expired;
    if (!IsActive()) return {};
    if (lastHandledDialog_ && SameDialog(*lastHandledDialog_, dialog)) return {};
    if (dialog.id < 0 || dialog.id == 65535) return Cancel("Speed-Ablauf abgebrochen: Dialog geschlossen.");
    SpeedTicketAction action;
    action.dialogId = dialog.id;
    if (stage_ == Stage::Category && IsListStyle(dialog.style)
        && Compact(dialog.caption) == "staatwantedkatalog") {
        action.row = FindUniqueRow(dialog, "ordnungswidrigkeit", false);
        if (action.row < 0) return Cancel("Speed-Ablauf abgebrochen: Ordnungswidrigkeit nicht eindeutig gefunden.");
        action.kind = SpeedTicketAction::Kind::SelectRow;
        action.input = SelectedLabel(dialog, action.row);
        stage_ = Stage::Offence;
    } else if (stage_ == Stage::Offence && IsListStyle(dialog.style)
        && Compact(dialog.caption) == "wantedsordnungswidrigkeit") {
        action.row = FindUniqueRow(dialog, "geschwindigkeitsueberschreitungab20kmh", true);
        if (action.row < 0) return Cancel("Speed-Ablauf abgebrochen: Geschwindigkeitsversto\xC3\x9F ab 20 km/h nicht eindeutig gefunden.");
        action.kind = SpeedTicketAction::Kind::SelectRow;
        action.input = SelectedLabel(dialog, action.row);
        stage_ = Stage::PlayerInput;
    } else if (stage_ == Stage::PlayerInput && IsExpectedInput(dialog)) {
        action.kind = SpeedTicketAction::Kind::SubmitInput;
        action.input = std::to_string(playerId_);
        action.notice = "Geschwindigkeitsversto\xC3\x9F f\xC3\xBCr Spieler-ID " + action.input + " abgesendet.";
        Reset();
        return action;
    } else {
        return Cancel("Speed-Ablauf abgebrochen: Unerwarteter Dialog. Bitte manuell fortfahren.");
    }
    lastHandledDialog_ = dialog;
    stageStartedAt_ = now;
    return action;
}

SpeedTicketAction SpeedTicketFlow::Update(unsigned long now, bool connected) {
    if (!IsActive()) return {};
    if (!connected) return Cancel("Speed-Ablauf abgebrochen: Verbindung getrennt.");
    if (now - stageStartedAt_ >= kStageTimeoutMs) return Cancel("Speed-Ablauf abgebrochen: Keine passende Serverantwort empfangen.");
    return {};
}

SpeedTicketAction SpeedTicketFlow::Cancel(const std::string& reason) {
    if (!IsActive()) return {};
    Reset();
    SpeedTicketAction action;
    action.kind = SpeedTicketAction::Kind::Cancelled;
    action.notice = reason;
    return action;
}

void SpeedTicketFlow::Reset() {
    stage_ = Stage::Idle;
    playerId_ = -1;
    stageStartedAt_ = 0;
    lastHandledDialog_.reset();
}

} // namespace keybinder
