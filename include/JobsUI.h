/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Jobs UI - Job tracking statistics display with overview + detail split
*/
#pragma once

#include "JobsTypes.h"
#include "JobEventStore.h"

#include <ctime>
#include <string>
#include <vector>

// Forward declarations
class SAMPBridge;

namespace jobs {
class JobTracker;
}

namespace keybinder {

// Edit modal state
struct EditModalState {
    bool open = false;
    bool openCorrectionFromEdit = false;
    bool initialized = false;
    std::string loadedJobId;
    int baseLifetime = 0;
    int baseHighestLevel = 0;
    int currentEarningsOffset = 0;
    int currentSkillOffset = 0;
    int editedTotalEarnings = 0;
    int editedTotalSkillPoints = 0;
    std::string validationError;
};

// Correction modal state
struct CorrectionModalState {
    bool open = false;
    int typeIndex = 0;        // 0 = Einnahme, 1 = Skill-Level
    int amountOrLevel = 0;
    std::time_t timestamp = 0;
    std::string note;
    bool timestampEdited = false;
    std::string validationError;
    // Timestamp input buffer (day.month hour:minute format)
    int tsDay = 1, tsMonth = 1, tsYear = 2026, tsHour = 12, tsMinute = 0;
};

// Delete confirmation state
struct DeleteConfirmState {
    bool open = false;
    std::string eventId;
    int eventIndex = -1;
};

class JobsUI {
public:
    JobsUI();
    ~JobsUI() = default;

    // Render the main view
    void Render();

    // Set bridge for data access
    void SetSAMPBridge(SAMPBridge* bridge) { bridge_ = bridge; }

    // Test accessors
    int GetSelectedJobIndex() const { return selectedJobIndex_; }
    void SetSelectedJobIndex(int idx) { selectedJobIndex_ = idx; }
    bool IsDefaultSelectionResolved() const { return defaultSelectionResolved_; }

    // ── Formatting (public static for testability) ──
    static std::string FormatMoney(int amount);
    static std::string FormatTimestamp(std::time_t ts);
    static std::string FormatEventTypeBadge(const std::string& type);
    static std::string FormatSourceBadge(const std::string& source);

    // ── Correction validation (public static for testability) ──
    static std::string ValidateCorrection(int typeIndex, int amountOrLevel, const std::string& jobId);

    // Test accessors for modal state
    const CorrectionModalState& GetModalState() const { return modalState_; }
    CorrectionModalState& GetModalState() { return modalState_; }
    const EditModalState& GetEditModalState() const { return editModalState_; }
    EditModalState& GetEditModalState() { return editModalState_; }

    void SetTestJobTracker(jobs::JobTracker* tracker) { testTracker_ = tracker; }
    void DebugOpenEditModal(const jobs::JobId& jobId);
    void DebugApplyEditModalChanges();
    void DebugCancelEditModal();
    void DebugHandleCorrectionModalClosed();

private:
    SAMPBridge* bridge_ = nullptr;

    // Selection state: index into registry-snapshot jobs vector
    // -1 = no selection; resolved once to first enabled job
    int selectedJobIndex_ = -1;
    bool defaultSelectionResolved_ = false;

    // Correction state
    CorrectionModalState modalState_;
    DeleteConfirmState deleteConfirm_;
    EditModalState editModalState_;

    // ── Top-level render ──
    void RenderSummaryCards();
    void RenderJobOverview();
    void RenderJobDetail();

    // ── Overview helpers ──
    void ResolveDefaultSelection();
    void RenderOverviewRow(const jobs::JobDefinition& job, int index,
                           const jobs::JobWindowAggregates& agg,
                           const jobs::JobStats& stats);

    // ── Detail helpers ──
    void RenderDetailEarningsCards(const jobs::JobWindowAggregates& agg,
                                   const jobs::JobStats& stats,
                                   int adjustedSkillLevel);
    void RenderDetailSkillTree(const std::vector<jobs::SkillTreeNode>& skillTree,
                                int currentLevel);
    void RenderDetailRecentEvents(const std::vector<jobs::JobEvent>& events);

    // ── Correction helpers ──
    void RenderEditModal(const jobs::JobId& jobId);
    void RenderCorrectionModal(const jobs::JobId& jobId);
    void RenderDeleteConfirm();
    void OpenCorrectionModal();
    void OpenEditModal(const jobs::JobId& jobId);
    void CloseEditModal(bool preserveDraft = false);
    void LoadEditModalSnapshot(const jobs::JobId& jobId);
    void ApplyEditModalChanges();
    void HandleCorrectionModalClosed();
    void OpenDeleteConfirm(const std::string& eventId, int eventIndex);
    void SubmitCorrection(const jobs::JobId& jobId);
    void InitializeModalTimestamp();

    jobs::JobTracker* ResolveJobTracker() const;

    jobs::JobTracker* testTracker_ = nullptr;

    // ── Skill tree animation state ──
    std::string skillTreeAnimId_;
    std::string skillTreeAnimJobId_;
    int skillTreeAnimLevel_ = -1;
    int lastJobsRenderFrame_ = -1;
};

} // namespace keybinder
