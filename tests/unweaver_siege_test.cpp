#include "chapter_boss.h"
#include "cocoon.h"
#include "game_logic.h"
#include "player_upgrades.h"
#include "unweaver.h"
#include <array>
#include <cmath>
#include <iostream>

namespace root = threadlight::chapter_boss;
namespace uw = threadlight::unweaver;
namespace barriers = threadlight::unweaver::barriers;
namespace combat = threadlight::combat;
namespace upgrades = threadlight::upgrades;
namespace cocoon = threadlight::cocoon;
using Point = threadlight::foundation::Point;

namespace {
int gFailures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    ++gFailures; std::cerr << "CHECK failed: " #condition " at " << __FILE__ << ':' << __LINE__ << '\n'; \
    return; } } while (false)

constexpr root::Arena kArena{12.f, 912.f, 12.f, 496.f};
constexpr Point kCenter{462.f, 254.f};
constexpr std::uint64_t kPressureSource = 0x5a17u;

root::State makePhaseFour() {
    root::State boss;
    root::start(boss, kCenter, kArena, root::Kind::Unweaver);
    boss.phase = boss.unweaver.progress.phase = 4;
    boss.stage = root::Stage::Recover;
    boss.remaining = 20000;
    boss.unweaver.playerBounds = kArena;
    boss.unweaver.arenaBounds = kArena;
    boss.unweaver.playerSpeed = 315.f;
    boss.unweaver.playerDashMultiplier = 3.7f;
    return boss;
}

upgrades::Stats defenseBuild() {
    upgrades::Ranks ranks;
    ranks.value[upgrades::index(upgrades::Id::Heartwood)] = 5;
    ranks.value[upgrades::index(upgrades::Id::Ironbark)] = 5;
    ranks.value[upgrades::index(upgrades::Id::Quick)] = 5;
    ranks.value[upgrades::index(upgrades::Id::Rhythm)] = 1;
    return upgrades::derive(ranks);
}

void activationBoundaryAndWarningPolicy() {
    auto normal = makePhaseFour();
    for (int i = 0; i < 300; ++i) root::tick(normal, kCenter, kArena);
    CHECK(normal.unweaver.progress.elapsedTicks == 300);
    CHECK(!normal.unweaver.drive.active);
    CHECK(normal.unweaver.pressure.count == 0);

    auto boundary = makePhaseFour();
    boundary.unweaver.progress.elapsedTicks = 35998;
    root::tick(boundary, kCenter, kArena);
    CHECK(boundary.unweaver.progress.elapsedTicks == 35999);
    CHECK(!boundary.unweaver.drive.active);
    root::tick(boundary, kCenter, kArena);
    CHECK(boundary.unweaver.progress.elapsedTicks == 36000);
    CHECK(boundary.unweaver.drive.active && boundary.unweaver.drive.age == 0);

    auto locked = makePhaseFour();
    locked.unweaver.progress.elapsedTicks = 35999;
    locked.unweaver.planCommitted = true;
    locked.unweaver.warningTicks = 120;
    locked.unweaver.clock = 1;
    locked.unweaver.sequenceTicks = 600;
    locked.stage = root::Stage::RootWarning;
    locked.remaining = 80;
    root::tick(locked, kCenter, kArena);
    CHECK(locked.unweaver.drive.active);
    CHECK(locked.unweaver.warningTicks == 120);
    CHECK(locked.remaining == 79);

    auto fresh = makePhaseFour();
    fresh.unweaver.drive.active = true;
    CHECK(uw::commit(fresh, {720.f, 254.f}, kArena));
    CHECK(fresh.unweaver.warningTicks == fresh.unweaver.drive.warning(102));
    CHECK(fresh.unweaver.warningTicks == 68);
}

