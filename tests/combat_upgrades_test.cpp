#include "combat.h"
#include "game_logic.h"
#include "player_upgrades.h"
#include "skitter.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << "FAIL " << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(x) check((x), #x, __LINE__)
namespace c = threadlight::combat;
namespace u = threadlight::upgrades;
namespace f = threadlight::foundation;
bool near(float a, float b) { return std::abs(a - b) < .0001f; }

f::RewardLedger credits(int count) {
    f::RewardLedger ledger;
    int total = 0;
    for (int i = 0; i < count; ++i) total += i == 0 ? 20 : i == 1 ? 30 : i == 2 ? 40 : 48;
    if (total) { auto id = ledger.registerEnemy(total, 0); ledger.defeatEnemy(id); ledger.collectPickup(id); }
    return ledger;
}

void testSharedCombatAndBreakpoints() {
    c::EnemyHealth brute{300, 300};
    CHECK(c::applySilk(brute, 165) == c::SilkHit::Damaged);
    CHECK(brute.hp == 135);
    for (int i = 0; i < 10; ++i) CHECK(c::applySilk(brute, 165) == c::SilkHit::None);
    for (int i = 0; i < 32; ++i) c::tick(brute);
    CHECK(c::applySilk(brute, 165) == c::SilkHit::None);
    c::tick(brute);
    CHECK(c::applySilk(brute, 165) == c::SilkHit::Killed);
    CHECK(brute.hp == 0);
    CHECK(c::applySilk(brute, 165) == c::SilkHit::None);
    c::EnemyHealth skitter{150, 150};
    CHECK(c::applySilk(skitter, 130) == c::SilkHit::Damaged);
    CHECK(skitter.hp == 20);
    c::EnemyHealth stronger{150, 150};
    CHECK(c::applySilk(stronger, 165) == c::SilkHit::Killed);
    CHECK(c::definition(c::EnemyKind::Hunter).hp == 100);
    CHECK(c::definition(c::EnemyKind::Brute).hp == 300);
    CHECK(c::definition(c::EnemyKind::Skitter).speed == threadlight::skitter::kFlankSpeed);
}

void testSweptCollisionAndIncomingOwnership() {
    CHECK(c::silkContact({-30, 0}, {30, 0}, 1, {0, -10}, {0, 10}, 1));
    CHECK(!c::silkContact({-30, 13}, {30, 13}, 1, {0, -10}, {0, 10}, 1));
    CHECK(c::silkContact({0, 0}, {0, 0}, 1, {0, 0}, {0, 0}, 1));
    CHECK(c::bodyContact({0, 0}, {100, 0}, 12, {50, 0}, {50, 0}, 11));
    CHECK(!c::bodyContact({0, 0}, {100, 0}, 12, {50, 24}, {50, 24}, 11));
    CHECK(near(f::segmentsDistanceSquared({0, 0}, {10, 0}, {5, 0}, {15, 0}), 0));
    CHECK(near(f::segmentsDistanceSquared({0, 0}, {10, 0}, {0, 3}, {10, 3}), 9));
    c::IncomingHit first, reversed;
    first.consider(1000, 7); first.consider(1600, 9); first.consider(1600, 3);
    reversed.consider(1600, 3); reversed.consider(1600, 9); reversed.consider(1000, 7);
    CHECK(first.damage == reversed.damage && first.sourceId == reversed.sourceId);
    CHECK(first.damage == 1600 && first.sourceId == 3);
    c::PlayerHealth player;
    CHECK(c::canReceiveIncoming(player,false));
    CHECK(!c::canReceiveIncoming(player,true));
    CHECK(c::applyIncoming(player, first, threadlight::dashProtectsCollisionThisFrame(.00001f)) == 0);
    CHECK(player.hp == 10000 && player.immunityTicks == 0);
    CHECK(c::applyIncoming(player, first, false) == 1600);
    CHECK(player.hp == 8400);
    CHECK(!c::canReceiveIncoming(player,false));
    for (int i = 0; i < 93; ++i) c::tick(player);
    CHECK(c::applyIncoming(player, first, false) == 0);
    c::tick(player);
    CHECK(c::canReceiveIncoming(player,false));
    CHECK(c::applyIncoming(player, first, false) == 1600);
    c::PlayerHealth dying{100, 10000};
    CHECK(c::applyIncoming(dying, first, false) == 100);
    CHECK(!dying.alive() && c::applyIncoming(dying, first, false) == 0);
    CHECK(!c::canReceiveIncoming(dying,false));
}

