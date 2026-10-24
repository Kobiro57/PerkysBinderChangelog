#include "../include/OnlinePlayerInfoService.h"

#include "../include/Logger.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace keybinder {

namespace {

constexpr const char* kPlayersApiUrl = "https://nova-legacy.de/api/players/online";
constexpr std::size_t kMaxChatMessageLength = 144;

std::string TrimWhitespace(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string TruncateForSampChat(std::string text) {
    if (text.size() <= kMaxChatMessageLength) {
        return text;
    }

    if (kMaxChatMessageLength <= 3) {
        text.resize(kMaxChatMessageLength);
        return text;
    }

    text.resize(kMaxChatMessageLength - 3);
    text += "...";
    return text;
}

bool NormalizeOptionalText(bool hasValue, std::string& value) {
    if (!hasValue) {
        value.clear();
        return false;
    }

    value = TrimWhitespace(value);
    if (value.empty()) {
        value.clear();
        return false;
    }

    return true;
}

void AppendUtf8CodePoint(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
        return;
    }

    if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }

    if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }

    output.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
    output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
}

bool IsHexDigit(char ch) {
    return (ch >= '0' && ch <= '9')
        || (ch >= 'a' && ch <= 'f')
        || (ch >= 'A' && ch <= 'F');
}

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }

    return 10 + (ch - 'A');
}

class JsonReader final {
public:
    explicit JsonReader(const std::string& input)
        : input_(input) {
    }

    bool ParsePlayers(std::vector<OnlinePlayerInfo>& outPlayers) {
        outPlayers.clear();

        if (!Expect('{', "response root must be a JSON object")) {
            return false;
        }

        bool foundPlayers = false;
        SkipWhitespace();
        if (Consume('}')) {
            return Fail("response object is missing players array");
        }

        while (true) {
            std::string key;
            if (!ParseString(key)) {
                return Fail("expected root object key");
            }
            if (!Expect(':', "expected ':' after root object key")) {
                return false;
            }

            if (key == "players") {
                if (!ParsePlayersArray(outPlayers)) {
                    return false;
                }
                foundPlayers = true;
            } else if (!SkipValue()) {
                return false;
            }

            SkipWhitespace();
            if (Consume('}')) {
                break;
            }

            if (!Consume(',')) {
                return Fail("expected ',' or '}' after root field");
            }
        }

        SkipWhitespace();
        if (!foundPlayers) {
            return Fail("response object is missing players array");
        }
        if (position_ != input_.size()) {
            return Fail("unexpected trailing data after response object");
        }
        return true;
    }

    const std::string& GetError() const {
        return error_;
    }

private:
    bool ParsePlayersArray(std::vector<OnlinePlayerInfo>& outPlayers) {
        if (!Expect('[', "players field must be an array")) {
            return false;
        }

        SkipWhitespace();
        if (Consume(']')) {
            return true;
        }

        std::size_t playerIndex = 0;

        while (true) {
            currentPlayerIndex_ = playerIndex;
            insidePlayer_ = true;
            currentField_.clear();
            OnlinePlayerInfo player;
            bool usablePlayer = false;
            if (!ParsePlayerObject(player, usablePlayer)) {
                return error_.empty() ? Fail("failed parsing player object") : false;
            }

            currentField_.clear();

            if (usablePlayer) {
                outPlayers.push_back(player);
            }

            SkipWhitespace();
            if (Consume(']')) {
                insidePlayer_ = false;
                break;
            }

            if (!Consume(',')) {
                return Fail("expected ',' or ']' after player object");
            }

            insidePlayer_ = false;
            ++playerIndex;
        }

        return true;
    }

