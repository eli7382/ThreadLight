#include "../src/combat.h"
#include "../src/game_layout.h"
#include "../src/game_logic.h"
#include "../src/player_upgrades.h"
#include "../src/rootwarden.h"
#include "../src/spark_pickup.h"
#include "../src/visual_effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << line << ": " << expression << '\n';
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

namespace combat = threadlight::combat;
namespace foundation = threadlight::foundation;
namespace layout = threadlight::layout;
namespace root = threadlight::rootwarden;
namespace upgrades = threadlight::upgrades;
using Point = foundation::Point;
using Arena = root::Arena;

constexpr int kSimulationLimitTicks = 180 * foundation::kTicksPerSecond;
constexpr float kDt = 1.0f / foundation::kTicksPerSecond;
constexpr float kPlayerRadius = layout::kPlayerRadius;
constexpr float kTrailPointSpacing = 9.0f;
constexpr float kDashSpeedMultiplier = 3.7f;
constexpr float kRecoveryRadius = 48.0f;

struct BotPolicy {
    const char* label;
    float outerRadius;
    float recoveryRadius;
    float angularSpeed;
    int recoveryDelayTicks;
};

constexpr BotPolicy kApproachPolicy{"approach75_close48", 75.0f, 48.0f, 2.4f, 0};
constexpr BotPolicy kCautiousPolicy{"cautious110_wait36_close48", 110.0f, 48.0f, 1.9f, 36};

struct ArenaFixture {
    Arena boss;
    layout::PlayBounds playerBounds;
    const char* label;
};

ArenaFixture mainArena() {
    const auto bounds = layout::playBounds(1280, 800);
    return {{bounds.left, bounds.right, bounds.top, bounds.bottom}, bounds, "1280x672"};
}

ArenaFixture minimumArena() {
    const auto bounds = layout::playBounds(960, 640);
    return {{bounds.left, bounds.right, bounds.top, bounds.bottom}, bounds, "960x512"};
}

struct Build {
    const char* name;
    upgrades::Ranks ranks{};
};

std::array<Build, 5> builds() {
    std::array<Build, 5> result{};
    result[0].name = "zero";

    result[1].name = "offense_razorIII";
    result[1].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Razor))] = 3;

    result[2].name = "defense_heartwoodIII";
    result[2].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Heartwood))] = 3;

    result[3].name = "mobility_quickII_rhythmI";
    result[3].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Quick))] = 2;
    result[3].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Rhythm))] = 1;

    result[4].name = "mixed_razorI_livingI_heartwoodI";
    result[4].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Razor))] = 1;
    result[4].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Living))] = 1;
    result[4].ranks.value[static_cast<std::size_t>(upgrades::index(upgrades::Id::Heartwood))] = 1;
    return result;
}

int rankTotal(const upgrades::Ranks& ranks) {
    int total = 0;
    for (const int value : ranks.value) total += value;
    return total;
}

Point add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point scale(Point p, float amount) { return {p.x * amount, p.y * amount}; }
float length(Point p) { return std::sqrt(p.x * p.x + p.y * p.y); }

Point moveToward(Point from, Point target, float maxDistance) {
    const Point delta = sub(target, from);
    const float distance = length(delta);
    if (distance <= maxDistance || distance <= 0.001f) return target;
    return add(from, scale(delta, maxDistance / distance));
}

struct TrailPoint {
    Point p{};
    float age = 0.0f;
};

struct Support {
    Point p{}, velocity{};
    combat::EnemyHealth health{};
    float phase = 0.0f;
    foundation::RewardId rewardId = 0;
};

struct Pickup {
    Point p{};
    foundation::PickupState state{};
    bool bossCore = false;
};

struct RunSummary {
    const char* build = "";
    const char* arena = "";
    const char* route = "";
    int startAngleDegrees = 0;
    const char* outcome = "cap";
    bool win = false;
    float seconds = 0.0f;
    float hp = 0.0f;
    std::uint64_t hits = 0;
    std::uint64_t exposedHits = 0;
    float bossDamage = 0.0f;
    float playerDamage = 0.0f;
    std::uint64_t cycles = 0;
    int coreCreated = 0;
    int coreCollected = 0;
    int supportSpawns = 0;
    int supportCancelled = 0;
};

