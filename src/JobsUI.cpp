#include "../include/JobsUI.h"
#include "../include/SAMPBridge.h"
#include "../include/JobTracker.h"
#include "../include/UIComponents.h"
#include "../include/ThemeManager.h"
#include "../include/AnimationHelper.h"

#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace keybinder {

namespace {

constexpr const char* kIconEdit = "\xEE\x8F\x89";

static inline float GetLayoutScale() {
    return ThemeManager::Instance().GetAppliedScale();
}

static inline float Scale(float value) {
    return value * GetLayoutScale();
}

} // anonymous namespace

JobsUI::JobsUI()
    : bridge_(nullptr)
    , selectedJobIndex_(-1)
    , defaultSelectionResolved_(false) {
}

std::string JobsUI::FormatMoney(int amount) {
    char buf[64] = {};
    std::snprintf(buf, sizeof(buf), "%d $", amount);
    return buf;
}

std::string JobsUI::FormatTimestamp(std::time_t ts) {
    if (ts <= 0) return "-";
    char buf[32] = {};
    struct tm tmbuf;
    localtime_s(&tmbuf, &ts);
    std::strftime(buf, sizeof(buf), "%d.%m %H:%M", &tmbuf);
    return buf;
}

std::string JobsUI::FormatEventTypeBadge(const std::string& type) {
    if (type == "earning" || type == "manual_earning")      return "Verdienst";
    if (type == "skill_level" || type == "skill_level_absolute" || type == "manual_skill_level")  return "Level";
    if (type == "job_message") return "Info";
    if (type == "correction")   return "Korrektur";
    return type;
}

std::string JobsUI::FormatSourceBadge(const std::string& source) {
    if (source == "auto" || source == "chat_auto")     return "Auto";
    if (source == "manual" || source == "manual_add")   return "Manuell";
    return source;
}

std::string JobsUI::ValidateCorrection(int typeIndex, int amountOrLevel, const std::string& jobId) {
    if (jobId.empty()) {
        return "Kein Job ausgewählt.";
    }

    if (typeIndex == 0) {
        if (amountOrLevel < 0) {
            return "Negativer Betrag nicht erlaubt.";
        }
    } else {
        if (amountOrLevel < 0) {
            return "Negatives Level nicht erlaubt.";
        }
    }

    return {};
}

jobs::JobTracker* JobsUI::ResolveJobTracker() const {
    if (testTracker_ != nullptr) {
        return testTracker_;
    }

    return bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
}

void JobsUI::OpenEditModal(const jobs::JobId& jobId) {
    editModalState_.open = true;
    editModalState_.openCorrectionFromEdit = false;
    editModalState_.validationError.clear();

    const bool needsReload = !editModalState_.initialized || editModalState_.loadedJobId != jobId;
    if (needsReload) {
        editModalState_.initialized = false;
        LoadEditModalSnapshot(jobId);
    }
}

void JobsUI::CloseEditModal(bool preserveDraft) {
    editModalState_.open = false;
    editModalState_.openCorrectionFromEdit = false;

    if (!preserveDraft) {
        editModalState_.initialized = false;
        editModalState_.loadedJobId.clear();
        editModalState_.baseLifetime = 0;
        editModalState_.baseHighestLevel = 0;
        editModalState_.currentEarningsOffset = 0;
        editModalState_.currentSkillOffset = 0;
        editModalState_.editedTotalEarnings = 0;
        editModalState_.editedTotalSkillPoints = 0;
        editModalState_.validationError.clear();
    }
}

void JobsUI::LoadEditModalSnapshot(const jobs::JobId& jobId) {
    editModalState_.loadedJobId = jobId;
    editModalState_.validationError.clear();

    auto* tracker = ResolveJobTracker();
    if (tracker == nullptr || !tracker->IsInitialized()) {
        editModalState_.baseLifetime = 0;
        editModalState_.baseHighestLevel = 0;
        editModalState_.currentEarningsOffset = 0;
        editModalState_.currentSkillOffset = 0;
        editModalState_.editedTotalEarnings = 0;
        editModalState_.editedTotalSkillPoints = 0;
        editModalState_.initialized = true;
        return;
    }

    const jobs::JobEditSnapshot snapshot = tracker->GetJobEditSnapshot(jobId);
    editModalState_.baseLifetime = snapshot.baseTotalEarnings;
    editModalState_.baseHighestLevel = snapshot.baseHighestSkill;
    editModalState_.currentEarningsOffset = snapshot.earningsOffset;
    editModalState_.currentSkillOffset = snapshot.skillOffset;
    editModalState_.editedTotalEarnings = snapshot.adjustedTotalEarnings;
    editModalState_.editedTotalSkillPoints = snapshot.adjustedHighestSkill;
    editModalState_.initialized = true;
}

