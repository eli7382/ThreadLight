#include "game_logic.h"
#include "game_timing.h"
#include "reward_ledger.h"
#include "spark_pickup.h"
#include "wave_foundation.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using namespace threadlight::foundation;

struct ScenarioPickup {
    PickupState state;
    Point position;
};

struct Snapshot {
    std::uint64_t simulationTicks{};
    std::uint64_t spawned{}, killed{}, despawned{};
    std::uint64_t pickupsCreated{}, pickupsCollected{}, pickupsExpired{};
    std::uint64_t creditsIssued{}, creditsResolved{}, pendingCredits{};
    std::int64_t remainder{};
    std::uint64_t score{};
    std::size_t alive{};
    Phase phase{Phase::Complete};
    int phaseElapsed{};
    std::vector<PickupState> pickupStates;
    std::vector<Point> pickupPositions;
};

struct ScenarioResult {
    std::uint64_t simulationTicks{};
    std::uint64_t spawned{}, killed{}, despawned{};
    std::uint64_t pickupsCreated{}, pickupsCollected{}, pickupsExpired{};
    std::uint64_t sparksCreated{}, sparksCollected{}, sparksExpired{};
    std::uint64_t creditsIssued{}, creditsResolved{}, pendingCredits{};
    std::int64_t remainder{};
    std::uint64_t score{};
    int normalCombatTicks{}, bossCombatTicks{};
    int normalCollectionTicks{}, bossCollectionTicks{};
    int pauseFrames{}, rewardUiFrames{};
    int carriedAgeAtTransition{}, carriedAgeAtBossCollection{}, carriedAgeAtContact{};
    int carryoverId{};
    bool pausedFreezePassed{}, pendingUiFreezePassed{}, noPhantomUiPassed{};
    bool carryoverPassed{}, completed{};
    std::string trace;
};

class Scenario {
public:
    Scenario() : mode_(threadlight::Mode::Pause) {
        lifecycle_.start({12, false, 2});
        addTrace("encounter1_start");
    }

    ScenarioResult run(double requestedRenderFps) {
        for (int frame = 0; frame < kPauseFrames; ++frame) {
            const Snapshot before = snapshot();
            CHECK(!threadlight::beginPlayFrame(mode_, false));
            CHECK(mode_ == threadlight::Mode::Pause);
            CHECK(sameSnapshot(before, snapshot()));
            CHECK(clock_.remainder() == 0.0);
            ++pauseFrames_;
        }
        mode_ = threadlight::Mode::Play;
        clock_.clear();

        int renderFrames = 0;
        const int renderFrameLimit = static_cast<int>(requestedRenderFps * 20.0);
        while (!done_ && renderFrames < renderFrameLimit) {
            ++renderFrames;
            if (mode_ == threadlight::Mode::Upgrade) {
                resolvePendingCreditOnUiFrame();
                continue;
            }

            CHECK(mode_ == threadlight::Mode::Play);
            CHECK(threadlight::beginPlayFrame(mode_, false));
            const int steps = clock_.advance(1.0 / requestedRenderFps);
            for (int step = 0; step < steps && !done_; ++step) {
                simulateOneFixedTick();
                if (mode_ != threadlight::Mode::Play) break;
            }
        }

        CHECK(done_);
        CHECK(renderFrames < renderFrameLimit);
        return result();
    }

private:
    Snapshot snapshot() const {
        Snapshot out;
        out.simulationTicks = simulationTicks_;
        const auto& c = rewards_.counters();
        out.spawned = c.spawned;
        out.killed = c.killed;
        out.despawned = c.despawned;
        out.pickupsCreated = c.pickupsCreated;
        out.pickupsCollected = c.pickupsCollected;
        out.pickupsExpired = c.pickupsExpired;
        out.creditsIssued = c.creditsIssued;
        out.creditsResolved = c.creditsResolved;
        out.pendingCredits = rewards_.pendingCredits();
        out.remainder = rewards_.remainder();
        out.score = rewards_.score();
        out.alive = enemiesAlive_;
        out.phase = lifecycle_.phase();
        out.phaseElapsed = lifecycle_.elapsedTicks();
        for (const auto& pickup : pickups_) {
            out.pickupStates.push_back(pickup.state);
            out.pickupPositions.push_back(pickup.position);
        }
        return out;
    }

