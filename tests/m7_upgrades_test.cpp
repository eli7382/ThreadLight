#include "player_upgrades.h"
#include "silk_utilities.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
namespace u = threadlight::upgrades;
namespace h = threadlight::combat;
namespace s = threadlight::silk_utilities;
namespace f = threadlight::foundation;

int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << "FAIL " << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(x) check((x), #x, __LINE__)
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }

int totalRanks(const u::Ranks& ranks) {
    int total = 0;
    for (int i = 0; i < u::kPermanentCount; ++i) total += u::rank(ranks, static_cast<u::Id>(i));
    return total;
}

void preserveOrdinalsAndDeriveEffects() {
    CHECK(u::index(u::Id::Razor) == 0);
    CHECK(u::index(u::Id::Living) == 1);
    CHECK(u::index(u::Id::Broad) == 2);
    CHECK(u::index(u::Id::Quick) == 3);
    CHECK(u::index(u::Id::Rhythm) == 4);
    CHECK(u::index(u::Id::Heartwood) == 5);
    CHECK(u::index(u::Id::Magnet) == 6);
    CHECK(u::index(u::Id::Ironbark) == 7);
    CHECK(u::index(u::Id::Longstride) == 8);
    CHECK(u::index(u::Id::Needlecatch) == 9);
    CHECK(u::index(u::Id::Bloodstitch) == 10);
    CHECK(u::index(u::Id::Restitch) == 11);
    CHECK(u::index(u::Id::Count) == 12);
    CHECK(u::kPermanentCount == 11);
    CHECK(u::maximum(u::Id::Restitch) == 0 && u::maximum(u::Id::Count) == 0);

    constexpr int dr[] = {0, 7, 12, 17, 21, 25};
    constexpr float dash[] = {3.7f, 4.f, 4.3f, 4.6f};
    constexpr int needle[] = {0, 600, 480, 360};
    constexpr int bloodKills[] = {0, 5, 4, 3};
    constexpr int bloodCap[] = {0, 900, 1200, 1500};
    u::Ranks ranks{};
    for (int rank = 0; rank <= 5; ++rank) {
        ranks.value[u::index(u::Id::Ironbark)] = rank;
        const auto stats = u::derive(ranks);
        CHECK(stats.damageReductionPercent == dr[rank]);
    }
    for (int rank = 0; rank <= 3; ++rank) {
        ranks.value[u::index(u::Id::Longstride)] = rank;
        CHECK(near(u::derive(ranks).dashMultiplier, dash[rank]));
    }
    for (int rank = 0; rank <= 3; ++rank) {
        ranks.value[u::index(u::Id::Needlecatch)] = rank;
        ranks.value[u::index(u::Id::Bloodstitch)] = rank;
        const auto stats = u::derive(ranks);
        CHECK(stats.needleCooldownTicks == needle[rank]);
        CHECK(stats.bloodKillsPerHeal == bloodKills[rank]);
        CHECK(stats.bloodWaveCap == bloodCap[rank]);
    }

    ranks = {};
    auto baseline = u::derive(ranks);
    CHECK(baseline.silkDamage == 100 && baseline.maxHp == 10000);
    CHECK(near(baseline.activeSilkSeconds, 1.2f) && near(baseline.silkHalfWidth, 5.f));
    CHECK(near(baseline.moveSpeed, 255.f) && near(baseline.dashCooldownSeconds, 1.85f));
    CHECK(near(baseline.pickupRadius, 110.f) && near(baseline.pickupPullSpeed, 220.f));
    CHECK(near(baseline.dashMultiplier, 3.7f));
    ranks.value[u::index(u::Id::Razor)] = 5;
    ranks.value[u::index(u::Id::Living)] = 3;
    ranks.value[u::index(u::Id::Broad)] = 3;
    ranks.value[u::index(u::Id::Quick)] = 5;
    ranks.value[u::index(u::Id::Rhythm)] = 3;
    ranks.value[u::index(u::Id::Heartwood)] = 5;
    ranks.value[u::index(u::Id::Magnet)] = 3;
    baseline = u::derive(ranks);
    CHECK(baseline.silkDamage == 270 && baseline.maxHp == 16000);
    CHECK(near(baseline.activeSilkSeconds, 2.1f) && near(baseline.silkHalfWidth, 11.f));
    CHECK(near(baseline.moveSpeed, 315.f) && near(baseline.dashCooldownSeconds, 1.4f));
    CHECK(near(baseline.pickupRadius, 220.f) && near(baseline.pickupPullSpeed, 380.f));

    for (int i = 0; i < u::kPermanentCount; ++i) {
        const auto id = static_cast<u::Id>(i);
        CHECK(u::maximum(id) >= 1);
        CHECK(u::name(id)[0] != '\0');
    }
    CHECK(u::name(u::Id::Restitch)[0] != '\0');
    ranks = {};
    h::PlayerHealth health{};
    for (auto id : {u::Id::Ironbark, u::Id::Longstride, u::Id::Needlecatch, u::Id::Bloodstitch})
        CHECK(!u::effectText(id, ranks, health).empty());
    CHECK(u::effectText(u::Id::Needlecatch, ranks, health).find("0.0s") == std::string::npos);
    CHECK(u::effectText(u::Id::Restitch, ranks, health).find("Restore") != std::string::npos);
}

