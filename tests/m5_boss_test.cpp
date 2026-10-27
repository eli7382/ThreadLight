#include "../src/afterimage.h"
#include "../src/chapter_boss.h"
#include "../src/combat.h"
#include "../src/game_layout.h"
#include "../src/game_logic.h"
#include "../src/player_upgrades.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <utility>

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { ++failures; std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n'; } } while (false)
namespace battle = threadlight::combat;
namespace oldBoss = threadlight::rootwarden;
namespace boss = threadlight::chapter_boss;
namespace ranged = threadlight::ranged;
namespace upgrades = threadlight::upgrades;
namespace afterimage = threadlight::afterimage;
using Point = threadlight::foundation::Point;
using Arena = boss::Arena;
constexpr Arena kArena{0.0f, 1280.0f, 0.0f, 800.0f};
constexpr Point kTarget{1100.0f, 400.0f};
constexpr float kPlayerRadius = threadlight::layout::kPlayerRadius;
float distance(Point a, Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
Point add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point subtract(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point scale(Point p, float amount) { return {p.x * amount, p.y * amount}; }
Point unit(Point p) {
    const float length = distance(p, {});
    return length > 0.001f ? scale(p, 1.0f / length) : Point{1.0f, 0.0f};
}
bool near(float a, float b, float epsilon = 0.01f) { return std::fabs(a - b) <= epsilon; }
bool nearPoint(Point a, Point b, float epsilon = 0.02f) {
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}
bool sameEvents(oldBoss::Events a, oldBoss::Events b) {
    return a.supports == b.supports && a.warning == b.warning &&
           a.phaseChanged == b.phaseChanged && a.defeated == b.defeated;
}
void sameLegacyState(const oldBoss::State& a, const oldBoss::State& b) {
    CHECK(a.health.hp == b.health.hp && a.health.maxHp == b.health.maxHp && a.health.silkCooldownTicks == b.health.silkCooldownTicks);
    CHECK(a.stage == b.stage && a.phase == b.phase && a.remaining == b.remaining && a.branchCount == b.branchCount);
    CHECK(a.supportsReleased == b.supportsReleased && a.nextReach == b.nextReach && a.firstCycleReleased == b.firstCycleReleased);
    CHECK(nearPoint(a.p, b.p) && nearPoint(a.previousP, b.previousP) && nearPoint(a.stepStart, b.stepStart) && nearPoint(a.stepEnd, b.stepEnd));
    for (std::size_t i = 0; i < a.branches.size(); ++i) {
        CHECK(nearPoint(a.branches[i].a, b.branches[i].a) && nearPoint(a.branches[i].b, b.branches[i].b));
    }
    CHECK(a.metrics.ticks == b.metrics.ticks && a.metrics.hits == b.metrics.hits && a.metrics.exposedHits == b.metrics.exposedHits && a.metrics.contactTicks == b.metrics.contactTicks);
    CHECK(a.metrics.cycles == b.metrics.cycles && a.metrics.silkDamage == b.metrics.silkDamage && a.metrics.playerDamage == b.metrics.playerDamage);
    CHECK(a.metrics.signatureHits == b.metrics.signatureHits && a.metrics.signatureDamage == b.metrics.signatureDamage);
}
std::uint32_t nextRandom(std::uint32_t& state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
Point routeTarget(std::uint32_t& rng, Arena arena) {
    const auto sample = [](float minimum, float maximum, std::uint32_t value) {
        return minimum + static_cast<float>(value % 10001u) / 10000.0f * (maximum - minimum);
    };
    return {sample(arena.left + 120.0f, arena.right - 120.0f, nextRandom(rng)),
            sample(arena.top + 120.0f, arena.bottom - 120.0f, nextRandom(rng))};
}
Point moveToward(Point from, Point to, float maximumDistance) {
    const Point delta = subtract(to, from);
    const float length = distance(delta, {});
    return length <= maximumDistance || length < 0.001f ? to : add(from, scale(delta, maximumDistance / length));
}
Point remapPoint(Point p, Arena from, Arena to) {
    return {to.left + (p.x - from.left) / (from.right - from.left) * (to.right - to.left),
            to.top + (p.y - from.top) / (from.bottom - from.top) * (to.bottom - to.top)};
}
void rootFacadeMatchesSeededLegacyRoute() {
    oldBoss::State legacy;
    boss::State facade;
    oldBoss::start(legacy, {320.0f, 330.0f}, kArena);
    boss::start(facade, {320.0f, 330.0f}, kArena, boss::Kind::Rootwarden);
    CHECK(facade.kind == boss::Kind::Rootwarden);
    battle::PlayerHealth playerHealth{}; std::uint32_t rng = 0x9e3779b9u; Arena arena = kArena;
    Point player{900.0f, 400.0f}, target = player, previous = player;
    for (int tick = 0; tick < 1500 && oldBoss::alive(legacy); ++tick) {
        if (tick % 83 == 0) target = routeTarget(rng, arena);
        previous = player; player = moveToward(player, target, 4.0f);
        battle::tick(playerHealth);
        const auto oldEvents = oldBoss::tick(legacy, player, arena);
        const auto newEvents = boss::tick(facade, player, arena);
        CHECK(sameEvents(oldEvents, newEvents));
        const auto oldIncoming = oldBoss::incomingDamage(legacy, previous, player, 12.0f);
        const auto newIncoming = boss::incomingDamage(facade, previous, player, 12.0f);
        CHECK(oldIncoming == newIncoming && oldBoss::silkContact(legacy, previous, player, 5.0f) == boss::silkContact(facade, previous, player, 5.0f));
        if (tick == 130 || tick == 530) {
            const auto oldHit = oldBoss::applySilk(legacy, 130), newHit = boss::applySilk(facade, 130);
            CHECK(oldHit.hit == newHit.hit && oldHit.damage == newHit.damage && oldHit.exposed == newHit.exposed && sameEvents(oldHit.events, newHit.events));
        }
        if (tick == 850) {
            const auto oldHit = oldBoss::applySignature(legacy, 70), newHit = boss::applySignature(facade, 70);
            CHECK(oldHit.hit == newHit.hit && oldHit.damage == newHit.damage && oldHit.exposed == newHit.exposed && sameEvents(oldHit.events, newHit.events));
        }
        if (tick == 900) {
            const Arena wide{100.0f, 2660.0f, 50.0f, 1650.0f};
            player = remapPoint(player, arena, wide);
            target = remapPoint(target, arena, wide);
            oldBoss::remap(legacy, arena, wide);
            boss::remap(facade, arena, wide);
            arena = wide;
        }
        sameLegacyState(legacy, static_cast<const oldBoss::State&>(facade));
    }
}
void tickBoss(boss::State& state, battle::PlayerHealth& playerHealth,
              Point player = kTarget, Arena arena = kArena) {
    battle::tick(playerHealth); // Combat updates own player immunity time.
    boss::tick(state, player, arena);
}
void tickBossN(boss::State& state, battle::PlayerHealth& playerHealth,
               int count, Point player = kTarget, Arena arena = kArena) {
    for (int i = 0; i < count; ++i) tickBoss(state, playerHealth, player, arena);
}
void prepareDancerPhase(boss::State& state, battle::PlayerHealth& playerHealth, int phase) {
    boss::start(state, {300.0f, 400.0f}, kArena, boss::Kind::Dancer);
    if (phase == 2) {
        const auto result = boss::applySignature(state, 4051); // 70% threshold crossed by one HP.
        CHECK(result.events.phaseChanged && result.events.supports == 2);
    } else if (phase == 3) {
        const auto result = boss::applySignature(state, 8776); // Direct phase skip stays within the 4-support budget.
        CHECK(result.events.phaseChanged && result.events.supports == 4);
    }
    if (phase > 1) tickBossN(state, playerHealth, 108, {1000.0f, 400.0f});
    tickBossN(state, playerHealth, 90, {1000.0f, 400.0f});
    CHECK(state.phase == phase && state.stage == boss::Stage::StepWarning);
}
void dancerPhaseThresholdsAndFiniteSupports() {
    boss::State state; boss::start(state, {300, 400}, kArena, boss::Kind::Dancer);
    const auto at70 = boss::applySignature(state, 4050); CHECK(!at70.events.phaseChanged && state.phase == 1 && state.health.hp == 9450);
    const auto p2 = boss::applySignature(state, 1); CHECK(p2.events.phaseChanged && p2.events.supports == 2 && state.phase == 2);
    const auto at35 = boss::applySignature(state, 4724); CHECK(!at35.events.phaseChanged && state.health.hp == 4725 && state.phase == 2);
    const auto p3 = boss::applySignature(state, 1); CHECK(p3.events.phaseChanged && p3.events.supports == 2 && state.phase == 3);
    const auto repeat = boss::applySignature(state, 1); CHECK(!repeat.events.phaseChanged && repeat.events.supports == 0 && state.supportsReleased == 4);
}
void dancerWarningResizeAndActiveLungeRetiming() {
    boss::State state; battle::PlayerHealth playerHealth{}; boss::start(state, {300, 400}, kArena, boss::Kind::Dancer);
    tickBossN(state, playerHealth, 90, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::StepWarning && state.remaining == 48 && state.warnings == 1);
    const Point lockedStart = state.stepStart, lockedEnd = state.stepEnd;
    tickBossN(state, playerHealth, 20, {1100.0f, 400.0f}); CHECK(state.remaining == 28);
    const Arena wide{0.0f, 2560.0f, 0.0f, 1600.0f};
    const Point expectedWideStart = remapPoint(lockedStart, kArena, wide);
    const Point expectedWideEnd = remapPoint(lockedEnd, kArena, wide);
    boss::remap(state, kArena, wide);
    CHECK(state.stage == boss::Stage::StepWarning && state.remaining == 28 && nearPoint(state.stepStart, expectedWideStart) && nearPoint(state.stepEnd, expectedWideEnd));
    const int warningStrikeTicks = std::max(34, static_cast<int>(std::ceil(distance(expectedWideStart, expectedWideEnd) / 5.0f)));
    CHECK(state.strikeTicks == warningStrikeTicks && state.strikeTicks >= 34);
    tickBossN(state, playerHealth, 28, {2200.0f, 1300.0f}, wide);
    CHECK(state.stage == boss::Stage::StepActive && state.remaining == state.strikeTicks && nearPoint(state.stepEnd, expectedWideEnd));
    tickBossN(state, playerHealth, 10, {200.0f, 200.0f}, wide);
    const float progressBeforeResize = 1.0f - state.remaining / static_cast<float>(state.strikeTicks);
    const Arena wider{0.0f, 5120.0f, 0.0f, 3200.0f};
    const Point expectedWiderEnd = remapPoint(state.stepEnd, wide, wider);
    boss::remap(state, wide, wider);
    const int retimedStrikeTicks = std::max(34, static_cast<int>(std::ceil(distance(state.stepStart, state.stepEnd) / 5.0f)));
    const int expectedRemaining = std::max(1, static_cast<int>(std::ceil((1.0f - progressBeforeResize) * retimedStrikeTicks)));
    CHECK(state.stage == boss::Stage::StepActive && state.strikeTicks == retimedStrikeTicks && state.remaining == expectedRemaining && nearPoint(state.stepEnd, expectedWiderEnd));
    Point last = state.p;
    for (int i = 0, count = state.remaining; i < count; ++i) {
        tickBoss(state, playerHealth, {5000, 3000}, wider);
        CHECK(distance(last, state.p) * 120.0f <= 600.1f && nearPoint(state.stepEnd, expectedWiderEnd)); last = state.p;
    }
    CHECK(state.stage == boss::Stage::Recover && state.remaining == 168 && state.strikeThisTick && nearPoint(state.p, expectedWiderEnd));
    CHECK(boss::incomingDamage(state, expectedWiderEnd, expectedWiderEnd, 10.0f) == 2200);
}
void dancerChainHasFreshWarningsCounterstepsAndRecovery() {
    for (const int phase : {2, 3}) {
        boss::State state; battle::PlayerHealth playerHealth{};
        prepareDancerPhase(state, playerHealth, phase);
        int expectedWarningCount = 1;
        for (int strike = 0; strike < phase; ++strike) {
            CHECK(state.stage == boss::Stage::StepWarning && state.remaining == 48);
            CHECK(state.warnings == static_cast<std::uint64_t>(expectedWarningCount));
            tickBossN(state, playerHealth, 48, {1000.0f, 400.0f});
            CHECK(state.stage == boss::Stage::StepActive && state.remaining == 34);
            tickBossN(state, playerHealth, 34, {1000.0f, 400.0f});
            if (strike + 1 < phase) {
                CHECK(state.stage == boss::Stage::Stalk && state.counterstepping && state.chain == strike + 1 && state.remaining == 48);
                CHECK(near(distance(state.counterStart, state.counterEnd), 48.0f));
                for (int step = 0; step < 48; ++step) {
                    const Point oldPosition = state.p;
                    tickBoss(state, playerHealth, {1000.0f, 400.0f});
                    CHECK(distance(oldPosition, state.p) <= 1.001f && boss::incomingDamage(state, oldPosition, oldPosition, 100.0f) == 0);
                }
                CHECK(!state.counterstepping && state.stage == boss::Stage::StepWarning && state.chain == strike + 1 && state.remaining == 48);
                ++expectedWarningCount;
                CHECK(state.warnings == static_cast<std::uint64_t>(expectedWarningCount));
                CHECK(boss::incomingDamage(state, state.p, state.p, 100.0f) == 0);
                for (int tick = 0; tick < 12; ++tick) {
                    tickBoss(state, playerHealth, {1000.0f, 400.0f});
                    CHECK(boss::incomingDamage(state, state.p, state.p, 100.0f) == (tick < 11 ? 0 : 2000));
                }
                CHECK(state.remaining == 36);
            } else {
                CHECK(state.stage == boss::Stage::Recover && state.chain == phase && state.remaining == (phase == 3 ? 240 : 168));
            }
        }
    }
}
void dancerDeathCancelsCommittedStrike() {
    boss::State state; battle::PlayerHealth playerHealth{}; boss::start(state, {300, 400}, kArena, boss::Kind::Dancer);
    tickBossN(state, playerHealth, 90); tickBossN(state, playerHealth, 48); tickBossN(state, playerHealth, 4);
    CHECK(state.stage == boss::Stage::StepActive && state.strikeThisTick);
    const auto defeated = boss::applySignature(state, state.health.hp);
    CHECK(defeated.events.defeated && !boss::alive(state) && state.stage == boss::Stage::Defeated);
    CHECK(!state.strikeThisTick && !state.counterstepping && boss::incomingDamage(state, state.p, state.p, 100.0f) == 0);
}
const ranged::Projectile* findProjectile(const ranged::Pool& pool, ranged::Pool::Ticket ticket) {
    for (const auto& projectile : pool.items()) if (projectile.id == ticket && projectile.state != ranged::SlotState::Empty) return &projectile;
    return nullptr;
}
void prepareConductorCycle(boss::State& state, battle::PlayerHealth& playerHealth, int cycles) {
    boss::start(state, {640.0f, 400.0f}, kArena, boss::Kind::Conductor);
    state.metrics.cycles = static_cast<std::uint64_t>(cycles); // Select an existing deterministic cycle pattern.
    tickBossN(state, playerHealth, 90, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::RootWarning && state.remaining == 54);
}
void conductorWarningFanAndReachableRingGap() {
    boss::State fan; battle::PlayerHealth fanHealth{};
    prepareConductorCycle(fan, fanHealth, 0);
    CHECK(fan.pattern == 0 && fan.warnings == 1 && fan.shotCount == 6 && fan.budCount == 0);
    CHECK(fan.bullets.reservedCount() == 6 && fan.bullets.flyingCount() == 0);
    tickBossN(fan, fanHealth, 53, {1100.0f, 400.0f});
    CHECK(fan.stage == boss::Stage::RootWarning && fan.remaining == 1);
    tickBoss(fan, fanHealth, {1100.0f, 400.0f});
    CHECK(fan.stage == boss::Stage::RootActive && fan.remaining == 60);
    CHECK(fan.shotsFired == 6 && fan.shotCursor == 6 && fan.bullets.flyingCount() == 6);
    for (int i = 0; i < 6; ++i) {
        const auto* shot = findProjectile(fan.bullets, fan.shots[static_cast<std::size_t>(i)].ticket);
        CHECK(shot && shot->state == ranged::SlotState::Flying);
        if (shot) CHECK(shot->spec.damage == 2000 && near(shot->spec.speed, 180.0f) && near(shot->spec.radius, 5.0f) && shot->spec.lifetimeTicks == 360);
    }
    boss::State gap; battle::PlayerHealth gapHealth{};
    prepareConductorCycle(gap, gapHealth, 2);
    CHECK(gap.pattern == 2 && gap.shotCount == 7 && gap.budCount == 0);
    const Point aim = unit(subtract(Point{1100.0f, 400.0f}, gap.p));
    for (int i = 0; i < gap.shotCount; ++i) {
        const Point direction = gap.shots[static_cast<std::size_t>(i)].direction;
        CHECK(direction.x * aim.x + direction.y * aim.y <= 0.501f);
    }
    tickBossN(gap, gapHealth, 54, {1100.0f, 400.0f});
    CHECK(gap.stage == boss::Stage::RootActive && gap.remaining == 60);
    CHECK(gap.shotsFired == 7 && gap.bullets.flyingCount() == 7);
}
void conductorSequentialRewardlessBudsCancelOnlyUnfiredShot() {
    boss::State state; battle::PlayerHealth playerHealth{};
    prepareConductorCycle(state, playerHealth, 1);
    CHECK(state.pattern == 1 && state.shotCount == 3 && state.budCount == 3);
    CHECK(state.bullets.reservedCount() == 3);
    const auto cutCount = state.budsCut;
    tickBossN(state, playerHealth, 54, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::RootActive && state.remaining == 42 && state.shotCursor == 1 && state.shotsFired == 1);
    const auto firstBudShot = state.shots[0].ticket;
    const auto* fired = findProjectile(state.bullets, firstBudShot);
    CHECK(fired && fired->state == ranged::SlotState::Flying);
    if (fired) CHECK(fired->spec.damage == 2400 && fired->spec.lifetimeTicks == 360);
    CHECK(state.bullets.reservedCount() == 2 && state.bullets.flyingCount() == 1);
    const auto hit = boss::damageBud(state, 1, 200, false);
    CHECK(hit == battle::SilkHit::Killed && state.budsCut == cutCount + 1 && state.bullets.reservedCount() == 1 && state.bullets.flyingCount() == 1);
    CHECK(findProjectile(state.bullets, firstBudShot) != nullptr); // Destroying another bud preserves fired shots.
    CHECK(boss::damageBud(state, 1, 200, false) == battle::SilkHit::None);
    CHECK(state.budsCut == cutCount + 1 && state.metrics.signatureHits == 0);
    tickBossN(state, playerHealth, 42, {1100.0f, 400.0f});
    CHECK(state.shotCursor == 2 && state.shotsFired == 1); // Dead bud's reservation never launches.
    CHECK(state.bullets.reservedCount() == 1 && state.bullets.flyingCount() == 1);
    tickBossN(state, playerHealth, 42, {1100.0f, 400.0f});
    CHECK(state.shotCursor == 3 && state.shotsFired == 2);
    CHECK(state.bullets.reservedCount() == 0 && state.bullets.flyingCount() == 2);
    tickBossN(state, playerHealth, 42, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::Recover && state.remaining == 216 && state.budCount == 0);
}
void conductorFinalPatternHasEightyFourTickBudDelay() {
    boss::State state; battle::PlayerHealth playerHealth{};
    boss::start(state, {640.0f, 400.0f}, kArena, boss::Kind::Conductor);
    state.metrics.cycles = 1;
    const auto phaseSkip = boss::applySignature(state, 14701); // Leaves 29% HP and stays alive.
    CHECK(phaseSkip.events.phaseChanged && phaseSkip.events.supports == 4 && state.phase == 3);
    tickBossN(state, playerHealth, 108, {1100.0f, 400.0f}); tickBossN(state, playerHealth, 90, {1100.0f, 400.0f});
    CHECK(state.pattern == 3 && state.stage == boss::Stage::RootWarning);
    CHECK(state.shotCount == 7 && state.budCount == 1 && state.bullets.reservedCount() == 7);
    tickBossN(state, playerHealth, 54, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::RootActive && state.remaining == 84);
    CHECK(state.shotsFired == 6 && state.shotCursor == 6 && state.bullets.reservedCount() == 1);
    tickBossN(state, playerHealth, 83, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::RootActive && state.remaining == 1 && state.shotsFired == 6);
    tickBoss(state, playerHealth, {1100.0f, 400.0f});
    CHECK(state.shotsFired == 7 && state.shotCursor == 7 && state.remaining == 42);
    const auto* budShot = findProjectile(state.bullets, state.shots[6].ticket);
    CHECK(budShot && budShot->spec.damage == 2400);
    tickBossN(state, playerHealth, 42, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::Recover && state.remaining == 240);
}
void conductorPoolCapacityAndThreatCancellation() {
    boss::State state; battle::PlayerHealth playerHealth{};
    boss::start(state, {640.0f, 400.0f}, kArena, boss::Kind::Conductor);
    // Boss-only 64-slot pool; ordinary shots use a separate main pool.
    std::array<ranged::Pool::Ticket, 59> blockers{};
    for (std::size_t i = 0; i < blockers.size(); ++i)
        blockers[i] = state.bullets.reserve(static_cast<std::uint64_t>(i + 1));
    CHECK(state.bullets.freeSlots() == 5 && state.bullets.reservedCount() == 59);
    tickBossN(state, playerHealth, 90, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::Stalk && state.remaining == 30 && state.warnings == 0 && state.shotCount == 0 && state.bullets.reservedCount() == 59);
    CHECK(state.bullets.cancelReservation(blockers[0]));
    tickBossN(state, playerHealth, 29, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::Stalk && state.remaining == 1);
    tickBoss(state, playerHealth, {1100.0f, 400.0f});
    CHECK(state.stage == boss::Stage::RootWarning && state.shotCount == 6 && state.bullets.reservedCount() == ranged::kProjectileCapacity);
    CHECK(state.bullets.freeSlots() == 0 && state.bullets.reserve(999999) == 0);
    boss::State phaseCancel; battle::PlayerHealth phaseHealth{};
    prepareConductorCycle(phaseCancel, phaseHealth, 0);
    CHECK(phaseCancel.bullets.reservedCount() == 6);
    const auto phaseHit = boss::applySignature(phaseCancel, 7351);
    CHECK(phaseHit.events.phaseChanged && phaseHit.events.supports == 2);
    CHECK(phaseCancel.stage == boss::Stage::Transition && phaseCancel.remaining == 108 && phaseCancel.shotCount == 0 && phaseCancel.budCount == 0);
    CHECK(phaseCancel.bullets.reservedCount() == 0 && phaseCancel.bullets.flyingCount() == 0);
    boss::State deathCancel; battle::PlayerHealth deathHealth{};
    prepareConductorCycle(deathCancel, deathHealth, 0);
    tickBossN(deathCancel, deathHealth, 54, {1100.0f, 400.0f});
    CHECK(deathCancel.bullets.flyingCount() == 6);
    const auto defeat = boss::applySignature(deathCancel, deathCancel.health.hp);
    CHECK(defeat.events.defeated && deathCancel.stage == boss::Stage::Defeated);
    CHECK(deathCancel.bullets.flyingCount() == 0 && deathCancel.bullets.reservedCount() == 0);
}
void conductorProjectileCollisionConsumesImmunity() {
    boss::State state; battle::PlayerHealth player{10000, 10000, 0};
    boss::start(state, {640.0f, 400.0f}, kArena, boss::Kind::Conductor);
    const auto first = state.bullets.reserve(101);
    CHECK(state.bullets.launch(first, {0.0f, 0.0f}, {1.0f, 0.0f}, {180.0f, 5.0f, 2000, 360}));
    battle::tick(player);
    const auto firstImpact = boss::projectileTick(state, {1.5f, 0.0f}, {1.5f, 0.0f}, 10.0f, kArena);
    CHECK(firstImpact.count == 1 && firstImpact.items[0].damage == 2000);
    battle::IncomingHit incoming;
    incoming.consider(firstImpact.items[0].damage, firstImpact.items[0].sourceId);
    CHECK(battle::applyIncoming(player, incoming, false) == 2000 && player.hp == 8000 && player.immunityTicks == battle::kPlayerImmunityTicks);
    const auto second = state.bullets.reserve(102);
    CHECK(state.bullets.launch(second, {1.5f, 0.0f}, {1.0f, 0.0f}, {180.0f, 5.0f, 2400, 360}));
    battle::tick(player);
    const auto secondImpact = boss::projectileTick(state, {1.5f, 0.0f}, {1.5f, 0.0f}, 10.0f, kArena);
    CHECK(secondImpact.count == 1 && secondImpact.items[0].damage == 2400);
    incoming = {};
    incoming.consider(secondImpact.items[0].damage, secondImpact.items[0].sourceId);
    CHECK(battle::applyIncoming(player, incoming, false) == 0 && player.hp == 8000 && player.immunityTicks == battle::kPlayerImmunityTicks - 1);
    CHECK(state.bullets.flyingCount() == 0); // Physical collision consumes the projectile even during immunity.
}
int rankTotal(const upgrades::Ranks& ranks) {
    int total = 0;
    for (int value : ranks.value) total += value;
    return total;
}
upgrades::Ranks carryInRanks(int total, bool conductor, int archetype=0) {
    upgrades::Ranks result{};
    auto set = [&](upgrades::Id id, int rank) { result.value[static_cast<std::size_t>(upgrades::index(id))] = rank; };
    if (total == 6) { set(upgrades::Id::Razor, 2); set(upgrades::Id::Living, 1); set(upgrades::Id::Broad, 1); set(upgrades::Id::Quick, 1); set(upgrades::Id::Heartwood, 1); }
    if (total == 8) { set(upgrades::Id::Razor, 3); set(upgrades::Id::Living, 1); set(upgrades::Id::Broad, 1); set(upgrades::Id::Quick, 1); set(upgrades::Id::Heartwood, 2); }
    if (total == 9) { set(upgrades::Id::Razor, 3); set(upgrades::Id::Living, 2); set(upgrades::Id::Broad, 1); set(upgrades::Id::Quick, 1); set(upgrades::Id::Rhythm, 1); set(upgrades::Id::Heartwood, 1); }
    if (total == 12) { set(upgrades::Id::Razor, 4); set(upgrades::Id::Living, 2); set(upgrades::Id::Broad, 2); set(upgrades::Id::Quick, 1); set(upgrades::Id::Rhythm, 1); set(upgrades::Id::Heartwood, 1); set(upgrades::Id::Magnet, 1); }
    if(archetype==1){result={};set(upgrades::Id::Heartwood,conductor?4:3);set(upgrades::Id::Living,conductor?3:2);set(upgrades::Id::Broad,conductor?2:1);}
    if(archetype==2){result={};set(upgrades::Id::Quick,conductor?4:3);set(upgrades::Id::Rhythm,conductor?3:2);set(upgrades::Id::Living,conductor?2:1);}
    CHECK((total == 0 && rankTotal(result) == 0) || rankTotal(result) == total);
    CHECK((conductor && (total == 0 || total == 9 || total == 12)) ||
          (!conductor && (total == 0 || total == 6 || total == 8)));
    return result;
}
struct SilkPoint { Point p{}; int ageTicks = 0; };
struct ScenarioResult {
    boss::Kind kind{};
    bool approach=false;int ranks = 0, archetype=0, combatTicks = 0, startHp = 0, finalHp = 0, bossHp=0;
    int playerDamage = 0;
    bool playerAlive = true, bossAlive = true;
    oldBoss::Metrics metrics{};
    std::uint64_t warnings = 0, shotsFired = 0, supportsReleased = 0;
};
ScenarioResult runBotScenario(boss::Kind kind, int rankPoints, int archetype=0, bool approach=false) {
    const bool conductor = kind == boss::Kind::Conductor;
    const upgrades::Ranks ranks = carryInRanks(rankPoints, conductor, archetype);
    const upgrades::Stats stats = upgrades::derive(ranks);
    const Arena arena{0.0f, 1280.0f, 0.0f, 800.0f}; const Point center{640.0f, 400.0f};
    constexpr float orbitRadius = 225.0f;
    float angle = 0.0f, dashCooldown = 0.0f;
    int dashTicks = 0;
    Point player{center.x + orbitRadius, center.y}, previousPlayer = player;
    battle::PlayerHealth health{stats.maxHp, stats.maxHp, 0}; boss::State state;
    boss::start(state, center, arena, kind);
    afterimage::State echo; std::deque<SilkPoint> thread; ScenarioResult result;
    result.kind = kind; result.ranks = rankPoints;result.archetype=archetype;result.approach=approach; result.startHp = health.hp;
    constexpr int kScenarioLimit = 120 * 240; constexpr std::uint64_t kBossId = 0xB0550001ull;
    for (int tick = 0; tick < kScenarioLimit && boss::alive(state) && health.alive(); ++tick) {
        battle::tick(health);
        dashCooldown = std::max(0.0f, dashCooldown - 1.0f / 120.0f);
        if (dashCooldown <= 0.0f && dashTicks == 0) {
            afterimage::beginDash(echo, player, true, true);
            dashCooldown = stats.dashCooldownSeconds; dashTicks = static_cast<int>(std::ceil(threadlight::kDashDurationSeconds * 120.0f));
        }
        const bool dashActive = dashTicks > 0;
        const float speed = stats.moveSpeed * (dashActive ? 3.7f : 1.0f); angle += speed / orbitRadius / 120.0f;
        Point nextPlayer{center.x + orbitRadius * std::cos(angle), center.y + orbitRadius * std::sin(angle)};
        if(approach){
            // State-aware approach uses visible recovery/warnings, never future
            // aim or damage immunity. Movement remains limited by derived speed.
            const float reach=boss::exposed(state)?boss::vulnerableRadius(state)-3.f:155.f;
            const Point goal{std::clamp(state.p.x+reach*std::cos(angle),12.f,1268.f),std::clamp(state.p.y+reach*std::sin(angle),12.f,788.f)};
            const auto delta=boss::sub(goal,player);const float d=boss::length(delta);
            nextPlayer=boss::add(player,boss::scale(boss::unit(delta),std::min(d,speed/120.f)));
        }
        previousPlayer = player; player = nextPlayer;
        if (dashActive) {
            afterimage::appendDash(echo, previousPlayer, player, true);
            if (--dashTicks == 0) afterimage::finishDash(echo, true, true);
        }
        if (thread.empty() || distance(thread.back().p, player) > 9.0f) thread.push_back({player, 0});
        for (SilkPoint& point : thread) ++point.ageTicks;
        const int silkLifetime = static_cast<int>(std::ceil(stats.activeSilkSeconds * 120.0f));
        while (!thread.empty() && (thread.front().ageTicks > silkLifetime || thread.size() > 520)) thread.pop_front();
        boss::tick(state, player, arena); bool touched = false;
        for (std::size_t i = 1; i < thread.size(); ++i) {
            if (thread[i].ageTicks > silkLifetime ||
                !boss::silkContact(state, thread[i - 1].p, thread[i].p, stats.silkHalfWidth)) continue;
            touched = true; boss::applySilk(state, stats.silkDamage); break;
        }
        if (touched) ++state.metrics.contactTicks;
        if (boss::alive(state) && afterimage::claimContact(echo, kBossId, state.previousP, state.p, boss::vulnerableRadius(state)))
            boss::applySignature(state, afterimage::damage(stats.silkDamage, true));
        afterimage::advance(echo, true);
        battle::IncomingHit incoming;
        incoming.consider(boss::incomingDamage(state, previousPlayer, player, kPlayerRadius), kBossId);
        const auto impacts = boss::projectileTick(state, previousPlayer, player, kPlayerRadius, arena);
        for (int i = 0; i < impacts.count; ++i)
            incoming.consider(impacts.items[static_cast<std::size_t>(i)].damage,
                              impacts.items[static_cast<std::size_t>(i)].sourceId);
        const int taken = battle::applyIncoming(health, incoming, dashActive);
        result.playerDamage += taken; state.metrics.playerDamage += taken; ++result.combatTicks;
    }
    result.bossHp=state.health.hp;result.finalHp = health.hp; result.playerAlive = health.alive(); result.bossAlive = boss::alive(state);
    result.metrics = state.metrics; result.warnings = state.warnings;
    result.shotsFired = state.shotsFired; result.supportsReleased = state.supportsReleased;
    return result;
}
void printScenario(const ScenarioResult& result) {
    const char* name = result.kind == boss::Kind::Dancer ? "dancer" : "conductor";
    const char* outcome = !result.bossAlive ? "boss_defeated" : !result.playerAlive ? "player_died" : "time_cap";
    std::cout << "scenario=" << name << " ranks=" << result.ranks << " outcome=" << outcome
              << " approach=" << result.approach << " archetype=" << result.archetype << " boss_hp=" << result.bossHp << " seconds=" << result.combatTicks / 120.0
              << " player_hp=" << result.finalHp << "/" << result.startHp
              << " player_damage=" << result.playerDamage
              << " contacts=" << result.metrics.contactTicks
              << " silk_hits=" << result.metrics.hits
              << " exposed_hits=" << result.metrics.exposedHits
              << " signature_hits=" << result.metrics.signatureHits
              << " signature_damage=" << result.metrics.signatureDamage
              << " boss_cycles=" << result.metrics.cycles
              << " warnings=" << result.warnings << " shots=" << result.shotsFired
              << " supports_released=" << result.supportsReleased << " support_bodies=omitted" << '\n';
}
void carryInBotScenariosReportMeasurementsOnly() {
    const std::array<std::pair<boss::Kind, int>, 6> fixtures{{
        {boss::Kind::Dancer, 0}, {boss::Kind::Dancer, 6}, {boss::Kind::Dancer, 8},
        {boss::Kind::Conductor, 0}, {boss::Kind::Conductor, 9}, {boss::Kind::Conductor, 12}
    }};
    for(const auto kind:{boss::Kind::Dancer,boss::Kind::Conductor})for(int archetype:{1,2}){
        printScenario(runBotScenario(kind,kind==boss::Kind::Dancer?6:9,archetype,true));
    }
    for(const auto kind:{boss::Kind::Dancer,boss::Kind::Conductor})for(int points:{0,kind==boss::Kind::Dancer?6:9})
        printScenario(runBotScenario(kind,points,0,true));
    for (const auto& [kind, ranks] : fixtures) {
        const ScenarioResult result = runBotScenario(kind, ranks);
        printScenario(result);
        CHECK(result.combatTicks > 0 && result.combatTicks <= 120 * 240);
        CHECK(result.metrics.contactTicks <= static_cast<std::uint64_t>(result.combatTicks));
    }
}
} // namespace

int main() {
    rootFacadeMatchesSeededLegacyRoute();
    dancerPhaseThresholdsAndFiniteSupports();
    dancerWarningResizeAndActiveLungeRetiming();
    dancerChainHasFreshWarningsCounterstepsAndRecovery();
    dancerDeathCancelsCommittedStrike();
    conductorWarningFanAndReachableRingGap();
    conductorSequentialRewardlessBudsCancelOnlyUnfiredShot();
    conductorFinalPatternHasEightyFourTickBudDelay();
    conductorPoolCapacityAndThreatCancellation();
    conductorProjectileCollisionConsumesImmunity();
    carryInBotScenariosReportMeasurementsOnly();
    if (failures != 0) {
        std::cerr << failures << " M5 boss check(s) failed\n";
        return 1;
    }
    std::cout << "M5 boss checks passed\n";
    return 0;
}
