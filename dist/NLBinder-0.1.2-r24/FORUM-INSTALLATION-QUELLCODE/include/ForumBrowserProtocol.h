#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace keybinder::forum {
constexpr std::uint32_t kMagic = 0x4E4C4346;
constexpr std::uint32_t kVersion = 1;
constexpr int kMaximumWidth = 1920;
constexpr int kMaximumHeight = 1080;
constexpr unsigned int kCommandCount = 1024;
constexpr const char* kHome = "https://nova-legacy.de/forum";

enum class CommandType : std::uint32_t {
    Resize, MouseMove, MouseButton, MouseWheel, Key, Focus,
    Back, Forward, Reload, Home, Close, Copy, Paste, Cut, SelectAll
};
enum class State : std::int32_t { Starting, Ready, Closing, Closed, Error };
enum class KeyType : std::int32_t { Down, Up, Character };
enum Modifiers : std::uint32_t {
    Shift = 1, Control = 2, Alt = 4, LeftButton = 8, MiddleButton = 16,
    RightButton = 32, CapsLock = 64, NumLock = 128, Repeat = 256
};
struct Command {
    CommandType type;
    std::int32_t a = 0, b = 0, c = 0, d = 0, e = 0;
    std::uint32_t modifiers = 0;
};

// Fixed-width POD layout shared by the x86 ASI and the x64 CEF host.
// Synchronization uses Win32 Interlocked operations, never C++ objects or
// process-local pointers in shared memory. Characters/keys are not logged.
struct alignas(8) SharedState {
    std::uint32_t magic = 0, version = 0, bytes = 0, reserved = 0;
    volatile std::int32_t commandWrite = 0, commandRead = 0;
    volatile std::int32_t frameLock = 0, frameSequence = 0;
    volatile std::int32_t state = 0, width = 0, height = 0;
    volatile std::int32_t canGoBack = 0, canGoForward = 0, loading = 0;
    volatile std::int32_t errorCode = 0, blockedLink = 0;
    std::uint64_t parentWindow = 0;
    volatile std::int32_t screenX = 0, screenY = 0;
    char title[512]{};
    char url[2048]{};
    Command commands[kCommandCount]{};
    std::uint8_t pixels[kMaximumWidth * kMaximumHeight * 4]{};
};
static_assert(sizeof(Command) == 28);
static_assert(offsetof(SharedState, parentWindow) == 64);
static_assert(std::is_trivially_copyable_v<Command> && std::is_trivially_copyable_v<SharedState>);
} // namespace keybinder::forum
