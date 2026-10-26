#pragma once

#include "combat.h"
#include "game_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace threadlight::area_hazards {

using Point = foundation::Point;
using Ticket = std::uint64_t;

constexpr std::size_t kPoolSlots = 12;
constexpr int kOrdinaryPoolCapacity = 6;
constexpr int kBossPoolCapacity = 4;
constexpr int kOrdinaryOwnerCapacity = 2;
constexpr int kBossOwnerCapacity = 4;
constexpr int kHarmlessFadeTicks = 30;
constexpr float kMaxOccupiedFraction = 0.18f;

struct Bounds { float left{}, right{}, top{}, bottom{}; };

// A capsule-shaped protected route. `halfWidth` is the radius around the
// segment; leave `enabled` false when the encounter has no protected route.
struct Corridor {
    bool enabled = false;
    Point a{}, b{};
    float halfWidth = 0.0f;
};

enum class PoolKind { Ordinary, Boss };
enum class Shape { Disk, Capsule, Annulus };
enum class SlotState { Empty, Warning, Active, Fading };

// Annulus gaps are two equal, fixed cones centered on `gapDirection` and its
// opposite. A zero half-angle closes the gaps. `radiusStart` to `radiusEnd`
// is swept over `activeTicks`; `ringHalfWidth` is the ring's radial thickness.
struct PatchSpec {
    Shape shape = Shape::Disk;
    std::uint64_t ownerId = 0;
    Point center{};
    Point a{}, b{};
    float radius = 0.0f;
    float halfWidth = 0.0f;
    float radiusStart = 0.0f;
    float radiusEnd = 0.0f;
    float ringHalfWidth = 0.0f;
    Point gapDirection{1.0f, 0.0f};
    float gapHalfAngleRadians = 0.0f;
    int warningTicks = 108;
    int activeTicks = 1;
    int damage = 0;
};

struct Patch {
    Ticket ticket = 0;
    PatchSpec spec{};
    SlotState state = SlotState::Empty;
    int warningRemaining = 0;
    int activeRemaining = 0;
    int fadeRemaining = 0;
    int activeElapsed = 0;
    bool fresh = false;
};

// The active annulus expands by one tick from activeRadius(patch) to
// activeRadius(patch, 1). Render the first value after each authoritative tick.
inline float activeRadius(const Patch& patch, int elapsedOffset = 0) {
    if (patch.spec.shape != Shape::Annulus || patch.spec.activeTicks <= 0) return 0.0f;
    const int elapsed = std::clamp(patch.activeElapsed + elapsedOffset, 0,
                                   patch.spec.activeTicks);
    const float progress = static_cast<float>(elapsed) /
                           static_cast<float>(patch.spec.activeTicks);
    return patch.spec.radiusStart + (patch.spec.radiusEnd - patch.spec.radiusStart) * progress;
}

namespace detail {

inline bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y); }
inline float dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }
inline float lengthSquared(Point p) { return dot(p, p); }
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

inline Bounds ordered(Bounds b) {
    if (b.left > b.right) std::swap(b.left, b.right);
    if (b.top > b.bottom) std::swap(b.top, b.bottom);
    return b;
}

inline bool validBounds(Bounds b) {
    b = ordered(b);
    return std::isfinite(b.left) && std::isfinite(b.right) &&
           std::isfinite(b.top) && std::isfinite(b.bottom) &&
           b.right > b.left && b.bottom > b.top;
}

inline float boundsArea(Bounds b) {
    b = ordered(b);
    if (!validBounds(b)) return 0.0f;
    return (b.right - b.left) * (b.bottom - b.top);
}

