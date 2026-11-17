#pragma once

#include "ForumBrowserProtocol.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace keybinder::forum {

struct KeyCommandBatch {
    bool handled = false;
    std::array<Command, 2> commands{};
    std::size_t count = 0;
};

// Consume Windows' translated text instead of translating virtual keys again:
// a second ToUnicodeEx call would disturb dead-key composition. The overlay
// installs a Unicode WndProc, so WM_CHAR already contains UTF-16 code units.
inline KeyCommandBatch BuildForumKeyCommands(UINT message, WPARAM key,
                                             LPARAM native, unsigned int modifiers) {
    KeyCommandBatch result;
    const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
    const bool character = message == WM_CHAR || message == WM_SYSCHAR;
    const bool unicodeScalar = message == WM_UNICHAR;
    const bool deadCharacter = message == WM_DEADCHAR || message == WM_SYSDEADCHAR;
    if (!down && !up && !character && !unicodeScalar && !deadCharacter) return result;

    // Escape belongs to the overlay close action, Alt+F4 to the native window.
    if (key == VK_ESCAPE) return result;
    if ((down || up) && key == VK_F4
        && (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || (modifiers & Alt))) return result;
    result.handled = true;
    if (deadCharacter || (unicodeScalar && key == UNICODE_NOCHAR)) return result;

    if (character && key > 0xFFFFu) return result;
    if (unicodeScalar && (key > 0x10FFFFu || (key >= 0xD800u && key <= 0xDFFFu))) return result;
    if ((character || unicodeScalar) && key == 0) return result;

    Command command{CommandType::Key};
    command.a = static_cast<int>(character || unicodeScalar ? KeyType::Character
        : (up ? KeyType::Up : KeyType::Down));
    command.b = static_cast<std::int32_t>(key);
    command.c = static_cast<std::int32_t>(static_cast<std::uint32_t>(native));
    command.d = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SYSCHAR;
    command.modifiers = modifiers;
    if (up) command.modifiers &= ~static_cast<unsigned int>(Repeat);
    else if (static_cast<std::uint32_t>(native) & (1u << 30)) command.modifiers |= Repeat;

    // On Windows AltGr is reported as Ctrl+Alt. Passing those shortcut flags
    // with an already translated '[' makes Chromium treat it as Ctrl+[ (ESC).
    // CEF's OSR example likewise clears Control/Alt for AltGr character events:
    // https://github.com/chromiumembedded/cef/blob/master/tests/cefclient/browser/osr_window_win.cc
    // Send printable text without shortcut/system flags, remaining compatible
    // with the installed host protocol. Raw Ctrl+C/V/X/A events stay untouched.
    const bool printable = key >= 0x20u && key != 0x7Fu;
    if ((character || unicodeScalar) && printable
        && (modifiers & (Control | Alt)) == (Control | Alt)) {
        command.modifiers &= ~static_cast<unsigned int>(Control | Alt);
        command.d = 0;
        command.c = static_cast<std::int32_t>(static_cast<std::uint32_t>(command.c) & ~(1u << 29));
    }

    if (unicodeScalar && key > 0xFFFFu) {
        const auto scalar = static_cast<std::uint32_t>(key) - 0x10000u;
        command.b = static_cast<std::int32_t>(0xD800u + (scalar >> 10));
        result.commands[result.count++] = command;
        command.b = static_cast<std::int32_t>(0xDC00u + (scalar & 0x3FFu));
    }
    result.commands[result.count++] = command;
    return result;
}

} // namespace keybinder::forum