struct Run {
    upgrades::Ranks ranks{};
    upgrades::Stats stats{};
    combat::PlayerHealth health{};
    Arena bossArena{};
    layout::PlayBounds playerBounds{};
    BotPolicy policy{};
    root::State boss{};
    foundation::RewardLedger rewards{};
    foundation::RewardId bossRewardId = 0;
    foundation::WaveLifecycle lifecycle{};
    Point player{}, previousPlayer{};
    float orbitAngle = 0.0f;
    float dashRemaining = 0.0f;
    float dashCooldown = 0.0f;
    bool dashProtectedThisTick = false;
    int recoveryTicksObserved = 0;
    int combatTicks = 0;
    int supportSpawns = 0;
    int supportCancelled = 0;
    int coreCreated = 0;
    int coreCollected = 0;
    int collectionTicks = 0;
    bool timeoutObserved = false;
    std::deque<TrailPoint> trail;
    std::vector<Support> supports;
    std::vector<Pickup> pickups;

    Run(const Build& build, ArenaFixture arena, float initialBossAngle, BotPolicy botPolicy)
        : ranks(build.ranks), stats(upgrades::derive(ranks)),
          health{stats.maxHp, stats.maxHp, 0}, bossArena(arena.boss),
          playerBounds(arena.playerBounds), policy(botPolicy) {
        const Point center{(bossArena.left + bossArena.right) * 0.5f,
                           (bossArena.top + bossArena.bottom) * 0.5f};
        player = previousPlayer = center;
        const Point bossStart{center.x + 230.0f * std::cos(initialBossAngle),
                              center.y + 230.0f * std::sin(initialBossAngle)};
        root::start(boss, bossStart, bossArena);
        orbitAngle = std::atan2(player.y - boss.p.y, player.x - boss.p.x);
        bossRewardId = rewards.registerEnemy(root::kCoreValue, 250);
        lifecycle.start({1, true, foundation::kCountdownTicks});
        for (int i = 0; i < foundation::kCountdownTicks; ++i)
            lifecycle.tick(0, false, false, true);
    }
};

void ageAndExtendTrail(Run& run) {
    if (run.trail.empty() || length(sub(run.trail.back().p, run.player)) > kTrailPointSpacing)
        run.trail.push_back({run.player, 0.0f});
    for (TrailPoint& point : run.trail) point.age += kDt;
    while (!run.trail.empty() &&
           (run.trail.front().age > threadlight::visual::kSilkLifetimeSeconds || run.trail.size() > 520)) {
        run.trail.pop_front();
    }
}

void spawnSupports(Run& run, int count) {
    CHECK(count >= 0);
    CHECK(run.supportSpawns + count <= root::kSupportBudget);
    const combat::EnemyDefinition definition = combat::definition(combat::EnemyKind::Hunter);
    const float width = run.bossArena.right - run.bossArena.left;
    const float height = run.bossArena.bottom - run.bossArena.top;
    const float padding = definition.radius + 5.0f;
    for (int i = 0; i < count; ++i) {
        // Match main's safe-spawn request and angular schedule for ordinary Hunters.
        const float angle = threadlight::chapter::kPi * 0.25f +
            threadlight::chapter::kPi * static_cast<float>(run.boss.supportsReleased + i) * 0.5f;
        const auto spawn = threadlight::chooseSafeSpawn(
            angle, run.player.x - run.bossArena.left, run.player.y - run.bossArena.top,
            width, height, padding, std::max(padding, width * 0.53f),
            std::max(padding, height * 0.53f), kPlayerRadius + definition.radius + 70.0f);
        Support support;
        support.p = {spawn.x + run.bossArena.left, spawn.y + run.bossArena.top};
        support.health = {definition.hp, definition.hp, 0};
        support.phase = angle;
        support.rewardId = run.rewards.registerEnemy(definition.sparkValue, definition.killScore);
        CHECK(support.rewardId != 0);
        run.supports.push_back(support);
        ++run.supportSpawns;
    }
}

void cancelLiveSupports(Run& run) {
    const foundation::RewardCounters before = run.rewards.counters();
    for (const Support& support : run.supports) {
        if (run.rewards.despawnEnemy(support.rewardId)) ++run.supportCancelled;
    }
    run.supports.clear();
    const foundation::RewardCounters after = run.rewards.counters();
    CHECK(after.despawned - before.despawned == static_cast<std::uint64_t>(run.supportCancelled));
    CHECK(after.killed == before.killed);
    CHECK(after.pickupsCreated == before.pickupsCreated);
    CHECK(after.sparksCreated == before.sparksCreated);
}

