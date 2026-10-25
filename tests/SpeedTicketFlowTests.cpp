#include "../include/SpeedTicketFlow.h"

#include <cstdio>
#include <limits>
#include <string>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] SpeedTicketFlow: %s (line %d)\n", #value, __LINE__); return; } } while (0)
using Flow = keybinder::SpeedTicketFlow;
using Dialog = keybinder::SpeedTicketDialog;
using Kind = keybinder::SpeedTicketAction::Kind;

Dialog Category() {
    return {101, 2, "Staat \xC2\xBB Wantedkatalog", "Ordnungswidrigkeit\nVergehen\nVerbrechen"};
}

Dialog Offence() {
    return {102, 5, "Wanteds \xC2\xBB Ordnungswidrigkeit",
        "Tatbestand\tWantedpunkte\nLeichter Versto\xC3\x9F gegen die StVO\t3\n"
        "Schwerer Versto\xC3\x9F gegen die StVO\t5\nParken innerhalb eines Sperrgebiets\t2\n"
        "Erregung \xC3\xB6" "ffentlichen \xC3\x84rgernisses\t8\nUmweltverschmutzung\t9\n"
        "Geschwindigkeits\xC3\xBC" "berschreitung ab 20km/h\t15"};
}

Dialog PlayerInput() {
    return {103, 1, "Wanteds vergeben",
        "Grund: Geschwindigkeits\xC3\xBC" "berschreitung ab 20km/h\nWantedpunkte: {FF0000}15\n\n"
        "Bitte Spieler-ID oder Name eingeben:"};
}

void TestScreenshotLayoutSubmitsExactlyOnceWithoutUserClick() {
    Flow flow;
    std::string error;
    CHECK(flow.Start("42", 1000, error));
    CHECK(error.empty() && flow.IsActive() && flow.TargetPlayerId() == 42);
    const auto category = flow.OnDialog(Category(), 1000);
    CHECK(category.kind == Kind::SelectRow && category.dialogId == 101 && category.row == 0);
    CHECK(category.input == "Ordnungswidrigkeit");
    CHECK(flow.GetStage() == Flow::Stage::Offence);
    CHECK(flow.OnDialog(Category(), 1000).kind == Kind::None);
    const auto offence = flow.OnDialog(Offence(), 1000);
    CHECK(offence.kind == Kind::SelectRow && offence.dialogId == 102 && offence.row == 5);
    CHECK(offence.input == "Geschwindigkeits\xC3\xBC" "berschreitung ab 20km/h");
    CHECK(flow.GetStage() == Flow::Stage::PlayerInput);
    CHECK(flow.OnDialog(Offence(), 1000).kind == Kind::None);
    const auto submit = flow.OnDialog(PlayerInput(), 1000);
    CHECK(submit.kind == Kind::SubmitInput && submit.dialogId == 103 && submit.row == -1);
    CHECK(submit.input == "42" && !submit.notice.empty());
    CHECK(!flow.IsActive() && flow.TargetPlayerId() == -1);
    CHECK(flow.OnDialog(PlayerInput(), 1001).kind == Kind::None);
    CHECK(flow.OnDialog(Offence(), 1002).kind == Kind::None);
    CHECK(flow.Update(100000).kind == Kind::None);
    CHECK(flow.Start("1003", 100000, error));
}

void TestRowsAreFoundByMeaningRatherThanFixedIndexOrLastRow() {
    Flow flow;
    std::string error;
    CHECK(flow.Start("007", 1000, error));
    Dialog category{300, 5, "{FFFFFF}STAAT \xC2\xBB WANTEDKATALOG",
        "Kategorie\nVerbrechen\n{FF0000}Ordnungswidrigkeit\nVergehen"};
    auto action = flow.OnDialog(category, 1100);
    CHECK(action.kind == Kind::SelectRow && action.row == 1);
    CHECK(action.input == "Ordnungswidrigkeit");
    Dialog offence{301, 4, "{FFAABB}Wanteds \xC2\xBB Ordnungswidrigkeit",
        "Anderer Tatbestand\t3\nGeschwindigkeits\xC3\x9C" "BERSCHREITUNG ab 20 km / h\t{FF0000}15\nNoch ein Tatbestand\t9"};
    action = flow.OnDialog(offence, 1200);
    CHECK(action.kind == Kind::SelectRow && action.row == 1);
    CHECK(action.input == "Geschwindigkeits\xC3\x9C" "BERSCHREITUNG ab 20 km / h");
    Dialog input = PlayerInput();
    input.id = 302;
    action = flow.OnDialog(input, 1300);
    CHECK(action.kind == Kind::SubmitInput && action.input == "7" && action.dialogId == 302);
}

void TestPlayerIdParsingAndBusyRequestsNeverReplaceTheActiveTarget() {
    for (const auto& value : {std::string("0"), std::string("1003"), std::string(" 0007 \t")}) {
        int id = -1;
        CHECK(Flow::ParsePlayerId(value, id));
        CHECK(id >= 0 && id <= 1003);
    }
    for (const auto& value : {std::string(), std::string("1004"), std::string("-1"), std::string("+12"),
            std::string("1 2"), std::string("Fixture_Player"), std::string("12 extra"), std::string("0x10"),
            std::string("99999999999999999999999999999999"), std::string("1\0x", 3)}) {
        int id = 91;
        CHECK(!Flow::ParsePlayerId(value, id));
        CHECK(id == 91);
    }
    Flow flow;
    std::string error;
    CHECK(!flow.Start("", 1000, error) && !error.empty() && !flow.IsActive());
    CHECK(flow.Start("7", 1000, error));
    CHECK(!flow.Start("8", 1100, error));
    CHECK(!error.empty() && flow.TargetPlayerId() == 7 && flow.GetStage() == Flow::Stage::Category);
    CHECK(flow.Cancel("Abgebrochen").kind == Kind::Cancelled && !flow.IsActive());
    CHECK(flow.Cancel("Noch einmal").kind == Kind::None);
    CHECK(flow.Start("8", 1200, error) && flow.TargetPlayerId() == 8);
}

