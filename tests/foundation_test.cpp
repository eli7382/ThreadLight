#include "reward_ledger.h"
#include "spark_pickup.h"
#include "wave_foundation.h"

#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using namespace threadlight::foundation;

void testLifecycleCountdownAndSpawnBoundaries() {
    WaveLifecycle lifecycle;
    lifecycle.start({2, false, 3});
    CHECK(lifecycle.phase() == Phase::Countdown);
    CHECK(lifecycle.elapsedTicks() == 0);
    CHECK(lifecycle.remainingTicks() == 3);

    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Countdown);
    CHECK(lifecycle.elapsedTicks() == 1);
    CHECK(lifecycle.remainingTicks() == 2);
    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Countdown);
    CHECK(lifecycle.elapsedTicks() == 2);
    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Spawning);
    CHECK(lifecycle.elapsedTicks() == 0);
    CHECK(lifecycle.remainingTicks() == 2);

    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Spawning);
    CHECK(lifecycle.elapsedTicks() == 1);
    CHECK(lifecycle.remainingTicks() == 1);
    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Cleanup);
    CHECK(lifecycle.elapsedTicks() == 0);
    CHECK(lifecycle.remainingTicks() == kCleanupTicks);
}

void testNoSurvivorsEnterCollectionOnTheSpawningBoundary() {
    WaveLifecycle lifecycle;
    lifecycle.start({1, false, 0});
    CHECK(lifecycle.phase() == Phase::Spawning);

    // Model one complete combat update, then tick lifecycle at the end of it.
    const Phase phaseAtStart = lifecycle.phase();
    CHECK(isCombat(phaseAtStart));
    const auto events = lifecycle.tick(0);
    CHECK(events.enteredCollection);
    CHECK(!events.timeoutDespawn);
    CHECK(lifecycle.phase() == Phase::Collection);
    CHECK(lifecycle.elapsedTicks() == 0);
}

void testCleanupHasExactly1440CombatUpdates() {
    WaveLifecycle lifecycle;
    lifecycle.start({1, false, 0});
    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Cleanup);
    CHECK(lifecycle.elapsedTicks() == 0);

    for (int tick = 1; tick < kCleanupTicks; ++tick) {
        const auto events = lifecycle.tick(1);
        CHECK(!events.timeoutDespawn);
        CHECK(!events.enteredCollection);
        CHECK(lifecycle.phase() == Phase::Cleanup);
        CHECK(lifecycle.elapsedTicks() == tick);
        CHECK(lifecycle.remainingTicks() == kCleanupTicks - tick);
    }

    const auto finalTick = lifecycle.tick(1);
    CHECK(finalTick.timeoutDespawn);
    CHECK(finalTick.enteredCollection);
    CHECK(lifecycle.phase() == Phase::Collection);
    CHECK(lifecycle.elapsedTicks() == 0);
}

