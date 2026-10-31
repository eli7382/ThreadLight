#include "chapter_boss.h"
#include "game_logic.h"
#include "game_layout.h"
#include "needle_interception.h"
#include <array>
#include <cmath>
#include <iostream>

namespace root = threadlight::chapter_boss;
namespace uw = threadlight::unweaver;
namespace ranged = threadlight::ranged;
namespace needle = threadlight::needle_interception;
using Point = threadlight::foundation::Point;

namespace {
constexpr root::Arena kArena{12.f, 912.f, 12.f, 496.f};
constexpr float kPlayerRadius = threadlight::layout::kPlayerRadius;
constexpr float kStep = static_cast<float>(threadlight::timing::kFixedStepSeconds);
int failures = 0;
int checks = 0;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        std::cerr << "event loom check failed at " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
    } \
} while (false)

struct PositionCase { const char* name; Point player; };
constexpr std::array<PositionCase, 5> kPositions{{
    {"center-lane", {460.f, 110.f}},
    {"northwest", {28.f, 28.f}},
    {"northeast", {896.f, 28.f}},
    {"southwest", {28.f, 480.f}},
    {"southeast", {896.f, 480.f}},
}};

struct LoomSnapshot {
    std::array<uw::Ray, 8> rays{};
    std::array<bool, 8> locked{};
};

root::State startLoom(Point player, bool dashReady) {
    root::State boss;
    root::start(boss, {460.f, 254.f}, kArena, root::Kind::Unweaver);
    auto& state = boss.unweaver;
    state.playerBounds = kArena;
    state.playerSpeed = 255.f;
    state.playerDashMultiplier = 3.7f;
    state.dashCooldown = dashReady ? 0.f : threadlight::kDashCooldownSeconds;
    state.dashRemaining = 0.f;
    state.forcedMechanic = static_cast<int>(uw::Mechanic::EventLoom);
    // Prevent Phase I's mandatory opening charge so this forced event plan
    // goes through the normal Stalk -> commit path on the first tick.
    state.huntExecuted = true;
    boss.remaining = 1;
    const auto events = root::tick(boss, player, kArena);
    CHECK(events.warning);
    CHECK(boss.stage == root::Stage::RootWarning);
    CHECK(state.planCommitted && state.loom);
    CHECK(state.mechanic == uw::Mechanic::EventLoom);
    CHECK(state.warningTicks == 96);
    CHECK(state.sequenceTicks == 481);
    CHECK(state.sequenceTicks != state.safetyTicks);
    CHECK(state.rayCount == 8);
    for (int i = 0; i < state.rayCount; ++i) {
        CHECK(state.rays[i].curveTicks == 132);
        CHECK(state.rays[i].ticket != 0);
        CHECK(state.rays[i].committed == (i < 2));
        CHECK(state.rays[i].emitted == 0 && !state.rays[i].removed);
    }
    CHECK(state.loomStage == 0 && state.loomStageTick == 0);
    CHECK(state.walls.count == 6);
    for (int i = 0; i < state.walls.count; ++i) CHECK(state.walls.walls[i].warning == 120);
    CHECK(ranged::kProjectileCapacity == 64);
    CHECK(boss.bullets.freeSlots() == 56);
    CHECK(boss.bullets.reservedCount() == 8);
    const auto impacts = root::projectileTick(boss, player, player, kPlayerRadius, kArena);
    CHECK(impacts.count == 0);
    (void)uw::finish(boss);
    return boss;
}

LoomSnapshot snapshot(const uw::State& state) {
    LoomSnapshot result;
    for (int i = 0; i < 8; ++i) {
        if (!state.rays[i].committed) continue;
        result.rays[i] = state.rays[i];
        result.locked[i] = true;
    }
    return result;
}

bool sameCurve(const uw::Ray& a, const uw::Ray& b) {
    return a.origin.x == b.origin.x && a.origin.y == b.origin.y &&
           a.control.x == b.control.x && a.control.y == b.control.y &&
           a.end.x == b.end.x && a.end.y == b.end.y &&
           a.curveTicks == b.curveTicks && a.flightTicks == b.flightTicks &&
           a.fireTick == b.fireTick;
}

