#include "../src/briar_caller.h"
#include "../src/reward_ledger.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iterator>

namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) {
        std::cerr << "FAIL " << line << ": " << expression << '\n';
        ++failures;
    }
}
#define CHECK(x) check(static_cast<bool>(x), #x, __LINE__)

namespace caller = threadlight::briar_caller;
namespace foundation = threadlight::foundation;

bool near(float first, float second, float tolerance = 0.001f) {
    return std::fabs(first - second) <= tolerance;
}

void testFusionRangeAndGroupHealth() {
    constexpr int improvement[] = {0, 20, 35, 50, 100, 100};
    constexpr int scaledHp[] = {350, 420, 473, 525, 700, 700};
    constexpr int scaledScore[] = {45, 54, 61, 68, 90, 90};
    for (int members = 1; members <= 6; ++members) {
        CHECK(caller::improvementPercent(members) == improvement[members - 1]);
        CHECK(caller::scale(350, members) == scaledHp[members - 1]);
        CHECK(caller::scale(45, members) == scaledScore[members - 1]);
    }

    const caller::State single{};
    const caller::State pair{2};
    const caller::State trio{3};
    CHECK(caller::readyToFuse({0, 0}, single, {150, 0}, single));
    CHECK(!caller::readyToFuse({0, 0}, single, {150.01f, 0}, single));
    // A larger group's radius cannot pull a lone Caller in from beyond its own range.
    CHECK(caller::readyToFuse({0, 0}, pair, {150, 0}, single));
    CHECK(!caller::readyToFuse({0, 0}, pair, {150.01f, 0}, single));
    CHECK(caller::readyToFuse({0, 0}, pair, {180, 0}, trio));
    CHECK(!caller::readyToFuse({0, 0}, pair, {180.01f, 0}, trio));

    CHECK(caller::fusedMembers(single, single) == 2);
    CHECK(caller::fusedMembers(pair, single) == 3);
    CHECK(caller::fusedMembers(caller::State{3}, caller::State{2}) == 5);
    CHECK(caller::fusedMembers(caller::State{0}, caller::State{-4}) == 2);

    CHECK(caller::scale(350, 2) == 420);
    CHECK(caller::fusedHealth(175, 350, single, 350, 350, single,
                              caller::scale(350, 2)) == 315);
    CHECK(caller::fusedHealth(210, 420, pair, 350, 350, single,
                              caller::scale(350, 3)) == 315);
    CHECK(caller::fusedHealth(420, 420, pair, 350, 350, single,
                              caller::scale(350, 3)) == 473);
    CHECK(caller::fusedHealth(0, 350, single, 0, 350, single, 420) == 1);
    CHECK(caller::fusedHealth(0, 0, single, 0, 350, single, 420) == 420);
    CHECK(caller::fusedHealth(0, 350, single, 0, 350, single, 0) == 0);
}

void testAuraStrengthBoundaryRefreshAndExpiry() {
    constexpr int members[] = {1, 2, 3, 4, 5, 6};
    constexpr int strength[] = {18, 22, 24, 27, 36, 36};
    constexpr int duration[] = {240, 288, 324, 360, 480, 480};
    constexpr float radius[] = {150, 180, 202.5f, 225, 300, 300};

    for (std::size_t i = 0; i < std::size(members); ++i) {
        const caller::State source{members[i]};
        CHECK(caller::speedBoostPercent(source) == strength[i]);
        CHECK(caller::boostDurationTicks(source) == duration[i]);
        CHECK(near(caller::auraRadius(source), radius[i]));
        CHECK(caller::insideAura({20, 30}, {20 + radius[i], 30}, source));
        CHECK(!caller::insideAura({20, 30}, {20 + radius[i] + .01f, 30}, source));

        caller::SpeedBoost boost{};
        CHECK(near(caller::boostedSpeed(100, boost), 100));
        caller::applyAura(boost, source);
        CHECK(boost.percent == strength[i]);
        CHECK(boost.remainingTicks == duration[i]);
        CHECK(near(caller::boostedSpeed(100, boost), 100 + strength[i]));
        for (int tick = 0; tick < duration[i] - 1; ++tick) caller::tick(boost);
        CHECK(boost.remainingTicks == 1 && boost.percent == strength[i]);
        CHECK(near(caller::boostedSpeed(100, boost), 100 + strength[i]));
        caller::tick(boost);
        CHECK(boost.remainingTicks == 0 && boost.percent == 0);
        CHECK(near(caller::boostedSpeed(100, boost), 100));
        caller::tick(boost);
        CHECK(boost.remainingTicks == 0 && boost.percent == 0);
    }

    caller::SpeedBoost refreshed{};
    caller::applyAura(refreshed, caller::State{1});
    for (int tick = 0; tick < 100; ++tick) caller::tick(refreshed);
    caller::applyAura(refreshed, caller::State{4});
    CHECK(refreshed.percent == 27 && refreshed.remainingTicks == 360);
}

struct Actor {
    foundation::Point position{};
    bool alive = true;
    bool isCaller = true;
};

