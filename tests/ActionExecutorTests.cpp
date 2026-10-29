/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    ActionExecutor unit tests
*/
#define KEYBINDER_TESTING

#include "../include/ActionExecutor.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace keybinder;

// ---- Test infrastructure ----

struct ExecutorTestState {
    int passed = 0;
    int failed = 0;
    std::string error;
};

ExecutorTestState g_exec_state;

#define EXEC_ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        g_exec_state.error = "Assertion failed: " #cond; \
        return false; \
    } \
    ++g_exec_state.passed; \
} while(0)

#define EXEC_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        g_exec_state.error = "Equality failed: " #a " != " #b; \
        return false; \
    } \
    ++g_exec_state.passed; \
} while(0)

#define EXEC_RUN_TEST(name) do { \
    if (name()) { \
        std::printf("[PASS] %s\n", #name); \
    } else { \
        ++g_exec_state.failed; \
        std::printf("[FAIL] %s: %s\n", #name, g_exec_state.error.c_str()); \
    } \
} while(0)

// ---- Mock objects ----

class MockBridge : public SAMPBridgeInterface {
public:
    mutable std::vector<std::string> chatMessages;
    mutable std::vector<std::string> commands;

    bool SendChat(const std::string& text) const override {
        chatMessages.push_back(text);
        return true;
    }

    bool SendCommand(const std::string& cmd) const override {
        commands.push_back(cmd);
        return true;
    }
};

class MockZoneProvider : public ZoneProvider {
public:
    std::string zoneName;

    std::string GetCurrentZoneName() const override {
        return zoneName;
    }
};

class ManualInputBridge : public MockBridge {
public:
    bool acceptsInput = true;
    bool inputActive = false;
    int prompts = 0;
    int polls = 0;
    int cancellations = 0;
    ManualTextInputStatus nextStatus = ManualTextInputStatus::Waiting;
    std::string inputValue;

    bool BeginManualTextInput() override {
        ++prompts;
        if (!acceptsInput || inputActive) return false;
        inputActive = true;
        nextStatus = ManualTextInputStatus::Waiting;
        inputValue.clear();
        return true;
    }

    ManualTextInputStatus PollManualTextInput(std::string& value) override {
        ++polls;
        if (!inputActive) return ManualTextInputStatus::Cancelled;
        if (nextStatus == ManualTextInputStatus::Submitted) value = inputValue;
        if (nextStatus != ManualTextInputStatus::Waiting) inputActive = false;
        return nextStatus;
    }

    void CancelManualTextInput() override {
        ++cancellations;
        inputActive = false;
    }
};

class MockGameDataProvider : public GameDataProvider {
public:
    float playerHp = 100.0f;
    float playerArmour = 50.0f;
    float vehicleHp = 1000.0f;
    int totalKills = 10;
    int totalDeaths = 5;
    std::string vehicleName = "Infernus";

    float GetPlayerHP() const override { return playerHp; }
    float GetPlayerArmour() const override { return playerArmour; }
    float GetVehicleHP() const override { return vehicleHp; }
    int GetTotalKills() const override { return totalKills; }
    int GetTotalDeaths() const override { return totalDeaths; }
    std::string GetVehicleName() const override { return vehicleName; }
};

// ---- Tests ----

bool TestExecuteEmptySequence() {
    ActionExecutor executor;
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    executor.Execute({});

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    return true;
}

bool TestExecuteSingleChat() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Hello world")
    };
    executor.Execute(actions);

    EXEC_ASSERT_TRUE(executor.IsExecuting());
    executor.Update();
    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Hello world");
    return true;
}

bool TestExecuteSingleCommand() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Command, "/me waves")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.commands.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.commands[0], "/me waves");
    return true;
}