void JobsUI::ApplyEditModalChanges() {
    auto* tracker = ResolveJobTracker();
    if (tracker == nullptr || !tracker->IsInitialized()) {
        editModalState_.validationError = "Tracker nicht verfügbar.";
        return;
    }

    if (editModalState_.editedTotalEarnings < 0) {
        editModalState_.validationError = "Verdienst darf nicht negativ sein.";
        return;
    }

    if (editModalState_.editedTotalSkillPoints < 0) {
        editModalState_.validationError = "Skill-Punkte dürfen nicht negativ sein.";
        return;
    }

    const int earningsOffset = editModalState_.editedTotalEarnings - editModalState_.baseLifetime;
    const int skillOffset = editModalState_.editedTotalSkillPoints - editModalState_.baseHighestLevel;

    tracker->ApplyJobSummaryAdjustments(editModalState_.loadedJobId, earningsOffset, skillOffset);

    editModalState_.validationError.clear();
    editModalState_.initialized = false;
    editModalState_.open = false;
}

void JobsUI::HandleCorrectionModalClosed() {
    modalState_.open = false;
    modalState_.validationError.clear();
    modalState_.note.clear();
    modalState_.timestampEdited = false;

    if (editModalState_.openCorrectionFromEdit) {
        editModalState_.openCorrectionFromEdit = false;
        editModalState_.open = true;
        editModalState_.initialized = false;
    }
}

#ifdef KEYBINDER_TESTING
void JobsUI::DebugOpenEditModal(const jobs::JobId& jobId) {
    OpenEditModal(jobId);
}

void JobsUI::DebugApplyEditModalChanges() {
    ApplyEditModalChanges();
}

void JobsUI::DebugCancelEditModal() {
    CloseEditModal(false);
}

void JobsUI::DebugHandleCorrectionModalClosed() {
    HandleCorrectionModalClosed();
}
#endif

void JobsUI::OpenCorrectionModal() {
    modalState_.open = true;
    modalState_.typeIndex = 0;
    modalState_.amountOrLevel = 0;
    modalState_.note.clear();
    modalState_.validationError.clear();
    modalState_.timestampEdited = false;
    InitializeModalTimestamp();
}

void JobsUI::InitializeModalTimestamp() {
    std::time_t now = std::time(nullptr);
    struct tm localTm;
    localtime_s(&localTm, &now);
    modalState_.tsDay = localTm.tm_mday;
    modalState_.tsMonth = localTm.tm_mon + 1;
    modalState_.tsYear = localTm.tm_year + 1900;
    modalState_.tsHour = localTm.tm_hour;
    modalState_.tsMinute = localTm.tm_min;
    modalState_.timestamp = now;
}

