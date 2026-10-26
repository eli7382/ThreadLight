#pragma once

#include "game_geometry.h"
#include "game_timing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace threadlight::ranged {

using Point = foundation::Point;

struct Bounds {
    float left{}, right{}, top{}, bottom{};
};

enum class Phase { Seek, Windup };

struct State {
    Phase phase = Phase::Seek;
    int remaining = 324;
    Point aimPoint{};
    Point aimDirection{1.0f, 0.0f};
    bool aimLocked = false;
};

struct TickEvents {
    bool warningStarted{};
    bool aimLocked{};
    bool shotReady{};
};

constexpr int kSeekTicks = 324;
constexpr int kWindupTicks = 108;
constexpr int kSoftAimTicks = 30;
constexpr float kSeekSpeed = 72.0f;
constexpr float kProjectileSpeed = 180.0f;
constexpr float kProjectileRadius = 5.0f;
constexpr float kMuzzleOffset = 20.0f;
constexpr int kProjectileLifetimeTicks = 360;
constexpr int kProjectileDamage = 1400;
constexpr std::size_t kProjectileCapacity = 64;

namespace detail {

inline Bounds ordered(Bounds bounds) {
    if (bounds.left > bounds.right) std::swap(bounds.left, bounds.right);
    if (bounds.top > bounds.bottom) std::swap(bounds.top, bounds.bottom);
    return bounds;
}

inline bool finite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

inline float lengthSquared(Point point) {
    return point.x * point.x + point.y * point.y;
}

inline Point unitOr(Point direction, Point fallback = {1.0f, 0.0f}) {
    if (!finite(direction)) direction = {};
    float squared = lengthSquared(direction);
    if (squared > 1.0e-8f && std::isfinite(squared)) {
        const float inverseLength = 1.0f / std::sqrt(squared);
        return {direction.x * inverseLength, direction.y * inverseLength};
    }

    if (!finite(fallback)) return {1.0f, 0.0f};
    squared = lengthSquared(fallback);
    if (!(squared > 1.0e-8f) || !std::isfinite(squared)) return {1.0f, 0.0f};
    const float inverseLength = 1.0f / std::sqrt(squared);
    return {fallback.x * inverseLength, fallback.y * inverseLength};
}

inline Point clamp(Point point, Bounds bounds) {
    return {std::clamp(point.x, bounds.left, bounds.right),
            std::clamp(point.y, bounds.top, bounds.bottom)};
}

inline Point mapPoint(Point point, Bounds from, Bounds to) {
    const float x = from.right > from.left
        ? (point.x - from.left) / (from.right - from.left) : 0.5f;
    const float y = from.bottom > from.top
        ? (point.y - from.top) / (from.bottom - from.top) : 0.5f;
    return {to.left + x * (to.right - to.left),
            to.top + y * (to.bottom - to.top)};
}

inline Point remapVector(Point origin, Point vector, Bounds from, Bounds to) {
    const Point mappedOrigin = mapPoint(origin, from, to);
    const Point mappedEnd = mapPoint({origin.x + vector.x, origin.y + vector.y}, from, to);
    return {mappedEnd.x - mappedOrigin.x, mappedEnd.y - mappedOrigin.y};
}

inline void sampleAim(State& state, Point position, Point player) {
    if (!finite(player)) return;
    state.aimPoint = player;
    state.aimDirection = unitOr({player.x - position.x, player.y - position.y},
                                state.aimDirection);
}

inline bool inside(Point point, Bounds bounds) {
    return finite(point) && point.x >= bounds.left && point.x <= bounds.right &&
           point.y >= bounds.top && point.y <= bounds.bottom;
}

} // namespace detail

inline bool wantsCommit(const State& state) {
    return state.phase == Phase::Seek && state.remaining <= 1;
}

inline bool committed(const State& state) {
    return state.phase == Phase::Windup;
}