void rewardUtilityAndReset() {
    auto boss = makePhaseFour();
    auto& drive = boss.unweaver.drive;
    drive.tick(4, 35999);
    CHECK(!drive.active);
    drive.tick(4, 36000);
    CHECK(drive.active && drive.age == 0);
    constexpr int action = static_cast<int>(uw::Action::Charge);
    drive.attempt(action);
    drive.outcome(action, 500, false);
    const float ordinaryUtility = drive.utility(action);
    CHECK(drive.damage == 500 && drive.kills == 0);
    CHECK(ordinaryUtility > 0.f);
    drive.outcome(action, 50, true);
    CHECK(drive.damage == 550 && drive.kills == 1);
    CHECK(drive.reward[action] > 160.f);
    CHECK(drive.utility(action) > ordinaryUtility);

    boss.unweaver.profile = {};
    boss.unweaver.activeTicks = 100;
    boss.unweaver.radialTicks = 30;
    CHECK(uw::selectAction(boss, {600.f, 254.f}) == uw::Action::Charge);

    root::start(boss, kCenter, kArena, root::Kind::Unweaver);
    CHECK(!boss.unweaver.drive.active && boss.unweaver.drive.age == 0);
    CHECK(boss.unweaver.drive.damage == 0 && boss.unweaver.drive.kills == 0);
    CHECK(boss.unweaver.drive.utility(action) == 0.f);
}

void shrunkBodyGeometryAndSupportSteering() {
    auto boss = makePhaseFour();
    CHECK(std::abs(uw::kVulnerableRadius - 76.f * .88f) < .001f);
    CHECK(std::abs(uw::kContactRadius - 32.f * .88f) < .001f);
    CHECK(std::abs(root::vulnerableRadius(boss) - 76.f * .88f) < .001f);
    CHECK(std::abs(root::contactRadius(boss) - 32.f * .88f) < .001f);

    constexpr float clearance = 80.f, budget = 3.f;
    const Point oldNear{510.f, 300.f}, proposedThroughBoss{509.5f, 300.f}, fixedBoss{500.f, 300.f};
    const Point pushed = uw::steerSupport(oldNear, proposedThroughBoss, fixedBoss, {700.f, 300.f}, clearance, budget, 1,
                                           {0.f, 1000.f, 0.f, 600.f});
    CHECK(uw::length(uw::sub(pushed, oldNear)) <= budget + .001f);
    CHECK(uw::length(uw::sub(pushed, fixedBoss)) > uw::length(uw::sub(oldNear, fixedBoss)));
    CHECK(threadlight::foundation::segmentDistanceSquared(fixedBoss, oldNear, pushed) > .01f);

    const Point oldOutside{600.f, 300.f}, proposedAcross{400.f, 320.f};
    const Point steered = uw::steerSupport(oldOutside, proposedAcross, fixedBoss, {400.f, 500.f}, clearance, budget, -1,
                                            {0.f, 1000.f, 0.f, 600.f});
    CHECK(uw::length(uw::sub(steered, oldOutside)) <= budget + .001f);
    CHECK(threadlight::foundation::segmentDistanceSquared(fixedBoss, oldOutside, steered) >= clearance * clearance);
}

void cocoonBarrierUsesRealDamageGate() {
    auto health = combat::PlayerHealth{21000, 21000, 0}; // Heartwood 5 plus a 50 HP test bonus.
    cocoon::State shield;
    shield.barrierCenti = cocoon::kBarrierCapacityCenti;
    shield.remainingTicks = cocoon::kBarrierDurationTicks;
    const int originalHp = health.hp;
    combat::IncomingHit spike;
    spike.consider(barriers::kSpikeDamage, kPressureSource);
    const bool dashProtected = threadlight::dashProtectsCollisionThisFrame(0.0f);
    CHECK(!dashProtected);
    const int taken = combat::applyIncoming(health, spike, dashProtected, &shield.barrierCenti, 25);
    CHECK(taken == 450);
    CHECK(health.hp == originalHp - taken);
    CHECK(shield.barrierCenti == 0);
    CHECK(health.immunityTicks == combat::kPlayerImmunityTicks);

    combat::PlayerHealth dashed{10000, 10000, 0};
    const float dashTime = threadlight::kDashDurationSeconds;
    const bool protectedThisFrame = threadlight::dashProtectsCollisionThisFrame(dashTime);
    CHECK(protectedThisFrame);
    CHECK(combat::applyIncoming(dashed, spike, protectedThisFrame) == 0);
    CHECK(dashed.hp == dashed.maxHp && dashed.immunityTicks == 0);
}

