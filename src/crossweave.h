#pragma once

#include "game_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace threadlight::crossweave {

constexpr int kCooldownTicks = 300;
constexpr int kMinimumSegmentAgeTicks = 72;
constexpr int kMaximumSegmentAgeTicks = 420;
constexpr float kTravelDistance = 160.0f;
constexpr float kPulseRadius = 110.0f;
constexpr std::size_t kClaimCapacity = 640;
constexpr double kEndpointInset = 0.05;
constexpr double kLineDeadZone = 0.05;

struct Segment {
    std::uint64_t id = 0;
    foundation::Point a{};
    foundation::Point b{};
    int ageTicks = 0;
};

struct PendingCrossing {
    std::uint64_t id = 0;
    int entrySide = 0;
    foundation::Point lastOnLine{};
};

struct State {
    int cooldownTicks = 0;
    float travelDistance = 0.0f;
    std::array<std::uint64_t, kClaimCapacity> consumedIds{};
    std::size_t consumedCount = 0;
    std::array<PendingCrossing, kClaimCapacity> pendingCrossings{};
    std::size_t pendingCount = 0;
    // If a caller exceeds the supported live-history bound, fail closed until
    // clear() so an unrecorded crossed ID can never become eligible again.
    bool claimOverflow = false;
};

struct Pulse {
    bool triggered = false;
    foundation::Point center{};
};

struct Target {
    std::uint64_t id=0;foundation::Point p{};float radius=0;
    int index=0,kind=0;
};
struct Selection {std::array<Target,6> items{};std::size_t count=0;};
inline Selection selectTargets(std::span<const Target> candidates,foundation::Point center){
    Selection result;
    auto distance=[&](const Target& t){const float x=t.p.x-center.x,y=t.p.y-center.y;return x*x+y*y;};
    auto nearer=[&](const Target& a,const Target& b){const float x=distance(a),y=distance(b);return x<y||(x==y&&a.id<b.id);};
    for(const auto& target:candidates){
        const float d=distance(target),reach=kPulseRadius+target.radius;
        if(!target.id||!std::isfinite(d)||!std::isfinite(reach)||target.radius<0||d>reach*reach)continue;
        bool duplicate=false;for(std::size_t i=0;i<result.count;++i)if(result.items[i].id==target.id)duplicate=true;
        if(duplicate)continue;
        if(result.count<result.items.size())result.items[result.count++]=target;
        else if(nearer(target,result.items.back()))result.items.back()=target;
        else continue;
        for(std::size_t i=result.count-1;i>0&&nearer(result.items[i],result.items[i-1]);--i)
            std::swap(result.items[i],result.items[i-1]);
    }
    return result;
}

