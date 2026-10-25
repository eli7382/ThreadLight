#pragma once

#include <algorithm>
#include <cmath>

namespace threadlight::foundation {

struct Point { float x{}, y{}; };

inline float segmentDistanceSquared(Point point, Point a, Point b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float t = lengthSquared > 0 ? std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared, 0.f, 1.f) : 0.f;
    const float x = a.x + t * dx - point.x, y = a.y + t * dy - point.y;
    return x * x + y * y;
}

inline float segmentsDistanceSquared(Point a, Point b, Point c, Point d) {
    const Point r{b.x - a.x, b.y - a.y}, s{d.x - c.x, d.y - c.y};
    const Point delta{c.x - a.x, c.y - a.y};
    const float denominator = r.x * s.y - r.y * s.x;
    if (std::abs(denominator) > 1.e-6f) {
        const float t = (delta.x * s.y - delta.y * s.x) / denominator;
        const float u = (delta.x * r.y - delta.y * r.x) / denominator;
        if (t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f) return 0.f;
    }
    return std::min({segmentDistanceSquared(a, c, d), segmentDistanceSquared(b, c, d),
                     segmentDistanceSquared(c, a, b), segmentDistanceSquared(d, a, b)});
}

} // namespace threadlight::foundation
