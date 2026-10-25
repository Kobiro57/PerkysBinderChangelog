#include "../include/SAMPBridge.h"
#include <cstring>

namespace {
std::string JobSkillsGameText(const std::string& text) {
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
} // namespace

void SAMPBridge::UpdateStartupJobSkills(unsigned long now) {
    if (!startupStatsComplete_) return;
#ifdef KEYBINDER_TESTING
    const bool ready = GetTestingHooks().isStartupJobSkillsReady && GetTestingHooks().isStartupJobSkillsReady();
#else
    const bool ready = true;
#endif
    if (ready && !startupJobSkillsRequested_ && !speedTicketFlow_.IsActive()
        && !IsChatOpen() && !IsDialogOpen() && !IsCursorActive()) {
        startupJobSkillsRequested_ = true;
        startupJobSkillsRequestedAt_ = now;
        if (!SendCommand("/jobskills")) {
            startupJobSkillsFailed_ = true;
            QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Jobskills konnten nicht abgefragt werden. /jobskills kann manuell geöffnet werden.");
        }
    }
    if (ownJobSkillsCommandObserved_.exchange(false, std::memory_order_acq_rel)) {
        jobSkillsRequestPending_ = true;
        jobSkillsRequestAt_ = now;
    }
    std::vector<keybinder::StatsDialogEvent> dialogs;
    {
        std::lock_guard<std::mutex> lock(startupStatsMutex_);
        dialogs.swap(jobSkillsDialogs_);
    }
    // RPC capture is primary. Native text is a fallback when a local plugin
    // displays the response instead of forwarding a ShowDialog packet.
    if (jobSkillsRequestPending_ && dialogs.empty()) {
        if (const auto* dialog = GetDialog(); dialog && dialog->m_bIsActive && dialog->m_nType == 5) {
            keybinder::StatsDialogEvent event{};
            event.id = static_cast<unsigned short>(dialog->m_nId);
            event.style = static_cast<unsigned char>(dialog->m_nType);
            event.caption.assign(dialog->m_szCaption, strnlen(dialog->m_szCaption, 65));
            if (dialog->m_szText) {
                const auto size = strnlen(dialog->m_szText, 32768);
                if (size < 32768) event.text.assign(dialog->m_szText, size);
            }
            dialogs.push_back(std::move(event));
        }
    }
    if (jobSkillsRequestPending_ && now - jobSkillsRequestAt_ <= 8000 && jobTracker_) {
        for (const auto& event : dialogs) {
            if (event.style != 5) continue;
            jobs::JobSkillsSnapshot snapshot;
            std::string error;
            if (!jobs::ParseJobSkillsDialog(JobSkillsGameText(event.caption), JobSkillsGameText(event.text),
                    jobTracker_->GetRegistrySnapshot(), true, snapshot, error)) continue;
            if (!jobTracker_->ImportSynchronizedSkills(snapshot.points)) {
                QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Jobskills konnten nicht gespeichert werden.");
                break;
            }
            const bool first = !startupJobSkillsComplete_;
            startupJobSkillsComplete_ = true;
            startupJobSkillsFailed_ = false;
            jobSkillsRequestPending_ = false;
            if (first) {
                startupStatsDialogToClose_ = event;
                startupStatsDialogCloseAt_ = now;
                CloseStartupStatsDialog(now);
            }
            if (!startupSyncNotified_ && startupStatsSaved_) {
                QueueLocalMessage("{33CCFF}[NL-BINDER]{FFFFFF} Synchronisation abgeschlossen.");
                startupSyncNotified_ = true;
            }
            break;
        }
    }
    if (startupJobSkillsRequested_ && !startupJobSkillsComplete_ && !startupJobSkillsFailed_
        && now - startupJobSkillsRequestedAt_ >= 8000) {
        startupJobSkillsFailed_ = true;
        jobSkillsRequestPending_ = false;
        QueueLocalMessage("{FF6600}[NL-BINDER]{FFFFFF} Statistik übernommen, aber keine Jobskills empfangen. Eigene /jobskills erneut öffnen.");
    }
}
