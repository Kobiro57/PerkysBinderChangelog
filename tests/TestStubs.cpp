// Stub implementations for test build
// These replace external dependencies not available in test environment

#include <cstddef>
#include <functional>
#include <atomic>

#include "../include/plugin-sdk/safetyhook/safetyhook.hpp"

class CSprite2d;

class CHud {
public:
    static bool& bScriptDontDisplayAreaName;
    static bool& bScriptDontDisplayVehicleName;
    static bool& bScriptForceDisplayWithCounters;
    static bool& bScriptDontDisplayRadar;
    static bool& bDrawClock;
    static bool& m_bDraw3dMarkers;
    static bool& m_Wants_To_Draw_Hud;
    static bool& bDrawingVitalStats;
    static CSprite2d* Sprites;
};

namespace safetyhook {

// Stub implementation of execute_while_frozen for testing
// The real implementation is in the safetyhook pre-built library
void execute_while_frozen(
    const std::function<void()>& fn,
    const std::function<void(unsigned int, void*, void*)>&
) {
    // In test environment, just execute directly without frozen context
    fn();
}

} // namespace safetyhook

namespace safetyhook {

Allocation::~Allocation() = default;

void Allocation::free() {}

InlineHook::~InlineHook() = default;

void InlineHook::reset() {}

} // namespace safetyhook

// Stub for GetD3DDevice - returns nullptr in test environment
void* GetD3DDevice() {
    return nullptr;
}

// Global shutdown phase for crash diagnostics (used by ImGuiOverlay)
std::atomic<int> g_shutdown_phase{0};

// Minimal CHud visibility state used by interface/editor tests
namespace {

bool g_hudDontDisplayAreaName = false;
bool g_hudDontDisplayVehicleName = false;
bool g_hudForceDisplayWithCounters = false;
bool g_hudDontDisplayRadar = false;
bool g_hudDrawClock = false;
bool g_hudDraw3dMarkers = false;
bool g_hudWantsToDrawHud = true;
bool g_hudDrawingVitalStats = false;

} // namespace

bool& CHud::bScriptDontDisplayAreaName = g_hudDontDisplayAreaName;
bool& CHud::bScriptDontDisplayVehicleName = g_hudDontDisplayVehicleName;
bool& CHud::bScriptForceDisplayWithCounters = g_hudForceDisplayWithCounters;
bool& CHud::bScriptDontDisplayRadar = g_hudDontDisplayRadar;
bool& CHud::bDrawClock = g_hudDrawClock;
bool& CHud::m_bDraw3dMarkers = g_hudDraw3dMarkers;
bool& CHud::m_Wants_To_Draw_Hud = g_hudWantsToDrawHud;
bool& CHud::bDrawingVitalStats = g_hudDrawingVitalStats;
CSprite2d* CHud::Sprites = nullptr;