inline float clampNonnegative(float value) {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

inline float diskArea(float radius) {
    constexpr float pi = 3.14159265358979323846f;
    radius = clampNonnegative(radius);
    return pi * radius * radius;
}

inline float shapeArea(const PatchSpec& s) {
    constexpr float pi = 3.14159265358979323846f;
    switch (s.shape) {
    case Shape::Disk:
        return diskArea(s.radius);
    case Shape::Capsule: {
        const float r = clampNonnegative(s.halfWidth);
        const Point delta{s.b.x - s.a.x, s.b.y - s.a.y};
        const float span = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        return pi * r * r + 2.0f * r * span;
    }
    case Shape::Annulus: {
        const float radius = std::max(s.radiusStart, s.radiusEnd);
        const float inner = std::max(0.0f, radius - clampNonnegative(s.ringHalfWidth));
        const float outer = radius + clampNonnegative(s.ringHalfWidth);
        const float coveredFraction = std::clamp(1.0f -
            2.0f * std::clamp(s.gapHalfAngleRadians, 0.0f, 1.5707963f) / pi, 0.0f, 1.0f);
        return pi * std::max(0.0f, outer * outer - inner * inner) * coveredFraction;
    }
    }
    return 0.0f;
}

inline float outerRadius(const PatchSpec& s) {
    switch (s.shape) {
    case Shape::Disk: return s.radius;
    case Shape::Capsule: return s.halfWidth;
    case Shape::Annulus: return std::max(s.radiusStart, s.radiusEnd) + s.ringHalfWidth;
    }
    return 0.0f;
}

inline bool validSpec(const PatchSpec& s) {
    if (!s.ownerId || s.warningTicks < 1 || s.activeTicks < 1 || s.damage < 1 ||
        !finite(s.center) || !std::isfinite(s.warningTicks) ||
        !std::isfinite(s.activeTicks) || !std::isfinite(s.damage)) return false;
    switch (s.shape) {
    case Shape::Disk:
        return std::isfinite(s.radius) && s.radius > 0.0f;
    case Shape::Capsule:
        return finite(s.a) && finite(s.b) && std::isfinite(s.halfWidth) && s.halfWidth > 0.0f;
    case Shape::Annulus:
        return std::isfinite(s.radiusStart) && std::isfinite(s.radiusEnd) &&
               std::isfinite(s.ringHalfWidth) && std::isfinite(s.gapHalfAngleRadians) &&
               finite(s.gapDirection) && s.radiusStart >= 0.0f &&
               s.radiusEnd >= 0.0f && s.ringHalfWidth > 0.0f &&
               s.gapHalfAngleRadians >= 0.0f && s.gapHalfAngleRadians < 1.5707963f &&
               (s.gapHalfAngleRadians == 0.0f || lengthSquared(s.gapDirection) > 1.0e-8f);
    }
    return false;
}

inline bool insideBounds(const PatchSpec& s, Bounds b) {
    b = ordered(b);
    const float r = outerRadius(s);
    if (!std::isfinite(r) || r < 0.0f) return false;
    if (s.shape == Shape::Capsule) {
        return s.a.x - r >= b.left && s.a.x + r <= b.right &&
               s.a.y - r >= b.top && s.a.y + r <= b.bottom &&
               s.b.x - r >= b.left && s.b.x + r <= b.right &&
               s.b.y - r >= b.top && s.b.y + r <= b.bottom;
    }
    return s.center.x - r >= b.left && s.center.x + r <= b.right &&
           s.center.y - r >= b.top && s.center.y + r <= b.bottom;
}

inline bool insideBoundsForPool(const PatchSpec& s, Bounds b, PoolKind kind) {
    b = ordered(b);
    if (kind == PoolKind::Boss && s.shape == Shape::Annulus)
        return s.center.x >= b.left && s.center.x <= b.right &&
               s.center.y >= b.top && s.center.y <= b.bottom;
    return insideBounds(s, b);
}

struct PrecisePoint { double x{}, y{}; };
inline double preciseDot(PrecisePoint a, PrecisePoint b) { return a.x*b.x+a.y*b.y; }
inline double preciseCross(PrecisePoint a, PrecisePoint b) { return a.x*b.y-a.y*b.x; }

// Closed annular sectors have only circular arcs and four radial boundary
// segments. Test distance to that actual set; parts of the player's disk in
// the empty center never have to fit inside a gap cone.
inline double annularDistanceSquared(PrecisePoint p, double inner, double outer,
                                     PrecisePoint gap, double halfAngle,
                                     const std::array<PrecisePoint,4>& edges) {
    const double distance=std::hypot(p.x,p.y);
    const bool closedAngle=halfAngle<=0.0 || distance<=1.0e-12 ||
        std::abs(preciseDot(p,gap))<=distance*std::cos(halfAngle);
    double nearest=std::numeric_limits<double>::infinity();
    if(closedAngle){
        const double delta=distance-std::clamp(distance,inner,outer);
        nearest=delta*delta;
    }
    if(halfAngle>0.0)for(const auto& edge:edges){
        const double along=std::clamp(preciseDot(p,edge),inner,outer);
        const PrecisePoint offset{p.x-edge.x*along,p.y-edge.y*along};
        nearest=std::min(nearest,preciseDot(offset,offset));
    }
    return nearest;
}

// Player motion and ring expansion share the same time parameter. Boundary
// contact times are roots of linear/quadratic equations. Between those roots
// contact cannot change, so bounded event intervals replace spatial sampling.
inline bool sweptAnnulus(const PatchSpec& s, Point previousPlayer, Point player,
                          double playerRadius, double ring0, double ring1) {
    const PrecisePoint p{static_cast<double>(previousPlayer.x)-s.center.x,
                         static_cast<double>(previousPlayer.y)-s.center.y};
    const PrecisePoint velocity{static_cast<double>(player.x)-previousPlayer.x,
                                static_cast<double>(player.y)-previousPlayer.y};
    const double gapLength=std::hypot(s.gapDirection.x,s.gapDirection.y);
    const PrecisePoint gap=gapLength>1.0e-12?
        PrecisePoint{s.gapDirection.x/gapLength,s.gapDirection.y/gapLength}:PrecisePoint{1,0};
    const double angle=s.gapHalfAngleRadians, cosine=std::cos(angle), sine=std::sin(angle);
    const PrecisePoint edge0{gap.x*cosine-gap.y*sine,gap.x*sine+gap.y*cosine};
    const PrecisePoint edge1{gap.x*cosine+gap.y*sine,-gap.x*sine+gap.y*cosine};
    const std::array<PrecisePoint,4> edges{{edge0,edge1,{-edge0.x,-edge0.y},{-edge1.x,-edge1.y}}};
    const double radialVelocity=ring1-ring0, width=s.ringHalfWidth;
    constexpr double contactTolerance=1.0e-5; // world units, including tangency
    const double collisionRadius=playerRadius+contactTolerance;
    const double contactSquared=collisionRadius*collisionRadius;
    auto contact=[&](double time){
        const double radius=ring0+radialVelocity*time;
        return annularDistanceSquared({p.x+velocity.x*time,p.y+velocity.y*time},
            std::max(0.0,radius-width),radius+width,gap,angle,edges)<=contactSquared;
    };
    std::array<double,3> boundaries{{0,1,1}};int boundaryCount=2;
    if(std::abs(radialVelocity)>1.0e-12){
        const double zeroInner=(width-ring0)/radialVelocity;
        if(zeroInner>0&&zeroInner<1){boundaries[1]=zeroInner;boundaries[2]=1;boundaryCount=3;}
    }
    for(int interval=0;interval+1<boundaryCount;++interval){
        const double begin=boundaries[interval],end=boundaries[interval+1];
        std::array<double,64> events{};std::size_t count=0;
        auto event=[&](double time){
            if(std::isfinite(time)&&time>=begin-1.0e-9&&time<=end+1.0e-9)
                events[count++]=std::clamp(time,begin,end);
        };
        auto quadratic=[&](double a,double b,double c){
            const double coefficientScale=std::max({1.0,std::abs(a),std::abs(b),std::abs(c)});
            if(std::abs(a)<=1.0e-12*coefficientScale){
                if(std::abs(b)>1.0e-12*coefficientScale)event(-c/b);
                return;
            }
            double discriminant=b*b-4*a*c;
            const double tolerance=1.0e-12*std::max(1.0,b*b+std::abs(4*a*c));
            if(discriminant < -tolerance)return;
            discriminant=std::max(0.0,discriminant);
            const double root=std::sqrt(discriminant);
            const double q=-.5*(b+std::copysign(root,b));
            if(std::abs(q)<=1.0e-24){event(-b/(2*a));return;}
            event(q/a);event(c/q);
        };
        auto radialContact=[&](double radius,double speed){
            quadratic(preciseDot(velocity,velocity)-speed*speed,
                2*(preciseDot(p,velocity)-radius*speed),preciseDot(p,p)-radius*radius);
        };
        const bool innerMoves=ring0+radialVelocity*(begin+end)*.5>width;
        const double inner=innerMoves?ring0-width:0,innerSpeed=innerMoves?radialVelocity:0;
        const double outer=ring0+width;
        event(begin);event(end);
        for(double offset:{-collisionRadius,collisionRadius}){
            radialContact(inner+offset,innerSpeed);
            radialContact(outer+offset,radialVelocity);
        }
        if(angle>0)for(const auto& edge:edges){
            const double sideSpeed=preciseCross(velocity,edge);
            if(std::abs(sideSpeed)>1.0e-12)
                for(double side:{-collisionRadius,collisionRadius})
                    event((side-preciseCross(p,edge))/sideSpeed);
            for(int endpoint=0;endpoint<2;++endpoint){
                const double radius=endpoint?outer:inner,speed=endpoint?radialVelocity:innerSpeed;
                const PrecisePoint offset{p.x-edge.x*radius,p.y-edge.y*radius};
                const PrecisePoint motion{velocity.x-edge.x*speed,velocity.y-edge.y*speed};
                quadratic(preciseDot(motion,motion),2*preciseDot(offset,motion),
                    preciseDot(offset,offset)-collisionRadius*collisionRadius);
            }
        }
        // At most 34 events: 2 endpoints, 8 arc roots, 8 side roots and
        // 16 moving-corner roots. No allocation or unbounded subdivision.
        std::sort(events.begin(),events.begin()+count);
        for(std::size_t i=0;i<count;++i){
            if(contact(events[i]))return true;
            if(i+1<count&&contact((events[i]+events[i+1])*.5))return true;
        }
    }
    return false;
}

inline bool annulusCorridorClear(const PatchSpec& s, const Corridor& c) {
    const float routeRadius = clampNonnegative(c.halfWidth);
    if (!(s.gapHalfAngleRadians > 0.0f)) return false;
    const float inner = std::max(0.0f, std::min(s.radiusStart, s.radiusEnd) - s.ringHalfWidth);
    const float outer = std::max(s.radiusStart, s.radiusEnd) + s.ringHalfWidth;
    const float span = length(sub(c.b, c.a));
    const int samples = std::clamp(static_cast<int>(std::ceil(span)), 1, 16384);
    const Point gap = unitOr(s.gapDirection);

    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const Point p{c.a.x + (c.b.x - c.a.x) * t,
                      c.a.y + (c.b.y - c.a.y) * t};
        const Point q = sub(p, s.center);
        const float distance = length(q);
        if (distance + routeRadius < inner || distance - routeRadius > outer) continue;
        if (distance <= 1.0e-5f) return false;

        // At each radius, the corridor disk covers an angular half-width.
        // Check the widest section of that disk within the ring band.
        const auto apertureAt = [&](float radius) {
            const float denominator = 2.0f * radius * distance;
            if (!(denominator > 0.0f)) return 3.14159265358979323846f;
            const float cosine = (radius * radius + distance * distance -
                                  routeRadius * routeRadius) / denominator;
            if (cosine <= -1.0f) return 3.14159265358979323846f;
            if (cosine >= 1.0f) return 0.0f;
            return std::acos(cosine);
        };
        float aperture = std::max(apertureAt(inner), apertureAt(outer));
        if (distance > routeRadius) {
            const float critical = std::sqrt(distance * distance - routeRadius * routeRadius);
            if (critical > inner && critical < outer)
                aperture = std::max(aperture, apertureAt(critical));
        }
        const float alignment = std::clamp(std::abs(dot(scale(q, 1.0f / distance), gap)),
                                           0.0f, 1.0f);
        const float centerAngle = std::acos(alignment);
        if (centerAngle + aperture > s.gapHalfAngleRadians + 1.0e-4f) return false;
    }
    return true;
}

