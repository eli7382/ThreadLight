#include "../src/rootwarden.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << line << ": " << expression << '\n';
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

namespace rw = threadlight::rootwarden;
using rw::Arena;
using rw::Events;
using rw::Point;
using rw::Stage;
using rw::State;

constexpr Arena kArena{0.0f, 1280.0f, 0.0f, 800.0f};

bool near(float a, float b, float tolerance = 0.001f) {
    return std::fabs(a - b) <= tolerance;
}

bool nearPoint(Point a, Point b, float tolerance = 0.001f) {
    return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance);
}

bool sameBranch(rw::Branch a, rw::Branch b) {
    return nearPoint(a.a, b.a) && nearPoint(a.b, b.b);
}

Events tickN(State& state, Point player, Arena arena, int count) {
    Events last;
    for (int i = 0; i < count; ++i) last = rw::tick(state, player, arena);
    return last;
}

Point mapPoint(Point point, Arena from, Arena to) {
    return {to.left + (point.x - from.left) / (from.right - from.left) * (to.right - to.left),
            to.top + (point.y - from.top) / (from.bottom - from.top) * (to.bottom - to.top)};
}

Point expectedRemap(Point point, Point oldBoss, Arena from, Arena to, float inset) {
    const Point mappedBoss = mapPoint(oldBoss, from, to);
    const Point boundedBoss = rw::clamp(mappedBoss, to, rw::kVulnerableRadius);
    const Point correction{boundedBoss.x - mappedBoss.x, boundedBoss.y - mappedBoss.y};
    const Point mapped = mapPoint(point, from, to);
    return rw::clamp({mapped.x + correction.x, mapped.y + correction.y}, to, inset);
}

void exactStageTimingAndSupportEvents() {
    State state;
    rw::start(state, {640.0f, 400.0f}, kArena);
    CHECK(rw::alive(state));
    CHECK(state.health.hp == rw::kMaxHp);
    CHECK(state.stage == Stage::Stalk && state.remaining == rw::kStalkTicks);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kStalkTicks - 1);
    CHECK(state.stage == Stage::Stalk && state.remaining == 1);
    Events events = rw::tick(state, {1000.0f, 400.0f}, kArena);
    CHECK(events.warning);
    CHECK(state.stage == Stage::RootWarning && state.remaining == rw::kWarningTicks);
    CHECK(state.branchCount == 2);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kWarningTicks - 1);
    CHECK(state.stage == Stage::RootWarning && state.remaining == 1);
    events = rw::tick(state, {1000.0f, 400.0f}, kArena);
    CHECK(!events.warning);
    CHECK(state.stage == Stage::RootActive && state.remaining == rw::kReachTicks);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kReachTicks - 1);
    CHECK(state.stage == Stage::RootActive && state.remaining == 1);
    rw::tick(state, {1000.0f, 400.0f}, kArena);
    CHECK(state.stage == Stage::Recover && state.remaining == rw::kRecoveryTicks);
    CHECK(state.branchCount == 0);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kRecoveryTicks - 1);
    CHECK(state.stage == Stage::Recover && state.remaining == 1);
    events = rw::tick(state, {1000.0f, 400.0f}, kArena);
    CHECK(events.supports == 2);
    CHECK(state.supportsReleased == 2);
    CHECK(state.firstCycleReleased);
    CHECK(state.metrics.cycles == 1);
    CHECK(state.stage == Stage::Stalk && state.remaining == rw::kStalkTicks);

    events = tickN(state, {200.0f, 700.0f}, kArena, rw::kStalkTicks);
    CHECK(events.warning);
    CHECK(state.stage == Stage::StepWarning && state.remaining == rw::kWarningTicks);
    CHECK(state.branchCount == 0);
    tickN(state, {200.0f, 700.0f}, kArena, rw::kWarningTicks);
    CHECK(state.stage == Stage::StepActive && state.remaining == rw::kStepTicks);
    tickN(state, {200.0f, 700.0f}, kArena, rw::kStepTicks);
    CHECK(state.stage == Stage::Recover && state.remaining == rw::kRecoveryTicks);
    events = tickN(state, {200.0f, 700.0f}, kArena, rw::kRecoveryTicks);
    CHECK(events.supports == 0);
    CHECK(state.supportsReleased == 2);
    CHECK(state.metrics.cycles == 2);
    CHECK(state.stage == Stage::Stalk);
}

