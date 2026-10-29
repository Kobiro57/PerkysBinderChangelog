#pragma once
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <string>

namespace keybinder {

// Worker-only launch policy. A successful button invocation is not a successful
// reconnect: the new binder confirms entry after its local player has spawned.
class NovaReconnectFlow {
public:
    enum class Action { Wait, InvokePlay, Complete, TimedOut };
    NovaReconnectFlow(std::uint64_t started, std::uint64_t timeout = 90000,
                      std::uint64_t retry = 2500)
        : deadline_(started + timeout), retry_(retry), next_attempt_(started) {}
    Action Update(std::uint64_t now, bool gameExists, bool entered, bool playAvailable) {
        if (entered && gameExists) return Action::Complete;
        if (now >= deadline_) return Action::TimedOut;
        if (gameExists || !playAvailable || now < next_attempt_) return Action::Wait;
        next_attempt_ = now + retry_;
        ++attempts_;
        return Action::InvokePlay;
    }
    unsigned Attempts() const { return attempts_; }
private:
    std::uint64_t deadline_, retry_, next_attempt_;
    unsigned attempts_ = 0;
};

inline std::wstring NormalizeNovaPlayLabel(const std::wstring& input) {
    std::wstring result;
    bool space = false;
    for (wchar_t ch : input) {
        if (std::iswspace(ch) || ch == 0xa0) { space = !result.empty(); continue; }
        if (space) result += L' ';
        result += static_cast<wchar_t>(std::towlower(ch));
        space = false;
    }
    return result;
}

inline std::wstring ReconnectEnteredEventName(const std::wstring& gameExecutable, unsigned long pid) {
    // Session-local and scoped to the exact executable plus the NEW process ID.
    // The previous game cannot confirm a replacement process's connection.
    std::uint64_t hash = 14695981039346656037ull;
    for (wchar_t ch : gameExecutable) {
        if (ch == L'/') ch = L'\\';
        ch = static_cast<wchar_t>(std::towlower(ch));
        hash ^= static_cast<unsigned>(ch);
        hash *= 1099511628211ull;
    }
    return L"Local\\NLBinderReconnectEntered-" + std::to_wstring(pid) + L"-" + std::to_wstring(hash);
}

} // namespace keybinder