inline bool conflictsCorridor(const PatchSpec& s, const Corridor& c) {
    if (!c.enabled) return false;
    if (!finite(c.a) || !finite(c.b) || !std::isfinite(c.halfWidth) || c.halfWidth < 0.0f)
        return true;
    const float routeRadius = c.halfWidth;
    switch (s.shape) {
    case Shape::Disk: {
        const float r = routeRadius + s.radius;
        return foundation::segmentDistanceSquared(s.center, c.a, c.b) <= r * r;
    }
    case Shape::Capsule: {
        const float r = routeRadius + s.halfWidth;
        return foundation::segmentsDistanceSquared(c.a, c.b, s.a, s.b) <= r * r;
    }
    case Shape::Annulus: {
        const float inner = std::max(0.0f, std::min(s.radiusStart, s.radiusEnd) - s.ringHalfWidth);
        const float outer = std::max(s.radiusStart, s.radiusEnd) + s.ringHalfWidth;
        const float nearest = std::sqrt(foundation::segmentDistanceSquared(s.center, c.a, c.b));
        const float farthest = std::max(length(sub(c.a, s.center)), length(sub(c.b, s.center)));
        const bool radialOverlap = nearest - routeRadius <= outer &&
                                   farthest + routeRadius >= inner;
        return radialOverlap && !annulusCorridorClear(s, c);
    }
    }
    return true;
}