void validateOffer(const u::Offer& offer, const u::Ranks& ranks, const h::PlayerHealth& health) {
    CHECK(offer.count >= 1 && offer.count <= 3);
    for (int i = 0; i < offer.count; ++i) {
        CHECK(u::index(offer.cards[i]) >= 0 && u::index(offer.cards[i]) < u::index(u::Id::Count));
        CHECK(u::eligible(offer.cards[i], ranks, health));
        for (int j = 0; j < i; ++j) CHECK(offer.cards[i] != offer.cards[j]);
    }
}

void offerDistributionDroughtDiscoveryAndBudgets() {
    std::array<bool, u::kPermanentCount> observed{};
    for (std::uint32_t seed = 1; seed <= 160; ++seed) {
        u::OfferState state{};
        state.rng = seed;
        const u::Ranks ranks{};
        const h::PlayerHealth health{};
        const auto offer = u::prepareOffer(state, ranks, health, 1);
        validateOffer(offer, ranks, health);
        CHECK(offer.count == 3);
        for (int i = 0; i < offer.count; ++i) {
            CHECK(offer.cards[i] != u::Id::Restitch);
            observed[static_cast<std::size_t>(u::index(offer.cards[i]))] = true;
        }
        const auto rngAfter = state.rng;
        const auto absencesAfter = state.absences;
        CHECK(u::prepareOffer(state, ranks, health, 1).cards == offer.cards);
        CHECK(state.rng == rngAfter && state.absences == absencesAfter);
    }
    for (bool seen : observed) CHECK(seen);

    u::Ranks ranks{};
    ranks.value[u::index(u::Id::Ironbark)] = 1;
    h::PlayerHealth health{};
    u::OfferState drought{};
    drought.rng = 22;
    drought.absences[u::index(u::Id::Ironbark)] = 3;
    const auto overdue = u::prepareOffer(drought, ranks, health, 1);
    CHECK(overdue.cards[0] == u::Id::Ironbark);

    h::PlayerHealth recoveryHealth{6000, 10000};
    u::OfferState recovery{};
    CHECK(u::eligible(u::Id::Restitch, ranks, recoveryHealth));
    CHECK(u::contains(u::prepareOffer(recovery, ranks, recoveryHealth, 1, true, true), u::Id::Restitch));
    recovery.recoveryMissed = true;
    CHECK(u::contains(u::prepareOffer(recovery, ranks, recoveryHealth, 2), u::Id::Restitch));
    recoveryHealth.hp = recoveryHealth.maxHp - 1499;
    CHECK(!u::eligible(u::Id::Restitch, ranks, recoveryHealth));
    recoveryHealth.hp = recoveryHealth.maxHp;
    CHECK(!u::contains(u::prepareOffer(recovery, ranks, recoveryHealth, 3), u::Id::Restitch));

    bool discoveryChecked = false;
    for (std::uint32_t seed = 1; seed <= 1000 && !discoveryChecked; ++seed) {
        u::OfferState state{};
        state.rng = seed;
        u::Ranks owned{};
        for (u::Id id : {u::Id::Razor, u::Id::Living, u::Id::Broad, u::Id::Quick, u::Id::Rhythm})
            owned.value[u::index(id)] = 1;
        const auto offer = u::prepareOffer(state, owned, health, 1);
        if (offer.count == 3 && u::rank(owned, offer.cards[0]) > 0 && u::rank(owned, offer.cards[1]) > 0) {
            CHECK(u::rank(owned, offer.cards[2]) == 0);
            discoveryChecked = true;
        }
    }
    CHECK(discoveryChecked);

    for (const std::uint32_t seed : {16u, 22u, 26u}) {
        u::OfferState state{};
        state.rng = seed;
        u::Ranks budgetRanks{};
        h::PlayerHealth budgetHealth{};
        const int budgets[] = {16, 22, 26};
        const int budget = budgets[seed == 16 ? 0 : seed == 22 ? 1 : 2];
        for (int credit = 1; credit <= budget; ++credit) {
            const auto offer = u::prepareOffer(state, budgetRanks, budgetHealth, static_cast<std::uint64_t>(credit));
            validateOffer(offer, budgetRanks, budgetHealth);
            const auto selected = offer.cards[(credit + static_cast<int>(seed)) % offer.count];
            if (selected == u::Id::Restitch) continue;
            ++budgetRanks.value[u::index(selected)];
            budgetHealth.maxHp = u::derive(budgetRanks).maxHp;
            budgetHealth.hp = budgetHealth.maxHp;
        }
        CHECK(totalRanks(budgetRanks) <= budget);
        for (int i = 0; i < u::kPermanentCount; ++i)
            CHECK(u::rank(budgetRanks, static_cast<u::Id>(i)) <= u::maximum(static_cast<u::Id>(i)));
    }

    const int maximumSelections = [] {
        int total = 0;
        for (int i = 0; i < u::kPermanentCount; ++i) total += u::maximum(static_cast<u::Id>(i));
        return total;
    }();
    for (const std::uint32_t seed : {16u, 22u, 26u}) {
        u::OfferState state{};
        state.rng = seed;
        u::Ranks complete{};
        h::PlayerHealth completeHealth{};
        for (int credit = 1; credit <= maximumSelections; ++credit) {
            const auto offer = u::prepareOffer(state, complete, completeHealth, static_cast<std::uint64_t>(credit));
            CHECK(offer.count > 0);
            validateOffer(offer, complete, completeHealth);
            int selected = -1;
            for (int i = 0; i < offer.count; ++i) {
                if (offer.cards[i] != u::Id::Restitch) { selected = i; break; }
            }
            CHECK(selected >= 0);
            if (selected < 0) break;
            const auto id = offer.cards[selected];
            CHECK(u::rank(complete, id) < u::maximum(id));
            ++complete.value[u::index(id)];
            completeHealth.maxHp = u::derive(complete).maxHp;
            completeHealth.hp = completeHealth.maxHp;
        }
        CHECK(totalRanks(complete) == maximumSelections);
        for (int i = 0; i < u::kPermanentCount; ++i) {
            const auto id = static_cast<u::Id>(i);
            CHECK(u::rank(complete, id) == u::maximum(id));
            CHECK(!u::eligible(id, complete, completeHealth));
        }
        CHECK(u::prepareOffer(state, complete, completeHealth,
                              static_cast<std::uint64_t>(maximumSelections + 1)).count == 0);
    }
}

