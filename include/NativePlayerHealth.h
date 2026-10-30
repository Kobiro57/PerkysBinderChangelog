#pragma once

#include <cmath>

namespace keybinder {

struct NativePlayerHealth {
    float value;
    bool protectedEncoding;
};

// Observed in the running Nova client: 8,000,094 means 94 HP;
// 7,999,945 is lethal overkill and 8,000,000 means zero HP. The GTA
// ped and SA-MP active/wasted flags remain alive during this scripted death.
// Decode BEFORE any uint8 sync conversion, which wraps negative HP to 248 etc.
inline NativePlayerHealth DecodeNativePlayerHealth(float raw) {
    constexpr float offset = 8000000.0f;
    const bool encoded = std::isfinite(raw) && raw >= offset - 65536.0f && raw <= offset + 65536.0f;
    const float logical = encoded ? raw - offset : raw;
    return {std::isfinite(logical) && logical < 0.0f ? 0.0f : logical, encoded};
}

} // namespace keybinder
