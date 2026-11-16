#include "../include/ForumKeyboardInput.h"

#include <cstdio>
#include <cstdint>
#include <limits>

namespace {
using namespace keybinder::forum;
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] ForumKeyboardInput: %s (line %d)\n", #value, __LINE__); return; } } while (0)

constexpr LPARAM Native(std::uint32_t bits) { return static_cast<LPARAM>(static_cast<std::int32_t>(bits)); }

void AltGrTextKeepsTheTranslatedCharacters() {
    // German layout: AltGr+7/8/9/0/Q/E/ß/< /+; also shifted AltGr characters.
    for (WPARAM text : {WPARAM('['), WPARAM(']'), WPARAM('{'), WPARAM('}'), WPARAM('@'),
                       WPARAM(0x20AC), WPARAM('\\'), WPARAM('|'), WPARAM('~'), WPARAM(0x1E9E)}) {
        for (UINT message : {UINT(WM_CHAR), UINT(WM_SYSCHAR)}) {
            const auto batch = BuildForumKeyCommands(message, text, Native(0x60100001u),
                Control | Alt | Shift | CapsLock | NumLock | LeftButton);
            CHECK(batch.handled && batch.count == 1);
            const auto& command = batch.commands[0];
            CHECK(command.type == CommandType::Key);
            CHECK(command.a == static_cast<int>(KeyType::Character));
            CHECK(command.b == static_cast<std::int32_t>(text));
            CHECK((command.modifiers & (Control | Alt)) == 0);
            CHECK((command.modifiers & (Shift | CapsLock | NumLock | LeftButton))
                == (Shift | CapsLock | NumLock | LeftButton));
            CHECK(command.modifiers & Repeat);
            CHECK(command.d == 0);
            CHECK(static_cast<std::uint32_t>(command.c) == 0x40100001u);
        }
    }
}

void RawKeysAndRealShortcutsRemainIntact() {
    for (WPARAM key : {WPARAM('C'), WPARAM('V'), WPARAM('X'), WPARAM('A'), WPARAM(VK_DELETE),
                       WPARAM(VK_HOME), WPARAM(VK_END), WPARAM(VK_LEFT), WPARAM(VK_RIGHT),
                       WPARAM(VK_UP), WPARAM(VK_DOWN), WPARAM(VK_TAB), WPARAM(VK_RETURN)}) {
        const auto down = BuildForumKeyCommands(WM_KEYDOWN, key, Native(0x001E0001u), Control | Shift);
        CHECK(down.handled && down.count == 1);
        CHECK(down.commands[0].a == static_cast<int>(KeyType::Down));
        CHECK(down.commands[0].b == static_cast<std::int32_t>(key) && down.commands[0].c == Native(0x001E0001u));
        CHECK(down.commands[0].modifiers == (Control | Shift));
        CHECK(down.commands[0].d == 0);
        const auto up = BuildForumKeyCommands(WM_KEYUP, key, Native(0xC01E0001u), Control | Shift | Repeat);
        CHECK(up.handled && up.count == 1);
        CHECK(up.commands[0].a == static_cast<int>(KeyType::Up));
        CHECK(up.commands[0].b == static_cast<std::int32_t>(key) && up.commands[0].c == Native(0xC01E0001u));
        CHECK(up.commands[0].modifiers == (Control | Shift));
    }
    const auto altGrKey = BuildForumKeyCommands(WM_SYSKEYDOWN, '8', Native(0x20090001u), Control | Alt);
    CHECK(altGrKey.handled && altGrKey.count == 1);
    CHECK(altGrKey.commands[0].modifiers == (Control | Alt));
    CHECK(altGrKey.commands[0].d == 1);
    const auto controlCharacter = BuildForumKeyCommands(WM_CHAR, 3, Native(0x002E0001u), Control);
    CHECK(controlCharacter.handled && controlCharacter.count == 1);
    CHECK(controlCharacter.commands[0].b == 3 && controlCharacter.commands[0].modifiers == Control);
    const auto genuineAltText = BuildForumKeyCommands(WM_SYSCHAR, 'f', Native(0x20210001u), Alt);
    CHECK(genuineAltText.handled && genuineAltText.count == 1);
    CHECK(genuineAltText.commands[0].d == 1 && genuineAltText.commands[0].modifiers == Alt);
}

void PlainUnicodeTextAndDeadKeysAreNotRetranslated() {
    for (WPARAM text : {WPARAM('a'), WPARAM('A'), WPARAM('!'), WPARAM(0x00E4), WPARAM(0x00F6),
                       WPARAM(0x00FC), WPARAM(0x00DF), WPARAM(0x00E9), WPARAM(0x03A9), WPARAM(0x4E2D)}) {
        const auto batch = BuildForumKeyCommands(WM_CHAR, text, Native(0x001E0001u), Shift | CapsLock);
        CHECK(batch.handled && batch.count == 1);
        CHECK(batch.commands[0].b == static_cast<std::int32_t>(text));
        CHECK(batch.commands[0].modifiers == (Shift | CapsLock));
    }
    for (UINT message : {UINT(WM_DEADCHAR), UINT(WM_SYSDEADCHAR)}) {
        const auto dead = BuildForumKeyCommands(message, '^', Native(0x00290001u), 0);
        CHECK(dead.handled && dead.count == 0);
    }
    const auto composed = BuildForumKeyCommands(WM_CHAR, 0x00E2, Native(0x001E0001u), 0);
    CHECK(composed.handled && composed.count == 1 && composed.commands[0].b == 0x00E2);
    const auto high = BuildForumKeyCommands(WM_CHAR, 0xD83D, Native(0x00000001u), 0);
    const auto low = BuildForumKeyCommands(WM_CHAR, 0xDE00, Native(0x00000001u), 0);
    CHECK(high.handled && high.count == 1 && high.commands[0].b == 0xD83D);
    CHECK(low.handled && low.count == 1 && low.commands[0].b == 0xDE00);
}

