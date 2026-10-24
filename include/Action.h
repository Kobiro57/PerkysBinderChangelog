/*
    NLBinder - SA-MP 0.3.DL Keybinder Plugin
    Keybind domain model - Action structure
*/
#pragma once

#include <string>
#include <cstdint>

namespace keybinder {

enum class ActionType : int {
    Chat = 0,     // Send chat message
    Command = 1,  // Execute a command
    Delay = 2,    // Wait/delay (milliseconds)
    KeyPress = 3  // Simulate a key press
};

struct Action {
    ActionType type;
    std::string data;  // text for chat/command, milliseconds for delay, virtual-key code (or key name) for KeyPress

    Action() : type(ActionType::Chat), data("") {}
    explicit Action(ActionType t, std::string d = "") : type(t), data(std::move(d)) {}

    bool operator==(const Action& other) const {
        return type == other.type && data == other.data;
    }

    bool operator!=(const Action& other) const {
        return !(*this == other);
    }
};

} // namespace keybinder