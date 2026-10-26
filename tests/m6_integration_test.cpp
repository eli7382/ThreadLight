#include "area_hazards.h"
#include "chapter_boss.h"
#include "chapter_one.h"
#include "cocoon.h"
#include "combat.h"
#include "player_upgrades.h"
#include "reward_ledger.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* expression, int line) {
    ++checks;
    if (condition) return;
    ++failures;
    std::cerr << "M6 integration check failed at line " << line << ": " << expression << '\n';
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

namespace boss = threadlight::chapter_boss;
namespace combat = threadlight::combat;
namespace cocoon = threadlight::cocoon;
namespace field = threadlight::area_hazards;
namespace foundation = threadlight::foundation;
namespace upgrades = threadlight::upgrades;
namespace chapter = threadlight::chapter;
using Point = foundation::Point;
using Arena = boss::Arena;

constexpr Arena kOldArena{0.0f, 924.0f, 0.0f, 508.0f};
constexpr Arena kResizedArena{0.0f, 924.0f, 0.0f, 430.0f};
constexpr Point kOldCenter{462.0f, 254.0f};

combat::IncomingHit hit(int damage, std::uint64_t source = 1) {
    combat::IncomingHit result;
    result.consider(damage, source);
    return result;
}

int applyCocoonDamage(cocoon::State& shield, combat::PlayerHealth& player,
                      combat::IncomingHit incoming, bool dashProtected) {
    int* barrier = cocoon::barrierActive(shield) ? &shield.barrierCenti : nullptr;
    return combat::applyIncoming(player, incoming, dashProtected, barrier);
}

void cocoonUsesSharedDashAndImmunityGates() {
    cocoon::State shield;
    shield.barrierCenti = cocoon::kBarrierCapacityCenti;
    shield.remainingTicks = cocoon::kBarrierDurationTicks;
    CHECK(cocoon::barrierActive(shield));

    combat::PlayerHealth player{10000, 10000, 0};
    CHECK(applyCocoonDamage(shield, player, hit(1200), false) == 0);
    CHECK(player.hp == 10000 && player.immunityTicks == 0 && shield.barrierCenti == 0);

    shield.barrierCenti = cocoon::kBarrierCapacityCenti;
    player = {10000, 10000, 0};
    CHECK(applyCocoonDamage(shield, player, hit(1600), false) == 400);
    CHECK(player.hp == 9600 && player.immunityTicks == combat::kPlayerImmunityTicks);
    CHECK(shield.barrierCenti == 0);
    CHECK(applyCocoonDamage(shield, player, hit(900), false) == 0);
    CHECK(player.hp == 9600 && player.immunityTicks == combat::kPlayerImmunityTicks);

    shield.barrierCenti = cocoon::kBarrierCapacityCenti;
    player = {10000, 10000, 0};
    CHECK(applyCocoonDamage(shield, player, hit(400), true) == 0);
    CHECK(player.hp == 10000 && player.immunityTicks == 0 && shield.barrierCenti == 1200);

    player.immunityTicks = 17;
    CHECK(applyCocoonDamage(shield, player, hit(400), false) == 0);
    CHECK(player.hp == 10000 && player.immunityTicks == 17 && shield.barrierCenti == 1200);
}

void finalBossDefeatRecordsNoPickupExactlyOnce() {
    foundation::RewardLedger ledger;
    const auto id = ledger.registerEnemy(10, 250);
    CHECK(id != 0 && ledger.defeatEnemyWithoutPickup(id));
    CHECK(!ledger.defeatEnemyWithoutPickup(id));
    CHECK(!ledger.defeatEnemy(id));
    CHECK(!ledger.collectPickup(id) && !ledger.expirePickup(id));
    const auto& c = ledger.counters();
    CHECK(c.spawned == 1 && c.killed == 1 && c.pickupsCreated == 0 &&
          c.pickupsCollected == 0 && c.pickupsExpired == 0 && c.sparksCreated == 0);
    CHECK(ledger.score() == 250 && ledger.remainder() == 0 &&
          ledger.pendingCredits() == 0 && c.creditsIssued == 0);
}

void collectSparkValue(foundation::RewardLedger& ledger, int value) {
    const auto id = ledger.registerEnemy(value, 0);
    CHECK(id != 0 && ledger.defeatEnemy(id).has_value());
    CHECK(ledger.collectPickup(id));
}

void collectBossSupportSparks(foundation::RewardLedger& ledger, boss::Kind kind,
                              int count, int chapterNumber, int& total) {
    boss::State state;
    state.kind = kind;
    for (int ordinal = 0; ordinal < count; ++ordinal) {
        const int value = combat::definition(boss::supportKind(state, ordinal), chapterNumber).sparkValue;
        total += value;
        collectSparkValue(ledger, value);
    }
}

