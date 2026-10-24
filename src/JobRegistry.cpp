#include "../include/JobRegistry.h"

#include "../include/Logger.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>

#include <windows.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace jobs {

namespace {

struct EmbeddedResourceView {
    const void* data = nullptr;
    std::size_t size = 0;
};

std::string TrimWhitespace(std::string value) {
    const auto isWhitespace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) {
        return !isWhitespace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) {
        return !isWhitespace(ch);
    }).base(), value.end());
    return value;
}

bool IsEscapedJsonCharacter(const std::string& text, std::size_t pos) {
    if (pos == 0 || pos >= text.size()) {
        return false;
    }

    std::size_t backslashCount = 0;
    std::size_t checkPos = pos;
    while (checkPos > 0 && text[checkPos - 1] == '\\') {
        ++backslashCount;
        --checkPos;
    }

    return (backslashCount % 2) != 0;
}

std::string UnescapeJsonString(const std::string& value) {
    std::string unescaped;
    unescaped.reserve(value.size());

    const auto appendUtf8 = [&unescaped](unsigned int codePoint) {
        if (codePoint <= 0x7F) {
            unescaped.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            unescaped.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
            unescaped.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0xFFFF) {
            unescaped.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
            unescaped.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            unescaped.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            unescaped.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
            unescaped.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            unescaped.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            unescaped.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    };

    const auto parseHex4 = [&value](std::size_t start, unsigned int& outCodePoint) {
        if (start + 4 > value.size()) {
            return false;
        }

        outCodePoint = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            const char hex = value[start + j];
            outCodePoint <<= 4;
            if (hex >= '0' && hex <= '9') {
                outCodePoint |= static_cast<unsigned int>(hex - '0');
            } else if (hex >= 'a' && hex <= 'f') {
                outCodePoint |= static_cast<unsigned int>(hex - 'a' + 10);
            } else if (hex >= 'A' && hex <= 'F') {
                outCodePoint |= static_cast<unsigned int>(hex - 'A' + 10);
            } else {
                return false;
            }
        }

        return true;
    };

    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch != '\\' || i + 1 >= value.size()) {
            unescaped.push_back(ch);
            continue;
        }

        const char escaped = value[++i];
        switch (escaped) {
            case '"': unescaped.push_back('"'); break;
            case '\\': unescaped.push_back('\\'); break;
            case '/': unescaped.push_back('/'); break;
            case 'b': unescaped.push_back('\b'); break;
            case 'f': unescaped.push_back('\f'); break;
            case 'n': unescaped.push_back('\n'); break;
            case 'r': unescaped.push_back('\r'); break;
            case 't': unescaped.push_back('\t'); break;
            case 'u': {
                unsigned int codePoint = 0;
                if (!parseHex4(i + 1, codePoint)) {
                    unescaped.push_back('u');
                    break;
                }

                i += 4;
                appendUtf8(codePoint);
                break;
            }
            default:
                unescaped.push_back(escaped);
                break;
        }
    }

    return unescaped;
}

