#include "../include/TextDrawAdapter.h"

#include "../include/Logger.h"
#include "../include/TextDrawRPC.h"
#include "../include/RakHook/include/RakNet/BitStream.h"

#ifndef KEYBINDER_TESTING
#include "../include/RakHook/include/RakHook/rakhook.hpp"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CNetGame.h"
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CTextDrawPool.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/CPlayerPed.h"
#include "../include/plugin-sdk/plugin_sa/game_sa/enums/ePedState.h"
#include "common.h"
#include "safetyhook.hpp"
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

#ifndef KEYBINDER_TESTING
extern std::atomic<int> g_shutdown_phase;
#endif

namespace keybinder {

namespace {

constexpr int kMaxServerTextDraws = 2048;
constexpr int kMaxLocalTextDraws = 256;
constexpr int kMaxTextDrawId = kMaxServerTextDraws + kMaxLocalTextDraws - 1;
constexpr int kPlayerTextDrawIdOffset = kMaxServerTextDraws;
constexpr unsigned short kMaxRpcTextDrawTextLength = 800;

struct TextDrawSnapshot {
    char rawText[801] = {};
    unsigned char flags = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    int style = 0;
    int box = 0;
    int center = 0;
    float boxSizeX = 0.0f;
    float boxSizeY = 0.0f;
    float letterWidth = 0.0f;
    float letterHeight = 0.0f;
};

enum class TextDrawRole {
    Unknown,
    SpeedometerBox,
    VehicleName,
    SpeedValue,
    SpeedUnit,
    FuelLabel,
    FuelValue,
    GaugeSegment
};

struct TextDrawClassification {
    TextDrawRole role = TextDrawRole::Unknown;
    int confidence = 0;
    bool removable = false;
};

constexpr int kNlTachoV2PlayerPanelTextDrawId = 2052;
constexpr int kNlTachoV2PlayerSpeedValueTextDrawId = 2069;
constexpr int kNlTachoV2PlayerFuelLabelTextDrawId = 2070;
constexpr int kNlTachoV2PlayerSpeedUnitTextDrawId = 2071;
constexpr int kNlTachoV2AlternateVehicleNameTextDrawId = 2084;
constexpr int kNlTachoV2GaugeBackgroundTextDrawId = 2085;
constexpr int kNlTachoV2AlternateGaugeBackgroundTextDrawId = 2086;
constexpr int kNlTachoV2GaugeFillTextDrawId = 2087;
constexpr int kNlTachoV2VehicleNameTextDrawId = 2088;
constexpr int kNlTachoV2SpeedValueTextDrawId = 2089;
constexpr int kNlTachoV2FuelValueTextDrawId = 2090;
constexpr int kNlTachoV2DynamicFirstTextDrawId = kNlTachoV2AlternateVehicleNameTextDrawId;
// NL servers periodically pack additional dynamic speedometer textdraws past
// the nominal FuelValue id (2090). Observed live: speed digit at id 2094 and
// fuel % at id 2095. Extending the dynamic range to +5 keeps the existing
// position/text classifier authoritative while letting the id gate pass these
// through so the suppressor and fuel cache actually fire.
constexpr int kNlTachoV2DynamicLastTextDrawId = kNlTachoV2FuelValueTextDrawId + 5;
// Live NL servers render the fuel/gauge bar as a background box plus a colored
// fill overlay. The colored fill lands inside the dynamic range (observed id
// 2084), but the background box sits two ids below the dynamic-first vehicle
// name id (observed id 2082). Gauge classification must reach this lower floor
// so the background box is suppressed alongside the fill; otherwise removing the
// fill leaves the opaque background box on screen as a black bar. The strict
// position/box/text signature in the gauge classifiers keeps the wider id floor
// from matching the control-hint textdraws that reuse ids 2082/2083 on foot.
constexpr int kNlTachoV2GaugeCandidateFirstTextDrawId = kNlTachoV2DynamicFirstTextDrawId - 2;
constexpr int kHighConfidenceTextDrawMatch = 85;
constexpr int kNlTachoV2TextDrawIds[] = {
    kNlTachoV2PlayerPanelTextDrawId,
    kNlTachoV2PlayerSpeedValueTextDrawId,
    kNlTachoV2PlayerFuelLabelTextDrawId,
    kNlTachoV2PlayerSpeedUnitTextDrawId,
    kNlTachoV2AlternateVehicleNameTextDrawId,
    kNlTachoV2GaugeBackgroundTextDrawId,
    kNlTachoV2AlternateGaugeBackgroundTextDrawId,
    kNlTachoV2GaugeFillTextDrawId,
    kNlTachoV2VehicleNameTextDrawId,
    kNlTachoV2SpeedValueTextDrawId,
    kNlTachoV2FuelValueTextDrawId};

std::string StripTextDrawColorCodes(const std::string& text);
float ParseFuelValueFromText(const std::string& text);
std::string BoundedCString(const char* text, std::size_t maxLength);

bool IsValidTextDrawId(int id) {
    return id >= 0 && id <= kMaxTextDrawId;
}

bool IsLocalPlayerTextDrawRpcId(int id) {
    return id >= 0 && id < kMaxLocalTextDraws;
}

int ToAbsolutePlayerTextDrawId(int localId) {
    return kPlayerTextDrawIdOffset + localId;
}

int NormalizeRpcTextDrawId(int id) {
    return IsLocalPlayerTextDrawRpcId(id) ? ToAbsolutePlayerTextDrawId(id) : id;
}

bool IsNlTachoV2TextDrawAbsoluteId(int absoluteId) {
    for (int textDrawId : kNlTachoV2TextDrawIds) {
        if (absoluteId == textDrawId) {
            return true;
        }
    }

    return false;
}
bool IsNlTachoV2DynamicTextDrawAbsoluteId(int absoluteId) {
    return absoluteId >= kNlTachoV2DynamicFirstTextDrawId
        && absoluteId <= kNlTachoV2DynamicLastTextDrawId;
}


bool IsKnownPlayerTextDrawAbsoluteId(int absoluteId) {
    return IsNlTachoV2TextDrawAbsoluteId(absoluteId)
        || IsNlTachoV2DynamicTextDrawAbsoluteId(absoluteId);
}

bool TryGetKnownPlayerTextDrawAlias(int id, int& aliasId) {
    if (IsLocalPlayerTextDrawRpcId(id)) {
        const int absoluteId = ToAbsolutePlayerTextDrawId(id);
        if (IsKnownPlayerTextDrawAbsoluteId(absoluteId)) {
            aliasId = absoluteId;
            return true;
        }
        return false;
    }

    if (id >= kPlayerTextDrawIdOffset && id <= kMaxTextDrawId && IsKnownPlayerTextDrawAbsoluteId(id)) {
        aliasId = id - kPlayerTextDrawIdOffset;
        return true;
    }

    return false;
}

bool IsNlSpeedometerTextDrawId(int id) {
    return IsKnownPlayerTextDrawAbsoluteId(NormalizeRpcTextDrawId(id));
}

bool MatchesNlSpeedometerTextDrawId(int rpcId) {
    return IsValidTextDrawId(rpcId) && IsNlSpeedometerTextDrawId(rpcId);
}

bool MatchesNlFuelTextDrawId(int rpcId) {
    const int absoluteId = NormalizeRpcTextDrawId(rpcId);
    return absoluteId >= kNlTachoV2FuelValueTextDrawId
        && absoluteId <= kNlTachoV2DynamicLastTextDrawId;
}

bool MatchesNlTachoV2PlayerSpeedValueTextDrawId(int rpcId) {
    return NormalizeRpcTextDrawId(rpcId) == kNlTachoV2PlayerSpeedValueTextDrawId;
}

bool IsNlTachoV2GaugeCandidateAbsoluteId(int absoluteId) {
    return absoluteId >= kNlTachoV2GaugeCandidateFirstTextDrawId
        && absoluteId <= kNlTachoV2DynamicLastTextDrawId;
}

bool MatchesNlTachoV2VehicleNameAbsoluteId(int absoluteId) {
    return IsNlTachoV2DynamicTextDrawAbsoluteId(absoluteId);
}

bool IsClose(float actual, float expected, float tolerance) {
    return std::fabs(actual - expected) <= tolerance;
}

bool IsInRange(float value, float minValue, float maxValue) {
    return value >= minValue && value <= maxValue;
}

TextDrawClassification MakeClassification(TextDrawRole role, int confidence, bool removable) {
    TextDrawClassification classification;
    classification.role = role;
    classification.confidence = confidence;
    classification.removable = removable;
    return classification;
}

bool IsHighConfidenceRemovableTextDraw(const TextDrawClassification& classification) {
    return classification.removable && classification.confidence >= kHighConfidenceTextDrawMatch;
}

const char* TextDrawRoleName(TextDrawRole role) {
    switch (role) {
    case TextDrawRole::SpeedometerBox: return "SpeedometerBox";
    case TextDrawRole::VehicleName: return "VehicleName";
    case TextDrawRole::SpeedValue: return "SpeedValue";
    case TextDrawRole::SpeedUnit: return "SpeedUnit";
    case TextDrawRole::FuelLabel: return "FuelLabel";
    case TextDrawRole::FuelValue: return "FuelValue";
    case TextDrawRole::GaugeSegment: return "GaugeSegment";
    case TextDrawRole::Unknown:
    default:
        return "Unknown";
    }
}

bool IsStrippedUnsignedNumber(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

bool LooksLikeNlTachoV2PlayerSpeedValueText(const std::string& rawText) {
    if (rawText.find("~g~") == std::string::npos) {
        return false;
    }

    const std::string stripped = StripTextDrawColorCodes(rawText);
    return stripped.size() >= 1 && stripped.size() <= 3 && IsStrippedUnsignedNumber(stripped);
}
bool LooksLikeNlTachoV2PlayerSpeedValueTextDraw(const TextDrawSnapshot& snapshot, const std::string& rawText) {
    return snapshot.style == 3
        && snapshot.box == 0
        && snapshot.center == 0
        && IsClose(snapshot.posX, 636.0f, 8.0f)
        && IsClose(snapshot.posY, 2.0f, 8.0f)
        && IsClose(snapshot.letterWidth, 0.3f, 0.08f)
        && IsClose(snapshot.letterHeight, 1.5f, 0.4f)
        && LooksLikeNlTachoV2PlayerSpeedValueText(rawText);
}


bool IsStrippedUnsignedDecimalNumber(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    bool sawDigit = false;
    bool sawDecimalPoint = false;
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            sawDigit = true;
            continue;
        }
        if (ch == '.' && !sawDecimalPoint) {
            sawDecimalPoint = true;
            continue;
        }
        return false;
    }