void tickPickups(Run& run, bool combatActive) {
    for (Pickup& pickup : run.pickups) {
        const auto result = foundation::tickPickup(
            pickup.state, pickup.p, run.previousPlayer, run.player, kPlayerRadius,
            combatActive, true, run.stats.pickupRadius, run.stats.pickupPullSpeed);
        if (result == foundation::PickupResult::Collected) {
            if (run.rewards.collectPickup(pickup.state.id) && pickup.bossCore)
                ++run.coreCollected;
        } else if (result == foundation::PickupResult::Expired) {
            run.rewards.expirePickup(pickup.state.id);
        }
    }
    run.pickups.erase(std::remove_if(run.pickups.begin(), run.pickups.end(), [](const Pickup& pickup) {
        return pickup.state.terminal != foundation::PickupResult::None;
    }), run.pickups.end());
}

void updateSupports(Run& run, combat::IncomingHit& incoming) {
    std::vector<Support> survivors;
    survivors.reserve(run.supports.size());
    for (Support& support : run.supports) {
        const combat::EnemyDefinition definition = combat::definition(combat::EnemyKind::Hunter);
        const Point oldPosition = support.p;
        support.phase += kDt;
        combat::tick(support.health);
        const Point toPlayer = sub(run.player, support.p);
        const float distance = length(toPlayer);
        const Point direction = distance > 0.001f ? scale(toPlayer, 1.0f / distance) : Point{};
        const float wobble = std::sin(support.phase * 3.0f) * 0.28f;
        const Point steering{direction.x - direction.y * wobble,
                             direction.y + direction.x * wobble};
        support.velocity = add(support.velocity,
                               scale(steering, definition.speed * kDt * 2.5f));
        const float velocityLength = length(support.velocity);
        if (velocityLength > definition.speed)
            support.velocity = scale(support.velocity, definition.speed / velocityLength);
        support.p = add(support.p, scale(support.velocity, kDt));

        bool defeated = false;
        for (std::size_t i = 1; i < run.trail.size(); ++i) {
            const TrailPoint& first = run.trail[i - 1];
            const TrailPoint& second = run.trail[i];
            if (second.age > run.stats.activeSilkSeconds) continue;
            if (!combat::silkContact(oldPosition, support.p, definition.radius,
                                    first.p, second.p, run.stats.silkHalfWidth)) continue;
            const combat::SilkHit hit = combat::applySilk(support.health, run.stats.silkDamage);
            if (hit == combat::SilkHit::None) break;
            if (hit == combat::SilkHit::Killed) {
                if (const auto reward = run.rewards.defeatEnemy(support.rewardId)) {
                    Pickup pickup;
                    pickup.p = support.p;
                    pickup.state.id = reward->id;
                    run.pickups.push_back(pickup);
                }
                defeated = true;
            }
            break;
        }

        if (defeated || !support.health.alive()) continue;
        if (combat::bodyContact(run.previousPlayer, run.player, kPlayerRadius,
                                oldPosition, support.p, definition.radius)) {
            incoming.consider(definition.contactDamage, support.rewardId);
        }
        survivors.push_back(support);
    }
    run.supports.swap(survivors);
}

Point orbitGoal(Run& run) {
    // This is an idealized state-aware bot policy, not human input: it knows the
    // boss stage and closes only for the exposed Recover window.
    run.orbitAngle += run.policy.angularSpeed * kDt;
    float radius = run.policy.outerRadius;
    if (run.boss.stage == root::Stage::Recover) {
        if (run.recoveryTicksObserved >= run.policy.recoveryDelayTicks)
            radius = run.policy.recoveryRadius;
        ++run.recoveryTicksObserved;
    } else {
        run.recoveryTicksObserved = 0;
    }
    const Point desired{run.boss.p.x + std::cos(run.orbitAngle) * radius,
                        run.boss.p.y + std::sin(run.orbitAngle) * radius};
    return {layout::clampPlayerX(desired.x, run.playerBounds),
            layout::clampPlayerY(desired.y, run.playerBounds)};
}

bool prospectiveDanger(const Run& run, Point candidate) {
    if (root::incomingDamage(run.boss, run.player, candidate, kPlayerRadius) > 0) return true;
    for (const Support& support : run.supports) {
        if (!support.health.alive()) continue;
        const auto definition = combat::definition(combat::EnemyKind::Hunter);
        if (combat::bodyContact(run.player, candidate, kPlayerRadius,
                                support.p, support.p, definition.radius)) return true;
    }
    return false;
}

