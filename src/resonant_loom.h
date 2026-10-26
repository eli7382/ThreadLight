#pragma once

#include "game_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace threadlight::resonant_loom {

constexpr int kHitsPerBurst = 5;
constexpr int kCooldownTicks = 240;
constexpr float kNeighborRadius = 180.0f;

struct State {
    int charges = 0;
    int cooldownTicks = 0;
};

enum class HitKind : std::uint8_t {
    OrdinarySilk,
    SignatureProc,
    RewardlessBud
};

struct Target {
    std::uint64_t id = 0;
    foundation::Point position{};
    int index = 0;
    int kind = 0;
};

using Candidate = Target;

struct Selection {
    std::array<Target, 3> items{};
    std::size_t count = 0;
};

inline void clear(State& state) {
    state = State{};
}

inline void tick(State& state, bool combat, bool owned) {
    if (!combat || !owned) return;
    state.charges = std::clamp(state.charges, 0, kHitsPerBurst - 1);
    state.cooldownTicks = std::max(0, state.cooldownTicks);
    if (state.cooldownTicks > 0) --state.cooldownTicks;
}

// Call once for a resolved hit. Passing the origin explicitly prevents
// signature damage or rewardless buds from feeding this ordinary-hit counter.
inline bool onHit(State& state, bool owned, bool actualHpDamage, HitKind kind) {
    if (!owned || !actualHpDamage || kind != HitKind::OrdinarySilk) return false;
    state.charges = std::clamp(state.charges, 0, kHitsPerBurst - 1);
    if (state.cooldownTicks > 0) return false;

    ++state.charges;
    if (state.charges < kHitsPerBurst) return false;

    state.charges = 0;
    state.cooldownTicks = kCooldownTicks;
    return true;
}

inline double distanceSquared(foundation::Point a, foundation::Point b) {
    if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
        !std::isfinite(b.x) || !std::isfinite(b.y)) return INFINITY;
    const double dx = static_cast<double>(a.x) - b.x;
    const double dy = static_cast<double>(a.y) - b.y;
    return dx * dx + dy * dy;
}

inline bool nearer(const Target& a, const Target& b, foundation::Point source) {
    const double aDistance = distanceSquared(a.position, source);
    const double bDistance = distanceSquared(b.position, source);
    return aDistance < bDistance || (aDistance == bDistance && a.id < b.id);
}

// The resolved hit source is always the first target. At most two distinct
// live candidates within 180 units are appended, ordered by distance then ID.
inline Selection selectTargets(Target source, std::span<const Candidate> candidates) {
    Selection result;
    if (source.id == 0 || !std::isfinite(source.position.x) ||
        !std::isfinite(source.position.y)) return result;
    result.items[result.count++] = source;

    const double radiusSquared = static_cast<double>(kNeighborRadius) * kNeighborRadius;
    for (const Candidate& candidate : candidates) {
        if (candidate.id == 0 || candidate.id == source.id ||
            !std::isfinite(candidate.position.x) || !std::isfinite(candidate.position.y) ||
            distanceSquared(candidate.position, source.position) > radiusSquared) continue;

        bool duplicate = false;
        for (std::size_t i = 1; i < result.count; ++i) {
            if (result.items[i].id == candidate.id) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        if (result.count < result.items.size()) {
            result.items[result.count++] = candidate;
        } else if (nearer(candidate, result.items.back(), source.position)) {
            result.items.back() = candidate;
        } else {
            continue;
        }

        for (std::size_t i = result.count - 1; i > 1 &&
             nearer(result.items[i], result.items[i - 1], source.position); --i) {
            std::swap(result.items[i], result.items[i - 1]);
        }
    }
    return result;
}

inline int damage(int baseCenti, bool boss = false) {
    if (baseCenti <= 0) return 0;
    const std::int64_t numerator = 3;
    const std::int64_t denominator = boss ? 10 : 5;
    const std::int64_t scaled = static_cast<std::int64_t>(baseCenti) * numerator;
    return static_cast<int>((scaled + denominator / 2) / denominator);
}

} // namespace threadlight::resonant_loom