    bool ParsePlayerObject(OnlinePlayerInfo& outPlayer, bool& outUsablePlayer) {
        if (!Expect('{', "player entry must be an object")) {
            return false;
        }

        outUsablePlayer = false;
        bool hasName = false;
        bool nameHasValue = false;
        bool hasLevel = false;

        SkipWhitespace();
        if (Consume('}')) {
            return Fail("player object is empty");
        }

        while (true) {
            std::string key;
            currentField_.clear();
            if (!ParseString(key)) {
                return Fail("expected player field name");
            }
            currentField_ = key;
            if (!Expect(':', "expected ':' after player field name")) {
                return false;
            }

            if (key == "id") {
                if (!ParseString(outPlayer.id)) {
                    return false;
                }
            } else if (key == "name") {
                if (!ParseNullableText(outPlayer.name, nameHasValue)) {
                    return false;
                }
                hasName = true;
            } else if (key == "level") {
                if (!ParseInteger(outPlayer.level)) {
                    return false;
                }
                hasLevel = true;
            } else if (key == "faction") {
                if (!ParseNullableString(outPlayer.hasFaction, outPlayer.faction)) {
                    return false;
                }
            } else if (key == "gang") {
                if (!ParseNullableString(outPlayer.hasGang, outPlayer.gang)) {
                    return false;
                }
            } else if (key == "skinId") {
                if (!ParseNullableInteger(outPlayer.skinId)) {
                    return false;
                }
            } else if (key == "teamRank") {
                if (!ParseNullableString(outPlayer.hasTeamRank, outPlayer.teamRank)) {
                    return false;
                }
            } else if (key == "wanted") {
                if (!ParseNullableInteger(outPlayer.wanted)) {
                    return false;
                }
            } else if (!SkipValue()) {
                return false;
            }

            SkipWhitespace();
            if (Consume('}')) {
                break;
            }

            if (!Consume(',')) {
                return Fail("expected ',' or '}' after player field");
            }
        }

        outPlayer.hasFaction = NormalizeOptionalText(outPlayer.hasFaction, outPlayer.faction);
        outPlayer.hasGang = NormalizeOptionalText(outPlayer.hasGang, outPlayer.gang);
        outPlayer.hasTeamRank = NormalizeOptionalText(outPlayer.hasTeamRank, outPlayer.teamRank);
        outPlayer.name = TrimWhitespace(outPlayer.name);
        currentField_.clear();

        if (!hasName) {
            return Fail("player missing required name field");
        }
        if (!hasLevel) {
            return Fail("player missing required level field");
        }

        outUsablePlayer = nameHasValue && !outPlayer.name.empty();
        return true;
    }

    bool ParseNullableText(std::string& outValue, bool& hasValue) {
        SkipWhitespace();
        if (StartsWith("null")) {
            position_ += 4;
            hasValue = false;
            outValue.clear();
            return true;
        }

        if (!ParseString(outValue)) {
            return Fail("expected string or null");
        }

        hasValue = true;
        return true;
    }

    bool ParseNullableString(bool& hasValue, std::string& outValue) {
        return ParseNullableText(outValue, hasValue);
    }

    bool ParseNullableInteger(int& outValue) {
        SkipWhitespace();
        if (StartsWith("null")) {
            position_ += 4;
            outValue = 0;
            return true;
        }

        return ParseInteger(outValue);
    }
    bool ParseInteger(int& outValue) {
        SkipWhitespace();
        if (position_ >= input_.size()) {
            return Fail("expected integer");
        }

        std::size_t start = position_;
        if (input_[position_] == '-') {
            ++position_;
        }

        if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            return Fail("expected integer digit");
        }