void movePlayer(Run& run, Point goal) {
    combat::tick(run.health);
    run.dashCooldown = std::max(0.0f, run.dashCooldown - kDt);
    const Point normalCandidate = moveToward(run.player, goal, run.stats.moveSpeed * kDt);
    if (run.dashCooldown <= 0.0f && run.dashRemaining <= 0.0f &&
        run.health.immunityTicks == 0 && prospectiveDanger(run, normalCandidate)) {
        // Dash is reserved for a geometrically predicted boss/support contact.
        run.dashRemaining = threadlight::kDashDurationSeconds;
        run.dashCooldown = run.stats.dashCooldownSeconds;
    }
    const bool dashActive = threadlight::dashProtectsCollisionThisFrame(run.dashRemaining);
    run.dashProtectedThisTick = dashActive;
    const float speed = run.stats.moveSpeed * (dashActive ? kDashSpeedMultiplier : 1.0f);
    run.player = moveToward(run.player, goal, speed * kDt);
    run.player.x = layout::clampPlayerX(run.player.x, run.playerBounds);
    run.player.y = layout::clampPlayerY(run.player.y, run.playerBounds);
    if (dashActive) run.dashRemaining = std::max(0.0f, run.dashRemaining - kDt);
}

bool applyRootSilkAndBossRewards(Run& run, int supportsFromTick) {
    if (supportsFromTick) spawnSupports(run, supportsFromTick);
    bool defeatedNow = false;
    bool touched = false;
    for (std::size_t i = 1; i < run.trail.size(); ++i) {
        const TrailPoint& first = run.trail[i - 1];
        const TrailPoint& second = run.trail[i];
        if (second.age > run.stats.activeSilkSeconds ||
            !root::silkContact(run.boss, first.p, second.p, run.stats.silkHalfWidth)) continue;
        touched = true;
        const root::DamageResult hit = root::applySilk(run.boss, run.stats.silkDamage);
        if (hit.hit == combat::SilkHit::None) break;
        if (hit.events.phaseChanged) spawnSupports(run, hit.events.supports);
        if (hit.events.defeated) {
            cancelLiveSupports(run);
            if (const auto core = run.rewards.defeatEnemy(run.bossRewardId)) {
                Pickup pickup;
                pickup.p = {layout::clampPlayerX(run.boss.p.x, run.playerBounds),
                            layout::clampPlayerY(run.boss.p.y, run.playerBounds)};
                pickup.state.id = core->id;
                pickup.bossCore = true;
                run.pickups.push_back(pickup);
                ++run.coreCreated;
            }
            defeatedNow = true;
        }
        break;
    }
    if (touched) ++run.boss.metrics.contactTicks;
    return defeatedNow;
}

void combatTick(Run& run) {
    ++run.combatTicks;
    run.previousPlayer = run.player;
    const Point goal = orbitGoal(run);
    movePlayer(run, goal);
    ageAndExtendTrail(run);

    int supportsFromTick = 0;
    if (root::alive(run.boss)) {
        const root::Events events = root::tick(run.boss, run.player, run.bossArena);
        supportsFromTick = events.supports;
    }

    const bool bossDefeatedNow = applyRootSilkAndBossRewards(run, supportsFromTick);
    combat::IncomingHit incoming;
    incoming.consider(root::incomingDamage(run.boss, run.previousPlayer, run.player, kPlayerRadius),
                      run.bossRewardId);
    if (!bossDefeatedNow) updateSupports(run, incoming);
    if (const int damage = combat::applyIncoming(run.health, incoming, run.dashProtectedThisTick))
        run.boss.metrics.playerDamage += damage;

    tickPickups(run, true);
    const foundation::WaveEvents waveEvents = run.lifecycle.tick(
        run.supports.size(), !root::alive(run.boss), false, !run.pickups.empty());
    run.timeoutObserved = run.timeoutObserved || waveEvents.timeoutDespawn;
    if (waveEvents.enteredCollection) {
        run.trail.clear();
        run.dashRemaining = 0.0f;
    }
}