void TestUnexpectedOrAmbiguousCatalogueNeverSelectsAnUnrelatedOffence() {
    for (Dialog category : {Dialog{1, 2, "Staat \xC2\xBB Wantedkatalog", "Vergehen\nVerbrechen"},
            Dialog{1, 2, "Staat \xC2\xBB Wantedkatalog", "Ordnungswidrigkeit\nOrdnungswidrigkeit"},
            Dialog{1, 1, "Staat \xC2\xBB Wantedkatalog", "Ordnungswidrigkeit"},
            Dialog{1, 2, "Inventar", "Ordnungswidrigkeit"}}) {
        Flow flow;
        std::string error;
        CHECK(flow.Start("12", 1000, error));
        const auto action = flow.OnDialog(category, 1001);
        CHECK(action.kind == Kind::Cancelled && !action.notice.empty() && !flow.IsActive());
    }
    for (const std::string row : {std::string("Geschwindigkeitsueberschreitung ab 200km/h\t15"),
            std::string("Geschwindigkeitsueberschreitung ab 20km/h\t150"),
            std::string("Geschwindigkeitsueberschreitung ab 20km/h\t15\nGeschwindigkeitsueberschreitung ab 20km/h\t15")}) {
        Flow flow;
        std::string error;
        CHECK(flow.Start("12", 1000, error));
        CHECK(flow.OnDialog(Category(), 1001).kind == Kind::SelectRow);
        const auto action = flow.OnDialog({2, 4, "Wanteds \xC2\xBB Ordnungswidrigkeit", row}, 1002);
        CHECK(action.kind == Kind::Cancelled && !flow.IsActive());
    }
}

void TestFinalInputRequiresTheCorrectReasonPointsAndPrompt() {
    for (int failure = 0; failure < 5; ++failure) {
        Flow flow;
        std::string error;
        CHECK(flow.Start("12", 1000, error));
        CHECK(flow.OnDialog(Category(), 1001).kind == Kind::SelectRow);
        CHECK(flow.OnDialog(Offence(), 1002).kind == Kind::SelectRow);
        Dialog input = PlayerInput();
        if (failure == 0) input.style = 3;
        if (failure == 1) input.caption = "Spieler suchen";
        if (failure == 2) input.text = "Grund: Anderer Verstoß\nWantedpunkte: 15\nBitte Spieler-ID oder Name eingeben:";
        if (failure == 3) input.text = "Grund: Geschwindigkeitsueberschreitung ab 20km/h\nWantedpunkte: 150\nBitte Spieler-ID oder Name eingeben:";
        if (failure == 4) input.text = "Grund: Geschwindigkeitsueberschreitung ab 20km/h\nWantedpunkte: 15\nBitte Betrag eingeben:";
        const auto action = flow.OnDialog(input, 1003);
        CHECK(action.kind == Kind::Cancelled && action.input.empty() && !flow.IsActive());
    }
}

void TestTimeoutDisconnectCloseAndClockWrapResetTheFlow() {
    Flow flow;
    std::string error;
    CHECK(flow.Start("12", 1000, error));
    CHECK(flow.Update(8999).kind == Kind::None);
    CHECK(flow.Update(9000).kind == Kind::Cancelled && !flow.IsActive());
    CHECK(flow.Start("12", 1000, error));
    CHECK(flow.OnDialog(Category(), 8999).kind == Kind::SelectRow);
    CHECK(flow.Update(16998).kind == Kind::None);
    CHECK(flow.Update(16999).kind == Kind::Cancelled && !flow.IsActive());
    CHECK(flow.Start("12", 1000, error));
    CHECK(flow.Update(1001, false).kind == Kind::Cancelled && !flow.IsActive());
    CHECK(flow.Start("12", 1000, error));
    CHECK(flow.OnDialog({65535, 0, "", ""}, 1001).kind == Kind::Cancelled);
    const unsigned long nearWrap = (std::numeric_limits<unsigned long>::max)() - 100;
    CHECK(flow.Start("12", nearWrap, error));
    CHECK(flow.Update(50).kind == Kind::None);
    CHECK(flow.Update(8000).kind == Kind::Cancelled);
    CHECK(flow.Start("12", 1000, error));
    flow.Reset();
    CHECK(!flow.IsActive() && flow.TargetPlayerId() == -1 && flow.Update(100000).kind == Kind::None);
}
} // namespace

int RunSpeedTicketFlowTests() {
    checks = failures = 0;
    TestScreenshotLayoutSubmitsExactlyOnceWithoutUserClick();
    TestRowsAreFoundByMeaningRatherThanFixedIndexOrLastRow();
    TestPlayerIdParsingAndBusyRequestsNeverReplaceTheActiveTarget();
    TestUnexpectedOrAmbiguousCatalogueNeverSelectsAnUnrelatedOffence();
    TestFinalInputRequiresTheCorrectReasonPointsAndPrompt();
    TestTimeoutDisconnectCloseAndClockWrapResetTheFlow();
    std::printf("SpeedTicketFlowTests: %d checks, %d failures\n", checks, failures);
    return failures;
}

#ifdef SPEED_TICKET_FLOW_TEST_MAIN
int main() { return RunSpeedTicketFlowTests(); }
#endif