std::size_t FindMatchingDelimiter(const std::string& text, std::size_t openPos, char openDelimiter, char closeDelimiter) {
    if (openPos == std::string::npos || openPos >= text.size() || text[openPos] != openDelimiter) {
        return std::string::npos;
    }

    std::size_t depth = 0;
    bool inString = false;
    for (std::size_t i = openPos; i < text.size(); ++i) {
        const char ch = text[i];

        if (ch == '"' && !IsEscapedJsonCharacter(text, i)) {
            inString = !inString;
            continue;
        }

        if (inString) {
            continue;
        }

        if (ch == openDelimiter) {
            ++depth;
        } else if (ch == closeDelimiter) {
            if (depth == 0) {
                return std::string::npos;
            }

            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

bool FindFieldColon(const std::string& object, const char* key, std::size_t& colonPos) {
    const std::size_t keyPos = object.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    colonPos = object.find(':', keyPos);
    return colonPos != std::string::npos;
}

bool ParseBoolField(const std::string& object, const char* key, bool& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t valuePos = object.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valuePos == std::string::npos) {
        return false;
    }

    if (object.compare(valuePos, 4, "true") == 0) {
        outValue = true;
        return true;
    }

    if (object.compare(valuePos, 5, "false") == 0) {
        outValue = false;
        return true;
    }

    return false;
}

bool ParseIntField(const std::string& object, const char* key, int& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t numStart = object.find_first_of("0123456789-", colonPos + 1);
    if (numStart == std::string::npos) {
        return false;
    }

    const std::size_t numEnd = object.find_first_not_of("0123456789-", numStart);
    outValue = std::atoi(object.substr(numStart, numEnd - numStart).c_str());
    return true;
}

bool ParseStringField(const std::string& object, const char* key, std::string& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t firstQuote = object.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return false;
    }

    std::size_t secondQuote = firstQuote + 1;
    while (secondQuote < object.size()) {
        secondQuote = object.find('"', secondQuote);
        if (secondQuote == std::string::npos) {
            return false;
        }

        if (!IsEscapedJsonCharacter(object, secondQuote)) {
            break;
        }

        ++secondQuote;
    }

    if (secondQuote == std::string::npos || secondQuote >= object.size()) {
        return false;
    }

    outValue = UnescapeJsonString(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
    return true;
}

bool ExtractJsonArrayField(const std::string& object, const char* key, std::string& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t arrayStart = object.find('[', colonPos + 1);
    if (arrayStart == std::string::npos) {
        return false;
    }

    const std::size_t arrayEnd = FindMatchingDelimiter(object, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos) {
        return false;
    }

    outValue = object.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
    return true;
}

bool ExtractJsonObjectField(const std::string& object, const char* key, std::string& outValue) {
    std::size_t colonPos = std::string::npos;
    if (!FindFieldColon(object, key, colonPos)) {
        return false;
    }

    const std::size_t objectStart = object.find('{', colonPos + 1);
    if (objectStart == std::string::npos) {
        return false;
    }

    const std::size_t objectEnd = FindMatchingDelimiter(object, objectStart, '{', '}');
    if (objectEnd == std::string::npos) {
        return false;
    }

    outValue = object.substr(objectStart + 1, objectEnd - objectStart - 1);
    return true;
}

std::vector<std::string> ExtractObjectEntries(const std::string& arrayBody) {
    std::vector<std::string> objects;
    std::size_t cursor = 0;

    while (cursor < arrayBody.size()) {
        const std::size_t openPos = arrayBody.find('{', cursor);
        if (openPos == std::string::npos) {
            break;
        }

        const std::size_t closePos = FindMatchingDelimiter(arrayBody, openPos, '{', '}');
        if (closePos == std::string::npos) {
            objects.clear();
            return objects;
        }

        objects.push_back(arrayBody.substr(openPos, closePos - openPos + 1));
        cursor = closePos + 1;
    }

    return objects;
}

bool ParseCaptureGroups(const std::string& object, std::unordered_map<std::string, int>& captureGroups) {
    captureGroups.clear();
    std::size_t cursor = 0;

    while (cursor < object.size()) {
        const std::size_t keyStart = object.find('"', cursor);
        if (keyStart == std::string::npos) {
            break;
        }

        std::size_t keyEnd = keyStart + 1;
        while (keyEnd < object.size()) {
            keyEnd = object.find('"', keyEnd);
            if (keyEnd == std::string::npos) {
                return false;
            }

            if (!IsEscapedJsonCharacter(object, keyEnd)) {
                break;
            }

            ++keyEnd;
        }

        if (keyEnd == std::string::npos) {
            return false;
        }

        const std::string key = UnescapeJsonString(object.substr(keyStart + 1, keyEnd - keyStart - 1));
        const std::size_t colonPos = object.find(':', keyEnd + 1);
        if (colonPos == std::string::npos) {
            return false;
        }

        const std::size_t valueStart = object.find_first_of("0123456789-", colonPos + 1);
        if (valueStart == std::string::npos) {
            return false;
        }

        const std::size_t valueEnd = object.find_first_not_of("0123456789-", valueStart);
        captureGroups[key] = std::atoi(object.substr(valueStart, valueEnd - valueStart).c_str());
        cursor = valueEnd == std::string::npos ? object.size() : valueEnd;
    }

    return true;
}

bool ParseStringArray(const std::string& arrayBody, std::vector<std::string>& values) {
    values.clear();
    std::size_t cursor = 0;

    while (cursor < arrayBody.size()) {
        const std::size_t valueStart = arrayBody.find('"', cursor);
        if (valueStart == std::string::npos) {
            break;
        }

        std::size_t valueEnd = valueStart + 1;
        while (valueEnd < arrayBody.size()) {
            valueEnd = arrayBody.find('"', valueEnd);
            if (valueEnd == std::string::npos) {
                return false;
            }

            if (!IsEscapedJsonCharacter(arrayBody, valueEnd)) {
                break;
            }

            ++valueEnd;
        }

        if (valueEnd == std::string::npos) {
            return false;
        }

        values.push_back(UnescapeJsonString(arrayBody.substr(valueStart + 1, valueEnd - valueStart - 1)));
        cursor = valueEnd + 1;
    }

    return true;
}

bool ParseParserRule(const std::string& object, ParserRule& rule) {
    rule = {};

    if (!ParseStringField(object, "\"pattern\"", rule.pattern)) {
        return false;
    }

    if (!ParseStringField(object, "\"eventType\"", rule.eventType)) {
        return false;
    }

    std::string captureGroupsObject;
    if (ExtractJsonObjectField(object, "\"captureGroups\"", captureGroupsObject)) {
        if (!ParseCaptureGroups(captureGroupsObject, rule.captureGroups)) {
            return false;
        }
    }

    return true;
}

bool ParseSkillTreeNode(const std::string& object, SkillTreeNode& node) {
    node = {};

    if (!ParseStringField(object, "\"id\"", node.id)) {
        return false;
    }

    if (!ParseStringField(object, "\"name\"", node.name)) {
        return false;
    }

    if (!ParseIntField(object, "\"maxPoints\"", node.maxPoints)) {
        return false;
    }

    if (!ParseIntField(object, "\"unlockOrder\"", node.unlockOrder)) {
        return false;
    }

    ParseStringField(object, "\"perkType\"", node.perkType);
    ParseStringField(object, "\"perkValue\"", node.perkValue);

    return true;
}

bool ParseJobDefinition(const std::string& object, JobDefinition& definition) {
    definition = {};

    if (!ParseStringField(object, "\"id\"", definition.id)) {
        return false;
    }

    if (!ParseStringField(object, "\"name\"", definition.name)) {
        return false;
    }

    if (!ParseBoolField(object, "\"enabled\"", definition.enabled)) {
        return false;
    }

    if (!ParseStringField(object, "\"icon\"", definition.icon)) {
        return false;
    }

    std::string speakerAliasesArray;
    if (ExtractJsonArrayField(object, "\"speakerAliases\"", speakerAliasesArray)) {
        if (!ParseStringArray(speakerAliasesArray, definition.speakerAliases)) {
            return false;
        }
    }

    std::string parserRulesArray;
    if (ExtractJsonArrayField(object, "\"parserRules\"", parserRulesArray)) {
        const std::vector<std::string> parserRuleObjects = ExtractObjectEntries(parserRulesArray);
        if (!parserRulesArray.empty() && parserRuleObjects.empty()) {
            return false;
        }

        for (const std::string& parserRuleObject : parserRuleObjects) {
            ParserRule rule;
            if (!ParseParserRule(parserRuleObject, rule)) {
                return false;
            }

            definition.parserRules.push_back(std::move(rule));
        }
    }

    std::string skillTreeArray;
    if (ExtractJsonArrayField(object, "\"skillTree\"", skillTreeArray)) {
        const std::vector<std::string> skillTreeObjects = ExtractObjectEntries(skillTreeArray);
        if (!skillTreeArray.empty() && skillTreeObjects.empty()) {
            return false;
        }

        for (const std::string& skillTreeObject : skillTreeObjects) {
            SkillTreeNode node;
            if (!ParseSkillTreeNode(skillTreeObject, node)) {
                return false;
            }

            definition.skillTree.push_back(std::move(node));
        }
    }

    return true;
}

bool TryLoadEmbeddedResource(std::uint16_t resourceId, EmbeddedResourceView* outResource) {
    if (outResource == nullptr) {
        return false;
    }

    *outResource = {};

    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    if (module == nullptr) {
        return false;
    }

    const LPCWSTR resourceType = reinterpret_cast<LPCWSTR>(RT_RCDATA);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), resourceType);
    if (resource == nullptr) {
        return false;
    }

    const DWORD resourceSize = SizeofResource(module, resource);
    if (resourceSize == 0) {
        return false;
    }

    const HGLOBAL loadedResource = LoadResource(module, resource);
    if (loadedResource == nullptr) {
        return false;
    }

    const void* resourceData = LockResource(loadedResource);
    if (resourceData == nullptr) {
        return false;
    }

    outResource->data = resourceData;
    outResource->size = static_cast<std::size_t>(resourceSize);
    return true;
}

} // namespace