void bloodstitchThresholdsCapsAndHealthGates() {
    constexpr int threshold[] = {0, 5, 4, 3};
    constexpr int cap[] = {0, 900, 1200, 1500};
    for (int rank = 1; rank <= 3; ++rank) {
        s::BloodstitchState state{};
        h::PlayerHealth health{5000, 10000};
        for (int kill = 1; kill < threshold[rank]; ++kill) {
            CHECK(s::onOrdinaryKill(state, rank, health) == 0);
            CHECK(health.hp == 5000);
        }
        CHECK(s::onOrdinaryKill(state, rank, health) == 300);
        CHECK(health.hp == 5300 && state.healedCenti == 300 && state.kills == 0);
        for (int kill = 1; kill < threshold[rank]; ++kill) (void)s::onOrdinaryKill(state, rank, health);
        CHECK(s::onOrdinaryKill(state, rank, health) == 300);
        CHECK(state.healedCenti == 600);
        CHECK(cap[rank] >= state.healedCenti);

        s::BloodstitchState capState{};
        h::PlayerHealth capHealth{1000, 10000};
        const int completeGroups = cap[rank] / 300;
        for (int group = 0; group < completeGroups; ++group) {
            for (int kill = 1; kill < threshold[rank]; ++kill)
                CHECK(s::onOrdinaryKill(capState, rank, capHealth) == 0);
            CHECK(s::onOrdinaryKill(capState, rank, capHealth) == 300);
        }
        CHECK(capState.healedCenti == cap[rank]);
        CHECK(capHealth.hp == 1000 + cap[rank]);
    }

    s::BloodstitchState fullState{};
    h::PlayerHealth fullHealth{};
    for (int i = 0; i < 5; ++i) CHECK(s::onOrdinaryKill(fullState, 1, fullHealth) == 0);
    CHECK(fullState.kills == 0 && fullState.healedCenti == 0 && fullHealth.hp == fullHealth.maxHp);
    fullHealth.hp -= 1000;
    for (int i = 0; i < 4; ++i) CHECK(s::onOrdinaryKill(fullState, 1, fullHealth) == 0);
    CHECK(s::onOrdinaryKill(fullState, 1, fullHealth) == 300);
    CHECK(fullHealth.hp == 9300 && fullState.healedCenti == 300);

    s::BloodstitchState partial{};
    partial.healedCenti = 1450;
    h::PlayerHealth health{4000, 10000};
    for (int i = 0; i < 2; ++i) CHECK(s::onOrdinaryKill(partial, 3, health) == 0);
    CHECK(s::onOrdinaryKill(partial, 3, health) == 50);
    CHECK(partial.healedCenti == 1500 && health.hp == 4050);
    for (int i = 0; i < 3; ++i) CHECK(s::onOrdinaryKill(partial, 3, health) == 0);
    CHECK(health.hp == 4050 && partial.kills == 0);

    s::BloodstitchState excluded{};
    h::PlayerHealth injured{5000, 10000};
    for (int i = 0; i < 9; ++i) CHECK(s::onOrdinaryKill(excluded, 2, injured, false) == 0);
    CHECK(excluded.kills == 0);
    CHECK(s::onOrdinaryKill(excluded, 0, injured) == 0 && excluded.kills == 0);
    h::PlayerHealth dead{0, 10000};
    CHECK(s::onOrdinaryKill(excluded, 1, dead) == 0);
    CHECK(dead.hp == 0 && excluded.kills == 0);

    s::BloodstitchState nextEncounter{};
    h::PlayerHealth nextHealth{5000, 10000};
    CHECK(s::onOrdinaryKill(nextEncounter, 1, nextHealth) == 0);
    nextEncounter = {};
    for (int i = 0; i < 4; ++i) CHECK(s::onOrdinaryKill(nextEncounter, 1, nextHealth) == 0);
    CHECK(s::onOrdinaryKill(nextEncounter, 1, nextHealth) == 300);
}

