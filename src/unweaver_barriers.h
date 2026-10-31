#pragma once

#include "game_geometry.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace threadlight::unweaver::barriers {
using Point = foundation::Point;

constexpr int kWallCount = 6;
constexpr int kWarningTicks = 180;
constexpr int kLifetimeTicks = 720;
constexpr int kSpikeDamage = 2200;
constexpr float kHalfWidth = 8.f;

struct Bounds { float left, right, top, bottom; };
struct Wall {
    Point a{}, b{};
    float halfwidth = 0.f;
    int warning = 0;
    int remaining = 0;
    bool spiked = false;
    bool fresh = false;
};
struct State {
    std::array<Wall, kWallCount> walls{};
    int count = 0;
};

namespace detail {
inline Bounds ordered(Bounds b) {
    if (b.left > b.right) std::swap(b.left, b.right);
    if (b.top > b.bottom) std::swap(b.top, b.bottom);
    return b;
}
inline Point interpolate(Point a, Point b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
inline float clamp(float value, float low, float high) {
    if (low > high) std::swap(low, high);
    return std::clamp(value, low, high);
}
inline bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
inline bool live(const Wall& wall) { return wall.remaining > 0; }
inline bool active(const Wall& wall) { return wall.warning <= 0 && live(wall); }

inline bool segmentHitsBox(Point start, Point end,
                           float left, float right, float top, float bottom) {
    float enter = 0.f, leave = 1.f;
    const auto clip = [&](float origin, float delta, float low, float high,
                          float& first, float& last) {
        if (std::abs(delta) < 1e-7f) return origin >= low && origin <= high;
        float a = (low - origin) / delta;
        float b = (high - origin) / delta;
        if (a > b) std::swap(a, b);
        first = std::max(first, a);
        last = std::min(last, b);
        return first <= last;
    };
    return clip(start.x, end.x - start.x, left, right, enter, leave) &&
           clip(start.y, end.y - start.y, top, bottom, enter, leave) &&
           leave >= 0.f && enter <= 1.f;
}

inline Point slideAtWall(const Wall& wall, Point start, Point target, float radius) {
    const float dx = std::abs(wall.b.x - wall.a.x);
    const float dy = std::abs(wall.b.y - wall.a.y);
    const float halfwidth = std::max(0.f, wall.halfwidth);
    constexpr float kClearance = .01f;
    if (dy >= dx) {
        const float x = (wall.a.x + wall.b.x) * .5f;
        const float left = x - halfwidth - radius;
        const float right = x + halfwidth + radius;
        const float top = std::min(wall.a.y, wall.b.y) - radius;
        const float bottom = std::max(wall.a.y, wall.b.y) + radius;
        if (!segmentHitsBox(start, target, left, right, top, bottom)) return target;
        if (start.x < left && target.x > start.x) target.x = left - kClearance;
        else if (start.x > right && target.x < start.x) target.x = right + kClearance;
        else if (start.x >= left && start.x <= right) {
            if (target.x < left) { /* Move out through the left face. */ }
            else if (target.x > right) { /* Move out through the right face. */ }
            else target.x = start.x;
        }
    } else {
        const float y = (wall.a.y + wall.b.y) * .5f;
        const float left = std::min(wall.a.x, wall.b.x) - radius;
        const float right = std::max(wall.a.x, wall.b.x) + radius;
        const float top = y - halfwidth - radius;
        const float bottom = y + halfwidth + radius;
        if (!segmentHitsBox(start, target, left, right, top, bottom)) return target;
        if (start.y < top && target.y > start.y) target.y = top - kClearance;
        else if (start.y > bottom && target.y < start.y) target.y = bottom + kClearance;
        else if (start.y >= top && start.y <= bottom) {
            if (target.y < top) { /* Move out through the top face. */ }
            else if (target.y > bottom) { /* Move out through the bottom face. */ }
            else target.y = start.y;
        }
    }
    return target;
}
} // namespace detail

inline void clear(State& state) { state = {}; }

// Author three short-crossing partitions, each split around an open lane.
// The lanes stay open in both modes; lethal mode narrows them but never seals
// the arena into a cage. Geometry is fixed when the warning starts.
inline void plan(State& state, Bounds bounds, Point player, Point forecast, bool lethal = false) {
    clear(state);
    bounds = detail::ordered(bounds);
    if (!detail::finite(player) || !detail::finite(forecast) ||
        !std::isfinite(bounds.left) || !std::isfinite(bounds.right) ||
        !std::isfinite(bounds.top) || !std::isfinite(bounds.bottom) ||
        bounds.right - bounds.left < 64.f || bounds.bottom - bounds.top < 64.f) return;

    const float dx = forecast.x - player.x;
    const float dy = forecast.y - player.y;
    const bool verticalPartitions = std::abs(dx) >= std::abs(dy);
    const float gap = lethal ? 72.f : 108.f;
    const float margin = std::min(16.f, std::min(bounds.right - bounds.left,
                                                bounds.bottom - bounds.top) * .1f);
    const std::array<float, 3> fractions{.25f, .5f, .75f};
    for (int line = 0; line < 3; ++line) {
        const float t = fractions[static_cast<std::size_t>(line)];
        float along, cross;
        if (verticalPartitions) {
            along = std::abs(dx) >= 96.f
                ? detail::interpolate({player.x, 0.f}, {forecast.x, 0.f}, t).x
                : (player.x + forecast.x) * .5f + (line - 1) * 64.f;
            along = detail::clamp(along, bounds.left + margin, bounds.right - margin);
            cross = detail::clamp(player.y + dy * t, bounds.top + margin, bounds.bottom - margin);
            const float topEnd = detail::clamp(cross - gap * .5f, bounds.top + margin, bounds.bottom - margin);
            const float bottomStart = detail::clamp(cross + gap * .5f, bounds.top + margin, bounds.bottom - margin);
            state.walls[line * 2] = {{along, bounds.top + margin}, {along, topEnd},
                                      kHalfWidth, kWarningTicks, kLifetimeTicks,
                                      (line % 2) != 0, false};
            state.walls[line * 2 + 1] = {{along, bottomStart}, {along, bounds.bottom - margin},
                                          kHalfWidth, kWarningTicks, kLifetimeTicks,
                                          (line % 2) == 0, false};
        } else {
            along = std::abs(dy) >= 96.f
                ? detail::interpolate({0.f, player.y}, {0.f, forecast.y}, t).y
                : (player.y + forecast.y) * .5f + (line - 1) * 64.f;
            along = detail::clamp(along, bounds.top + margin, bounds.bottom - margin);
            cross = detail::clamp(player.x + dx * t, bounds.left + margin, bounds.right - margin);
            const float leftEnd = detail::clamp(cross - gap * .5f, bounds.left + margin, bounds.right - margin);
            const float rightStart = detail::clamp(cross + gap * .5f, bounds.left + margin, bounds.right - margin);
            state.walls[line * 2] = {{bounds.left + margin, along}, {leftEnd, along},
                                      kHalfWidth, kWarningTicks, kLifetimeTicks,
                                      (line % 2) != 0, false};
            state.walls[line * 2 + 1] = {{rightStart, along}, {bounds.right - margin, along},
                                          kHalfWidth, kWarningTicks, kLifetimeTicks,
                                          (line % 2) == 0, false};
        }
    }
    state.count = kWallCount;
}

inline void tick(State& state) {
    for (int i = 0; i < std::clamp(state.count, 0, kWallCount); ++i) {
        Wall& wall = state.walls[static_cast<std::size_t>(i)];
        if (!detail::live(wall)) { wall = {}; continue; }
        if (wall.warning > 0) {
            --wall.warning;
            if (wall.warning == 0) wall.fresh = true;
            continue;
        }
        wall.fresh = false;
        if (--wall.remaining <= 0) wall = {};
    }
}

// Swept circle movement against solid walls. Expanding the wall to an AABB
// makes contact conservative at its endpoints; blocked normal motion still
// allows the other axis to slide along the wall.
inline Point move(const State& state, Point previous, Point proposed, float radius) {
    if (!detail::finite(previous) || !detail::finite(proposed)) return previous;
    radius = std::max(0.f, std::isfinite(radius) ? radius : 0.f);
    Point result = proposed;
    const int count = std::clamp(state.count, 0, kWallCount);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < count; ++i) {
            const Wall& wall = state.walls[static_cast<std::size_t>(i)];
            if (!detail::active(wall) || wall.spiked) continue;
            result = detail::slideAtWall(wall, previous, result, radius);
        }
    }
    return result;
}