void UnicodeScalarInputSupportsSupplementaryCharacters() {
    const auto probe = BuildForumKeyCommands(WM_UNICHAR, UNICODE_NOCHAR, 0, 0);
    CHECK(probe.handled && probe.count == 0);
    const auto euro = BuildForumKeyCommands(WM_UNICHAR, 0x20AC, 0, Control | Alt);
    CHECK(euro.handled && euro.count == 1 && euro.commands[0].b == 0x20AC);
    CHECK(euro.commands[0].modifiers == 0);
    const auto emoji = BuildForumKeyCommands(WM_UNICHAR, 0x1F600, Native(0x20000001u), Control | Alt | Shift);
    CHECK(emoji.handled && emoji.count == 2);
    CHECK(emoji.commands[0].b == 0xD83D && emoji.commands[1].b == 0xDE00);
    CHECK(emoji.commands[0].type == CommandType::Key && emoji.commands[1].type == CommandType::Key);
    CHECK(emoji.commands[0].a == static_cast<int>(KeyType::Character)
        && emoji.commands[1].a == static_cast<int>(KeyType::Character));
    CHECK(emoji.commands[0].modifiers == Shift && emoji.commands[1].modifiers == Shift);
    CHECK(emoji.commands[0].d == 0 && emoji.commands[1].d == 0);
    CHECK(emoji.commands[0].c == 1 && emoji.commands[1].c == 1);
    const auto maximum = BuildForumKeyCommands(WM_UNICHAR, 0x10FFFF, 0, 0);
    CHECK(maximum.handled && maximum.count == 2);
    CHECK(maximum.commands[0].b == 0xDBFF && maximum.commands[1].b == 0xDFFF);
    for (WPARAM invalid : {WPARAM(0), WPARAM(0xD800), WPARAM(0xDFFF), WPARAM(0x110000),
                          (std::numeric_limits<WPARAM>::max)()}) {
        const auto rejected = BuildForumKeyCommands(WM_UNICHAR, invalid, 0, Control | Alt);
        CHECK(rejected.handled && rejected.count == 0);
    }
    const auto invalidChar = BuildForumKeyCommands(WM_CHAR, 0x10000, 0, 0);
    CHECK(invalidChar.handled && invalidChar.count == 0);
    const auto nul = BuildForumKeyCommands(WM_CHAR, 0, 0, 0);
    CHECK(nul.handled && nul.count == 0);
}

void EscapeAndNativeCloseRemainAvailable() {
    for (UINT message : {UINT(WM_KEYDOWN), UINT(WM_KEYUP), UINT(WM_SYSKEYDOWN), UINT(WM_SYSKEYUP),
                         UINT(WM_CHAR), UINT(WM_SYSCHAR), UINT(WM_UNICHAR)}) {
        const auto escape = BuildForumKeyCommands(message, VK_ESCAPE, 0, 0);
        CHECK(!escape.handled && escape.count == 0);
    }
    for (UINT message : {UINT(WM_SYSKEYDOWN), UINT(WM_SYSKEYUP), UINT(WM_KEYDOWN), UINT(WM_KEYUP)}) {
        const auto close = BuildForumKeyCommands(message, VK_F4, Native(0x203E0001u), Alt);
        CHECK(!close.handled && close.count == 0);
    }
    const auto plainF4 = BuildForumKeyCommands(WM_KEYDOWN, VK_F4, Native(0x003E0001u), 0);
    CHECK(plainF4.handled && plainF4.count == 1);
    // The character 's' has the same numeric value as VK_F4 but is ordinary text.
    const auto textS = BuildForumKeyCommands(WM_SYSCHAR, 's', Native(0x201F0001u), Control | Alt);
    CHECK(textS.handled && textS.count == 1 && textS.commands[0].b == 's');
    for (UINT message : {UINT(WM_MOUSEMOVE), UINT(WM_KILLFOCUS), UINT(WM_INPUT), UINT(WM_CLOSE)}) {
        const auto other = BuildForumKeyCommands(message, 'a', 0, 0);
        CHECK(!other.handled && other.count == 0);
    }
}

void RepeatFlagsDistinguishCharacterRepeatsFromReleases() {
    for (UINT message : {UINT(WM_KEYDOWN), UINT(WM_SYSKEYDOWN), UINT(WM_CHAR), UINT(WM_SYSCHAR)}) {
        const auto repeat = BuildForumKeyCommands(message, 'x', Native(0x40100001u), 0);
        CHECK(repeat.handled && repeat.count == 1 && (repeat.commands[0].modifiers & Repeat));
    }
    for (UINT message : {UINT(WM_KEYUP), UINT(WM_SYSKEYUP)}) {
        const auto release = BuildForumKeyCommands(message, 'x', Native(0xC0100001u), Alt | Repeat);
        CHECK(release.handled && release.count == 1 && !(release.commands[0].modifiers & Repeat));
        CHECK(release.commands[0].modifiers & Alt);
    }
}
} // namespace

int RunForumKeyboardInputTests() {
    checks = failures = 0;
    AltGrTextKeepsTheTranslatedCharacters();
    RawKeysAndRealShortcutsRemainIntact();
    PlainUnicodeTextAndDeadKeysAreNotRetranslated();
    UnicodeScalarInputSupportsSupplementaryCharacters();
    EscapeAndNativeCloseRemainAvailable();
    RepeatFlagsDistinguishCharacterRepeatsFromReleases();
    std::printf("ForumKeyboardInputTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
