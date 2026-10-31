#include "campaign_director.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace threadlight;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed: " #condition " at " << __FILE__ << ':' << __LINE__ << '\n'; \
    std::abort(); } } while (false)

namespace {
using KindCounts = std::array<int, 6>;
int kindIndex(combat::EnemyKind kind) { return static_cast<int>(kind); }

KindCounts counts(const chapter::Schedule& value) {
    KindCounts result{};
    for (int i = 0; i < value.count; ++i) ++result[static_cast<std::size_t>(kindIndex(value.items[i].kind))];
    return result;
}
int sparkTotal(const chapter::Schedule& value) {
    int sum = 0;
    for (int i = 0; i < value.count; ++i) sum += combat::definition(value.items[i].kind, 5).sparkValue;
    return sum;
}
int scoreTotal(const chapter::Schedule& value) {
    int sum = 0;
    for (int i = 0; i < value.count; ++i) sum += combat::definition(value.items[i].kind, 5).killScore;
    return sum;
}
int weightedThreat(const chapter::Schedule& value) {
    int sum = 0;
    for (int i = 0; i < value.count; ++i) {
        const auto stats = combat::definition(value.items[i].kind, 5);
        sum += stats.contactDamage + 2 * stats.killScore + 3 * stats.sparkValue;
    }
    return sum;
}
int peakWindowThreat(const chapter::Schedule& value, int windowTicks) {
    int peak = 0;
    for (int i = 0; i < value.count; ++i) {
        int load = 0;
        const int start = value.items[i].tick;
        for (int j = 0; j < value.count; ++j) {
            if (value.items[j].tick >= start && value.items[j].tick < start + windowTicks) {
                const auto stats = combat::definition(value.items[j].kind, 5);
                load += stats.contactDamage + 2 * stats.killScore + 3 * stats.sparkValue;
            }
        }
        peak = std::max(peak, load);
    }
    return peak;
}
bool sameArrangement(const chapter::Schedule& a, const chapter::Schedule& b) {
    if (a.count != b.count) return false;
    for (int i = 0; i < a.count; ++i) {
        if (a.items[i].tick != b.items[i].tick || a.items[i].kind != b.items[i].kind ||
            std::abs(a.items[i].angle - b.items[i].angle) > .00001f) return false;
    }
    return true;
}
bool sameCompositionAndReward(const chapter::Schedule& a, const chapter::Schedule& b) {
    if (a.count != b.count) return false;
    for (int i = 0; i < a.count; ++i) {
        if (a.items[i].kind != b.items[i].kind) return false;
    }
    return sparkTotal(a) == sparkTotal(b) && scoreTotal(a) == scoreTotal(b) &&
           weightedThreat(a) == weightedThreat(b);
}
observation::Summary stationaryStyle() {
    observation::State state;
    for (int i = 0; i < 260; ++i) state.observe({500.f, 400.f}, {500.f, 400.f}, false, false, {0, 0, 1000, 800});
    for (int i = 0; i < 12; ++i) state.onThreatResponse(false);
    return state.summary();
}
observation::Summary mobileStyle() {
    observation::State state;
    foundation::Point p{500.f, 400.f};
    for (int i = 0; i < 260; ++i) {
        foundation::Point next{p.x + 7.f, p.y + (i % 2 ? 2.f : -2.f)};
        state.observe(p, next, i % 3 == 0, i % 3 == 0, {0, 0, 1000, 800});
        p = next;
    }
    for (int i = 0; i < 12; ++i) { state.onThreatResponse(true); state.onCounterResult(i % 2 == 0); }
    return state.summary();
}
void checkInvariants(int wave) {
    const auto base = chapter::schedule(wave, 13579u);
    const auto stationary = campaign::schedule(wave, 13579u, stationaryStyle());
    const auto mobile = campaign::schedule(wave, 13579u, mobileStyle());
    const int deadline = chapter::encounter(wave).seconds * 120 - 240;
    CHECK(base.count > 0 && base.count <= static_cast<int>(base.items.size()));
    CHECK(stationary.count == base.count && mobile.count == base.count);
    CHECK(counts(stationary) == counts(base));
    CHECK(counts(mobile) == counts(base));
    CHECK(sparkTotal(stationary) == sparkTotal(base));
    CHECK(sparkTotal(mobile) == sparkTotal(base));
    CHECK(scoreTotal(stationary) == scoreTotal(base));
    CHECK(scoreTotal(mobile) == scoreTotal(base));
    CHECK(weightedThreat(stationary) == weightedThreat(base));
    CHECK(weightedThreat(mobile) == weightedThreat(base));
    CHECK(peakWindowThreat(stationary, 240) <= peakWindowThreat(base, 240) * 5 / 4);
    CHECK(peakWindowThreat(mobile, 240) <= peakWindowThreat(base, 240) * 5 / 4);
    const auto directed = campaign::direct(wave, 13579u, mobileStyle());
    CHECK(directed.durationSeconds == chapter::encounter(wave).seconds);
    CHECK(directed.timingShiftTicks <= campaign::kMaximumTimingAdjustmentTicks);
    for (const auto* schedule : {&base, &stationary, &mobile}) {
        int prior = -1;
        for (int i = 0; i < schedule->count; ++i) {
            CHECK(schedule->items[i].tick >= 0 && schedule->items[i].tick <= deadline);
            CHECK(schedule->items[i].tick >= prior);
            prior = schedule->items[i].tick;
        }
    }
    for (const auto* directedSchedule : {&stationary, &mobile}) {
        for (int i = 0; i < directedSchedule->count; ++i) {
            int nearest = deadline;
            for (int j = 0; j < base.count; ++j) {
                if (directedSchedule->items[i].kind == base.items[j].kind)
                    nearest = std::min(nearest, std::abs(directedSchedule->items[i].tick - base.items[j].tick));
            }
            CHECK(nearest <= campaign::kMaximumTimingAdjustmentTicks);
        }
    }
    CHECK(!sameArrangement(stationary, mobile));
    const auto alternateSeed = campaign::schedule(wave, 24680u, mobileStyle());
    CHECK(!sameArrangement(mobile, alternateSeed));
    CHECK(campaign::direct(wave, 13579u, mobileStyle()).durationSeconds ==
          campaign::direct(wave, 24680u, mobileStyle()).durationSeconds);
}
void checkLiveReassessment() {
    const auto base = chapter::schedule(campaign::kFirstAdaptiveWave, 8128u);
    auto profile = observation::Summary{};
    profile.confidence = 1.f;
    profile.horizontalBias = 1.f;
    profile.repetition = .9f;
    profile.perimeter = .95f;
    profile.offense = .9f;
    profile.counterSuccess = .8f;
    profile.responseRate = .9f;

    auto first = base;
    auto sameInput = base;
    const int changed = campaign::reassess(first, 3, 400, profile);
    CHECK(changed > 0);
    CHECK(campaign::reassess(sameInput, 3, 400, profile) == changed);
    CHECK(sameArrangement(first, sameInput));
    CHECK(sameCompositionAndReward(first, base));
    CHECK(first.items[first.count - 1].tick == base.items[base.count - 1].tick);
    CHECK(peakWindowThreat(first, 240) <= peakWindowThreat(base, 240) * 5 / 4);
    int retimed = 0;
    int priorTick = -1;
    for (int i = 0; i < base.count; ++i) {
        const bool eligible = i >= 3 && base.items[i].tick > 520;
        if (!eligible) CHECK(std::abs(first.items[i].angle - base.items[i].angle) <= 1.e-5f);
        CHECK(std::abs(std::remainder(first.items[i].angle - base.items[i].angle,
                                     2.f * chapter::kPi)) <= .80001f);
        if (!eligible) CHECK(first.items[i].tick == base.items[i].tick);
        else {
            CHECK(first.items[i].tick > 520);
            CHECK(std::abs(first.items[i].tick - base.items[i].tick) <= 48);
            if (first.items[i].tick != base.items[i].tick) ++retimed;
        }
        CHECK(first.items[i].tick >= priorTick);
        priorTick = first.items[i].tick;
    }
    CHECK(retimed > 0);

    auto changedHeading = profile;
    changedHeading.horizontalBias = 0.f;
    changedHeading.verticalBias = 1.f;
    auto headingResult = base;
    CHECK(campaign::reassess(headingResult, 0, -1000, changedHeading) > 0);
    CHECK(!sameArrangement(first, headingResult));

    auto changedPerimeter = profile;
    changedPerimeter.perimeter = .1f;
    auto perimeterResult = base;
    CHECK(campaign::reassess(perimeterResult, 0, -1000, changedPerimeter) > 0);
    CHECK(!sameArrangement(first, perimeterResult));

    auto changedAggression = profile;
    changedAggression.offense = 0.f;
    changedAggression.counterSuccess = 0.f;
    changedAggression.responseRate = 0.f;
    auto aggressionResult = base;
    CHECK(campaign::reassess(aggressionResult, 0, -1000, changedAggression) > 0);
    CHECK(!sameArrangement(first, aggressionResult));
    CHECK(sameCompositionAndReward(headingResult, base));
    CHECK(sameCompositionAndReward(perimeterResult, base));
    CHECK(sameCompositionAndReward(aggressionResult, base));
    for (const auto* reassessed : {&headingResult, &perimeterResult, &aggressionResult}) {
        CHECK(reassessed->items[reassessed->count - 1].tick == base.items[base.count - 1].tick);
        CHECK(peakWindowThreat(*reassessed, 240) <= peakWindowThreat(base, 240) * 5 / 4);
        for (int i = 1; i < reassessed->count; ++i)
            CHECK(reassessed->items[i].tick >= reassessed->items[i - 1].tick);
    }

    auto committedOnly = base;
    CHECK(campaign::reassess(committedOnly, base.count, 0, profile) == 0);
    CHECK(sameArrangement(committedOnly, base));
}
void checkIntentTargetsAndReviewCadence() {
    auto stationary = observation::Summary{};
    CHECK(campaign::chooseIntent(stationary) == campaign::Intent::Pursuit);

    auto mobileStraight = stationary;
    mobileStraight.movement = .88f;
    mobileStraight.repetition = .18f;
    CHECK(campaign::chooseIntent(mobileStraight) == campaign::Intent::Intercept);

    auto mobileRepeated = mobileStraight;
    mobileRepeated.repetition = .82f;
    CHECK(campaign::chooseIntent(mobileRepeated) == campaign::Intent::Encircle);

    auto perimeterOffense = stationary;
    perimeterOffense.perimeter = .86f;
    perimeterOffense.offense = 3.2f;
    CHECK(campaign::chooseIntent(perimeterOffense) == campaign::Intent::Protect);

    const observation::Bounds bounds{0.f, 0.f, 1000.f, 800.f};
    const foundation::Point player{500.f, 400.f};
    const foundation::Point enemy{250.f, 400.f};
    const foundation::Point velocity{250.f, 0.f};
    const auto pursuit = campaign::tacticalTarget(campaign::Intent::Pursuit, combat::EnemyKind::Hunter,
                                                   enemy, player, velocity, bounds, 1);
    const auto intercept = campaign::tacticalTarget(campaign::Intent::Intercept, combat::EnemyKind::Hunter,
                                                     enemy, player, velocity, bounds, 1);
    CHECK(std::abs(pursuit.x - player.x) < .001f && std::abs(pursuit.y - player.y) < .001f);
    CHECK(intercept.x > player.x + 100.f);
    const auto extremeIntercept = campaign::tacticalTarget(campaign::Intent::Intercept, combat::EnemyKind::Hunter,
                                                            enemy, player, {10000.f, 0.f}, bounds, 1);
    CHECK(extremeIntercept.x - player.x <= 210.001f);

    const auto flankLeft = campaign::tacticalTarget(campaign::Intent::Encircle, combat::EnemyKind::Skitter,
                                                     enemy, player, velocity, bounds, -1);
    const auto flankRight = campaign::tacticalTarget(campaign::Intent::Encircle, combat::EnemyKind::Skitter,
                                                      enemy, player, velocity, bounds, 1);
    CHECK(flankLeft.y < player.y && flankRight.y > player.y);
    CHECK(std::abs(flankLeft.y - flankRight.y) > 150.f);
    const auto wallCutoff = campaign::tacticalTarget(campaign::Intent::Encircle, combat::EnemyKind::Hunter,
                                                      {800.f, 400.f}, {950.f, 400.f}, {350.f, 0.f}, bounds, 1);
    CHECK(wallCutoff.x < bounds.right - 26.f);

    const auto guardPost = campaign::tacticalTarget(campaign::Intent::Protect, combat::EnemyKind::Hunter,
                                                     {800.f, 400.f}, {950.f, 400.f}, {}, bounds, 1);
    CHECK(guardPost.x < 950.f);
    for (const auto target : {pursuit, intercept, extremeIntercept, flankLeft, flankRight, wallCutoff, guardPost}) {
        CHECK(target.x >= 26.f && target.x <= 974.f);
        CHECK(target.y >= 26.f && target.y <= 774.f);
    }

    campaign::ReviewState cadence;
    auto stable = observation::Summary{};
    stable.confidence = 1.f;
    CHECK(!campaign::reviewDue(cadence, 300, stable));
    CHECK(!campaign::reviewDue(cadence, 599, stable));
    CHECK(campaign::reviewDue(cadence, 600, stable));
    CHECK(!campaign::reviewDue(cadence, 899, stable));
    CHECK(campaign::reviewDue(cadence, 1200, stable));

    campaign::ReviewState significant;
    auto changing = stable;
    changing.movement = .5f;
    CHECK(campaign::reviewDue(significant, 300, changing));
    CHECK(!campaign::reviewDue(significant, 599, changing));
    changing.perimeter = .5f;
    CHECK(campaign::reviewDue(significant, 600, changing));

    campaign::ReviewState capped;
    auto profile = stable;
    for (int review = 1; review <= 6; ++review) {
        profile.movement = (review & 1) ? 1.f : 0.f;
        CHECK(campaign::reviewDue(capped, review * 300, profile));
    }
    profile.movement = .5f;
    CHECK(!campaign::reviewDue(capped, 2100, profile));
    CHECK(capped.reviews == 6);
}
}

