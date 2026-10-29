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

bool IsReadableStatsDialog(int style) {
    return style == sampapi::v03dl::CDialog::DIALOG_MESSAGEBOX
        || style == sampapi::v03dl::CDialog::DIALOG_LIST
        || style == sampapi::v03dl::CDialog::DIALOG_TABLIST
        || style == sampapi::v03dl::CDialog::DIALOG_HEADERSLIST;
}

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

std::string VisibleStatsCaption(const std::string& value) {
    static const std::regex color(R"(\{[[:xdigit:]]{6}([[:xdigit:]]{2})?\})");
    const std::string plain = std::regex_replace(value, color, "");
    const auto first = plain.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return plain.substr(first, plain.find_last_not_of(" \t\r\n") - first + 1);
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
    startupStatsDialogToClose_.reset();
    startupJobSkillsRequested_ = startupJobSkillsComplete_ = startupJobSkillsFailed_ = false;
    startupStatsSaved_ = startupSyncNotified_ = false;
    manualStateSyncCycle_ = manualStateSyncPending_ = false;
    manualStateSyncAcceptedAt_ = 0;
    manualStateSyncSessionKillsAtRequest_ = manualStateSyncSessionDeathsAtRequest_ = 0;
    jobSkillsRequestPending_ = false;
    ownJobSkillsCommandObserved_.store(false, std::memory_order_release);
    ownStatsCommandObserved_.store(false, std::memory_order_release);
    statsDialogRequestPending_ = false;
    std::lock_guard<std::mutex> lock(startupStatsMutex_);
    startupStatsLines_.clear();
    startupStatsDialogs_.clear();
    jobSkillsDialogs_.clear();
    speedTicketDialogs_.clear();
    startupStatsOwnName_.clear();
    startupStatsPendingChat_ = startupStatsHiddenChat_ = false;
}

bool SAMPBridge::RequestManualStateSynchronization(std::string& error) {
    error.clear();
    if (!initialized_) {
        error = "Der Binder ist noch nicht mit dem Spiel verbunden.";
        return false;
    }
    if (manualStateSyncPending_) {
        error = "Eine manuelle Synchronisation l\xC3\xA4uft bereits.";
        return false;
    }
    const auto* network = GetNetGame();
    const auto* player = GetLocalPlayer();
    const std::string name = GetLocalPlayerName();
#ifdef KEYBINDER_TESTING
    const bool ready = GetTestingHooks().isStartupStatsReady && GetTestingHooks().isStartupStatsReady();
    const unsigned long now = GetTestingHooks().getTickCount ? GetTestingHooks().getTickCount() : GetTickCount();
#else
    const bool ready = IsLocalCombatActive();
    const unsigned long now = GetTickCount();
#endif
    if (!ready || !network
        || network->m_nGameState != sampapi::v03dl::CNetGame::GAME_MODE_CONNECTED
        || !player || !player->m_bIsActive || player->m_bIsWasted || !player->m_pPed
        || player->m_bDoesSpectating || name.empty() || !GetInput()) {
        error = "Du musst eingeloggt und im Spiel sein, um deinen Spielstand zu synchronisieren.";
        return false;
    }
    if (manualInputOwner_ || IsChatOpen() || IsDialogOpen() || speedTicketFlow_.IsActive()) {
        error = "Bitte beende zuerst die aktuelle Eingabe oder den ge\xC3\xB6" "ffneten Dialog.";
        return false;
    }
    if ((startupStatsRequested_ && !startupStatsComplete_ && !startupStatsFailed_)
        || (startupStatsComplete_ && !startupJobSkillsComplete_ && !startupJobSkillsFailed_)) {
        error = "Die Synchronisation beim Einloggen l\xC3\xA4uft noch. Bitte warte auf ihren Abschluss.";
        return false;
    }
    // Reuse both automatic phases, retaining live counters, cached account
    // values, session events and job income until fresh responses are accepted.
    ResetStartupStats();
    startupStatsConnected_ = true;
    startupStatsIdentity_ = std::string(network->m_szHostAddress, strnlen(network->m_szHostAddress, 257))
        + ":" + std::to_string(network->m_nPort) + "/" + LowerStatsName(name);
    manualStateSyncCycle_ = manualStateSyncPending_ = true;
    manualStateSyncAcceptedAt_ = now;
    manualStateSyncSessionKillsAtRequest_ = GetKillTracker()->GetSessionKills();
    manualStateSyncSessionDeathsAtRequest_ = GetKillTracker()->GetSessionDeaths();
    ownStatsContext_ = false;
    return true;
}