void phaseThresholdExposureAndFiniteSupports() {
    State state;
    rw::start(state, {640.0f, 400.0f}, kArena);
    CHECK(rw::supportKind(state, 6) == threadlight::combat::EnemyKind::Hunter);
    CHECK(rw::supportKind(state, 5) == threadlight::combat::EnemyKind::Hunter);
    CHECK(rw::incomingDamage(state, state.p, state.p, 0.0f) == rw::kDamage);

    const rw::DamageResult atThreshold = rw::applySilk(state, 3150);
    CHECK(atThreshold.hit == threadlight::combat::SilkHit::Damaged);
    CHECK(atThreshold.damage == 3150);
    CHECK(state.health.hp == 3850);
    CHECK(state.phase == 1);
    CHECK(!atThreshold.events.phaseChanged);
    CHECK(atThreshold.events.supports == 0);

    tickN(state, {1000.0f, 400.0f}, kArena, threadlight::combat::kSilkHitIntervalTicks);
    const rw::DamageResult crossing = rw::applySilk(state, 1);
    CHECK(crossing.hit == threadlight::combat::SilkHit::Damaged);
    CHECK(crossing.damage == 1);
    CHECK(state.health.hp == 3849);
    CHECK(state.phase == 2);
    CHECK(rw::supportKind(state, 6) == threadlight::combat::EnemyKind::Skitter);
    CHECK(rw::supportKind(state, 5) == threadlight::combat::EnemyKind::Hunter);
    CHECK(rw::supportKind(state, 7) == threadlight::combat::EnemyKind::Hunter);
    CHECK(state.stage == Stage::Transition && state.remaining == rw::kTransitionTicks);
    CHECK(crossing.events.phaseChanged);
    CHECK(crossing.events.supports == 2);
    CHECK(state.supportsReleased == 2);
    CHECK(rw::incomingDamage(state, state.p, state.p, 0.0f) == 0);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kTransitionTicks - 1);
    CHECK(state.stage == Stage::Transition && state.remaining == 1);
    rw::tick(state, {1000.0f, 400.0f}, kArena);
    CHECK(state.stage == Stage::Stalk && state.remaining == rw::kStalkTicks);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kStalkTicks);
    CHECK(state.stage == Stage::RootWarning && state.branchCount == 3);
    tickN(state, {1000.0f, 400.0f}, kArena, rw::kWarningTicks);
    CHECK(state.stage == Stage::RootActive);
    tickN(state, {1000.0f, 400.0f}, kArena, rw::kReachTicks);
    CHECK(state.stage == Stage::Recover && rw::exposed(state));

    const rw::DamageResult exposedHit = rw::applySilk(state, 100);
    CHECK(exposedHit.exposed);
    CHECK(exposedHit.hit == threadlight::combat::SilkHit::Damaged);
    CHECK(exposedHit.damage == 100 + rw::kExposureBonus);
    const int hpAfterExposedHit = state.health.hp;
    const rw::DamageResult gatedHit = rw::applySilk(state, 100);
    CHECK(gatedHit.exposed);
    CHECK(gatedHit.hit == threadlight::combat::SilkHit::None);
    CHECK(gatedHit.damage == 0);
    CHECK(state.health.hp == hpAfterExposedHit);

    tickN(state, {1000.0f, 400.0f}, kArena, threadlight::combat::kSilkHitIntervalTicks - 1);
    CHECK(rw::applySilk(state, 100).hit == threadlight::combat::SilkHit::None);
    rw::tick(state, {1000.0f, 400.0f}, kArena);
    const rw::DamageResult nextExposedHit = rw::applySilk(state, 100);
    CHECK(nextExposedHit.exposed);
    CHECK(nextExposedHit.hit == threadlight::combat::SilkHit::Damaged);
    CHECK(nextExposedHit.damage == 100 + rw::kExposureBonus);
    CHECK(state.metrics.hits == 4);
    CHECK(state.metrics.exposedHits == 2);
    CHECK(state.metrics.silkDamage == 3150 + 1 + 2 * (100 + rw::kExposureBonus));

    Events events = tickN(state, {1000.0f, 400.0f}, kArena, state.remaining);
    CHECK(events.supports == 2);
    CHECK(state.supportsReleased == rw::kSupportBudget);
    CHECK(state.metrics.cycles == 1);

    tickN(state, {1000.0f, 400.0f}, kArena, rw::kStalkTicks);
    CHECK(state.stage == Stage::StepWarning);
    tickN(state, {1000.0f, 400.0f}, kArena, rw::kWarningTicks);
    CHECK(state.stage == Stage::StepActive);
    tickN(state, {1000.0f, 400.0f}, kArena, rw::kStepTicks);
    CHECK(state.stage == Stage::Recover);
    events = tickN(state, {1000.0f, 400.0f}, kArena, rw::kRecoveryTicks);
    CHECK(events.supports == 0);
    CHECK(state.supportsReleased == rw::kSupportBudget);
    CHECK(state.metrics.cycles == 2);
    CHECK(rw::supports(state, 100) == 0);
    CHECK(state.supportsReleased == rw::kSupportBudget);

    State bounded;
    rw::start(bounded, {640.0f, 400.0f}, kArena);
    CHECK(rw::supports(bounded, 100) == rw::kSupportBudget);
    CHECK(rw::supports(bounded, 1) == 0);
    CHECK(bounded.supportsReleased == rw::kSupportBudget);
}