void JobsUI::RenderEditModal(const jobs::JobId& jobId) {
    if (!editModalState_.open) return;

    auto* tracker = ResolveJobTracker();
    auto& theme = ThemeManager::Instance();

    if (!editModalState_.initialized || editModalState_.loadedJobId != jobId) {
        LoadEditModalSnapshot(jobId);
    }

    ImGui::OpenPopup("Bearbeiten##JobsEdit");

    if (ImGui::BeginPopupModal("Bearbeiten##JobsEdit", &editModalState_.open,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

        UIComponents::CardHeader(jobId.c_str(), "Werte bearbeiten");
        UIComponents::Separator();

        if (tracker != nullptr && tracker->IsInitialized()) {
            UIComponents::InputInt("Gesamtverdienst ($)", &editModalState_.editedTotalEarnings, "0", Scale(200.0f));
            ImGui::Spacing();
            UIComponents::InputInt("Skill-Punkte", &editModalState_.editedTotalSkillPoints, "0", Scale(200.0f));
            ImGui::Spacing();

            jobs::JobStats stats;
            tracker->GetJobStats(jobId, stats);
            UIComponents::InfoRow("Ereignisse", std::to_string(stats.totalEvents).c_str());
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
            char baseInfo[128];
            std::snprintf(baseInfo, sizeof(baseInfo), "Basis: %s | Skill: %d",
                FormatMoney(editModalState_.baseLifetime).c_str(),
                editModalState_.baseHighestLevel);
            ImGui::TextUnformatted(baseInfo);
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
            ImGui::TextWrapped("Tracker nicht verfügbar.");
            ImGui::PopStyleColor();
        }

        if (!editModalState_.validationError.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Danger));
            ImGui::TextWrapped("%s", editModalState_.validationError.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        UIComponents::Separator();
        ImGui::Spacing();

        if (UIComponents::PrimaryButton("Speichern", ImVec2(Scale(120.0f), 0.0f))) {
            ApplyEditModalChanges();
        }
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(Scale(120.0f), 0.0f))) {
            CloseEditModal(false);
        }

        ImGui::Spacing();

        if (UIComponents::GhostButton("+ Eintrag hinzufügen", ImVec2(Scale(200.0f), 0.0f))) {
            editModalState_.open = false;
            editModalState_.openCorrectionFromEdit = true;
            OpenCorrectionModal();
        }

        ImGui::EndPopup();
    } else {
        if (!editModalState_.openCorrectionFromEdit) {
            CloseEditModal(false);
        }
    }
}

void JobsUI::RenderCorrectionModal(const jobs::JobId& jobId) {
    if (!modalState_.open) return;

    auto& theme = ThemeManager::Instance();

    ImGui::OpenPopup("Eintrag hinzufügen##JobsCorrection");

    if (ImGui::BeginPopupModal("Eintrag hinzufügen##JobsCorrection", &modalState_.open,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

        UIComponents::CardHeader("Neuen Eintrag manuell hinzufügen", "Der Eintrag wird als 'Manuell' markiert.");
        UIComponents::Separator();

        const char* typeItems[] = { "Einnahme", "Skill-Level" };
        UIComponents::Combo("Typ", &modalState_.typeIndex, typeItems, 2, Scale(200.0f));

        ImGui::Spacing();

        const char* amountLabel = (modalState_.typeIndex == 0) ? "Betrag ($)" : "Level";
        UIComponents::InputInt(amountLabel, &modalState_.amountOrLevel, "0", Scale(200.0f));

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextUnformatted("Zeitpunkt:");
        ImGui::PopStyleColor();

        ImGui::PushItemWidth(Scale(60.0f));
        ImGui::InputInt("##TsDay", &modalState_.tsDay, 0);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 2);
        ImGui::TextUnformatted(".");
        ImGui::SameLine(0, 2);
        ImGui::PushItemWidth(Scale(60.0f));
        ImGui::InputInt("##TsMonth", &modalState_.tsMonth, 0);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 2);
        ImGui::TextUnformatted(".");
        ImGui::SameLine(0, 2);
        ImGui::PushItemWidth(Scale(80.0f));
        ImGui::InputInt("##TsYear", &modalState_.tsYear, 0);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 8);
        ImGui::PushItemWidth(Scale(50.0f));
        ImGui::InputInt("##TsHour", &modalState_.tsHour, 0);
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 2);
        ImGui::TextUnformatted(":");
        ImGui::SameLine(0, 2);
        ImGui::PushItemWidth(Scale(50.0f));
        ImGui::InputInt("##TsMinute", &modalState_.tsMinute, 0);
        ImGui::PopItemWidth();

        modalState_.timestampEdited = true;

        ImGui::Spacing();

        UIComponents::InputText("Notiz", &modalState_.note, "z.B. Korrektur, Nachtrag...", Scale(200.0f));

        ImGui::Spacing();

        UIComponents::InfoRow("Job", jobId.c_str());

        ImGui::Spacing();

        if (!modalState_.validationError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Danger));
            ImGui::TextWrapped("%s", modalState_.validationError.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (UIComponents::PrimaryButton("Hinzufügen", ImVec2(Scale(140.0f), 0.0f))) {
            SubmitCorrection(jobId);
        }
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(Scale(120.0f), 0.0f))) {
            HandleCorrectionModalClosed();
        }

        ImGui::EndPopup();
    } else {
        HandleCorrectionModalClosed();
    }
}

