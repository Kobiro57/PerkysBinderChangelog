#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace keybinder {
struct StaminaReading {
    bool available = false;
    bool unlimited = false;
    float energy = 0.0f;
    float maximum = 0.0f;
};

struct StaminaDisplay {
    bool available = false;
    float fraction = 0.0f;
    int percent = 0;
    std::string status = "Nicht verf\xC3\xBC" "gbar";
};

class StaminaGauge {
public:
    void Reset() { initialized_ = false; idleSeconds_ = 0.0f; trend_ = 0; }
    StaminaDisplay Observe(const StaminaReading& sample, float elapsed) {
        StaminaDisplay result;
        if (!sample.available || !std::isfinite(sample.energy) || !std::isfinite(sample.maximum)
            || sample.maximum <= 0.0f || sample.maximum > 1000000.0f) {
            Reset(); return result;
        }
        result.available = true;
        // GTA HandleSprintEnergy consumes the native reserve down to -150.
        // The upper limit comes from GetFatAndMuscleModifier(TIME_CAN_RUN).
        result.fraction = sample.unlimited ? 1.0f
            : std::clamp((sample.energy + 150.0f) / (sample.maximum + 150.0f), 0.0f, 1.0f);
        result.percent = static_cast<int>(std::round(result.fraction * 100.0f));
        if (initialized_ && std::fabs(sample.energy - previous_) > 0.001f) {
            trend_ = sample.energy > previous_ ? 1 : -1;
            idleSeconds_ = 0.0f;
        } else if (std::isfinite(elapsed) && elapsed > 0.0f) {
            idleSeconds_ += elapsed;
            if (idleSeconds_ > 0.65f) trend_ = 0;
        }
        previous_ = sample.energy;
        initialized_ = true;
        if (sample.unlimited) result.status = "Unbegrenzt";
        else if (sample.energy >= sample.maximum - 0.01f) result.status = "Voll";
        else if (trend_ > 0) result.status = "Erholt sich";
        else if (sample.energy <= -150.0f) result.status = "Ersch\xC3\xB6pft";
        else if (trend_ < 0) result.status = "Verbrauch";
        else result.status = "Pause";
        return result;
    }
private:
    bool initialized_ = false;
    float previous_ = 0.0f;
    float idleSeconds_ = 0.0f;
    int trend_ = 0;
};
} // namespace keybinder