void testIronbarkRankVAppliedDamageMatchesItsAdvertisedReduction() {
    u::Ranks ranks{};
    ranks.value[u::index(u::Id::Ironbark)] = u::maximum(u::Id::Ironbark);
    const auto stats = u::derive(ranks);
    CHECK(stats.damageReductionPercent == 25);

    c::PlayerHealth player;
    c::IncomingHit hit{};
    hit.consider(1000, 1);
    const int taken = c::applyIncoming(player, hit, false, nullptr,
                                       stats.damageReductionPercent);
    CHECK(taken == 750);
    CHECK(player.hp == 9250);
    CHECK(player.immunityTicks == c::kPlayerImmunityTicks);
}

void testDerivedRankValues() {
    u::Ranks ranks;
    auto stats = u::derive(ranks);
    CHECK(stats.silkDamage == 100 && stats.maxHp == 10000);
    CHECK(near(stats.moveSpeed, 255) && near(stats.dashCooldownSeconds, 1.85f));
    constexpr int damage[] = {100, 130, 165, 200, 235, 270};
    constexpr int health[] = {10000, 11200, 12400, 13600, 14800, 16000};
    constexpr float speed[] = {255, 269, 282, 294, 305, 315};
    for (int rank = 0; rank <= 5; ++rank) {
        ranks.value[u::index(u::Id::Razor)] = rank;
        ranks.value[u::index(u::Id::Heartwood)] = rank;
        ranks.value[u::index(u::Id::Quick)] = rank;
        stats = u::derive(ranks);
        CHECK(stats.silkDamage == damage[rank]); CHECK(stats.maxHp == health[rank]); CHECK(near(stats.moveSpeed, speed[rank]));
    }
    constexpr float lives[] = {1.2f, 1.5f, 1.8f, 2.1f};
    constexpr float widths[] = {5, 7, 9, 11};
    constexpr float cooldown[] = {1.85f, 1.7f, 1.55f, 1.4f};
    constexpr float radius[] = {110, 145, 180, 220};
    constexpr float pull[] = {220, 270, 320, 380};
    for (int rank = 0; rank <= 3; ++rank) {
        for (auto id : {u::Id::Living, u::Id::Broad, u::Id::Rhythm, u::Id::Magnet}) ranks.value[u::index(id)] = rank;
        stats = u::derive(ranks);
        CHECK(near(stats.activeSilkSeconds, lives[rank])); CHECK(near(stats.silkHalfWidth, widths[rank]));
        CHECK(near(stats.dashCooldownSeconds, cooldown[rank])); CHECK(near(stats.pickupRadius, radius[rank])); CHECK(near(stats.pickupPullSpeed, pull[rank]));
    }
    for (auto& rank : ranks.value) rank = 1000;
    stats = u::derive(ranks);
    CHECK(stats.silkDamage == 270 && stats.maxHp == 16000 && stats.moveSpeed == 315);
}

