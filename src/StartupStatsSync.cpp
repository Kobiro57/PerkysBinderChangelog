#include "../include/SAMPBridge.h"
#include "../include/SettingsManager.h"
#include "../include/Logger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>
#include <tuple>

namespace {
constexpr unsigned long kStatsResponseTimeoutMs = 8000;

std::string LowerStatsName(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool IsOwnStatsHeader(const std::string& line, const std::string& name) {
    static const std::regex header(R"(^\*\*\*\s*([^\s*]+)\s*\*\*\*$)");
    std::smatch result;
    return !name.empty() && std::regex_match(line, result, header)
        && LowerStatsName(result[1].str()) == LowerStatsName(name);
}

bool EqualStats(const keybinder::ServerStatsSnapshot& first, const keybinder::ServerStatsSnapshot& second) {
    return std::tie(first.playerName, first.serverAddress, first.serverPort, first.kills, first.deaths,
        first.cash, first.bank, first.wealth, first.level, first.gold, first.eventcoins,
        first.wantedPoints, first.experience, first.nextLevelExperience, first.nextPaydayMinutes)
        == std::tie(second.playerName, second.serverAddress, second.serverPort, second.kills, second.deaths,
        second.cash, second.bank, second.wealth, second.level, second.gold, second.eventcoins,
        second.wantedPoints, second.experience, second.nextLevelExperience, second.nextPaydayMinutes);
}

std::string StatsGameTextToUtf8(const char* text, std::size_t maximum) {
    if (!text) return {};
    const int size = static_cast<int>(strnlen(text, maximum));
    if (size == 0 || size == static_cast<int>(maximum)) return {};
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, size, nullptr, 0) > 0) return {text, static_cast<std::size_t>(size)};
    const int wideSize = MultiByteToWideChar(CP_ACP, 0, text, size, nullptr, 0);
    if (wideSize <= 0) return {};
    std::wstring wide(wideSize, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, size, wide.data(), wideSize);
    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, nullptr, 0, nullptr, nullptr);
    std::string result(utf8Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideSize, result.data(), utf8Size, nullptr, nullptr);
    return result;
}
} // namespace

void SAMPBridge::ResetStartupStats() {
    captureStartupStats_.store(false, std::memory_order_release);
    startupStatsConnected_ = startupStatsRequested_ = startupStatsComplete_ = startupStatsFailed_ = false;
    startupStatsIdentity_.clear();
    startupStatsCollector_.Reset();
    std::lock_guard<std::mutex> lock(startupStatsMutex_);
    startupStatsLines_.clear();
    startupStatsOwnName_.clear();
    startupStatsPendingChat_ = startupStatsHiddenChat_ = false;
}

void SAMPBridge::CaptureStartupStatsLine(const std::string& text) {
    if (!captureStartupStats_.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lock(startupStatsMutex_);
    if (startupStatsLines_.size() < 128) startupStatsLines_.push_back(text);
}

bool SAMPBridge::ShouldDisplayStartupStatsChat(const jobs::JobChatEvent& event) {
    // CChat may be called from a network callback. Only protect the small
    // capture/filter state here; parsing, persistence and notices run in Update.
    std::lock_guard<std::mutex> lock(startupStatsMutex_);
    const std::string& line = event.normalizedLine;
#ifdef KEYBINDER_TESTING
    const unsigned long now = GetTestingHooks().getTickCount ? GetTestingHooks().getTickCount() : GetTickCount();
#else
    const unsigned long now = GetTickCount();
#endif
    if (startupStatsHiddenChat_ && now - startupStatsChatHeaderAt_ >= keybinder::ServerStatsCollector::kContextTtlMs)
        startupStatsHiddenChat_ = false;
    if (line.rfind("***", 0) == 0 && !IsOwnStatsHeader(line, startupStatsOwnName_)) {
        startupStatsHiddenChat_ = false;
        return true;
    }
    if (IsOwnStatsHeader(line, startupStatsOwnName_)) {
        startupStatsHiddenChat_ = startupStatsPendingChat_;
        if (startupStatsHiddenChat_) startupStatsChatHeaderAt_ = now;
        return !startupStatsHiddenChat_;
    }
    if (!startupStatsHiddenChat_ || !keybinder::ServerStatsCollector::IsRecognizedStatsLine(line)) return true;
    // This is the last line in the observed Nova /stats chat block.
    if (line.rfind("Waffenscheine:", 0) == 0) startupStatsHiddenChat_ = false;
    return false;
}

void SAMPBridge::CompleteStartupStats(unsigned long now) {
    const bool firstCompletion = !startupStatsComplete_;
    const auto previous = synchronizedStats_;
    synchronizedStats_ = startupStatsCollector_.Snapshot();
    auto* netGame = GetNetGame();
    synchronizedStats_.serverAddress = netGame ? std::string(netGame->m_szHostAddress, strnlen(netGame->m_szHostAddress, 257)) : std::string();
    synchronizedStats_.serverPort = netGame ? netGame->m_nPort : 0;
    if (!firstCompletion && EqualStats(previous, synchronizedStats_)) return;
    // The first received account snapshot is a baseline, never a newly made
    // kill/death. Chat snapshots may already have updated these counters.
    auto* tracker = GetKillTracker();
    if (firstCompletion && !serverCombatBaselineEstablished_) {
        tracker->ImportKillCount(*synchronizedStats_.kills);
        tracker->ImportDeathCount(*synchronizedStats_.deaths);
    }
    lastServerKillTotal_ = *synchronizedStats_.kills;
    lastServerDeathTotal_ = *synchronizedStats_.deaths;
    serverCombatBaselineEstablished_ = true;
    startupStatsComplete_ = true;
    if (firstCompletion) startupStatsCompletedAt_ = now;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        startupStatsPendingChat_ = false;
    }
    const bool saved = !statsSynchronizedCallback_ || statsSynchronizedCallback_(synchronizedStats_);
    if (firstCompletion) QueueLocalMessage(saved
        ? "{33CCFF}[NL-BINDER]{FFFFFF} Synchronisation abgeschlossen."
        : "{FF6600}[NL-BINDER]{FFFFFF} Synchronisation gelesen, aber die Daten konnten nicht gespeichert werden.");
}