void committedGeometryAndResizeRemap() {
    const Point firstTarget{1100.0f, 400.0f};
    State state;
    rw::start(state, {250.0f, 400.0f}, kArena);
    Events events = tickN(state, firstTarget, kArena, rw::kStalkTicks - 1);
    CHECK(!events.warning);
    CHECK(state.stage == Stage::Stalk && state.remaining == 1);
    events = rw::tick(state, firstTarget, kArena);
    CHECK(events.warning);
    CHECK(state.stage == Stage::RootWarning && state.branchCount == 2);

    const auto originalBranches = state.branches;
    const Point warningMid = rw::interpolate(originalBranches[0].a, originalBranches[0].b, 0.5f);
    CHECK(rw::incomingDamage(state, warningMid, warningMid, 0.0f) == 0);
    const Point committedBoss = state.p;
    const int warningTicksBeforeRemap = state.remaining;
    const Arena wide{100.0f, 1636.0f, 50.0f, 1050.0f};
    std::array<rw::Branch, 3> expectedBranches{};
    for (int i = 0; i < state.branchCount; ++i) {
        expectedBranches[static_cast<std::size_t>(i)].a = expectedRemap(
            originalBranches[static_cast<std::size_t>(i)].a, committedBoss, kArena, wide, rw::kRootHalfWidth);
        expectedBranches[static_cast<std::size_t>(i)].b = expectedRemap(
            originalBranches[static_cast<std::size_t>(i)].b, committedBoss, kArena, wide, rw::kRootHalfWidth);
    }
    const Point expectedBoss = rw::clamp(mapPoint(committedBoss, kArena, wide), wide, rw::kVulnerableRadius);
    rw::remap(state, kArena, wide);
    CHECK(state.stage == Stage::RootWarning);
    CHECK(state.remaining == warningTicksBeforeRemap);
    CHECK(nearPoint(state.p, expectedBoss));
    CHECK(nearPoint(state.previousP, state.p));
    for (int i = 0; i < state.branchCount; ++i) {
        CHECK(sameBranch(state.branches[static_cast<std::size_t>(i)], expectedBranches[static_cast<std::size_t>(i)]));
    }

    tickN(state, {300.0f, 900.0f}, wide, 17);
    CHECK(state.stage == Stage::RootWarning);
    CHECK(state.branchCount == 2);
    CHECK(nearPoint(state.p, expectedBoss));
    for (int i = 0; i < state.branchCount; ++i) {
        CHECK(sameBranch(state.branches[static_cast<std::size_t>(i)], expectedBranches[static_cast<std::size_t>(i)]));
    }

    tickN(state, {300.0f, 900.0f}, wide, state.remaining);
    CHECK(state.stage == Stage::RootActive && state.remaining == rw::kReachTicks);
    CHECK(nearPoint(rw::activeBranch(state, 0).a, state.branches[0].a));
    CHECK(nearPoint(rw::activeBranch(state, 0).b,
                    rw::interpolate(state.branches[0].a, state.branches[0].b, 1.0f / rw::kReachTicks)));
    CHECK(sameBranch(state.branches[0], expectedBranches[0]));

    tickN(state, {300.0f, 900.0f}, wide, rw::kReachTicks / 2);
    const rw::Branch activeHalf = rw::activeBranch(state, 0);
    CHECK(nearPoint(activeHalf.b,
                    rw::interpolate(state.branches[0].a, state.branches[0].b,
                                    static_cast<float>(rw::kReachTicks / 2 + 1) / rw::kReachTicks)));
    const Point branchHit = rw::interpolate(activeHalf.a, activeHalf.b, 0.8f);
    CHECK(rw::incomingDamage(state, branchHit, branchHit, 0.0f) == rw::kDamage);
    CHECK(sameBranch(state.branches[0], expectedBranches[0]));

    tickN(state, {800.0f, 150.0f}, wide, rw::kReachTicks / 2 - 1);
    CHECK(state.stage == Stage::RootActive && state.remaining == 1);
    const Point fullReachEndpoint = rw::activeBranch(state, 0).b;
    CHECK(nearPoint(fullReachEndpoint, state.branches[0].b));
    CHECK(rw::incomingDamage(state, fullReachEndpoint, fullReachEndpoint, 0.0f) == rw::kDamage);
    rw::tick(state, {800.0f, 150.0f}, wide);
    CHECK(state.stage == Stage::Recover);
    CHECK(rw::incomingDamage(state, fullReachEndpoint, fullReachEndpoint, 0.0f) == 0);
    events = tickN(state, {800.0f, 150.0f}, wide, rw::kRecoveryTicks);
    CHECK(events.supports == 2);
    CHECK(state.stage == Stage::Stalk && state.nextReach == false);

    events = tickN(state, {800.0f, 150.0f}, wide, rw::kStalkTicks);
    CHECK(events.warning);
    CHECK(state.stage == Stage::StepWarning);
    const Point oldStepBoss = state.p;
    const Point oldStepStart = state.stepStart;
    const Point oldStepEnd = state.stepEnd;
    const int stepWarningRemaining = state.remaining;
    const Arena expanded{-40.0f, 1880.0f, 80.0f, 1120.0f};
    const Point expectedStart = expectedRemap(oldStepStart, oldStepBoss, wide, expanded, rw::kVulnerableRadius);
    const Point expectedEnd = expectedRemap(oldStepEnd, oldStepBoss, wide, expanded, rw::kVulnerableRadius);
    const Point expectedStepBoss = rw::clamp(mapPoint(oldStepBoss, wide, expanded), expanded, rw::kVulnerableRadius);
    rw::remap(state, wide, expanded);
    CHECK(state.stage == Stage::StepWarning);
    CHECK(state.remaining == stepWarningRemaining);
    CHECK(nearPoint(state.p, expectedStepBoss));
    CHECK(nearPoint(state.stepStart, expectedStart));
    CHECK(nearPoint(state.stepEnd, expectedEnd));

    tickN(state, {150.0f, 1000.0f}, expanded, 15);
    CHECK(state.stage == Stage::StepWarning);
    CHECK(nearPoint(state.stepStart, expectedStart));
    CHECK(nearPoint(state.stepEnd, expectedEnd));
    tickN(state, {150.0f, 1000.0f}, expanded, state.remaining);
    CHECK(state.stage == Stage::StepActive && state.remaining == rw::kStepTicks);
    tickN(state, {1700.0f, 200.0f}, expanded, 1);
    CHECK(nearPoint(state.p, rw::interpolate(expectedStart, expectedEnd, 1.0f / rw::kStepTicks)));
    CHECK(nearPoint(state.stepStart, expectedStart));
    CHECK(nearPoint(state.stepEnd, expectedEnd));
    tickN(state, {1700.0f, 200.0f}, expanded, rw::kStepTicks / 2 - 1);
    CHECK(nearPoint(state.p, rw::interpolate(expectedStart, expectedEnd, 0.5f)));
    CHECK(rw::incomingDamage(state, state.p, state.p, 0.0f) == rw::kDamage);
    tickN(state, {1700.0f, 200.0f}, expanded, rw::kStepTicks / 2);
    CHECK(state.stage == Stage::Recover);
    CHECK(nearPoint(state.p, expectedEnd));
    CHECK(nearPoint(state.stepStart, expectedStart));
    CHECK(nearPoint(state.stepEnd, expectedEnd));
}

