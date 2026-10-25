#include "bulwark.h"
#include "crossweave.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using threadlight::foundation::Point;
namespace cw = threadlight::crossweave;
namespace bw = threadlight::bulwark;

cw::Segment vertical(std::uint64_t id, int age, float x = 0.0f,
                     float y0 = -10.0f, float y1 = 10.0f) {
    return {id, {x, y0}, {x, y1}, age};
}

void ageWindowAndPulseCenter() {
    cw::State state;
    const std::array<cw::Segment, 4> segments{{
        vertical(1, 71), vertical(2, 72), vertical(3, 420), vertical(4, 421)
    }};
    // Cross at y=-20 for the travel setup, then cross eligible knots at y=0.
    cw::tick(state, {-80.0f, -20.0f}, {80.0f, -20.0f}, {}, true, true);
    cw::tick(state, {80.0f, -20.0f}, {80.0f, 0.0f}, {}, true, true);
    const cw::Pulse pulse = cw::tick(state, {80.0f, 0.0f}, {-80.0f, 0.0f}, segments, true, true);
    CHECK(pulse.triggered);
    CHECK(pulse.center.x == 0.0f && pulse.center.y == 0.0f);
    CHECK(state.consumedCount == 2);
    CHECK(cw::claimed(state, 2) && cw::claimed(state, 3));
    CHECK(!cw::claimed(state, 1) && !cw::claimed(state, 4));
    CHECK(state.cooldownTicks == cw::kCooldownTicks);
    CHECK(state.travelDistance == 0.0f);
}

void strictCrossingsAndEndpointJitter() {
    cw::State state;
    state.travelDistance = cw::kTravelDistance;
    const std::array<cw::Segment, 1> verticalKnot{{{10, {-10.0f, 0.0f}, {10.0f, 0.0f}, 100}}};

    // Collinear movement is not a proper crossing.
    const cw::Pulse collinear = cw::tick(state, {-5.0f, 0.0f}, {5.0f, 0.0f},
                                         verticalKnot, true, true);
    CHECK(!collinear.triggered && !cw::claimed(state, 10));

    // Touching the knot endpoint, including a sub-inset jitter past it, does
    // not claim it; a clear interior crossing does.
    const cw::Segment endpointKnot{11, {0.0f, 0.0f}, {10.0f, 0.0f}, 100};
    const std::array<cw::Segment, 1> endpointSpan{{endpointKnot}};
    const cw::Pulse endpointTouch = cw::tick(state, {-5.0f, -5.0f}, {0.0f, 0.0f},
                                             endpointSpan, true, true);
    CHECK(!endpointTouch.triggered && !cw::claimed(state, 11));
    state.travelDistance = cw::kTravelDistance;
    const cw::Pulse jitter = cw::tick(state, {-5.0f, -0.02f}, {5.0f, 0.02f},
                                      endpointSpan, true, true);
    CHECK(!jitter.triggered && !cw::claimed(state, 11));

    state.travelDistance = cw::kTravelDistance;
    const cw::Pulse interior = cw::tick(state, {5.0f, -5.0f}, {5.0f, 5.0f},
                                        endpointSpan, true, true);
    CHECK(interior.triggered);
    CHECK(interior.center.x == 5.0f && interior.center.y == 0.0f);
}

void exactBoundaryCrossingAndPauseFreeze() {
    cw::State state;
    state.travelDistance = cw::kTravelDistance;
    const std::array<cw::Segment, 1> knot{{{12, {-10.0f, 0.0f}, {10.0f, 0.0f}, 100}}};

    const cw::Pulse touch = cw::tick(state, {0.0f, -2.125f}, {0.0f, 0.0f}, knot, true, true);
    CHECK(!touch.triggered && !cw::claimed(state, 12));
    CHECK(state.pendingCount == 1);
    CHECK(state.pendingCrossings[0].id == 12);
    const Point heldPoint = state.pendingCrossings[0].lastOnLine;
    const float heldTravel = state.travelDistance;
    const int heldCooldown = state.cooldownTicks;

    // A paused frame that would have crossed the line must freeze the pending
    // side, cooldown, and travel until combat resumes.
    const cw::Pulse paused = cw::tick(state, {0.0f, 0.0f}, {0.0f, 2.125f}, knot, false, true);
    CHECK(!paused.triggered && state.pendingCount == 1);
    CHECK(state.pendingCrossings[0].lastOnLine.x == heldPoint.x &&
          state.pendingCrossings[0].lastOnLine.y == heldPoint.y);
    CHECK(state.travelDistance == heldTravel && state.cooldownTicks == heldCooldown);

    const cw::Pulse exit = cw::tick(state, {0.0f, 0.0f}, {0.0f, 2.125f}, knot, true, true);
    CHECK(exit.triggered && cw::claimed(state, 12));
    CHECK(exit.center.x == 0.0f && exit.center.y == 0.0f);
    CHECK(state.pendingCount == 0);
}

