#include "signature_catalog.h"
#include "signature_effects.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* expression, int line) {
    ++checks;
    if (condition) return;
    ++failures;
    std::cerr << "M7 signature check failed at line " << line << ": " << expression << '\n';
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

namespace catalog = threadlight::signature_catalog;
namespace effects = threadlight::signature_effects;
namespace reward = threadlight::signature_reward;
namespace foundation = threadlight::foundation;

bool sameOffer(const catalog::Offer& a, const catalog::Offer& b) {
    return a.bossSource == b.bossSource && a.count == b.count && a.choices == b.choices;
}

bool hasCategory(const catalog::Offer& offer, catalog::Category wanted) {
    for (int i = 0; i < offer.count; ++i)
        if (catalog::category(offer.choices[static_cast<std::size_t>(i)]) == wanted) return true;
    return false;
}

void categoryOfferIsSeededDistinctAndIdempotent() {
    catalog::State first;
    first.rng = 0x12345678u;
    const auto offer = catalog::prepare(first, 41);
    CHECK(offer.bossSource == 41 && offer.count == 3);
    CHECK(hasCategory(offer, catalog::Category::Offensive));
    CHECK(hasCategory(offer, catalog::Category::Utility));
    for (int i = 0; i < offer.count; ++i) {
        CHECK(!catalog::owns(first, offer.choices[static_cast<std::size_t>(i)]));
        for (int j = i + 1; j < offer.count; ++j)
            CHECK(offer.choices[static_cast<std::size_t>(i)] != offer.choices[static_cast<std::size_t>(j)]);
    }

    const std::uint32_t afterPrepare = first.rng;
    CHECK(sameOffer(offer, catalog::prepare(first, 41)));
    CHECK(sameOffer(offer, catalog::prepare(first, 99)));
    CHECK(first.rng == afterPrepare && first.pending);

    catalog::State sameSeed;
    sameSeed.rng = 0x12345678u;
    CHECK(sameOffer(offer, catalog::prepare(sameSeed, 41)));
    CHECK(catalog::category(catalog::Id::Count) == catalog::Category::Invalid);
    for (std::size_t i = 0; i < catalog::kCount; ++i) {
        const auto id = static_cast<catalog::Id>(i);
        CHECK(!catalog::name(id).empty());
        const auto lines = catalog::descriptionLines(id);
        for (std::string_view line : lines) CHECK(!line.empty() && line.size() <= 32);
    }
}

void categoryFallbackAndFourthGiftKeepRemainingChoices() {
    catalog::State utilityOnly;
    for (catalog::Id id : catalog::kOffensive) utilityOnly.owned[catalog::index(id)] = true;
    const auto utilities = catalog::prepare(utilityOnly, 10);
    CHECK(utilities.count == 2 && !hasCategory(utilities, catalog::Category::Offensive));
    for (int i = 0; i < utilities.count; ++i)
        CHECK(catalog::category(utilities.choices[static_cast<std::size_t>(i)]) == catalog::Category::Utility);

    catalog::State campaign;
    campaign.rng = 0x99887766u;
    for (std::uint64_t gift = 0; gift < 3; ++gift) {
        const std::uint64_t source = 100 + gift;
        const auto offer = catalog::prepare(campaign, source);
        reward::RewardState legacy;
        CHECK(reward::queue(legacy, source, true));
        CHECK(catalog::accept(campaign, legacy, foundation::Phase::PendingRewards, true, 0, 0));
        CHECK(catalog::ownedCount(campaign) == static_cast<int>(gift + 1));
        CHECK(!campaign.pending && legacy.owned && !legacy.pending);
        CHECK(!catalog::owns(campaign, offer.choices[1])); // Only the chosen signature is granted.
    }
    const auto fourthGift = catalog::prepare(campaign, 103);
    CHECK(fourthGift.count == 3 && catalog::ownedCount(campaign) == 3);
    for (int i = 0; i < fourthGift.count; ++i)
        CHECK(!catalog::owns(campaign, fourthGift.choices[static_cast<std::size_t>(i)]));

    std::array<int, catalog::kCount> seen{};
    for (std::uint32_t seed = 1; seed <= 1000; ++seed) {
        catalog::State sample;
        sample.rng = seed;
        for (std::uint64_t gift = 0; gift < 4; ++gift) {
            const bool offenseAvailable = [&] {
                for (catalog::Id id : catalog::kOffensive) if (!catalog::owns(sample, id)) return true;
                return false;
            }();
            const bool utilityAvailable = [&] {
                for (catalog::Id id : catalog::kUtility) if (!catalog::owns(sample, id)) return true;
                return false;
            }();
            const std::uint64_t source = 1000 + gift;
            const auto offer = catalog::prepare(sample, source);
            CHECK(offer.count == 3);
            CHECK(!offenseAvailable || hasCategory(offer, catalog::Category::Offensive));
            CHECK(!utilityAvailable || hasCategory(offer, catalog::Category::Utility));
            for (int i = 0; i < offer.count; ++i) {
                const auto id = offer.choices[static_cast<std::size_t>(i)];
                CHECK(!catalog::owns(sample, id));
                ++seen[catalog::index(id)];
                for (int j = i + 1; j < offer.count; ++j)
                    CHECK(id != offer.choices[static_cast<std::size_t>(j)]);
            }
            reward::RewardState legacy;
            CHECK(reward::queue(legacy, source, true));
            const int choice = offer.count > 0 ?
                static_cast<int>((seed + gift) % static_cast<std::uint32_t>(offer.count)) : 0;
            CHECK(catalog::accept(sample, legacy, foundation::Phase::PendingRewards,
                                  true, 0, choice));
        }
        CHECK(catalog::ownedCount(sample) == 4);
    }
    for (int offered : seen) CHECK(offered > 0);
}