void cancelAndDeathAreTerminal() {
    State canceled;
    rw::start(canceled, {640.0f, 400.0f}, kArena);
    tickN(canceled, {1000.0f, 400.0f}, kArena, rw::kStalkTicks);
    CHECK(canceled.stage == Stage::RootWarning && canceled.branchCount == 2);
    rw::cancel(canceled);
    CHECK(canceled.stage == Stage::Defeated);
    CHECK(canceled.remaining == 0 && canceled.branchCount == 0);
    CHECK(!rw::alive(canceled));
    CHECK(!rw::silkContact(canceled, {0.0f, 400.0f}, {1280.0f, 400.0f}, 2.0f));
    CHECK(rw::incomingDamage(canceled, canceled.p, canceled.p, 0.0f) == 0);
    const rw::DamageResult canceledHit = rw::applySilk(canceled, 1000);
    CHECK(canceledHit.hit == threadlight::combat::SilkHit::None);
    CHECK(!canceledHit.events.defeated);

    State killed;
    rw::start(killed, {640.0f, 400.0f}, kArena);
    const rw::DamageResult fatal = rw::applySilk(killed, rw::kMaxHp);
    CHECK(fatal.hit == threadlight::combat::SilkHit::Killed);
    CHECK(fatal.damage == rw::kMaxHp);
    CHECK(fatal.events.defeated);
    CHECK(killed.stage == Stage::Defeated && killed.remaining == 0);
    CHECK(killed.branchCount == 0 && killed.health.hp == 0);
    CHECK(!rw::alive(killed));
    CHECK(rw::incomingDamage(killed, killed.p, killed.p, 0.0f) == 0);
    CHECK(!rw::silkContact(killed, {0.0f, 400.0f}, {1280.0f, 400.0f}, 2.0f));
    const auto ticksAtDeath = killed.metrics.ticks;
    const Events afterDeath = rw::tick(killed, {1200.0f, 700.0f}, kArena);
    CHECK(killed.metrics.ticks == ticksAtDeath);
    CHECK(!afterDeath.warning && !afterDeath.phaseChanged && !afterDeath.defeated);
    const rw::DamageResult repeated = rw::applySilk(killed, rw::kMaxHp);
    CHECK(repeated.hit == threadlight::combat::SilkHit::None);
    CHECK(repeated.damage == 0 && !repeated.events.defeated);
}