int main() {
    observation::State state;
    CHECK(state.summary().confidence == 0.f);
    state.observe({100, 100}, {110, 100}, true, true, {0, 0, 800, 600});
    state.onOffense(combat::EnemyKind::Brute);
    state.onIncoming(100, true);
    state.onCounterResult(true);
    state.onThreatResponse(true);
    CHECK(state.movementCount() == 1);
    CHECK(state.summary().confidence > 0.f);
    CHECK(state.summary().counterSuccess > .99f);
    CHECK(state.summary().responseRate > .99f);
    for (int i = 0; i < 100; ++i) state.observe({0, 0}, {1, 0}, false, false, {0, 0, 100, 100});
    CHECK(state.movementCount() == observation::State::kMovementCapacity);
    state.reset();
    CHECK(state.movementCount() == 0);
    CHECK(state.summary().confidence == 0.f);
    CHECK(state.summary().offense == 0.f);

    for (int wave = campaign::kFirstAdaptiveWave; wave <= campaign::kLastAdaptiveWave; ++wave) checkInvariants(wave);
    checkLiveReassessment();
    checkIntentTargetsAndReviewCadence();
    const auto outside = campaign::schedule(27, 1u, mobileStyle());
    CHECK(sameArrangement(outside, chapter::schedule(27, 1u)));
    std::cout << "adaptive director invariants passed\n";
}