void testCreditTransactionAndRecovery() {
    auto ledger = credits(8);
    u::Ranks ranks;
    c::PlayerHealth player{5500, 10000};
    u::Offer heart{1, {u::Id::Heartwood, u::Id::Count, u::Id::Count}, 1};
    CHECK(!u::resolveChoice(ledger, f::Phase::Spawning, heart, 0, ranks, player));
    CHECK(!u::resolveChoice(ledger, f::Phase::PendingRewards, heart, -1, ranks, player));
    CHECK(u::resolveChoice(ledger, f::Phase::PendingRewards, heart, 0, ranks, player));
    CHECK(player.hp == 6700 && player.maxHp == 11200);
    CHECK(player.maxHp - player.hp == 4500);
    CHECK(!u::resolveChoice(ledger, f::Phase::PendingRewards, heart, 0, ranks, player));
    CHECK(player.hp == 6700 && u::rank(ranks, u::Id::Heartwood) == 1);
    u::Offer heal{2, {u::Id::Restitch, u::Id::Count, u::Id::Count}, 1};
    CHECK(u::resolveChoice(ledger, f::Phase::PendingRewards, heal, 0, ranks, player));
    CHECK(player.hp == 10200 && player.maxHp == 11200);
    CHECK(!u::eligible(u::Id::Restitch, ranks, player));
    player.hp = 1000; heal.credit = 3;
    CHECK(u::resolveChoice(ledger, f::Phase::PendingRewards, heal, 0, ranks, player));
    CHECK(player.hp == 4500);
    player.hp = 0; heal.credit = 4;
    CHECK(!u::resolveChoice(ledger, f::Phase::PendingRewards, heal, 0, ranks, player));
    CHECK(player.hp == 0 && ledger.nextCredit() == 4);
}

void testOfferEligibilityDeterminismAndPity() {
    for (unsigned seed = 1; seed <= 400; ++seed) {
        u::OfferState a, b; a.rng = b.rng = seed;
        u::Ranks ranks; c::PlayerHealth player;
        const auto x = u::prepareOffer(a, ranks, player, 1);
        const auto y = u::prepareOffer(b, ranks, player, 1);
        CHECK(x.cards == y.cards && x.count == 3);
        for (int i = 0; i < x.count; ++i) {
            CHECK(u::eligible(x.cards[i], ranks, player));
            CHECK(x.cards[i] != u::Id::Restitch);
            for (int j = 0; j < i; ++j) CHECK(x.cards[i] != x.cards[j]);
        }
        const auto rngBefore = a.rng; const auto history = a.absences;
        CHECK(u::prepareOffer(a, ranks, player, 1).cards == x.cards);
        CHECK(a.rng == rngBefore && a.absences == history);
    }
    u::Ranks ranks; c::PlayerHealth player;
    ranks.value[u::index(u::Id::Razor)] = 1;
    u::OfferState state; state.absences[u::index(u::Id::Razor)] = 3;
    CHECK(u::contains(u::prepareOffer(state, ranks, player, 1), u::Id::Razor));
    player.hp = 6000;
    u::OfferState preBoss;
    CHECK(u::contains(u::prepareOffer(preBoss, ranks, player, 1, true, true), u::Id::Restitch));
    CHECK(!preBoss.recoveryMissed);
    state.recoveryMissed = true;
    CHECK(u::contains(u::prepareOffer(state, ranks, player, 2), u::Id::Restitch));
    for (int i = 0; i < u::kPermanentCount; ++i) ranks.value[i] = u::maximum(static_cast<u::Id>(i));
    player.hp = player.maxHp;
    CHECK(u::prepareOffer(state, ranks, player, 3).count == 0);
    ranks.value[u::index(u::Id::Living)] = 2;
    const auto one = u::prepareOffer(state, ranks, player, 4);
    CHECK(one.count == 1 && one.cards[0] == u::Id::Living);
    ranks.value[u::index(u::Id::Magnet)] = 0;
    CHECK(!u::contains(u::prepareOffer(state, ranks, player, 5, false), u::Id::Magnet));
    player.hp = 0; state.recoveryMissed = true;
    CHECK(u::prepareOffer(state, ranks, player, 6).count == 0);
}
void testPendingBatchAndBoundedSkitterPressure() {
    auto ledger = credits(10);
    u::Ranks ranks; c::PlayerHealth player{5000, 10000}; u::OfferState state;
    while (ledger.pendingCredits()) {
        const auto id = ledger.nextCredit();
        const auto offer = u::prepareOffer(state, ranks, player, id);
        CHECK(offer.count >= 1 && offer.count <= 3);
        CHECK(u::resolveChoice(ledger, f::Phase::PendingRewards, offer, 0, ranks, player));
        const auto after = ranks.value; const int hp = player.hp;
        CHECK(!u::resolveChoice(ledger, f::Phase::PendingRewards, offer, 0, ranks, player));
        CHECK(ranks.value == after && player.hp == hp);
    }
    CHECK(ledger.counters().creditsResolved == 10);
    namespace sk = threadlight::skitter;
    std::array<sk::State, 5> skitters{};
    std::array<f::Point, 5> positions{}, velocities{};
    for (auto& skitter : skitters) skitter.ticksRemaining = 1;
    for (int tick = 0; tick < 1200; ++tick) {
        int commitments = 0;
        for (const auto& skitter : skitters) commitments += sk::committed(skitter) ? 1 : 0;
        for (size_t i = 0; i < skitters.size(); ++i) {
            const bool before = sk::committed(skitters[i]);
            sk::tick(skitters[i], positions[i], velocities[i], {100, 100}, {1, 0}, {-500, 500, -500, 500}, commitments < 2);
            commitments += (sk::committed(skitters[i]) ? 1 : 0) - (before ? 1 : 0);
            CHECK(commitments <= 2);
        }
    }
}

