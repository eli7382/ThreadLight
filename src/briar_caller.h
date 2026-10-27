#pragma once

#include "game_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace threadlight::briar_caller {

using Point = foundation::Point;

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

inline int fusedMembers(const State& first, const State& second) {
    return std::max(1, first.members) + std::max(1, second.members);
}

inline int fusedHealth(int firstHp, int firstMaxHp, const State& first,
                       int secondHp, int secondMaxHp, const State& second,
                       int combinedMaxHp) {
    if (combinedMaxHp <= 0) return 0;
    if (firstMaxHp <= 0 || secondMaxHp <= 0) return combinedMaxHp;

    const int members = fusedMembers(first, second);
    const long long denominator = static_cast<long long>(firstMaxHp) *
                                  secondMaxHp * members;
    const long long numerator = static_cast<long long>(firstHp) * secondMaxHp *
                                    std::max(1, first.members) +
                                static_cast<long long>(secondHp) * firstMaxHp *
                                    std::max(1, second.members);
    if (denominator <= 0) return combinedMaxHp;
    return std::clamp(static_cast<int>((static_cast<long long>(combinedMaxHp) *
                                        numerator + denominator / 2) / denominator),
                      1, combinedMaxHp);
}

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

inline float movementSpeed(float baseSpeed, const State& state) {
    return baseSpeed * movementScale(state.members);
}

template <typename Range, typename Actor, typename IsAlive, typename IsCaller, typename Position>
inline Point movementTarget(Point current, Point player, const Range& actors,
                            const Actor* self, IsAlive isAlive, IsCaller isCaller,
                            Position position) {
    Point target = player;
    float nearestSquared = std::numeric_limits<float>::max();
    for (const auto& actor : actors) {
        if (&actor == self || !isAlive(actor) || !isCaller(actor)) continue;
        const Point candidate = position(actor);
        const float dx = candidate.x - current.x;
        const float dy = candidate.y - current.y;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared < nearestSquared) {
            nearestSquared = distanceSquared;
            target = candidate;
        }
    }
    return target;
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