// Spike contact is a swept capsule hit. Spikes stay traversable and only
// contribute their fixed contact damage after the warning and fresh tick.
inline int damage(const State& state, Point previous, Point now, float radius) {
    if (!detail::finite(previous) || !detail::finite(now)) return 0;
    radius = std::max(0.f, std::isfinite(radius) ? radius : 0.f);
    const int count = std::clamp(state.count, 0, kWallCount);
    for (int i = 0; i < count; ++i) {
        const Wall& wall = state.walls[static_cast<std::size_t>(i)];
        if (!detail::active(wall) || !wall.spiked || wall.fresh) continue;
        if (foundation::segmentsDistanceSquared(previous, now, wall.a, wall.b) <=
            (radius + std::max(0.f, wall.halfwidth)) *
            (radius + std::max(0.f, wall.halfwidth))) return kSpikeDamage;
    }
    return 0;
}
// Small obstacle detour for uncommitted supports; preserve their movement budget.
inline Point navigate(const State& state,Point start,Point proposed,Point target,float radius,float budget){
 Point result=move(state,start,proposed,radius);
 const auto sq=[](Point a,Point b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;};
 if(sq(start,result)>.25f*sq(start,proposed))return result;
 for(int i=0;i<state.count;++i){const auto& w=state.walls[i];if(!detail::active(w)||w.spiked)continue;
  const float pad=radius+w.halfwidth+4;
  if(!detail::segmentHitsBox(start,proposed,std::min(w.a.x,w.b.x)-pad,std::max(w.a.x,w.b.x)+pad,std::min(w.a.y,w.b.y)-pad,std::max(w.a.y,w.b.y)+pad))continue;
  const float dx=w.b.x-w.a.x,dy=w.b.y-w.a.y;const float len=std::hypot(dx,dy);if(len<1)continue;
  Point a{w.a.x-dx/len*pad,w.a.y-dy/len*pad},b{w.b.x+dx/len*pad,w.b.y+dy/len*pad};
  const Point goal=std::sqrt(sq(start,a))+std::sqrt(sq(a,target))<std::sqrt(sq(start,b))+std::sqrt(sq(b,target))?a:b;
  const float distance=std::sqrt(sq(start,goal));if(distance<.01f)continue;
  const Point step{start.x+(goal.x-start.x)*std::min(1.f,budget/distance),start.y+(goal.y-start.y)*std::min(1.f,budget/distance)};
  const auto moved=move(state,start,step,radius);if(sq(start,moved)>sq(start,result))result=moved;
 }
 return result;
}
inline bool blocksProjectile(const State& state,Point a,Point b,float radius){
 for(int i=0;i<state.count;++i){const auto& w=state.walls[i];if(!detail::active(w)||w.spiked)continue;
  const float pad=w.halfwidth+radius;
  if(detail::segmentHitsBox(a,b,std::min(w.a.x,w.b.x)-pad,std::max(w.a.x,w.b.x)+pad,std::min(w.a.y,w.b.y)-pad,std::max(w.a.y,w.b.y)+pad))return true;
 }
 return false;
}
} // namespace threadlight::unweaver::barriers
