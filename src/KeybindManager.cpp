#include "../include/KeybindManager.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "../include/Logger.h"
#include "../include/SettingsManager.h"

namespace keybinder {

namespace {

std::string GetPersistentConfigPath() {
    if (PersistentPaths::EnsureLayout().empty()) {
        return std::string();
    }

    return PersistentPaths::ConfigFile();
}

}

namespace json {

inline std::string EscapeString(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

inline std::string UnescapeString(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[++i]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

inline std::string Indent(int depth) {
    return std::string(depth * 2, ' ');
}

}

KeybindManager::KeybindManager()
    : config_path_(GetPersistentConfigPath())
{}

KeybindManager::KeybindManager(const std::string& config_path)
    : config_path_(config_path)
{}

void KeybindManager::Add(Keybind keybind) {
    keybinds_.push_back(std::move(keybind));
    Logger::Info("[KeybindManager] Added keybind '%s' (key=%d, actions=%zu)",
        keybinds_.back().name.c_str(), keybinds_.back().key, keybinds_.back().actions.size());
}

bool KeybindManager::Remove(size_t index) {
    if (index >= keybinds_.size()) {
        Logger::Warning("[KeybindManager] Remove failed: index %zu out of range (count=%zu)", index, keybinds_.size());
        return false;
    }
    std::string name = keybinds_[index].name;
    keybinds_.erase(keybinds_.begin() + static_cast<std::vector<Keybind>::difference_type>(index));
    Logger::Info("[KeybindManager] Removed keybind '%s' at index %zu", name.c_str(), index);
    return true;
}

const Keybind* KeybindManager::Get(size_t index) const {
    if (index >= keybinds_.size()) return nullptr;
    return &keybinds_[index];
}

const Keybind* KeybindManager::GetByName(const std::string& name) const {
    for (const auto& kb : keybinds_) {
        if (kb.name == name) return &kb;
    }
    return nullptr;
}

std::vector<size_t> KeybindManager::GetByKey(int key) const {
    std::vector<size_t> result;
    for (size_t i = 0; i < keybinds_.size(); ++i) {
        const auto& kb = keybinds_[i];
        if (!kb.HasMultiKeys() && kb.key == key) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<size_t> KeybindManager::GetByKeys(const std::vector<int>& downKeys, int justPressedKey) const {
    std::vector<size_t> result;
    for (size_t i = 0; i < keybinds_.size(); ++i) {
        const auto& kb = keybinds_[i];
        
        std::vector<int> keybindKeys = kb.GetAllKeys();
        if (keybindKeys.empty()) continue;

        bool allKeysDown = true;
        for (int key : keybindKeys) {
            bool found = false;
            for (int downKey : downKeys) {
                if (downKey == key) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                allKeysDown = false;
                break;
            }
        }
        if (!allKeysDown) continue;

        bool hasJustPressed = false;
        for (int key : keybindKeys) {
            if (key == justPressedKey) {
                hasJustPressed = true;
                break;
            }
        }
        
        if (hasJustPressed) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<size_t> KeybindManager::GetBySlashCommand(const std::string& command) const {
    std::vector<size_t> result;
    if (command.empty()) return result;
    
    std::string normalizedInput = command;
    if (!normalizedInput.empty() && normalizedInput[0] == '/') {
        normalizedInput = normalizedInput.substr(1);
    }
    std::transform(normalizedInput.begin(), normalizedInput.end(), normalizedInput.begin(), ::tolower);
    
    for (size_t i = 0; i < keybinds_.size(); ++i) {
        const auto& kb = keybinds_[i];
        if (kb.slashCommand.empty()) continue;
        std::string normalizedStored = kb.slashCommand;
        if (!normalizedStored.empty() && normalizedStored[0] == '/') {
            normalizedStored = normalizedStored.substr(1);
        }
        std::transform(normalizedStored.begin(), normalizedStored.end(), normalizedStored.begin(), ::tolower);
        
        if (normalizedInput == normalizedStored) {
            result.push_back(i);
        }
    }
    return result;
}

bool KeybindManager::ValidateKeybind(const Keybind& kb) {
    if (kb.name.empty()) return false;
    bool hasKeyTrigger = kb.key != 0 || !kb.keys.empty();
    bool hasSlashCommandTrigger = !kb.slashCommand.empty();
    if (!hasKeyTrigger && !hasSlashCommandTrigger) return false;
    if (kb.actions.empty()) return false;
    for (const auto& action : kb.actions) {
        int type = static_cast<int>(action.type);
        if (type < 0 || type > 3) return false;
        if (action.type == ActionType::Chat || action.type == ActionType::Command) {
            if (action.data.empty()) return false;
        }
    }
    return true;
}

bool KeybindManager::Validate() const {
    for (const auto& kb : keybinds_) {
        if (!ValidateKeybind(kb)) return false;
    }
    return true;
}

namespace {

std::string ActionTypeToString(ActionType type) {
    switch (type) {
        case ActionType::Chat: return "chat";
        case ActionType::Command: return "command";
        case ActionType::Delay: return "delay";
        case ActionType::KeyPress: return "key";
        default: return "chat";
    }
}

ActionType StringToActionType(const std::string& s) {
    if (s == "command") return ActionType::Command;
    if (s == "delay") return ActionType::Delay;
    if (s == "key") return ActionType::KeyPress;
    return ActionType::Chat;
}

void WriteAction(const Action& action, std::ostream& os, int indent) {
    os << "{\n";
    os << json::Indent(indent + 1) << "\"type\": \"" << ActionTypeToString(action.type) << "\",\n";
    os << json::Indent(indent + 1) << "\"data\": \"" << json::EscapeString(action.data) << "\"\n";
    os << json::Indent(indent) << "}";
}

void WriteKeybind(const Keybind& kb, std::ostream& os, int indent) {
    os << "{\n";
    os << json::Indent(indent + 1) << "\"name\": \"" << json::EscapeString(kb.name) << "\",\n";
    os << json::Indent(indent + 1) << "\"key\": " << kb.key << ",\n";
    
    os << json::Indent(indent + 1) << "\"keys\": [";
    if (!kb.keys.empty()) {
        os << "\n";
        for (size_t i = 0; i < kb.keys.size(); ++i) {
            os << json::Indent(indent + 2) << kb.keys[i];
            if (i < kb.keys.size() - 1) os << ",";
            os << "\n";
        }
        os << json::Indent(indent + 1);
    }
    os << "],\n";
    
    os << json::Indent(indent + 1) << "\"category\": \"" << json::EscapeString(kb.category) << "\",\n";
    os << json::Indent(indent + 1) << "\"enabled\": " << (kb.enabled ? "true" : "false") << ",\n";
    os << json::Indent(indent + 1) << "\"passthrough\": " << (kb.passthrough ? "true" : "false") << ",\n";
    os << json::Indent(indent + 1) << "\"slashCommand\": \"" << json::EscapeString(kb.slashCommand) << "\",\n";
    os << json::Indent(indent + 1) << "\"actions\": [";
    if (!kb.actions.empty()) {
        os << "\n";
        for (size_t i = 0; i < kb.actions.size(); ++i) {
            os << json::Indent(indent + 2);
            WriteAction(kb.actions[i], os, indent + 1);
            if (i < kb.actions.size() - 1) os << ",";
            os << "\n";
        }
        os << json::Indent(indent + 1);
    }
    os << "]\n";
    os << json::Indent(indent) << "}";
}

}

bool KeybindManager::Save(SettingsManager* settingsManager) const {
    if (config_path_.empty()) {
        Logger::Error("[KeybindManager] Save: unable to resolve config path");
        return false;
    }

    std::ofstream file(config_path_);
    if (!file.is_open()) {
        Logger::Error("[KeybindManager] Save: failed to open '%s' for writing", config_path_.c_str());
        return false;
    }

    file << "{\n";
    file << json::Indent(1) << "\"version\": " << CONFIG_VERSION << ",\n";
    
    if (settingsManager != nullptr) {
        file << settingsManager->GetSettingsJson() << ",\n";
    }
    
    file << json::Indent(1) << "\"keybinds\": [";
    if (!keybinds_.empty()) {
        file << "\n";
        for (size_t i = 0; i < keybinds_.size(); ++i) {
            file << json::Indent(2);
            WriteKeybind(keybinds_[i], file, 2);
            if (i < keybinds_.size() - 1) file << ",";
            file << "\n";
        }
        file << json::Indent(1);
    }
    file << "]\n";
    file << "}\n";
    file.close();
    
    if (!file.good()) {
        Logger::Error("[KeybindManager] Save: failed while writing '%s'", config_path_.c_str());
        return false;
    }

    Logger::Info("[KeybindManager] Saved %zu keybind(s) to '%s'", keybinds_.size(), config_path_.c_str());
    return true;
}

namespace {

enum class JsonToken {
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Colon,
    Comma,
    String,
    Number,
    True,
    False,
    Null,
    EndOfFile
};

struct JsonTokenizer {
    std::istream& is;
    std::string token_str;
    double token_num;

    JsonTokenizer(std::istream& s) : is(s), token_num(0) {}

    JsonToken Next() {
        token_str.clear();
        char c;
        while (is.get(c)) {
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
            if (c == '{') return JsonToken::LeftBrace;
            if (c == '}') return JsonToken::RightBrace;
            if (c == '[') return JsonToken::LeftBracket;
            if (c == ']') return JsonToken::RightBracket;
            if (c == ':') return JsonToken::Colon;
            if (c == ',') return JsonToken::Comma;
            if (c == '"') {
                while (is.get(c)) {
                    if (c == '"') break;
                    if (c == '\\') {
                        is.get(c);
                        switch (c) {
                            case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't':
                                token_str += c;
                                break;
                            case 'u':
                                token_str += c;
                                for (int j = 0; j < 4; ++j) {
                                    is.get(c);
                                    token_str += c;
                                }
                                break;
                            default:
                                token_str += c;
                                break;
                        }
                    } else {
                        token_str += c;
                    }
                }
                return JsonToken::String;
            }
            if (c == '-' || (c >= '0' && c <= '9')) {
                token_str += c;
                while (is.peek() >= '0' && is.peek() <= '9') {
                    is.get(c);
                    token_str += c;
                }
                if (is.peek() == '.') {
                    is.get(c);
                    token_str += c;
                    while (is.peek() >= '0' && is.peek() <= '9') {
                        is.get(c);
                        token_str += c;
                    }
                }
                if (is.peek() == 'e' || is.peek() == 'E') {
                    is.get(c);
                    token_str += c;
                    if (is.peek() == '+' || is.peek() == '-') {
                        is.get(c);
                        token_str += c;
                    }
                    while (is.peek() >= '0' && is.peek() <= '9') {
                        is.get(c);
                        token_str += c;
                    }
                }
                token_num = std::atof(token_str.c_str());
                return JsonToken::Number;
            }
            if (c == 't') {
                token_str = "true";
                if (is.peek() == 'r') { is.get(c); token_str += c; }
                if (is.peek() == 'u') { is.get(c); token_str += c; }
                if (is.peek() == 'e') { is.get(c); token_str += c; }
                return JsonToken::True;
            }
            if (c == 'f') {
                token_str = "false";
                if (is.peek() == 'a') { is.get(c); token_str += c; }
                if (is.peek() == 'l') { is.get(c); token_str += c; }
                if (is.peek() == 's') { is.get(c); token_str += c; }
                if (is.peek() == 'e') { is.get(c); token_str += c; }
                return JsonToken::False;
            }
            if (c == 'n') {
                token_str = "null";
                return JsonToken::Null;
            }
        }
        return JsonToken::EndOfFile;
    }
};

class JsonParser {
public:
    JsonParser(std::istream& is) : tokenizer_(is), current_token_(JsonToken::EndOfFile) {
        NextToken();
    }

    bool ParseObject(std::vector<Keybind>& out_keybinds, std::string& out_settings_json) {
        if (current_token_ != JsonToken::LeftBrace) {
            Logger::Debug("[KeybindManager] ParseObject: Expected LeftBrace, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();

        while (current_token_ != JsonToken::RightBrace && current_token_ != JsonToken::EndOfFile) {
            if (current_token_ != JsonToken::String) {
                Logger::Debug("[KeybindManager] ParseObject: Expected String key, got token %d", static_cast<int>(current_token_));
                return false;
            }
            std::string key = tokenizer_.token_str;
            Logger::Debug("[KeybindManager] ParseObject: Processing key '%s'", key.c_str());
            NextToken();
            if (current_token_ != JsonToken::Colon) {
                Logger::Debug("[KeybindManager] ParseObject: Expected Colon after key '%s', got token %d", key.c_str(), static_cast<int>(current_token_));
                return false;
            }
            NextToken();

            if (key == "version") {
                if (current_token_ != JsonToken::Number) {
                    Logger::Debug("[KeybindManager] ParseObject: Expected Number for version, got token %d", static_cast<int>(current_token_));
                    return false;
                }
                NextToken();
            } else if (key == "settings") {
                out_settings_json = ExtractSettingsJson();
            } else if (key == "keybinds") {
                if (current_token_ != JsonToken::LeftBracket) {
                    Logger::Debug("[KeybindManager] ParseObject: Expected LeftBracket for keybinds, got token %d", static_cast<int>(current_token_));
                    return false;
                }
                NextToken();
                if (!ParseKeybindsArray(out_keybinds)) {
                    Logger::Debug("[KeybindManager] ParseObject: ParseKeybindsArray failed");
                    return false;
                }
            } else {
                Logger::Debug("[KeybindManager] ParseObject: Skipping unknown key '%s'", key.c_str());
                SkipValue();
            }

            if (current_token_ == JsonToken::Comma) {
                NextToken();
            }
        }

        if (current_token_ != JsonToken::RightBrace) {
            Logger::Debug("[KeybindManager] ParseObject: Expected RightBrace at end, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();
        return true;
    }

    private:
    std::string ExtractSettingsJson() {
        if (current_token_ != JsonToken::LeftBrace) {
            return "";
        }

        std::string result = "{";
        int brace_depth = 1;

        while (brace_depth > 0) {
            NextToken();
            if (current_token_ == JsonToken::EndOfFile) {
                break;
            }

            switch (current_token_) {
                case JsonToken::LeftBrace:
                    result += "{";
                    brace_depth++;
                    break;
                case JsonToken::RightBrace:
                    brace_depth--;
                    result += "}";
                    break;
                case JsonToken::LeftBracket:
                    result += "[";
                    break;
                case JsonToken::RightBracket:
                    result += "]";
                    break;
                case JsonToken::Colon:
                    result += ": ";
                    break;
                case JsonToken::Comma:
                    result += ", ";
                    break;
                case JsonToken::String:
                    result += "\"" + tokenizer_.token_str + "\"";
                    break;
                case JsonToken::Number:
                    result += tokenizer_.token_str;
                    break;
                case JsonToken::True:
                    result += "true";
                    break;
                case JsonToken::False:
                    result += "false";
                    break;
                case JsonToken::Null:
                    result += "null";
                    break;
                default:
                    break;
            }
        }

        NextToken();
        return result;
    }

    void SkipValue() {
        if (current_token_ == JsonToken::String ||
            current_token_ == JsonToken::Number ||
            current_token_ == JsonToken::True ||
            current_token_ == JsonToken::False ||
            current_token_ == JsonToken::Null) {
            NextToken();
        } else if (current_token_ == JsonToken::LeftBrace) {
            NextToken();
            while (current_token_ != JsonToken::RightBrace && current_token_ != JsonToken::EndOfFile) {
                if (current_token_ == JsonToken::String) {
                    NextToken();
                    if (current_token_ == JsonToken::Colon) {
                        NextToken();
                        SkipValue();
                    }
                } else {
                    NextToken();
                }
                if (current_token_ == JsonToken::Comma) {
                    NextToken();
                }
            }
            if (current_token_ == JsonToken::RightBrace) {
                NextToken();
            }
        } else if (current_token_ == JsonToken::LeftBracket) {
            NextToken();
            while (current_token_ != JsonToken::RightBracket && current_token_ != JsonToken::EndOfFile) {
                SkipValue();
                if (current_token_ == JsonToken::Comma) {
                    NextToken();
                }
            }
            if (current_token_ == JsonToken::RightBracket) {
                NextToken();
            }
        }
    }

    bool ParseKeybindsArray(std::vector<Keybind>& out_keybinds) {
        Logger::Debug("[KeybindManager] ParseKeybindsArray: Starting, token=%d", static_cast<int>(current_token_));
        if (current_token_ == JsonToken::RightBracket) {
            Logger::Debug("[KeybindManager] ParseKeybindsArray: Empty array");
            NextToken();
            return true;
        }

        int keybind_count = 0;
        do {
            if (current_token_ != JsonToken::LeftBrace) {
                Logger::Debug("[KeybindManager] ParseKeybindsArray: Expected LeftBrace for keybind %d, got token %d", keybind_count, static_cast<int>(current_token_));
                return false;
            }
            Keybind kb;
            if (!ParseKeybind(kb)) {
                Logger::Debug("[KeybindManager] ParseKeybindsArray: ParseKeybind failed for keybind %d", keybind_count);
                return false;
            }
            out_keybinds.push_back(std::move(kb));
            keybind_count++;
            Logger::Debug("[KeybindManager] ParseKeybindsArray: Parsed keybind %d, next token=%d", keybind_count, static_cast<int>(current_token_));

            if (current_token_ == JsonToken::Comma) {
                NextToken();
            }
        } while (current_token_ != JsonToken::RightBracket && current_token_ != JsonToken::EndOfFile);

        if (current_token_ != JsonToken::RightBracket) {
            Logger::Debug("[KeybindManager] ParseKeybindsArray: Expected RightBracket, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();
        Logger::Debug("[KeybindManager] ParseKeybindsArray: Success, parsed %d keybinds", keybind_count);
        return true;
    }

    bool ParseKeybind(Keybind& kb) {
        Logger::Debug("[KeybindManager] ParseKeybind: Starting, token=%d", static_cast<int>(current_token_));
        if (current_token_ != JsonToken::LeftBrace) {
            Logger::Debug("[KeybindManager] ParseKeybind: Expected LeftBrace, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();

        while (current_token_ != JsonToken::RightBrace && current_token_ != JsonToken::EndOfFile) {
            if (current_token_ != JsonToken::String) {
                Logger::Debug("[KeybindManager] ParseKeybind: Expected String key, got token %d", static_cast<int>(current_token_));
                return false;
            }

            std::string key = tokenizer_.token_str;
            Logger::Debug("[KeybindManager] ParseKeybind: Processing field '%s'", key.c_str());
            NextToken();
            if (current_token_ != JsonToken::Colon) {
                Logger::Debug("[KeybindManager] ParseKeybind: Expected Colon after '%s', got token %d", key.c_str(), static_cast<int>(current_token_));
                return false;
            }
            NextToken();

            if (key == "name") {
                if (current_token_ != JsonToken::String) return false;
                kb.name = json::UnescapeString(tokenizer_.token_str);
                NextToken();
            } else if (key == "key") {
                if (current_token_ != JsonToken::Number) return false;
                kb.key = static_cast<int>(tokenizer_.token_num);
                NextToken();
            } else if (key == "keys") {
                if (current_token_ != JsonToken::LeftBracket) return false;
                NextToken();
                if (!ParseKeysArray(kb.keys)) return false;
            } else if (key == "category") {
                if (current_token_ != JsonToken::String) return false;
                kb.category = json::UnescapeString(tokenizer_.token_str);
                NextToken();
            } else if (key == "enabled") {
                if (current_token_ == JsonToken::True) kb.enabled = true;
                else if (current_token_ == JsonToken::False) kb.enabled = false;
                else return false;
                NextToken();
            } else if (key == "passthrough") {
                if (current_token_ == JsonToken::True) kb.passthrough = true;
                else if (current_token_ == JsonToken::False) kb.passthrough = false;
                else return false;
                NextToken();
            } else if (key == "slashCommand") {
                if (current_token_ != JsonToken::String) return false;
                kb.slashCommand = json::UnescapeString(tokenizer_.token_str);
                NextToken();
            } else if (key == "actions") {
                if (current_token_ != JsonToken::LeftBracket) return false;
                NextToken();
                if (!ParseActionsArray(kb.actions)) return false;
            }

            if (current_token_ == JsonToken::Comma) {
                NextToken();
            }
        }

        if (current_token_ != JsonToken::RightBrace) return false;
        NextToken();
        return true;
    }

    bool ParseActionsArray(std::vector<Action>& out_actions) {
        Logger::Debug("[KeybindManager] ParseActionsArray: Starting, token=%d", static_cast<int>(current_token_));
        if (current_token_ == JsonToken::RightBracket) {
            Logger::Debug("[KeybindManager] ParseActionsArray: Empty array");
            NextToken();
            return true;
        }

        int action_count = 0;
        do {
            if (current_token_ != JsonToken::LeftBrace) {
                Logger::Debug("[KeybindManager] ParseActionsArray: Expected LeftBrace for action %d, got token %d", action_count, static_cast<int>(current_token_));
                return false;
            }
            Action action;
            if (!ParseAction(action)) {
                Logger::Debug("[KeybindManager] ParseActionsArray: ParseAction failed for action %d", action_count);
                return false;
            }
            out_actions.push_back(std::move(action));
            action_count++;
            Logger::Debug("[KeybindManager] ParseActionsArray: Parsed action %d, next token=%d", action_count, static_cast<int>(current_token_));

            if (current_token_ == JsonToken::Comma) {
                NextToken();
            }
        } while (current_token_ != JsonToken::RightBracket && current_token_ != JsonToken::EndOfFile);

        if (current_token_ != JsonToken::RightBracket) {
            Logger::Debug("[KeybindManager] ParseActionsArray: Expected RightBracket, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();
        Logger::Debug("[KeybindManager] ParseActionsArray: Success, parsed %d actions", action_count);
        return true;
    }

    bool ParseKeysArray(std::vector<int>& out_keys) {
        Logger::Debug("[KeybindManager] ParseKeysArray: Starting, token=%d", static_cast<int>(current_token_));
        if (current_token_ == JsonToken::RightBracket) {
            Logger::Debug("[KeybindManager] ParseKeysArray: Empty array");
            NextToken();
            return true;
        }

        int key_count = 0;
        do {
            if (current_token_ != JsonToken::Number) {
                Logger::Debug("[KeybindManager] ParseKeysArray: Expected Number for key %d, got token %d", key_count, static_cast<int>(current_token_));
                return false;
            }
            out_keys.push_back(static_cast<int>(tokenizer_.token_num));
            key_count++;
            Logger::Debug("[KeybindManager] ParseKeysArray: Parsed key %d (vk=%d)", key_count, static_cast<int>(tokenizer_.token_num));
            NextToken();

            if (current_token_ == JsonToken::Comma) {
                NextToken();
            }
        } while (current_token_ != JsonToken::RightBracket && current_token_ != JsonToken::EndOfFile);

        if (current_token_ != JsonToken::RightBracket) {
            Logger::Debug("[KeybindManager] ParseKeysArray: Expected RightBracket, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();
        Logger::Debug("[KeybindManager] ParseKeysArray: Success, parsed %d keys", key_count);
        return true;
    }

    bool ParseAction(Action& action) {
        Logger::Debug("[KeybindManager] ParseAction: Starting, token=%d", static_cast<int>(current_token_));
        if (current_token_ != JsonToken::LeftBrace) {
            Logger::Debug("[KeybindManager] ParseAction: Expected LeftBrace, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();

        while (current_token_ != JsonToken::RightBrace && current_token_ != JsonToken::EndOfFile) {
            if (current_token_ != JsonToken::String) {
                Logger::Debug("[KeybindManager] ParseAction: Expected String key, got token %d", static_cast<int>(current_token_));
                return false;
            }

            std::string key = tokenizer_.token_str;
            Logger::Debug("[KeybindManager] ParseAction: Processing field '%s'", key.c_str());
            NextToken();
            if (current_token_ != JsonToken::Colon) {
                Logger::Debug("[KeybindManager] ParseAction: Expected Colon after '%s', got token %d", key.c_str(), static_cast<int>(current_token_));
                return false;
            }
            NextToken();

            if (key == "type") {
                if (current_token_ != JsonToken::String) {
                    Logger::Debug("[KeybindManager] ParseAction: Expected String for type, got token %d", static_cast<int>(current_token_));
                    return false;
                }
                action.type = StringToActionType(tokenizer_.token_str);
                NextToken();
            } else if (key == "data") {
                if (current_token_ != JsonToken::String) {
                    Logger::Debug("[KeybindManager] ParseAction: Expected String for data, got token %d", static_cast<int>(current_token_));
                    return false;
                }
                action.data = json::UnescapeString(tokenizer_.token_str);
                NextToken();
            } else {
                Logger::Debug("[KeybindManager] ParseAction: Unknown field '%s', skipping", key.c_str());
                if (current_token_ == JsonToken::String) {
                    NextToken();
                } else if (current_token_ == JsonToken::Number) {
                    NextToken();
                } else if (current_token_ == JsonToken::True || current_token_ == JsonToken::False) {
                    NextToken();
                } else if (current_token_ == JsonToken::LeftBrace) {
                    int brace_depth = 1;
                    while (current_token_ != JsonToken::EndOfFile && brace_depth > 0) {
                        NextToken();
                        if (current_token_ == JsonToken::LeftBrace) brace_depth++;
                        else if (current_token_ == JsonToken::RightBrace) brace_depth--;
                    }
                    if (current_token_ == JsonToken::RightBrace) NextToken();
                } else if (current_token_ == JsonToken::LeftBracket) {
                    int bracket_depth = 1;
                    while (current_token_ != JsonToken::EndOfFile && bracket_depth > 0) {
                        NextToken();
                        if (current_token_ == JsonToken::LeftBracket) bracket_depth++;
                        else if (current_token_ == JsonToken::RightBracket) bracket_depth--;
                    }
                    if (current_token_ == JsonToken::RightBracket) NextToken();
                }
            }

            if (current_token_ == JsonToken::Comma) {
                NextToken();
            }
        }

        if (current_token_ != JsonToken::RightBrace) {
            Logger::Debug("[KeybindManager] ParseAction: Expected RightBrace at end, got token %d", static_cast<int>(current_token_));
            return false;
        }
        NextToken();
        Logger::Debug("[KeybindManager] ParseAction: Success");
        return true;
    }

    void NextToken() {
        current_token_ = tokenizer_.Next();
    }

    JsonTokenizer tokenizer_;
    JsonToken current_token_;
};

}

bool KeybindManager::Load(SettingsManager* settingsManager) {
    if (config_path_.empty()) {
        Logger::Error("[KeybindManager] Load: unable to resolve config path");
        keybinds_.clear();
        return false;
    }

    std::ifstream file(config_path_);
    if (!file.is_open()) {
        Logger::Info("[KeybindManager] No config file '%s' found; starting with empty state", config_path_.c_str());
        keybinds_.clear();
        return true;
    }

    JsonParser parser(file);
    std::vector<Keybind> parsed_keybinds;
    std::string settings_json;
    
    if (!parser.ParseObject(parsed_keybinds, settings_json)) {
        Logger::Error("[KeybindManager] Parse error in '%s'; falling back to empty state", config_path_.c_str());
        file.close();

        std::ifstream debug_file(config_path_);
        if (debug_file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(debug_file)),
                                 std::istreambuf_iterator<char>());
            Logger::Debug("[KeybindManager] File content: %s", content.c_str());
            debug_file.close();
        }
        
        keybinds_.clear();
        return false;
    }

    keybinds_ = std::move(parsed_keybinds);
    
    if (settingsManager != nullptr && !settings_json.empty()) {
        settingsManager->LoadFromJson(settings_json);
    }
    
    Logger::Info("[KeybindManager] Loaded %zu keybind(s) from '%s'", keybinds_.size(), config_path_.c_str());
    return true;
}

}
