#pragma once

#include <string>
#include <vector>
#include "Keybind.h"
#include "PersistentPaths.h"

namespace keybinder {

class SettingsManager;

class KeybindManager {
public:
    static constexpr int CONFIG_VERSION = 1;

    KeybindManager();
    explicit KeybindManager(const std::string& config_path);

    bool Load(SettingsManager* settingsManager = nullptr);
    bool Save(SettingsManager* settingsManager = nullptr) const;

    void Add(Keybind keybind);
    bool Remove(size_t index);
    const Keybind* Get(size_t index) const;
    const Keybind* GetByName(const std::string& name) const;

    std::vector<size_t> GetByKey(int key) const;

    std::vector<size_t> GetByKeys(const std::vector<int>& downKeys, int justPressedKey) const;

    std::vector<size_t> GetBySlashCommand(const std::string& command) const;

    size_t Count() const { return keybinds_.size(); }
    const std::vector<Keybind>& GetAll() const { return keybinds_; }

    std::vector<Keybind>& GetAll() { return keybinds_; }

    bool Validate() const;

private:
    std::vector<Keybind> keybinds_;
    std::string config_path_;

    static bool ValidateKeybind(const Keybind& kb);
};

} // namespace keybinder