void JobsUI::SubmitCorrection(const jobs::JobId& jobId) {
    std::string error = ValidateCorrection(modalState_.typeIndex, modalState_.amountOrLevel, jobId);
    if (!error.empty()) {
        modalState_.validationError = error;
        return;
    }

    struct tm localTm = {};
    localTm.tm_year = modalState_.tsYear - 1900;
    localTm.tm_mon = modalState_.tsMonth - 1;
    localTm.tm_mday = modalState_.tsDay;
    localTm.tm_hour = modalState_.tsHour;
    localTm.tm_min = modalState_.tsMinute;
    localTm.tm_sec = 0;
    localTm.tm_isdst = -1;
    std::time_t ts = std::mktime(&localTm);
    if (ts == static_cast<std::time_t>(-1)) {
        ts = std::time(nullptr);
    }

    auto* tracker = ResolveJobTracker();
    if (tracker == nullptr || !tracker->IsInitialized()) {
        modalState_.validationError = "Tracker nicht verfügbar.";
        return;
    }

    std::string eventType = (modalState_.typeIndex == 0) ? "manual_earning" : "manual_skill_level";
    int amount = (modalState_.typeIndex == 0) ? modalState_.amountOrLevel : 0;
    int level = (modalState_.typeIndex == 1) ? modalState_.amountOrLevel : 0;

    std::string eventId = tracker->AddManualEvent(jobId, eventType, amount, level, ts, modalState_.note);
    if (eventId.empty()) {
        modalState_.validationError = "Eintrag konnte nicht gespeichert werden.";
        return;
    }

    HandleCorrectionModalClosed();
}