JobRegistry::JobRegistry(std::uint16_t resourceId)
    : resourceId_(resourceId) {}

bool JobRegistry::LoadEmbedded(Snapshot& snapshot) const {
    snapshot = {};

    EmbeddedResourceView resource;
    if (!TryLoadEmbeddedResource(resourceId_, &resource)) {
        keybinder::Logger::Warning("[JobRegistry] Failed to load embedded jobs registry resource: %u", static_cast<unsigned>(resourceId_));
        return false;
    }

    const std::string json(reinterpret_cast<const char*>(resource.data), resource.size);
    return LoadFromJson(json, snapshot);
}

bool JobRegistry::LoadFromJson(const std::string& json, Snapshot& snapshot) const {
    snapshot = {};

    if (TrimWhitespace(json).empty()) {
        keybinder::Logger::Warning("[JobRegistry] Registry payload is empty");
        return false;
    }

    Snapshot parsed;
    if (!ParseIntField(json, "\"version\"", parsed.version)) {
        keybinder::Logger::Warning("[JobRegistry] Registry payload is missing a valid version field");
        return false;
    }

    std::string jobsArray;
    if (!ExtractJsonArrayField(json, "\"jobs\"", jobsArray)) {
        keybinder::Logger::Warning("[JobRegistry] Registry payload is missing jobs array");
        return false;
    }

    const std::vector<std::string> jobObjects = ExtractObjectEntries(jobsArray);
    if (!jobsArray.empty() && jobObjects.empty()) {
        keybinder::Logger::Warning("[JobRegistry] Registry payload contains malformed job objects");
        return false;
    }

    for (const std::string& jobObject : jobObjects) {
        JobDefinition definition;
        if (!ParseJobDefinition(jobObject, definition)) {
            keybinder::Logger::Warning("[JobRegistry] Failed to parse job definition from registry payload");
            snapshot = {};
            return false;
        }

        parsed.jobs.push_back(std::move(definition));
    }

    if (!Validate(parsed)) {
        keybinder::Logger::Warning("[JobRegistry] Registry payload failed validation");
        snapshot = {};
        return false;
    }

    snapshot = std::move(parsed);
    keybinder::Logger::Info("[JobRegistry] Loaded %zu job definitions from registry payload", snapshot.jobs.size());
    return true;
}

