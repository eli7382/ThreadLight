#include "afterimage.h"
#include "attack_coordinator.h"
#include "chapter_one.h"
#include "combat.h"
#include "game_logic.h"
#include "player_upgrades.h"
#include "ranged_combat.h"
#include "reward_ledger.h"
#include "signature_reward.h"
#include "skitter.h"
#include "spark_pickup.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {
int failures = 0;
#define CHECK(expression) do { if (!(expression)) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": " << #expression << '\n'; } } while (false)

namespace a = threadlight::afterimage; namespace attacks = threadlight::attacks;
namespace battle = threadlight::combat; namespace chapter = threadlight::chapter;
namespace foundation = threadlight::foundation; namespace ranged = threadlight::ranged;
namespace sig = threadlight::signature_reward; namespace skitter = threadlight::skitter;
namespace upgrades = threadlight::upgrades;
using foundation::Point;

constexpr float kPlayerRadius = 12.0f;
constexpr float kStep = 1.0f / foundation::kTicksPerSecond;
constexpr ranged::Bounds kArena{0.0f, 1280.0f, 0.0f, 720.0f};
constexpr std::array<Point, 4> kWaypoints{{{410, 260}, {870, 260}, {870, 460}, {410, 460}}};
constexpr std::array<Point, 4> kWideWaypoints{{{220, 145}, {1060, 145}, {1060, 575}, {220, 575}}};
struct Fixture { std::string_view name; upgrades::Ranks ranks; };
struct Foe {
    foundation::RewardId id = 0;
    battle::EnemyKind kind = battle::EnemyKind::Hunter;
    battle::EnemyDefinition definition{};
    battle::EnemyHealth health{};
    Point position{}, velocity{};
    skitter::State skitterState{};
    ranged::State rangedState{};
    ranged::Pool::Ticket ticket = 0;
    int warningTick = 0;
};
struct Drop { foundation::PickupState state; Point position; };
struct Player {
    Point position{640, 360}, previous{640, 360};
    std::size_t waypoint = 0;
    float dashTime = 0, dashCooldown = 0;
};
struct Summary {
    int ordinaryHits = 0, ordinaryDamage = 0, signatureHits = 0, signatureDamage = 0;
    int warnings = 0, shots = 0, projectileImpacts = 0, projectilePressure = 0;
    int playerDamage = 0, finalHp = 0, kills = 0, despawns = 0, pickupsCollected = 0, creditsResolved = 0;
    bool survived = false, wave8Complete = false, wave9Complete = false;
    friend bool operator==(const Summary&, const Summary&) = default;
};
Point unit(Point v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y);
    return length > 0.001f ? Point{v.x / length, v.y / length} : Point{};
}
bool movePlayer(Player& player, const upgrades::Stats& stats, bool combat, bool ownsSignature, a::State& echo,
                const std::array<Point,4>& route) {
    player.previous = player.position;
    if (combat) {
        player.dashCooldown = std::max(0.0f, player.dashCooldown - kStep);
        if (player.dashTime <= 0.0f && player.dashCooldown <= 0.0f) {
            a::beginDash(echo, player.position, true, ownsSignature);
            player.dashTime = threadlight::kDashDurationSeconds;
            player.dashCooldown = stats.dashCooldownSeconds;
        }
    }

    Point delta{route[player.waypoint].x - player.position.x, route[player.waypoint].y - player.position.y};
    if (delta.x * delta.x + delta.y * delta.y < 100.0f) {
        player.waypoint = (player.waypoint + 1) % route.size();
        delta = {route[player.waypoint].x - player.position.x, route[player.waypoint].y - player.position.y};
    }
    const Point direction = unit(delta);
    const bool dashActive = combat && threadlight::dashProtectsCollisionThisFrame(player.dashTime);
    const float speed = stats.moveSpeed * (dashActive ? 3.7f : 1.0f);
    player.position.x = std::clamp(player.position.x + direction.x * speed * kStep, kPlayerRadius, kArena.right - kPlayerRadius);
    player.position.y = std::clamp(player.position.y + direction.y * speed * kStep, kPlayerRadius, kArena.bottom - kPlayerRadius);
    if (dashActive) {
        player.dashTime = std::max(0.0f, player.dashTime - kStep);
        a::appendDash(echo, player.previous, player.position, true);
        if (player.dashTime <= 0.0f) a::finishDash(echo, true, ownsSignature);
    }
    return dashActive;
}
void movePursuer(Foe& foe, Point player) {
    const Point delta{player.x - foe.position.x, player.y - foe.position.y};
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const float step = std::min(distance, foe.definition.speed * kStep);
    const Point previous = foe.position;
    if (distance > 0.001f) foe.position = {foe.position.x + delta.x * step / distance,
                                           foe.position.y + delta.y * step / distance};
    foe.velocity = {(foe.position.x - previous.x) / kStep, (foe.position.y - previous.y) / kStep};
}
Foe spawnFoe(const chapter::Spawn& spawn, foundation::RewardId id) {
    Foe foe;
    foe.id = id;
    foe.kind = spawn.kind;
    foe.definition = battle::definition(spawn.kind, 2);
    foe.health = {foe.definition.hp, foe.definition.hp};
    foe.position = {640.0f + std::cos(spawn.angle) * 470.0f, 360.0f + std::sin(spawn.angle) * 300.0f};
    foe.position.x = std::clamp(foe.position.x, foe.definition.radius, kArena.right - foe.definition.radius);
    foe.position.y = std::clamp(foe.position.y, foe.definition.radius, kArena.bottom - foe.definition.radius);
    foe.skitterState.side = (id & 1U) ? 1 : -1;
    return foe;
}
void collectDrops(std::vector<Drop>& drops, Point previousPlayer, Point player, bool combat,
                  const upgrades::Stats& stats, foundation::RewardLedger& ledger, Summary& summary) {
    for (Drop& drop : drops) {
        const auto result = foundation::tickPickup(drop.state, drop.position, previousPlayer, player,
            kPlayerRadius, combat, true, stats.pickupRadius, stats.pickupPullSpeed);
        if (result == foundation::PickupResult::Collected && ledger.collectPickup(drop.state.id)) ++summary.pickupsCollected;
        else if (result == foundation::PickupResult::Expired) ledger.expirePickup(drop.state.id);
    }
    drops.erase(std::remove_if(drops.begin(), drops.end(), [](const Drop& drop) {
        return drop.state.terminal != foundation::PickupResult::None;
    }), drops.end());
}
void recordDeath(Foe& foe, ranged::Pool& pool, foundation::RewardLedger& ledger,
                 std::vector<Drop>& drops, const upgrades::Stats& stats, Summary& summary) {
    if (foe.health.alive()) return;
    if (foe.ticket) pool.cancelReservation(foe.ticket);
    foe.ticket = 0;
    if (const auto reward = ledger.defeatEnemy(foe.id)) {
        drops.push_back({{reward->id, 0, false, stats.pickupPullSpeed}, foe.position});
        ++summary.kills;
    }
}
Summary runScenario(const Fixture& fixture, bool afterimageOwned, bool wide) {
    Summary summary;
    const upgrades::Stats stats = upgrades::derive(fixture.ranks);
    battle::PlayerHealth playerHealth;
    playerHealth.hp = playerHealth.maxHp = stats.maxHp;
    Player player;
    a::State echo;
    sig::RewardState signature;
    if (afterimageOwned) {
        CHECK(sig::queue(signature, 90001, true)); // Prior Rootwarden signature choice is the fixture boundary.
        CHECK(sig::accept(signature, foundation::Phase::PendingRewards, true, 0));
    }

    foundation::RewardLedger ledger;
    std::vector<Drop> drops;
    std::vector<Foe> foes;
    std::vector<std::pair<Point, int>> trail;
    ranged::Pool projectiles;
    attacks::Coordinator coordinator;
    int globalTick = 0;

    for (int wave = 7; wave <= 8 && playerHealth.alive(); ++wave) {
        const auto encounter = chapter::encounter(wave);
        const auto schedule = chapter::schedule(wave, 7382);
        CHECK(encounter.chapter == 2 && !encounter.boss);
        foundation::WaveLifecycle lifecycle;
        lifecycle.start({encounter.seconds * foundation::kTicksPerSecond, false, 0});
        std::size_t spawnIndex = 0;
        const std::uint64_t creditsBeforeWave = ledger.counters().creditsResolved;
        int lastSpitterWarning = 0;
        const int combatLimit = encounter.seconds * foundation::kTicksPerSecond + foundation::kCleanupTicks;

        while (foundation::isCombat(lifecycle.phase()) && lifecycle.elapsedTicks() < combatLimit && playerHealth.alive()) {
            ++globalTick;
            battle::tick(playerHealth);
            const bool spawning = lifecycle.phase() == foundation::Phase::Spawning;
            while (spawning && spawnIndex < static_cast<std::size_t>(schedule.count) &&
                   schedule.items[spawnIndex].tick == lifecycle.elapsedTicks()) {
                const auto& entry = schedule.items[spawnIndex++];
                const auto id = ledger.registerEnemy(battle::definition(entry.kind, 2).sparkValue,
                                                     battle::definition(entry.kind, 2).killScore);
                foes.push_back(spawnFoe(entry, id));
            }

            const bool dashProtected = movePlayer(player, stats, true, signature.owned, echo,wide?kWideWaypoints:kWaypoints);
            for (auto& point : trail) ++point.second;
            trail.push_back({player.position, 0});
            const int trailLimit = static_cast<int>(std::ceil(stats.activeSilkSeconds * foundation::kTicksPerSecond));
            trail.erase(std::remove_if(trail.begin(), trail.end(), [trailLimit](const auto& point) {
                return point.second > trailLimit;
            }), trail.end());

            int activeCommitments = 0;
            std::array<attacks::Request, 64> requests{};
            int requestCount = 0;
            for (const Foe& foe : foes) {
                if (foe.kind == battle::EnemyKind::Skitter) {
                    activeCommitments += skitter::committed(foe.skitterState) ? 1 : 0;
                    if (foe.skitterState.phase == skitter::Phase::Flank && foe.skitterState.ticksRemaining <= 1)
                        requests[requestCount++] = {foe.id, attacks::Kind::Skitter};
                } else if (foe.kind == battle::EnemyKind::Spitter) {
                    activeCommitments += ranged::committed(foe.rangedState) ? 1 : 0;
                    if (ranged::wantsCommit(foe.rangedState))
                        requests[requestCount++] = {foe.id, attacks::Kind::Spitter};
                }
            }
            const auto grants = coordinator.choose(std::span<const attacks::Request>(requests.data(), requestCount),
                                                   activeCommitments, static_cast<int>(projectiles.freeSlots()),
                                                   static_cast<std::uint64_t>(globalTick));
            for (const auto& grant : grants.items) {
                if (!grant.id || grant.kind != attacks::Kind::Spitter) continue;
                for (Foe& foe : foes) if (foe.id == grant.id) foe.ticket = projectiles.reserve(foe.id);
            }

            battle::IncomingHit incoming;
            const Point playerMotion{player.position.x - player.previous.x,
                                     player.position.y - player.previous.y};
            for (Foe& foe : foes) {
                if (!foe.health.alive()) continue;
                battle::tick(foe.health);
                const Point oldPosition = foe.position;
                const ranged::Bounds enemyBounds{foe.definition.radius,
                    kArena.right - foe.definition.radius, foe.definition.radius,
                    kArena.bottom - foe.definition.radius};
                if (foe.kind == battle::EnemyKind::Skitter) {
                    const bool canCommit = grants.contains(foe.id);
                    const auto event = skitter::tick(foe.skitterState, foe.position, foe.velocity,
                        player.position, playerMotion,
                        {enemyBounds.left, enemyBounds.right, enemyBounds.top, enemyBounds.bottom}, canCommit,
                        foe.definition.speed);
                    (void)event;
                } else if (foe.kind == battle::EnemyKind::Spitter) {
                    const bool canCommit = grants.contains(foe.id) && foe.ticket != 0;
                    const auto event = ranged::tick(foe.rangedState, foe.position, foe.velocity,
                        player.position, enemyBounds, canCommit);
                    if (event.warningStarted) {
                        ++summary.warnings;
                        CHECK(lastSpitterWarning == 0 || globalTick - lastSpitterWarning >= 42);
                        lastSpitterWarning = globalTick;
                        foe.warningTick = globalTick;
                    }
                    if (event.shotReady) {
                        CHECK(foe.warningTick > 0 && globalTick - foe.warningTick == ranged::kWindupTicks);
                        CHECK(projectiles.launch(foe.ticket,
                            ranged::muzzle(foe.position, foe.rangedState.aimDirection), foe.rangedState.aimDirection));
                        ++summary.shots;
                        foe.ticket = 0;
                    }
                } else {
                    movePursuer(foe, player.position); // Simple Hunter/Brute pursuit proxy; special brains are real headers.
                }

                for (std::size_t i = 1; foe.health.alive() && i < trail.size(); ++i) {
                    if (!battle::silkContact(oldPosition, foe.position, foe.definition.radius,
                            trail[i - 1].first, trail[i].first, stats.silkHalfWidth)) continue;
                    const int hpBefore = foe.health.hp;
                    const auto hit = battle::applySilk(foe.health, stats.silkDamage);
                    if (hit != battle::SilkHit::None) {
                        ++summary.ordinaryHits;
                        summary.ordinaryDamage += hpBefore - foe.health.hp;
                    }
                    break;
                }
                if (foe.health.alive() && signature.owned &&
                    a::claimContact(echo, foe.id, oldPosition, foe.position, foe.definition.radius)) {
                    const int hpBefore = foe.health.hp;
                    const int gateBefore = foe.health.silkCooldownTicks;
                    battle::applyDamage(foe.health, a::damage(stats.silkDamage));
                    CHECK(foe.health.silkCooldownTicks == gateBefore);
                    ++summary.signatureHits;
                    summary.signatureDamage += hpBefore - foe.health.hp;
                }
                if (foe.health.alive() && battle::bodyContact(player.previous, player.position,
                        kPlayerRadius, oldPosition, foe.position, foe.definition.radius))
                    incoming.consider(foe.definition.contactDamage, foe.id);
                recordDeath(foe, projectiles, ledger, drops, stats, summary);
            }

            const auto impacts = projectiles.tick(player.previous, player.position, kPlayerRadius, kArena);
            for (int i = 0; i < impacts.count; ++i) {
                incoming.consider(impacts.items[i].damage, impacts.items[i].sourceId);
                ++summary.projectileImpacts;
                summary.projectilePressure += impacts.items[i].damage;
            }
            a::advance(echo, true);
            summary.playerDamage += battle::applyIncoming(playerHealth, incoming, dashProtected);
            collectDrops(drops, player.previous, player.position, true, stats, ledger, summary);
            foes.erase(std::remove_if(foes.begin(), foes.end(), [](const Foe& foe) {
                return !foe.health.alive();
            }), foes.end());
            int commitmentsAfter = 0;
            for (const Foe& foe : foes) {
                if (foe.kind == battle::EnemyKind::Skitter) commitmentsAfter += skitter::committed(foe.skitterState) ? 1 : 0;
                if (foe.kind == battle::EnemyKind::Spitter) commitmentsAfter += ranged::committed(foe.rangedState) ? 1 : 0;
            }
            CHECK(commitmentsAfter <= 2);
            if (!playerHealth.alive()) break;

            const auto events = lifecycle.tick(foes.size());
            if (events.timeoutDespawn) {
                for (Foe& foe : foes) if (foe.health.alive() && ledger.despawnEnemy(foe.id)) ++summary.despawns;
                foes.clear();
            }
            if (events.enteredCollection) {
                a::clear(echo);
                trail.clear();
                player.dashTime = 0;
                projectiles.clear();
                coordinator.clear();
            }
        }

        if (!playerHealth.alive()) break;
        if (lifecycle.phase() == foundation::Phase::Collection) {
            for (int tick = 0; tick < foundation::kNormalCollectionTicks &&
                                lifecycle.phase() == foundation::Phase::Collection; ++tick) {
                battle::tick(playerHealth);
                movePlayer(player, stats, false, signature.owned, echo,wide?kWideWaypoints:kWaypoints);
                collectDrops(drops, player.previous, player.position, false, stats, ledger, summary);
                lifecycle.tick(0, false, false, !drops.empty());
            }
        }
        if (lifecycle.phase() == foundation::Phase::PendingRewards) {
            if(wave==7)while (ledger.pendingCredits() > 0)
                CHECK(ledger.resolveCredit(ledger.nextCredit(), foundation::Phase::PendingRewards));
            summary.creditsResolved += static_cast<int>(ledger.counters().creditsResolved - creditsBeforeWave);
            if(wave==7)CHECK(lifecycle.finishRewards(false));
        }
        if (wave == 7) summary.wave8Complete = lifecycle.phase() == foundation::Phase::Complete;
        else summary.wave9Complete = lifecycle.phase() == foundation::Phase::PendingRewards;
        if (wave==7 && lifecycle.phase() != foundation::Phase::Complete) break;
    }

    summary.finalHp = playerHealth.hp;
    summary.survived = playerHealth.alive();
    CHECK(summary.playerDamage == stats.maxHp - summary.finalHp);
    CHECK(summary.ordinaryDamage <= summary.ordinaryHits * stats.silkDamage);
    CHECK(summary.signatureDamage <= summary.signatureHits * a::damage(stats.silkDamage));
    CHECK(summary.shots <= summary.warnings);
    CHECK(summary.projectileImpacts <= summary.shots);
    CHECK(summary.projectilePressure == summary.projectileImpacts * ranged::kProjectileDamage);
    if (!signature.owned) CHECK(summary.signatureHits == 0 && summary.signatureDamage == 0);
    return summary;
}
void print(const Fixture& fixture, bool owned, const Summary& result) {
    std::cout << fixture.name << " afterimage=" << (owned ? 1 : 0)
              << " alive=" << result.survived << " hp=" << result.finalHp
              << " ordinary=" << result.ordinaryHits << '/' << result.ordinaryDamage
              << " signature=" << result.signatureHits << '/' << result.signatureDamage
              << " spitter=" << result.warnings << '/' << result.shots
              << " impacts=" << result.projectileImpacts << " pressure=" << result.projectilePressure
              << " playerDamage=" << result.playerDamage << " kills=" << result.kills
              << " despawns=" << result.despawns << " pickups=" << result.pickupsCollected
              << " credits=" << result.creditsResolved << " W8=" << result.wave8Complete
              << " W9=" << result.wave9Complete << '\n';
}
} // namespace
int main() {
    upgrades::Ranks offense, defense, mobility, mixed;
    offense.value[upgrades::index(upgrades::Id::Razor)] = 3;
    offense.value[upgrades::index(upgrades::Id::Living)] = 1;
    defense.value[upgrades::index(upgrades::Id::Heartwood)] = 3;
    defense.value[upgrades::index(upgrades::Id::Living)] = 1;
    mobility.value[upgrades::index(upgrades::Id::Quick)] = 2;
    mobility.value[upgrades::index(upgrades::Id::Rhythm)] = 2;
    mixed.value[upgrades::index(upgrades::Id::Razor)] = 1;
    mixed.value[upgrades::index(upgrades::Id::Living)] = 1;
    mixed.value[upgrades::index(upgrades::Id::Heartwood)] = 1;
    const std::array<Fixture, 5> fixtures{{{"zero", {}}, {"offense-4", offense},
        {"defense-4", defense}, {"mobility-4", mobility}, {"mixed-3", mixed}}};

    for(bool wide:{true,false})for (const Fixture& fixture : fixtures) {
        for (bool owned : {false, true}) {
            const Summary result = runScenario(fixture, owned,wide);
            const Summary replay = runScenario(fixture, owned,wide);
            CHECK(result == replay);
            CHECK(result.wave9Complete || !result.survived);
            std::cout<<(wide?"wide ":"tight ");print(fixture, owned, result);
        }
    }
    if (failures) {
        std::cerr << failures << " M4 scenario check(s) failed\n";
        return 1;
    }
    std::cout << "M4 deterministic scenario checks passed\n";
    return 0;
}
