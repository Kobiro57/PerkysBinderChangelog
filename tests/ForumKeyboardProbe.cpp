// Emits only synthetic test commands through the same translator as the ASI.
// The Python integration runner forwards these bytes to the unchanged CEF host.
#include "../include/ForumKeyboardInput.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace ipc = keybinder::forum;
namespace {
struct Case {
    const char* name;
    const char* element = "input";
    std::vector<std::uint32_t> expected;
    std::vector<ipc::Command> commands;
    bool legacy = false;
    bool prefill = false;
};

void Append(Case& test, UINT message, WPARAM key, std::uint32_t native, unsigned modifiers) {
    if (!test.legacy) {
        const auto batch = ipc::BuildForumKeyCommands(message, key, static_cast<LPARAM>(native), modifiers);
        test.commands.insert(test.commands.end(), batch.commands.begin(), batch.commands.begin() + batch.count);
        return;
    }
    ipc::Command command{ipc::CommandType::Key};
    command.a = static_cast<int>((message == WM_CHAR || message == WM_SYSCHAR) ? ipc::KeyType::Character
        : ((message == WM_KEYUP || message == WM_SYSKEYUP) ? ipc::KeyType::Up : ipc::KeyType::Down));
    command.b = static_cast<int>(key);
    command.c = static_cast<int>(native);
    command.d = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SYSCHAR;
    command.modifiers = modifiers;
    if (native & (1u << 30)) command.modifiers |= ipc::Repeat;
    test.commands.push_back(command);
}

void AltGrBegin(Case& test) {
    Append(test, WM_KEYDOWN, VK_CONTROL, 0x001D0001, ipc::Control);
    Append(test, WM_SYSKEYDOWN, VK_MENU, 0x21380001, ipc::Control | ipc::Alt);
}
void AltGrEnd(Case& test) {
    Append(test, WM_SYSKEYUP, VK_MENU, 0xE1380001, ipc::Control);
    Append(test, WM_KEYUP, VK_CONTROL, 0xC01D0001, 0);
}
void AltGrCharacter(Case& test, std::uint32_t character, int virtualKey, unsigned scan, bool systemCharacter = false) {
    const unsigned mods = ipc::Control | ipc::Alt;
    Append(test, WM_KEYDOWN, virtualKey, 1u | (scan << 16), mods);
    Append(test, systemCharacter ? WM_SYSCHAR : WM_CHAR, character,
        1u | (scan << 16) | (systemCharacter ? (1u << 29) : 0), mods);
    Append(test, WM_KEYUP, virtualKey, 0xC0000001u | (scan << 16), mods);
}

Case AltGr(const char* name, const char* element, bool legacy = false, bool systemCharacter = false) {
    Case test{name, element};
    test.legacy = legacy;
    test.expected = {'[', ']', '{', '}', '@', 0x20AC, '\\', '|', '~'};
    AltGrBegin(test);
    const int keys[] = {'8', '9', '7', '0', 'Q', 'E', VK_OEM_4, VK_OEM_102, VK_OEM_PLUS};
    const unsigned scans[] = {9, 10, 8, 11, 16, 18, 12, 86, 27};
    for (std::size_t i = 0; i < test.expected.size(); ++i)
        AltGrCharacter(test, test.expected[i], keys[i], scans[i], systemCharacter);
    AltGrEnd(test);
    return test;
}

Case Unicode() {
    Case test{"unicode_text_and_supplementary_scalar", "textarea"};
    test.expected = {0xE4, 0xF6, 0xFC, 0xC4, 0xD6, 0xDC, 0xDF, 0x20AC, 0x1F600};
    for (std::uint32_t scalar : test.expected)
        Append(test, scalar > 0xFFFF ? WM_UNICHAR : WM_CHAR, scalar, 1, 0);
    return test;
}

Case DeadKey() {
    Case test{"composed_dead_key_text", "input"};
    test.expected = {0xEA, ' ', '^'};
    Append(test, WM_KEYDOWN, VK_OEM_5, 0x00290001, 0);
    Append(test, WM_DEADCHAR, '^', 0x00290001, 0);
    Append(test, WM_KEYUP, VK_OEM_5, 0xC0290001, 0);
    Append(test, WM_KEYDOWN, 'E', 0x00120001, 0);
    Append(test, WM_CHAR, 0xEA, 0x00120001, 0);
    Append(test, WM_KEYUP, 'E', 0xC0120001, 0);
    Append(test, WM_CHAR, ' ', 0x00390001, 0);
    Append(test, WM_CHAR, '^', 0x00290001, 0);
    return test;
}

Case ControlA() {
    Case test{"control_a_selection_then_typing", "textarea"};
    test.prefill = true;
    test.expected = {'[', 'o', 'k', ']'};
    Append(test, WM_KEYDOWN, VK_CONTROL, 0x001D0001, ipc::Control);
    Append(test, WM_KEYDOWN, 'A', 0x001E0001, ipc::Control);
    Append(test, WM_CHAR, 1, 0x001E0001, ipc::Control);
    Append(test, WM_KEYUP, 'A', 0xC01E0001, ipc::Control);
    Append(test, WM_KEYUP, VK_CONTROL, 0xC01D0001, 0);
    AltGrBegin(test);
    AltGrCharacter(test, '[', '8', 9);
    AltGrEnd(test);
    Append(test, WM_CHAR, 'o', 1, 0);
    Append(test, WM_CHAR, 'k', 1, 0);
    AltGrBegin(test);
    AltGrCharacter(test, ']', '9', 10);
    AltGrEnd(test);
    return test;
}

void Print(const std::vector<Case>& cases) {
    std::cout << "{\"protocol_version\":1,\"cases\":[";
    bool firstCase = true;
    for (const auto& test : cases) {
        if (!firstCase) std::cout << ',';
        firstCase = false;
        std::cout << "{\"name\":\"" << test.name << "\",\"element\":\"" << test.element
            << "\",\"legacy\":" << (test.legacy ? "true" : "false")
            << ",\"prefill\":" << (test.prefill ? "true" : "false") << ",\"expected_scalars\":[";
        for (std::size_t i = 0; i < test.expected.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << test.expected[i];
        }
        std::cout << "],\"commands\":[";
        for (std::size_t i = 0; i < test.commands.size(); ++i) {
            if (i) std::cout << ',';
            const auto& c = test.commands[i];
            std::cout << '[' << static_cast<int>(c.type) << ',' << c.a << ',' << c.b << ',' << c.c
                << ',' << c.d << ',' << c.e << ',' << c.modifiers << ']';
        }
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
} // namespace

int main() {
    Print({AltGr("legacy_altgr_characters", "input", true),
        AltGr("altgr_input", "input"), AltGr("altgr_textarea", "textarea"),
        AltGr("altgr_contenteditable", "contenteditable"),
        AltGr("altgr_system_character", "input", false, true), Unicode(), DeadKey(), ControlA()});
}