void touchReverseAndAlongLineJitter() {
    const std::array<cw::Segment, 1> knot{{{13, {-10.0f, 0.0f}, {10.0f, 0.0f}, 100}}};
    cw::State reverse;
    reverse.travelDistance = cw::kTravelDistance;
    CHECK(!cw::tick(reverse, {0.0f, -2.125f}, {0.0f, 0.0f}, knot, true, true).triggered);
    CHECK(reverse.pendingCount == 1);
    CHECK(!cw::tick(reverse, {0.0f, 0.0f}, {0.0f, -2.125f}, knot, true, true).triggered);
    CHECK(reverse.pendingCount == 0 && !cw::claimed(reverse, 13));

    CHECK(!cw::tick(reverse, {0.0f, -2.125f}, {0.0f, 0.0f}, knot, true, true).triggered);
    CHECK(!cw::tick(reverse, {0.0f, 0.0f}, {2.0f, 0.02f}, knot, true, true).triggered);
    CHECK(!cw::tick(reverse, {2.0f, 0.02f}, {4.0f, -0.02f}, knot, true, true).triggered);
    CHECK(reverse.pendingCount == 1 && !cw::claimed(reverse, 13));
    CHECK(reverse.pendingCrossings[0].lastOnLine.x == 4.0f);

    const cw::Pulse crossed = cw::tick(reverse, {4.0f, -0.02f}, {4.0f, 2.125f}, knot, true, true);
    CHECK(crossed.triggered && cw::claimed(reverse, 13));
    CHECK(crossed.center.x == 4.0f && crossed.center.y == 0.0f);
}

void pendingExpiryAndClaimReuseGuard() {
    cw::State pending;
    pending.travelDistance = cw::kTravelDistance;
    const std::array<cw::Segment, 1> young{{{14, {-10.0f, 0.0f}, {10.0f, 0.0f}, 72}}};
    CHECK(!cw::tick(pending, {0.0f, -2.125f}, {0.0f, 0.0f}, young, true, true).triggered);
    CHECK(pending.pendingCount == 1);

    const std::array<cw::Segment, 1> expired{{{14, {-10.0f, 0.0f}, {10.0f, 0.0f}, 421}}};
    CHECK(!cw::tick(pending, {0.0f, 0.0f}, {0.0f, 2.125f}, expired, true, true).triggered);
    CHECK(pending.pendingCount == 0 && !cw::claimed(pending, 14));

    // A consumed stable ID stays claimed while present, including after it
    // ages outside the eligible window, so it cannot be revived by age drift.
    cw::State consumed;
    consumed.travelDistance = cw::kTravelDistance;
    const std::array<cw::Segment, 1> liveAtLimit{{{15, {-10.0f, 0.0f}, {10.0f, 0.0f}, 420}}};
    CHECK(cw::tick(consumed, {0.0f, -2.125f}, {0.0f, 2.125f}, liveAtLimit, true, true).triggered);
    const std::array<cw::Segment, 1> nowExpired{{{15, {-10.0f, 0.0f}, {10.0f, 0.0f}, 421}}};
    CHECK(!cw::tick(consumed, {0.0f, -2.125f}, {0.0f, 2.125f}, nowExpired, true, true).triggered);
    CHECK(cw::claimed(consumed, 15));
    const std::array<cw::Segment, 0> gone{};
    cw::tick(consumed, {0.0f, 2.125f}, {0.0f, 2.125f}, gone, true, true);
    CHECK(!cw::claimed(consumed, 15));
}