void testCleanupTimeoutDespawnsLedgerSurvivorWithoutReward() {
    WaveLifecycle lifecycle;
    RewardLedger ledger;
    const RewardId survivor = ledger.registerEnemy(13, 25);
    CHECK(survivor == 1);
    lifecycle.start({1, false, 0});

    // The final spawning combat update is followed by lifecycle advancement.
    lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Cleanup);
    CHECK(lifecycle.elapsedTicks() == 0);
    const auto beforeCleanup = ledger.counters();
    CHECK(ledger.score() == 0);
    CHECK(beforeCleanup.spawned == 1);
    CHECK(beforeCleanup.killed == 0);
    CHECK(beforeCleanup.despawned == 0);
    CHECK(beforeCleanup.pickupsCreated == 0);
    CHECK(beforeCleanup.creditsIssued == 0);

    for (int tick = 1; tick < kCleanupTicks; ++tick) {
        const auto events = lifecycle.tick(1);
        CHECK(!events.timeoutDespawn);
        CHECK(lifecycle.phase() == Phase::Cleanup);
        CHECK(lifecycle.elapsedTicks() == tick);
    }

    const auto timeout = lifecycle.tick(1);
    CHECK(timeout.timeoutDespawn);
    CHECK(timeout.enteredCollection);
    CHECK(lifecycle.phase() == Phase::Collection);
    CHECK(ledger.despawnEnemy(survivor));
    CHECK(!ledger.despawnEnemy(survivor));
    CHECK(!ledger.defeatEnemy(survivor));
    CHECK(!ledger.collectPickup(survivor));
    CHECK(!ledger.expirePickup(survivor));

    const auto& afterRemoval = ledger.counters();
    CHECK(ledger.score() == 0);
    CHECK(afterRemoval.spawned == 1);
    CHECK(afterRemoval.killed == beforeCleanup.killed);
    CHECK(afterRemoval.despawned == 1);
    CHECK(afterRemoval.pickupsCreated == beforeCleanup.pickupsCreated);
    CHECK(afterRemoval.pickupsCollected == 0);
    CHECK(afterRemoval.pickupsExpired == 0);
    CHECK(afterRemoval.sparksCreated == 0);
    CHECK(afterRemoval.sparksCollected == 0);
    CHECK(afterRemoval.sparksExpired == 0);
    CHECK(afterRemoval.creditsIssued == beforeCleanup.creditsIssued);
    CHECK(afterRemoval.creditsResolved == 0);
    CHECK(ledger.pendingCredits() == 0);
}

void testCleanupCollectsWhenLastSurvivorIsGone() {
    WaveLifecycle lifecycle;
    lifecycle.start({1, false, 0});
    lifecycle.tick(1);
    for (int tick = 0; tick < 37; ++tick) lifecycle.tick(1);
    CHECK(lifecycle.phase() == Phase::Cleanup);
    CHECK(lifecycle.elapsedTicks() == 37);

    const auto events = lifecycle.tick(0);
    CHECK(events.enteredCollection);
    CHECK(!events.timeoutDespawn);
    CHECK(lifecycle.phase() == Phase::Collection);
}

void testBossRequiresDefeatAndGetsLongerCollection() {
    WaveLifecycle lifecycle;
    lifecycle.start({1, true, 0});
    CHECK(lifecycle.phase() == Phase::Spawning);
    CHECK(lifecycle.remainingTicks() == 0);
    for (int tick = 0; tick < kCleanupTicks + 17; ++tick) {
        const auto events = lifecycle.tick(0, false, false, false);
        CHECK(!events.enteredCollection);
        CHECK(!events.bossDefeated);
        CHECK(lifecycle.phase() == Phase::Spawning);
        CHECK(lifecycle.elapsedTicks() == 0);
    }

    const auto defeated = lifecycle.tick(0, true);
    CHECK(defeated.bossDefeated);
    CHECK(defeated.enteredCollection);
    CHECK(lifecycle.phase() == Phase::Collection);
    CHECK(lifecycle.collectionTicks() == kBossCollectionTicks);
    CHECK(lifecycle.remainingTicks() == kBossCollectionTicks);

    for (int tick = 1; tick < kBossCollectionTicks; ++tick) {
        const auto events = lifecycle.tick(0, false, false, true);
        CHECK(!events.enteredPendingRewards);
        CHECK(lifecycle.phase() == Phase::Collection);
        CHECK(lifecycle.elapsedTicks() == tick);
    }
    CHECK(lifecycle.remainingTicks() == 1);
    const auto finalTick = lifecycle.tick(0, false, false, true);
    CHECK(finalTick.enteredPendingRewards);
    CHECK(lifecycle.phase() == Phase::PendingRewards);
}