bool TestMultiActionSequence() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "First"),
        Action(ActionType::Command, "/second"),
        Action(ActionType::Chat, "Third")
    };
    executor.Execute(actions);

    // Frame 1: executes Chat "First" and advances
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "First");

    // Frame 2: executes Command "/second"
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());
    EXEC_ASSERT_EQ(bridge.commands.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.commands[0], "/second");

    // Frame 3: executes Chat "Third" and completes
    executor.Update();
    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(2));
    EXEC_ASSERT_EQ(bridge.chatMessages[1], "Third");
    return true;
}

bool TestDelayBehavior() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    unsigned long fakeTick = 0;
    executor.SetTickFunction([&]() -> unsigned long { return fakeTick; });

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Before delay"),
        Action(ActionType::Delay, "500"),
        Action(ActionType::Chat, "After delay")
    };
    executor.Execute(actions);

    // Frame 1: execute Chat "Before delay"
    fakeTick = 100;
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Before delay");

    // Frame 2: begin delay (records start tick)
    fakeTick = 200;
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1)); // no new message yet

    // Frame 3: still in delay (300ms elapsed out of 500)
    fakeTick = 500;
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));

    // Frame 4: delay elapsed (700ms - 200ms start = 500ms)
    fakeTick = 700;
    executor.Update();
    // After delay expires, it advances to next action but doesn't execute it yet
    EXEC_ASSERT_TRUE(executor.IsExecuting());

    // Frame 5: execute "After delay"
    executor.Update();
    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(2));
    EXEC_ASSERT_EQ(bridge.chatMessages[1], "After delay");
    return true;
}

bool TestDelayZeroSkips() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Before"),
        Action(ActionType::Delay, "0"),
        Action(ActionType::Chat, "After")
    };
    executor.Execute(actions);

    // Frame 1: Chat "Before"
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));

    // Frame 2: Delay "0" skips immediately, advances
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());

    // Frame 3: Chat "After" -> complete
    executor.Update();
    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(2));
    return true;
}

bool TestDelayInvalidSkips() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Before"),
        Action(ActionType::Delay, "notanumber"),
        Action(ActionType::Chat, "After")
    };
    executor.Execute(actions);

    executor.Update(); // Chat "Before"
    executor.Update(); // Delay invalid -> skip
    executor.Update(); // Chat "After"

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(2));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Before");
    EXEC_ASSERT_EQ(bridge.chatMessages[1], "After");
    return true;
}

bool TestPlaceholderExpansionZone() {
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = "Los Santos";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "I am in {zone} right now")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "I am in Los Santos right now");
    return true;
}

bool TestPlaceholderExpansionZoneUpper() {
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = "Los Santos";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Zone: {ZONE}")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Zone: LOS SANTOS");
    return true;
}

bool TestPlaceholderFallbackUnresolved() {
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = ""; // empty = unresolved
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "I am in {zone}"),
        Action(ActionType::Command, "/loc {ZONE}")
    };
    executor.Execute(actions);
    executor.Update(); // chat
    executor.Update(); // command

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "I am in Unknown");
    EXEC_ASSERT_EQ(bridge.commands[0], "/loc UNKNOWN");
    return true;
}

bool TestPlaceholderNoExpansionInDelay() {
    // Delay data should NOT be placeholder-expanded
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = "Vinewood";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);

    unsigned long fakeTick = 0;
    executor.SetTickFunction([&]() -> unsigned long { return fakeTick; });
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Delay, "100"),
        Action(ActionType::Chat, "Done")
    };
    executor.Execute(actions);

    // Frame 1: begin delay with data "100" (not expanded)
    fakeTick = 0;
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());

    // Frame 2: delay elapsed
    fakeTick = 200;
    executor.Update();

    // Frame 3: chat
    executor.Update();
    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Done");
    return true;
}

bool TestPlaceholderMixed() {
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = "Idlewood";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "{zone} to {ZONE} at {zone}")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Idlewood to IDLEWOOD at Idlewood");
    return true;
}

bool TestPlaceholderUnknownTag() {
    // Unknown placeholders like {unknown} should be kept as-is
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = "LS";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Hello {unknown} {zone}")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Hello {unknown} LS");
    return true;
}

