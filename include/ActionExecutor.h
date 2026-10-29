#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Action.h"

namespace keybinder {

enum class ManualTextInputStatus {
    Cancelled,
    Waiting,
    Submitted
};

class SAMPBridgeInterface {
public:
    virtual ~SAMPBridgeInterface() = default;
    virtual bool SendChat(const std::string& text) const = 0;
    virtual bool SendCommand(const std::string& cmd) const = 0;
    virtual bool BeginManualTextInput() { return false; }
    virtual ManualTextInputStatus PollManualTextInput(std::string& value) {
        (void)value;
        return ManualTextInputStatus::Cancelled;
    }
    virtual void CancelManualTextInput() {}
};

class ZoneProvider {
public:
    virtual ~ZoneProvider() = default;
    virtual std::string GetCurrentZoneName() const = 0;
};

class GameDataProvider {
public:
    virtual ~GameDataProvider() = default;
    virtual float GetPlayerHP() const = 0;
    virtual float GetPlayerArmour() const = 0;
    virtual float GetVehicleHP() const = 0;
    virtual int GetTotalKills() const = 0;
    virtual int GetTotalDeaths() const = 0;
    virtual std::string GetVehicleName() const = 0;
    // Defaults preserve existing providers that do not expose location data.
    virtual std::string GetCityName() const { return {}; }
    virtual std::string GetDistrictName() const { return {}; }
};

class ActionExecutor {
public:
    using CompletionCallback = std::function<void()>;
    using TickFunction = std::function<unsigned long()>;

    ActionExecutor();
    ~ActionExecutor();

    ActionExecutor(const ActionExecutor&) = delete;
    ActionExecutor& operator=(const ActionExecutor&) = delete;

    void Execute(const std::vector<Action>& actions);
    void Stop();
    bool IsExecuting() const;
    void Update();

    void SetSAMPBridge(SAMPBridgeInterface* bridge);
    void SetZoneProvider(ZoneProvider* provider);
    void SetGameDataProvider(GameDataProvider* provider);
    void SetTickFunction(TickFunction func);

    CompletionCallback OnComplete;

    static std::string DescribeKeyPressByName(const std::string& name);
    static void SimulateKeyPressByName(const std::string& name);
    static void SimulateKeyPress(int vk);
    static int ParseKeyName(const std::string& name);
    // Resolves a KeyPress action's stored data to a virtual-key code. The editor
    // persists numeric VK codes (e.g. "90"); legacy/hand-edited configs may use a
    // symbolic key name (e.g. "Z", "F1"). Returns 0 when nothing resolves.
    static int ResolveKeyPressVirtualKey(const std::string& data);

private:
    enum class State {
        Idle,
        Executing,
        WaitingDelay,
        RequestingInput,
        WaitingInput
    };

    void ProcessCurrentAction();
    void AdvanceToNextAction();
    void FinishExecution();

    void ExecuteChat(const std::string& data);
    void ExecuteCommand(const std::string& data);
    void ExecuteKeyPress(const std::string& data);
    void BeginDelay(const std::string& data);

    std::string ExpandPlaceholders(const std::string& text) const;
    std::string GetCurrentZone() const;
    std::string GetCurrentCity() const;
    std::string GetPlayerHPString() const;
    std::string GetPlayerArmourString() const;
    std::string GetVehicleHPString() const;
    std::string GetVehicleNameString() const;
    std::string GetTotalKillsString() const;
    std::string GetTotalDeathsString() const;
    std::string GetKDString() const;

    unsigned long GetTick() const;

    State state_;
    std::vector<Action> queue_;
    size_t currentIndex_;
    unsigned long delayStartTick_;
    unsigned long delayDuration_;
    bool updating_;
    std::string manualInput_;
    SAMPBridgeInterface* manualInputBridge_;

    SAMPBridgeInterface* bridge_;
    ZoneProvider* zoneProvider_;
    GameDataProvider* gameDataProvider_;
    TickFunction tickFunc_;
};

} // namespace keybinder