    static bool sameSnapshot(const Snapshot& a, const Snapshot& b) {
        if (a.simulationTicks != b.simulationTicks || a.spawned != b.spawned ||
            a.killed != b.killed || a.despawned != b.despawned ||
            a.pickupsCreated != b.pickupsCreated ||
            a.pickupsCollected != b.pickupsCollected ||
            a.pickupsExpired != b.pickupsExpired ||
            a.creditsIssued != b.creditsIssued ||
            a.creditsResolved != b.creditsResolved ||
            a.pendingCredits != b.pendingCredits || a.remainder != b.remainder ||
            a.score != b.score || a.alive != b.alive || a.phase != b.phase ||
            a.phaseElapsed != b.phaseElapsed ||
            a.pickupStates.size() != b.pickupStates.size() ||
            a.pickupPositions.size() != b.pickupPositions.size()) return false;
        for (std::size_t i = 0; i < a.pickupStates.size(); ++i) {
            const auto& x = a.pickupStates[i];
            const auto& y = b.pickupStates[i];
            const auto& px = a.pickupPositions[i];
            const auto& py = b.pickupPositions[i];
            if (x.id != y.id || x.combatAgeTicks != y.combatAgeTicks ||
                x.attracted != y.attracted || x.pullSpeed != y.pullSpeed ||
                x.terminal != y.terminal || px.x != py.x || px.y != py.y) return false;
        }
        return true;
    }

    static bool sameFrozenSimulation(const Snapshot& a, const Snapshot& b) {
        Snapshot before = a;
        Snapshot after = b;
        before.creditsResolved = after.creditsResolved;
        before.pendingCredits = after.pendingCredits;
        return sameSnapshot(before, after);
    }

    void addTrace(const std::string& event) {
        if (!trace_.empty()) trace_ += ';';
        trace_ += std::to_string(simulationTicks_) + ':' + event;
    }

    ScenarioPickup* pickupFor(RewardId id) {
        for (auto& pickup : pickups_) {
            if (pickup.state.id == id) return &pickup;
        }
        return nullptr;
    }

    std::size_t livePickupCount() const {
        std::size_t count = 0;
        for (const auto& pickup : pickups_) {
            if (pickup.state.terminal == PickupResult::None) ++count;
        }
        return count;
    }

    RewardId spawnEnemy(int sparkValue, int killScore) {
        const RewardId id = rewards_.registerEnemy(sparkValue, killScore);
        CHECK(id != 0);
        ++enemiesAlive_;
        addTrace("spawn_" + std::to_string(id));
        return id;
    }

    void defeat(RewardId id, Point dropPosition) {
        const auto reward = rewards_.defeatEnemy(id);
        CHECK(reward.has_value());
        CHECK(enemiesAlive_ > 0);
        if (!reward) return;
        --enemiesAlive_;
        pickups_.push_back({PickupState{reward->id}, dropPosition});
        addTrace("kill_" + std::to_string(id));
        addTrace("pickup_create_" + std::to_string(id));
    }

    Point nextPlayerPosition(Phase phaseAtStart) const {
        if (encounter_ == 2 && phaseAtStart == Phase::Spawning &&
            bossCombatUpdates_ == 5) return {500.0f, 0.0f};
        if (encounter_ == 2 && phaseAtStart == Phase::Collection &&
            bossCollectionTicks_ == 0) return {900.0f, 0.0f};
        if (encounter_ == 2 && phaseAtStart == Phase::Collection &&
            bossCollectionTicks_ == 1) return {1200.0f, 0.0f};
        return player_;
    }

    void updatePickups(Phase phaseAtStart, Point previousPlayer, Point currentPlayer) {
        const bool active = phaseAtStart != Phase::PendingRewards &&
                            phaseAtStart != Phase::Complete;
        for (auto& pickup : pickups_) {
            if (pickup.state.terminal != PickupResult::None) continue;
            const auto result = tickPickup(pickup.state, pickup.position,
                previousPlayer, currentPlayer, 0.0f, isCombat(phaseAtStart), active);
            if (result == PickupResult::Collected) {
                CHECK(rewards_.collectPickup(pickup.state.id));
                addTrace("pickup_collect_" + std::to_string(pickup.state.id));
                if (pickup.state.id == carriedId_)
                    carriedAgeAtContact_ = pickup.state.combatAgeTicks;
            } else if (result == PickupResult::Expired) {
                CHECK(rewards_.expirePickup(pickup.state.id));
                addTrace("pickup_expire_" + std::to_string(pickup.state.id));
            }
        }
    }