void acceptanceValidatesAtomicallyAndNeverRerolls() {
    catalog::State state;
    const auto offer = catalog::prepare(state, 501);
    reward::RewardState legacy;
    CHECK(reward::queue(legacy, 501, true));

    CHECK(!catalog::accept(state, legacy, foundation::Phase::PendingRewards, true, 1, 0));
    CHECK(state.pending && catalog::ownedCount(state) == 0 && legacy.pending && !legacy.owned);
    CHECK(!catalog::accept(state, legacy, foundation::Phase::Spawning, true, 0, 0));
    CHECK(!catalog::accept(state, legacy, foundation::Phase::PendingRewards, true, 0, 3));
    CHECK(state.pending && legacy.pending && catalog::ownedCount(state) == 0);

    reward::RewardState wrongSource;
    CHECK(reward::queue(wrongSource, 777, true));
    CHECK(!catalog::accept(state, wrongSource, foundation::Phase::PendingRewards, true, 0, 0));
    CHECK(state.pending && wrongSource.pending && !wrongSource.owned);

    CHECK(catalog::accept(state, legacy, foundation::Phase::PendingRewards, true, 0, 1));
    const auto chosen = offer.choices[1];
    CHECK(catalog::owns(state, chosen) && catalog::ownedCount(state) == 1);
    CHECK(!state.pending && legacy.owned && !legacy.pending);
    CHECK(!catalog::accept(state, legacy, foundation::Phase::PendingRewards, true, 0, 1));
    CHECK(catalog::ownedCount(state) == 1 && catalog::prepare(state, 501).count == 0);

    const auto next = catalog::prepare(state, 502);
    reward::RewardState dyingLegacy;
    CHECK(next.count == 3 && reward::queue(dyingLegacy, 502, true));
    CHECK(!catalog::accept(state, dyingLegacy, foundation::Phase::PendingRewards, false, 0, 0));
    CHECK(!state.pending && !dyingLegacy.pending && !dyingLegacy.owned);
    CHECK(catalog::ownedCount(state) == 1 && catalog::prepare(state, 502).count == 0);
}

void slipknotTimersFreezeAndOnlySlowMovement() {
    effects::SlowState slow;
    CHECK(!effects::onOrdinaryHit(slow, false));
    CHECK(slow.remainingTicks == 0 && slow.cooldownTicks == 0);
    CHECK(effects::onOrdinaryHit(slow, true)); // Guaranteed when its per-enemy cooldown is ready.
    CHECK(slow.remainingTicks == 90 && slow.cooldownTicks == 300);
    CHECK(std::abs(effects::factor(slow, false) - 0.75f) < 0.0001f);
    CHECK(std::abs(effects::factor(slow, true) - 0.92f) < 0.0001f);
    CHECK(!effects::onOrdinaryHit(slow, true));
    CHECK(slow.remainingTicks == 90 && slow.cooldownTicks == 300);

    effects::tick(slow, false);
    effects::tick(slow, false);
    CHECK(slow.remainingTicks == 90 && slow.cooldownTicks == 300);
    for (int i = 0; i < 89; ++i) effects::tick(slow, true);
    CHECK(slow.remainingTicks == 1 && slow.cooldownTicks == 211);
    effects::tick(slow, true);
    CHECK(slow.remainingTicks == 0 && slow.cooldownTicks == 210);
    CHECK(effects::factor(slow, false) == 1.0f);
    CHECK(!effects::onOrdinaryHit(slow, true));
    CHECK(slow.remainingTicks == 0 && slow.cooldownTicks == 210);

    effects::SlowState anotherEnemy;
    CHECK(effects::onOrdinaryHit(anotherEnemy, true));
    CHECK(anotherEnemy.remainingTicks == 90 && anotherEnemy.cooldownTicks == 300);
}