    return sawDigit;
}

bool LooksLikeNlPercentValueText(const std::string& strippedText) {
    if (strippedText.size() < 2 || strippedText.back() != '%') {
        return false;
    }

    const std::string number = strippedText.substr(0, strippedText.size() - 1);
    return IsStrippedUnsignedDecimalNumber(number) && ParseFuelValueFromText(strippedText) >= 0.0f;
}

std::string TrimText(std::string text) {
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    }).base(), text.end());
    return text;
}

bool LooksLikeNlFuelValueText(const std::string& rawText) {
    const std::string stripped = TrimText(StripTextDrawColorCodes(rawText));
    return LooksLikeNlPercentValueText(stripped);
}


TextDrawClassification ClassifyNlSpeedometerShowTextDraw(int id, const TextDrawSnapshot& snapshot) {
    const int absoluteId = NormalizeRpcTextDrawId(id);
    const std::string rawText = BoundedCString(snapshot.rawText, sizeof(snapshot.rawText));
    const std::string strippedText = TrimText(StripTextDrawColorCodes(rawText));


    if (strippedText == "_"
        && snapshot.style == 1
        && snapshot.box == 1
        && snapshot.center == 1
        && IsClose(snapshot.posX, 575.0f, 8.0f)
        && IsClose(snapshot.posY, 426.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 307.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 155.5f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.6f, 0.12f)
        && IsClose(snapshot.letterHeight, 2.25f, 0.4f)) {
        return MakeClassification(TextDrawRole::SpeedometerBox, 100, true);
    }

    if (strippedText == "Tankvolumen"
        && snapshot.style == 1
        && snapshot.box == 0
        && snapshot.center == 1
        && IsClose(snapshot.posX, 595.0f, 8.0f)
        && IsClose(snapshot.posY, 424.5f, 8.0f)
        && IsClose(snapshot.boxSizeX, 400.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 107.0f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.224996f, 0.08f)
        && IsClose(snapshot.letterHeight, 1.15f, 0.25f)) {
        return MakeClassification(TextDrawRole::FuelLabel, 100, true);
    }

    if (strippedText == "KM/H"
        && snapshot.style == 1
        && snapshot.box == 0
        && snapshot.center == 1
        && IsClose(snapshot.posX, 535.0f, 8.0f)
        && IsClose(snapshot.posY, 435.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 400.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 107.0f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.237498f, 0.08f)
        && IsClose(snapshot.letterHeight, 1.05f, 0.25f)) {
        return MakeClassification(TextDrawRole::SpeedUnit, 100, true);
    }

    if (IsNlTachoV2GaugeCandidateAbsoluteId(absoluteId)
        && strippedText == "_"
        && snapshot.style == 1
        && snapshot.box == 1
        && snapshot.center == 0
        && IsClose(snapshot.posX, 559.0f, 8.0f)
        && IsClose(snapshot.posY, 437.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 638.5f, 12.0f)
        && IsClose(snapshot.boxSizeY, 0.0f, 8.0f)
        && IsInRange(snapshot.letterWidth, 0.8f, 1.1f)
        && IsInRange(snapshot.letterHeight, 0.65f, 0.85f)) {
        return MakeClassification(TextDrawRole::GaugeSegment, 100, true);
    }

    if (IsNlTachoV2GaugeCandidateAbsoluteId(absoluteId)
        && strippedText == "_"
        && snapshot.style == 1
        && (snapshot.box == 0 || snapshot.box == 1)
        && snapshot.center == 0
        && IsClose(snapshot.posX, 561.45f, 8.0f)
        && IsClose(snapshot.posY, 438.0f, 8.0f)
        && IsInRange(snapshot.boxSizeX, 550.0f, 645.0f)
        && IsClose(snapshot.boxSizeY, 0.0f, 8.0f)
        && IsInRange(snapshot.letterWidth, 0.8f, 1.1f)
        && IsInRange(snapshot.letterHeight, 0.45f, 0.85f)) {
        return MakeClassification(TextDrawRole::GaugeSegment, 100, true);
    }

    if (MatchesNlTachoV2VehicleNameAbsoluteId(absoluteId)
        && snapshot.style == 1
        && snapshot.box == 0
        && snapshot.center == 1
        && IsClose(snapshot.posX, 527.0f, 8.0f)
        && IsClose(snapshot.posY, 424.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 400.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 107.0f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.287499f, 0.08f)
        && IsClose(snapshot.letterHeight, 1.25f, 0.25f)
        && !strippedText.empty()) {
        return MakeClassification(TextDrawRole::VehicleName, 100, true);
    }

    if (IsNlTachoV2DynamicTextDrawAbsoluteId(absoluteId)
        && snapshot.style == 1
        && snapshot.box == 0
        && snapshot.center == 1
        && IsClose(snapshot.posX, 515.0f, 8.0f)
        && IsClose(snapshot.posY, 435.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 400.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 107.0f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.237498f, 0.08f)
        && IsClose(snapshot.letterHeight, 1.05f, 0.25f)
        && IsStrippedUnsignedDecimalNumber(strippedText)) {
        return MakeClassification(TextDrawRole::SpeedValue, 100, true);
    }

    if (IsNlTachoV2DynamicTextDrawAbsoluteId(absoluteId)
        && snapshot.style == 1
        && snapshot.box == 0
        && snapshot.center == 1
        && IsClose(snapshot.posX, 595.0f, 8.0f)
        && IsClose(snapshot.posY, 437.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 400.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 107.0f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.174998f, 0.08f)
        && IsClose(snapshot.letterHeight, 0.849995f, 0.25f)
        && LooksLikeNlFuelValueText(rawText)) {
        return MakeClassification(TextDrawRole::FuelValue, 100, true);
    }

    return MakeClassification(TextDrawRole::Unknown, 0, false);
}


