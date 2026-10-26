#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <span>

namespace threadlight::silk {

// Squared-distance rejection avoids a square root except extremely close to
// the old strict boundary, where sqrt rounding can change the result.
template<class Point>
bool shouldAppendKnot(Point last, Point player) {
    const float dx=last.x-player.x,dy=last.y-player.y;
    const float squared=dx*dx+dy*dy;
    if(squared<=80.99f)return false;
    if(squared>=81.01f)return true;
    return std::sqrt(squared)>9.f;
}

// Knots are appended at age zero and all age together, so their ages are
// non-increasing. Segment i uses knot i's age and joins knots i-1 and i.
// Return size() when there are no active segments; preserve oldest-first order.
template<class Knot>
std::size_t firstActiveSegment(std::span<const Knot> knots, float activeSeconds) {
    if (knots.size() < 2) return knots.size();
    const auto first = std::partition_point(knots.begin() + 1, knots.end(),
        [activeSeconds](const Knot& knot) { return knot.age > activeSeconds; });
    return static_cast<std::size_t>(first - knots.begin());
}

} // namespace threadlight::silk