void JobsUI::RenderDeleteConfirm() {
    if (!deleteConfirm_.open) return;

    auto& theme = ThemeManager::Instance();

    ImGui::OpenPopup("Ereignis löschen?##JobsDeleteEvent");

    if (ImGui::BeginPopupModal("Ereignis löschen?##JobsDeleteEvent", &deleteConfirm_.open,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

        ImGui::Text("Möchtest du dieses Ereignis wirklich löschen?");
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Warning));
        ImGui::TextWrapped("Das Ereignis wird ausgeblendet und nicht mehr in Zusammenfassungen gezählt.");
        ImGui::PopStyleColor();
        UIComponents::Separator();

        if (UIComponents::DangerButton("Ja, löschen", ImVec2(Scale(120.0f), 0.0f))) {
            auto* tracker = bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
            if (tracker != nullptr && tracker->IsInitialized()) {
                tracker->DeleteEvent(deleteConfirm_.eventId);
            }
            deleteConfirm_.open = false;
            deleteConfirm_.eventId.clear();
            deleteConfirm_.eventIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (UIComponents::SecondaryButton("Abbrechen", ImVec2(Scale(120.0f), 0.0f))) {
            deleteConfirm_.open = false;
            deleteConfirm_.eventId.clear();
            deleteConfirm_.eventIndex = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        deleteConfirm_.open = false;
    }
}

void JobsUI::OpenDeleteConfirm(const std::string& eventId, int eventIndex) {
    deleteConfirm_.open = true;
    deleteConfirm_.eventId = eventId;
    deleteConfirm_.eventIndex = eventIndex;
}

void JobsUI::ResolveDefaultSelection() {
    if (defaultSelectionResolved_) return;

    auto* tracker = bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
    if (tracker == nullptr || !tracker->IsInitialized()) return;

    const auto& jobs = tracker->GetJobs();
    if (jobs.empty()) return;

    for (int i = 0; i < static_cast<int>(jobs.size()); ++i) {
        if (jobs[i].enabled) {
            selectedJobIndex_ = i;
            break;
        }
    }

    defaultSelectionResolved_ = true;
}

void JobsUI::Render() {
    auto& theme = ThemeManager::Instance();

    AnimationHelper::Update(ImGui::GetIO().DeltaTime);

    ResolveDefaultSelection();

    RenderSummaryCards();

    ImGui::Dummy(ImVec2(0.0f, theme.GetSpacing(ThemeSpacingRole::CardGap)));

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float paneGap = theme.GetSpacing(ThemeSpacingRole::CardGap);
    const float preferredLeftPaneWidth = Scale(330.0f);
    const float minLeftPaneWidth = Scale(260.0f);
    const float minRightPaneWidth = Scale(360.0f);

    float scaledLeftPaneWidth = preferredLeftPaneWidth;
    const float maxLeftPaneWidth = availableWidth - minRightPaneWidth - paneGap;
    if (maxLeftPaneWidth > minLeftPaneWidth) {
        scaledLeftPaneWidth = (std::min)((std::max)(preferredLeftPaneWidth, minLeftPaneWidth), maxLeftPaneWidth);
    } else {
        scaledLeftPaneWidth = (std::max)((availableWidth - paneGap) * 0.34f, Scale(230.0f));
    }

    const float rightPaneWidth = availableWidth - scaledLeftPaneWidth - paneGap;

    UIComponents::BeginCardFillHeight("JobOverviewPane", scaledLeftPaneWidth, false);
    RenderJobOverview();
    UIComponents::EndCard();

    ImGui::SameLine(0.0f, paneGap);

    UIComponents::BeginCardFillHeight("JobDetailPane", rightPaneWidth, selectedJobIndex_ >= 0);
    RenderJobDetail();
    UIComponents::EndCard();
}

void JobsUI::RenderSummaryCards() {
    auto* tracker = bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
    auto& theme = ThemeManager::Instance();

    int todayTotal = 0;
    int last7DaysTotal = 0;
    int last30DaysTotal = 0;
    int bestDayTotal = 0;

    if (tracker != nullptr && tracker->IsInitialized()) {
        const auto& jobs = tracker->GetJobs();
        for (const auto& job : jobs) {
            jobs::JobWindowAggregates agg = tracker->GetAggregates(job.id);
            todayTotal += agg.today;
            last7DaysTotal += agg.last7Days;
            last30DaysTotal += agg.last30Days;
        }
        bestDayTotal = tracker->GetGlobalBestDay();
    }

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float metricWidth = (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f;
    const ImVec4 accentColor = theme.GetColor(ThemeColorRole::Accent);

    UIComponents::MetricCard("JobsTodayCard", "Heute", FormatMoney(todayTotal), "", &accentColor, nullptr, metricWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("Jobs7dCard", "7 Tage", FormatMoney(last7DaysTotal), "", nullptr, nullptr, metricWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("Jobs30dCard", "30 Tage", FormatMoney(last30DaysTotal), "", nullptr, nullptr, metricWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("JobsBestDayCard", "Bester Tag", FormatMoney(bestDayTotal), "", nullptr, nullptr, metricWidth);
}

void JobsUI::RenderJobOverview() {
    auto* tracker = bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
    auto& theme = ThemeManager::Instance();

    UIComponents::CardHeader("Job-Übersicht", "Wähle einen Job für Details");

    if (tracker == nullptr || !tracker->IsInitialized()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Job-Tracker nicht verfügbar. Verbinde dich mit einem Server, um Job-Daten zu laden.");
        ImGui::PopStyleColor();
        return;
    }

    const auto& jobs = tracker->GetJobs();
    if (jobs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Keine Jobs verfügbar");
        ImGui::PopStyleColor();
        return;
    }

    for (int i = 0; i < static_cast<int>(jobs.size()); ++i) {
        const auto& job = jobs[i];
        jobs::JobWindowAggregates agg = tracker->GetAggregates(job.id);
        jobs::JobStats stats;
        tracker->GetJobStats(job.id, stats);

        ImGui::PushID(i);
        RenderOverviewRow(job, i, agg, stats);
        ImGui::PopID();
    }
}

void JobsUI::RenderOverviewRow(const jobs::JobDefinition& job, int index,
                                const jobs::JobWindowAggregates& agg,
                                const jobs::JobStats& stats) {
    auto& theme = ThemeManager::Instance();
    const bool isSelected = (selectedJobIndex_ == index);
    const bool isDisabled = !job.enabled;

    if (isDisabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextDisabled));
    }

    char labelBuf[128] = {};
    std::snprintf(labelBuf, sizeof(labelBuf), "%s", job.name.c_str());

    if (UIComponents::SelectableItem("job_row", labelBuf, isSelected, nullptr)) {
        if (!isDisabled) {
            selectedJobIndex_ = index;
        }
    }

    if (isDisabled) {
        ImGui::PopStyleColor();
        return;
    }

}

void JobsUI::RenderJobDetail() {
    auto* tracker = bridge_ != nullptr ? bridge_->GetJobTracker() : nullptr;
    auto& theme = ThemeManager::Instance();

    if (selectedJobIndex_ < 0 || tracker == nullptr || !tracker->IsInitialized()) {
        UIComponents::CardHeader("Details", nullptr);
        UIComponents::Separator();
        ImGui::Spacing();
        UIComponents::InfoRow("Status", "Kein Job ausgewählt");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextSecondary));
        ImGui::TextWrapped("Wähle links einen Job aus, um detaillierte Statistiken zu sehen.");
        ImGui::PopStyleColor();
        return;
    }

    const auto& jobs = tracker->GetJobs();
    if (selectedJobIndex_ >= static_cast<int>(jobs.size())) {
        selectedJobIndex_ = -1;
        UIComponents::CardHeader("Details", nullptr);
        UIComponents::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Ungültige Auswahl. Bitte wähle erneut.");
        ImGui::PopStyleColor();
        return;
    }

    const auto& job = jobs[selectedJobIndex_];

    {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
        ImGui::TextUnformatted(job.name.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextPrimary));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
        if (ImGui::SmallButton(kIconEdit)) {
            OpenEditModal(job.id);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetItemTooltip("Bearbeiten");
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

    }

    jobs::JobWindowAggregates agg = tracker->GetAggregates(job.id);
    jobs::JobStats stats;
    tracker->GetJobStats(job.id, stats);
    const int adjustedSkillLevel = tracker->GetAdjustedHighestSkill(job.id);

    RenderDetailEarningsCards(agg, stats, adjustedSkillLevel);

    {
        const int currentFrame = ImGui::GetFrameCount();
        const bool viewReopened = (lastJobsRenderFrame_ >= 0) && (currentFrame > lastJobsRenderFrame_ + 1);
        const bool jobChanged = (skillTreeAnimJobId_ != job.id);
        const bool levelChanged = (skillTreeAnimLevel_ != adjustedSkillLevel);

        if (viewReopened || jobChanged || levelChanged || skillTreeAnimId_.empty()) {
            skillTreeAnimId_ = "JobsSkillTree:" + job.id;
            AnimationHelper::StartAnimation(skillTreeAnimId_, 0.65f);
            skillTreeAnimJobId_ = job.id;
            skillTreeAnimLevel_ = adjustedSkillLevel;
        }

        lastJobsRenderFrame_ = currentFrame;
    }

    if (!job.skillTree.empty()) {
        RenderDetailSkillTree(job.skillTree, adjustedSkillLevel);
    }

    std::vector<jobs::JobEvent> events = tracker->GetEventsForJob(job.id, false);
    RenderDetailRecentEvents(events);

    RenderEditModal(job.id);

    RenderCorrectionModal(job.id);

    RenderDeleteConfirm();
}

void JobsUI::RenderDetailEarningsCards(const jobs::JobWindowAggregates& agg,
                                        const jobs::JobStats& stats,
                                        int adjustedSkillLevel) {
    auto& theme = ThemeManager::Instance();

    UIComponents::SectionHeader("Informationen", nullptr);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float thirdWidth = (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f;
    const ImVec4 accentColor = theme.GetColor(ThemeColorRole::Accent);
    const int skillPoints = (std::max)(adjustedSkillLevel, 0);

    UIComponents::MetricCard("DetailHeute",  "Heute",        FormatMoney(agg.today),      "", &accentColor, nullptr, thirdWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("DetailTotal",  "Gesamt",       FormatMoney(agg.lifetime),   "", &accentColor, nullptr, thirdWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("DetailSkill",  "Skill Punkte", std::to_string(skillPoints), "", nullptr, nullptr, thirdWidth);

    UIComponents::MetricCard("Detail7d",     "7 Tage",       FormatMoney(agg.last7Days),  "", nullptr, nullptr, thirdWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("Detail30d",    "30 Tage",      FormatMoney(agg.last30Days), "", nullptr, nullptr, thirdWidth);
    ImGui::SameLine();
    UIComponents::MetricCard("DetailBest",   "Bester Tag",   FormatMoney(agg.bestDay),    "", nullptr, nullptr, thirdWidth);
}

void JobsUI::RenderDetailSkillTree(const std::vector<jobs::SkillTreeNode>& skillTree,
                                    int currentLevel) {
    auto& theme = ThemeManager::Instance();

    UIComponents::SectionHeader("Skill-Tree", nullptr);

    if (skillTree.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Kein Skill-Tree vorhanden.");
        ImGui::PopStyleColor();
        return;
    }

    std::vector<int> sortedIndices;
    sortedIndices.reserve(skillTree.size());
    for (int i = 0; i < static_cast<int>(skillTree.size()); ++i) {
        sortedIndices.push_back(i);
    }
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
        return skillTree[a].unlockOrder < skillTree[b].unlockOrder;
    });

    const float cardPaddingX = Scale(10.0f);
    const float cardPaddingY = Scale(6.0f);
    const float cardRadius = Scale(4.0f);
    const float cardGap = Scale(4.0f);

    for (int si = 0; si < static_cast<int>(sortedIndices.size()); ++si) {
        const auto& node = skillTree[sortedIndices[si]];
        ImGui::PushID(sortedIndices[si]);

        std::string perkDisplay;
        if (node.perkType == "skin") {
            perkDisplay = "Skin " + node.perkValue;
        } else if (node.perkType == "lohn_bonus") {
            perkDisplay = "Lohn +" + node.perkValue;
        } else if (node.perkType == "privatfahrzeug") {
            perkDisplay = "Privatfahrzeug: " + node.perkValue;
        } else if (node.perkType == "dienstfahrzeug") {
            perkDisplay = "Dienstfahrzeug: " + node.perkValue;
        } else if (node.perkType == "effizienz") {
            perkDisplay = node.perkValue;
        } else if (node.perkType == "job_freigeschaltet") {
            perkDisplay = "Freigeschaltet: " + node.perkValue;
        } else if (node.perkType == "spezial_mission") {
            perkDisplay = "Spezial-Mission: " + node.perkValue;
        } else if (node.perkType == "job_erweiterung") {
            perkDisplay = "Erweiterung: " + node.perkValue;
        } else if (node.perkType == "fast_travel") {
            perkDisplay = node.perkValue;
        } else if (!node.perkType.empty()) {
            perkDisplay = node.perkValue;
        } else {
            perkDisplay = node.name;
        }

        float targetProgress = 0.0f;
        if (node.maxPoints > 0) {
            targetProgress = static_cast<float>(currentLevel) / static_cast<float>(node.maxPoints);
            if (targetProgress > 1.0f) targetProgress = 1.0f;
            if (targetProgress < 0.0f) targetProgress = 0.0f;
        }

        const float animT = AnimationHelper::EaseInOut(AnimationHelper::GetProgress(skillTreeAnimId_));
        const float progress = targetProgress * animT;

        const float cardWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 basePos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        const ImVec4 trackBg = theme.GetColor(ThemeColorRole::SurfaceAlt);
        const ImVec4 progressFill = theme.GetColor(ThemeColorRole::Success);

        const ImVec2 textSize = ImGui::CalcTextSize(perkDisplay.c_str());
        char thresholdText[32] = {};
        if (node.maxPoints > 0) {
            std::snprintf(thresholdText, sizeof(thresholdText), "%d P.", node.maxPoints);
        }
        const ImVec2 threshSize = ImGui::CalcTextSize(thresholdText);
        const float cardHeight = textSize.y + cardPaddingY * 2.0f;
        const float textOffsetY = (cardHeight - textSize.y) * 0.5f;

        dl->AddRectFilled(
            basePos,
            ImVec2(basePos.x + cardWidth, basePos.y + cardHeight),
            ImGui::GetColorU32(trackBg),
            cardRadius);

        const float fillWidth = cardWidth * progress;
        if (fillWidth > 0.0f) {
            dl->PushClipRect(
                basePos,
                ImVec2(basePos.x + fillWidth, basePos.y + cardHeight),
                true);
            dl->AddRectFilled(
                basePos,
                ImVec2(basePos.x + cardWidth, basePos.y + cardHeight),
                ImGui::GetColorU32(progressFill),
                cardRadius);
            dl->PopClipRect();

        }

        dl->AddRect(
            basePos,
            ImVec2(basePos.x + cardWidth, basePos.y + cardHeight),
            ImGui::GetColorU32(theme.GetColor(ThemeColorRole::Border)),
            cardRadius,
            0,
            Scale(1.0f));

        const ImVec4 textColor = theme.GetColor(ThemeColorRole::TextPrimary);
        dl->AddText(
            ImVec2(basePos.x + cardPaddingX, basePos.y + textOffsetY),
            ImGui::GetColorU32(textColor),
            perkDisplay.c_str());

        if (node.maxPoints > 0) {
            dl->AddText(
                ImVec2(basePos.x + cardWidth - cardPaddingX - threshSize.x, basePos.y + textOffsetY),
                ImGui::GetColorU32(theme.GetColor(ThemeColorRole::TextMuted)),
                thresholdText);
        }

        ImGui::Dummy(ImVec2(cardWidth, cardHeight));

        if (si < static_cast<int>(sortedIndices.size()) - 1) {
            ImGui::Dummy(ImVec2(0.0f, cardGap));
        }

        ImGui::PopID();
    }
}

void JobsUI::RenderDetailRecentEvents(const std::vector<jobs::JobEvent>& events) {
    auto& theme = ThemeManager::Instance();

    UIComponents::SectionHeader("Letzte Ereignisse", nullptr);

    if (events.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextWrapped("Keine Ereignisse vorhanden.");
        ImGui::PopStyleColor();
        return;
    }

    int shown = 0;
    UIComponents::BeginCard("RecentEventsList", 0.0f, false);

    const float deleteBtnSize = theme.GetControlHeight(ThemeControlHeightRole::Button) * 0.65f;

    for (int i = static_cast<int>(events.size()) - 1; i >= 0 && shown < 10; --i) {
        const auto& evt = events[i];
        if (evt.deleted) continue;

        ImGui::PushID(i);

        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
        ImGui::TextUnformatted(FormatTimestamp(evt.timestamp).c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();

        const bool isEarning = (evt.type == "earning" || evt.type == "manual_earning");
        const bool isCorrection = (evt.type == "correction");
        const bool isMessage = (evt.type == "job_message");
        ImVec4 typeColor;
        if (isEarning) {
            typeColor = theme.GetColor(ThemeColorRole::Success);
        } else if (isCorrection) {
            typeColor = theme.GetColor(ThemeColorRole::Warning);
        } else if (isMessage) {
            typeColor = theme.GetColor(ThemeColorRole::Accent);
        } else {
            typeColor = theme.GetColor(ThemeColorRole::Info);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, typeColor);
        ImGui::Text("[%s]", FormatEventTypeBadge(evt.type).c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();

        if (evt.type == "skill_level" || evt.type == "skill_level_absolute" || evt.type == "manual_skill_level") {
            ImGui::Text("Lv.%d", evt.level);
        } else if (evt.type == "job_message") {
            ImGui::TextUnformatted(evt.note.empty() ? "-" : evt.note.c_str());
        } else {
            ImGui::Text("%s", FormatMoney(evt.amount).c_str());
        }

        ImGui::SameLine();

        const bool isManual = (evt.source == "manual" || evt.source == "manual_add");
        const ImVec4 srcColor = isManual
            ? theme.GetColor(ThemeColorRole::Accent)
            : theme.GetColor(ThemeColorRole::TextSecondary);

        ImGui::PushStyleColor(ImGuiCol_Text, srcColor);
        ImGui::Text("(%s)", FormatSourceBadge(evt.source).c_str());
        ImGui::PopStyleColor();

        if (!evt.note.empty() && evt.type != "job_message") {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::TextMuted));
            ImGui::TextWrapped("- %s", evt.note.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - deleteBtnSize);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.GetColor(ThemeColorRole::DangerHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme.GetColor(ThemeColorRole::Danger));
        ImGui::PushStyleColor(ImGuiCol_Text, theme.GetColor(ThemeColorRole::Danger));
        char delLabel[32];
        std::snprintf(delLabel, sizeof(delLabel), "X##del_%d", i);
        if (ImGui::SmallButton(delLabel)) {
            OpenDeleteConfirm(evt.id, i);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Ereignis löschen");
        }
        ImGui::PopStyleColor(4);

        ImGui::PopID();

        ++shown;
        if (shown < 10 && i > 0) {
            ImGui::Spacing();
        }
    }

    UIComponents::EndCard();
}

} // namespace keybinder