bool TestStopExecution() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);

    unsigned long fakeTick = 0;
    executor.SetTickFunction([&]() -> unsigned long { return fakeTick; });

    std::vector<Action> actions = {
        Action(ActionType::Chat, "First"),
        Action(ActionType::Delay, "5000"),
        Action(ActionType::Chat, "Should not appear")
    };
    executor.Execute(actions);

    // Execute first chat
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));

    // Start delay
    fakeTick = 100;
    executor.Update();
    EXEC_ASSERT_TRUE(executor.IsExecuting());

    // Stop mid-delay
    executor.Stop();
    EXEC_ASSERT_TRUE(!executor.IsExecuting());

    // Update after stop should do nothing
    fakeTick = 10000;
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1)); // no "Should not appear"
    return true;
}

bool TestReentrancyProtection() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);

    bool callbackCalled = false;
    executor.OnComplete = [&]() {
        callbackCalled = true;
        // Try to call Update from within the completion callback
        // This should be a no-op due to re-entrancy guard
        executor.Update();
    };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Test")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(callbackCalled);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    return true;
}

bool TestExecuteReplacesCurrentSequence() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);

    unsigned long fakeTick = 0;
    executor.SetTickFunction([&]() -> unsigned long { return fakeTick; });

    std::vector<Action> actions1 = {
        Action(ActionType::Chat, "Sequence1-Start"),
        Action(ActionType::Delay, "5000"),
        Action(ActionType::Chat, "Sequence1-End")
    };
    executor.Execute(actions1);
    executor.Update(); // Chat
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));

    // Start new sequence while old one is in delay
    std::vector<Action> actions2 = {
        Action(ActionType::Chat, "Sequence2-Only")
    };
    executor.Execute(actions2);
    executor.Update();

    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(2));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Sequence1-Start");
    EXEC_ASSERT_EQ(bridge.chatMessages[1], "Sequence2-Only");
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    return true;
}

bool TestNoBridgeChatNoOp() {
    // Without a bridge set, chat/command actions should be no-ops (not crash)
    ActionExecutor executor;
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Hello"),
        Action(ActionType::Command, "/test")
    };
    executor.Execute(actions);
    executor.Update(); // Chat (no-op, no bridge)
    executor.Update(); // Command (no-op, no bridge)

    EXEC_ASSERT_TRUE(completed);
    return true;
}

bool TestKeyPressParsing() {
    // Test that ParseKeyName works for various key names
    // We can test this indirectly through the executor, but the key press
    // itself is a no-op under KEYBINDER_TESTING
    ActionExecutor executor;
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::KeyPress, "F1"),
        Action(ActionType::KeyPress, "Enter"),
        Action(ActionType::KeyPress, "A"),
        Action(ActionType::KeyPress, "MOUSE1")
    };
    executor.Execute(actions);
    executor.Update(); // F1
    executor.Update(); // Enter
    executor.Update(); // A
    executor.Update(); // MOUSE1

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    return true;
}

bool TestResolveKeyPressVirtualKey() {
    // The keybind editor persists KeyPress targets as a numeric virtual-key code
    // string (e.g. "90" for Z). Regression: this used to resolve to 0 because the
    // executor only understood symbolic names, so the simulated key press never fired.
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("90"), 90);   // 'Z'
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("112"), 112); // VK_F1
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("13"), 13);   // VK_RETURN
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("1"), 1);     // VK_LBUTTON
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("254"), 254); // upper VK bound

    // Symbolic key names still resolve (legacy / hand-edited configs).
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("Z"), 90);
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("F1"), 112);
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("Enter"), 13);
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("MOUSE1"), 1);

    // Unset / invalid targets resolve to 0 (no key press fired).
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey(""), 0);
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("0"), 0);
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("255"), 0);   // reserved code
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("99999"), 0); // out of range
    EXEC_ASSERT_EQ(ActionExecutor::ResolveKeyPressVirtualKey("1A"), 0);    // not a pure number
    return true;
}

