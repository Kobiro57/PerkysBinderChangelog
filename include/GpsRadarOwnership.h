#pragma once
#include "GpsManager.h"
#include <cstdint>

namespace keybinder {
struct GpsRadarTraceSnapshot {
    bool inUse = false;
    std::uint16_t generation = 0;
    int type = 0;
    GpsPosition position;
};
// A recycled slot belongs to the server/game. Never clear it with an old handle.
inline bool OwnsGpsRadarTrace(int handle, const GpsPosition& target, const GpsRadarTraceSnapshot& trace) {
    const auto bits = static_cast<std::uint32_t>(handle);
    return handle != -1 && (bits & 0xffff) < 175 && trace.inUse
        && trace.generation == (bits >> 16) && trace.type == 4
        && trace.position.x == target.x && trace.position.y == target.y && trace.position.z == target.z - 0.9f;
}
}