void SAMPBridge::FailManualStateSynchronization(const std::string& reason) {
    if (!manualStateSyncPending_) return;
    manualStateSyncPending_ = false;
    // End this requested cycle; a late dialog must never announce success for
    // it, nor start an unrequested second phase after the button is available.
    startupSyncNotified_ = true;
    startupStatsRequested_ = startupJobSkillsRequested_ = true;
    if (!startupStatsComplete_) startupStatsFailed_ = true;
    if (!startupJobSkillsComplete_) startupJobSkillsFailed_ = true;
    statsDialogRequestPending_ = jobSkillsRequestPending_ = false;
    QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Manuelle Synchronisation nicht abgeschlossen: " + reason);
}

void SAMPBridge::ObserveStatsCommand(const std::string& cmd) const {
    const auto first = cmd.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    const std::string command = LowerStatsName(cmd.substr(first, cmd.find_last_not_of(" \t\r\n") - first + 1));
    if (command == "/stats") ownStatsCommandObserved_.store(true, std::memory_order_release);
    if (command == "/jobskills") ownJobSkillsCommandObserved_.store(true, std::memory_order_release);
}

void SAMPBridge::InitializeStartupStatsAdapter() {
    if (!statsDialogAdapter_) statsDialogAdapter_ = std::make_unique<keybinder::StatsDialogRPCAdapter>();
    statsDialogAdapter_->SetDialogCallback([this](const keybinder::StatsDialogEvent& event) {
        CaptureStartupStatsDialog(event);
    });
    statsDialogAdapter_->Initialize();
}

void SAMPBridge::CaptureStartupStatsDialog(const keybinder::StatsDialogEvent& event) {
    std::lock_guard<std::mutex> lock(startupStatsMutex_);
    if (startupStatsDialogs_.size() < 8) startupStatsDialogs_.push_back(event);
    if (captureSpeedDialogs_.load(std::memory_order_acquire) && speedTicketDialogs_.size() < 8) speedTicketDialogs_.push_back(event);
    if (jobSkillsDialogs_.size() < 8) jobSkillsDialogs_.push_back(event);
}

#ifdef KEYBINDER_TESTING
void SAMPBridge::ProcessStatsDialogForTesting(const keybinder::StatsDialogEvent& event) {
    CaptureStartupStatsDialog(event);
}
bool SAMPBridge::ProcessStatsDialogRpcForTesting(RakNet::BitStream* payload) {
    return statsDialogAdapter_ && statsDialogAdapter_->ProcessRpcForTesting(61, payload);
}
#endif

bool SAMPBridge::AcceptStartupStatsDialog(const keybinder::StatsDialogEvent& event, unsigned long now) {
    if (!IsReadableStatsDialog(event.style) || !statsDialogRequestPending_
        || now - statsDialogRequestAt_ > kStatsResponseTimeoutMs) return false;
    keybinder::ServerStatsCollector candidate;
    if (!candidate.ConsumeDialog(StatsGameTextToUtf8(event.caption.c_str(), 256),
            StatsGameTextToUtf8(event.text.c_str(), 32768), GetLocalPlayerName(), now, true)
        || !candidate.Snapshot().HasCounters()) return false;
    startupStatsCollector_ = std::move(candidate);
    statsDialogRequestPending_ = false;
    // A subsequent manually opened stats window updates the saved values too,
    // but remains open once the startup synchronisation has completed.
    if (!startupStatsComplete_) {
        startupStatsDialogToClose_ = event;
        startupStatsDialogCloseAt_ = now;
    }
    return true;
}

