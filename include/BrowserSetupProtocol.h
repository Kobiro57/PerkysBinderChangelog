#pragma once
#include <cstdint>

namespace keybinder::browser_setup {
inline constexpr std::uint32_t kSetupMagic = 0x4E4C4253;
enum class Phase : std::int32_t { Checking=1, Copying, Downloading, Extracting, Ready, Failed, Cancelled };
struct SetupProgress {
    std::uint32_t magic = kSetupMagic;
    std::uint32_t bytes = sizeof(SetupProgress);
    volatile std::int32_t phase = static_cast<std::int32_t>(Phase::Checking);
    volatile std::int32_t percent = 0;
    volatile std::int32_t cancel = 0;
    char error[256]{};
};
static_assert(sizeof(SetupProgress)==276);
}