void consumeWhileBlockedAndNeverReuse() {
    cw::State state;
    const std::array<cw::Segment, 1> first{{vertical(20, 120)}};
    const cw::Pulse waiting = cw::tick(state, {-5.0f, 0.0f}, {5.0f, 0.0f}, first, true, true);
    CHECK(!waiting.triggered);
    CHECK(cw::claimed(state, 20)); // Travel gate did not leave the knot reusable.

    state.travelDistance = cw::kTravelDistance;
    const cw::Pulse reused = cw::tick(state, {5.0f, 0.0f}, {-5.0f, 0.0f}, first, true, true);
    CHECK(!reused.triggered);

    const std::array<cw::Segment, 1> second{{vertical(21, 120, 20.0f)}};
    state.travelDistance = cw::kTravelDistance;
    const cw::Pulse firstPulse = cw::tick(state, {15.0f, 0.0f}, {25.0f, 0.0f}, second, true, true);
    CHECK(firstPulse.triggered && cw::claimed(state, 21));

    const std::array<cw::Segment, 1> third{{vertical(22, 120, 40.0f)}};
    state.travelDistance = cw::kTravelDistance;
    const cw::Pulse cooldownBlocked = cw::tick(state, {35.0f, 0.0f}, {45.0f, 0.0f}, third, true, true);
    CHECK(!cooldownBlocked.triggered && cw::claimed(state, 22));
    CHECK(state.cooldownTicks == cw::kCooldownTicks - 1);
}

void cooldownBoundaryPauseAndClaimPruning() {
    cw::State state;
    const std::array<cw::Segment, 1> first{{vertical(30, 90)}};
    state.travelDistance = cw::kTravelDistance;
    CHECK(cw::tick(state, {-5.0f, 0.0f}, {5.0f, 0.0f}, first, true, true).triggered);

    const float heldTravel = state.travelDistance;
    const int heldCooldown = state.cooldownTicks;
    const std::array<cw::Segment, 1> later{{vertical(31, 90, 10.0f)}};
    const cw::Pulse paused = cw::tick(state, {5.0f, 0.0f}, {15.0f, 0.0f}, later, false, true);
    CHECK(!paused.triggered);
    CHECK(state.cooldownTicks == heldCooldown && state.travelDistance == heldTravel);
    CHECK(!cw::claimed(state, 31));

    for (int i = 0; i < cw::kCooldownTicks - 1; ++i) {
        cw::tick(state, {15.0f, 0.0f}, {15.0f, 0.0f}, later, true, true);
    }
    CHECK(state.cooldownTicks == 1);
    state.travelDistance = cw::kTravelDistance;
    const cw::Pulse atBoundary = cw::tick(state, {5.0f, 0.0f}, {15.0f, 0.0f}, later, true, true);
    CHECK(atBoundary.triggered);
    CHECK(state.cooldownTicks == cw::kCooldownTicks);

    const std::array<cw::Segment, 0> noHistory{};
    cw::tick(state, {15.0f, 0.0f}, {15.0f, 0.0f}, noHistory, true, true);
    CHECK(!cw::claimed(state, 30) && !cw::claimed(state, 31));
    CHECK(state.consumedCount == 0);
}

void boundedHistoryClaimCapacity() {
    std::array<cw::Segment, 520> live{};
    for (std::size_t i = 0; i < live.size(); ++i) {
        live[i] = vertical(static_cast<std::uint64_t>(i + 1), 100);
    }
    cw::State withinBound;
    withinBound.travelDistance = cw::kTravelDistance;
    const cw::Pulse pulse = cw::tick(withinBound, {-5.0f, 0.0f}, {5.0f, 0.0f}, live, true, true);
    CHECK(pulse.triggered);
    CHECK(withinBound.consumedCount == live.size());
    CHECK(!withinBound.claimOverflow);
    CHECK(cw::claimed(withinBound, 1) && cw::claimed(withinBound, 520));

    std::array<cw::Segment, cw::kClaimCapacity + 1> excessive{};
    for (std::size_t i = 0; i < excessive.size(); ++i) {
        excessive[i] = vertical(static_cast<std::uint64_t>(i + 1000), 100);
    }
    cw::State failClosed;
    failClosed.travelDistance = cw::kTravelDistance;
    const cw::Pulse blocked = cw::tick(failClosed, {-5.0f, 0.0f}, {5.0f, 0.0f}, excessive, true, true);
    CHECK(!blocked.triggered);
    CHECK(failClosed.consumedCount == cw::kClaimCapacity);
    CHECK(failClosed.claimOverflow);

    std::array<cw::Segment, 520> pendingLive{};
    for (std::size_t i = 0; i < pendingLive.size(); ++i) {
        pendingLive[i] = {static_cast<std::uint64_t>(i + 1), {-10.0f, 0.0f}, {10.0f, 0.0f}, 100};
    }
    cw::State pendingWithinBound;
    pendingWithinBound.travelDistance = cw::kTravelDistance;
    const cw::Pulse touched = cw::tick(pendingWithinBound, {0.0f, -2.125f}, {0.0f, 0.0f},
                                        pendingLive, true, true);
    CHECK(!touched.triggered);
    CHECK(pendingWithinBound.pendingCount == pendingLive.size());
    CHECK(!pendingWithinBound.claimOverflow);
    const cw::Pulse exited = cw::tick(pendingWithinBound, {0.0f, 0.0f}, {0.0f, 2.125f},
                                       pendingLive, true, true);
    CHECK(exited.triggered);
    CHECK(pendingWithinBound.consumedCount == pendingLive.size());

    std::array<cw::Segment, cw::kClaimCapacity + 1> excessivePending{};
    for (std::size_t i = 0; i < excessivePending.size(); ++i) {
        excessivePending[i] = {static_cast<std::uint64_t>(i + 3000), {-10.0f, 0.0f}, {10.0f, 0.0f}, 100};
    }
    cw::State pendingFailClosed;
    pendingFailClosed.travelDistance = cw::kTravelDistance;
    CHECK(!cw::tick(pendingFailClosed, {0.0f, -2.125f}, {0.0f, 0.0f},
                    excessivePending, true, true).triggered);
    CHECK(pendingFailClosed.pendingCount == cw::kClaimCapacity);
    CHECK(pendingFailClosed.claimOverflow);
    CHECK(!cw::tick(pendingFailClosed, {0.0f, 0.0f}, {0.0f, 2.125f},
                    excessivePending, true, true).triggered);
}