void needlecatchOfferAvailabilityGate() {
    u::Ranks ranks{};
    h::PlayerHealth health{};
    CHECK(u::eligible(u::Id::Needlecatch, ranks, health));
    CHECK(!u::eligible(u::Id::Needlecatch, ranks, health, true, false));
    for (std::uint32_t seed = 1; seed <= 96; ++seed) {
        u::OfferState state{};
        state.rng = seed;
        const auto offer = u::prepareOffer(state, ranks, health, 1, true, false, false);
        validateOffer(offer, ranks, health);
        CHECK(!u::contains(offer, u::Id::Needlecatch));
    }

    ranks.value[u::index(u::Id::Needlecatch)] = u::maximum(u::Id::Needlecatch);
    CHECK(!u::eligible(u::Id::Needlecatch, ranks, health, true, true));
    ranks.value[u::index(u::Id::Needlecatch)] = 0;

    f::RewardLedger choiceLedger;
    const auto choiceReward = choiceLedger.registerEnemy(20, 0);
    CHECK(choiceLedger.defeatEnemy(choiceReward).has_value());
    CHECK(choiceLedger.collectPickup(choiceReward));
    const u::Offer needleOffer{1, {u::Id::Needlecatch, u::Id::Count, u::Id::Count}, 1};
    CHECK(!u::resolveChoice(choiceLedger, f::Phase::PendingRewards, needleOffer, 0,
                            ranks, health, true, false));
    CHECK(ranks.value[u::index(u::Id::Needlecatch)] == 0 && choiceLedger.pendingCredits() == 1);
    CHECK(u::resolveChoice(choiceLedger, f::Phase::PendingRewards, needleOffer, 0, ranks, health));
    CHECK(ranks.value[u::index(u::Id::Needlecatch)] == 1 && choiceLedger.pendingCredits() == 0);

    u::Ranks onlyNeedlecatch{};
    for (int i = 0; i < u::kPermanentCount; ++i) {
        const auto id = static_cast<u::Id>(i);
        onlyNeedlecatch.value[i] = id == u::Id::Needlecatch ? 0 : u::maximum(id);
    }
    u::Offer empty{1, {}, 0};
    f::RewardLedger exhaustionLedger;
    const auto exhaustionReward = exhaustionLedger.registerEnemy(20, 0);
    CHECK(exhaustionLedger.defeatEnemy(exhaustionReward).has_value());
    CHECK(exhaustionLedger.collectPickup(exhaustionReward));
    CHECK(!u::resolveExhaustedCredit(exhaustionLedger, f::Phase::PendingRewards,
                                    empty, onlyNeedlecatch, health, true, true));
    CHECK(u::resolveExhaustedCredit(exhaustionLedger, f::Phase::PendingRewards,
                                    empty, onlyNeedlecatch, health, true, false));
}

