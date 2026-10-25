#include "afterimage.h"
#include "signature_reward.h"

#include <cmath>
#include <cstdint>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using threadlight::foundation::Point;
using threadlight::afterimage::State;
namespace ai = threadlight::afterimage;

void recordL(State& state) {
    ai::beginDash(state, {0.0f, 0.0f}, true, true);
    ai::appendDash(state, {0.0f, 0.0f}, {100.0f, 0.0f}, true);
    ai::appendDash(state, {100.0f, 0.0f}, {100.0f, 100.0f}, true);
    ai::finishDash(state, true, true);
}

void completedCurvedRouteAndNoInventedChord() {
    State state;
    ai::beginDash(state, {0.0f, 0.0f}, true, true);
    ai::appendDash(state, {0.0f, 0.0f}, {100.0f, 0.0f}, true);
    ai::appendDash(state, {100.0f, 0.0f}, {100.0f, 100.0f}, true);
    ai::appendDash(state, {-5.0f, 7.0f}, {0.0f, 100.0f}, true); // Discontinuity is not a chord.
    CHECK(state.recording);
    CHECK(state.count == 3);
    ai::finishDash(state, true, true);
    CHECK(!state.recording);
    CHECK(state.active());
    CHECK(state.remainingTicks == 78);

    CHECK(!ai::claimContact(state, 1, {50.0f, 50.0f}, {50.0f, 50.0f}, 0.0f));
    CHECK(ai::claimContact(state, 2, {50.0f, -20.0f}, {50.0f, 20.0f}, 0.0f));
    CHECK(!ai::claimContact(state, 2, {100.0f, 100.0f}, {100.0f, 100.0f}, 0.0f));
    CHECK(ai::claimContact(state, 3, {100.0f, 100.0f}, {100.0f, 100.0f}, 0.0f));
}

void ownershipCombatAndZeroDistanceGates() {
    State unowned;
    ai::beginDash(unowned, {4.0f, 9.0f}, true, false);
    ai::appendDash(unowned, {4.0f, 9.0f}, {10.0f, 9.0f}, true);
    ai::finishDash(unowned, true, false);
    CHECK(!unowned.active() && !unowned.recording && unowned.count == 0);

    State noncombat;
    ai::beginDash(noncombat, {4.0f, 9.0f}, false, true);
    ai::appendDash(noncombat, {4.0f, 9.0f}, {10.0f, 9.0f}, false);
    ai::finishDash(noncombat, false, true);
    CHECK(!noncombat.active() && !noncombat.recording);

    State wall;
    ai::beginDash(wall, {20.0f, 20.0f}, true, true);
    ai::appendDash(wall, {20.0f, 20.0f}, {20.0f, 20.0f}, true);
    ai::finishDash(wall, true, true);
    CHECK(!wall.active() && wall.count == 0);
}

void ttlPauseAndExpiration() {
    State state;
    recordL(state);
    for (int i = 0; i < 17; ++i) ai::advance(state, false);
    CHECK(state.remainingTicks == 78);
    ai::advance(state, true);
    CHECK(state.remainingTicks == 77);
    for (int i = 0; i < 10; ++i) ai::advance(state, false);
    CHECK(state.remainingTicks == 77);
    for (int i = 0; i < 76; ++i) ai::advance(state, true);
    CHECK(state.active() && state.remainingTicks == 1);
    ai::advance(state, true);
    CHECK(!state.active() && state.remainingTicks == 0 && state.count == 0);
}

void stableIdsClearAndFailSafeOverflow() {
    State state;
    recordL(state);
    CHECK(ai::claimContact(state, 41, {100.0f, 0.0f}, {100.0f, 0.0f}, 1.0f));
    CHECK(!ai::claimContact(state, 41, {100.0f, 0.0f}, {100.0f, 0.0f}, 1.0f));

    State overflow;
    recordL(overflow);
    for (std::uint64_t id = 1; id <= ai::kHitTargetCapacity; ++id) {
        CHECK(ai::claimContact(overflow, id, {100.0f, 0.0f}, {100.0f, 0.0f}, 0.0f));
    }
    CHECK(!ai::claimContact(overflow, 1000, {100.0f, 0.0f}, {100.0f, 0.0f}, 0.0f));
    CHECK(overflow.hitTargetOverflow);
    CHECK(!ai::claimContact(overflow, 1000, {100.0f, 0.0f}, {100.0f, 0.0f}, 0.0f));

    ai::clear(state); // Transition/death owner clears all offensive state.
    CHECK(!state.active() && !state.recording && state.count == 0);
    CHECK(!ai::claimContact(state, 42, {100.0f, 0.0f}, {100.0f, 0.0f}, 1.0f));
}