bool LooksLikeNlSpeedometerTextDraw(int id, const TextDrawSnapshot& snapshot) {
    return IsHighConfidenceRemovableTextDraw(ClassifyNlSpeedometerShowTextDraw(id, snapshot));
}


bool LooksLikeNlFuelTextDraw(int id, const TextDrawSnapshot& snapshot) {
    const int absoluteId = NormalizeRpcTextDrawId(id);
    if (!IsNlTachoV2DynamicTextDrawAbsoluteId(absoluteId)) {
        return false;
    }

    const std::string rawText = BoundedCString(snapshot.rawText, sizeof(snapshot.rawText));
    return snapshot.style == 1
        && snapshot.box == 0
        && snapshot.center == 1
        && IsClose(snapshot.posX, 595.0f, 8.0f)
        && IsClose(snapshot.posY, 437.0f, 8.0f)
        && IsClose(snapshot.boxSizeX, 400.0f, 12.0f)
        && IsClose(snapshot.boxSizeY, 107.0f, 12.0f)
        && IsClose(snapshot.letterWidth, 0.174998f, 0.08f)
        && IsClose(snapshot.letterHeight, 0.849995f, 0.25f)
        && LooksLikeNlFuelValueText(rawText);
}

bool IsHexDigit(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

std::string StripTextDrawColorCodes(const std::string& text) {
    std::string stripped;
    stripped.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{' && i + 7 < text.size() && text[i + 7] == '}') {
            bool isColor = true;
            for (std::size_t j = i + 1; j < i + 7; ++j) {
                if (!IsHexDigit(text[j])) {
                    isColor = false;
                    break;
                }
            }
            if (isColor) {
                i += 7;
                continue;
            }
        }

        if (text[i] == '~' && i + 2 < text.size() && text[i + 2] == '~'
            && std::isalpha(static_cast<unsigned char>(text[i + 1])) != 0) {
            i += 2;
            continue;
        }

        stripped.push_back(text[i]);
    }

    return stripped;
}

float ParseFuelValueFromText(const std::string& text) {
    const std::string stripped = StripTextDrawColorCodes(text);
    for (std::size_t i = 0; i < stripped.size(); ++i) {
        const char ch = stripped[i];
        const bool startsNumber = std::isdigit(static_cast<unsigned char>(ch)) != 0
            || ((ch == '-' || ch == '+') && i + 1 < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[i + 1])) != 0)
            || (ch == '.' && i + 1 < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[i + 1])) != 0);
        if (!startsNumber) {
            continue;
        }

        char* end = nullptr;
        const float value = std::strtof(stripped.c_str() + i, &end);
        if (end != stripped.c_str() + i && std::isfinite(value)) {
            return (std::max)(0.0f, (std::min)(100.0f, value));
        }
    }

    return -1.0f;
}

std::string BoundedCString(const char* text, std::size_t maxLength) {
    if (text == nullptr || maxLength == 0) {
        return {};
    }

    std::size_t length = 0;
    while (length < maxLength && text[length] != '\0') {
        ++length;
    }
    return std::string(text, length);
}

#ifndef KEYBINDER_TESTING
bool TryGetTextDrawPool(sampapi::v03dl::CTextDrawPool** out) {
    if (out == nullptr) {
        return false;
    }

    *out = nullptr;
    __try {
        auto* netGame = sampapi::v03dl::RefNetGame();
        if (netGame == nullptr || netGame->m_pPools == nullptr || netGame->m_pPools->m_pTextDraw == nullptr) {
            return false;
        }

        *out = netGame->m_pPools->m_pTextDraw;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *out = nullptr;
        return false;
    }
}