Point playerAtTick(int tick) {
    return {200.0f + static_cast<float>((tick * 7) % 880),
            120.0f + static_cast<float>((tick * 11) % 560)};
}

State replayAtRenderRate(int fps) {
    State state;
    rw::start(state, {640.0f, 400.0f}, kArena);
    int accumulator = 0;
    int simulatedTicks = 0;
    const int frames = 60 * fps;
    for (int frame = 0; frame < frames; ++frame) {
        accumulator += threadlight::foundation::kTicksPerSecond;
        while (accumulator >= fps) {
            rw::tick(state, playerAtTick(simulatedTicks), kArena);
            ++simulatedTicks;
            accumulator -= fps;
        }
    }
    CHECK(simulatedTicks == 7200);
    CHECK(state.metrics.ticks == 7200);
    return state;
}

bool sameState(const State& a, const State& b) {
    if (!nearPoint(a.p, b.p) || !nearPoint(a.previousP, b.previousP) ||
        !nearPoint(a.stepStart, b.stepStart) || !nearPoint(a.stepEnd, b.stepEnd) ||
        a.health.hp != b.health.hp || a.health.maxHp != b.health.maxHp ||
        a.health.silkCooldownTicks != b.health.silkCooldownTicks || a.stage != b.stage ||
        a.remaining != b.remaining || a.phase != b.phase || a.branchCount != b.branchCount ||
        a.supportsReleased != b.supportsReleased || a.nextReach != b.nextReach ||
        a.firstCycleReleased != b.firstCycleReleased || a.metrics.ticks != b.metrics.ticks ||
        a.metrics.hits != b.metrics.hits || a.metrics.exposedHits != b.metrics.exposedHits ||
        a.metrics.contactTicks != b.metrics.contactTicks || a.metrics.cycles != b.metrics.cycles ||
        a.metrics.silkDamage != b.metrics.silkDamage || a.metrics.playerDamage != b.metrics.playerDamage) {
        return false;
    }
    for (std::size_t i = 0; i < a.branches.size(); ++i) {
        if (!sameBranch(a.branches[i], b.branches[i])) return false;
    }
    return true;
}

void fixedTickReplayIsRenderRateIndependent() {
    constexpr std::array<int, 6> rates{30, 60, 120, 144, 240, 500};
    const State reference = replayAtRenderRate(rates[0]);
    CHECK(reference.metrics.ticks == 7200);
    CHECK(reference.supportsReleased == 2);
    for (std::size_t i = 1; i < rates.size(); ++i) {
        const State candidate = replayAtRenderRate(rates[i]);
        CHECK(sameState(reference, candidate));
        CHECK(candidate.metrics.cycles > 0);
    }
}

}  // namespace

int main() {
    exactStageTimingAndSupportEvents();
    phaseThresholdExposureAndFiniteSupports();
    committedGeometryAndResizeRemap();
    cancelAndDeathAreTerminal();
    fixedTickReplayIsRenderRateIndependent();

    if (failures != 0) {
        std::cerr << failures << " Rootwarden check(s) failed\n";
        return 1;
    }
    std::cout << "Rootwarden checks passed\n";
    return 0;
}