void testNormalCollectionAndEarlyContinue() {
    WaveLifecycle timed;
    timed.start({1, false, 0});
    timed.tick(0);
    CHECK(timed.phase() == Phase::Collection);
    CHECK(timed.collectionTicks() == kNormalCollectionTicks);
    CHECK(timed.remainingTicks() == kNormalCollectionTicks);
    for (int tick = 1; tick < kNormalCollectionTicks; ++tick) {
        const auto events = timed.tick(0, false, false, true);
        CHECK(!events.enteredPendingRewards);
        CHECK(timed.phase() == Phase::Collection);
        CHECK(timed.elapsedTicks() == tick);
    }
    CHECK(timed.remainingTicks() == 1);
    CHECK(timed.tick(0, false, false, true).enteredPendingRewards);
    CHECK(timed.phase() == Phase::PendingRewards);

    WaveLifecycle continued;
    continued.start({1, false, 0});
    continued.tick(0);
    const auto early = continued.tick(0, false, true, true);
    CHECK(early.enteredPendingRewards);
    CHECK(continued.phase() == Phase::PendingRewards);
}

void testPickupAbsenceCanEndCollection() {
    WaveLifecycle lifecycle;
    lifecycle.start({1, false, 0});
    lifecycle.tick(0);
    const auto events = lifecycle.tick(0, false, false, false);
    CHECK(events.enteredPendingRewards);
    CHECK(lifecycle.phase() == Phase::PendingRewards);
}

void testPendingRewardsGateCompletion() {
    WaveLifecycle lifecycle;
    lifecycle.start({1, false, 0});
    CHECK(!lifecycle.finishRewards(false));
    lifecycle.tick(0);
    CHECK(!lifecycle.finishRewards(false));
    lifecycle.tick(0, false, true, true);
    CHECK(lifecycle.phase() == Phase::PendingRewards);
    CHECK(!lifecycle.finishRewards(true));
    CHECK(lifecycle.phase() == Phase::PendingRewards);
    CHECK(lifecycle.finishRewards(false));
    CHECK(lifecycle.phase() == Phase::Complete);
    CHECK(!lifecycle.finishRewards(false));
}

void testRewardIdsAndRemovalEventsAreIdempotent() {
    RewardLedger ledger;
    CHECK(ledger.registerEnemy(0, 1) == 0);
    CHECK(ledger.registerEnemy(-1, 1) == 0);
    CHECK(ledger.registerEnemy(1, -1) == 0);

    const RewardId despawned = ledger.registerEnemy(9, 17);
    const RewardId killedThenExpired = ledger.registerEnemy(6, 11);
    const RewardId killedThenCollected = ledger.registerEnemy(14, 13);
    const RewardId later = ledger.registerEnemy(3, 0);
    CHECK(despawned == 1);
    CHECK(killedThenExpired == 2);
    CHECK(killedThenCollected == 3);
    CHECK(later == 4);
    CHECK(ledger.counters().spawned == 4);

    CHECK(ledger.despawnEnemy(despawned));
    CHECK(!ledger.despawnEnemy(despawned));
    CHECK(!ledger.defeatEnemy(despawned));
    CHECK(!ledger.collectPickup(despawned));
    CHECK(!ledger.expirePickup(despawned));
    CHECK(ledger.counters().despawned == 1);
    CHECK(ledger.counters().killed == 0);
    CHECK(ledger.counters().pickupsCreated == 0);
    CHECK(ledger.score() == 0);

    const auto expiredReward = ledger.defeatEnemy(killedThenExpired);
    CHECK(expiredReward.has_value());
    CHECK(expiredReward->id == killedThenExpired);
    CHECK(expiredReward->sparkValue == 6);
    CHECK(!ledger.defeatEnemy(killedThenExpired));
    CHECK(!ledger.despawnEnemy(killedThenExpired));
    CHECK(ledger.score() == 11);
    CHECK(ledger.expirePickup(killedThenExpired));
    CHECK(!ledger.expirePickup(killedThenExpired));
    CHECK(!ledger.collectPickup(killedThenExpired));
    CHECK(!ledger.defeatEnemy(killedThenExpired));
    CHECK(ledger.score() == 11);
    CHECK(ledger.counters().killed == 1);
    CHECK(ledger.counters().pickupsCreated == 1);
    CHECK(ledger.counters().pickupsExpired == 1);
    CHECK(ledger.counters().sparksExpired == 6);
    CHECK(ledger.counters().pickupsCollected == 0);
    CHECK(ledger.counters().sparksCollected == 0);
    CHECK(ledger.counters().creditsIssued == 0);

    const auto collectedReward = ledger.defeatEnemy(killedThenCollected);
    CHECK(collectedReward.has_value());
    CHECK(collectedReward->sparkValue == 14);
    CHECK(!ledger.defeatEnemy(killedThenCollected));
    CHECK(ledger.collectPickup(killedThenCollected));
    CHECK(!ledger.collectPickup(killedThenCollected));
    CHECK(!ledger.expirePickup(killedThenCollected));
    CHECK(!ledger.despawnEnemy(killedThenCollected));
    CHECK(ledger.score() == 94);
    CHECK(ledger.counters().killed == 2);
    CHECK(ledger.counters().pickupsCreated == 2);
    CHECK(ledger.counters().pickupsCollected == 1);
    CHECK(ledger.counters().sparksCreated == 20);
    CHECK(ledger.counters().sparksCollected == 14);
    CHECK(ledger.counters().sparksExpired == 6);
    CHECK(ledger.counters().despawned == 1);

    CHECK(ledger.despawnEnemy(later));
    CHECK(!ledger.despawnEnemy(later));
    CHECK(!ledger.defeatEnemy(later));
    CHECK(!ledger.collectPickup(later));
    CHECK(!ledger.expirePickup(later));
    CHECK(ledger.score() == 94);
    CHECK(ledger.counters().killed == 2);
    CHECK(ledger.counters().pickupsCreated == 2);
}

