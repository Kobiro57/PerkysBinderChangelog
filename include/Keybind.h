#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "Action.h"

namespace keybinder {

struct Keybind {
    std::string name;
    int key;
    std::vector<int> keys;
    std::string category;
    bool enabled;
    bool passthrough;
    std::string slashCommand;
    std::vector<Action> actions;

    Keybind()
        : name("")
        , key(0)
        , keys()
        , category("Default")
        , enabled(true)
        , passthrough(false)
        , slashCommand("")
        , actions()
    {}

    Keybind(std::string n, int k, std::string cat = "Default", bool en = true, bool pt = false,
            std::vector<Action> acts = {})
        : name(std::move(n))
        , key(k)
        , keys()
        , category(std::move(cat))
        , enabled(en)
        , passthrough(pt)
        , slashCommand("")
        , actions(std::move(acts))
    {}

    bool HasMultiKeys() const { return keys.size() > 1; }

    std::vector<int> GetAllKeys() const {
        if (!keys.empty()) return keys;
        if (key != 0) return {key};
        return {};
    }

    bool operator==(const Keybind& other) const {
        return name == other.name
            && key == other.key
            && keys == other.keys
            && category == other.category
            && enabled == other.enabled
            && passthrough == other.passthrough
            && slashCommand == other.slashCommand
            && actions == other.actions;
    }

    bool operator!=(const Keybind& other) const {
        return !(*this == other);
    }
};

} // namespace keybinder
