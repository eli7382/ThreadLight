#pragma once

#include <algorithm>

namespace threadlight::visual {

constexpr float kDamageFeedbackDuration = 0.32f;

inline float damageImpactStrength(float secondsRemaining) {
    const float t = std::clamp(secondsRemaining / kDamageFeedbackDuration, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float damageRingProgress(float secondsRemaining) {
    return 1.0f - std::clamp(secondsRemaining / kDamageFeedbackDuration, 0.0f, 1.0f);
}

inline float damageRingRadius(float playerRadius, float secondsRemaining) {
    return playerRadius + 6.0f + 34.0f * damageRingProgress(secondsRemaining);
}

} // namespace threadlight::visual