void anchorRequiresTwoDamagingHitsOnSameTarget() {
    effects::AnchorState anchor;
    CHECK(!effects::onOrdinaryHit(anchor, false, 7, 100));
    CHECK(!effects::onOrdinaryHit(anchor, true, 7, 0));
    CHECK(!effects::onOrdinaryHit(anchor, true, 7, 100, true));
    CHECK(anchor.targetId == 0 && anchor.windowTicks == 0 && anchor.cooldownTicks == 0);

    CHECK(!effects::onOrdinaryHit(anchor, true, 7, 100));
    CHECK(anchor.targetId == 7 && anchor.windowTicks == 144 && anchor.cooldownTicks == 480);
    CHECK(!effects::onOrdinaryHit(anchor, true, 9, 100));
    CHECK(anchor.targetId == 7 && anchor.windowTicks == 144 && anchor.cooldownTicks == 480);
    effects::tick(anchor, false);
    CHECK(anchor.targetId == 7 && anchor.windowTicks == 144 && anchor.cooldownTicks == 480);
    for (int i = 0; i < 143; ++i) effects::tick(anchor, true);
    CHECK(anchor.targetId == 7 && anchor.windowTicks == 1 && anchor.cooldownTicks == 337);
    CHECK(effects::onOrdinaryHit(anchor, true, 7, 1));
    CHECK(anchor.targetId == 0 && anchor.windowTicks == 0 && anchor.cooldownTicks == 337);
    CHECK(!effects::onOrdinaryHit(anchor, true, 7, 100));
    CHECK(anchor.targetId == 0 && anchor.windowTicks == 0 && anchor.cooldownTicks == 337);

    effects::AnchorState missed;
    CHECK(!effects::onOrdinaryHit(missed, true, 21, 1));
    for (int i = 0; i < 144; ++i) effects::tick(missed, true);
    CHECK(missed.targetId == 0 && missed.windowTicks == 0 && missed.cooldownTicks == 336);
    CHECK(!effects::onOrdinaryHit(missed, true, 22, 1));
    CHECK(missed.targetId == 0 && missed.cooldownTicks == 336); // An expired mark does not refund CD.

    CHECK(effects::damage(100, false) == 100);
    CHECK(effects::damage(101, true) == 51);
    CHECK(effects::damage(100, true) == 50);
    CHECK(effects::damage(0, true) == 0);
    CHECK(effects::damage(-9, false) == 0);
}

void runResetClearsCatalogAndEffectState() {
    catalog::State catalogState;
    const auto offer = catalog::prepare(catalogState, 900);
    reward::RewardState legacy;
    CHECK(offer.count == 3 && reward::queue(legacy, 900, true));
    CHECK(catalog::accept(catalogState, legacy, foundation::Phase::PendingRewards, true, 0, 0));
    catalogState = {};
    CHECK(catalog::ownedCount(catalogState) == 0 && !catalogState.pending &&
          catalogState.lastPreparedBossSource == 0);

    effects::SlowState slow;
    CHECK(effects::onOrdinaryHit(slow, true));
    slow = {};
    CHECK(slow.remainingTicks == 0 && slow.cooldownTicks == 0 && effects::factor(slow, false) == 1.0f);

    effects::AnchorState anchor;
    CHECK(!effects::onOrdinaryHit(anchor, true, 99, 1));
    anchor = {};
    CHECK(anchor.targetId == 0 && anchor.windowTicks == 0 && anchor.cooldownTicks == 0);
}

} // namespace

int main() {
    categoryOfferIsSeededDistinctAndIdempotent();
    categoryFallbackAndFourthGiftKeepRemainingChoices();
    acceptanceValidatesAtomicallyAndNeverRerolls();
    slipknotTimersFreezeAndOnlySlowMovement();
    anchorRequiresTwoDamagingHitsOnSameTarget();
    runResetClearsCatalogAndEffectState();
    if (failures != 0) {
        std::cerr << failures << " M7 signature check(s) failed across " << checks << " checks\n";
        return 1;
    }
    std::cout << "M7 signature checks passed: " << checks << " checks\n";
    return 0;
}
