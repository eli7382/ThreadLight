#pragma once

#include "game_geometry.h"
#include "game_timing.h"

#include <algorithm>
#include <cmath>

namespace threadlight::skitter {

using Point = foundation::Point;

struct Bounds {
    float left{}, right{}, top{}, bottom{};
};

enum class Phase { Flank, Windup, Burst, Recover };

struct State {
    Phase phase = Phase::Flank;
    int ticksRemaining = 255;
    Point burstStart{};
    Point burstEnd{};
    Point lockedDirection{};
    int side = 1;
};

struct TickResult {
    bool telegraphStarted{};
    bool burstStarted{};
    bool recovered{};
};

constexpr int kFlankTicks = 255;
constexpr int kWindupTicks = 84;
constexpr int kBurstTicks = 39;
constexpr int kRecoverTicks = 78;
constexpr float kFlankSpeed = 115.0f;
constexpr float kBurstSpeed = 260.0f;
constexpr float kFlankOffset = 90.0f;

namespace detail {

inline Bounds ordered(Bounds bounds) {
    if (bounds.left > bounds.right) std::swap(bounds.left, bounds.right);
    if (bounds.top > bounds.bottom) std::swap(bounds.top, bounds.bottom);
    return bounds;
}

inline Point clamp(Point point, Bounds bounds) {
    return {std::clamp(point.x, bounds.left, bounds.right),
            std::clamp(point.y, bounds.top, bounds.bottom)};
}

inline bool finite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

inline float lengthSquared(Point point) {
    return point.x * point.x + point.y * point.y;
}

inline Point unitOr(Point point, Point fallback) {
    if (!finite(point)) point = {};
    const float squared = lengthSquared(point);
    if (squared > 1.0e-8f && std::isfinite(squared)) {
        const float inverseLength = 1.0f / std::sqrt(squared);
        return {point.x * inverseLength, point.y * inverseLength};
    }

    if (!finite(fallback)) return {1.0f, 0.0f};
    const float fallbackSquared = lengthSquared(fallback);
    if (!(fallbackSquared > 1.0e-8f) || !std::isfinite(fallbackSquared))
        return {1.0f, 0.0f};
    const float inverseLength = 1.0f / std::sqrt(fallbackSquared);
    return {fallback.x * inverseLength, fallback.y * inverseLength};
}

inline Point flankTarget(Point position, Point player, Point playerMotion,
                         int side, Bounds bounds) {
    const Point fromPlayer{position.x - player.x, position.y - player.y};
    const Point heading = unitOr(playerMotion, unitOr(fromPlayer, {1.0f, 0.0f}));
    const int sideSign = side < 0 ? -1 : 1;
    const Point perpendicular{-heading.y, heading.x};
    return clamp({player.x + perpendicular.x * kFlankOffset * sideSign,
                  player.y + perpendicular.y * kFlankOffset * sideSign}, bounds);
}

inline void moveToward(Point& position, Point& velocity, Point target,
                       float speed, Bounds bounds) {
    constexpr float stepSeconds = static_cast<float>(timing::kFixedStepSeconds);
    const Point delta{target.x - position.x, target.y - position.y};
    const float squared = lengthSquared(delta);
    const float distance = squared > 0.0f && std::isfinite(squared)
        ? std::sqrt(squared) : 0.0f;
    const float maximumStep = speed * stepSeconds;
    const float scale = distance > maximumStep && distance > 0.0f
        ? maximumStep / distance : 1.0f;
    const Point previous = position;
    position = clamp({position.x + delta.x * scale, position.y + delta.y * scale}, bounds);
    velocity = {(position.x - previous.x) / stepSeconds,
                (position.y - previous.y) / stepSeconds};
}

inline float rayDistanceToBounds(Point start, Point direction, Bounds bounds,
                                 float maximumDistance) {
    float distance = maximumDistance;
    if (direction.x > 1.0e-6f)
        distance = std::min(distance, (bounds.right - start.x) / direction.x);
    else if (direction.x < -1.0e-6f)
        distance = std::min(distance, (bounds.left - start.x) / direction.x);
    if (direction.y > 1.0e-6f)
        distance = std::min(distance, (bounds.bottom - start.y) / direction.y);
    else if (direction.y < -1.0e-6f)
        distance = std::min(distance, (bounds.top - start.y) / direction.y);
    return std::clamp(distance, 0.0f, maximumDistance);
}

inline void startWindup(State& state, Point position, Point player,
                        Point playerMotion, Bounds bounds) {
    const Point toPlayer{player.x - position.x, player.y - position.y};
    state.lockedDirection = unitOr(toPlayer, unitOr(playerMotion, {1.0f, 0.0f}));
    state.burstStart = position;

    constexpr float stepSeconds = static_cast<float>(timing::kFixedStepSeconds);
    constexpr float maximumDistance = kBurstSpeed * kBurstTicks * stepSeconds;
    const float laneDistance = rayDistanceToBounds(
        state.burstStart, state.lockedDirection, bounds, maximumDistance);
    state.burstEnd = clamp({state.burstStart.x + state.lockedDirection.x * laneDistance,
                            state.burstStart.y + state.lockedDirection.y * laneDistance}, bounds);
    state.phase = Phase::Windup;
    state.ticksRemaining = kWindupTicks;
}

} // namespace detail

inline bool committed(const State& state) {
    return state.phase == Phase::Windup || state.phase == Phase::Burst;
}

// Resize is a presentation-space remap, not a simulation update. Use enemy-center
// bounds for all three points so clipping cannot bend the locked lane. Preserve
// phase/progress and update direction; no extra attack or reward event is emitted.
inline void remapBounds(State& state, Point& position, Point& velocity, Bounds from, Bounds to) {
    from = detail::ordered(from); to = detail::ordered(to);
    auto remap = [&](Point p) {
        const float x = from.right > from.left ? (p.x - from.left) / (from.right - from.left) : .5f;
        const float y = from.bottom > from.top ? (p.y - from.top) / (from.bottom - from.top) : .5f;
        return Point{to.left + std::clamp(x, 0.f, 1.f) * (to.right - to.left),
                     to.top + std::clamp(y, 0.f, 1.f) * (to.bottom - to.top)};
    };
    position = remap(position); velocity = {};
    if (committed(state)) {
        state.burstStart = remap(state.burstStart); state.burstEnd = remap(state.burstEnd);
        state.lockedDirection = detail::unitOr({state.burstEnd.x - state.burstStart.x,
                                               state.burstEnd.y - state.burstStart.y}, state.lockedDirection);
    }
}

inline TickResult tick(State& state, Point& position, Point& velocity,
                       Point player, Point playerMotion, Bounds bounds,
                       bool canCommit = true) {
    TickResult result;
    bounds = detail::ordered(bounds);
    const Point boundedPosition = detail::clamp(position, bounds);
    if (boundedPosition.x != position.x || boundedPosition.y != position.y)
        velocity = {};
    position = boundedPosition;

    constexpr float stepSeconds = static_cast<float>(timing::kFixedStepSeconds);
    switch (state.phase) {
    case Phase::Flank:
        detail::moveToward(position, velocity,
                           detail::flankTarget(position, player, playerMotion,
                                              state.side, bounds),
                           kFlankSpeed, bounds);
        if (state.ticksRemaining > 1) {
            --state.ticksRemaining;
        } else if (canCommit) {
            detail::startWindup(state, position, player, playerMotion, bounds);
            result.telegraphStarted = true;
        } else {
            state.ticksRemaining = 1;
        }
        break;

    case Phase::Windup:
        velocity = {};
        if (state.ticksRemaining > 1) {
            --state.ticksRemaining;
        } else {
            state.phase = Phase::Burst;
            state.ticksRemaining = kBurstTicks;
            result.burstStarted = true;
        }
        break;

    case Phase::Burst: {
        if (state.ticksRemaining > kBurstTicks)
            state.ticksRemaining = kBurstTicks;
        if (state.ticksRemaining < 1) state.ticksRemaining = 1;
        const int updateIndex = kBurstTicks - state.ticksRemaining + 1;
        const float progress = static_cast<float>(updateIndex) / kBurstTicks;
        const Point previous = position;
        position = detail::clamp({
            state.burstStart.x + (state.burstEnd.x - state.burstStart.x) * progress,
            state.burstStart.y + (state.burstEnd.y - state.burstStart.y) * progress}, bounds);
        velocity = {(position.x - previous.x) / stepSeconds,
                    (position.y - previous.y) / stepSeconds};
        --state.ticksRemaining;
        if (state.ticksRemaining == 0) {
            state.phase = Phase::Recover;
            state.ticksRemaining = kRecoverTicks;
        }
        break;
    }

    case Phase::Recover:
        velocity = {};
        if (state.ticksRemaining > 1) {
            --state.ticksRemaining;
        } else {
            state.phase = Phase::Flank;
            state.ticksRemaining = kFlankTicks;
            result.recovered = true;
        }
        break;
    }

    return result;
}

} // namespace threadlight::skitter