void fullCampaignRewardCeiling() {
    foundation::RewardLedger ledger;
    int total = 0;
    for (int wave = 0; wave < chapter::kEncounterCount; ++wave) {
        const auto encounter = chapter::encounter(wave);
        const auto schedule = chapter::schedule(wave, 0xdecafbadU);
        for (int index = 0; index < schedule.count; ++index) {
            const auto& spawn = schedule.items[index];
            const int value = combat::definition(spawn.kind, encounter.chapter).sparkValue;
            total += value;
            collectSparkValue(ledger, value);
        }
    }

    // Earlier boss support and core pickups remain in the accepted campaign total.
    constexpr std::array<int, 3> oldSupportCounts{4, 4, 6};
    constexpr std::array<boss::Kind, 3> oldBosses{
        boss::Kind::Rootwarden, boss::Kind::Dancer, boss::Kind::Conductor};
    constexpr std::array<int, 3> oldBossWaves{6, 13, 20};
    for (std::size_t i = 0; i < oldBosses.size(); ++i) {
        const int chapterNumber = chapter::encounter(oldBossWaves[i]).chapter;
        collectBossSupportSparks(ledger, oldBosses[i], oldSupportCounts[i], chapterNumber, total);
        total += boss::kCoreValue;
        collectSparkValue(ledger, boss::kCoreValue);
    }

    collectBossSupportSparks(ledger, boss::Kind::Hollow, 6, chapter::encounter(27).chapter, total);
    total += boss::kCoreValue;
    collectSparkValue(ledger, boss::kCoreValue);
    collectBossSupportSparks(ledger, boss::Kind::Heart, 8, chapter::encounter(34).chapter, total);
    const auto finalBossId = ledger.registerEnemy(boss::kCoreValue, 250);
    CHECK(ledger.defeatEnemyWithoutPickup(finalBossId));

    CHECK(total == 1364);
    CHECK(ledger.counters().sparksCollected == 1364);
    CHECK(ledger.counters().creditsIssued == 29 && ledger.remainder() == 26);
    CHECK(ledger.pendingCredits() == 29 && ledger.counters().creditsResolved == 0);
}

void exhaustedOfferCanDiscardWhenOnlyMagnetIsIrrelevant() {
    upgrades::Ranks ranks{};
    for (int i = 0; i < upgrades::kPermanentCount; ++i) {
        if(i==upgrades::index(upgrades::Id::Magnet))continue;
        const auto id = static_cast<upgrades::Id>(i);
        ranks.value[static_cast<std::size_t>(i)] = upgrades::maximum(id);
    }
    combat::PlayerHealth health{10000, 10000, 0};
    CHECK(upgrades::rank(ranks, upgrades::Id::Magnet) == 0 && health.hp == health.maxHp);

    foundation::RewardLedger discardedLedger;
    const auto discardedReward = discardedLedger.registerEnemy(20, 0);
    CHECK(discardedLedger.defeatEnemy(discardedReward).has_value());
    CHECK(discardedLedger.collectPickup(discardedReward));
    upgrades::OfferState discardedOffers;
    const auto& exhausted = upgrades::prepareOffer(discardedOffers, ranks, health,
                                                    discardedLedger.nextCredit(), false);
    CHECK(exhausted.count == 0);
    CHECK(upgrades::resolveExhaustedCredit(discardedLedger, foundation::Phase::PendingRewards,
                                           exhausted, ranks, health, false));
    CHECK(discardedLedger.pendingCredits() == 0 && discardedLedger.counters().creditsResolved == 1);

    foundation::RewardLedger retainedLedger;
    const auto retainedReward = retainedLedger.registerEnemy(20, 0);
    CHECK(retainedLedger.defeatEnemy(retainedReward).has_value());
    CHECK(retainedLedger.collectPickup(retainedReward));
    upgrades::OfferState retainedOffers;
    const auto& magnetOffer = upgrades::prepareOffer(retainedOffers, ranks, health,
                                                      retainedLedger.nextCredit(), true);
    CHECK(magnetOffer.count == 1 && upgrades::contains(magnetOffer, upgrades::Id::Magnet));
    CHECK(!upgrades::resolveExhaustedCredit(retainedLedger, foundation::Phase::PendingRewards,
                                            magnetOffer, ranks, health, true));
    CHECK(retainedLedger.pendingCredits() == 1 && retainedLedger.counters().creditsResolved == 0);
}

void startHollowFold(boss::State& hollow) {
    boss::start(hollow, kOldCenter, kOldArena, boss::Kind::Hollow);
    const Point player{kOldCenter.x + 250.0f, kOldCenter.y};
    boss::Events events{};
    for (int tick = 0; tick < 90; ++tick) events = boss::tick(hollow, player, kOldArena);
    CHECK(events.warning && hollow.spatialPattern == boss::Pattern::Fold);
    CHECK(hollow.fields.count(field::SlotState::Warning) == 2 && hollow.corridor.enabled);
}

