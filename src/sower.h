#pragma once

#include "area_hazards.h"
#include "game_timing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace threadlight::sower {

using Point = foundation::Point;
using Bounds = area_hazards::Bounds;
using Corridor = area_hazards::Corridor;

constexpr int kWarningTicks = 108;
constexpr int kSoftAimTicks = 30;
constexpr int kLockedAimTicks = 78;
constexpr float kEnemyRadius = 16.0f;
constexpr float kStandOffDistance = 240.0f;
constexpr float kStandOffTolerance = 12.0f;
constexpr float kPredictionSeconds = 0.20f;
constexpr float kPredictionLimit = 48.0f;

enum class Phase { Seek, Windup };

struct Config {
    int chapter = 4;
    int castIntervalTicks = 540;
    int activeTicks = 336;
    int damage = 1800;
    float seekSpeed = 64.0f;
    float radius = 45.0f;
};

inline Config configForChapter(int chapter) {
    if (chapter >= 5) return {5, 480, 384, 2000, 76.0f, 55.0f};
    return {4, 540, 336, 1800, 64.0f, 45.0f};
}

struct State {
    Phase phase = Phase::Seek;
    int remaining = 432;
    std::uint64_t ownerId = 0;
    area_hazards::Ticket ticket = 0;
    Config config{};
    Point aimPoint{};
    Point aimDirection{1.0f, 0.0f};
    bool aimLocked = false;
};

struct TickEvents {
    bool warningStarted = false;
    bool aimLocked = false;
    bool shotReady = false;
    bool commitDenied = false;
    area_hazards::Ticket ticket = 0;
};

namespace detail {

inline bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
inline float lengthSquared(Point p) { return p.x * p.x + p.y * p.y; }
inline float length(Point p) { return std::sqrt(lengthSquared(p)); }
inline Point add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
inline Point sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
inline Point scale(Point p, float amount) { return {p.x * amount, p.y * amount}; }

inline Point unitOr(Point p, Point fallback = {1.0f, 0.0f}) {
    float squared = lengthSquared(p);
    if (finite(p) && squared > 1.0e-8f && std::isfinite(squared))
        return scale(p, 1.0f / std::sqrt(squared));
    squared = lengthSquared(fallback);
    if (!finite(fallback) || !(squared > 1.0e-8f) || !std::isfinite(squared))
        return {1.0f, 0.0f};
    return scale(fallback, 1.0f / std::sqrt(squared));
}

inline Bounds ordered(Bounds bounds) {
    if (bounds.left > bounds.right) std::swap(bounds.left, bounds.right);
    if (bounds.top > bounds.bottom) std::swap(bounds.top, bounds.bottom);
    return bounds;
}

inline Point mapPoint(Point p, Bounds from, Bounds to) {
    from = ordered(from); to = ordered(to);
    const float x = from.right > from.left ? (p.x - from.left) / (from.right - from.left) : 0.5f;
    const float y = from.bottom > from.top ? (p.y - from.top) / (from.bottom - from.top) : 0.5f;
    return {to.left + x * (to.right - to.left), to.top + y * (to.bottom - to.top)};
}

inline Point clampWithInset(Point point, Bounds bounds, float inset) {
    bounds = ordered(bounds);
    float left = bounds.left + inset, right = bounds.right - inset;
    float top = bounds.top + inset, bottom = bounds.bottom - inset;
    if (left > right) left = right = (bounds.left + bounds.right) * 0.5f;
    if (top > bottom) top = bottom = (bounds.top + bounds.bottom) * 0.5f;
    return {std::clamp(point.x, left, right), std::clamp(point.y, top, bottom)};
}

inline Point predict(Point player, Point velocity) {
    if (!finite(player)) return {};
    if (!finite(velocity)) velocity = {};
    Point offset = scale(velocity, kPredictionSeconds);
    const float distance = length(offset);
    if (distance > kPredictionLimit && distance > 0.0f)
        offset = scale(offset, kPredictionLimit / distance);
    return add(player, offset);
}

inline void moveTowardStandOff(Point& position, Point& velocity, Point player,
                               float speed, Bounds bounds) {
    constexpr float stepSeconds = static_cast<float>(timing::kFixedStepSeconds);
    position = clampWithInset(position, bounds, kEnemyRadius);
    if (!finite(player)) { velocity = {}; return; }
    const Point delta = sub(player, position);
    const float distance = length(delta);
    const Point direction = unitOr(delta);
    float travel = 0.0f;
    if (distance > kStandOffDistance + kStandOffTolerance)
        travel = std::min(speed * stepSeconds, distance - kStandOffDistance);
    else if (distance < kStandOffDistance - kStandOffTolerance)
        travel = -std::min(speed * stepSeconds, kStandOffDistance - distance);
    if (travel == 0.0f) { velocity = {}; return; }
    const Point previous = position;
    position = clampWithInset(add(position, scale(direction, travel)), bounds, kEnemyRadius);
    velocity = scale(sub(position, previous), 1.0f / stepSeconds);
}

inline int seekTicks(const Config& config) {
    return std::max(1, config.castIntervalTicks - kWarningTicks);
}

inline Config sanitize(Config config) {
    if (config.chapter >= 5) return configForChapter(5);
    if (config.chapter <= 0) config.chapter = 4;
    if (config.castIntervalTicks <= kWarningTicks) config.castIntervalTicks = 540;
    if (config.activeTicks <= 0) config.activeTicks = 336;
    if (config.damage <= 0) config.damage = 1800;
    if (!std::isfinite(config.seekSpeed) || config.seekSpeed <= 0.0f) config.seekSpeed = 64.0f;
    if (!std::isfinite(config.radius) || config.radius <= 0.0f) config.radius = 45.0f;
    return config;
}

inline void updateAim(State& state, Point position, Point player, Point playerMotion) {
    const Point predicted = predict(player, playerMotion);
    state.aimPoint = predicted;
    state.aimDirection = unitOr(sub(predicted, position), state.aimDirection);
}

} // namespace detail