    void startBossEncounter() {
        encounter_ = 2;
        lifecycle_.start({1, true, 0});
        enemiesAlive_ = 0;
        bossCombatUpdates_ = 0;
        bossCollectionTicks_ = 0;
        const auto* first = pickupFor(firstPickupId_);
        const auto* carried = pickupFor(carriedId_);
        CHECK(first != nullptr);
        CHECK(carried != nullptr);
        if (first && carried) {
            carryoverIdStillConsistent_ = first->state.id == firstPickupId_ &&
                carried->state.id == carriedId_ &&
                first->state.terminal == PickupResult::None &&
                carried->state.terminal == PickupResult::None;
            carriedAgeAtTransition_ = carried->state.combatAgeTicks;
            addTrace("carryover_" + std::to_string(carriedId_) + "_" +
                     std::to_string(carriedAgeAtTransition_));
        }
        addTrace("boss_start");
    }

    void openPendingCreditUi() {
        CHECK(lifecycle_.phase() == Phase::PendingRewards);
        CHECK(rewards_.pendingCredits() != 0);
        mode_ = threadlight::Mode::Upgrade;
        shownCredit_ = rewards_.nextCredit();
        CHECK(shownCredit_ != 0);
        clock_.clear();
        addTrace("credit_ui_open_" + std::to_string(shownCredit_));
    }

    void resolvePendingCreditOnUiFrame() {
        const Snapshot before = snapshot();
        CHECK(mode_ == threadlight::Mode::Upgrade);
        CHECK(lifecycle_.phase() == Phase::PendingRewards);
        CHECK(rewards_.pendingCredits() != 0);
        CHECK(shownCredit_ == rewards_.nextCredit());
        CHECK(clock_.remainder() == 0.0);
        CHECK(!threadlight::beginPlayFrame(mode_, false));

        const auto resolvedBefore = rewards_.counters().creditsResolved;
        const CreditId resolving = shownCredit_;
        CHECK(rewards_.resolveCredit(resolving, lifecycle_.phase()));
        CHECK(rewards_.counters().creditsResolved == resolvedBefore + 1);
        CHECK(rewards_.pendingCredits() + 1 == before.pendingCredits);
        addTrace("credit_resolve_" + std::to_string(resolving));
        ++rewardUiFrames_;
        shownCredit_ = rewards_.nextCredit();

        // Credit UI advances the FIFO ledger, while the pending phase world remains frozen.
        pendingUiFreezePassed_ = pendingUiFreezePassed_ &&
                                 sameFrozenSimulation(before, snapshot());
        CHECK(pendingUiFreezePassed_);
        CHECK(lifecycle_.phase() == Phase::PendingRewards);
        if (rewards_.pendingCredits() == 0) {
            shownCredit_ = 0;
            mode_ = threadlight::Mode::Play;
            finishEncounter();
        } else {
            CHECK(shownCredit_ == rewards_.nextCredit());
        }
    }

    void finishEncounter() {
        CHECK(rewards_.pendingCredits() == 0);
        CHECK(lifecycle_.finishRewards(false));
        if (encounter_ == 1) {
            addTrace("encounter1_complete");
            mode_ = threadlight::Mode::Play;
            startBossEncounter();
            clock_.clear();
        } else {
            addTrace("boss_complete");
            done_ = true;
            mode_ = threadlight::Mode::Won;
        }
    }