void resizeKeepsHollowFoldSafeAndFadesOrdinaryConflict() {
    boss::State hollow;
    startHollowFold(hollow);
    field::Pool ordinary(field::PoolKind::Ordinary);
    field::PatchSpec disk;
    disk.shape = field::Shape::Disk;
    disk.ownerId = 0xA001u;
    disk.center = {hollow.p.x, hollow.p.y + 96.0f};
    disk.radius = 20.0f;
    disk.warningTicks = 1;
    disk.activeTicks = 100;
    disk.damage = 1800;
    const field::Bounds oldBounds{kOldArena.left, kOldArena.right, kOldArena.top, kOldArena.bottom};
    const field::Bounds resizedBounds{kResizedArena.left, kResizedArena.right,
                                      kResizedArena.top, kResizedArena.bottom};
    const auto ticket = ordinary.add(disk, oldBounds, hollow.corridor,
                                      hollow.fields.areaOccupied(oldBounds));
    CHECK(ticket != 0);
    (void)ordinary.tick(disk.center, disk.center, 12.0f);
    (void)ordinary.tick(disk.center, disk.center, 12.0f);
    CHECK(ordinary.count(field::SlotState::Active) == 1);
    CHECK(!field::detail::conflictsCorridor(disk, hollow.corridor));

    ordinary.remap(oldBounds, resizedBounds, hollow.fields.areaOccupied(oldBounds));
    boss::remap(hollow, kOldArena, kResizedArena,
                ordinary.areaOccupied(resizedBounds));
    CHECK(hollow.fields.count(field::SlotState::Warning) == 2);
    CHECK(hollow.corridor.enabled);
    for (const auto& patch : hollow.fields.items()) {
        if (patch.state == field::SlotState::Empty) continue;
        CHECK(!field::detail::conflictsCorridor(patch.spec, hollow.corridor));
    }

    const auto* movedDisk = static_cast<const field::Patch*>(nullptr);
    for (const auto& patch : ordinary.items())
        if (patch.ticket == ticket) movedDisk = &patch;
    CHECK(movedDisk != nullptr && movedDisk->state == field::SlotState::Active);
    CHECK(movedDisk != nullptr && field::detail::conflictsCorridor(movedDisk->spec, hollow.corridor));
    ordinary.cancelUnsafeCorridors(hollow.corridor);
    CHECK(ordinary.count(field::SlotState::Active) == 0 &&
          ordinary.count(field::SlotState::Fading) == 1);

    const Point routeStart{kResizedArena.left, hollow.p.y};
    const Point routeEnd{kResizedArena.right, hollow.p.y};
    CHECK(ordinary.tick(routeStart, routeEnd, 12.0f).damage == 0);
    CHECK(ordinary.count(field::SlotState::Fading) == 1); // Fresh cancellation tick preserves the full fade.
    for (int tick = 0; tick < field::kHarmlessFadeTicks - 1; ++tick)
        CHECK(ordinary.tick(routeStart, routeEnd, 12.0f).damage == 0);
    CHECK(ordinary.count(field::SlotState::Fading) == 1);
    CHECK(ordinary.tick(routeStart, routeEnd, 12.0f).damage == 0);
    CHECK(ordinary.occupiedSlotCount() == 0);

    combat::IncomingHit hitDuringFold{};
    for (int tick = 0; tick <= 120; ++tick)
        hitDuringFold = hollow.fields.tick(routeStart, routeEnd, 12.0f);
    CHECK(hollow.fields.count(field::SlotState::Active) == 2);
    CHECK(hitDuringFold.damage == 0);
    CHECK(hollow.fields.tick(routeStart, routeEnd, 12.0f).damage == 0);

    // A stronger shrink moves the boss's own fixed-width Fold seams into its
    // fixed-width safe route. Both warnings must become harmless immediately.
    boss::State tight;
    startHollowFold(tight);
    boss::State activeTight=tight;
    const Arena tightArena{0.f,924.f,0.f,340.f};
    boss::remap(tight,kOldArena,tightArena);
    CHECK(tight.fields.liveCount()==0);
    CHECK(tight.fields.occupiedSlotCount()==0); // Canceled warnings disappear; active fields fade.
    CHECK(tight.fields.tick(tight.corridor.a,tight.corridor.b,12.f).damage==0);
    for(int tick=0;tick<=120;++tick)(void)activeTight.fields.tick(activeTight.corridor.a,activeTight.corridor.a,12.f);
    CHECK(activeTight.fields.count(field::SlotState::Active)==2);
    boss::remap(activeTight,kOldArena,tightArena);
    CHECK(activeTight.fields.liveCount()==0);
    CHECK(activeTight.fields.count(field::SlotState::Fading)==2);
    CHECK(activeTight.fields.tick(activeTight.corridor.a,activeTight.corridor.b,12.f).damage==0);
}

} // namespace

int main() {
    cocoonUsesSharedDashAndImmunityGates();
    finalBossDefeatRecordsNoPickupExactlyOnce();
    fullCampaignRewardCeiling();
    exhaustedOfferCanDiscardWhenOnlyMagnetIsIrrelevant();
    resizeKeepsHollowFoldSafeAndFadesOrdinaryConflict();
    if (failures != 0) {
        std::cerr << failures << " M6 integration check(s) failed across " << checks << " checks\n";
        return 1;
    }
    std::cout << "M6 integration checks passed: " << checks << " checks\n";
    return 0;
}