struct ReplayResult { bool admitted = false; int unsafeContacts = 0; int ticks = 0; };
ReplayResult replayLoom(const PositionCase& scenario, bool dashReady) {
    auto boss = startLoom(scenario.player, dashReady);
    auto& state = boss.unweaver;
    ReplayResult result;
    if (!state.planCommitted || !state.loom) return result;
    result.admitted = true;
    Point player = scenario.player;
    Point held{};
    float dashTime = 0.f;
    float dashCooldown = state.dashCooldown;
    LoomSnapshot locked = snapshot(state);
    int lastStage = state.loomStage;
    for (int frame = 0; frame < state.sequenceTicks + 2 &&
         (boss.stage == root::Stage::RootWarning || boss.stage == root::Stage::RootActive); ++frame) {
        const int clock = state.clock;
        dashCooldown = std::max(0.f, dashCooldown - kStep);
        const Point movement = uw::responseDirection(state, player, clock, state.safeRoute, held);
        held = movement;
        const bool dashPressed = state.safeUsesDash && dashTime <= 0.f && dashCooldown <= 0.f &&
                                 clock % 4 == 0 && uw::dashWindow(state, clock);
        if (dashPressed) {
            dashTime = threadlight::kDashDurationSeconds;
            dashCooldown = threadlight::kDashCooldownSeconds;
        }
        const bool dashProtected = threadlight::dashProtectsCollisionThisFrame(dashTime);
        const Point previous = player;
        const float multiplier = dashProtected ? state.playerDashMultiplier : 1.f;
        player = uw::clamp(uw::add(player, uw::scale(movement, state.playerSpeed * multiplier / 120.f)),
                           state.playerBounds);
        if (dashProtected) dashTime = std::max(0.f, dashTime - kStep);
        const Point unblocked = player;
        player = uw::barriers::move(state.walls, previous, player, kPlayerRadius);
        state.playerVelocity = uw::scale(uw::sub(player, previous), 120.f);
        state.dashCooldown = dashCooldown;
        state.dashRemaining = dashTime;

        root::tick(boss, player, kArena);
        for (int i = 0; i < state.rayCount; ++i) {
            if (locked.locked[i]) CHECK(sameCurve(locked.rays[i], state.rays[i]));
            if (state.rays[i].committed && !locked.locked[i]) {
                locked.rays[i] = state.rays[i];
                locked.locked[i] = true;
            }
        }
        if (state.loomStage != lastStage) {
            CHECK(state.loomStage == lastStage + 1);
            CHECK(state.loomStageTick == clock + 1);
            CHECK(state.loomStageTick % 84 == 0);
            const int firstRay = state.loomStage * 2;
            CHECK(state.rays[firstRay].committed && state.rays[firstRay + 1].committed);
            CHECK(state.rays[firstRay].fireTick == state.loomStageTick + state.warningTicks);
            CHECK(state.rays[firstRay + 1].fireTick == state.loomStageTick + state.warningTicks);
            lastStage = state.loomStage;
        }
        CHECK(std::isfinite(unblocked.x) && std::isfinite(unblocked.y));
        const int incoming = root::incomingDamage(boss, previous, player, kPlayerRadius);
        auto block = [&](ranged::Pool& pool) {
            for (const auto& shot : pool.items()) {
                if (shot.state != ranged::SlotState::Flying) continue;
                const Point next = uw::add(shot.p, uw::scale(shot.velocity, 1.f / 120.f));
                if (uw::barriers::blocksProjectile(state.walls, shot.p, next, shot.spec.radius))
                    pool.intercept(shot.id);
            }
        };
        block(boss.bullets);
        const auto impacts = root::projectileTick(boss, previous, player, kPlayerRadius, kArena);
        if (!dashProtected && (incoming > 0 || impacts.count > 0)) {
            ++result.unsafeContacts;
            std::cerr << "unprotected event loom contact position=" << scenario.name
                      << " dash_ready=" << dashReady << " route=" << state.safeRoute
                      << " tick=" << clock << " incoming=" << incoming
                      << " projectile_hits=" << impacts.count << '\n';
        }
        CHECK(boss.bullets.freeSlots() + boss.bullets.reservedCount() +
              boss.bullets.flyingCount() == ranged::kProjectileCapacity);
        (void)uw::finish(boss);
        ++result.ticks;
    }
    CHECK(result.ticks <= state.sequenceTicks + 2);
    CHECK(boss.stage == root::Stage::Recover || boss.stage == root::Stage::Stalk);
    return result;
}