    void simulateOneFixedTick() {
        CHECK(lifecycle_.phase() != Phase::PendingRewards);
        ++simulationTicks_;
        const Phase phaseAtStart = lifecycle_.phase();
        const int elapsedAtStart = lifecycle_.elapsedTicks();
        const Point previousPlayer = player_;
        const Point currentPlayer = nextPlayerPosition(phaseAtStart);
        player_ = currentPlayer;

        if (encounter_ == 2 && phaseAtStart == Phase::Collection &&
            bossCollectionTicks_ == 0) {
            const auto* carried = pickupFor(carriedId_);
            CHECK(carried != nullptr);
            if (carried) {
                carriedAgeAtBossCollection_ = carried->state.combatAgeTicks;
                CHECK(carried->state.id == carriedId_);
                CHECK(carried->state.terminal == PickupResult::None);
                CHECK(!carried->state.attracted);
                CHECK(carried->state.pullSpeed == 220.0f);
                CHECK(carried->position.x == 900.0f && carried->position.y == 0.0f);
                CHECK(carriedAgeAtBossCollection_ == carriedAgeAtTransition_ + 9);
            }
        }

        // Simulate the phase's combat/contact first; advance lifecycle clocks last.
        updatePickups(phaseAtStart, previousPlayer, currentPlayer);
        bool bossDefeatedThisTick = false;
        bool continueCollection = false;

        if (isCombat(phaseAtStart)) {
            if (encounter_ == 1 && phaseAtStart == Phase::Spawning) {
                if (elapsedAtStart == 2)
                    firstPickupId_ = spawnEnemy(20, 11);
                if (elapsedAtStart == 5)
                    defeat(firstPickupId_, {500.0f, 0.0f});
                if (elapsedAtStart == 6)
                    carriedId_ = spawnEnemy(30, 7);
                if (elapsedAtStart == 9)
                    defeat(carriedId_, {900.0f, 0.0f});
                ++normalCombatTicks_;
            } else if (encounter_ == 2 && phaseAtStart == Phase::Spawning) {
                if (bossCombatUpdates_ == 0) bossId_ = spawnEnemy(48, 17);
                if (bossCombatUpdates_ == 8) {
                    defeat(bossId_, {1200.0f, 0.0f});
                    bossDefeatedThisTick = true;
                }
                ++bossCombatUpdates_;
                ++bossCombatTicks_;
            } else if (encounter_ == 1) {
                ++normalCombatTicks_;
            } else {
                ++bossCombatUpdates_;
                ++bossCombatTicks_;
            }
        }

        if (phaseAtStart == Phase::Collection) {
            if (encounter_ == 1) {
                continueCollection = normalCollectionTicks_ == 2;
                ++normalCollectionTicks_;
            } else {
                ++bossCollectionTicks_;
            }
        }

        const auto events = lifecycle_.tick(enemiesAlive_, bossDefeatedThisTick,
            continueCollection, livePickupCount() != 0);
        if (events.timeoutDespawn) addTrace("cleanup_timeout");
        if (events.enteredCollection) addTrace("phase_collection");
        if (events.enteredPendingRewards) addTrace("phase_pending_rewards");
        if (events.bossDefeated) addTrace("boss_defeated");

        if (events.enteredPendingRewards) {
            if (rewards_.pendingCredits() != 0) {
                openPendingCreditUi();
            } else {
                CHECK(mode_ == threadlight::Mode::Play);
                noPhantomUiPassed_ = mode_ != threadlight::Mode::Upgrade;
                addTrace("pending_empty_no_ui");
                finishEncounter();
            }
        }
    }