inline float distanceFromPointToSegment(Point center, Point a, Point b) {
    return std::sqrt(foundation::segmentDistanceSquared(center, a, b));
}

inline Point mapPoint(Point p, Bounds from, Bounds to) {
    from = ordered(from); to = ordered(to);
    const float nx = from.right > from.left ? (p.x - from.left) / (from.right - from.left) : 0.5f;
    const float ny = from.bottom > from.top ? (p.y - from.top) / (from.bottom - from.top) : 0.5f;
    return {to.left + nx * (to.right - to.left), to.top + ny * (to.bottom - to.top)};
}

} // namespace detail

class Pool {
public:
    explicit Pool(PoolKind kind = PoolKind::Ordinary) : kind_(kind) {}

    PoolKind kind() const { return kind_; }

    // Bounds are playable-world coordinates. External area is the other pool's
    // conservative world-area total, so each pool can enforce the shared 18% cap.
    Ticket add(PatchSpec spec, Bounds bounds, Corridor protectedCorridor = {},
               float externalOccupiedArea = 0.0f) {
        bounds = detail::ordered(bounds);
        if (!detail::validSpec(spec) || !detail::validBounds(bounds) ||
            !std::isfinite(externalOccupiedArea) || externalOccupiedArea < 0.0f ||
            !detail::insideBoundsForPool(spec, bounds, kind_) ||
            detail::conflictsCorridor(spec, protectedCorridor) ||
            liveCount() >= capacity() || ownerLiveCount(spec.ownerId) >= ownerCapacity()) return 0;
        const float arenaArea = detail::boundsArea(bounds);
        const float occupied = occupiedAreaInternal() + externalOccupiedArea;
        if (!(arenaArea > 0.0f) || occupied + detail::shapeArea(spec) >
            arenaArea * kMaxOccupiedFraction + 1.0e-3f) return 0;
        const std::size_t slot = emptySlot();
        if (slot == kPoolSlots || nextTicket_ == 0) return 0;
        const Ticket ticket = nextTicket_;
        nextTicket_ = ticket == std::numeric_limits<Ticket>::max() ? 0 : ticket + 1;
        items_[slot] = {ticket, spec, SlotState::Warning, spec.warningTicks,
                        spec.activeTicks, 0, 0, true};
        return ticket;
    }