void SAMPBridge::UpdateStartupStats(unsigned long now) {
    const auto* netGame = GetNetGame();
    const bool connected = netGame && netGame->m_nGameState == sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED;
    if (!connected) {
        if (startupStatsConnected_) ResetStartupStats();
        return;
    }
    const std::string name = GetLocalPlayerName();
    const std::string identity = std::string(netGame->m_szHostAddress, strnlen(netGame->m_szHostAddress, 257))
        + ":" + std::to_string(netGame->m_nPort) + "/" + LowerStatsName(name);
    if (!startupStatsConnected_ || startupStatsIdentity_ != identity) {
        ResetStartupStats();
        startupStatsConnected_ = true;
        startupStatsIdentity_ = identity;
        // Received totals from a previous server/account are not a baseline
        // for this connection. Saved values remain visible until the reply.
        serverCombatBaselineEstablished_ = false;
        ownStatsContext_ = false;
    }
    if (!startupStatsRequested_) {
#ifdef KEYBINDER_TESTING
        const bool ready = GetTestingHooks().isStartupStatsReady && GetTestingHooks().isStartupStatsReady();
#else
        const bool ready = IsLocalCombatActive();
#endif
        if (!ready || name.empty() || IsChatOpen() || IsDialogOpen() || IsCursorActive()) return;
        startupStatsRequested_ = true;
        startupStatsRequestedAt_ = now;
        startupStatsCollector_.Reset();
        {
            std::lock_guard<std::mutex> lock(startupStatsMutex_);
            startupStatsLines_.clear();
            startupStatsOwnName_ = name;
            startupStatsPendingChat_ = true;
        }
        captureStartupStats_.store(true, std::memory_order_release);
        if (!SendCommand("/stats")) {
            startupStatsFailed_ = true;
            captureStartupStats_.store(false, std::memory_order_release);
            std::lock_guard<std::mutex> lock(startupStatsMutex_);
            startupStatsPendingChat_ = false;
        }
    }
    if (startupStatsFailed_) return;
    if (startupStatsComplete_ && now - startupStatsCompletedAt_ >= keybinder::ServerStatsCollector::kContextTtlMs) {
        captureStartupStats_.store(false, std::memory_order_release);
        return;
    }
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        lines.swap(startupStatsLines_);
    }
    bool finalChatLine = false;
    for (const auto& line : lines) {
        const bool ownLine = startupStatsCollector_.ConsumeLine(line, name, now);
        if (ownLine && line.rfind("Waffenscheine:", 0) == 0) finalChatLine = true;
    }
    if (auto* dialog = GetDialog(); !startupStatsComplete_ && dialog && dialog->m_bIsActive
        && dialog->m_nType == sampapi::v03dl::CDialog::DIALOG_MESSAGEBOX) {
        const int dialogId = dialog->m_nId;
        keybinder::ServerStatsCollector candidate;
        if (candidate.ConsumeDialog(StatsGameTextToUtf8(dialog->m_szCaption, 65),
                StatsGameTextToUtf8(dialog->m_szText, 32768), name, now, true) && candidate.Snapshot().HasCore()) {
            startupStatsCollector_ = std::move(candidate);
            // Only dismiss the identified own stats response, never another
            // open dialog or the login UI.
#ifdef KEYBINDER_TESTING
            if (GetTestingHooks().closeStatsDialog) GetTestingHooks().closeStatsDialog(dialogId);
#else
            if (dialog == GetDialog() && dialog->m_nId == dialogId) dialog->Close(0);
#endif
        }
    }
    if (startupStatsCollector_.Snapshot().HasCore()) {
        CompleteStartupStats(now);
        if (finalChatLine) captureStartupStats_.store(false, std::memory_order_release);
    } else if (now - startupStatsRequestedAt_ >= kStatsResponseTimeoutMs) {
        startupStatsFailed_ = true;
        captureStartupStats_.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(startupStatsMutex_);
            startupStatsPendingChat_ = startupStatsHiddenChat_ = false;
        }
        QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Synchronisation nicht abgeschlossen. /stats kann manuell erneut geöffnet werden.");
    }
}