void testThresholdsUseIssuedCreditsAndResolutionIsFifo() {
    RewardLedger ledger;
    const int values[] = {20, 30, 40, 48};
    const std::uint64_t expectedCredits[] = {1, 2, 3, 4};
    const int expectedNextThresholds[] = {30, 40, 48, 48};
    for (std::size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        const RewardId id = ledger.registerEnemy(values[i], 0);
        CHECK(id != 0);
        CHECK(ledger.defeatEnemy(id).has_value());
        CHECK(ledger.collectPickup(id));
        CHECK(ledger.counters().creditsIssued == expectedCredits[i]);
        CHECK(ledger.counters().creditsResolved == 0);
        CHECK(ledger.pendingCredits() == expectedCredits[i]);
        CHECK(ledger.nextThreshold() == expectedNextThresholds[i]);
        CHECK(ledger.remainder() == 0);
    }

    CHECK(ledger.counters().creditsIssued == 4);
    CHECK(ledger.counters().creditsResolved == 0);
    CHECK(ledger.pendingCredits() == 4);
    CHECK(ledger.remainder() == 0);
    CHECK(ledger.nextThreshold() == 48);
    CHECK(ledger.nextCredit() == 1);

    CHECK(!ledger.resolveCredit(0, Phase::PendingRewards));
    CHECK(!ledger.resolveCredit(1, Phase::Collection));
    CHECK(!ledger.resolveCredit(2, Phase::PendingRewards));
    CHECK(ledger.counters().creditsResolved == 0);
    CHECK(ledger.resolveCredit(1, Phase::PendingRewards));
    CHECK(!ledger.resolveCredit(1, Phase::PendingRewards));
    CHECK(ledger.nextCredit() == 2);
    CHECK(!ledger.resolveCredit(3, Phase::PendingRewards));
    CHECK(ledger.resolveCredit(2, Phase::PendingRewards));
    CHECK(ledger.resolveCredit(3, Phase::PendingRewards));
    CHECK(ledger.resolveCredit(4, Phase::PendingRewards));
    CHECK(!ledger.resolveCredit(4, Phase::PendingRewards));
    CHECK(!ledger.resolveCredit(5, Phase::PendingRewards));
    CHECK(ledger.counters().creditsIssued == 4);
    CHECK(ledger.counters().creditsResolved == 4);
    CHECK(ledger.pendingCredits() == 0);
    CHECK(ledger.nextCredit() == 0);
}