void admissionAndReplayMatrix() {
    std::array<int, kPositions.size()> admittedByPosition{};
    int admittedReady = 0;
    int admittedExhausted = 0;
    int totalAdmitted = 0;
    for (std::size_t position = 0; position < kPositions.size(); ++position) {
        for (bool dashReady : {true, false}) {
            const ReplayResult result = replayLoom(kPositions[position], dashReady);
            if (!result.admitted) continue;
            ++admittedByPosition[position];
            ++totalAdmitted;
            if (dashReady) ++admittedReady;
            else ++admittedExhausted;
            CHECK(result.unsafeContacts == 0);
        }
    }
    for (std::size_t i = 0; i < kPositions.size(); ++i) {
        CHECK(admittedByPosition[i] > 0);
        if (admittedByPosition[i] == 0)
            std::cerr << "event loom never admitted position=" << kPositions[i].name << '\n';
    }
    CHECK(totalAdmitted > 0 && admittedReady > 0 && admittedExhausted > 0);
}

void stationaryCornerTakesRealFirstCastDamage() {
    const Point player{28.f, 28.f};
    auto boss = startLoom(player, true);
    CHECK(boss.unweaver.planCommitted && boss.unweaver.loom);
    int contacts = 0;
    const int endTick = boss.unweaver.rays[0].fireTick +
                        boss.unweaver.rays[0].flightTicks + 2;
    for (int frame = 0; frame < endTick + 2 &&
         (boss.stage == root::Stage::RootWarning || boss.stage == root::Stage::RootActive); ++frame) {
        boss.unweaver.playerVelocity = {};
        boss.unweaver.dashRemaining = 0.f;
        root::tick(boss, player, kArena);
        const int incoming = root::incomingDamage(boss, player, player, kPlayerRadius);
        const auto impacts = root::projectileTick(boss, player, player, kPlayerRadius, kArena);
        if (incoming > 0 || impacts.count > 0) ++contacts;
        (void)uw::finish(boss);
    }
    CHECK(contacts > 0);
    if (contacts == 0) std::cerr << "stationary corner had no first-cast Loom damage\n";
}

bool launchFirstCurve(root::State& boss, Point player) {
    for (int tick = 0; tick < 130; ++tick) {
        root::tick(boss, player, kArena);
        if (boss.unweaver.clock >= boss.unweaver.rays[0].fireTick && boss.bullets.flyingCount() > 0)
            return true;
        (void)root::projectileTick(boss, player, player, kPlayerRadius, kArena);
        (void)uw::finish(boss);
    }
    return false;
}

