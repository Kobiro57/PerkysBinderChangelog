#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace keybinder {

class AnimationHelper final {
public:
    enum class AnimationType {
        Hover,
        ToggleSlide,
        TabSelection,
        ColorFade
    };

    struct AnimationState {
        float durationSeconds = 0.0f;
        float elapsedSeconds = 0.0f;
        float progress = 0.0f;
        bool active = false;
        bool complete = false;
    };

    static constexpr float HoverDurationMinSeconds = 0.08f;
    static constexpr float HoverDurationMaxSeconds = 0.12f;
    static constexpr float ToggleSlideDurationMinSeconds = 0.12f;
    static constexpr float ToggleSlideDurationMaxSeconds = 0.16f;
    static constexpr float TabSelectionDurationMinSeconds = 0.14f;
    static constexpr float TabSelectionDurationMaxSeconds = 0.18f;
    static constexpr float ColorFadeDurationMinSeconds = 0.08f;
    static constexpr float ColorFadeDurationMaxSeconds = 0.12f;

    static float Lerp(float start, float end, float t) {
        return start + ((end - start) * Clamp01(t));
    }

    static float EaseInOut(float t) {
        t = Clamp01(t);
        if (t < 0.5f) {
            return 4.0f * t * t * t;
        }

        const float inv = (-2.0f * t) + 2.0f;
        return 1.0f - ((inv * inv * inv) / 2.0f);
    }

    static float SmoothStep(float start, float end, float t) {
        t = Clamp01(t);
        const float eased = t * t * (3.0f - (2.0f * t));
        return Lerp(start, end, eased);
    }

    static float GetDefaultDuration(AnimationType type) {
        switch (type) {
        case AnimationType::Hover:
        case AnimationType::ColorFade:
            return 0.10f;
        case AnimationType::ToggleSlide:
            return 0.14f;
        case AnimationType::TabSelection:
            return 0.16f;
        default:
            return 0.10f;
        }
    }

    static void StartAnimation(const std::string& id, float durationSeconds) {
        AnimationState& state = states_[id];
        state.durationSeconds = (std::max)(durationSeconds, 0.0f);
        state.elapsedSeconds = 0.0f;
        state.progress = state.durationSeconds > 0.0f ? 0.0f : 1.0f;
        state.active = state.durationSeconds > 0.0f;
        state.complete = state.durationSeconds <= 0.0f;
    }

    static void Update(float deltaTimeSeconds) {
        if (deltaTimeSeconds <= 0.0f || states_.empty()) {
            return;
        }

        for (auto& entry : states_) {
            AnimationState& state = entry.second;
            if (!state.active || state.complete) {
                continue;
            }

            state.elapsedSeconds += deltaTimeSeconds;
            if (state.durationSeconds <= 0.0f) {
                state.progress = 1.0f;
                state.active = false;
                state.complete = true;
                continue;
            }

            state.progress = Clamp01(state.elapsedSeconds / state.durationSeconds);
            if (state.progress >= 1.0f) {
                state.progress = 1.0f;
                state.active = false;
                state.complete = true;
            }
        }
    }

    static float GetProgress(const std::string& id) {
        const AnimationState* state = FindState(id);
        return state ? state->progress : 0.0f;
    }

    static bool IsComplete(const std::string& id) {
        const AnimationState* state = FindState(id);
        return state ? state->complete : false;
    }

    static const AnimationState* GetState(const std::string& id) {
        return FindState(id);
    }

    static void Clear() {
        states_.clear();
    }

private:
    static float Clamp01(float value) {
        return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    }

    static const AnimationState* FindState(const std::string& id) {
        const auto it = states_.find(id);
        return it == states_.end() ? nullptr : &it->second;
    }

    inline static std::unordered_map<std::string, AnimationState> states_{};
};

} // namespace keybinder
