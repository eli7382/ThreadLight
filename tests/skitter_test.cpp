#include "game_timing.h"
#include "skitter.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using threadlight::foundation::Point;
using namespace threadlight::skitter;

bool close(float a, float b, float epsilon = 0.001f) {
    return std::abs(a - b) <= epsilon;
}

bool close(Point a, Point b, float epsilon = 0.001f) {
    return close(a.x, b.x, epsilon) && close(a.y, b.y, epsilon);
}

bool finite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

float speed(Point velocity) {
    return std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
}

void testExactPhaseDurationsAndCycle() {
    State state;
    Point position{0.0f, 90.0f};
    Point velocity{10.0f, -3.0f};
    const Point player{0.0f, 0.0f};
    const Point playerMotion{1.0f, 0.0f};
    const Bounds bounds{-500.0f, 500.0f, -500.0f, 500.0f};

    CHECK(state.phase == Phase::Flank);
    CHECK(state.ticksRemaining == kFlankTicks);
    CHECK(!committed(state));

    for (int elapsed = 1; elapsed <= kFlankTicks; ++elapsed) {
        const auto result = tick(state, position, velocity, player,
                                 playerMotion, bounds);
        CHECK(!result.burstStarted);
        CHECK(!result.recovered);
        CHECK(result.telegraphStarted == (elapsed == kFlankTicks));
        if (elapsed < kFlankTicks) {
            CHECK(state.phase == Phase::Flank);
            CHECK(state.ticksRemaining == kFlankTicks - elapsed);
        }
    }
    CHECK(state.phase == Phase::Windup);
    CHECK(state.ticksRemaining == kWindupTicks);
    CHECK(committed(state));
    CHECK(close(position, {0.0f, 90.0f}));
    CHECK(close(velocity, {}));

    for (int elapsed = 1; elapsed <= kWindupTicks; ++elapsed) {
        const auto result = tick(state, position, velocity, player,
                                 playerMotion, bounds);
        CHECK(!result.telegraphStarted);
        CHECK(!result.recovered);
        CHECK(result.burstStarted == (elapsed == kWindupTicks));
        CHECK(close(position, {0.0f, 90.0f}));
        CHECK(close(velocity, {}));
        if (elapsed < kWindupTicks) {
            CHECK(state.phase == Phase::Windup);
            CHECK(state.ticksRemaining == kWindupTicks - elapsed);
        }
    }
    CHECK(state.phase == Phase::Burst);
    CHECK(state.ticksRemaining == kBurstTicks);

    for (int elapsed = 1; elapsed < kBurstTicks; ++elapsed) {
        const auto result = tick(state, position, velocity, player,
                                 playerMotion, bounds);
        CHECK(!result.telegraphStarted);
        CHECK(!result.burstStarted);
        CHECK(!result.recovered);
        CHECK(state.phase == Phase::Burst);
        CHECK(state.ticksRemaining == kBurstTicks - elapsed);
    }
    const auto burstEnd = tick(state, position, velocity, player,
                               playerMotion, bounds);
    CHECK(!burstEnd.telegraphStarted);
    CHECK(!burstEnd.burstStarted);
    CHECK(!burstEnd.recovered);
    CHECK(state.phase == Phase::Recover);
    CHECK(state.ticksRemaining == kRecoverTicks);

    for (int elapsed = 1; elapsed <= kRecoverTicks; ++elapsed) {
        const Point before = position;
        const auto result = tick(state, position, velocity, player,
                                 playerMotion, bounds);
        CHECK(!result.telegraphStarted);
        CHECK(!result.burstStarted);
        CHECK(result.recovered == (elapsed == kRecoverTicks));
        CHECK(close(position, before));
        CHECK(close(velocity, {}));
        if (elapsed < kRecoverTicks) {
            CHECK(state.phase == Phase::Recover);
            CHECK(state.ticksRemaining == kRecoverTicks - elapsed);
        }
    }
    CHECK(state.phase == Phase::Flank);
    CHECK(state.ticksRemaining == kFlankTicks);
    CHECK(!committed(state));

    CHECK(kFlankTicks + kWindupTicks + kBurstTicks + kRecoverTicks == 456);
}