void testPickupSweptContactAndRemotePickup() {
    PickupState sweptPlayer{101};
    Point stationaryPickup{50.0f, 0.0f};
    const auto sweptResult = tickPickup(sweptPlayer, stationaryPickup,
        {0.0f, 0.0f}, {100.0f, 0.0f}, 0.0f, true, true, 0.0f);
    CHECK(sweptResult == PickupResult::Collected);
    CHECK(sweptPlayer.terminal == PickupResult::Collected);
    CHECK(sweptPlayer.combatAgeTicks == 0);

    PickupState movingPickup{102};
    Point movingPosition{15.0f, 0.0f};
    CHECK(tickPickup(movingPickup, movingPosition, {0.0f, 0.0f},
        {0.0f, 0.0f}, 0.0f, true, true, 20.0f) == PickupResult::None);
    CHECK(movingPosition.x < 15.0f);
    CHECK(movingPickup.combatAgeTicks == 1);
    CHECK(tickPickup(movingPickup, movingPosition, {0.0f, 0.0f},
        {0.0f, 0.0f}, 0.0f, true, true, 0.0f) == PickupResult::Collected);
    CHECK(movingPickup.combatAgeTicks == 1);

    PickupState remote{103};
    Point remotePosition{1000.0f, 0.0f};
    CHECK(tickPickup(remote, remotePosition, {0.0f, 0.0f}, {0.0f, 0.0f},
        20.0f, true, true) == PickupResult::None);
    CHECK(!remote.attracted);
    CHECK(remotePosition.x == 1000.0f);
    CHECK(remote.combatAgeTicks == 1);
    CHECK(remote.terminal == PickupResult::None);
}

void testAttractionLatchesAndSpeedIsBounded() {
    PickupState state{201};
    Point position{1000.0f, 0.0f};
    CHECK(tickPickup(state, position, {0.0f, 0.0f}, {0.0f, 0.0f},
        0.0f, false, true, 1001.0f, 300.0f) == PickupResult::None);
    CHECK(state.attracted);
    CHECK(state.pullSpeed > 220.0f);
    CHECK(position.x < 1000.0f);
    const float firstPosition = position.x;

    for (int tick = 0; tick < 80; ++tick) {
        CHECK(tickPickup(state, position, {0.0f, 0.0f}, {0.0f, 0.0f},
            0.0f, false, true, 0.0f, 300.0f) == PickupResult::None);
        CHECK(state.attracted);
        CHECK(state.pullSpeed >= 300.0f);
        CHECK(state.pullSpeed <= 600.0f);
    }
    CHECK(state.pullSpeed == 600.0f);
    CHECK(position.x < firstPosition);
    CHECK(position.x > 12.0f);
    CHECK(state.combatAgeTicks == 0);
}

void testPickupPauseExpiryAndFinalTickContactPrecedence() {
    PickupState paused{301, 77, true, 350.0f, PickupResult::None};
    Point pausedPosition{20.0f, 0.0f};
    const auto beforePosition = pausedPosition;
    CHECK(tickPickup(paused, pausedPosition, {0.0f, 0.0f}, {0.0f, 0.0f},
        10.0f, true, false) == PickupResult::None);
    CHECK(pausedPosition.x == beforePosition.x && pausedPosition.y == beforePosition.y);
    CHECK(paused.combatAgeTicks == 77);
    CHECK(paused.attracted);
    CHECK(paused.pullSpeed == 350.0f);
    CHECK(paused.terminal == PickupResult::None);
    CHECK(tickPickup(paused, pausedPosition, {0.0f, 0.0f}, {0.0f, 0.0f},
        10.0f, true, true) == PickupResult::Collected);
    CHECK(paused.combatAgeTicks == 77);

    PickupState expiry{302, kPickupLifetimeTicks - 1};
    Point farPosition{1000.0f, 0.0f};
    CHECK(tickPickup(expiry, farPosition, {0.0f, 0.0f}, {0.0f, 0.0f},
        1.0f, true, true) == PickupResult::Expired);
    CHECK(expiry.combatAgeTicks == kPickupLifetimeTicks);
    CHECK(expiry.terminal == PickupResult::Expired);

    PickupState lastTickContact{303, kPickupLifetimeTicks - 1};
    Point onPlayer{0.0f, 0.0f};
    CHECK(tickPickup(lastTickContact, onPlayer, {0.0f, 0.0f},
        {0.0f, 0.0f}, 1.0f, true, true) == PickupResult::Collected);
    CHECK(lastTickContact.combatAgeTicks == kPickupLifetimeTicks - 1);
    CHECK(lastTickContact.terminal == PickupResult::Collected);
}