void crossweaveDamageRounding() {
    CHECK(cw::damage(270) == 270);
    CHECK(cw::damage(100, true) == 50);
    CHECK(cw::damage(101, true) == 51);
    CHECK(cw::damage(1, true) == 1);
    CHECK(cw::damage(0, true) == 0);
    CHECK(cw::damage(-1) == 0);
}

void bulwarkPhaseBoundariesAndPause() {
    bw::State state;
    CHECK(state.guarded && state.remainingTicks == bw::kGuardTicks);
    for (int i = 0; i < bw::kGuardTicks - 1; ++i) bw::tick(state, true);
    CHECK(state.guarded && state.remainingTicks == 1);
    bw::tick(state, true);
    CHECK(!state.guarded && state.remainingTicks == bw::kExposureTicks);

    const bw::State paused = state;
    for (int i = 0; i < 100; ++i) bw::tick(state, false);
    CHECK(state.guarded == paused.guarded && state.remainingTicks == paused.remainingTicks);

    for (int i = 0; i < bw::kExposureTicks - 1; ++i) bw::tick(state, true);
    CHECK(!state.guarded && state.remainingTicks == 1);
    bw::tick(state, true);
    CHECK(state.guarded && state.remainingTicks == bw::kGuardTicks);

    bw::clear(state);
    CHECK(state.guarded && state.remainingTicks == bw::kGuardTicks);
}

void bulwarkDamageSingleRoundingAndSaturation() {
    bw::State guarded;
    CHECK(bw::damage(100, guarded) == 60);
    CHECK(bw::damage(5, guarded) == 3);
    CHECK(bw::damage(1, guarded, 3, 4) == 0); // 0.45, not rounded between factors.
    CHECK(bw::damage(270, guarded, 3, 4) == 122);
    CHECK(bw::damage(1, guarded, 0, 4) == 0);
    CHECK(bw::damage(1, guarded, 3, 0) == 0);

    bw::State exposed;
    exposed.guarded = false;
    exposed.remainingTicks = bw::kExposureTicks;
    CHECK(bw::damage(100, exposed) == 125);
    CHECK(bw::damage(2, exposed) == 3); // 2.5 rounds upward.
    CHECK(bw::damage(std::numeric_limits<int>::max(), exposed,
                     std::numeric_limits<int>::max(), 1) == std::numeric_limits<int>::max());
}

} // namespace

int main() {
    ageWindowAndPulseCenter();
    strictCrossingsAndEndpointJitter();
    exactBoundaryCrossingAndPauseFreeze();
    touchReverseAndAlongLineJitter();
    pendingExpiryAndClaimReuseGuard();
    consumeWhileBlockedAndNeverReuse();
    cooldownBoundaryPauseAndClaimPruning();
    boundedHistoryClaimCapacity();
    crossweaveDamageRounding();
    bulwarkPhaseBoundariesAndPause();
    bulwarkDamageSingleRoundingAndSaturation();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All Crossweave and Bulwark tests passed\n";
    return 0;
}