void collectionTick(Run& run) {
    ++run.collectionTicks;
    run.previousPlayer = run.player;
    combat::tick(run.health);
    run.dashCooldown = std::max(0.0f, run.dashCooldown - kDt);
    Point target{(run.bossArena.left + run.bossArena.right) * 0.5f,
                 (run.bossArena.top + run.bossArena.bottom) * 0.5f};
    for (const Pickup& pickup : run.pickups) {
        if (pickup.bossCore) { target = pickup.p; break; }
        if (target.x == (run.bossArena.left + run.bossArena.right) * 0.5f &&
            target.y == (run.bossArena.top + run.bossArena.bottom) * 0.5f) target = pickup.p;
    }
    run.player = moveToward(run.player, target, run.stats.moveSpeed * kDt);
    run.player.x = layout::clampPlayerX(run.player.x, run.playerBounds);
    run.player.y = layout::clampPlayerY(run.player.y, run.playerBounds);
    tickPickups(run, false);
    run.lifecycle.tick(run.supports.size(), false, false, !run.pickups.empty());
}

RunSummary simulate(const Build& build, ArenaFixture arena, float initialBossAngle, BotPolicy policy) {
    Run run(build, arena, initialBossAngle, policy);
    const int ranks = rankTotal(run.ranks);
    CHECK(ranks <= 3);
    CHECK(run.stats.silkDamage > 0);
    CHECK(run.stats.moveSpeed >= 255.0f);
    CHECK(run.health.hp == run.stats.maxHp);

    while (run.combatTicks < kSimulationLimitTicks &&
           root::alive(run.boss) && run.health.alive()) {
        combatTick(run);
    }

    const bool bossDead = !root::alive(run.boss);
    const bool playerAlive = run.health.alive();
    const bool won = bossDead && playerAlive; // Main gives loss precedence on a lethal tick.
    if (won && run.lifecycle.phase() == foundation::Phase::Collection) {
        while (run.collectionTicks < foundation::kBossCollectionTicks &&
               run.lifecycle.phase() == foundation::Phase::Collection) {
            collectionTick(run);
        }
    }

    CHECK(!run.timeoutObserved);
    CHECK(run.boss.metrics.ticks == static_cast<std::uint64_t>(run.combatTicks));
    CHECK(run.supportSpawns <= root::kSupportBudget);
    CHECK(run.boss.supportsReleased <= root::kSupportBudget);
    CHECK(run.coreCreated <= 1);
    CHECK(run.coreCollected <= run.coreCreated);

    RunSummary result;
    result.build = build.name;
    result.arena = arena.label;
    result.route = policy.label;
    result.startAngleDegrees = static_cast<int>(std::lround(initialBossAngle * 180.0f / threadlight::chapter::kPi));
    result.outcome = won ? "win" : !playerAlive ? "died" : "cap";
    result.win = won;
    result.seconds = static_cast<float>(run.boss.metrics.ticks) / foundation::kTicksPerSecond;
    result.hp = static_cast<float>(run.health.hp) / 100.0f;
    result.hits = run.boss.metrics.hits;
    result.exposedHits = run.boss.metrics.exposedHits;
    result.bossDamage = static_cast<float>(run.boss.metrics.silkDamage) / 100.0f;
    result.playerDamage = static_cast<float>(run.boss.metrics.playerDamage) / 100.0f;
    result.cycles = run.boss.metrics.cycles;
    result.coreCreated = run.coreCreated;
    result.coreCollected = run.coreCollected;
    result.supportSpawns = run.supportSpawns;
    result.supportCancelled = run.supportCancelled;
    return result;
}

void printSummary(const RunSummary& run) {
    std::cout << "scenario build=" << run.build << " arena=" << run.arena
              << " route=" << run.route << " start_deg=" << run.startAngleDegrees
              << " result=" << run.outcome
              << " seconds=" << std::fixed << std::setprecision(2) << run.seconds
              << " hp=" << run.hp << " hits=" << run.hits
              << " exposure_hits=" << run.exposedHits << " damage=" << run.bossDamage
              << " taken=" << run.playerDamage
              << " cycles=" << run.cycles << " core=" << run.coreCreated << '/'
              << run.coreCollected << " supports=" << run.supportSpawns
              << " canceled=" << run.supportCancelled << '\n';
}

struct RewardFlowFixture {
    foundation::RewardLedger ledger{};
    foundation::WaveLifecycle lifecycle{};
    foundation::RewardId bossId = 0;
    std::array<foundation::RewardId, root::kSupportBudget> supportIds{};
    foundation::PickupState core{};
    Point corePosition{1240.0f, 708.0f};
    int supportsCancelled = 0;
};

