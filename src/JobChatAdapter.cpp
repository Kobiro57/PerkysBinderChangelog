#include "../include/JobChatAdapter.h"

#include "../include/Logger.h"

#ifdef KEYBINDER_TESTING
#include "../include/SAMPBridge.h"
#else
#include "../include/SAMP-API/include/sampapi/0.3.DL-1/CChat.h"
#include "../include/SAMP-API/include/sampapi/sampapi.h"
#include "../include/plugin-sdk/safetyhook/safetyhook.hpp"
#endif

#include <cctype>
#include <cstring>
#include <utility>

#include <windows.h>

namespace {

std::string TrimCopy(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = text.find_last_not_of(" \t\n\r");
    return text.substr(first, last - first + 1);
}

bool IsHexColorCodeChar(char ch) {
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

std::string BuildRawLine(const char* prefix, const char* text) {
    const std::string safePrefix = prefix != nullptr ? prefix : "";
    const std::string safeText = text != nullptr ? text : "";

    if (safePrefix.empty()) {
        return safeText;
    }

    if (safeText.empty()) {
        return safePrefix;
    }

    return safePrefix + ": " + safeText;
}

bool IsValidUtf8(const std::string& text) {
    if (text.empty()) {
        return true;
    }

    return ::MultiByteToWideChar(CP_UTF8,
                                 MB_ERR_INVALID_CHARS,
                                 text.data(),
                                 static_cast<int>(text.size()),
                                 nullptr,
                                 0) > 0;
}

std::string ConvertGameEncodingToUtf8(const std::string& text) {
    if (text.empty() || IsValidUtf8(text)) {
        return text;
    }

    const int wideLength = ::MultiByteToWideChar(CP_ACP,
                                                 0,
                                                 text.data(),
                                                 static_cast<int>(text.size()),
                                                 nullptr,
                                                 0);
    if (wideLength <= 0) {
        return text;
    }

    std::wstring wideText(static_cast<std::size_t>(wideLength), L'\0');
    if (::MultiByteToWideChar(CP_ACP,
                              0,
                              text.data(),
                              static_cast<int>(text.size()),
                              &wideText[0],
                              wideLength) <= 0) {
        return text;
    }

    const int utf8Length = ::WideCharToMultiByte(CP_UTF8,
                                                 0,
                                                 wideText.data(),
                                                 wideLength,
                                                 nullptr,
                                                 0,
                                                 nullptr,
                                                 nullptr);
    if (utf8Length <= 0) {
        return text;
    }

    std::string utf8Text(static_cast<std::size_t>(utf8Length), '\0');
    if (::WideCharToMultiByte(CP_UTF8,
                              0,
                              wideText.data(),
                              wideLength,
                              &utf8Text[0],
                              utf8Length,
                              nullptr,
                              nullptr) <= 0) {
        return text;
    }

    return utf8Text;
}

std::string StripTimestamp(const std::string& text) {
    if (text.size() < 10 || text[0] != '[' || text[3] != ':' || text[6] != ':' || text[9] != ']') {
        return text;
    }

    for (std::size_t i = 1; i < 9; ++i) {
        if (i == 3 || i == 6) {
            continue;
        }

        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return text;
        }
    }

    std::size_t start = 10;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    return text.substr(start);
}

std::string StripColorCodes(const std::string& text) {
    std::string stripped;
    stripped.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{' && i + 7 < text.size() && text[i + 7] == '}') {
            bool isColorCode = true;
            for (std::size_t j = i + 1; j < i + 7; ++j) {
                if (!IsHexColorCodeChar(text[j])) {
                    isColorCode = false;
                    break;
                }
            }

            if (isColorCode) {
                i += 7;
                continue;
            }
        }

        stripped.push_back(text[i]);
    }

    return stripped;
}

std::string ExtractJobPrefix(const std::string& normalizedLine) {
    const std::size_t separator = normalizedLine.find(':');
    if (separator == std::string::npos) {
        return {};
    }

    return TrimCopy(normalizedLine.substr(0, separator));
}

