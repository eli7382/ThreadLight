#pragma once

#include "crossweave.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace threadlight::cocoon {

constexpr int kBarrierCapacityCenti = 1200;
constexpr int kBarrierDurationTicks = 600;
constexpr int kCooldownTicks = 960;
constexpr double kMinimumClosedPath = 240.0;
constexpr double kMinimumEnclosedArea = 3500.0;
constexpr std::size_t kMaximumHistorySegments = 520;
constexpr std::size_t kMaximumPolygonVertices = kMaximumHistorySegments + 2;
constexpr double kHistoryJoinTolerance = 0.05;
constexpr double kPolygonTouchTolerance = 0.05;

struct State {
    crossweave::State detector{};
    int barrierCenti = 0;
    int remainingTicks = 0;
    int cooldownTicks = 0;
};

struct Formation {
    bool formed = false;
    foundation::Point center{};
    std::uint64_t segmentId = 0;
    double area = 0.0;
    double pathLength = 0.0;
};

struct ClosedPath {
    std::array<foundation::Point, kMaximumPolygonVertices> vertices{};
    std::size_t vertexCount = 0;
    double area = 0.0;
    double pathLength = 0.0;
};

inline bool barrierActive(const State& state) {
    return state.barrierCenti > 0 && state.remainingTicks > 0;
}

inline bool samePoint(foundation::Point a, foundation::Point b, double tolerance) {
    if (!crossweave::finite(a) || !crossweave::finite(b)) return false;
    return std::hypot(static_cast<double>(a.x) - b.x,
                      static_cast<double>(a.y) - b.y) <= tolerance;
}

inline bool appendVertex(ClosedPath& path, foundation::Point point) {
    if (!crossweave::finite(point)) return false;
    if (path.vertexCount > 0 &&
        samePoint(path.vertices[path.vertexCount - 1], point, kPolygonTouchTolerance)) return true;
    if (path.vertexCount >= path.vertices.size()) return false;
    path.vertices[path.vertexCount++] = point;
    return true;
}

inline bool crossingCenter(const crossweave::State& before,
                           foundation::Point previousPlayer,
                           foundation::Point currentPlayer,
                           const crossweave::Segment& segment,
                           foundation::Point& center) {
    if (!crossweave::eligible(segment)) return false;

    const int previousSide = crossweave::sideOfLine(previousPlayer, segment);
    const int currentSide = crossweave::sideOfLine(currentPlayer, segment);
    if (previousSide != 0 && currentSide != 0 && previousSide != currentSide &&
        crossweave::properCrossing(previousPlayer, currentPlayer, segment.a, segment.b, center)) {
        return true;
    }

    if (previousSide != 0 || currentSide == 0) return false;
    const std::size_t pending = crossweave::pendingIndex(before, segment.id);
    if (pending >= before.pendingCount || pending >= crossweave::kClaimCapacity) return false;
    const crossweave::PendingCrossing& candidate = before.pendingCrossings[pending];
    if (candidate.entrySide == 0 || currentSide != -candidate.entrySide) return false;

    foundation::Point projection{};
    if (!crossweave::projectInterior(currentPlayer, segment, projection)) return false;
    center = candidate.lastOnLine;
    return crossweave::finite(center);
}

