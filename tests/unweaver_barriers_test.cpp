#include "unweaver_barriers.h"
#include <cmath>
#include <iostream>

namespace barriers = threadlight::unweaver::barriers;
using Point = barriers::Point;

namespace {
int failures = 0;
int checks = 0;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        std::cerr << "barrier check failed at " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
    } \
} while (false)

bool near(float a, float b, float epsilon = .05f) {
    return std::abs(a - b) <= epsilon;
}

void authoredPlanHasWarningsMixedWallsAndEscapeGaps() {
    constexpr barriers::Bounds arena{12.f, 912.f, 12.f, 496.f};
    barriers::State state;
    barriers::plan(state, arena, {200.f, 250.f}, {720.f, 250.f});
    CHECK(state.count == 6);
    int spikes = 0;
    for (int i = 0; i < state.count; ++i) {
        const auto& wall = state.walls[i];
        CHECK(wall.halfwidth > 0.f);
        CHECK(wall.warning == barriers::kWarningTicks);
        CHECK(wall.remaining == barriers::kLifetimeTicks);
        CHECK(wall.fresh == false);
        spikes += wall.spiked ? 1 : 0;
        CHECK(wall.a.x == wall.b.x); // This plan follows horizontal travel.
        CHECK(wall.a.y >= arena.top && wall.a.y <= arena.bottom);
        CHECK(wall.b.y >= arena.top && wall.b.y <= arena.bottom);
    }
    CHECK(spikes == 3);
    // Each line has an authored center lane so pre-kill plans never seal the arena.
    for (int line = 0; line < 3; ++line) {
        const auto& upper = state.walls[line * 2];
        const auto& lower = state.walls[line * 2 + 1];
        CHECK(lower.a.y - upper.b.y >= 100.f);
        CHECK(upper.a.y == arena.top + 16.f);
        CHECK(lower.b.y == arena.bottom - 16.f);
    }

    barriers::State lethal;
    barriers::plan(lethal, arena, {200.f, 250.f}, {720.f, 250.f}, true);
    CHECK(lethal.count == barriers::kWallCount);
    CHECK(lethal.walls[1].a.y - lethal.walls[0].b.y <
          state.walls[1].a.y - state.walls[0].b.y);
    CHECK(lethal.walls[1].a.y > lethal.walls[0].b.y);
}

barriers::State oneWall(Point a, Point b, float width, bool spiked = false,
                        int warning = 0, int remaining = 20, bool fresh = false) {
    barriers::State state;
    state.count = 1;
    state.walls[0] = {a, b, width, warning, remaining, spiked, fresh};
    return state;
}

void sweptSolidsBlockAndAllowSliding() {
    const auto vertical = oneWall({100.f, 0.f}, {100.f, 100.f}, 6.f);
    const Point crossed = barriers::move(vertical, {0.f, 50.f}, {200.f, 50.f}, 5.f);
    CHECK(crossed.x <= 89.f && crossed.x >= 88.f);
    CHECK(near(crossed.y, 50.f)); // A fast dash cannot tunnel through the wall.

    const Point diagonal = barriers::move(vertical, {0.f, 40.f}, {200.f, 150.f}, 5.f);
    CHECK(diagonal.x <= 89.f && diagonal.x >= 88.f);
    CHECK(near(diagonal.y, 150.f)); // Tangential motion slides along the wall.

    const Point parallel = barriers::move(vertical, {89.f, 20.f}, {89.f, 80.f}, 5.f);
    CHECK(near(parallel.x, 89.f) && near(parallel.y, 80.f));
    const Point clearEndpoint = barriers::move(vertical, {80.f, -20.f}, {120.f, -20.f}, 5.f);
    CHECK(near(clearEndpoint.x, 120.f) && near(clearEndpoint.y, -20.f));
    const Point endpointHit = barriers::move(vertical, {80.f, -4.f}, {120.f, 6.f}, 5.f);
    CHECK(endpointHit.x <= 89.f && endpointHit.x >= 88.f);

    const auto horizontal = oneWall({0.f, 100.f}, {100.f, 100.f}, 6.f);
    const Point horizontalCross = barriers::move(horizontal, {50.f, 0.f}, {50.f, 200.f}, 5.f);
    CHECK(horizontalCross.y <= 89.f && horizontalCross.y >= 88.f);
    CHECK(near(horizontalCross.x, 50.f));
}