void testPickupExpiresAfterExactly2160CombatTicks() {
    RewardLedger ledger;
    const RewardId id = ledger.registerEnemy(17, 31);
    const auto reward = ledger.defeatEnemy(id);
    CHECK(reward.has_value());
    PickupState pickup{reward ? reward->id : 0};
    Point position{1000.0f, 0.0f};

    for (int tick = 1; tick <= kPickupLifetimeTicks; ++tick) {
        const auto result = tickPickup(pickup, position, {0.0f, 0.0f},
            {0.0f, 0.0f}, 0.0f, true, true, 0.0f);
        if (tick < kPickupLifetimeTicks) {
            CHECK(result == PickupResult::None);
            CHECK(pickup.combatAgeTicks == tick);
            CHECK(ledger.counters().pickupsExpired == 0);
        } else {
            CHECK(result == PickupResult::Expired);
            CHECK(pickup.combatAgeTicks == kPickupLifetimeTicks);
            CHECK(ledger.expirePickup(id));
            CHECK(!ledger.expirePickup(id));
        }
    }

    CHECK(pickup.terminal == PickupResult::Expired);
    CHECK(!ledger.collectPickup(id));
    CHECK(!ledger.defeatEnemy(id));
    CHECK(ledger.score() == 31);
    CHECK(ledger.counters().killed == 1);
    CHECK(ledger.counters().pickupsCreated == 1);
    CHECK(ledger.counters().pickupsExpired == 1);
    CHECK(ledger.counters().sparksCreated == 17);
    CHECK(ledger.counters().sparksExpired == 17);
    CHECK(ledger.counters().pickupsCollected == 0);
    CHECK(ledger.counters().creditsIssued == 0);
}

void testNonCombatPickupMovementDoesNotAge() {
    PickupState state{401, 19};
    Point position{15.0f, 0.0f};
    CHECK(tickPickup(state, position, {0.0f, 0.0f}, {0.0f, 0.0f},
        0.0f, false, true, 20.0f) == PickupResult::None);
    CHECK(position.x < 15.0f);
    CHECK(state.attracted);
    CHECK(state.combatAgeTicks == 19);
    CHECK(tickPickup(state, position, {0.0f, 0.0f}, {0.0f, 0.0f},
        0.0f, false, true, 0.0f) == PickupResult::Collected);
    CHECK(state.combatAgeTicks == 19);
}

} // namespace

int main() {
    testLifecycleCountdownAndSpawnBoundaries();
    testNoSurvivorsEnterCollectionOnTheSpawningBoundary();
    testCleanupHasExactly1440CombatUpdates();
    testCleanupTimeoutDespawnsLedgerSurvivorWithoutReward();
    testCleanupCollectsWhenLastSurvivorIsGone();
    testBossRequiresDefeatAndGetsLongerCollection();
    testNormalCollectionAndEarlyContinue();
    testPickupAbsenceCanEndCollection();
    testPendingRewardsGateCompletion();
    testRewardIdsAndRemovalEventsAreIdempotent();
    testThresholdsUseIssuedCreditsAndResolutionIsFifo();
    testPickupSweptContactAndRemotePickup();
    testAttractionLatchesAndSpeedIsBounded();
    testPickupPauseExpiryAndFinalTickContactPrecedence();
    testPickupExpiresAfterExactly2160CombatTicks();
    testNonCombatPickupMovementDoesNotAge();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "M1 foundation adversarial checks passed.\n";
    return EXIT_SUCCESS;
}