    // Only the first 30 warning ticks may move a disk to follow the player's
    // short predicted route. A rejected point leaves the previous valid point.
    bool retargetWarning(Ticket ticket, Point center, Bounds bounds,
                         Corridor protectedCorridor = {},
                         float externalOccupiedArea = 0.0f) {
        Patch* patch = find(ticket);
        if (!patch || patch->state != SlotState::Warning ||
            patch->warningRemaining <= kSoftAimTicksAfterWarning() ||
            !detail::finite(center) || !std::isfinite(externalOccupiedArea) ||
            externalOccupiedArea < 0.0f) return false;
        PatchSpec candidate = patch->spec;
        const Point offset{center.x - candidate.center.x, center.y - candidate.center.y};
        candidate.center = center;
        if (candidate.shape == Shape::Capsule) {
            candidate.a = detail::add(candidate.a, offset);
            candidate.b = detail::add(candidate.b, offset);
        }
        bounds = detail::ordered(bounds);
        if (!detail::validBounds(bounds) || !detail::insideBounds(candidate, bounds) ||
            detail::conflictsCorridor(candidate, protectedCorridor) ||
            occupiedAreaInternal() + externalOccupiedArea >
                detail::boundsArea(bounds) * kMaxOccupiedFraction + 1.0e-3f) return false;
        patch->spec = candidate;
        return true;
    }

