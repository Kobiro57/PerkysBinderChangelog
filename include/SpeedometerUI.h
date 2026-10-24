#pragma once

#include "SpeedometerManager.h"
#include "imgui-1.92.7/imgui.h"

namespace keybinder {

class SpeedometerUI {
public:
    void Render(const speedometer::SpeedometerState& state, speedometer::SpeedometerStyle style = speedometer::SpeedometerStyle::AnalogArc);

    static ImVec4 GetHealthColor(float healthPct);
    static ImVec4 GetFuelColor(float fuelPct);

private:
    void RenderTachometerArc(const speedometer::SpeedometerState& state);
    void RenderAnalogArc(const speedometer::SpeedometerState& state);
    void RenderMinimalDigital(const speedometer::SpeedometerState& state);
    void RenderArcGauge(const speedometer::SpeedometerState& state, bool analogVariant);

    float displayedArcPct_ = 0.0f;
    float displayedMinimalFuelPct_ = 0.0f;
    float displayedMinimalHealthPct_ = 0.0f;
    speedometer::SpeedometerStyle lastStyle_ = speedometer::SpeedometerStyle::AnalogArc;
};

} // namespace keybinder