inline bool finite(foundation::Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

inline bool properCrossing(foundation::Point previousPlayer,
                           foundation::Point currentPlayer,
                           foundation::Point a,
                           foundation::Point b,
                           foundation::Point& intersection) {
    if (!finite(previousPlayer) || !finite(currentPlayer) || !finite(a) || !finite(b)) return false;

    const double rx = static_cast<double>(currentPlayer.x) - previousPlayer.x;
    const double ry = static_cast<double>(currentPlayer.y) - previousPlayer.y;
    const double sx = static_cast<double>(b.x) - a.x;
    const double sy = static_cast<double>(b.y) - a.y;
    const double rLength = std::hypot(rx, ry);
    const double sLength = std::hypot(sx, sy);
    if (rLength <= 0.0 || sLength <= 0.0) return false;

    const double denominator = rx * sy - ry * sx;
    const double scale = rLength * sLength;
    if (std::abs(denominator) <= 1.0e-9 * std::max(1.0, scale)) return false;

    const double qpx = static_cast<double>(a.x) - previousPlayer.x;
    const double qpy = static_cast<double>(a.y) - previousPlayer.y;
    const double t = (qpx * sy - qpy * sx) / denominator;
    const double u = (qpx * ry - qpy * rx) / denominator;

    // Requiring a small interior margin rejects endpoint touches and tiny
    // endpoint jitter while retaining ordinary crossings through the segment.
    const double tInset = std::max(1.0e-9, kEndpointInset / rLength);
    const double uInset = std::max(1.0e-9, kEndpointInset / sLength);
    if (t <= tInset || t >= 1.0 - tInset || u <= uInset || u >= 1.0 - uInset) return false;

    intersection = {
        static_cast<float>(static_cast<double>(previousPlayer.x) + t * rx),
        static_cast<float>(static_cast<double>(previousPlayer.y) + t * ry)
    };
    return finite(intersection);
}

inline bool eligible(const Segment& segment) {
    return segment.id != 0 && segment.ageTicks >= kMinimumSegmentAgeTicks &&
           segment.ageTicks <= kMaximumSegmentAgeTicks;
}

inline bool projectInterior(foundation::Point point, const Segment& segment,
                            foundation::Point& projection) {
    if (!finite(point) || !finite(segment.a) || !finite(segment.b)) return false;
    const double sx = static_cast<double>(segment.b.x) - segment.a.x;
    const double sy = static_cast<double>(segment.b.y) - segment.a.y;
    const double lengthSquared = sx * sx + sy * sy;
    if (!(lengthSquared > 0.0) || !std::isfinite(lengthSquared)) return false;
    const double length = std::sqrt(lengthSquared);
    const double along = ((static_cast<double>(point.x) - segment.a.x) * sx +
                          (static_cast<double>(point.y) - segment.a.y) * sy) / length;
    if (along <= kEndpointInset || along >= length - kEndpointInset) return false;
    const double t = along / length;
    projection = {
        static_cast<float>(static_cast<double>(segment.a.x) + t * sx),
        static_cast<float>(static_cast<double>(segment.a.y) + t * sy)
    };
    return finite(projection);
}

// Signed distance to the historical segment's supporting line. Values in a
// narrow world-space band around the line count as on-line, absorbing float
// jitter without converting a same-side touch into a crossing.
inline int sideOfLine(foundation::Point point, const Segment& segment) {
    if (!finite(point) || !finite(segment.a) || !finite(segment.b)) return 0;
    const double sx = static_cast<double>(segment.b.x) - segment.a.x;
    const double sy = static_cast<double>(segment.b.y) - segment.a.y;
    const double length = std::hypot(sx, sy);
    if (!(length > 0.0) || !std::isfinite(length)) return 0;
    const double cross = sx * (static_cast<double>(point.y) - segment.a.y) -
                         sy * (static_cast<double>(point.x) - segment.a.x);
    const double distance = cross / length;
    if (distance > kLineDeadZone) return 1;
    if (distance < -kLineDeadZone) return -1;
    return 0;
}

inline std::size_t pendingIndex(const State& state, std::uint64_t id) {
    const std::size_t count = std::min(state.pendingCount, kClaimCapacity);
    for (std::size_t i = 0; i < count; ++i) {
        if (state.pendingCrossings[i].id == id) return i;
    }
    return count;
}

inline void removePending(State& state, std::size_t index) {
    if (index >= state.pendingCount || index >= kClaimCapacity) return;
    for (std::size_t i = index + 1; i < state.pendingCount; ++i) {
        state.pendingCrossings[i - 1] = state.pendingCrossings[i];
    }
    --state.pendingCount;
    state.pendingCrossings[state.pendingCount] = PendingCrossing{};
}

inline void prunePending(State& state, std::span<const Segment> segments) {
    std::size_t i = 0;
    while (i < state.pendingCount && i < kClaimCapacity) {
        const PendingCrossing& pending = state.pendingCrossings[i];
        bool live = false;
        for (const Segment& segment : segments) {
            if (segment.id == pending.id && eligible(segment)) {
                live = true;
                break;
            }
        }
        if (!live || pending.id == 0) removePending(state, i);
        else ++i;
    }
}

inline void beginPending(State& state, std::uint64_t id, int entrySide,
                         foundation::Point lastOnLine) {
    if (id == 0 || entrySide == 0 || pendingIndex(state, id) < state.pendingCount) return;
    if (state.pendingCount >= kClaimCapacity) {
        state.claimOverflow = true;
        return;
    }
    state.pendingCrossings[state.pendingCount++] = {id, entrySide, lastOnLine};
}

inline bool containsId(std::span<const Segment> segments, std::uint64_t id) {
    for (const Segment& segment : segments) {
        if (segment.id == id) return true;
    }
    return false;
}

inline bool claimed(const State& state, std::uint64_t id) {
    const std::size_t count = std::min(state.consumedCount, kClaimCapacity);
    for (std::size_t i = 0; i < count; ++i) {
        if (state.consumedIds[i] == id) return true;
    }
    return false;
}

inline void pruneClaims(State& state, std::span<const Segment> segments) {
    const std::size_t count = std::min(state.consumedCount, kClaimCapacity);
    std::size_t retained = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint64_t id = state.consumedIds[i];
        if (id != 0 && containsId(segments, id)) state.consumedIds[retained++] = id;
    }
    for (std::size_t i = retained; i < count; ++i) state.consumedIds[i] = 0;
    state.consumedCount = retained;
}

inline void consume(State& state, std::uint64_t id) {
    if (id == 0 || claimed(state, id)) return;
    if (state.consumedCount >= kClaimCapacity) {
        state.claimOverflow = true;
        return;
    }
    state.consumedIds[state.consumedCount++] = id;
}