void testCallerMovementSpeedAndTargetSelection() {
    constexpr float movement[] = {1.0f, .93f, .86f, .79f, .72f};
    for (int members = 1; members <= 5; ++members) {
        const caller::State group{members};
        CHECK(near(caller::movementScale(members), movement[members - 1]));
        CHECK(near(caller::movementSpeed(60, group), 60 * movement[members - 1]));
    }
    CHECK(caller::movementScale(0) == 1.0f);
    CHECK(near(caller::movementScale(15), .02f));
    CHECK(caller::movementScale(16) == 0.0f);
    CHECK(caller::movementScale(20) == 0.0f);
    CHECK(near(caller::boostedSpeed(60, caller::SpeedBoost{1, 18}), 70.8f));
    CHECK(near(caller::boostedSpeed(60, caller::SpeedBoost{0, 18}), 60));

    std::array<Actor, 5> actors{{
        {{0, 0}, true, true},
        {{120, 0}, true, true},
        {{40, 0}, true, false},
        {{25, 0}, false, true},
        {{-90, 0}, true, true},
    }};
    const auto isAlive = [](const Actor& actor) { return actor.alive; };
    const auto isCaller = [](const Actor& actor) { return actor.isCaller; };
    const auto position = [](const Actor& actor) { return actor.position; };
    const auto target = caller::movementTarget({0, 0}, {500, 500}, actors,
                                                &actors[0], isAlive, isCaller, position);
    CHECK(target.x == -90 && target.y == 0);
    actors[4].alive = false;
    const auto nextTarget = caller::movementTarget({0, 0}, {500, 500}, actors,
                                                    &actors[0], isAlive, isCaller, position);
    CHECK(nextTarget.x == 120 && nextTarget.y == 0);
    actors[1].alive = false;
    const auto playerTarget = caller::movementTarget({0, 0}, {500, 500}, actors,
                                                      &actors[0], isAlive, isCaller, position);
    CHECK(playerTarget.x == 500 && playerTarget.y == 500);
}

void testFusedRewardAccountingAndFailureCases() {
    constexpr int originalCallerCount = 5;
    foundation::RewardLedger fusedLedger;
    std::array<foundation::RewardId, originalCallerCount> ids{};
    for (auto& id : ids) id = fusedLedger.registerEnemy(3, 45);

    caller::State group{};
    for (int i = 1; i < originalCallerCount; ++i) {
        const caller::State singleton{};
        const int members = caller::fusedMembers(group, singleton);
        CHECK(caller::readyToFuse({0, 0}, group, {0, 0}, singleton));
        CHECK(fusedLedger.mergeAliveEnemies(ids[0], ids[static_cast<std::size_t>(i)],
                                            3 * members, caller::scale(45, members)));
        group.members = members;
    }
    CHECK(group.members == 5);
    CHECK(fusedLedger.counters().spawned == 5);
    CHECK(fusedLedger.counters().despawned == 4);
    CHECK(fusedLedger.counters().killed == 0);

    const auto combinedReward = fusedLedger.defeatEnemy(ids[0]);
    CHECK(combinedReward.has_value());
    CHECK(combinedReward && combinedReward->sparkValue == 15 && combinedReward->id == ids[0]);
    CHECK(fusedLedger.score() == 90);
    CHECK(!fusedLedger.defeatEnemy(ids[1]));
    CHECK(!fusedLedger.collectPickup(ids[1]));
    CHECK(fusedLedger.counters().killed == 1);
    CHECK(fusedLedger.counters().pickupsCreated == 1);
    CHECK(fusedLedger.counters().sparksCreated == 15);
    CHECK(fusedLedger.collectPickup(ids[0]));
    CHECK(!fusedLedger.collectPickup(ids[0]));
    CHECK(fusedLedger.score() == 165);
    CHECK(fusedLedger.counters().pickupsCollected == 1);
    CHECK(fusedLedger.counters().sparksCollected == 15);
    CHECK(fusedLedger.counters().creditsIssued == 0);

    foundation::RewardLedger separateLedger;
    for (int i = 0; i < originalCallerCount; ++i) {
        const auto id = separateLedger.registerEnemy(3, 45);
        CHECK(id != 0 && separateLedger.defeatEnemy(id).has_value());
    }
    CHECK(separateLedger.score() == 225);
    CHECK(separateLedger.counters().killed == originalCallerCount);
    CHECK(separateLedger.score() > 90);

    foundation::RewardLedger edgeLedger;
    const auto first = edgeLedger.registerEnemy(3, 45);
    const auto second = edgeLedger.registerEnemy(3, 45);
    CHECK(!edgeLedger.mergeAliveEnemies(0, second, 6, 54));
    CHECK(!edgeLedger.mergeAliveEnemies(first, first, 6, 54));
    CHECK(!edgeLedger.mergeAliveEnemies(999, second, 6, 54));
    CHECK(!edgeLedger.mergeAliveEnemies(first, second, 0, 54));
    CHECK(!edgeLedger.mergeAliveEnemies(first, second, 6, -1));
    CHECK(edgeLedger.counters().despawned == 0);
    CHECK(edgeLedger.defeatEnemy(second).has_value());
    CHECK(!edgeLedger.mergeAliveEnemies(first, second, 6, 54));
    CHECK(edgeLedger.despawnEnemy(first));
    CHECK(!edgeLedger.mergeAliveEnemies(first, second, 6, 54));
    CHECK(edgeLedger.counters().despawned == 1);
}
}

int main() {
    testFusionRangeAndGroupHealth();
    testAuraStrengthBoundaryRefreshAndExpiry();
    testCallerMovementSpeedAndTargetSelection();
    testFusedRewardAccountingAndFailureCases();
    if (failures) return EXIT_FAILURE;
    std::cout << "Briar Caller fusion, aura, movement, and reward checks passed.\n";
    return EXIT_SUCCESS;
}
