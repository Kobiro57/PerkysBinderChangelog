#pragma once

#include <optional>
#include <string>

namespace keybinder {

// Dialog strings are normalized UTF-8. The bridge owns network/native UI I/O.
struct SpeedTicketDialog {
    int id = -1;
    int style = -1;
    std::string caption;
    std::string text;
};

struct SpeedTicketAction {
    enum class Kind { None, SelectRow, SubmitInput, Cancelled };
    Kind kind = Kind::None;
    int dialogId = -1;
    int row = -1;
    std::string input;
    std::string notice;
};

// Navigates the named catalogue entries and submits the requested player ID
// exactly once when the offence and final input prompt both match.
class SpeedTicketFlow {
public:
    enum class Stage { Idle, Category, Offence, PlayerInput };
    static constexpr unsigned long kStageTimeoutMs = 8000;

    static bool ParsePlayerId(const std::string& arguments, int& id);
    bool Start(const std::string& arguments, unsigned long now, std::string& error);
    SpeedTicketAction OnDialog(const SpeedTicketDialog& dialog, unsigned long now);
    SpeedTicketAction Update(unsigned long now, bool connected = true);
    SpeedTicketAction Cancel(const std::string& reason);
    void Reset();

    bool IsActive() const { return stage_ != Stage::Idle; }
    Stage GetStage() const { return stage_; }
    int TargetPlayerId() const { return playerId_; }

private:
    Stage stage_ = Stage::Idle;
    int playerId_ = -1;
    unsigned long stageStartedAt_ = 0;
    std::optional<SpeedTicketDialog> lastHandledDialog_;
};

} // namespace keybinder