bool EmitChatEvent(const std::shared_ptr<jobs::JobChatAdapterState>& state,
                   const char* prefix,
                   const char* text) {
    if (!state || !state->isAlive || (!state->callback && !state->visibilityFilter)) {
        return true;
    }

    jobs::JobChatEvent event;
    event.rawLine = ConvertGameEncodingToUtf8(BuildRawLine(prefix, text));
    event.timestampFreeLine = TrimCopy(StripTimestamp(event.rawLine));
    event.normalizedLine = TrimCopy(StripColorCodes(event.timestampFreeLine));
    event.jobPrefix = ExtractJobPrefix(event.normalizedLine);
    event.arrivalTime = std::time(nullptr);

    // Decide visibility before delivering the event: a callback may complete
    // the automatic statistics block and clear its filter state on this line.
    const bool visible = !state->visibilityFilter || state->visibilityFilter(event);
    if (event.normalizedLine.empty()) return visible;

    if (!event.jobPrefix.empty()) {
        keybinder::Logger::Debug(
            "[JobChatAdapter] Captured chat raw='%s' normalized='%s' prefix='%s'",
            event.rawLine.c_str(),
            event.normalizedLine.c_str(),
            event.jobPrefix.c_str());
    }

    if (state->callback) state->callback(event);
    return visible;
}

#ifndef KEYBINDER_TESTING
safetyhook::InlineHook* g_addEntryHook = nullptr;

std::weak_ptr<jobs::JobChatAdapterState>& GetAdapterStateSlot() {
    static std::weak_ptr<jobs::JobChatAdapterState> state;
    return state;
}

void __fastcall HookedAddEntry(sampapi::v03dl::CChat* chat,
                               void* /*edx*/,
                               int nType,
                               const char* text,
                               const char* prefix,
                               unsigned long textColor,
                               unsigned long prefixColor) {
    (void)nType;
    (void)textColor;
    (void)prefixColor;

    bool visible = true;
    if (auto state = GetAdapterStateSlot().lock()) {
        visible = EmitChatEvent(state, prefix, text);
    }

    if (visible && g_addEntryHook != nullptr) {
        g_addEntryHook->thiscall<void>(chat, nType, text, prefix, textColor, prefixColor);
    }
}
#endif

} // namespace

namespace jobs {

JobChatAdapter::JobChatAdapter()
    : initialized_(false)
    , state_(std::make_shared<JobChatAdapterState>()) {
}

JobChatAdapter::~JobChatAdapter() {
    if (initialized_) {
        Shutdown();
    }
}

bool JobChatAdapter::Initialize() {
    if (initialized_) {
        return true;
    }

    state_->isAlive = true;

#ifdef KEYBINDER_TESTING
    const auto& hooks = SAMPBridge::GetTestingHooks();
    if (hooks.installIncomingChatInterceptor) {
        std::weak_ptr<JobChatAdapterState> weakState = state_;
        hooks.installIncomingChatInterceptor([weakState](sampapi::v03dl::CChat* chat,
                                                         const char* prefix,
                                                         unsigned long prefixColor,
                                                         const char* text) {
            (void)chat;
            (void)prefixColor;

            if (auto state = weakState.lock()) {
                EmitChatEvent(state, prefix, text);
            }
        });
    }

    initialized_ = true;
    return true;
#else
    GetAdapterStateSlot() = state_;

    if (g_addEntryHook == nullptr) {
        void* addEntry = reinterpret_cast<void*>(sampapi::GetBase() + 0x67650);
        auto hook = safetyhook::InlineHook::create(addEntry, reinterpret_cast<void*>(&HookedAddEntry));
        if (!hook) {
            GetAdapterStateSlot().reset();
            state_->isAlive = false;
            keybinder::Logger::Warning("[JobChatAdapter] Failed to install CChat::AddEntry hook");
            return false;
        }

        g_addEntryHook = new safetyhook::InlineHook(std::move(*hook));
    }

    initialized_ = true;
    keybinder::Logger::Info("[JobChatAdapter] Initialized CChat::AddEntry hook");
    return true;
#endif
}

void JobChatAdapter::Shutdown() {
    state_->isAlive = false;
    state_->callback = nullptr;
    state_->visibilityFilter = nullptr;
    if (!initialized_) {
        return;
    }

#ifdef KEYBINDER_TESTING
    const auto& hooks = SAMPBridge::GetTestingHooks();
    if (hooks.installIncomingChatInterceptor) {
        hooks.installIncomingChatInterceptor({});
    }
#else
    GetAdapterStateSlot().reset();
    delete g_addEntryHook;
    g_addEntryHook = nullptr;
    keybinder::Logger::Info("[JobChatAdapter] Shutdown complete");
#endif

    initialized_ = false;
}

void JobChatAdapter::SetCallback(JobChatEventCallback callback) {
    state_->callback = std::move(callback);
}

void JobChatAdapter::SetVisibilityFilter(JobChatVisibilityFilter filter) {
    state_->visibilityFilter = std::move(filter);
}

#ifdef KEYBINDER_TESTING
bool JobChatAdapter::DispatchLineForTesting(const char* prefix, const char* text) const {
    return EmitChatEvent(state_, prefix, text);
}
#endif

} // namespace jobs