        while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }

        try {
            outValue = std::stoi(input_.substr(start, position_ - start));
        } catch (...) {
            return Fail("integer value is out of range");
        }

        return true;
    }

    bool ParseString(std::string& outValue) {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] != '"') {
            return Fail("expected JSON string");
        }

        ++position_;
        outValue.clear();

        while (position_ < input_.size()) {
            const char ch = input_[position_++];
            if (ch == '"') {
                return true;
            }

            if (ch != '\\') {
                outValue.push_back(ch);
                continue;
            }

            if (position_ >= input_.size()) {
                return Fail("unterminated JSON escape sequence");
            }

            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': outValue.push_back('"'); break;
            case '\\': outValue.push_back('\\'); break;
            case '/': outValue.push_back('/'); break;
            case 'b': outValue.push_back('\b'); break;
            case 'f': outValue.push_back('\f'); break;
            case 'n': outValue.push_back('\n'); break;
            case 'r': outValue.push_back('\r'); break;
            case 't': outValue.push_back('\t'); break;
            case 'u': {
                if (position_ + 4 > input_.size()) {
                    return Fail("incomplete unicode escape");
                }

                std::uint32_t codePoint = 0;
                for (int i = 0; i < 4; ++i) {
                    const char hex = input_[position_ + i];
                    if (!IsHexDigit(hex)) {
                        return Fail("invalid unicode escape");
                    }
                    codePoint = (codePoint << 4) | static_cast<std::uint32_t>(HexValue(hex));
                }
                position_ += 4;
                AppendUtf8CodePoint(outValue, codePoint);
                break;
            }
            default:
                return Fail("invalid JSON string escape");
            }
        }

        return Fail("unterminated JSON string");
    }

    bool SkipValue() {
        SkipWhitespace();
        if (position_ >= input_.size()) {
            return Fail("expected JSON value");
        }

        const char ch = input_[position_];
        if (ch == '{') {
            ++position_;
            SkipWhitespace();
            if (Consume('}')) {
                return true;
            }

            while (true) {
                std::string ignoredKey;
                if (!ParseString(ignoredKey) || !Expect(':', "expected ':' after object key") || !SkipValue()) {
                    return false;
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                if (!Consume(',')) {
                    return Fail("expected ',' or '}' inside object");
                }
            }
        }

        if (ch == '[') {
            ++position_;
            SkipWhitespace();
            if (Consume(']')) {
                return true;
            }

            while (true) {
                if (!SkipValue()) {
                    return false;
                }

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                if (!Consume(',')) {
                    return Fail("expected ',' or ']' inside array");
                }
            }
        }

        if (ch == '"') {
            std::string ignored;
            return ParseString(ignored);
        }

        if (StartsWith("null")) {
            position_ += 4;
            return true;
        }

        if (StartsWith("true")) {
            position_ += 4;
            return true;
        }

        if (StartsWith("false")) {
            position_ += 5;
            return true;
        }

        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            if (ch == '-') {
                ++position_;
            }

            if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                return Fail("expected digit after '-'");
            }

            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }

            if (position_ < input_.size() && input_[position_] == '.') {
                ++position_;
                if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                    return Fail("expected digit after decimal point");
                }
                while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                    ++position_;
                }
            }

            if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
                ++position_;
                if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
                    ++position_;
                }
                if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                    return Fail("expected exponent digit");
                }
                while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                    ++position_;
                }
            }

            return true;
        }

        return Fail("invalid JSON value");
    }

    bool Expect(char expected, const char* reason) {
        if (Consume(expected)) {
            return true;
        }
        return Fail(reason);
    }

    bool Fail(const char* reason) {
        if (!error_.empty()) {
            return false;
        }

        error_ = reason;
        error_ += " at byte ";
        error_ += std::to_string(position_);

        if (insidePlayer_) {
            error_ += " (path players[";
            error_ += std::to_string(currentPlayerIndex_);
            error_ += "]";
            if (!currentField_.empty()) {
                error_ += ".";
                error_ += currentField_;
            }
            error_ += ")";
        }

        const std::string snippet = BuildSnippet();
        if (!snippet.empty()) {
            error_ += " near \"";
            error_ += snippet;
            error_ += "\"";
        }

        return false;
    }

    std::string BuildSnippet() const {
        std::string snippet;
        const std::size_t end = (std::min)(input_.size(), position_ + 80);
        snippet.reserve(end - position_);

        for (std::size_t i = position_; i < end; ++i) {
            const char ch = input_[i];
            switch (ch) {
            case '\n':
                snippet += "\\n";
                break;
            case '\r':
                snippet += "\\r";
                break;
            case '\t':
                snippet += "\\t";
                break;
            case '"':
                snippet += "\\\"";
                break;
            case '\\':
                snippet += "\\\\";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    snippet.push_back('?');
                } else {
                    snippet.push_back(ch);
                }
                break;
            }
        }

        return snippet;
    }

    bool Consume(char expected) {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }

        ++position_;
        return true;
    }

    bool StartsWith(const char* literal) const {
        std::size_t length = 0;
        while (literal[length] != '\0') {
            ++length;
        }

        return input_.compare(position_, length, literal) == 0;
    }

    void SkipWhitespace() {
        while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    const std::string& input_;
    std::size_t position_ = 0;
    std::string error_;
    bool insidePlayer_ = false;
    std::size_t currentPlayerIndex_ = 0;
    std::string currentField_;
};

std::string BuildPrefixedMessage(const std::string& body) {
    return TruncateForSampChat("{33CCFF}[NLBinder]{FFFFFF} " + body);
}

} // namespace

OnlinePlayerInfoService::OnlinePlayerInfoService() = default;

OnlinePlayerInfoService::~OnlinePlayerInfoService() {
    Shutdown();
}

void OnlinePlayerInfoService::Initialize() {
    Shutdown();

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        running_ = true;
        pendingUsernames_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(completedMutex_);
        completedLookups_.clear();
    }

    workerThread_ = std::thread(&OnlinePlayerInfoService::WorkerLoop, this);
}

void OnlinePlayerInfoService::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        running_ = false;
        pendingUsernames_.clear();
    }

    requestCv_.notify_all();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void OnlinePlayerInfoService::QueueLookup(const std::string& username) {
    const std::string normalized = NormalizeLookupName(username);
    if (normalized.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        if (!running_) {
            return;
        }

        pendingUsernames_.push_back(normalized);
    }

    requestCv_.notify_one();
}

std::vector<OnlinePlayerLookupResult> OnlinePlayerInfoService::ConsumeCompletedLookups() {
    std::lock_guard<std::mutex> lock(completedMutex_);
    std::vector<OnlinePlayerLookupResult> results;
    results.swap(completedLookups_);
    return results;
}