void curvedNeedlecatchUsesNextSweep() {
    const Point player{690.f, 254.f};
    auto boss = startLoom(player, true);
    CHECK(launchFirstCurve(boss, player));
    const ranged::Projectile* curve = nullptr;
    for (const auto& projectile : boss.bullets.items())
        if (projectile.state == ranged::SlotState::Flying && projectile.curved) {
            curve = &projectile;
            break;
        }
    CHECK(curve != nullptr);
    if (!curve) return;
    CHECK(curve->curveAge == 0);
    const Point next = ranged::quadraticBezier(curve->curveOrigin, curve->curveControl,
                                                curve->curveEnd, 1.f / curve->curveTicks);
    const Point delta = uw::sub(next, curve->p);
    const float length = uw::length(delta);
    CHECK(length > 0.f);
    if (length <= 0.f) return;
    const Point perpendicular{-delta.y / length, delta.x / length};
    const Point middle = uw::scale(uw::add(curve->p, next), .5f);
    const Point offset = uw::scale(perpendicular, 25.f);
    const std::array<needle::Segment, 1> silk{{{uw::sub(middle, offset), uw::add(middle, offset)}}};
    ranged::Pool ordinary;
    const auto caught = needle::find(ordinary, boss.bullets, silk, 4.f,
                                     player, player, kPlayerRadius,
                                     {kArena.left, kArena.right, kArena.top, kArena.bottom});
    CHECK(caught.id == curve->id && caught.pool == 1);
    CHECK(boss.bullets.intercept(caught.id));
    CHECK(!boss.bullets.intercept(caught.id));
}

void phaseCleanupClearsLoomCurves() {
    const Point player{690.f, 254.f};
    auto boss = startLoom(player, true);
    CHECK(launchFirstCurve(boss, player));
    bool hasCurved = false;
    for (const auto& projectile : boss.bullets.items()) hasCurved |= projectile.curved;
    CHECK(hasCurved);
    CHECK(boss.unweaver.loom && boss.unweaver.rayCount == 8);

    boss.unweaver.progress.elapsedTicks = 7199;
    root::tick(boss, player, kArena);
    (void)root::projectileTick(boss, player, player, kPlayerRadius, kArena);
    const auto boundary = uw::finish(boss);
    CHECK(boundary.phaseChanged && boss.phase == 2);
    CHECK(!boss.unweaver.loom && boss.unweaver.rayCount == 0);
    CHECK(boss.bullets.flyingCount() == 0 && boss.bullets.reservedCount() == 0);
    CHECK(boss.unweaver.walls.count == 0 && boss.unweaver.pressure.count == 0);
}
void earnedFormationOpeningPrecedesCounter() {
    const Point player{460.f,110.f};auto boss=startLoom(player,true);auto& u=boss.unweaver;
    const int duration=u.sequenceTicks;
    for(int i=0;i<duration-1;++i){root::tick(boss,player,kArena);(void)root::projectileTick(boss,player,player,kPlayerRadius,kArena);}
    u.hitDuringSequence=1;u.responseDashUsed=true;u.formationBreakPending=true;
    root::tick(boss,player,kArena);
    CHECK(boss.stage==root::Stage::Recover);
    CHECK(boss.remaining==300&&u.recoveryBonus==160&&u.opening==1);
    CHECK(!u.formationBreakPending&&!u.validCharge);
}
void wallCommitIsTransactional() {
    const Point player{460.f,110.f};
    auto active=startLoom(player,true);
    const auto wall=active.unweaver.walls.walls[0];
    CHECK(!uw::commit(active,{600.f,300.f},kArena));
    CHECK(active.unweaver.walls.walls[0].a.x==wall.a.x);
    CHECK(active.unweaver.walls.walls[0].a.y==wall.a.y);
    CHECK(active.unweaver.walls.walls[0].warning==wall.warning);
    root::State full;
    root::start(full,{460.f,254.f},kArena,root::Kind::Unweaver);
    full.unweaver.forcedMechanic=(int)uw::Mechanic::EventLoom;
    while(full.bullets.freeSlots())CHECK(full.bullets.reserve(full.nextSource++)!=0);
    CHECK(!uw::commit(full,player,kArena));
    CHECK(full.unweaver.walls.count==0);
}

}

int main() {
    admissionAndReplayMatrix();
    stationaryCornerTakesRealFirstCastDamage();
    curvedNeedlecatchUsesNextSweep();
    phaseCleanupClearsLoomCurves();
    earnedFormationOpeningPrecedesCounter();
    wallCommitIsTransactional();
    if (failures) {
        std::cerr << failures << " / " << checks << " event loom checks failed\n";
        return 1;
    }
    std::cout << "Unweaver event loom integration checks passed: " << checks << " checks\n";
    return 0;
}