    // Call once per authoritative simulation update. Newly created warnings
    // skip the update in which the pool first observes them.
    combat::IncomingHit tick(Point previousPlayer, Point player,
                             float playerRadius, bool active = true) {
        combat::IncomingHit hit;
        if (!active) return hit;
        playerRadius = detail::clampNonnegative(playerRadius);
        for (Patch& patch : items_) {
            if (patch.state == SlotState::Empty) continue;
            if (patch.fresh) { patch.fresh = false; continue; }
            if (patch.state == SlotState::Warning) {
                if (patch.warningRemaining <= 1) {
                    patch.warningRemaining = 0;
                    patch.state = SlotState::Active;
                    patch.activeElapsed = 0;
                } else {
                    --patch.warningRemaining;
                }
                continue;
            }
            if (patch.state == SlotState::Fading) {
                if (--patch.fadeRemaining <= 0) patch = {};
                continue;
            }

            if (patch.state == SlotState::Active && collides(patch, previousPlayer, player, playerRadius))
                hit.consider(patch.spec.damage, patch.spec.ownerId);
            ++patch.activeElapsed;
            if (--patch.activeRemaining <= 0) patch = {};
        }
        return hit;
    }

    void cancelOwner(std::uint64_t ownerId) {
        if (!ownerId) return;
        for (Patch& patch : items_) {
            if (patch.spec.ownerId != ownerId) continue;
            cancelPatch(patch);
        }
    }

    // Conflicting warnings disappear. Active geometry becomes harmless and is
    // retained for a short, visible fade so the displayed route remains clear.
    void cancelUnsafeCorridors(Corridor corridor) {
        if (!corridor.enabled) return;
        for (Patch& patch : items_) {
            if ((patch.state == SlotState::Warning || patch.state == SlotState::Active) &&
                detail::conflictsCorridor(patch.spec, corridor)) cancelPatch(patch);
        }
    }

    void clear() { items_ = {}; }

    const std::array<Patch, kPoolSlots>& items() const { return items_; }

    int count(SlotState state) const {
        int result = 0;
        for (const Patch& patch : items_) if (patch.state == state) ++result;
        return result;
    }

    int liveCount(std::uint64_t ownerId = 0) const {
        int result = 0;
        for (const Patch& patch : items_)
            if ((patch.state == SlotState::Warning || patch.state == SlotState::Active) &&
                (!ownerId || patch.spec.ownerId == ownerId)) ++result;
        return result;
    }

    int occupiedSlotCount() const {
        int result = 0;
        for (const Patch& patch : items_) if (patch.state != SlotState::Empty) ++result;
        return result;
    }

    // Annulus area is the full maximum instantaneous band minus its two safe
    // sectors. Edge clipping is intentionally overestimated for a firm cap.
    float areaOccupied(Bounds bounds, float externalOccupiedArea = 0.0f) const {
        if (!detail::validBounds(bounds) || !std::isfinite(externalOccupiedArea) ||
            externalOccupiedArea < 0.0f) return std::numeric_limits<float>::infinity();
        return occupiedAreaInternal() + externalOccupiedArea;
    }

    void remap(Bounds from, Bounds to, float externalOccupiedArea = 0.0f) {
        from = detail::ordered(from); to = detail::ordered(to);
        if (!detail::validBounds(from) || !detail::validBounds(to) ||
            !std::isfinite(externalOccupiedArea) || externalOccupiedArea < 0.0f) return;
        for (Patch& patch : items_) {
            if (patch.state == SlotState::Empty) continue;
            PatchSpec& s = patch.spec;
            const Point oldCenter = s.center;
            s.center = detail::mapPoint(s.center, from, to);
            s.a = detail::mapPoint(s.a, from, to);
            s.b = detail::mapPoint(s.b, from, to);
            if (s.shape == Shape::Annulus) {
                const Point oldEnd{oldCenter.x + s.gapDirection.x,
                                   oldCenter.y + s.gapDirection.y};
                const Point mappedEnd = detail::mapPoint(oldEnd, from, to);
                s.gapDirection = detail::unitOr(detail::sub(mappedEnd, s.center), s.gapDirection);
            }
        }
        const float maximumArea = detail::boundsArea(to) * kMaxOccupiedFraction;
        while (occupiedAreaInternal() + externalOccupiedArea > maximumArea + 1.0e-3f) {
            Patch* newest = nullptr;
            for (Patch& patch : items_)
                if ((patch.state == SlotState::Warning || patch.state == SlotState::Active) &&
                    (!newest || patch.ticket > newest->ticket)) newest = &patch;
            if (!newest) break;
            cancelPatch(*newest);
        }
    }

private:
    static constexpr int kSoftAimTicksAfterWarning() { return 78; }