    ScenarioResult result() const {
        ScenarioResult out;
        out.simulationTicks = simulationTicks_;
        const auto& c = rewards_.counters();
        out.spawned = c.spawned;
        out.killed = c.killed;
        out.despawned = c.despawned;
        out.pickupsCreated = c.pickupsCreated;
        out.pickupsCollected = c.pickupsCollected;
        out.pickupsExpired = c.pickupsExpired;
        out.sparksCreated = c.sparksCreated;
        out.sparksCollected = c.sparksCollected;
        out.sparksExpired = c.sparksExpired;
        out.creditsIssued = c.creditsIssued;
        out.creditsResolved = c.creditsResolved;
        out.pendingCredits = rewards_.pendingCredits();
        out.remainder = rewards_.remainder();
        out.score = rewards_.score();
        out.normalCombatTicks = normalCombatTicks_;
        out.bossCombatTicks = bossCombatTicks_;
        out.normalCollectionTicks = normalCollectionTicks_;
        out.bossCollectionTicks = bossCollectionTicks_;
        out.pauseFrames = pauseFrames_;
        out.rewardUiFrames = rewardUiFrames_;
        out.carriedAgeAtTransition = carriedAgeAtTransition_;
        out.carriedAgeAtBossCollection = carriedAgeAtBossCollection_;
        out.carriedAgeAtContact = carriedAgeAtContact_;
        out.carryoverId = static_cast<int>(carriedId_);
        out.pausedFreezePassed = pauseFrames_ == kPauseFrames;
        out.pendingUiFreezePassed = pendingUiFreezePassed_ &&
            static_cast<std::uint64_t>(rewardUiFrames_) == c.creditsResolved;
        out.noPhantomUiPassed = noPhantomUiPassed_;
        out.carryoverPassed = carryoverIdStillConsistent_ &&
            carriedAgeAtBossCollection_ == carriedAgeAtTransition_ + 9 &&
            carriedAgeAtContact_ == carriedAgeAtBossCollection_;
        out.completed = done_ && lifecycle_.phase() == Phase::Complete &&
                        mode_ == threadlight::Mode::Won;
        out.trace = trace_;
        return out;
    }

    WaveLifecycle lifecycle_;
    RewardLedger rewards_;
    std::vector<ScenarioPickup> pickups_;
    threadlight::timing::FixedStepClock clock_;
    threadlight::Mode mode_;
    Point player_{0.0f, 0.0f};
    std::size_t enemiesAlive_{};
    RewardId firstPickupId_{}, carriedId_{}, bossId_{};
    std::uint64_t simulationTicks_{};
    int encounter_{1};
    int normalCombatTicks_{}, bossCombatTicks_{}, bossCombatUpdates_{};
    int normalCollectionTicks_{}, bossCollectionTicks_{};
    int pauseFrames_{}, rewardUiFrames_{};
    int carriedAgeAtTransition_{}, carriedAgeAtBossCollection_{}, carriedAgeAtContact_{};
    bool carryoverIdStillConsistent_{true};
    bool pendingUiFreezePassed_{true};
    bool noPhantomUiPassed_{};
    bool done_{};
    std::string trace_;
    static constexpr int kPauseFrames = 4;
    CreditId shownCredit_{};
};

bool sameSummary(const ScenarioResult& a, const ScenarioResult& b) {
    return a.simulationTicks == b.simulationTicks && a.spawned == b.spawned &&
        a.killed == b.killed && a.despawned == b.despawned &&
        a.pickupsCreated == b.pickupsCreated &&
        a.pickupsCollected == b.pickupsCollected && a.pickupsExpired == b.pickupsExpired &&
        a.sparksCreated == b.sparksCreated && a.sparksCollected == b.sparksCollected &&
        a.sparksExpired == b.sparksExpired && a.creditsIssued == b.creditsIssued &&
        a.creditsResolved == b.creditsResolved && a.pendingCredits == b.pendingCredits &&
        a.remainder == b.remainder && a.score == b.score &&
        a.normalCombatTicks == b.normalCombatTicks && a.bossCombatTicks == b.bossCombatTicks &&
        a.normalCollectionTicks == b.normalCollectionTicks &&
        a.bossCollectionTicks == b.bossCollectionTicks &&
        a.pauseFrames == b.pauseFrames && a.rewardUiFrames == b.rewardUiFrames &&
        a.carriedAgeAtTransition == b.carriedAgeAtTransition &&
        a.carriedAgeAtBossCollection == b.carriedAgeAtBossCollection &&
        a.carriedAgeAtContact == b.carriedAgeAtContact && a.carryoverId == b.carryoverId &&
        a.pausedFreezePassed == b.pausedFreezePassed &&
        a.pendingUiFreezePassed == b.pendingUiFreezePassed &&
        a.noPhantomUiPassed == b.noPhantomUiPassed &&
        a.carryoverPassed == b.carryoverPassed && a.completed == b.completed &&
        a.trace == b.trace;
}