void testFlankOffsetSidesAndSilkCompatibleSpeed() {
    const Bounds bounds{-1000.0f, 1000.0f, -1000.0f, 1000.0f};
    const Point player{100.0f, 100.0f};
    const Point playerMotion{1.0f, 0.0f};

    for (const int side : {-1, 1}) {
        State state;
        state.side = side;
        Point position{0.0f, side > 0 ? 190.0f : 10.0f};
        Point velocity{};
        const float targetY = side > 0 ? 190.0f : 10.0f;

        for (int step = 0; step < 120; ++step) {
            const Point previous = position;
            const auto result = tick(state, position, velocity, player,
                                     playerMotion, bounds);
            CHECK(!result.telegraphStarted);
            CHECK(!result.burstStarted);
            CHECK(!result.recovered);
            CHECK(finite(position));
            CHECK(finite(velocity));
            CHECK(speed(velocity) <= kFlankSpeed + 0.001f);
            CHECK(speed({position.x - previous.x, position.y - previous.y}) <=
                  kFlankSpeed * static_cast<float>(threadlight::timing::kFixedStepSeconds) +
                      0.001f);
        }

        CHECK(close(position, {100.0f, targetY}));
        CHECK(state.phase == Phase::Flank);
        CHECK(state.ticksRemaining == kFlankTicks - 120);
    }
}

void testCommitGateLockedLaneAndBounds() {
    State state;
    state.ticksRemaining = 1;
    Point position{50.0f, 40.0f};
    Point velocity{};
    Point player{50.0f, 0.0f};
    Point playerMotion{1.0f, 0.0f};
    const Bounds bounds{0.0f, 100.0f, 0.0f, 100.0f};

    const auto denied = tick(state, position, velocity, player, playerMotion,
                             bounds, false);
    CHECK(!denied.telegraphStarted);
    CHECK(!denied.burstStarted);
    CHECK(!committed(state));
    CHECK(state.phase == Phase::Flank);
    CHECK(state.ticksRemaining == 1);

    const auto warning = tick(state, position, velocity, player, playerMotion,
                              bounds, true);
    CHECK(warning.telegraphStarted);
    CHECK(!warning.burstStarted);
    CHECK(state.phase == Phase::Windup);
    CHECK(state.ticksRemaining == kWindupTicks);
    CHECK(committed(state));
    CHECK(close(state.burstStart, position));
    CHECK(close(state.lockedDirection, {0.0f, -1.0f}));
    CHECK(close(state.burstEnd, {position.x, bounds.top}));
    CHECK(state.burstEnd.x >= bounds.left && state.burstEnd.x <= bounds.right);
    CHECK(state.burstEnd.y >= bounds.top && state.burstEnd.y <= bounds.bottom);

    const Point lockedStart = state.burstStart;
    const Point lockedEnd = state.burstEnd;
    for (int step = 0; step < kWindupTicks; ++step) {
        player = {95.0f, 85.0f};
        playerMotion = {-1.0f, 1.0f};
        const auto result = tick(state, position, velocity, player,
                                 playerMotion, bounds);
        CHECK(!result.telegraphStarted);
        CHECK(!result.recovered);
        CHECK(close(position, lockedStart));
        CHECK(close(velocity, {}));
        CHECK(close(state.burstStart, lockedStart));
        CHECK(close(state.burstEnd, lockedEnd));
        CHECK(close(state.lockedDirection, {0.0f, -1.0f}));
        if (step + 1 < kWindupTicks) CHECK(!result.burstStarted);
        else CHECK(result.burstStarted);
    }

    CHECK(committed(state));
    CHECK(state.phase == Phase::Burst);
    for (int step = 0; step < kBurstTicks; ++step) {
        const Point previous = position;
        player = {5.0f, 95.0f};
        playerMotion = {0.0f, -1.0f};
        const auto result = tick(state, position, velocity, player,
                                 playerMotion, bounds);
        CHECK(!result.telegraphStarted);
        CHECK(!result.burstStarted);
        CHECK(!result.recovered);
        CHECK(finite(position));
        CHECK(finite(velocity));
        CHECK(position.x >= bounds.left && position.x <= bounds.right);
        CHECK(position.y >= bounds.top && position.y <= bounds.bottom);
        CHECK(close(position.x, lockedStart.x));
        CHECK(position.y <= previous.y + 0.001f);
        CHECK(speed(velocity) <= kBurstSpeed + 0.001f);
        CHECK(speed({position.x - previous.x, position.y - previous.y}) <=
              kBurstSpeed * static_cast<float>(threadlight::timing::kFixedStepSeconds) +
                  0.001f);
    }
    CHECK(close(position, lockedEnd));
    CHECK(state.phase == Phase::Recover);
    CHECK(!committed(state));
}