bool TrySnapshotTextDraw(sampapi::v03dl::CTextDrawPool* pool, int id, TextDrawSnapshot* out) {
    if (pool == nullptr || out == nullptr || !IsValidTextDrawId(id)) {
        return false;
    }

    __try {
        if (!pool->m_bNotEmpty[id] || pool->m_pObject[id] == nullptr) {
            return false;
        }

        auto* textDraw = pool->m_pObject[id];
        std::size_t length = 0;
        while (length < sizeof(out->rawText) - 1 && textDraw->m_szText[length] != '\0') {
            ++length;
        }
        if (length == 0) {
            return false;
        }

        std::memcpy(out->rawText, textDraw->m_szText, length);
        out->rawText[length] = '\0';
        out->posX = textDraw->m_data.m_fX;
        out->posY = textDraw->m_data.m_fY;
        out->style = textDraw->m_data.m_nStyle;
        out->box = static_cast<int>(textDraw->m_data.m_bBox);
        out->center = static_cast<int>(textDraw->m_data.m_bCenter);
        out->boxSizeX = textDraw->m_data.m_fBoxSizeX;
        out->boxSizeY = textDraw->m_data.m_fBoxSizeY;
        out->letterWidth = textDraw->m_data.m_fLetterWidth;
        out->letterHeight = textDraw->m_data.m_fLetterHeight;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#endif
#ifndef KEYBINDER_TESTING
bool MatchesRuntimeNlTachoV2PlayerSpeedValueTextDraw(int id, const std::string& rawText) {
    if (!LooksLikeNlTachoV2PlayerSpeedValueText(rawText)) {
        return false;
    }

    sampapi::v03dl::CTextDrawPool* pool = nullptr;
    if (!TryGetTextDrawPool(&pool)) {
        return false;
    }

    TextDrawSnapshot snapshot;
    const int absoluteId = NormalizeRpcTextDrawId(id);
    return TrySnapshotTextDraw(pool, absoluteId, &snapshot)
        && LooksLikeNlTachoV2PlayerSpeedValueTextDraw(snapshot, rawText);
}
#endif


std::string FormatTimestamp(std::time_t value) {
    std::tm localTime = {};
    localtime_s(&localTime, &value);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool TryReadTextDrawId(RakNet::BitStream* bs, int& textDrawId) {
    if (bs == nullptr) {
        return false;
    }

    const int originalReadOffset = bs->GetReadOffset();
    unsigned short rawTextDrawId = 0;
    const bool ok = bs->Read(rawTextDrawId);
    bs->SetReadOffset(originalReadOffset);
    if (ok) {
        textDrawId = static_cast<int>(rawTextDrawId);
    }
    return ok;
}

bool ReadString16(RakNet::BitStream* bs, std::string& text) {
    unsigned short textLength = 0;
    if (bs == nullptr || !bs->Read(textLength)) {
        return false;
    }

    if (textLength > kMaxRpcTextDrawTextLength || bs->GetNumberOfUnreadBits() < static_cast<int>(textLength) * 8) {
        return false;
    }

    text.assign(textLength, '\0');
    if (textLength == 0) {
        return true;
    }

    return bs->Read(&text[0], static_cast<int>(textLength));
}

bool TryReadSetStringRpcText(RakNet::BitStream* bs, int& textDrawId, std::string& text) {
    unsigned short rawTextDrawId = 0;
    if (bs == nullptr || !bs->Read(rawTextDrawId)) {
        return false;
    }

    textDrawId = static_cast<int>(rawTextDrawId);
    return ReadString16(bs, text);
}

bool TryReadShowTextDrawRpcSnapshot(RakNet::BitStream* bs, int& textDrawId, TextDrawSnapshot& snapshot, std::string& text) {
    unsigned short rawTextDrawId = 0;
    unsigned char flags = 0;
    float letterWidth = 0.0f;
    float letterHeight = 0.0f;
    std::int32_t letterColor = 0;
    float lineWidth = 0.0f;
    float lineHeight = 0.0f;
    std::int32_t boxColor = 0;
    unsigned char shadow = 0;
    unsigned char outline = 0;
    std::int32_t backgroundColor = 0;
    unsigned char style = 0;
    unsigned char selectable = 0;
    float x = 0.0f;
    float y = 0.0f;
    unsigned short modelId = 0;
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float zoom = 0.0f;
    short color1 = 0;
    short color2 = 0;

    if (bs == nullptr
        || !bs->Read(rawTextDrawId)
        || !bs->Read(flags)
        || !bs->Read(letterWidth)
        || !bs->Read(letterHeight)
        || !bs->Read(letterColor)
        || !bs->Read(lineWidth)
        || !bs->Read(lineHeight)
        || !bs->Read(boxColor)
        || !bs->Read(shadow)
        || !bs->Read(outline)
        || !bs->Read(backgroundColor)
        || !bs->Read(style)
        || !bs->Read(selectable)
        || !bs->Read(x)
        || !bs->Read(y)
        || !bs->Read(modelId)
        || !bs->Read(rotX)
        || !bs->Read(rotY)
        || !bs->Read(rotZ)
        || !bs->Read(zoom)
        || !bs->Read(color1)
        || !bs->Read(color2)) {
        return false;
    }

    if (!ReadString16(bs, text)) {
        return false;
    }

    std::memset(snapshot.rawText, 0, sizeof(snapshot.rawText));
    const std::size_t copyLength = (std::min)(text.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, text.data(), copyLength);
    }
    snapshot.flags = flags;
    snapshot.posX = x;
    snapshot.posY = y;
    snapshot.style = static_cast<int>(style);
    snapshot.box = (flags & 0x01) != 0 ? 1 : 0;
    snapshot.center = (flags & 0x08) != 0 ? 1 : 0;
    snapshot.boxSizeX = lineWidth;
    snapshot.boxSizeY = lineHeight;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    (void)selectable;

    textDrawId = static_cast<int>(rawTextDrawId);
    return true;
}

bool TryReadShowTextDrawRpcText(RakNet::BitStream* bs, int& textDrawId, std::string& text) {
    TextDrawSnapshot snapshot;
    return TryReadShowTextDrawRpcSnapshot(bs, textDrawId, snapshot, text);
}

bool TryReadTextDrawRpcText(unsigned char rpcId, RakNet::BitStream* bs, int& textDrawId, std::string& text) {
    if (bs == nullptr) {
        return false;
    }

    const int originalReadOffset = bs->GetReadOffset();
    bool ok = false;
    if (rpcId == textdraw::RPC_SCR_SHOW_TEXTDRAW) {
        ok = TryReadShowTextDrawRpcText(bs, textDrawId, text);
    } else if (rpcId == textdraw::RPC_SCR_TEXTDRAW_SET_STRING) {
        ok = TryReadSetStringRpcText(bs, textDrawId, text);
    }
    bs->SetReadOffset(originalReadOffset);
    return ok;
}

} // namespace

struct TextDrawAdapterState {
    std::atomic<bool> isAlive{false};
    std::atomic<bool> speedometerEnabled{true};
    std::atomic<bool> inVehicleContext{false};
    std::atomic<bool> renderSuppressionReady{false};
    std::atomic<bool> renderHookAttempted{false};
    std::atomic<bool> fuelStale{false};
    std::atomic<bool> diagnosticsEnabled{false};
    std::atomic<unsigned long> diagnosticsEventCount{0};
    mutable std::mutex cacheMutex;
    std::string cachedFuelText;
    float cachedFuelValue = -1.0f;
    mutable std::mutex classificationMutex;
    std::array<TextDrawClassification, kMaxTextDrawId + 1> showClassifications{};
};

namespace {


bool IsVehicleContextActive(const TextDrawAdapterState& state) {
    return state.speedometerEnabled.load() && state.inVehicleContext.load();
}

TextDrawClassification GetCachedShowClassification(const TextDrawAdapterState& state, int id);

bool ContainsHideId(const TextDrawAdapterState& state, int id) {
    return IsVehicleContextActive(state)
        && IsValidTextDrawId(id)
        && MatchesNlSpeedometerTextDrawId(id);
}

bool ContainsFuelId(const TextDrawAdapterState& state, int id) {
    return IsVehicleContextActive(state)
        && IsValidTextDrawId(id)
        && (MatchesNlFuelTextDrawId(id) || GetCachedShowClassification(state, id).role == TextDrawRole::FuelValue);
}

bool ShouldCacheFuelText(const TextDrawAdapterState& state, int id, const std::string& rawText) {
    if (!IsVehicleContextActive(state) || !IsValidTextDrawId(id)) {
        return false;
    }

    return LooksLikeNlFuelValueText(rawText)
        && (MatchesNlFuelTextDrawId(id) || GetCachedShowClassification(state, id).role == TextDrawRole::FuelValue);
}

bool ShouldSuppressShowTextDraw(const TextDrawAdapterState& state, int id, const TextDrawSnapshot& snapshot) {
    return IsVehicleContextActive(state)
        && IsValidTextDrawId(id)
        && LooksLikeNlSpeedometerTextDraw(id, snapshot);
}

bool ShouldSuppressSetStringTextDraw(const TextDrawAdapterState& state, int id, const std::string& rawText) {
    if (!IsVehicleContextActive(state) || !IsValidTextDrawId(id)) {
        return false;
    }

#ifdef KEYBINDER_TESTING
    if (MatchesNlTachoV2PlayerSpeedValueTextDrawId(id) && LooksLikeNlTachoV2PlayerSpeedValueText(rawText)) {
        return true;
    }
#else
    if (MatchesRuntimeNlTachoV2PlayerSpeedValueTextDraw(id, rawText)) {
        return true;
    }
#endif

    return IsHighConfidenceRemovableTextDraw(GetCachedShowClassification(state, id));
}


const char* BoolText(bool value) {
    return value ? "yes" : "no";
}

const char* TextDrawRpcName(unsigned char rpcId) {
    if (rpcId == textdraw::RPC_SCR_SHOW_TEXTDRAW) {
        return "SHOW";
    }
    if (rpcId == textdraw::RPC_SCR_HIDE_TEXTDRAW) {
        return "HIDE";
    }
    if (rpcId == textdraw::RPC_SCR_TEXTDRAW_SET_STRING) {
        return "SET_STRING";
    }
    return "UNKNOWN";
}

std::string SanitizeTextForLog(std::string text) {
    std::replace(text.begin(), text.end(), '\r', ' ');
    std::replace(text.begin(), text.end(), '\n', ' ');
    std::replace(text.begin(), text.end(), '\t', ' ');

    constexpr std::size_t kMaxLoggedTextLength = 160;
    if (text.size() > kMaxLoggedTextLength) {
        text.resize(kMaxLoggedTextLength);
        text += "...";
    }
    return text;
}

void AppendTextDrawIdDetails(std::ostringstream& oss, int id) {
    oss << " rpc_id=" << id;
    if (IsLocalPlayerTextDrawRpcId(id)) {
        oss << " absolute_player_id=" << ToAbsolutePlayerTextDrawId(id) << " pool=player-local";
    } else if (id >= kPlayerTextDrawIdOffset) {
        oss << " pool=player";
    } else {
        oss << " pool=server";
    }
}

#ifndef KEYBINDER_TESTING
void LogTextDrawRpcDiagnostic(
    TextDrawAdapterState& state,
    unsigned char rpcId,
    RakNet::BitStream* bs,
    int id,
    bool isHideTextDraw,
    bool isFuelTextDraw,
    bool suppressed) {
    if (!state.diagnosticsEnabled.load()) {
        return;
    }

    const unsigned long eventNumber = ++state.diagnosticsEventCount;
    std::ostringstream oss;
    oss << "[TextDrawAdapter][Diag] #" << eventNumber
        << " rpc=" << TextDrawRpcName(rpcId);
    AppendTextDrawIdDetails(oss, id);
    oss << " speedometer=" << BoolText(state.speedometerEnabled.load())
        << " in_vehicle=" << BoolText(IsVehicleContextActive(state))
        << " hide_match=" << BoolText(isHideTextDraw)
        << " fuel_match=" << BoolText(isFuelTextDraw)
        << " fuel_stale=" << BoolText(state.fuelStale.load())
        << " tick_ms=" << static_cast<unsigned long long>(GetTickCount64())
        << " suppressed=" << BoolText(suppressed);

    if (bs != nullptr && rpcId == textdraw::RPC_SCR_SHOW_TEXTDRAW) {
        const int originalReadOffset = bs->GetReadOffset();
        int parsedId = -1;
        TextDrawSnapshot snapshot;
        std::string rawText;
        const bool parsed = TryReadShowTextDrawRpcSnapshot(bs, parsedId, snapshot, rawText);
        bs->SetReadOffset(originalReadOffset);

        if (parsed) {
            const TextDrawClassification classification = ClassifyNlSpeedometerShowTextDraw(id, snapshot);
            oss << " parsed_id=" << parsedId
                << " flags=" << static_cast<int>(snapshot.flags)
                << " pos=(" << snapshot.posX << "," << snapshot.posY << ")"
                << " style=" << snapshot.style
                << " box=" << snapshot.box
                << " center=" << snapshot.center
                << " box_size=(" << snapshot.boxSizeX << "," << snapshot.boxSizeY << ")"
                << " letter_size=(" << snapshot.letterWidth << "," << snapshot.letterHeight << ")"
                << " role=" << TextDrawRoleName(classification.role)
                << " confidence=" << classification.confidence
                << " raw='" << SanitizeTextForLog(rawText) << "'"
                << " stripped='" << SanitizeTextForLog(StripTextDrawColorCodes(rawText)) << "'";
        } else {
            oss << " show_parse=failed";
        }
    } else if (bs != nullptr && rpcId == textdraw::RPC_SCR_TEXTDRAW_SET_STRING) {
        int parsedId = -1;
        std::string rawText;
        if (TryReadTextDrawRpcText(rpcId, bs, parsedId, rawText)) {
            oss << " parsed_id=" << parsedId
                << " raw='" << SanitizeTextForLog(rawText) << "'"
                << " stripped='" << SanitizeTextForLog(StripTextDrawColorCodes(rawText)) << "'";
        } else {
            oss << " set_string_parse=failed";
        }
    }

    Logger::Info("%s", oss.str().c_str());
}
#endif

void ClearFuelCache(const std::shared_ptr<TextDrawAdapterState>& state) {
    if (!state) {
        return;
    }

    std::lock_guard<std::mutex> lock(state->cacheMutex);
    state->cachedFuelText.clear();
    state->cachedFuelValue = -1.0f;
}

void ClearShowClassificationCache(const std::shared_ptr<TextDrawAdapterState>& state) {
    if (!state) {
        return;
    }

    std::lock_guard<std::mutex> lock(state->classificationMutex);
    state->showClassifications.fill(TextDrawClassification{});
}

void CacheShowClassification(TextDrawAdapterState& state, int id, const TextDrawClassification& classification) {
    if (!IsValidTextDrawId(id)) {
        return;
    }

    std::lock_guard<std::mutex> lock(state.classificationMutex);
    state.showClassifications[static_cast<std::size_t>(id)] = classification;
}

void ClearShowClassification(TextDrawAdapterState& state, int id) {
    if (!IsValidTextDrawId(id)) {
        return;
    }

    std::lock_guard<std::mutex> lock(state.classificationMutex);
    state.showClassifications[static_cast<std::size_t>(id)] = TextDrawClassification{};
    int aliasId = -1;
    if (TryGetKnownPlayerTextDrawAlias(id, aliasId)) {
        state.showClassifications[static_cast<std::size_t>(aliasId)] = TextDrawClassification{};
    }
}

TextDrawClassification GetCachedShowClassification(const TextDrawAdapterState& state, int id) {
    if (!IsValidTextDrawId(id)) {
        return TextDrawClassification{};
    }

    std::lock_guard<std::mutex> lock(state.classificationMutex);
    TextDrawClassification classification = state.showClassifications[static_cast<std::size_t>(id)];
    if (classification.role != TextDrawRole::Unknown) {
        return classification;
    }

    if (id < kPlayerTextDrawIdOffset) {
        return classification;
    }

    int aliasId = -1;
    if (TryGetKnownPlayerTextDrawAlias(id, aliasId)) {
        classification = state.showClassifications[static_cast<std::size_t>(aliasId)];
    }
    return classification;
}

void CacheFuelText(TextDrawAdapterState& state, const std::string& rawText) {
    const float parsedValue = ParseFuelValueFromText(rawText);
    {
        std::lock_guard<std::mutex> lock(state.cacheMutex);
        state.cachedFuelText = rawText;
        state.cachedFuelValue = parsedValue;
    }
    state.fuelStale = false;
}

// Visibility is changed only for the duration of SA-MP's draw call. Objects,
// strings and incoming RPCs remain owned by SA-MP, so disabling the replacement
// immediately restores the current original display without replaying packets.
template <typename Object, typename Classify, typename Draw>
void DrawWithTemporaryVisibilityMask(int* visible, Object** objects, std::size_t count, Classify classify, Draw draw) {
    struct RestoreVisibility {
        struct Slot { std::size_t id; int visible; Object* object; };
        int* visible;
        Object** objects;
        std::array<Slot, kMaxTextDrawId + 1> saved{};
        std::size_t used = 0;
        ~RestoreVisibility() {
            for (std::size_t index = 0; index < used; ++index) {
                const auto& slot = saved[index];
                // Never revive a slot that SA-MP deleted/replaced while drawing.
                if (objects[slot.id] == slot.object && visible[slot.id] == 0) visible[slot.id] = slot.visible;
            }
        }
    } restore{visible, objects};
    if (visible != nullptr && objects != nullptr) {
        count = (std::min)(count, static_cast<std::size_t>(kMaxTextDrawId + 1));
        for (std::size_t id = 0; id < count; ++id) {
            if (visible[id] && objects[id] != nullptr && classify(static_cast<int>(id))) {
                restore.saved[restore.used++] = {id, visible[id], objects[id]};
                visible[id] = 0;
            }
        }
    }
    draw();
}

#ifndef KEYBINDER_TESTING
safetyhook::InlineHook* g_textDrawPoolDrawHook = nullptr;
std::weak_ptr<TextDrawAdapterState> g_textDrawRenderState;

void __fastcall DrawTextDrawPool(sampapi::v03dl::CTextDrawPool* pool, void*) {
    auto state = g_textDrawRenderState.lock();
    if (pool == nullptr || !state || !state->isAlive.load() || !IsVehicleContextActive(*state)) {
        g_textDrawPoolDrawHook->thiscall<void>(pool);
        return;
    }
    DrawWithTemporaryVisibilityMask(pool->m_bNotEmpty, pool->m_pObject, kMaxTextDrawId + 1,
        [pool](int id) {
            TextDrawSnapshot snapshot;
            if (!TrySnapshotTextDraw(pool, id, &snapshot)) return false;
            return LooksLikeNlSpeedometerTextDraw(id, snapshot)
                || (id >= kPlayerTextDrawIdOffset && LooksLikeNlTachoV2PlayerSpeedValueTextDraw(snapshot, snapshot.rawText));
        }, [pool]() { g_textDrawPoolDrawHook->thiscall<void>(pool); });
}

bool EnsureTextDrawRenderHook(const std::shared_ptr<TextDrawAdapterState>& state) {
    if (rakhook::samp_version() != rakhook::samp_ver::v03dlr1 || sampapi::GetBase() == 0) return false;
    g_textDrawRenderState = state;
    if (g_textDrawPoolDrawHook != nullptr) return true;
    if (state->renderHookAttempted.exchange(true)) return false;
    auto hook = safetyhook::InlineHook::create(reinterpret_cast<void*>(sampapi::GetAddress(0x1E2F0)),
        reinterpret_cast<void*>(&DrawTextDrawPool));
    if (!hook.has_value()) {
        Logger::Warning("[TextDrawAdapter] Tacho render hook unavailable; original Tacho retained");
        return false;
    }
    g_textDrawPoolDrawHook = new safetyhook::InlineHook(std::move(*hook));
    Logger::Info("[TextDrawAdapter] Reversible TachoV2 render hook installed");
    return true;
}
#endif


} // namespace

TextDrawAdapter::TextDrawAdapter()
    : state_(std::make_shared<TextDrawAdapterState>()) {
}

TextDrawAdapter::~TextDrawAdapter() {
#ifndef KEYBINDER_TESTING
    if (g_shutdown_phase.load() != 2) {
        return;
    }
#endif
    if (initialized_.load()) {
        Shutdown();
    }
}

bool TextDrawAdapter::Initialize() {
    if (initialized_.load()) {
        state_->isAlive = true;
#ifdef KEYBINDER_TESTING
        state_->renderSuppressionReady = true;
#endif
        return true;
    }

#ifdef KEYBINDER_TESTING
    initialized_ = true;
    state_->isAlive = true;
    state_->renderSuppressionReady = true;
    return true;
#else
    // Register before RakHook starts receiving RPCs. The callback itself checks
    // rakhook::initialized, which avoids mutating RakHook's callback vector
    // from the live receive path later.
    state_->isAlive = true;
    std::weak_ptr<TextDrawAdapterState> weakState = state_;
    rakhook::on_receive_rpc += [weakState](unsigned char& rpcId, RakNet::BitStream* bs) -> bool {
        if (!rakhook::initialized) {
            return true;
        }

        auto state = weakState.lock();
        if (!state || !state->isAlive.load()) {
            return true;
        }


        if (rpcId != textdraw::RPC_SCR_SHOW_TEXTDRAW
            && rpcId != textdraw::RPC_SCR_HIDE_TEXTDRAW
            && rpcId != textdraw::RPC_SCR_TEXTDRAW_SET_STRING) {
            return true;
        }


        int id = -1;
        if (!TryReadTextDrawId(bs, id)) {
            return true;
        }

        if (!IsValidTextDrawId(id)) {
            return true;
        }

        const bool isHideTextDraw = ContainsHideId(*state, id);
        const bool isFuelTextDraw = ContainsFuelId(*state, id);

        if (rpcId == textdraw::RPC_SCR_HIDE_TEXTDRAW) {
            if (isFuelTextDraw) {
                state->fuelStale = true;
            }
            ClearShowClassification(*state, id);
            LogTextDrawRpcDiagnostic(*state, rpcId, bs, id, isHideTextDraw, isFuelTextDraw, false);
            return true;
        }

        int parsedTextId = -1;
        std::string parsedRawText;
        const bool hasParsedText = (rpcId == textdraw::RPC_SCR_TEXTDRAW_SET_STRING || isFuelTextDraw)
            && TryReadTextDrawRpcText(rpcId, bs, parsedTextId, parsedRawText);

        if (isFuelTextDraw) {
            if (hasParsedText && parsedTextId == id) {
                if (ShouldCacheFuelText(*state, id, parsedRawText)) {
                    CacheFuelText(*state, parsedRawText);
                }
            } else {
                state->fuelStale = true;
            }
        } else if (hasParsedText && parsedTextId == id && ShouldCacheFuelText(*state, id, parsedRawText)) {
            CacheFuelText(*state, parsedRawText);
        }

        if (rpcId == textdraw::RPC_SCR_TEXTDRAW_SET_STRING) {
            const bool suppressSetString = hasParsedText
                && parsedTextId == id
                && ShouldSuppressSetStringTextDraw(*state, id, parsedRawText);
            LogTextDrawRpcDiagnostic(*state, rpcId, bs, id, isHideTextDraw, isFuelTextDraw, suppressSetString);
            // Keep the authoritative text current; the render hook hides it.
            return true;
        }

        const int originalReadOffset = bs->GetReadOffset();
        int parsedId = -1;
        TextDrawSnapshot snapshot;
        std::string rawText;
        const bool hasSnapshot = TryReadShowTextDrawRpcSnapshot(bs, parsedId, snapshot, rawText);
        bs->SetReadOffset(originalReadOffset);

        if (hasSnapshot && parsedId == id && LooksLikeNlFuelTextDraw(id, snapshot)) {
            CacheFuelText(*state, rawText);
        }

        if (hasSnapshot && parsedId == id && IsVehicleContextActive(*state)) {
            CacheShowClassification(*state, id, ClassifyNlSpeedometerShowTextDraw(id, snapshot));
        }

        const bool suppressShow = hasSnapshot
            && parsedId == id
            && ShouldSuppressShowTextDraw(*state, id, snapshot);
        LogTextDrawRpcDiagnostic(*state, rpcId, bs, id, isHideTextDraw, isFuelTextDraw, suppressShow);
        return true;
    };

    initialized_ = true;
    Logger::Info("[TextDrawAdapter] TextDraw RPC hook pre-registered");
    return true;
#endif
}

void TextDrawAdapter::Shutdown() {
    if (!initialized_.load()) {
        return;
    }

    if (!state_->isAlive.exchange(false)) {
        return;
    }

    ClearFuelCache(state_);
    ClearShowClassificationCache(state_);
    state_->inVehicleContext = false;
    state_->renderSuppressionReady = false;
    state_->diagnosticsEnabled = false;
    // Keep initialized_ true: RakHook lambdas stay registered for process life.

#ifndef KEYBINDER_TESTING
    Logger::Info("[TextDrawAdapter] Shutdown - callbacks disabled");
#endif
}


void TextDrawAdapter::SetSpeedometerEnabled(bool enabled) {
    state_->speedometerEnabled = enabled;
    if (!enabled) {
        state_->renderHookAttempted = false;
        state_->fuelStale = true;
        state_->inVehicleContext = false;
        ClearFuelCache(state_);
        ClearShowClassificationCache(state_);
    }
}

void TextDrawAdapter::SetVehicleContext(bool inVehicle) {
    const bool previous = state_->inVehicleContext.exchange(inVehicle);
    if (!inVehicle && previous) {
        state_->fuelStale = true;
        ClearFuelCache(state_);
        ClearShowClassificationCache(state_);
    }
#ifndef KEYBINDER_TESTING
    if (inVehicle && state_->isAlive.load() && state_->speedometerEnabled.load()) {
        state_->renderSuppressionReady = EnsureTextDrawRenderHook(state_);
    }
#endif
}

bool TextDrawAdapter::IsRenderSuppressionReady() const {
    return initialized_.load() && state_->isAlive.load() && state_->renderSuppressionReady.load();
}

void TextDrawAdapter::SetDiagnosticsEnabled(bool enabled) {
    state_->diagnosticsEnabled = enabled;
    state_->diagnosticsEventCount = 0;

#ifndef KEYBINDER_TESTING
    Logger::Info("[TextDrawAdapter] TextDraw RPC diagnostics %s", enabled ? "enabled" : "disabled");
#endif
}

bool TextDrawAdapter::IsDiagnosticsEnabled() const {
    return state_->diagnosticsEnabled.load();
}

unsigned long TextDrawAdapter::GetDiagnosticsEventCount() const {
    return state_->diagnosticsEventCount.load();
}


std::string TextDrawAdapter::GetFuelText() const {
    GetFuelValue();
    std::lock_guard<std::mutex> lock(state_->cacheMutex);
    return state_->cachedFuelText;
}

float TextDrawAdapter::GetFuelValue() const {
    if (!IsVehicleContextActive(*state_)) {
        state_->fuelStale = true;
        ClearFuelCache(state_);
        return -1.0f;
    }

    if (state_->fuelStale.exchange(false)) {
        ClearFuelCache(state_);
    }

#ifdef KEYBINDER_TESTING
    {
        std::lock_guard<std::mutex> lock(state_->cacheMutex);
        if (!state_->cachedFuelText.empty()) {
            return state_->cachedFuelValue;
        }
    }
    return -1.0f;
#else
    sampapi::v03dl::CTextDrawPool* pool = nullptr;
    if (!TryGetTextDrawPool(&pool)) {
        ClearFuelCache(state_);
        return -1.0f;
    }

    {
        std::lock_guard<std::mutex> lock(state_->cacheMutex);
        if (!state_->cachedFuelText.empty()) {
            const float cachedValue = state_->cachedFuelValue;
            return cachedValue;
        }
    }

    float parsedFuelValue = -1.0f;
    bool foundFuelText = false;
    TextDrawSnapshot snapshot;
    for (int textDrawId = kNlTachoV2DynamicFirstTextDrawId; textDrawId <= kNlTachoV2DynamicLastTextDrawId; ++textDrawId) {
        if (TrySnapshotTextDraw(pool, textDrawId, &snapshot) && LooksLikeNlFuelTextDraw(textDrawId, snapshot)) {
            const std::string rawText = BoundedCString(snapshot.rawText, sizeof(snapshot.rawText));
            CacheFuelText(*state_, rawText);
            parsedFuelValue = ParseFuelValueFromText(rawText);
            foundFuelText = true;
            break;
        }
    }

    if (foundFuelText) {
        return parsedFuelValue;
    }

    ClearFuelCache(state_);
    return -1.0f;
#endif
}

bool TextDrawAdapter::DumpAllTextDrawsToFile(const std::string& outputPath, std::size_t* dumpedCount) const {
    if (dumpedCount != nullptr) {
        *dumpedCount = 0;
    }

#ifdef KEYBINDER_TESTING
    (void)outputPath;
    return false;
#else
    sampapi::v03dl::CTextDrawPool* pool = nullptr;
    if (!TryGetTextDrawPool(&pool)) {
        Logger::Warning("[TextDrawAdapter] TextDraw dump failed: TextDraw pool unavailable");
        return false;
    }

    struct DumpEntry {
        int id = 0;
        const char* poolName = nullptr;
        std::string rawText;
        std::string strippedText;
        float posX = 0.0f;
        float posY = 0.0f;
        int style = 0;
        int box = 0;
        int center = 0;
        float boxSizeX = 0.0f;
        float boxSizeY = 0.0f;
        float letterWidth = 0.0f;
        float letterHeight = 0.0f;
    };

    std::vector<DumpEntry> entries;
    entries.reserve(64);
    std::size_t serverCount = 0;
    std::size_t playerCount = 0;

    for (int id = 0; id <= kMaxTextDrawId; ++id) {
        TextDrawSnapshot snapshot;
        if (!TrySnapshotTextDraw(pool, id, &snapshot)) {
            continue;
        }

        const std::string rawText = BoundedCString(snapshot.rawText, sizeof(snapshot.rawText));
        if (rawText.empty()) {
            continue;
        }

        DumpEntry entry;
        entry.id = id;
        entry.poolName = id < kMaxServerTextDraws ? "server" : "player";
        entry.rawText = rawText;
        entry.strippedText = StripTextDrawColorCodes(rawText);
        entry.posX = snapshot.posX;
        entry.posY = snapshot.posY;
        entry.style = snapshot.style;
        entry.box = snapshot.box;
        entry.center = snapshot.center;
        entry.boxSizeX = snapshot.boxSizeX;
        entry.boxSizeY = snapshot.boxSizeY;
        entry.letterWidth = snapshot.letterWidth;
        entry.letterHeight = snapshot.letterHeight;

        if (id < kMaxServerTextDraws) {
            ++serverCount;
        } else {
            ++playerCount;
        }

        entries.push_back(std::move(entry));
    }

    std::ofstream file(outputPath.c_str(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        Logger::Warning("[TextDrawAdapter] TextDraw dump failed: cannot open '%s'", outputPath.c_str());
        return false;
    }

    file << "Timestamp: " << FormatTimestamp(std::time(nullptr)) << "\n";
    file << "ServerCount: " << serverCount << "\n";
    file << "PlayerCount: " << playerCount << "\n";
    file << "TotalCount: " << entries.size() << "\n\n";

    for (const DumpEntry& entry : entries) {
        file << "Pool: " << entry.poolName << "\n";
        file << "Id: " << entry.id << "\n";
        file << "RawText: " << entry.rawText << "\n";
        file << "StrippedText: " << entry.strippedText << "\n";
        file << "Position: (" << entry.posX << ", " << entry.posY << ")\n";
        file << "Style: " << entry.style << "\n";
        file << "Box: " << entry.box << "\n";
        file << "Center: " << entry.center << "\n";
        file << "BoxSize: (" << entry.boxSizeX << ", " << entry.boxSizeY << ")\n";
        file << "LetterSize: (" << entry.letterWidth << ", " << entry.letterHeight << ")\n\n";
    }

    if (!file.good()) {
        Logger::Warning("[TextDrawAdapter] TextDraw dump failed while writing '%s'", outputPath.c_str());
        return false;
    }

    if (dumpedCount != nullptr) {
        *dumpedCount = entries.size();
    }
    return true;
#endif
}

#ifdef KEYBINDER_TESTING
void TextDrawAdapter::RenderVisibilityMaskForTesting(int* visible, void** objects, std::size_t count,
    const std::vector<int>& hiddenIds, const std::function<void()>& draw) {
    DrawWithTemporaryVisibilityMask(visible, objects, count,
        [&hiddenIds](int id) { return std::find(hiddenIds.begin(), hiddenIds.end(), id) != hiddenIds.end(); }, draw);
}

std::string TextDrawAdapter::StripColorCodesForTesting(const std::string& text) {
    return StripTextDrawColorCodes(text);
}

float TextDrawAdapter::ParseFuelValueForTesting(const std::string& text) {
    return ParseFuelValueFromText(text);
}

bool TextDrawAdapter::TryParseRpcTextForTesting(unsigned char rpcId, RakNet::BitStream* bs, int& textDrawId, std::string& text) {
    return TryReadTextDrawRpcText(rpcId, bs, textDrawId, text);
}

bool TextDrawAdapter::TextDrawIdMatchesForTesting(int rpcId) {
    return MatchesNlSpeedometerTextDrawId(rpcId);
}

bool TextDrawAdapter::ShouldSuppressShowTextDrawForTesting(
    int rpcId,
    bool speedometerEnabled,
    bool inVehicleContext,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& rawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = speedometerEnabled;
    state.inVehicleContext = inVehicleContext;

    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(rawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, rawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';

    return ShouldSuppressShowTextDraw(state, rpcId, snapshot);
}

bool TextDrawAdapter::ShouldSuppressSetStringTextDrawForTesting(
    int rpcId,
    bool speedometerEnabled,
    bool inVehicleContext,
    const std::string& rawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = speedometerEnabled;
    state.inVehicleContext = inVehicleContext;
    return ShouldSuppressSetStringTextDraw(state, rpcId, rawText);
}
bool TextDrawAdapter::ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
    int rpcId,
    bool speedometerEnabled,
    bool inVehicleContext,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& currentRawText,
    const std::string& setStringRawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = speedometerEnabled;
    state.inVehicleContext = inVehicleContext;

    if (!IsVehicleContextActive(state) || !IsValidTextDrawId(rpcId)) {
        return false;
    }

    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(currentRawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, currentRawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';

    return LooksLikeNlTachoV2PlayerSpeedValueTextDraw(snapshot, setStringRawText);
}

bool TextDrawAdapter::ShouldCacheFuelTextForTesting(
    int rpcId,
    bool speedometerEnabled,
    bool inVehicleContext,
    const std::string& rawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = speedometerEnabled;
    state.inVehicleContext = inVehicleContext;
    return ShouldCacheFuelText(state, rpcId, rawText);
}

bool TextDrawAdapter::LooksLikeSpeedometerTextDrawForTesting(
    int rpcId,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& rawText) {
    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(rawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, rawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';
    return LooksLikeNlSpeedometerTextDraw(rpcId, snapshot);
}

std::string TextDrawAdapter::ClassifyShowTextDrawRoleForTesting(
    int rpcId,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& rawText) {
    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(rawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, rawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';
    return TextDrawRoleName(ClassifyNlSpeedometerShowTextDraw(rpcId, snapshot).role);
}

int TextDrawAdapter::ClassifyShowTextDrawConfidenceForTesting(
    int rpcId,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& rawText) {
    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(rawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, rawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';
    return ClassifyNlSpeedometerShowTextDraw(rpcId, snapshot).confidence;
}

bool TextDrawAdapter::ShouldSuppressSetStringAfterShowForTesting(
    int showRpcId,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& showRawText,
    int setStringRpcId,
    bool speedometerEnabled,
    bool inVehicleContext,
    const std::string& setStringRawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = speedometerEnabled;
    state.inVehicleContext = inVehicleContext;

    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(showRawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, showRawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';

    CacheShowClassification(state, showRpcId, ClassifyNlSpeedometerShowTextDraw(showRpcId, snapshot));
    return ShouldSuppressSetStringTextDraw(state, setStringRpcId, setStringRawText);
}

bool TextDrawAdapter::ShouldSuppressSetStringAfterShowContextChangeForTesting(
    int showRpcId,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& showRawText,
    bool showSpeedometerEnabled,
    bool showInVehicleContext,
    int setStringRpcId,
    bool setStringSpeedometerEnabled,
    bool setStringInVehicleContext,
    const std::string& setStringRawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = showSpeedometerEnabled;
    state.inVehicleContext = showInVehicleContext;

    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(showRawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, showRawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';

    if (IsVehicleContextActive(state)) {
        CacheShowClassification(state, showRpcId, ClassifyNlSpeedometerShowTextDraw(showRpcId, snapshot));
    }

    state.speedometerEnabled = setStringSpeedometerEnabled;
    state.inVehicleContext = setStringInVehicleContext;
    return ShouldSuppressSetStringTextDraw(state, setStringRpcId, setStringRawText);
}

bool TextDrawAdapter::ShouldSuppressSetStringAfterShowThenHideForTesting(
    int showRpcId,
    float posX,
    float posY,
    int style,
    int box,
    int center,
    float boxSizeX,
    float boxSizeY,
    float letterWidth,
    float letterHeight,
    const std::string& showRawText,
    int hideRpcId,
    int setStringRpcId,
    bool speedometerEnabled,
    bool inVehicleContext,
    const std::string& setStringRawText) {
    TextDrawAdapterState state;
    state.speedometerEnabled = speedometerEnabled;
    state.inVehicleContext = inVehicleContext;

    TextDrawSnapshot snapshot;
    snapshot.posX = posX;
    snapshot.posY = posY;
    snapshot.style = style;
    snapshot.box = box;
    snapshot.center = center;
    snapshot.boxSizeX = boxSizeX;
    snapshot.boxSizeY = boxSizeY;
    snapshot.letterWidth = letterWidth;
    snapshot.letterHeight = letterHeight;
    const std::size_t copyLength = (std::min)(showRawText.size(), sizeof(snapshot.rawText) - 1);
    if (copyLength > 0) {
        std::memcpy(snapshot.rawText, showRawText.data(), copyLength);
    }
    snapshot.rawText[copyLength] = '\0';

    if (IsVehicleContextActive(state)) {
        CacheShowClassification(state, showRpcId, ClassifyNlSpeedometerShowTextDraw(showRpcId, snapshot));
    }
    ClearShowClassification(state, hideRpcId);
    return ShouldSuppressSetStringTextDraw(state, setStringRpcId, setStringRawText);
}
#endif

} // namespace keybinder