inline void start(State& state, std::uint64_t ownerId, Config config = {}) {
    state = State{};
    state.ownerId = ownerId;
    state.config = detail::sanitize(config);
    state.remaining = detail::seekTicks(state.config);
}

inline bool wantsCommit(const State& state) {
    return state.phase == Phase::Seek && state.remaining <= 1;
}

inline bool committed(const State& state) { return state.phase == Phase::Windup; }

// Call before the hazard pool tick in the same combat update. A successful
// commit reserves its warning patch immediately; the pool skips that creation
// update, and both clocks then run for the complete 108 warning ticks.
inline TickEvents tick(State& state, Point& position, Point& velocity,
                      Point player, Point playerMotion, Bounds arena,
                      area_hazards::Pool& pool, Corridor protectedCorridor = {},
                      float externalOccupiedArea = 0.0f, bool canCommit = true,
                      bool active = true, float movementMultiplier = 1.0f) {
    TickEvents events;
    if (!active || !state.ownerId) return events;
    arena = detail::ordered(arena);

    if (state.phase == Phase::Seek) {
        position = detail::clampWithInset(position, arena, kEnemyRadius);
        if (wantsCommit(state)) {
            if (canCommit) {
                detail::updateAim(state, position, player, playerMotion);
                const Point center = detail::clampWithInset(state.aimPoint, arena,
                                                             state.config.radius);
                area_hazards::PatchSpec spec;
                spec.shape = area_hazards::Shape::Disk;
                spec.ownerId = state.ownerId;
                spec.center = center;
                spec.radius = state.config.radius;
                spec.warningTicks = kWarningTicks;
                spec.activeTicks = state.config.activeTicks;
                spec.damage = state.config.damage;
                const auto ticket = pool.add(spec, arena, protectedCorridor,
                                             externalOccupiedArea);
                if (ticket) {
                    state.ticket = ticket;
                    state.phase = Phase::Windup;
                    state.remaining = kWarningTicks;
                    state.aimPoint = center;
                    state.aimDirection = detail::unitOr(detail::sub(center, position),
                                                        state.aimDirection);
                    state.aimLocked = false;
                    velocity = {};
                    events.warningStarted = true;
                    events.ticket = ticket;
                    return events;
                }
            }
            events.commitDenied = true;
            state.remaining = 1;
            detail::moveTowardStandOff(position, velocity, player,
                                       state.config.seekSpeed * std::clamp(movementMultiplier, 0.0f, 4.0f), arena);
            return events;
        }

        detail::moveTowardStandOff(position, velocity, player,
                                   state.config.seekSpeed * std::clamp(movementMultiplier, 0.0f, 4.0f), arena);
        if (state.remaining > 1) --state.remaining;
        else state.remaining = 1;
        return events;
    }

    velocity = {};
    if (state.remaining > kWarningTicks) state.remaining = kWarningTicks;
    if (state.remaining < 1) state.remaining = 1;
    if (!state.aimLocked) {
        const int elapsed = kWarningTicks - state.remaining + 1;
        if (elapsed <= kSoftAimTicks) {
            const Point predicted = detail::clampWithInset(
                detail::predict(player, playerMotion), arena, state.config.radius);
            if (pool.retargetWarning(state.ticket, predicted, arena,
                                     protectedCorridor, externalOccupiedArea)) {
                state.aimPoint = predicted;
                state.aimDirection = detail::unitOr(detail::sub(predicted, position),
                                                    state.aimDirection);
            }
            if (elapsed == kSoftAimTicks) {
                state.aimLocked = true;
                events.aimLocked = true;
            }
        }
    }

    if (state.remaining > 1) {
        --state.remaining;
    } else {
        events.shotReady = true;
        events.ticket = state.ticket;
        state.phase = Phase::Seek;
        state.remaining = detail::seekTicks(state.config);
        state.ticket = 0;
        state.aimLocked = false;
    }
    return events;
}

// Remap the stored aim with its warning geometry via Pool::remap separately;
// this helper preserves all Sower clocks and never retargets from the player.
inline void remap(State& state, Point& position, Point& velocity,
                  Bounds from, Bounds to) {
    position = detail::mapPoint(position, from, to);
    state.aimPoint = detail::mapPoint(state.aimPoint, from, to);
    if (state.aimLocked)
        state.aimDirection = detail::unitOr({state.aimPoint.x - position.x,
                                             state.aimPoint.y - position.y},
                                            state.aimDirection);
    else
        state.aimDirection = detail::unitOr(state.aimDirection, {1.0f, 0.0f});
    velocity = {};
}

} // namespace threadlight::sower