void emitTelemetry(double requestedRenderFps, const ScenarioResult& r) {
    // This deterministic helper scenario is not a game-play or performance measurement.
    std::cout << "{\"kind\":\"helper_scenario\",\"requested_render_fps\":"
              << requestedRenderFps
              << ",\"simulation_ticks\":" << r.simulationTicks
              << ",\"spawned\":" << r.spawned
              << ",\"killed\":" << r.killed
              << ",\"despawned\":" << r.despawned
              << ",\"pickups_created\":" << r.pickupsCreated
              << ",\"pickups_collected\":" << r.pickupsCollected
              << ",\"pickups_expired\":" << r.pickupsExpired
              << ",\"sparks_created\":" << r.sparksCreated
              << ",\"sparks_collected\":" << r.sparksCollected
              << ",\"sparks_expired\":" << r.sparksExpired
              << ",\"credits_issued\":" << r.creditsIssued
              << ",\"credits_resolved\":" << r.creditsResolved
              << ",\"pending_credits\":" << r.pendingCredits
              << ",\"remainder\":" << r.remainder
              << ",\"score\":" << r.score
              << ",\"normal_combat_ticks\":" << r.normalCombatTicks
              << ",\"boss_combat_ticks\":" << r.bossCombatTicks
              << ",\"normal_collection_ticks\":" << r.normalCollectionTicks
              << ",\"boss_collection_ticks\":" << r.bossCollectionTicks
              << ",\"credit_ui_frames\":" << r.rewardUiFrames
              << ",\"pause_freeze\":" << (r.pausedFreezePassed ? "true" : "false")
              << ",\"pending_ui_freeze\":" << (r.pendingUiFreezePassed ? "true" : "false")
              << ",\"empty_pending_no_ui\":" << (r.noPhantomUiPassed ? "true" : "false")
              << ",\"carryover\":" << (r.carryoverPassed ? "true" : "false")
              << ",\"carryover_age\":" << r.carriedAgeAtTransition
              << ",\"trace\":\"" << r.trace << "\"}\n";
}

} // namespace

int main(int argc, char** argv) {
    bool telemetry = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--telemetry") telemetry = true;
        else return 2;
    }

    constexpr std::array<double, 6> renderRates{30.0, 60.0, 120.0, 144.0, 165.0, 1000.0};
    std::array<ScenarioResult, renderRates.size()> results{};
    for (std::size_t i = 0; i < renderRates.size(); ++i) {
        Scenario scenario;
        results[i] = scenario.run(renderRates[i]);
        if (telemetry) emitTelemetry(renderRates[i], results[i]);
    }

    const auto& baseline = results.front();
    for (std::size_t i = 1; i < results.size(); ++i) CHECK(sameSummary(baseline, results[i]));
    CHECK(baseline.completed);
    CHECK(baseline.simulationTicks == 28);
    CHECK(baseline.spawned == 3);
    CHECK(baseline.killed == 3);
    CHECK(baseline.despawned == 0);
    CHECK(baseline.pickupsCreated == 3);
    CHECK(baseline.pickupsCollected == 3);
    CHECK(baseline.pickupsExpired == 0);
    CHECK(baseline.sparksCreated == 98);
    CHECK(baseline.sparksCollected == 98);
    CHECK(baseline.sparksExpired == 0);
    CHECK(baseline.creditsIssued == 3);
    CHECK(baseline.creditsResolved == 3);
    CHECK(baseline.pendingCredits == 0);
    CHECK(baseline.remainder == 8);
    CHECK(baseline.score == 525);
    CHECK(baseline.pauseFrames == 4);
    CHECK(baseline.rewardUiFrames == 3);
    CHECK(baseline.pausedFreezePassed);
    CHECK(baseline.pendingUiFreezePassed);
    CHECK(baseline.noPhantomUiPassed);
    CHECK(baseline.carryoverPassed);
    CHECK(baseline.carryoverId == 2);
    CHECK(baseline.carriedAgeAtContact == baseline.carriedAgeAtBossCollection);
    CHECK(baseline.normalCollectionTicks == 3);
    CHECK(baseline.bossCollectionTicks == 2);

    if (failures != 0) return EXIT_FAILURE;
    std::cout << "M1 deterministic fixed-step scenario passed across 30/60/120/144/165/1000 requested FPS.\n";
    return EXIT_SUCCESS;
}