void needlecatchCooldownBoundariesAndPause() {
    constexpr int cooldown[] = {0, 600, 480, 360};
    for (int rank = 1; rank <= 3; ++rank) {
        s::NeedlecatchState state{};
        CHECK(!s::ready(state, 0) && !s::trigger(state, 0));
        CHECK(s::ready(state, rank));
        CHECK(s::trigger(state, rank));
        CHECK(state.cooldownTicks == cooldown[rank] && !s::ready(state, rank));
        for (int i = 0; i < cooldown[rank] / 2; ++i) s::tick(state, true);
        const int halfway = state.cooldownTicks;
        for (int i = 0; i < 100; ++i) s::tick(state, false);
        CHECK(state.cooldownTicks == halfway);
        while (state.cooldownTicks > 1) {
            s::tick(state, true);
            CHECK(!s::ready(state, rank));
        }
        CHECK(state.cooldownTicks == 1 && !s::ready(state, rank));
        s::tick(state, true);
        CHECK(state.cooldownTicks == 0 && s::ready(state, rank));
        CHECK(s::trigger(state, rank) && state.cooldownTicks == cooldown[rank]);
        state = {};
        CHECK(s::ready(state, rank));
    }
    s::NeedlecatchState rankZero{};
    CHECK(!s::ready(rankZero, -1) && !s::trigger(rankZero, -1));
}
}

int main() {
    preserveOrdinalsAndDeriveEffects();
    offerDistributionDroughtDiscoveryAndBudgets();
    bloodstitchThresholdsCapsAndHealthGates();
    needlecatchOfferAvailabilityGate();
    needlecatchCooldownBoundariesAndPause();
    if (failures) return EXIT_FAILURE;
    std::cout << "All M7 upgrade and silk utility tests passed.\n";
}