void pressureEscalationUsesOrdinaryPlayerDamage() {
    auto boss = makePhaseFour();
    boss.unweaver.progress.elapsedTicks = 35999;
    auto stats = defenseBuild();
    CHECK(stats.maxHp == 16000);
    CHECK(stats.damageReductionPercent == 25);
    CHECK(std::abs(stats.moveSpeed - 315.f) < .001f);
    CHECK(std::abs(stats.dashCooldownSeconds - 1.70f) < .001f);
    combat::PlayerHealth health{stats.maxHp + 5000, stats.maxHp + 5000, 0}; // Include a 50 HP test bonus.
    Point player = kCenter, previous = player;
    std::uint64_t firstContact = 0, firstDamage = 0, deathTick = 0;
    int totalDamage = 0, hits = 0, maxDamage = 0;
    float dashRemaining = 0.f, dashCooldown = 0.f;

    for (int tick = 1; tick <= 7200 && health.alive(); ++tick) {
        combat::tick(health);
        previous = player; // The test stays at arena center while it remains safe there.
        root::tick(boss, player, kArena);
        const int environmental = barriers::damage(boss.unweaver.pressure, previous, player, 12.f);
        if (environmental && firstContact == 0) firstContact = static_cast<std::uint64_t>(tick);
        combat::IncomingHit incoming;
        incoming.consider(environmental, kPressureSource);
        dashCooldown = std::max(0.f, dashCooldown - 1.f / 120.f);
        if (environmental && dashCooldown <= 0.f) {
            dashRemaining = threadlight::kDashDurationSeconds;
            dashCooldown = stats.dashCooldownSeconds;
        }
        const bool dashProtected = threadlight::dashProtectsCollisionThisFrame(dashRemaining);
        dashRemaining = std::max(0.f, dashRemaining - 1.f / 120.f);
        const int damage = combat::applyIncoming(health, incoming, dashProtected, nullptr, stats.damageReductionPercent);
        if (damage) {
            if (!firstDamage) firstDamage = static_cast<std::uint64_t>(tick);
            ++hits;
            totalDamage += damage;
            maxDamage = std::max(maxDamage, damage);
        }
        if (!health.alive()) deathTick = static_cast<std::uint64_t>(tick);
    }
    std::cout << "unweaver_siege_pressure_ticks=" << boss.unweaver.progress.elapsedTicks - 35999
              << " drive_age=" << boss.unweaver.drive.age << " first_contact_tick=" << firstContact
              << " first_damage_tick=" << firstDamage << " damage_hits=" << hits << " total_damage=" << totalDamage
              << " largest_hit=" << maxDamage << " hp_remaining=" << health.hp << " death_tick=" << deathTick << '\n';
    CHECK(boss.unweaver.progress.elapsedTicks >= 36000);
    CHECK(boss.unweaver.drive.active && boss.unweaver.drive.age >= 4800);
    CHECK(boss.unweaver.pressure.count == 4);
    CHECK(firstContact > 0 && firstDamage > 0);
    CHECK(hits > 0 && totalDamage > 0);
    CHECK(deathTick > 0 && deathTick <= 7200);
}
}

int main() {
    activationBoundaryAndWarningPolicy();
    rewardUtilityAndReset();
    shrunkBodyGeometryAndSupportSteering();
    cocoonBarrierUsesRealDamageGate();
    pressureEscalationUsesOrdinaryPlayerDamage();
    if (gFailures != 0) return 1;
    std::cout << "Unweaver siege activation, rewards, body scale, support steering and environmental pressure passed\n";
    return 0;
}