void warningFreshSpikeContactAndExpiry() {
    auto spike = oneWall({100.f, 0.f}, {100.f, 100.f}, 6.f, true, 2, 2);
    const Point previous{0.f, 50.f}, next{200.f, 50.f};
    CHECK(barriers::damage(spike, previous, next, 5.f) == 0);
    CHECK(near(barriers::move(spike, previous, next, 5.f).x, 200.f));
    barriers::tick(spike);
    CHECK(spike.walls[0].warning == 1 && !spike.walls[0].fresh);
    CHECK(barriers::damage(spike, previous, next, 5.f) == 0);
    barriers::tick(spike);
    CHECK(spike.walls[0].warning == 0 && spike.walls[0].fresh);
    CHECK(barriers::damage(spike, previous, next, 5.f) == 0); // Activation tick is harmless.
    CHECK(near(barriers::move(spike, previous, next, 5.f).x, 200.f)); // Spike is traversable.
    const Point frozenA = spike.walls[0].a, frozenB = spike.walls[0].b;
    barriers::tick(spike);
    CHECK(!spike.walls[0].fresh);
    CHECK(spike.walls[0].a.x == frozenA.x && spike.walls[0].a.y == frozenA.y);
    CHECK(spike.walls[0].b.x == frozenB.x && spike.walls[0].b.y == frozenB.y);
    CHECK(barriers::damage(spike, previous, next, 5.f) == barriers::kSpikeDamage);
    CHECK(spike.walls[0].remaining == 1);
    barriers::tick(spike);
    CHECK(spike.walls[0].remaining == 0 && spike.walls[0].halfwidth == 0.f);
    CHECK(barriers::damage(spike, previous, next, 5.f) == 0);

    auto solid = oneWall({100.f, 0.f}, {100.f, 100.f}, 6.f, false, 0, 1);
    CHECK(barriers::damage(solid, previous, next, 5.f) == 0);
    CHECK(barriers::move(solid, previous, next, 5.f).x < 100.f);
    barriers::tick(solid);
    CHECK(near(barriers::move(solid, previous, next, 5.f).x, 200.f));
}

void clearAndInvalidPlanAreSafe() {
    const auto obstacle = oneWall({100.f, 0.f}, {100.f, 100.f}, 6.f);
    const Point idle = barriers::navigate(obstacle, {20.f, 50.f}, {20.f, 50.f}, {200.f, 50.f}, 5.f, 2.f);
    CHECK(near(idle.x, 20.f) && near(idle.y, 50.f));
    barriers::State state;
    barriers::plan(state, {12.f, 912.f, 12.f, 496.f}, {200.f, 250.f}, {720.f, 250.f});
    CHECK(state.count == 6);
    barriers::clear(state);
    CHECK(state.count == 0);
    for (const auto& wall : state.walls) CHECK(wall.remaining == 0 && wall.warning == 0);
    barriers::plan(state, {4.f, 20.f, 4.f, 20.f}, {8.f, 8.f}, {18.f, 18.f});
    CHECK(state.count == 0);
    const Point p = barriers::move(state, {10.f, 10.f}, {100.f, 100.f}, 12.f);
    CHECK(near(p.x, 100.f) && near(p.y, 100.f));
}
}

int main() {
    authoredPlanHasWarningsMixedWallsAndEscapeGaps();
    sweptSolidsBlockAndAllowSliding();
    warningFreshSpikeContactAndExpiry();
    clearAndInvalidPlanAreSafe();
    if (failures) {
        std::cerr << failures << " / " << checks << " barrier checks failed\n";
        return 1;
    }
    std::cout << "Unweaver barrier checks passed: " << checks << " checks\n";
    return 0;
}