    std::size_t emptySlot() const {
        for (std::size_t i = 0; i < items_.size(); ++i)
            if (items_[i].state == SlotState::Empty) return i;
        return items_.size();
    }

    int capacity() const {
        return kind_ == PoolKind::Boss ? kBossPoolCapacity : kOrdinaryPoolCapacity;
    }

    int ownerCapacity() const {
        return kind_ == PoolKind::Boss ? kBossOwnerCapacity : kOrdinaryOwnerCapacity;
    }

    int ownerLiveCount(std::uint64_t ownerId) const { return liveCount(ownerId); }

    Patch* find(Ticket ticket) {
        for (Patch& patch : items_)
            if (patch.state != SlotState::Empty && patch.ticket == ticket) return &patch;
        return nullptr;
    }

    void cancelPatch(Patch& patch) {
        if (patch.state == SlotState::Warning) {
            patch = {};
        } else if (patch.state == SlotState::Active) {
            patch.state = SlotState::Fading;
            patch.fadeRemaining = kHarmlessFadeTicks;
            patch.fresh = true;
        }
    }

    float occupiedAreaInternal() const {
        float result = 0.0f;
        for (const Patch& patch : items_)
            if (patch.state == SlotState::Warning || patch.state == SlotState::Active)
                result += detail::shapeArea(patch.spec);
        return result;
    }

    static bool collides(const Patch& patch, Point previousPlayer,
                         Point player, float playerRadius) {
        if (!detail::finite(previousPlayer) || !detail::finite(player)) return false;
        const PatchSpec& s = patch.spec;
        switch (s.shape) {
        case Shape::Disk: {
            const float r = playerRadius + s.radius;
            return foundation::segmentDistanceSquared(s.center, previousPlayer, player) <= r * r;
        }
        case Shape::Capsule: {
            const float r = playerRadius + s.halfWidth;
            return foundation::segmentsDistanceSquared(previousPlayer, player, s.a, s.b) <= r * r;
        }
        case Shape::Annulus: {
            const float ring0 = activeRadius(patch);
            const float ring1 = activeRadius(patch, 1);
            const detail::PrecisePoint start{static_cast<double>(previousPlayer.x)-s.center.x,
                                              static_cast<double>(previousPlayer.y)-s.center.y};
            const detail::PrecisePoint finish{static_cast<double>(player.x)-s.center.x,
                                               static_cast<double>(player.y)-s.center.y};
            const detail::PrecisePoint delta{finish.x-start.x,finish.y-start.y};
            const double squaredSpeed=detail::preciseDot(delta,delta);
            const double closest=squaredSpeed>0?
                std::clamp(-detail::preciseDot(start,delta)/squaredSpeed,0.0,1.0):0;
            const detail::PrecisePoint nearPoint{start.x+closest*delta.x,start.y+closest*delta.y};
            const double farthestSquared=std::max(detail::preciseDot(start,start),detail::preciseDot(finish,finish));
            const double inner=std::max(0.0,static_cast<double>(std::min(ring0,ring1))-s.ringHalfWidth-playerRadius-1.0e-5);
            const double outer=static_cast<double>(std::max(ring0,ring1))+s.ringHalfWidth+playerRadius+1.0e-5;
            if(detail::preciseDot(nearPoint,nearPoint)>outer*outer||farthestSquared<inner*inner)return false;
            return detail::sweptAnnulus(s,previousPlayer,player,playerRadius,ring0,ring1);
        }
        }
        return false;
    }

    PoolKind kind_;
    std::array<Patch, kPoolSlots> items_{};
    Ticket nextTicket_ = 1;
};

} // namespace threadlight::area_hazards