// `canCommit` is granted by the caller only after the shared coordinator has
// granted this Spitter and the caller has reserved its projectile slot.
inline TickEvents tick(State& state, Point& position, Point& velocity,
                       Point player, Bounds enemyCenterBounds,
                       bool canCommit, float seekSpeed=kSeekSpeed) {
    TickEvents events;
    enemyCenterBounds = detail::ordered(enemyCenterBounds);

    constexpr float stepSeconds = static_cast<float>(timing::kFixedStepSeconds);
    if (state.phase == Phase::Seek) {
        const Point bounded = detail::clamp(position, enemyCenterBounds);
        if (bounded.x != position.x || bounded.y != position.y) velocity = {};
        position = bounded;

        if (wantsCommit(state)) {
            if (canCommit) {
                state.phase = Phase::Windup;
                state.remaining = kWindupTicks;
                state.aimLocked = false;
                detail::sampleAim(state, position, player);
                velocity = {};
                events.warningStarted = true;
                return events;
            } else {
                state.remaining = 1;
            }
        }

        if (detail::finite(player) && detail::finite(position)) {
            const Point delta{player.x - position.x, player.y - position.y};
            const float squared = detail::lengthSquared(delta);
            const float distance = squared > 0.0f && std::isfinite(squared)
                ? std::sqrt(squared) : 0.0f;
            if (distance > 300.0f || distance < 200.0f) {
                const Point direction = distance < 200.0f
                    ? detail::unitOr({-delta.x, -delta.y}, state.aimDirection)
                    : detail::unitOr(delta, state.aimDirection);
                const Point previous = position;
                position = detail::clamp({position.x + direction.x * seekSpeed * stepSeconds,
                                          position.y + direction.y * seekSpeed * stepSeconds},
                                         enemyCenterBounds);
                velocity = {(position.x - previous.x) / stepSeconds,
                            (position.y - previous.y) / stepSeconds};
            } else {
                velocity = {};
            }
        } else {
            velocity = {};
        }

        if (state.remaining > 1) --state.remaining;
        else state.remaining = 1;
        return events;
    }

    // Only the first 30 windup updates read the player's position. The 30th
    // update captures the final soft-aim point and locks it for the remaining 78.
    if (state.remaining > kWindupTicks) state.remaining = kWindupTicks;
    if (state.remaining < 1) state.remaining = 1;
    velocity = {};
    if (!state.aimLocked) {
        const int elapsed = kWindupTicks - state.remaining + 1;
        if (elapsed <= kSoftAimTicks) {
            detail::sampleAim(state, position, player);
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
        state.phase = Phase::Seek;
        state.remaining = kSeekTicks;
        state.aimLocked = false;
    }
    return events;
}

// Resize preserves both the attack clock and the captured target point. A
// locked direction is recomputed from the mapped source and target; it never
// retargets toward the player's new position.
inline void remap(State& state, Point& position, Point& velocity,
                  Bounds from, Bounds to) {
    from = detail::ordered(from);
    to = detail::ordered(to);
    const Point oldPosition = position;
    position = detail::mapPoint(position, from, to);
    state.aimPoint = detail::mapPoint(state.aimPoint, from, to);
    if (state.aimLocked) {
        state.aimDirection = detail::unitOr(
            {state.aimPoint.x - position.x, state.aimPoint.y - position.y},
            detail::unitOr(detail::remapVector(oldPosition, state.aimDirection, from, to),
                           state.aimDirection));
    } else {
        state.aimDirection = detail::unitOr(
            detail::remapVector(oldPosition, state.aimDirection, from, to),
            state.aimDirection);
    }
    velocity = {};
}

inline Point muzzle(Point position, Point direction) {
    direction = detail::unitOr(direction);
    return {position.x + direction.x * kMuzzleOffset,
            position.y + direction.y * kMuzzleOffset};
}

enum class SlotState { Empty, Reserved, Flying };

struct ShotSpec {
    float speed=kProjectileSpeed, radius=kProjectileRadius;
    int damage=kProjectileDamage, lifetimeTicks=kProjectileLifetimeTicks;
};

struct Projectile {
    std::uint64_t id{};
    std::uint64_t sourceId{};
    Point p{};
    Point previousP{};
    Point velocity{};
    int remainingTicks{};
    SlotState state = SlotState::Empty;
    ShotSpec spec{};
};

struct Impact {
    std::uint64_t sourceId{};
    std::uint64_t projectileId{};
    int damage = kProjectileDamage;
};

struct Impacts {
    std::array<Impact, kProjectileCapacity> items{};
    int count{};
    int expired{};
};

class Pool {
public:
    using Ticket = std::uint64_t;

    Ticket reserve(std::uint64_t sourceId) {
        if (sourceId == 0) return 0;
        for (const Projectile& projectile : items_) {
            if (projectile.state == SlotState::Reserved &&
                projectile.sourceId == sourceId) return 0;
        }
        const auto slot = emptySlot();
        if (slot == kProjectileCapacity || nextId_ == 0) return 0;

        const Ticket ticket = nextId_;
        nextId_ = ticket == std::numeric_limits<Ticket>::max() ? 0 : ticket + 1;
        Projectile& projectile = items_[slot];
        projectile = {};
        projectile.id = ticket;
        projectile.sourceId = sourceId;
        projectile.state = SlotState::Reserved;
        return ticket;
    }

    bool cancelReservation(Ticket ticket) {
        const auto slot = find(ticket);
        if (slot == kProjectileCapacity || items_[slot].state != SlotState::Reserved)
            return false;
        items_[slot] = {};
        return true;
    }

    bool launch(Ticket ticket, Point origin, Point direction, ShotSpec spec={}) {
        const auto slot = find(ticket);
        if (slot == kProjectileCapacity || items_[slot].state != SlotState::Reserved ||
            !detail::finite(origin)||!std::isfinite(spec.speed)||spec.speed<=0||
            !std::isfinite(spec.radius)||spec.radius<=0||spec.damage<=0||spec.lifetimeTicks<=0) return false;
        direction = detail::unitOr(direction);
        Projectile& projectile = items_[slot];
        projectile.p = origin;
        projectile.previousP = origin;
        projectile.spec=spec;
        projectile.velocity = {direction.x * spec.speed,
                               direction.y * spec.speed};
        projectile.remainingTicks = spec.lifetimeTicks;
        projectile.state = SlotState::Flying;
        return true;
    }

    std::size_t freeSlots() const {
        std::size_t count = 0;
        for (const Projectile& projectile : items_)
            if (projectile.state == SlotState::Empty) ++count;
        return count;
    }

    std::size_t flyingCount() const {
        std::size_t count = 0;
        for (const Projectile& projectile : items_)
            if (projectile.state == SlotState::Flying) ++count;
        return count;
    }

    std::size_t reservedCount() const {
        std::size_t count = 0;
        for (const Projectile& projectile : items_)
            if (projectile.state == SlotState::Reserved) ++count;
        return count;
    }

    const std::array<Projectile, kProjectileCapacity>& items() const {
        return items_;
    }

    // Physical interception owns projectile removal only; it grants no rewards,
    // damage, player immunity or replacement shot. Reserved warnings are ineligible.
    bool intercept(Ticket ticket) {
        const auto slot=find(ticket);
        if(slot==kProjectileCapacity||items_[slot].state!=SlotState::Flying)return false;
        items_[slot]={};return true;
    }

    // Call only after all enemies on a combat update. Relative segment
    // distance sweeps both the projectile and player between their positions.
    Impacts tick(Point previousPlayer, Point player, float playerRadius,
                 Bounds fullArena, bool active = true) {
        Impacts impacts;
        if (!active) return impacts;

        fullArena = detail::ordered(fullArena);
        if (!std::isfinite(playerRadius) || playerRadius < 0.0f) playerRadius = 0.0f;
        constexpr float stepSeconds = static_cast<float>(timing::kFixedStepSeconds);

        for (Projectile& projectile : items_) {
            if (projectile.state != SlotState::Flying) continue;
            const float contactRadius=playerRadius+projectile.spec.radius;
            const float contactDistanceSquared=contactRadius*contactRadius;

            projectile.previousP = projectile.p;
            projectile.p = {projectile.p.x + projectile.velocity.x * stepSeconds,
                            projectile.p.y + projectile.velocity.y * stepSeconds};

            bool hit = false;
            if (detail::finite(previousPlayer) && detail::finite(player) &&
                detail::finite(projectile.previousP) && detail::finite(projectile.p)) {
                const Point relativeStart{previousPlayer.x - projectile.previousP.x,
                                          previousPlayer.y - projectile.previousP.y};
                const Point relativeEnd{player.x - projectile.p.x,
                                        player.y - projectile.p.y};
                hit = foundation::segmentDistanceSquared({0.0f, 0.0f}, relativeStart,
                                                         relativeEnd) <= contactDistanceSquared;
            }

            if (hit) {
                impacts.items[static_cast<std::size_t>(impacts.count++)] =
                    {projectile.sourceId, projectile.id, projectile.spec.damage};
                projectile = {};
                continue;
            }

            if (projectile.remainingTicks <= 1 || !detail::inside(projectile.p, fullArena)) {
                ++impacts.expired;
                projectile = {};
                continue;
            }
            --projectile.remainingTicks;
        }
        return impacts;
    }

    // Reservations and in-flight projectiles clear together at a safe
    // encounter boundary. IDs remain monotonic for this Pool instance.
    void clear() {
        items_ = {};
    }

    void remap(Bounds from, Bounds to) {
        from = detail::ordered(from);
        to = detail::ordered(to);
        for (Projectile& projectile : items_) {
            if (projectile.state != SlotState::Flying) continue;
            const Point oldPosition = projectile.p;
            projectile.p = detail::mapPoint(projectile.p, from, to);
            projectile.previousP = detail::mapPoint(projectile.previousP, from, to);
            const Point mappedDirection = detail::remapVector(
                oldPosition, detail::unitOr(projectile.velocity), from, to);
            const Point direction = detail::unitOr(mappedDirection, projectile.velocity);
            projectile.velocity = {direction.x * projectile.spec.speed,
                                   direction.y * projectile.spec.speed};
        }
    }

private:
    std::size_t emptySlot() const {
        for (std::size_t i = 0; i < items_.size(); ++i)
            if (items_[i].state == SlotState::Empty) return i;
        return kProjectileCapacity;
    }

    std::size_t find(Ticket ticket) const {
        if (ticket == 0) return kProjectileCapacity;
        for (std::size_t i = 0; i < items_.size(); ++i)
            if (items_[i].id == ticket && items_[i].state != SlotState::Empty) return i;
        return kProjectileCapacity;
    }

    std::array<Projectile, kProjectileCapacity> items_{};
    Ticket nextId_ = 1;
};

} // namespace threadlight::ranged