void SAMPBridge::CloseStartupStatsDialog(unsigned long now) {
    if (!startupStatsDialogToClose_) return;
    if (now - startupStatsDialogCloseAt_ > 1000) {
        startupStatsDialogToClose_.reset();
        return;
    }
    auto* dialog = GetDialog();
    const auto& expected = *startupStatsDialogToClose_;
    const std::string caption = dialog ? VisibleStatsCaption(StatsGameTextToUtf8(dialog->m_szCaption, 65)) : std::string();
    const std::string expectedPrefix = expected.caption.substr(0, 64);
    if (!dialog || !dialog->m_bIsActive || dialog->m_nId != expected.id
        || dialog->m_nType != expected.style
        || (caption != VisibleStatsCaption(StatsGameTextToUtf8(expected.caption.c_str(), 256))
            && caption != VisibleStatsCaption(StatsGameTextToUtf8(expectedPrefix.c_str(), 65)))) return;
    const int dialogId = dialog->m_nId;
    startupStatsDialogToClose_.reset();
#ifdef KEYBINDER_TESTING
    if (GetTestingHooks().closeStatsDialog) GetTestingHooks().closeStatsDialog(dialogId);
#else
    dialog->Close(0);
#endif
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
    const bool changedCounters = !previous.HasCounters() || previous.kills != synchronizedStats_.kills
        || previous.deaths != synchronizedStats_.deaths;
    if (firstCompletion || changedCounters) {
        const auto wallNow = std::time(nullptr);
        // A refresh can correct old cached totals immediately. A real event
        // confirmed after the request still gets the existing short protection
        // against an earlier snapshot arriving from the server.
        const bool manualKillsOverride = manualStateSyncPending_
            && tracker->GetSessionKills() == manualStateSyncSessionKillsAtRequest_;
        const bool manualDeathsOverride = manualStateSyncPending_
            && tracker->GetSessionDeaths() == manualStateSyncSessionDeathsAtRequest_;
        if (manualKillsOverride || !serverCombatBaselineEstablished_ || *synchronizedStats_.kills >= tracker->GetTotalKills()
            || wallNow - lastLocalKillTime_ > 5) tracker->ImportKillCount(*synchronizedStats_.kills);
        if (manualDeathsOverride || !serverCombatBaselineEstablished_ || *synchronizedStats_.deaths >= tracker->GetTotalDeaths()
            || wallNow - lastLocalDeathTime_ > 5) tracker->ImportDeathCount(*synchronizedStats_.deaths);
    }
    lastServerKillTotal_ = *synchronizedStats_.kills;
    lastServerDeathTotal_ = *synchronizedStats_.deaths;
    serverCombatBaselineEstablished_ = true;
    startupStatsComplete_ = true;
    startupStatsFailed_ = false;
    if (firstCompletion) startupStatsCompletedAt_ = now;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        startupStatsPendingChat_ = false;
    }
    const bool saved = !statsSynchronizedCallback_ || statsSynchronizedCallback_(synchronizedStats_);
    startupStatsSaved_ = saved;
    if (firstCompletion) {
        if (manualStateSyncPending_) {
            if (!saved) FailManualStateSynchronization("Die Statistikwerte konnten nicht gespeichert werden.");
        } else if (!manualStateSyncCycle_) {
            QueueLocalMessage(saved
                ? "{33CCFF}[NL-BINDER]{FFFFFF} Statistik übernommen."
                : "{FF6600}[NL-BINDER]{FFFFFF} Synchronisation gelesen, aber die Daten konnten nicht gespeichert werden.");
        }
    }
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
        if (manualStateSyncPending_ && now - manualStateSyncAcceptedAt_ >= kStatsResponseTimeoutMs) {
            FailManualStateSynchronization("Der Statistikabruf konnte nicht gestartet werden.");
            return;
        }