bool JobRegistry::Validate(const Snapshot& snapshot) {
    if (snapshot.version <= 0) {
        return false;
    }

    if (snapshot.jobs.size() != kExpectedJobDefinitionCount) {
        return false;
    }

    std::unordered_set<std::string> jobIds;
    for (const JobDefinition& definition : snapshot.jobs) {
        if (TrimWhitespace(definition.id).empty() || TrimWhitespace(definition.name).empty()) {
            return false;
        }

        if (!jobIds.insert(definition.id).second) {
            return false;
        }

        for (const std::string& alias : definition.speakerAliases) {
            if (TrimWhitespace(alias).empty()) {
                return false;
            }
        }

        std::unordered_set<int> unlockOrders;
        std::unordered_set<std::string> skillIds;
        for (const SkillTreeNode& node : definition.skillTree) {
            if (TrimWhitespace(node.id).empty() || TrimWhitespace(node.name).empty()) {
                return false;
            }

            if (node.maxPoints <= 0 || node.unlockOrder <= 0) {
                return false;
            }

            if (!skillIds.insert(node.id).second) {
                return false;
            }

            if (!unlockOrders.insert(node.unlockOrder).second) {
                return false;
            }
        }

        for (const ParserRule& rule : definition.parserRules) {
            if (TrimWhitespace(rule.pattern).empty() || TrimWhitespace(rule.eventType).empty()) {
                return false;
            }

            for (const auto& captureGroup : rule.captureGroups) {
                if (TrimWhitespace(captureGroup.first).empty() || captureGroup.second <= 0) {
                    return false;
                }
            }
        }
    }

    return true;
}

} // namespace jobs