RewardFlowFixture makeRewardFlowFixture() {
    RewardFlowFixture fixture;
    const Point origin{640.0f, 412.0f};
    const auto brutes = combat::definition(combat::EnemyKind::Brute);
    // Ten collected Brute drops model one ordinary unspent pre-boss credit; this
    // fixture is economy plumbing, not a second M1 encounter simulation.
    for (int i = 0; i < 10; ++i) {
        const auto id = fixture.ledger.registerEnemy(brutes.sparkValue, brutes.killScore);
        CHECK(id != 0);
        CHECK(fixture.ledger.defeatEnemy(id).has_value());
        foundation::PickupState pickup;
        pickup.id = id;
        Point position = origin;
        const auto contact = foundation::tickPickup(pickup, position, origin, origin,
                                                    kPlayerRadius, false, true);
        CHECK(contact == foundation::PickupResult::Collected);
        CHECK(fixture.ledger.collectPickup(id));
    }
    CHECK(fixture.ledger.counters().creditsIssued == 1);
    CHECK(fixture.ledger.pendingCredits() == 1);
    CHECK(fixture.ledger.remainder() == 0);

    fixture.lifecycle.start({1, true, 0});
    fixture.bossId = fixture.ledger.registerEnemy(root::kCoreValue, 250);
    const auto hunters = combat::definition(combat::EnemyKind::Hunter);
    for (auto& id : fixture.supportIds) id = fixture.ledger.registerEnemy(hunters.sparkValue, hunters.killScore);

    // Boss encounters deliberately ignore the normal spawn timeout, including
    // this full 180-second observation before the externally supplied death event.
    bool timeoutObserved = false;
    for (int tick = 0; tick < kSimulationLimitTicks; ++tick) {
        const foundation::WaveEvents event = fixture.lifecycle.tick(
            fixture.supportIds.size(), false, false, true);
        timeoutObserved = timeoutObserved || event.timeoutDespawn;
    }
    CHECK(!timeoutObserved);
    CHECK(fixture.lifecycle.phase() == foundation::Phase::Spawning);

    const foundation::RewardCounters beforeCancellation = fixture.ledger.counters();
    for (const auto id : fixture.supportIds) {
        if (fixture.ledger.despawnEnemy(id)) ++fixture.supportsCancelled;
    }
    const foundation::RewardCounters afterCancellation = fixture.ledger.counters();
    CHECK(fixture.supportsCancelled == root::kSupportBudget);
    CHECK(afterCancellation.despawned - beforeCancellation.despawned == root::kSupportBudget);
    CHECK(afterCancellation.killed == beforeCancellation.killed);
    CHECK(afterCancellation.pickupsCreated == beforeCancellation.pickupsCreated);
    CHECK(afterCancellation.sparksCreated == beforeCancellation.sparksCreated);

    const foundation::RewardCounters beforeCore = fixture.ledger.counters();
    const auto coreDrop = fixture.ledger.defeatEnemy(fixture.bossId);
    CHECK(coreDrop.has_value());
    if (coreDrop) {
        CHECK(coreDrop->sparkValue == root::kCoreValue);
        fixture.core.id = coreDrop->id;
    }
    const foundation::RewardCounters afterCore = fixture.ledger.counters();
    CHECK(afterCore.killed == beforeCore.killed + 1);
    CHECK(afterCore.pickupsCreated == beforeCore.pickupsCreated + 1);
    CHECK(afterCore.sparksCreated == beforeCore.sparksCreated + root::kCoreValue);
    CHECK(!fixture.ledger.defeatEnemy(fixture.bossId).has_value());
    const foundation::WaveEvents defeat = fixture.lifecycle.tick(0, true, false, true);
    CHECK(defeat.enteredCollection && defeat.bossDefeated && !defeat.timeoutDespawn);
    return fixture;
}