#ifdef KEYBINDER_TESTING
        const bool ready = GetTestingHooks().isStartupStatsReady && GetTestingHooks().isStartupStatsReady();
#else
        const bool ready = IsLocalCombatActive();
#endif
        if (!ready || name.empty() || IsChatOpen() || IsDialogOpen() || IsCursorActive() || speedTicketFlow_.IsActive()) return;
        startupStatsRequested_ = true;
        startupStatsRequestedAt_ = now;
        startupStatsCollector_.Reset();
        {
            std::lock_guard<std::mutex> lock(startupStatsMutex_);
            startupStatsLines_.clear();
            startupStatsDialogs_.clear(); // Discard dialogs queued before this request.
            startupStatsOwnName_ = name;
            startupStatsPendingChat_ = true;
        }
        captureStartupStats_.store(true, std::memory_order_release);
        if (!SendCommand("/stats")) {
            startupStatsFailed_ = true;
            {
                std::lock_guard<std::mutex> lock(startupStatsMutex_);
                startupStatsPendingChat_ = false;
            }
            if (manualStateSyncPending_) FailManualStateSynchronization("Die Statistik konnte nicht abgefragt werden.");
            else QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Automatischer Statistikabruf fehlgeschlagen. Eigene /stats erneut öffnen.");
        }
    }
    if (ownStatsCommandObserved_.exchange(false, std::memory_order_acq_rel)) {
        statsDialogRequestPending_ = true;
        statsDialogRequestAt_ = now;
    }
    // Keep accepting actual own statistics after a timeout. Reopening /stats
    // manually is a recovery path, not another automatic request loop.
    std::vector<std::string> lines;
    std::vector<keybinder::StatsDialogEvent> dialogs;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        lines.swap(startupStatsLines_);
        dialogs.swap(startupStatsDialogs_);
    }
    for (const auto& line : lines) {
        startupStatsCollector_.ConsumeLine(line, name, now);
    }
    for (const auto& event : dialogs) AcceptStartupStatsDialog(event, now);
    // Native polling remains a fallback. Lists and tab lists must not be
    // excluded: /stats is a dialog on Nova, whereas /oldstats is a chat block.
    if (auto* dialog = GetDialog(); !startupStatsComplete_ && dialog && dialog->m_bIsActive
        && IsReadableStatsDialog(dialog->m_nType) && !startupStatsDialogToClose_) {
        keybinder::StatsDialogEvent event{};
        event.id = static_cast<unsigned short>(dialog->m_nId);
        event.style = static_cast<unsigned char>(dialog->m_nType);
        event.caption = StatsGameTextToUtf8(dialog->m_szCaption, 65);
        event.text = StatsGameTextToUtf8(dialog->m_szText, 32768);
        AcceptStartupStatsDialog(event, now);
    }
    CloseStartupStatsDialog(now);
    if (startupStatsCollector_.Snapshot().HasCounters()) {
        CompleteStartupStats(now);
    } else if (!startupStatsComplete_ && !startupStatsFailed_ && now - startupStatsRequestedAt_ >= kStatsResponseTimeoutMs) {
        startupStatsFailed_ = true;
        statsDialogRequestPending_ = false;
        {
            std::lock_guard<std::mutex> lock(startupStatsMutex_);
            startupStatsPendingChat_ = startupStatsHiddenChat_ = false;
        }
        if (manualStateSyncPending_) FailManualStateSynchronization("Keine eigenen Statistikwerte empfangen.");
        else QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Keine eigenen Zähler empfangen. Eigene /stats erneut öffnen, um die Werte zu übernehmen.");
    }
}