bool TestCommandPlaceholderExpansion() {
    // Placeholders should also expand in command data
    ActionExecutor executor;
    MockBridge bridge;
    MockZoneProvider zoneProvider;
    zoneProvider.zoneName = "Market";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zoneProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Command, "/loc I'm at {zone}")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.commands[0], "/loc I'm at Market");
    return true;
}

bool TestPlaceholderExpansionCarName() {
    ActionExecutor executor;
    MockBridge bridge;
    MockGameDataProvider gameDataProvider;
    gameDataProvider.vehicleName = "Infernus";
    executor.SetSAMPBridge(&bridge);
    executor.SetGameDataProvider(&gameDataProvider);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Driving {carname}")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Driving Infernus");
    return true;
}

bool TestPlaceholderExpansionCarNameOnFootFallback() {
    ActionExecutor executor;
    MockBridge bridge;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };

    std::vector<Action> actions = {
        Action(ActionType::Chat, "Status: {carname}")
    };
    executor.Execute(actions);
    executor.Update();

    EXEC_ASSERT_TRUE(completed);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), static_cast<size_t>(1));
    EXEC_ASSERT_EQ(bridge.chatMessages[0], std::string("Status: Zu Fu\xC3\x9F"));
    return true;
}

bool TestManualInputPausesWholeBindAndPromptsOnce() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };
    executor.Execute({Action(ActionType::Chat, "Before"),
                      Action(ActionType::Chat, "/News In {input} befindet sich ein Blitzer. Bitte vorsichtig fahren.")});
    EXEC_ASSERT_EQ(bridge.prompts, 0); // Never open a new chat inside the slash callback.
    executor.Update();
    EXEC_ASSERT_EQ(bridge.prompts, 1);
    EXEC_ASSERT_TRUE(executor.IsExecuting());
    executor.Update();
    executor.Update();
    EXEC_ASSERT_EQ(bridge.prompts, 1);
    EXEC_ASSERT_TRUE(bridge.chatMessages.empty() && bridge.commands.empty());
    EXEC_ASSERT_TRUE(!completed);
    bridge.inputValue = "BSN";
    bridge.nextStatus = ManualTextInputStatus::Submitted;
    executor.Update();
    EXEC_ASSERT_TRUE(bridge.chatMessages.empty() && bridge.commands.empty());
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Before");
    executor.Update();
    EXEC_ASSERT_EQ(bridge.commands.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.commands[0], "/News In BSN befindet sich ein Blitzer. Bitte vorsichtig fahren.");
    EXEC_ASSERT_EQ(bridge.prompts, 1);
    EXEC_ASSERT_EQ(bridge.cancellations, 0);
    EXEC_ASSERT_TRUE(completed && !executor.IsExecuting());
    return true;
}

bool TestManualInputIsLiteralAndSharedAcrossActions() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    MockZoneProvider zone;
    zone.zoneName = "Market";
    executor.SetSAMPBridge(&bridge);
    executor.SetZoneProvider(&zone);
    executor.Execute({Action(ActionType::Chat, "Ort {input}|{input}|{zone}"),
                      Action(ActionType::Command, "/r {input}")});
    executor.Update();
    // Unicode and template-looking input must remain literal, including a slash.
    bridge.inputValue = "B\xC3\xBCrger {zone} /f {input}";
    bridge.nextStatus = ManualTextInputStatus::Submitted;
    executor.Update();
    executor.Update();
    executor.Update();
    EXEC_ASSERT_EQ(bridge.prompts, 1);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Ort B\xC3\xBCrger {zone} /f {input}|B\xC3\xBCrger {zone} /f {input}|Market");
    EXEC_ASSERT_EQ(bridge.commands.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.commands[0], "/r B\xC3\xBCrger {zone} /f {input}");
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    return true;
}