void rewardCollectionRemainsManualAndUnbanked() {
    const ArenaFixture arena = mainArena();
    const Point stationaryPlayer{640.0f, 412.0f};
    RewardFlowFixture remote = makeRewardFlowFixture();
    CHECK(length(sub(remote.corePosition, stationaryPlayer)) > 110.0f);
    int remoteTicks = 0;
    for (; remoteTicks < foundation::kBossCollectionTicks &&
           remote.lifecycle.phase() == foundation::Phase::Collection; ++remoteTicks) {
        const auto pickup = foundation::tickPickup(remote.core, remote.corePosition,
                                                   stationaryPlayer, stationaryPlayer,
                                                   kPlayerRadius, false, true);
        CHECK(pickup == foundation::PickupResult::None);
        const foundation::WaveEvents event = remote.lifecycle.tick(0, false, false, true);
        CHECK(!event.timeoutDespawn);
    }
    CHECK(remoteTicks == foundation::kBossCollectionTicks);
    CHECK(remote.lifecycle.phase() == foundation::Phase::PendingRewards);
    CHECK(remote.core.terminal == foundation::PickupResult::None);
    CHECK(remote.ledger.counters().sparksCollected == 20);
    CHECK(remote.ledger.counters().sparksCreated == 30);
    CHECK(remote.ledger.counters().sparksExpired == 0);
    CHECK(remote.ledger.remainder() == 0);
    CHECK(remote.ledger.pendingCredits() == 1);
    CHECK(remote.ledger.counters().creditsResolved == 0);

    RewardFlowFixture manual = makeRewardFlowFixture();
    Point player{640.0f, 412.0f};
    Point previousPlayer = player;
    const auto firstRemoteContact = foundation::tickPickup(
        manual.core, manual.corePosition, previousPlayer, player, kPlayerRadius,
        false, true);
    CHECK(firstRemoteContact == foundation::PickupResult::None);
    bool collected = false;
    int manualTicks = 0;
    const upgrades::Stats baseStats = upgrades::derive(upgrades::Ranks{});
    for (; manualTicks < foundation::kBossCollectionTicks &&
           manual.lifecycle.phase() == foundation::Phase::Collection && !collected;
         ++manualTicks) {
        previousPlayer = player;
        player = moveToward(player, manual.corePosition, baseStats.moveSpeed * kDt);
        player.x = layout::clampPlayerX(player.x, arena.playerBounds);
        player.y = layout::clampPlayerY(player.y, arena.playerBounds);
        const auto pickup = foundation::tickPickup(
            manual.core, manual.corePosition, previousPlayer, player, kPlayerRadius,
            false, true, baseStats.pickupRadius, baseStats.pickupPullSpeed);
        if (pickup == foundation::PickupResult::Collected) {
            collected = manual.ledger.collectPickup(manual.core.id);
        }
        manual.lifecycle.tick(0, false, false, manual.core.terminal == foundation::PickupResult::None);
    }
    CHECK(collected);
    CHECK(manual.core.terminal == foundation::PickupResult::Collected);
    CHECK(manualTicks < foundation::kBossCollectionTicks);
    CHECK(manual.lifecycle.phase() == foundation::Phase::PendingRewards);
    CHECK(manual.ledger.counters().sparksCollected == 30);
    CHECK(manual.ledger.remainder() == root::kCoreValue);
    CHECK(manual.ledger.pendingCredits() == 1);
    CHECK(manual.ledger.counters().creditsResolved == 0);
}

}  // namespace

int main() {
    const ArenaFixture arena = mainArena();
    const ArenaFixture minimum = minimumArena();
    const auto allBuilds = builds();
    constexpr std::array<float, 2> initialBossAngles{0.0f, threadlight::chapter::kPi * 0.5f};
    bool baselineRoutesWin = true;
    for (const Build& build : allBuilds) {
        for (const float angle : initialBossAngles) {
            const RunSummary baseline = simulate(build, arena, angle, kApproachPolicy);
            printSummary(baseline);
            baselineRoutesWin = baselineRoutesWin && baseline.win;
        }
    }
    CHECK(baselineRoutesWin);

    for (const Build& build : allBuilds)
        printSummary(simulate(build, minimum, 0.0f, kApproachPolicy));

    for (const float angle : initialBossAngles)
        printSummary(simulate(allBuilds[0], arena, angle, kCautiousPolicy));

    rewardCollectionRemainsManualAndUnbanked();
    std::cout << "reward_fixture arena=" << arena.label
              << " boss_timeout=none supports_cancelled=" << root::kSupportBudget
              << " remote_core=1/0 remote_ticks=" << foundation::kBossCollectionTicks
              << " credit_pending=1 core_uncollected=1 manual_core=1/1 banked=0\n";

    if (failures != 0) {
        std::cerr << failures << " Rootwarden scenario check(s) failed\n";
        return 1;
    }
    std::cout << "Rootwarden scripted scenario checks passed (bot evidence only; not human playtest)\n";
    return 0;
}