OnlinePlayerLookupResult OnlinePlayerInfoService::LookupNow(const std::string& username) const {
    OnlinePlayerLookupResult result;
    result.requestedName = NormalizeLookupName(username);

    std::string body;
    std::string lastModified;
    std::string etag;
    unsigned long statusCode = 0;
    if (!httpClient_.Fetch(kPlayersApiUrl, body, "", "", lastModified, etag, statusCode)) {
        result.status = OnlinePlayerLookupResult::Status::NetworkError;
        result.chatMessage = BuildPrefixedMessage("Spielerinfos konnten gerade nicht geladen werden.");
        return result;
    }

    if (statusCode != 200) {
        result.status = OnlinePlayerLookupResult::Status::NetworkError;
        result.chatMessage = BuildPrefixedMessage("Spieler-API antwortete mit HTTP " + std::to_string(statusCode) + ".");
        return result;
    }

    std::vector<OnlinePlayerInfo> players;
    std::string parseError;
    if (!ParsePlayersResponse(body, players, &parseError)) {
        Logger::Warning("[OnlinePlayerInfoService] Failed to parse online players response: %s",
                        parseError.empty() ? "unknown parse error" : parseError.c_str());
        result.status = OnlinePlayerLookupResult::Status::ParseError;
        result.chatMessage = BuildPrefixedMessage("Spielerinfos konnten nicht verarbeitet werden.");
        return result;
    }

    OnlinePlayerInfo player;
    if (!FindPlayerByName(players, result.requestedName, player)) {
        result.status = OnlinePlayerLookupResult::Status::NotFound;
        result.chatMessage = BuildPrefixedMessage("Kein Online-Spieler mit dem Namen " + result.requestedName + " gefunden.");
        return result;
    }

    result.status = OnlinePlayerLookupResult::Status::Success;
    result.chatMessage = BuildChatMessage(player);
    return result;
}

std::string OnlinePlayerInfoService::NormalizeLookupName(const std::string& username) {
    std::string normalized = TrimWhitespace(username);
    const std::size_t separator = normalized.find_first_of(" \t\n\r");
    if (separator != std::string::npos) {
        normalized.resize(separator);
    }
    return normalized;
}

bool OnlinePlayerInfoService::ParsePlayersResponse(const std::string& response,
                                                   std::vector<OnlinePlayerInfo>& outPlayers,
                                                   std::string* parseError) {
    if (parseError != nullptr) {
        parseError->clear();
    }

    JsonReader reader(response);
    if (reader.ParsePlayers(outPlayers)) {
        return true;
    }

    if (parseError != nullptr) {
        *parseError = reader.GetError();
        if (parseError->empty()) {
            *parseError = "unknown parse error";
        }
    }
    return false;
}

bool OnlinePlayerInfoService::FindPlayerByName(const std::vector<OnlinePlayerInfo>& players,
                                               const std::string& username,
                                               OnlinePlayerInfo& outPlayer) {
    const std::string needle = ToLowerAscii(NormalizeLookupName(username));
    if (needle.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < players.size(); ++i) {
        if (ToLowerAscii(players[i].name) == needle) {
            outPlayer = players[i];
            return true;
        }
    }

    return false;
}

std::string OnlinePlayerInfoService::BuildChatMessage(const OnlinePlayerInfo& player) {
    std::string message = "Infos für " + player.name + ": Level: " + std::to_string(player.level);
    if (player.hasFaction) {
        message += " Fraktion: " + player.faction;
    }
    if (player.hasGang) {
        message += " Gang: " + player.gang;
    }
    if (player.hasTeamRank) {
        message += " Team: " + player.teamRank;
    }

    return BuildPrefixedMessage(message);
}

void OnlinePlayerInfoService::WorkerLoop() {
    while (true) {
        std::string username;
        {
            std::unique_lock<std::mutex> lock(requestMutex_);
            requestCv_.wait(lock, [&]() {
                return !running_ || !pendingUsernames_.empty();
            });

            if (!running_ && pendingUsernames_.empty()) {
                return;
            }

            username = pendingUsernames_.front();
            pendingUsernames_.erase(pendingUsernames_.begin());
        }

        Logger::Info("[OnlinePlayerInfoService] Looking up '%s'", username.c_str());
        const OnlinePlayerLookupResult result = LookupNow(username);
        PushCompletedResult(result);
    }
}

void OnlinePlayerInfoService::PushCompletedResult(const OnlinePlayerLookupResult& result) {
    std::lock_guard<std::mutex> lock(completedMutex_);
    completedLookups_.push_back(result);
}

} // namespace keybinder
