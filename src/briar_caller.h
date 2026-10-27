#pragma once

#include "game_geometry.h"

#include <algorithm>
#include <cmath>

namespace threadlight::briar_caller {

constexpr float kBaseAuraRadius = 150.0f;
constexpr int kBaseSpeedBoostPercent = 18;
constexpr int kBaseBoostDurationTicks = 240; // 2 seconds at 120 Hz.

struct State {
    int members = 1;
};

struct SpeedBoost {
    int remainingTicks = 0;
    int percent = 0;
};

inline int improvementPercent(int members) {
    switch (std::max(1, members)) {
    case 1: return 0;
    case 2: return 20;
    case 3: return 35;
    case 4: return 50;
    default: return 100;
    }
}

inline int scale(int base, int members) {
    const long long numerator = static_cast<long long>(std::max(0, base)) *
                                (100 + improvementPercent(members));
    return static_cast<int>((numerator + 50) / 100);
}

inline float scale(float base, int members) {
    return base * (100 + improvementPercent(members)) / 100.0f;
}

inline float movementScale(int members) {
    const float slowdown = 0.07f * static_cast<float>(std::max(1, members) - 1);
    return std::max(0.0f, 1.0f - slowdown);
}

inline float auraRadius(const State& state) {
    return scale(kBaseAuraRadius, state.members);
}

inline int speedBoostPercent(const State& state) {
    return scale(kBaseSpeedBoostPercent, state.members);
}

inline int boostDurationTicks(const State& state) {
    return scale(kBaseBoostDurationTicks, state.members);
}

inline bool insideAura(foundation::Point center, foundation::Point target,
                       const State& state) {
    const float dx = center.x - target.x;
    const float dy = center.y - target.y;
    const float radius = auraRadius(state);
    return dx * dx + dy * dy <= radius * radius;
}

inline bool readyToFuse(foundation::Point first, const State& firstState,
                        foundation::Point second, const State& secondState) {
    const float dx = first.x - second.x;
    const float dy = first.y - second.y;
    const float radius = std::min(auraRadius(firstState), auraRadius(secondState));
    return dx * dx + dy * dy <= radius * radius;
}

inline void tick(SpeedBoost& boost) {
    if (boost.remainingTicks > 0) --boost.remainingTicks;
    if (boost.remainingTicks == 0) boost.percent = 0;
}

inline void applyAura(SpeedBoost& boost, const State& source) {
    boost.percent = speedBoostPercent(source);
    boost.remainingTicks = boostDurationTicks(source);
}

inline float boostedSpeed(float baseSpeed, const SpeedBoost& boost) {
    if (boost.remainingTicks <= 0 || boost.percent <= 0) return baseSpeed;
    return baseSpeed * (100.0f + boost.percent) / 100.0f;
}

} // namespace threadlight::briar_caller