bool TestManualInputCancelledDropsEntireBind() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    bool completed = false;
    executor.OnComplete = [&]() { completed = true; };
    executor.Execute({Action(ActionType::Chat, "Must not be sent"), Action(ActionType::Chat, "{input}")});
    executor.Update();
    bridge.nextStatus = ManualTextInputStatus::Cancelled;
    executor.Update();
    executor.Update();
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    EXEC_ASSERT_TRUE(!completed);
    EXEC_ASSERT_TRUE(bridge.chatMessages.empty() && bridge.commands.empty());
    EXEC_ASSERT_EQ(bridge.cancellations, 0); // Terminal poll already released the request.
    return true;
}

bool TestManualInputStopAndReplacementCancelOwnedRequest() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    executor.Execute({Action(ActionType::Chat, "Old {input}")});
    executor.Update();
    executor.Stop();
    EXEC_ASSERT_EQ(bridge.cancellations, 1);
    EXEC_ASSERT_TRUE(!bridge.inputActive && !executor.IsExecuting());
    executor.Update();
    executor.Execute({Action(ActionType::Chat, "Replace {input}")});
    executor.Update();
    executor.Execute({Action(ActionType::Chat, "Replacement")});
    EXEC_ASSERT_EQ(bridge.cancellations, 2);
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Replacement");
    executor.Execute({Action(ActionType::Chat, "Empty replacement {input}")});
    executor.Update();
    executor.Execute({});
    EXEC_ASSERT_EQ(bridge.cancellations, 3);
    EXEC_ASSERT_TRUE(!bridge.inputActive && !executor.IsExecuting());
    executor.Execute({Action(ActionType::Chat, "Not opened {input}")});
    executor.Stop();
    EXEC_ASSERT_EQ(bridge.prompts, 3);
    EXEC_ASSERT_EQ(bridge.cancellations, 3);
    return true;
}

bool TestManualInputNextInvocationAsksForFreshValue() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    const std::vector<Action> bind = {Action(ActionType::Chat, "Place {input}")};
    executor.Execute(bind);
    executor.Update();
    bridge.inputValue = "BSN";
    bridge.nextStatus = ManualTextInputStatus::Submitted;
    executor.Update();
    executor.Update();
    executor.Execute(bind);
    executor.Update();
    EXEC_ASSERT_EQ(bridge.prompts, 2);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{1});
    bridge.inputValue = "LSPD";
    bridge.nextStatus = ManualTextInputStatus::Submitted;
    executor.Update();
    executor.Update();
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{2});
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Place BSN");
    EXEC_ASSERT_EQ(bridge.chatMessages[1], "Place LSPD");
    return true;
}

bool TestManualInputOnlyExactChatAndCommandTagsPrompt() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    executor.Execute({Action(ActionType::Delay, "{input}"),
                      Action(ActionType::KeyPress, "{input}"),
                      Action(ActionType::Chat, "{INPUT}|{inputx}")});
    executor.Update();
    executor.Update();
    executor.Update();
    EXEC_ASSERT_EQ(bridge.prompts, 0);
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "{INPUT}|{inputx}");
    return true;
}

bool TestChatActionRoutesLeadingSlashToCommand() {
    MockBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    executor.Execute({Action(ActionType::Chat, "/r Test"),
                      Action(ActionType::Chat, "Normal chat"),
                      Action(ActionType::Command, "news Test")});
    executor.Update();
    executor.Update();
    executor.Update();
    EXEC_ASSERT_EQ(bridge.commands.size(), size_t{2});
    EXEC_ASSERT_EQ(bridge.commands[0], "/r Test");
    EXEC_ASSERT_EQ(bridge.commands[1], "news Test");
    EXEC_ASSERT_EQ(bridge.chatMessages.size(), size_t{1});
    EXEC_ASSERT_EQ(bridge.chatMessages[0], "Normal chat");
    return true;
}