inline void clear(State& state) {
    state = State{};
}

// One combat tick of pure geometry and signature timing. The caller supplies
// its full live history span (bounded by the encounter's 520-segment history).
// This helper returns only a pulse location; target selection and damage belong
// to the caller. Noncombat, paused, and unowned calls freeze all state.
inline Pulse tick(State& state,
                  foundation::Point previousPlayer,
                  foundation::Point currentPlayer,
                  std::span<const Segment> segments,
                  bool combat,
                  bool owned) {
    if (!combat || !owned) return {};

    pruneClaims(state, segments);
    prunePending(state, segments);

    state.cooldownTicks = std::max(0, state.cooldownTicks);
    if (state.cooldownTicks > 0) --state.cooldownTicks;

    if (!std::isfinite(state.travelDistance) || state.travelDistance < 0.0f) state.travelDistance = 0.0f;
    if (finite(previousPlayer) && finite(currentPlayer)) {
        const double dx = static_cast<double>(currentPlayer.x) - previousPlayer.x;
        const double dy = static_cast<double>(currentPlayer.y) - previousPlayer.y;
        const double distance = std::hypot(dx, dy);
        if (std::isfinite(distance)) {
            state.travelDistance = static_cast<float>(std::min(
                static_cast<double>(kTravelDistance),
                static_cast<double>(state.travelDistance) + distance));
        }
    }

    bool foundCrossing = false;
    std::uint64_t selectedId = 0;
    foundation::Point selectedCenter{};
    for (const Segment& segment : segments) {
        if (!eligible(segment)) continue;

        const std::size_t pendingAtStart = pendingIndex(state, segment.id);
        if (claimed(state, segment.id)) {
            if (pendingAtStart < state.pendingCount) removePending(state, pendingAtStart);
            continue;
        }

        foundation::Point crossing{};
        const int previousSide = sideOfLine(previousPlayer, segment);
        const int currentSide = sideOfLine(currentPlayer, segment);
        bool crossed = previousSide != 0 && currentSide != 0 && previousSide != currentSide &&
                       properCrossing(previousPlayer, currentPlayer, segment.a, segment.b, crossing);
        std::size_t pending = pendingIndex(state, segment.id);
        // A resize clamp/teleport must not turn a stale touch into a crossing.
        // Legitimate pending exits always begin on the line's dead zone.
        if(pending<state.pendingCount&&previousSide!=0){
            removePending(state,pending);pending=state.pendingCount;
        }
        if (crossed) {
            if (pending < state.pendingCount) removePending(state, pending);
        } else {
            foundation::Point projection{};
            const bool currentInterior = projectInterior(currentPlayer, segment, projection);
            if (pending < state.pendingCount) {
                PendingCrossing& candidate = state.pendingCrossings[pending];
                if (currentSide == 0 && currentInterior) {
                    candidate.lastOnLine = projection;
                    continue;
                }
                if (currentSide == -candidate.entrySide && currentInterior) {
                    crossing = candidate.lastOnLine;
                    crossed = true;
                    removePending(state, pending);
                } else {
                    // Returning to the entry side is a touch/reversal, not a
                    // crossing. Leaving the interior inset also cancels it.
                    removePending(state, pending);
                    continue;
                }
            } else if (previousSide != 0 && currentSide == 0 && currentInterior) {
                beginPending(state, segment.id, previousSide, projection);
                continue;
            } else {
                continue;
            }
        }

        // Consume every crossed eligible knot, even when cooldown/travel gates
        // are closed or another knot already determines this tick's pulse.
        const bool wasClaimed = claimed(state, segment.id);
        consume(state, segment.id);
        if (wasClaimed) continue;

        if (!foundCrossing || segment.id < selectedId) {
            foundCrossing = true;
            selectedId = segment.id;
            selectedCenter = crossing;
        }
    }

    if (!foundCrossing || state.claimOverflow || state.cooldownTicks > 0 ||
        state.travelDistance < kTravelDistance) return {};

    state.cooldownTicks = kCooldownTicks;
    state.travelDistance = 0.0f;
    return {true, selectedCenter};
}

// Normal Crossweave damage equals one silk hit; bosses receive half, rounded
// once to nearest with positive halves rounded upward. Exposure bonuses do not
// enter this signature calculation.
inline int damage(int baseCenti, bool boss = false) {
    if (baseCenti <= 0) return 0;
    if (!boss) return baseCenti;
    return static_cast<int>((static_cast<std::int64_t>(baseCenti) + 1) / 2);
}

} // namespace threadlight::crossweave