void testExhaustionAndCommittedResize() {
    auto ledger = credits(2); u::Ranks ranks; c::PlayerHealth health;
    u::Offer empty{1, {}, 0};
    CHECK(!u::resolveExhaustedCredit(ledger, f::Phase::PendingRewards, empty, ranks, health));
    for (int i = 0; i < u::kPermanentCount; ++i) ranks.value[i] = u::maximum(static_cast<u::Id>(i));
    health.hp = health.maxHp = u::derive(ranks).maxHp;
    CHECK(!u::resolveExhaustedCredit(ledger, f::Phase::Spawning, empty, ranks, health));
    CHECK(u::resolveExhaustedCredit(ledger, f::Phase::PendingRewards, empty, ranks, health));
    CHECK(!u::resolveExhaustedCredit(ledger, f::Phase::PendingRewards, empty, ranks, health));
    empty.credit = 2; health.hp -= 3500;
    CHECK(!u::resolveExhaustedCredit(ledger, f::Phase::PendingRewards, empty, ranks, health));
    namespace sk = threadlight::skitter;
    sk::State state; state.phase = sk::Phase::Burst; state.ticksRemaining = 20;
    state.burstStart = {20, 40}; state.burstEnd = {80, 100};
    const float progress = 19.f / 39.f;
    f::Point p{20+60*progress,40+60*progress}, v{260,260};
    sk::remapBounds(state,p,v,{0,100,0,200},{11,311,22,122});
    CHECK(state.phase == sk::Phase::Burst && state.ticksRemaining == 20);
    CHECK(near(state.burstStart.x,71) && near(state.burstStart.y,42));
    CHECK(near(state.burstEnd.x,251) && near(state.burstEnd.y,72));
    CHECK(near(p.x,71+180*progress) && near(p.y,42+30*progress));
    CHECK(v.x == 0 && v.y == 0);
    const auto before = p;
    sk::tick(state,p,v,{0,0},{0,0},{11,311,22,122});
    CHECK(p.x > before.x && p.y > before.y);
    CHECK(near(p.x,71+180*(20.f/39.f)));
}

}

int main() {
    testSharedCombatAndBreakpoints(); testSweptCollisionAndIncomingOwnership();
    testIronbarkRankVAppliedDamageMatchesItsAdvertisedReduction();
    testDerivedRankValues(); testCreditTransactionAndRecovery(); testOfferEligibilityDeterminismAndPity();
    testPendingBatchAndBoundedSkitterPressure(); testExhaustionAndCommittedResize();
    if (failures) return EXIT_FAILURE;
    std::cout << "M2 combat, rank, credit, and offer checks passed.\n";
}