bool TestManualInputUnavailableNeverSendsPartialBind() {
    ManualInputBridge bridge;
    ActionExecutor executor;
    executor.SetSAMPBridge(&bridge);
    bridge.acceptsInput = false;
    executor.Execute({Action(ActionType::Chat, "Before"), Action(ActionType::Command, "/r {input}")});
    executor.Update();
    EXEC_ASSERT_EQ(bridge.prompts, 1);
    EXEC_ASSERT_EQ(bridge.cancellations, 0);
    EXEC_ASSERT_TRUE(!executor.IsExecuting());
    EXEC_ASSERT_TRUE(bridge.chatMessages.empty() && bridge.commands.empty());
    ActionExecutor noBridge;
    noBridge.Execute({Action(ActionType::Chat, "{input}")});
    noBridge.Update();
    EXEC_ASSERT_TRUE(!noBridge.IsExecuting());
    return true;
}

bool TestManualInputChangingBridgeCancelsPreviousOwner() {
    ManualInputBridge first;
    ManualInputBridge second;
    ActionExecutor executor;
    executor.SetSAMPBridge(&first);
    executor.Execute({Action(ActionType::Chat, "{input}")});
    executor.Update();
    executor.SetSAMPBridge(&second);
    EXEC_ASSERT_EQ(first.cancellations, 1);
    EXEC_ASSERT_TRUE(!first.inputActive && !executor.IsExecuting());
    EXEC_ASSERT_EQ(second.prompts, 0);
    return true;
}

} // namespace

int RunActionExecutorTests() {
    std::printf("\n=== ActionExecutor Tests ===\n\n");

    EXEC_RUN_TEST(TestExecuteEmptySequence);
    EXEC_RUN_TEST(TestExecuteSingleChat);
    EXEC_RUN_TEST(TestExecuteSingleCommand);
    EXEC_RUN_TEST(TestMultiActionSequence);
    EXEC_RUN_TEST(TestDelayBehavior);
    EXEC_RUN_TEST(TestDelayZeroSkips);
    EXEC_RUN_TEST(TestDelayInvalidSkips);
    EXEC_RUN_TEST(TestPlaceholderExpansionZone);
    EXEC_RUN_TEST(TestPlaceholderExpansionZoneUpper);
    EXEC_RUN_TEST(TestPlaceholderFallbackUnresolved);
    EXEC_RUN_TEST(TestPlaceholderNoExpansionInDelay);
    EXEC_RUN_TEST(TestPlaceholderMixed);
    EXEC_RUN_TEST(TestPlaceholderUnknownTag);
    EXEC_RUN_TEST(TestStopExecution);
    EXEC_RUN_TEST(TestReentrancyProtection);
    EXEC_RUN_TEST(TestExecuteReplacesCurrentSequence);
    EXEC_RUN_TEST(TestNoBridgeChatNoOp);
    EXEC_RUN_TEST(TestKeyPressParsing);
    EXEC_RUN_TEST(TestResolveKeyPressVirtualKey);
    EXEC_RUN_TEST(TestCommandPlaceholderExpansion);
    EXEC_RUN_TEST(TestPlaceholderExpansionCarName);
    EXEC_RUN_TEST(TestPlaceholderExpansionCarNameOnFootFallback);
    EXEC_RUN_TEST(TestManualInputPausesWholeBindAndPromptsOnce);
    EXEC_RUN_TEST(TestManualInputIsLiteralAndSharedAcrossActions);
    EXEC_RUN_TEST(TestManualInputCancelledDropsEntireBind);
    EXEC_RUN_TEST(TestManualInputStopAndReplacementCancelOwnedRequest);
    EXEC_RUN_TEST(TestManualInputNextInvocationAsksForFreshValue);
    EXEC_RUN_TEST(TestManualInputOnlyExactChatAndCommandTagsPrompt);
    EXEC_RUN_TEST(TestChatActionRoutesLeadingSlashToCommand);
    EXEC_RUN_TEST(TestManualInputUnavailableNeverSendsPartialBind);
    EXEC_RUN_TEST(TestManualInputChangingBridgeCancelsPreviousOwner);

    std::printf("\n=== ActionExecutor Results: %d passed, %d failed ===\n\n",
                g_exec_state.passed, g_exec_state.failed);

    return g_exec_state.failed;
}
