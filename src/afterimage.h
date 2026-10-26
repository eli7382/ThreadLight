#pragma once

#include "game_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace threadlight::afterimage {

constexpr std::size_t kPathPointCapacity = 32;
constexpr std::size_t kHitTargetCapacity = 128;
constexpr int kEchoTicks = 78;
constexpr float kHalfWidth = 8.0f;

struct Bounds {
    float left{}, right{}, top{}, bottom{};
};

// The path is the in-progress recording while recording is true, then the
// completed echo path until it expires or is cleared.
struct State {
    std::array<foundation::Point, kPathPointCapacity> path{};
    std::size_t count = 0;
    Bounds pathBounds{};
    bool pathBoundsValid = false;
    int remainingTicks = 0;
    bool recording = false;
    std::array<std::uint64_t, kHitTargetCapacity> hitTargetIds{};
    std::size_t hitTargetCount = 0;
    bool hitTargetOverflow = false;

    bool active() const { return remainingTicks > 0; }
};

inline bool finite(foundation::Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

inline bool samePoint(foundation::Point a, foundation::Point b) {
    return a.x == b.x && a.y == b.y;
}

inline bool calculatePathBounds(const State& state, Bounds& bounds) {
    if (state.count < 2 || state.count > kPathPointCapacity || !finite(state.path[0])) return false;
    bounds={state.path[0].x,state.path[0].x,state.path[0].y,state.path[0].y};
    for(std::size_t i=1;i<state.count;++i){
        if(!finite(state.path[i]))return false;
        bounds.left=std::min(bounds.left,state.path[i].x);
        bounds.right=std::max(bounds.right,state.path[i].x);
        bounds.top=std::min(bounds.top,state.path[i].y);
        bounds.bottom=std::max(bounds.bottom,state.path[i].y);
    }
    return true;
}

inline void clearContactClaims(State& state) {
    state.hitTargetIds.fill(0);
    state.hitTargetCount = 0;
    state.hitTargetOverflow = false;
}

inline void clear(State& state) {
    state.path.fill({});
    state.count = 0;
    state.pathBounds={};
    state.pathBoundsValid=false;
    state.remainingTicks = 0;
    state.recording = false;
    clearContactClaims(state);
}

inline void beginDash(State& state, foundation::Point start, bool combat, bool owned) {
    clear(state);
    if (!combat || !owned || !finite(start)) return;
    state.path[0] = start;
    state.count = 1;
    state.recording = true;
}

// Append only a successive simulation segment. A discontinuous previous point
// is rejected instead of drawing an invented chord across a wall or teleport.
inline void appendDash(State& state, foundation::Point previous,
                       foundation::Point current, bool combat) {
    if (!combat || !state.recording || state.count == 0 ||
        state.count > kPathPointCapacity || !finite(previous) || !finite(current)) return;
    const foundation::Point last = state.path[state.count - 1];
    if (!samePoint(previous, last) || samePoint(previous, current)) return;
    if (state.count == kPathPointCapacity) return;
    state.path[state.count++] = current;
}

inline void finishDash(State& state, bool combat, bool owned) {
    if (!combat || !owned || !state.recording || state.count > kPathPointCapacity) {
        clear(state);
        return;
    }

    state.recording = false;
    bool moved = false;
    for (std::size_t i = 1; i < state.count; ++i) {
        if (!samePoint(state.path[i - 1], state.path[i])) {
            moved = true;
            break;
        }
    }
    if (!moved) {
        clear(state);
        return;
    }
    state.pathBoundsValid=calculatePathBounds(state,state.pathBounds);
    state.remainingTicks = kEchoTicks;
    clearContactClaims(state);
}

// Call after resolving this update's contacts. Paused/non-combat updates do not
// consume echo time.
inline void advance(State& state, bool combat) {
    if (!combat || state.recording || state.remainingTicks <= 0) return;
    --state.remainingTicks;
    if (state.remainingTicks == 0) {
        state.path.fill({});
        state.count = 0;
        state.pathBounds={};
        state.pathBoundsValid=false;
        clearContactClaims(state);
    }
}

inline bool claimContact(State& state, std::uint64_t targetId,
                         foundation::Point previousTarget,
                         foundation::Point currentTarget, float targetRadius) {
    if (!state.active() || state.recording || state.count < 2 ||
        state.count > kPathPointCapacity || targetId == 0 ||
        !finite(previousTarget) || !finite(currentTarget) || !std::isfinite(targetRadius)) return false;

    for (std::size_t i = 0; i < state.hitTargetCount && i < kHitTargetCapacity; ++i) {
        if (state.hitTargetIds[i] == targetId) return false;
    }
    if (state.hitTargetOverflow || state.hitTargetCount >= kHitTargetCapacity) {
        state.hitTargetOverflow = true;
        return false;
    }

    const float radius = kHalfWidth + std::max(0.0f, targetRadius);
    const float radiusSquared = radius * radius;
    if(state.pathBoundsValid&&std::isfinite(radius)&&std::isfinite(radiusSquared)){
        const float scale=std::max({std::abs(state.pathBounds.left),std::abs(state.pathBounds.right),
                                    std::abs(state.pathBounds.top),std::abs(state.pathBounds.bottom),
                                    std::abs(previousTarget.x),std::abs(previousTarget.y),
                                    std::abs(currentTarget.x),std::abs(currentTarget.y),radius,1.f});
        if(scale<=1.e18f){
            const float gapX=std::max({state.pathBounds.left-std::max(previousTarget.x,currentTarget.x),
                                       std::min(previousTarget.x,currentTarget.x)-state.pathBounds.right,0.f});
            const float gapY=std::max({state.pathBounds.top-std::max(previousTarget.y,currentTarget.y),
                                       std::min(previousTarget.y,currentTarget.y)-state.pathBounds.bottom,0.f});
            const float reach=radius+8.f*std::numeric_limits<float>::epsilon()*scale;
            if(gapX>reach||gapY>reach)return false;
        }
    }
    bool touched = false;
    for (std::size_t i = 1; i < state.count; ++i) {
        if (foundation::segmentsDistanceSquared(previousTarget, currentTarget,
                                                state.path[i - 1], state.path[i]) <= radiusSquared) {
            touched = true;
            break;
        }
    }
    if (!touched) return false;

    state.hitTargetIds[state.hitTargetCount++] = targetId;
    return true;
}

// Damage is integer hundredths. Round the product once, to nearest, with
// positive half values rounded upward (for example 270 * 3/4 -> 203).
inline int damage(int baseCenti, bool boss = false) {
    if (baseCenti <= 0) return 0;
    const std::int64_t numerator = static_cast<std::int64_t>(baseCenti) * 3;
    const std::int64_t denominator = boss ? 8 : 4;
    return static_cast<int>((numerator + denominator / 2) / denominator);
}

inline bool finiteBounds(Bounds bounds) {
    return std::isfinite(bounds.left) && std::isfinite(bounds.right) &&
           std::isfinite(bounds.top) && std::isfinite(bounds.bottom);
}

inline foundation::Point remapPoint(foundation::Point point, Bounds from, Bounds to) {
    return {
        to.left + ((point.x - from.left) / (from.right - from.left)) * (to.right - to.left),
        to.top + ((point.y - from.top) / (from.bottom - from.top)) * (to.bottom - to.top)
    };
}

// Remapping changes coordinates only; it never advances recording or echo time.
inline void remap(State& state, Bounds from, Bounds to) {
    if (!finiteBounds(from) || !finiteBounds(to) ||
        from.left == from.right || from.top == from.bottom) return;
    const std::size_t pointCount = std::min(state.count, kPathPointCapacity);
    for (std::size_t i = 0; i < pointCount; ++i) {
        state.path[i] = remapPoint(state.path[i], from, to);
    }
    if(state.pathBoundsValid)state.pathBoundsValid=calculatePathBounds(state,state.pathBounds);
}

} // namespace threadlight::afterimage
