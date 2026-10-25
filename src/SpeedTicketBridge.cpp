#include "../include/SAMPBridge.h"
#include "../include/RakHook/include/RakNet/BitStream.h"
#ifndef KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/rakhook.hpp"
#endif
#include <algorithm>
#include <cstring>
#include <regex>

namespace {
std::string SpeedGameText(const std::string& text) {
    if (text.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0) > 0) return text;
    const int count = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
    std::string output(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, output.data(), size, nullptr, nullptr);
    return output;
}
std::string SpeedVisibleCaption(const std::string& text) {
    static const std::regex color(R"(\{[[:xdigit:]]{6}([[:xdigit:]]{2})?\})");
    return std::regex_replace(SpeedGameText(text), color, "");
}
std::string SpeedToGameText(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return text;
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
    const int size = WideCharToMultiByte(CP_ACP, 0, wide.data(), count, nullptr, 0, nullptr, nullptr);
    std::string output(size, '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.data(), count, output.data(), size, nullptr, nullptr);
    return output;
}
} // namespace

bool SAMPBridge::RequestSpeedTicket(const std::string& arguments, std::string& error) {
    const auto* network = GetNetGame();
    if (!initialized_ || !network || network->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED) {
        error = "Du bist noch nicht mit dem Server verbunden.";
        return false;
    }
    if (IsDialogOpen()) { error = "Bitte zuerst den offenen Dialog schließen."; return false; }
#ifdef KEYBINDER_TESTING
    const auto now = GetTestingHooks().getTickCount ? GetTestingHooks().getTickCount() : GetTickCount();
#else
    const auto now = GetTickCount();
#endif
    if (!speedTicketFlow_.Start(arguments, now, error)) return false;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        speedTicketDialogs_.clear();
    }
    pendingSpeedDialogs_.clear();
    captureSpeedDialogs_.store(true, std::memory_order_release);
    if (SendCommand("/su")) return true;
    captureSpeedDialogs_.store(false, std::memory_order_release);
    speedTicketFlow_.Reset();
    error = "Der Wantedkatalog konnte nicht geöffnet werden.";
    return false;
}

bool SAMPBridge::SendSpeedDialogChoice(const keybinder::SpeedTicketAction& action) {
    if (action.dialogId < 0 || action.dialogId > 32767 || action.row < -1 || action.row > 32767) return false;
    const std::string text = SpeedToGameText(action.input);
    if (text.size() > 255 || text.find('\0') != std::string::npos) return false;
    // open.mp Shared/NetCode/dialog.hpp: RPC 62 contains INT16 id, UINT8
    // response, INT16 list item (-1 for input), then a byte-length string.
    RakNet::BitStream payload;
    payload.Write(static_cast<short>(action.dialogId));
    payload.Write(static_cast<unsigned char>(1));
    payload.Write(static_cast<short>(action.row));
    payload.Write(static_cast<unsigned char>(text.size()));
    if (!text.empty()) payload.Write(text.data(), static_cast<int>(text.size()));
#ifdef KEYBINDER_TESTING
    const auto& hooks = GetTestingHooks();
    const bool sent = hooks.sendDialogResponse && hooks.sendDialogResponse(&payload);
    if (sent && hooks.hideDialog) hooks.hideDialog(action.dialogId);
#else
    const bool sent = rakhook::initialized && rakhook::send_rpc(62, &payload, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false);
    if (sent) { if (auto* dialog = GetDialog(); dialog && dialog->m_bIsActive && dialog->m_nId == action.dialogId) dialog->Hide(); }
#endif
    return sent;
}

void SAMPBridge::UpdateSpeedTicketFlow(unsigned long now) {
    const auto* network = GetNetGame();
    const bool connected = network && network->m_nGameState == sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    auto notice = speedTicketFlow_.Update(now, connected);
    if (notice.kind == keybinder::SpeedTicketAction::Kind::Cancelled)
        QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} " + notice.notice);
    std::vector<keybinder::StatsDialogEvent> received;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        received.swap(speedTicketDialogs_);
    }
    for (auto& event : received) if (pendingSpeedDialogs_.size() < 8) pendingSpeedDialogs_.push_back(std::move(event));
    while (speedTicketFlow_.IsActive() && !pendingSpeedDialogs_.empty()) {
        const auto* dialog = GetDialog();
        if (!dialog || !dialog->m_bIsActive) break; // original ShowDialog handler has not displayed it yet
        const auto event = pendingSpeedDialogs_.front();
        pendingSpeedDialogs_.erase(pendingSpeedDialogs_.begin());
        if (dialog->m_nId != event.id || dialog->m_nType != event.style) continue; // stale replaced window
        const std::string caption(dialog->m_szCaption, strnlen(dialog->m_szCaption, 65));
        if (SpeedVisibleCaption(caption) != SpeedVisibleCaption(event.caption.substr(0, 64))) {
            notice = speedTicketFlow_.Cancel("/speed abgebrochen: Der angezeigte Dialog hat sich geändert.");
        } else {
            notice = speedTicketFlow_.OnDialog({event.id, event.style, SpeedGameText(event.caption), SpeedGameText(event.text)}, now);
        }
        if (notice.kind == keybinder::SpeedTicketAction::Kind::Cancelled) {
            QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} " + notice.notice);
            break;
        }
        if (notice.kind == keybinder::SpeedTicketAction::Kind::SelectRow || notice.kind == keybinder::SpeedTicketAction::Kind::SubmitInput) {
            if (!SendSpeedDialogChoice(notice)) {
                speedTicketFlow_.Reset();
                QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} /speed abgebrochen: Die Dialogantwort konnte nicht gesendet werden.");
                break;
            }
        }
    }
    if (!speedTicketFlow_.IsActive()) {
        captureSpeedDialogs_.store(false, std::memory_order_release);
        pendingSpeedDialogs_.clear();
    }
}