inline bool closedPathMetrics(foundation::Point crossing,
                              foundation::Point previousPlayer,
                              std::span<const crossweave::Segment> segments,
                              std::size_t segmentIndex,
                              ClosedPath& result) {
    result = ClosedPath{};
    if (segments.size() > kMaximumHistorySegments || segmentIndex >= segments.size() ||
        !crossweave::finite(crossing) || !crossweave::finite(previousPlayer)) return false;

    const crossweave::Segment& oldSegment = segments[segmentIndex];
    if (!crossweave::eligible(oldSegment) || !crossweave::finite(oldSegment.b)) return false;
    foundation::Point projected{};
    if (!crossweave::projectInterior(crossing, oldSegment, projected) ||
        !samePoint(crossing, projected, crossweave::kLineDeadZone)) return false;

    double length = 0.0;
    foundation::Point previous = crossing;
    if (!appendVertex(result, crossing)) return false;
    const auto append = [&](foundation::Point point, foundation::Point& prior,
                            double& accumulatedLength, ClosedPath& path) {
        if (!crossweave::finite(point) || !crossweave::finite(prior)) return false;
        const double dx = static_cast<double>(point.x) - prior.x;
        const double dy = static_cast<double>(point.y) - prior.y;
        accumulatedLength += std::hypot(dx, dy);
        if (!appendVertex(path, point)) return false;
        prior = point;
        return std::isfinite(accumulatedLength);
    };

    // The first edge follows the older strand from the actual intersection to
    // its newer endpoint. Subsequent edges follow the recorded player route.
    if (!append(oldSegment.b, previous, length, result)) return false;
    std::uint64_t previousId = oldSegment.id;
    int previousAge = oldSegment.ageTicks;
    for (std::size_t i = segmentIndex + 1; i < segments.size(); ++i) {
        const crossweave::Segment& segment = segments[i];
        if (segment.id <= previousId || segment.ageTicks > previousAge ||
            !crossweave::finite(segment.a) || !crossweave::finite(segment.b) ||
            !samePoint(previous, segment.a, kHistoryJoinTolerance)) return false;
        if (!append(segment.b, previous, length, result)) return false;
        previousId = segment.id;
        previousAge = segment.ageTicks;
    }

    // The history must end at the prior player position. The last edge closes
    // along this tick's actual movement to the crossing, not a shortcut chord.
    if (!samePoint(previous, previousPlayer, kHistoryJoinTolerance)) return false;
    length += std::hypot(static_cast<double>(crossing.x) - previous.x,
                         static_cast<double>(crossing.y) - previous.y);
    if (!std::isfinite(length)) return false;

    // The closing crossing is implicit in the polygon edges. Drop a final
    // point that only duplicates its first vertex within the touch tolerance.
    if (result.vertexCount > 1 &&
        samePoint(result.vertices[0], result.vertices[result.vertexCount - 1],
                  kPolygonTouchTolerance)) {
        --result.vertexCount;
    }
    if (result.vertexCount < 3) return false;

    double twiceArea = 0.0;
    for (std::size_t i = 0; i < result.vertexCount; ++i) {
        const foundation::Point a = result.vertices[i];
        const foundation::Point b = result.vertices[(i + 1) % result.vertexCount];
        twiceArea += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    result.area = std::abs(twiceArea) * 0.5;
    result.pathLength = length;
    return std::isfinite(result.area) && std::isfinite(result.pathLength);
}

inline bool adjacentCollinearBacktrack(foundation::Point previous,
                                       foundation::Point vertex,
                                       foundation::Point next) {
    const double ux = static_cast<double>(vertex.x) - previous.x;
    const double uy = static_cast<double>(vertex.y) - previous.y;
    const double vx = static_cast<double>(next.x) - vertex.x;
    const double vy = static_cast<double>(next.y) - vertex.y;
    const double uLength = std::hypot(ux, uy);
    const double vLength = std::hypot(vx, vy);
    if (uLength <= kPolygonTouchTolerance || vLength <= kPolygonTouchTolerance) return false;
    const double cross = ux * vy - uy * vx;
    const double dot = ux * vx + uy * vy;
    const double crossTolerance = kPolygonTouchTolerance * std::max(uLength, vLength);
    return std::abs(cross) <= crossTolerance &&
           dot < -(kPolygonTouchTolerance * kPolygonTouchTolerance);
}

inline bool simplePolygon(std::span<const foundation::Point> input) {
    if (input.size() > kMaximumPolygonVertices) return false;

    std::array<foundation::Point, kMaximumPolygonVertices> vertices{};
    std::size_t count = 0;
    for (foundation::Point point : input) {
        if (!crossweave::finite(point)) return false;
        if (count > 0 && samePoint(vertices[count - 1], point, kPolygonTouchTolerance)) continue;
        vertices[count++] = point;
    }
    if (count > 1 && samePoint(vertices[0], vertices[count - 1], kPolygonTouchTolerance)) --count;
    if (count < 3) return false;

    for (std::size_t i = 0; i < count; ++i) {
        const foundation::Point previous = vertices[(i + count - 1) % count];
        const foundation::Point vertex = vertices[i];
        const foundation::Point next = vertices[(i + 1) % count];
        if (adjacentCollinearBacktrack(previous, vertex, next)) return false;
    }

    const float touchSquared = static_cast<float>(kPolygonTouchTolerance * kPolygonTouchTolerance);
    for (std::size_t i = 0; i < count; ++i) {
        const foundation::Point a = vertices[i];
        const foundation::Point b = vertices[(i + 1) % count];
        for (std::size_t j = i + 1; j < count; ++j) {
            if (j == i + 1 || (i == 0 && j == count - 1)) continue;
            const foundation::Point c = vertices[j];
            const foundation::Point d = vertices[(j + 1) % count];
            if (foundation::segmentsDistanceSquared(a, b, c, d) <= touchSquared) return false;
        }
    }
    return true;
}

inline void clear(State& state) {
    state = State{};
}

// The Crossweave detector supplies stable-ID consumption and the shared
// endpoint-pending logic. Its own travel and cooldown are reset before each
// call so Cocoon's 240-unit loop path and 960-tick cooldown remain independent.
// This helper reports formation only; combat owns barrier absorption and HP.
inline Formation tick(State& state,
                      foundation::Point previousPlayer,
                      foundation::Point currentPlayer,
                      std::span<const crossweave::Segment> segments,
                      bool combat,
                      bool owned) {
    if (!combat || !owned) return {};

    state.barrierCenti = std::max(0, state.barrierCenti);
    state.remainingTicks = std::max(0, state.remainingTicks);
    state.cooldownTicks = std::max(0, state.cooldownTicks);
    if (state.remainingTicks > 0) --state.remainingTicks;
    if (state.cooldownTicks > 0) --state.cooldownTicks;
    if (state.remainingTicks == 0) state.barrierCenti = 0;

    // Most owned ticks are blocked from forming by the active barrier or its
    // cooldown. Keep advancing the detector on those ticks, but avoid copying
    // its large fixed-capacity history unless formation could be considered.
    const bool mayForm = !state.detector.claimOverflow && !barrierActive(state) &&
                         state.cooldownTicks == 0;
    std::optional<crossweave::State> before;
    if (mayForm) before.emplace(state.detector);
    state.detector.cooldownTicks = 0;
    state.detector.travelDistance = crossweave::kTravelDistance;
    (void)crossweave::tick(state.detector, previousPlayer, currentPlayer, segments, true, true);

    // All IDs crossed on this movement have now been recorded. Evaluate each
    // new claim separately because Crossweave's public result selects only one
    // center when several historical strands are crossed in the same tick.
    if (!mayForm || state.detector.claimOverflow) return {};

    for (std::size_t i = 0; i < segments.size(); ++i) {
        const crossweave::Segment& segment = segments[i];
        if (!crossweave::eligible(segment) || crossweave::claimed(*before, segment.id) ||
            !crossweave::claimed(state.detector, segment.id)) continue;

        foundation::Point center{};
        if (!crossingCenter(*before, previousPlayer, currentPlayer, segment, center)) continue;

        ClosedPath path;
        if (!closedPathMetrics(center, previousPlayer, segments, i, path) ||
            path.area < kMinimumEnclosedArea || path.pathLength < kMinimumClosedPath ||
            !simplePolygon(std::span<const foundation::Point>(path.vertices.data(), path.vertexCount))) {
            continue;
        }

        state.barrierCenti = kBarrierCapacityCenti;
        state.remainingTicks = kBarrierDurationTicks;
        state.cooldownTicks = kCooldownTicks;
        return {true, center, segment.id, path.area, path.pathLength};
    }
    return {};
}

} // namespace threadlight::cocoon