void pathCapacityAndResizeRemap() {
    State recording;
    ai::beginDash(recording, {10.0f, 10.0f}, true, true);
    Point previous{10.0f, 10.0f};
    for (int i = 1; i < 40; ++i) {
        const Point current{10.0f + static_cast<float>(i), 10.0f + static_cast<float>(i % 3)};
        ai::appendDash(recording, previous, current, true);
        previous = current;
    }
    CHECK(recording.count == ai::kPathPointCapacity);
    const ai::Bounds from{0.0f, 100.0f, 0.0f, 100.0f};
    const ai::Bounds to{100.0f, 300.0f, 200.0f, 400.0f};
    const Point oldFirst = recording.path[0];
    const Point oldLast = recording.path[recording.count - 1];
    ai::remap(recording, from, to);
    CHECK(recording.recording && recording.count == ai::kPathPointCapacity);
    CHECK(std::fabs(recording.path[0].x - 120.0f) < 0.001f);
    CHECK(std::fabs(recording.path[0].y - 220.0f) < 0.001f);
    CHECK(std::fabs(recording.path[recording.count - 1].x - (100.0f + oldLast.x * 2.0f)) < 0.001f);
    CHECK(std::fabs(recording.path[recording.count - 1].y - (200.0f + oldLast.y * 2.0f)) < 0.001f);
    CHECK(oldFirst.x == 10.0f && oldLast.x >= oldFirst.x);

    State active;
    recordL(active);
    active.path[2] = active.path[1]; // A zero edge is preserved by coordinate mapping.
    const int beforeTicks = active.remainingTicks;
    ai::remap(active, from, to);
    CHECK(active.active() && active.remainingTicks == beforeTicks);
    CHECK(active.path[1].x == active.path[2].x && active.path[1].y == active.path[2].y);
    const Point beforeInvalid = active.path[0];
    ai::remap(active, {5.0f, 5.0f, 0.0f, 100.0f}, to);
    CHECK(active.path[0].x == beforeInvalid.x && active.path[0].y == beforeInvalid.y);

    State collapsedDestination;
    recordL(collapsedDestination);
    ai::remap(collapsedDestination, from, {10.0f, 10.0f, 30.0f, 330.0f});
    CHECK(collapsedDestination.path[0].x == 10.0f);
    CHECK(collapsedDestination.path[1].x == 10.0f);
    CHECK(collapsedDestination.path[2].x == 10.0f);
}

void integerDamageRounding() {
    CHECK(ai::damage(100) == 75);
    CHECK(ai::damage(100, true) == 38);
    CHECK(ai::damage(270) == 203);
    CHECK(ai::damage(270, true) == 101);
    CHECK(ai::damage(1) == 1);
    CHECK(ai::damage(1, true) == 0);
    CHECK(ai::damage(0) == 0);
    CHECK(ai::damage(-20, true) == 0);
}

void signatureQueueSurvivalAndOrdinaryCreditGate() {
    namespace sig = threadlight::signature_reward;
    using threadlight::foundation::Phase;

    sig::RewardState state{};
    CHECK(!sig::queue(state, 0, true));
    CHECK(!sig::queue(state, 7, false));
    CHECK(sig::queue(state, 7, true));
    CHECK(state.source == 7 && state.pending && !state.owned);
    CHECK(!sig::queue(state, 7, true));
    CHECK(!sig::queue(state, 8, true));
    CHECK(!sig::accept(state, Phase::Collection, true, 0));
    CHECK(!sig::accept(state, Phase::PendingRewards, true, 2));
    CHECK(!sig::accept(state, Phase::PendingRewards, true, 1));
    CHECK(state.pending && !state.owned); // FIFO ordinary credits retain priority.
    CHECK(sig::accept(state, Phase::PendingRewards, true, 0));
    CHECK(!state.pending && state.owned);
    CHECK(!sig::accept(state, Phase::PendingRewards, true, 0));

    sig::RewardState died{};
    CHECK(sig::queue(died, 12, true));
    CHECK(!sig::accept(died, Phase::PendingRewards, false, 0));
    CHECK(!died.pending && !died.owned);
}

} // namespace

int main() {
    completedCurvedRouteAndNoInventedChord();
    ownershipCombatAndZeroDistanceGates();
    ttlPauseAndExpiration();
    stableIdsClearAndFailSafeOverflow();
    pathCapacityAndResizeRemap();
    integerDamageRounding();
    signatureQueueSurvivalAndOrdinaryCreditGate();

    if (failures != 0) {
        std::cerr << failures << " Afterimage/signature check(s) failed\n";
        return 1;
    }
    std::cout << "Afterimage/signature checks passed\n";
    return 0;
}