void testZeroAimDegenerateAndReversedBoundsStayFinite() {
    State state;
    Point position{50.0f, -10.0f};
    Point velocity{2.0f, 3.0f};
    const Point player{3.0f, 5.0f};
    const Point playerMotion{};
    const Bounds reversed{5.0f, 1.0f, 8.0f, 2.0f};

    const auto result = tick(state, position, velocity, player, playerMotion,
                             reversed, true);
    CHECK(!result.telegraphStarted);
    CHECK(finite(position));
    CHECK(finite(velocity));
    CHECK(position.x >= 1.0f && position.x <= 5.0f);
    CHECK(position.y >= 2.0f && position.y <= 8.0f);

    State pointState;
    Point pointPosition{100.0f, -20.0f};
    Point pointVelocity{};
    const Point pointPlayer{5.0f, 5.0f};
    const Bounds pointBounds{5.0f, 5.0f, 5.0f, 5.0f};
    for (int step = 0; step < kFlankTicks; ++step) {
        const auto pointResult = tick(pointState, pointPosition, pointVelocity,
                                      pointPlayer, {}, pointBounds, true);
        CHECK(!pointResult.burstStarted);
        CHECK(finite(pointPosition));
        CHECK(finite(pointVelocity));
        CHECK(close(pointPosition, {5.0f, 5.0f}));
    }
    CHECK(pointState.phase == Phase::Windup);
    CHECK(close(pointState.burstStart, {5.0f, 5.0f}));
    CHECK(close(pointState.burstEnd, {5.0f, 5.0f}));
    CHECK(finite(pointState.lockedDirection));
}

struct ScheduleResult {
    int ticks{};
    State state{};
    Point position{};
    Point velocity{};
    std::vector<int> telegraphs;
    std::vector<int> bursts;
    std::vector<int> recoveries;
};

ScheduleResult simulateAtRenderRate(double renderFps) {
    using threadlight::timing::FixedStepClock;
    constexpr double duration = 10.0;
    const int frameCount = static_cast<int>(std::lround(renderFps * duration));
    FixedStepClock clock;
    ScheduleResult result;
    result.position = {450.0f, 350.0f};
    const Point player{120.0f, 220.0f};
    const Point playerMotion{1.0f, 0.0f};
    const Bounds bounds{0.0f, 800.0f, 0.0f, 600.0f};

    for (int frame = 0; frame < frameCount; ++frame) {
        const int steps = clock.advance(1.0 / renderFps);
        for (int step = 0; step < steps; ++step) {
            ++result.ticks;
            const auto event = tick(result.state, result.position, result.velocity,
                                    player, playerMotion, bounds);
            if (event.telegraphStarted) result.telegraphs.push_back(result.ticks);
            if (event.burstStarted) result.bursts.push_back(result.ticks);
            if (event.recovered) result.recoveries.push_back(result.ticks);
        }
    }
    return result;
}

void testFixedStepScheduleAtConfiguredRenderRates() {
    const ScheduleResult baseline = simulateAtRenderRate(30.0);
    CHECK(baseline.ticks == 1200);
    CHECK((baseline.telegraphs == std::vector<int>{255, 711, 1167}));
    CHECK((baseline.bursts == std::vector<int>{339, 795}));
    CHECK((baseline.recoveries == std::vector<int>{456, 912}));

    for (const double fps : {60.0, 120.0, 144.0, 165.0, 1000.0}) {
        const ScheduleResult result = simulateAtRenderRate(fps);
        CHECK(result.ticks == baseline.ticks);
        CHECK(result.telegraphs == baseline.telegraphs);
        CHECK(result.bursts == baseline.bursts);
        CHECK(result.recoveries == baseline.recoveries);
        CHECK(result.state.phase == baseline.state.phase);
        CHECK(result.state.ticksRemaining == baseline.state.ticksRemaining);
        CHECK(close(result.position, baseline.position));
        CHECK(close(result.velocity, baseline.velocity));
        CHECK(close(result.state.burstStart, baseline.state.burstStart));
        CHECK(close(result.state.burstEnd, baseline.state.burstEnd));
        CHECK(close(result.state.lockedDirection, baseline.state.lockedDirection));
    }
}

} // namespace

int main() {
    testExactPhaseDurationsAndCycle();
    testFlankOffsetSidesAndSilkCompatibleSpeed();
    testCommitGateLockedLaneAndBounds();
    testZeroAimDegenerateAndReversedBoundsStayFinite();
    testFixedStepScheduleAtConfiguredRenderRates();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Skitter state machine checks passed.\n";
    return EXIT_SUCCESS;
}
