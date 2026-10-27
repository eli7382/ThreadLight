#include "chapter_boss.h"
#include "afterimage.h"
#include "game_logic.h"
#include "game_layout.h"
#include "player_upgrades.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <string>
#include <utility>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* expression, int line) {
    ++checks;
    if (condition) return;
    std::cerr << "M6 boss check failed at line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

namespace boss = threadlight::chapter_boss;
namespace field = threadlight::area_hazards;
namespace combat = threadlight::combat;
namespace upgrades = threadlight::upgrades;
namespace echo = threadlight::afterimage;
using Point = threadlight::foundation::Point;
using Arena = boss::Arena;

constexpr Arena kArena{0.0f, 924.0f, 0.0f, 508.0f};
constexpr Point kCenter{462.0f, 254.0f};
constexpr float kPlayerRadius = threadlight::layout::kPlayerRadius;
constexpr std::uint64_t kPlayerContactId = 0xB0550002ull;

float distance(Point a, Point b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

Point add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point scale(Point a, float amount) { return {a.x * amount, a.y * amount}; }

struct StepResult {
    boss::Events events{};
    combat::IncomingHit fieldHit{};
    bool poolActiveAfterBoss = false;
};

// Match integration order: update boss AI, then tick its field pool. The pool
// sees the same player sweep, so newly reserved warnings skip their creation tick.
StepResult tickBossAndFields(boss::State& state, Point previousPlayer,
                             Point player, Arena arena = kArena,
                             float externalArea = 0.0f) {
    StepResult result;
    result.events = boss::tick(state, player, arena, externalArea);
    result.poolActiveAfterBoss = state.fields.count(field::SlotState::Active) > 0;
    result.fieldHit = state.fields.tick(previousPlayer, player, kPlayerRadius);
    return result;
}

void tickN(boss::State& state, int ticks, Point player, Arena arena = kArena,
           float externalArea = 0.0f) {
    for (int i = 0; i < ticks; ++i)
        (void)tickBossAndFields(state, player, player, arena, externalArea);
}

void finishFieldPattern(boss::State& state, int activeTicks, Point player,
                        Arena arena = kArena) {
    tickN(state, activeTicks, player, arena);
    // The AI observes the pool expiry on its next update because the pool is
    // authoritative after boss AI in the combat-step order.
    if (state.stage == boss::Stage::RootActive && state.remaining == 1 &&
        state.fields.liveCount() == 0)
        (void)tickBossAndFields(state, player, player, arena);
}

bool near(float a, float b, float epsilon = 0.02f) {
    return std::abs(a - b) <= epsilon;
}

int rankTotal(const upgrades::Ranks& ranks) {
    int total = 0;
    for (int value : ranks.value) total += value;
    return total;
}

void setRank(upgrades::Ranks& ranks, upgrades::Id id, int value) {
    ranks.value[static_cast<std::size_t>(upgrades::index(id))] = value;
}

void phaseThresholdsAndFiniteSupportBudget() {
    boss::State hollow;
    boss::start(hollow, kCenter, kArena, boss::Kind::Hollow);
    const auto owner = hollow.hazardOwner;
    CHECK(owner != 0 && owner == 0x7000000000000001ull);
    CHECK(hollow.health.hp == 28500 && hollow.health.maxHp == 28500);

    auto result = boss::applySignature(hollow, 9975); // Exactly 65% HP.
    CHECK(hollow.health.hp == 18525 && hollow.phase == 1 && !result.events.phaseChanged);
    result = boss::applySignature(hollow, 1); // Strictly below 65%.
    CHECK(hollow.health.hp == 18524 && hollow.phase == 2 && result.events.phaseChanged);
    CHECK(result.events.supports == 2 && hollow.stage == boss::Stage::Transition &&
          hollow.remaining == 120 && hollow.hazardOwner == owner);
    result = boss::applySignature(hollow, 9974); // Exactly 30% HP.
    CHECK(hollow.health.hp == 8550 && hollow.phase == 2 && !result.events.phaseChanged);
    result = boss::applySignature(hollow, 1); // Strictly below 30%.
    CHECK(hollow.health.hp == 8549 && hollow.phase == 3 && result.events.phaseChanged);
    CHECK(result.events.supports == 2 && hollow.supportsReleased == 4 &&
          hollow.hazardOwner == owner);

    boss::State heart;
    boss::start(heart, kCenter, kArena, boss::Kind::Heart);
    CHECK(heart.health.hp == 36000 && heart.health.maxHp == 36000);
    result = boss::applySignature(heart, 9000); // Exactly 75% HP.
    CHECK(heart.health.hp == 27000 && heart.phase == 1 && !result.events.phaseChanged);
    result = boss::applySignature(heart, 1);
    CHECK(heart.phase == 2 && result.events.phaseChanged && result.events.supports == 2);
    result = boss::applySignature(heart, 8999); // Exactly 50% HP.
    CHECK(heart.health.hp == 18000 && heart.phase == 2 && !result.events.phaseChanged);
    result = boss::applySignature(heart, 1);
    CHECK(heart.phase == 3 && result.events.phaseChanged && result.events.supports == 2);
    result = boss::applySignature(heart, 8999); // Exactly 25% HP.
    CHECK(heart.health.hp == 9000 && heart.phase == 3 && !result.events.phaseChanged);
    result = boss::applySignature(heart, 1);
    CHECK(heart.health.hp == 8999 && heart.phase == 4 && result.events.phaseChanged);
    CHECK(result.events.supports == 2 && heart.supportsReleased == 6 &&
          heart.remaining == 120 && heart.hazardOwner != 0);

    // A direct phase skip still respects the total support grant budget. The
    // first cycle can use the final two grants, for totals of six and eight.
    boss::State skipHollow;
    boss::start(skipHollow, kCenter, kArena, boss::Kind::Hollow);
    result = boss::applySignature(skipHollow, 20000); // 29.8%: phase 1 -> 3.
    CHECK(result.events.phaseChanged && result.events.supports == 4);
    skipHollow.stage = boss::Stage::Recover;
    skipHollow.remaining = 1;
    result.events = tickBossAndFields(skipHollow, kCenter, kCenter).events;
    CHECK(result.events.supports == 2 && skipHollow.supportsReleased == 6);
    skipHollow.stage = boss::Stage::Recover;
    skipHollow.remaining = 1;
    result.events = tickBossAndFields(skipHollow, kCenter, kCenter).events;
    CHECK(result.events.supports == 0 && skipHollow.supportsReleased == 6);

    boss::State skipHeart;
    boss::start(skipHeart, kCenter, kArena, boss::Kind::Heart);
    result = boss::applySignature(skipHeart, 27001); // 24.99%: cross all three thresholds.
    CHECK(result.events.phaseChanged && result.events.supports == 6 && skipHeart.phase == 4);
    skipHeart.stage = boss::Stage::Recover;
    skipHeart.remaining = 1;
    result.events = tickBossAndFields(skipHeart, kCenter, kCenter).events;
    CHECK(result.events.supports == 2 && skipHeart.supportsReleased == 8);
    skipHeart.stage = boss::Stage::Recover;
    skipHeart.remaining = 1;
    result.events = tickBossAndFields(skipHeart, kCenter, kCenter).events;
    CHECK(result.events.supports == 0 && skipHeart.supportsReleased == 8);

    CHECK(boss::supportCapacity(hollow) == 3 && boss::supportCapacity(heart) == 3);
    for (int ordinal = 0; ordinal < 4; ++ordinal) {
        CHECK(boss::supportKind(hollow, ordinal) == combat::EnemyKind::Hunter);
        CHECK(boss::supportKind(heart, ordinal) == combat::EnemyKind::Hunter);
    }
    CHECK(boss::supportKind(hollow, 4) == combat::EnemyKind::Sower);
    CHECK(boss::supportKind(heart, 4) == combat::EnemyKind::Skitter);
    CHECK(boss::supportKind(heart, 5) == combat::EnemyKind::Skitter);
    CHECK(boss::supportKind(heart, 6) == combat::EnemyKind::Spitter);
    CHECK(boss::supportKind(heart, 7) == combat::EnemyKind::Spitter);
    CHECK(boss::supportSlots(heart) == 2); // Transition can queue at most two.
}

void phaseAndDeathClearCommittedDanger() {
    boss::State phaseChange;
    boss::start(phaseChange, kCenter, kArena, boss::Kind::Hollow);
    const Point player{kCenter.x + 250.0f, kCenter.y};
    tickN(phaseChange, 90, player);
    CHECK(phaseChange.fields.liveCount() > 0 && phaseChange.fields.count(field::SlotState::Warning) > 0);
    const std::uint64_t stableOwner = phaseChange.hazardOwner;
    const auto changed = boss::applySignature(phaseChange, 9976);
    CHECK(changed.events.phaseChanged && phaseChange.fields.occupiedSlotCount() == 0 &&
          phaseChange.fields.liveCount() == 0 && phaseChange.hazardOwner == stableOwner);

    boss::State death;
    boss::start(death, kCenter, kArena, boss::Kind::Heart);
    tickN(death, 90, player);
    CHECK(death.fields.liveCount() > 0);
    const auto defeated = boss::applySignature(death, death.health.hp);
    CHECK(defeated.events.defeated && !boss::alive(death));
    CHECK(death.fields.occupiedSlotCount() == 0 && death.fields.liveCount() == 0);

    boss::State explicitCancel;
    boss::start(explicitCancel, kCenter, kArena, boss::Kind::Hollow);
    tickN(explicitCancel, 90, player);
    CHECK(explicitCancel.fields.liveCount() > 0);
    boss::cancel(explicitCancel);
    CHECK(!boss::alive(explicitCancel) && explicitCancel.fields.occupiedSlotCount() == 0);
}

void hollowWarningGeometryAndFullActiveLifetime() {
    boss::State state;
    boss::start(state, kCenter, kArena, boss::Kind::Hollow);
    const Point player{kCenter.x + 260.0f, kCenter.y};
    CHECK(state.hazardOwner != 0);
    for (int i = 0; i < 89; ++i) (void)tickBossAndFields(state, player, player);
    CHECK(state.stage == boss::Stage::Stalk && state.remaining == 1);
    const StepResult warning = tickBossAndFields(state, player, player);
    CHECK(warning.events.warning && state.stage == boss::Stage::RootWarning);
    CHECK(state.spatialPattern == boss::Pattern::Fold && state.remaining == 60);
    CHECK(state.fields.count(field::SlotState::Warning) == 2);
    for (const auto& patch : state.fields.items()) {
        if (patch.state != field::SlotState::Warning) continue;
        CHECK(patch.spec.ownerId == state.hazardOwner);
        CHECK(patch.warningRemaining == 60 && patch.fresh == false);
        CHECK(patch.spec.warningTicks == 60 && patch.spec.activeTicks == 360);
    }

    for (int i = 0; i < 59; ++i) {
        const StepResult step = tickBossAndFields(state, player, player);
        CHECK(step.fieldHit.damage == 0);
    }
    CHECK(state.stage == boss::Stage::RootWarning && state.remaining == 1);
    for (const auto& patch : state.fields.items())
        if (patch.state == field::SlotState::Warning) CHECK(patch.warningRemaining == 1);
    const StepResult activation = tickBossAndFields(state, player, player);
    CHECK(activation.fieldHit.damage == 0 && state.stage == boss::Stage::RootActive);
    CHECK(state.activePatternTicks == 360 && state.fields.count(field::SlotState::Active) == 2);
    for (const auto& patch : state.fields.items())
        if (patch.state == field::SlotState::Active) CHECK(patch.activeRemaining == 360);

    int poolActiveUpdates = 0;
    for (int i = 0; i < 360; ++i) {
        const StepResult step = tickBossAndFields(state, player, player);
        poolActiveUpdates += step.poolActiveAfterBoss ? 1 : 0;
    }
    finishFieldPattern(state, 0, player);
    CHECK(state.stage == boss::Stage::Recover && state.remaining == 240);
    CHECK(poolActiveUpdates == 360); // Regression: the last post-AI pool tick must run.
    CHECK(state.fields.occupiedSlotCount() == 0);
}

void heartPulseMinimumAreaWarningAndActiveLifetime() {
    const Arena minimum{0.0f, 924.0f, 0.0f, 508.0f};
    boss::State state;
    boss::start(state, kCenter, minimum, boss::Kind::Heart);
    const Point player{kCenter.x + 260.0f, kCenter.y};
    for (int i = 0; i < 89; ++i) (void)tickBossAndFields(state, player, player, minimum);
    const StepResult warning = tickBossAndFields(state, player, player, minimum);
    CHECK(warning.events.warning && state.spatialPattern == boss::Pattern::Pulse);
    CHECK(state.stage == boss::Stage::RootWarning && state.remaining == 57);
    CHECK(state.fields.count(field::SlotState::Warning) == 1);
    const auto& patch = state.fields.items()[0];
    CHECK(patch.spec.shape == field::Shape::Annulus);
    CHECK(patch.spec.radiusStart == 50.0f && patch.spec.radiusEnd == 300.0f);
    CHECK(patch.spec.warningTicks == 57 && patch.spec.activeTicks == 120 &&
          patch.spec.damage == 2800 && patch.spec.gapHalfAngleRadians == 0.8f);
    CHECK(state.fields.areaOccupied({minimum.left, minimum.right, minimum.top, minimum.bottom}) <
          (minimum.right - minimum.left) * (minimum.bottom - minimum.top) * field::kMaxOccupiedFraction);

    for (int i = 0; i < 56; ++i) {
        const StepResult step = tickBossAndFields(state, player, player, minimum);
        CHECK(step.fieldHit.damage == 0);
    }
    CHECK(state.stage == boss::Stage::RootWarning && state.remaining == 1);
    const auto activation = tickBossAndFields(state, player, player, minimum);
    CHECK(activation.fieldHit.damage == 0 && state.stage == boss::Stage::RootActive);
    CHECK(state.fields.count(field::SlotState::Active) == 1 &&
          state.fields.items()[0].activeRemaining == 120);

    int poolActiveUpdates = 0;
    for (int i = 0; i < 120; ++i) {
        const auto step = tickBossAndFields(state, player, player, minimum);
        poolActiveUpdates += step.poolActiveAfterBoss ? 1 : 0;
    }
    finishFieldPattern(state, 0, player, minimum);
    CHECK(state.stage == boss::Stage::Recover && state.remaining == 240);
    CHECK(poolActiveUpdates == 120);
    CHECK(state.fields.occupiedSlotCount() == 0);
}

void finalHeartSequenceAndPrefinalForkPasses() {
    boss::State final;
    boss::start(final, kCenter, kArena, boss::Kind::Heart);
    const auto phase = boss::applySignature(final, 27001);
    CHECK(phase.events.phaseChanged && final.phase == 4 && phase.events.supports == 6);
    tickN(final, 120, {kCenter.x + 250.0f, kCenter.y});
    CHECK(final.stage == boss::Stage::Stalk && final.remaining == 90);
    tickN(final, 89, {kCenter.x + 250.0f, kCenter.y});
    const auto laneStart = tickBossAndFields(final, {kCenter.x + 250.0f, kCenter.y},
                                             {kCenter.x + 250.0f, kCenter.y});
    CHECK(laneStart.events.warning && final.spatialPattern == boss::Pattern::Lane);
    CHECK(final.sequenceBeat == 0 && final.stage == boss::Stage::RootWarning &&
          final.remaining == 57 && final.fields.count(field::SlotState::Warning) == 2);

    tickN(final, 57, {kCenter.x + 250.0f, kCenter.y});
    CHECK(final.stage == boss::Stage::RootActive && final.activePatternTicks == 90);
    finishFieldPattern(final, 90, {kCenter.x + 250.0f, kCenter.y});
    CHECK(final.spatialPattern == boss::Pattern::Lunge && final.sequenceBeat == 1);
    CHECK(final.stage == boss::Stage::StepWarning && final.remaining == 51 &&
          final.strikeTicks == 42 && final.fields.liveCount() == 0);
    const Point lockedEnd = final.stepEnd;
    tickN(final, 51, {kCenter.x, kCenter.y + 220.0f});
    CHECK(final.stage == boss::Stage::StepActive && final.remaining == 42);
    for (int i = 0; i < 42; ++i)
        (void)tickBossAndFields(final, {kCenter.x, kCenter.y - 200.0f},
                                {kCenter.x, kCenter.y - 200.0f});
    CHECK(final.spatialPattern == boss::Pattern::Pulse && final.sequenceBeat == 2);
    CHECK(final.stage == boss::Stage::RootWarning && final.remaining == 57 &&
          final.fields.count(field::SlotState::Warning) == 1);
    CHECK(near(final.stepEnd.x, lockedEnd.x) && near(final.stepEnd.y, lockedEnd.y));

    tickN(final, 57, {kCenter.x, kCenter.y - 200.0f});
    CHECK(final.stage == boss::Stage::RootActive && final.activePatternTicks == 120);
    finishFieldPattern(final, 120, {kCenter.x, kCenter.y - 200.0f});
    CHECK(final.spatialPattern == boss::Pattern::Fork && final.sequenceBeat == 3);
    CHECK(final.stage == boss::Stage::RootWarning && final.remaining == 57 &&
          final.fields.count(field::SlotState::Warning) == 2);
    tickN(final, 57, {kCenter.x, kCenter.y - 200.0f});
    CHECK(final.stage == boss::Stage::RootActive && final.activePatternTicks == 90);
    finishFieldPattern(final, 90, {kCenter.x, kCenter.y - 200.0f});
    CHECK(final.stage == boss::Stage::Recover && final.remaining == 360);
    CHECK(final.sequenceBeat == 3 && final.fields.occupiedSlotCount() == 0);

    boss::State prefinal;
    boss::start(prefinal, kCenter, kArena, boss::Kind::Heart);
    prefinal.metrics.cycles = 2; // Isolated cycle-index fixture for the Fork branch.
    prefinal.remaining = 1;
    const auto firstFork = tickBossAndFields(prefinal, {kCenter.x + 250.0f, kCenter.y},
                                             {kCenter.x + 250.0f, kCenter.y});
    CHECK(firstFork.events.warning && prefinal.spatialPattern == boss::Pattern::Fork &&
          prefinal.spatialPass == 0 && prefinal.remaining == 57);
    tickN(prefinal, 57, {kCenter.x + 250.0f, kCenter.y});
    finishFieldPattern(prefinal, 90, {kCenter.x + 250.0f, kCenter.y});
    CHECK(prefinal.spatialPattern == boss::Pattern::Fork && prefinal.spatialPass == 1 &&
          prefinal.stage == boss::Stage::RootWarning && prefinal.remaining == 57);
    tickN(prefinal, 57, {kCenter.x + 250.0f, kCenter.y});
    finishFieldPattern(prefinal, 90, {kCenter.x + 250.0f, kCenter.y});
    CHECK(prefinal.stage == boss::Stage::Recover && prefinal.remaining == 240 &&
          prefinal.fields.occupiedSlotCount() == 0);
}

void hollowSpokesFallbackKeepsWideOpposedGapCorridor() {
    constexpr float externalArea = 63000.0f;
    boss::State state;
    boss::start(state, kCenter, kArena, boss::Kind::Hollow);
    state.phase = 3;
    state.metrics.cycles = 1; // Isolated phase/cycle fixture selects Spokes.
    state.remaining = 1;
    // Model the other hazard pool's occupied area. The four spokes exceed the
    // remaining combined-area limit, while one gapped ring still fits.
    const Point player{kCenter.x + 250.0f, kCenter.y};
    const auto warning = tickBossAndFields(state, player, player, kArena, externalArea);
    CHECK(warning.events.warning);
    CHECK(state.spatialPattern == boss::Pattern::Ring);
    CHECK(state.stage == boss::Stage::RootWarning && state.remaining == 60);
    CHECK(state.corridor.enabled && near(state.corridor.halfWidth, 68.0f));
    CHECK(distance(state.corridor.a, state.corridor.b) >= 920.0f);
    CHECK(state.fields.count(field::SlotState::Warning) == 1);
    const auto& ring = state.fields.items()[0].spec;
    CHECK(ring.shape == field::Shape::Annulus && ring.radiusStart == 119.0f &&
          ring.radiusEnd == 119.0f && ring.gapHalfAngleRadians == 0.9f);
    CHECK(distance(ring.gapDirection, {1.0f, 0.0f}) < 0.001f);
    const float arenaArea = (kArena.right - kArena.left) * (kArena.bottom - kArena.top);
    CHECK(state.fields.areaOccupied({kArena.left, kArena.right, kArena.top, kArena.bottom}) +
          externalArea < arenaArea * field::kMaxOccupiedFraction);
    for (int i = 0; i < 60; ++i)
        (void)tickBossAndFields(state, player, player, kArena, externalArea);
    CHECK(state.stage == boss::Stage::RootActive);
    const auto safeCrossing = state.fields.tick({0.0f, kCenter.y}, {kArena.right, kCenter.y}, 12.0f);
    CHECK(safeCrossing.damage == 0);

    boss::State edge;
    boss::start(edge, {74.0f, kCenter.y}, kArena, boss::Kind::Hollow);
    edge.phase = 3;
    edge.metrics.cycles = 1;
    edge.remaining = 1;
    const Point edgePlayer{edge.p.x + 250.0f, edge.p.y};
    const auto edgeWarning = tickBossAndFields(edge, edgePlayer, edgePlayer);
    CHECK(edgeWarning.events.warning && edge.spatialPattern == boss::Pattern::Spokes);
    CHECK(edge.fields.count(field::SlotState::Warning) == 4);
    for (const auto& patch : edge.fields.items()) {
        if (patch.state != field::SlotState::Warning) continue;
        const float halfWidth = patch.spec.halfWidth;
        CHECK(patch.spec.a.x - halfWidth >= kArena.left - 0.01f &&
              patch.spec.a.x + halfWidth <= kArena.right + 0.01f &&
              patch.spec.a.y - halfWidth >= kArena.top - 0.01f &&
              patch.spec.a.y + halfWidth <= kArena.bottom + 0.01f);
        CHECK(patch.spec.b.x - halfWidth >= kArena.left - 0.01f &&
              patch.spec.b.x + halfWidth <= kArena.right + 0.01f &&
              patch.spec.b.y - halfWidth >= kArena.top - 0.01f &&
              patch.spec.b.y + halfWidth <= kArena.bottom + 0.01f);
    }
}

void heartLungeRemapKeepsAimAndSixHundredSpeedCap() {
    const Arena wide{0.0f, 1848.0f, 0.0f, 1016.0f};
    boss::State state;
    boss::start(state, kCenter, kArena, boss::Kind::Heart);
    state.metrics.cycles = 1; // Isolated Stalk branch fixture selects Lunge.
    state.remaining = 1;
    const auto warning = tickBossAndFields(state, {kCenter.x + 400.0f, kCenter.y},
                                           {kCenter.x + 400.0f, kCenter.y});
    CHECK(warning.events.warning && state.spatialPattern == boss::Pattern::Lunge);
    CHECK(state.stage == boss::Stage::StepWarning && state.remaining == 51 &&
          state.strikeTicks == 42);
    const Point originalStart = state.stepStart, originalEnd = state.stepEnd;
    tickN(state, 51, {kCenter.x, kCenter.y + 350.0f});
    CHECK(state.stage == boss::Stage::StepActive && state.remaining == 42);
    for (int i = 0; i < 12; ++i)
        (void)tickBossAndFields(state, {kCenter.x, kCenter.y - 300.0f},
                                {kCenter.x, kCenter.y - 300.0f});
    const int previousRemaining = state.remaining;
    const Point mappedStart{originalStart.x * 2.0f, originalStart.y * 2.0f};
    const Point mappedEnd{originalEnd.x * 2.0f, originalEnd.y * 2.0f};
    boss::remap(state, kArena, wide);
    CHECK(state.stage == boss::Stage::StepActive && state.remaining > 0);
    CHECK(near(state.stepStart.x, mappedStart.x) && near(state.stepStart.y, mappedStart.y));
    CHECK(near(state.stepEnd.x, mappedEnd.x) && near(state.stepEnd.y, mappedEnd.y));
    CHECK(state.remaining > 0 && previousRemaining > 0);
    CHECK(distance(state.stepStart, state.stepEnd) * 120.0f / state.strikeTicks <= 600.1f);
    const Point fixedEnd = state.stepEnd;
    for (int i = 0; i < 8 && state.stage == boss::Stage::StepActive; ++i) {
        const Point previous = state.p;
        (void)tickBossAndFields(state, {wide.right - 20.0f, wide.top + 20.0f},
                                {wide.right - 20.0f, wide.top + 20.0f}, wide);
        CHECK(distance(previous, state.p) <= 5.01f);
        CHECK(near(state.stepEnd.x, fixedEnd.x) && near(state.stepEnd.y, fixedEnd.y));
    }

    // A committed Hollow relocation remaps its path and stays inside the new
    // minimum body inset for every later active update.
    const Arena minimum{0.0f, 150.0f, 0.0f, 160.0f};
    boss::State hollow;
    boss::start(hollow, {850.0f, 400.0f}, kArena, boss::Kind::Hollow);
    hollow.stage = boss::Stage::RootActive;
    hollow.remaining = 180;
    hollow.activePatternTicks = 360;
    hollow.spatialPattern = boss::Pattern::Ring;
    hollow.patternOrigin = {850.0f, 400.0f};
    hollow.relocateEnd = {850.0f, 265.0f};
    hollow.p = hollow.previousP = boss::interpolate(
        hollow.patternOrigin, hollow.relocateEnd, 0.5f);
    boss::remap(hollow, kArena, minimum);
    for (int i = 0; i < 180; ++i) {
        (void)tickBossAndFields(hollow, {75.0f, 80.0f}, {75.0f, 80.0f}, minimum);
        CHECK(hollow.p.x >= 74.0f && hollow.p.x <= 76.0f);
        CHECK(hollow.p.y >= 74.0f && hollow.p.y <= 86.0f);
    }
}

enum class Build { Baseline, Offense, NoRazorMobility, Mixed };

const char* buildName(Build build) {
    switch (build) {
    case Build::Baseline: return "baseline";
    case Build::Offense: return "offense";
    case Build::NoRazorMobility: return "no_razor_mobility";
    case Build::Mixed: return "mixed";
    }
    return "unknown";
}

upgrades::Ranks makeBuild(Build build, boss::Kind kind) {
    upgrades::Ranks ranks{};
    if (build == Build::Offense) {
        setRank(ranks, upgrades::Id::Razor, 3);
        setRank(ranks, upgrades::Id::Living, 2);
        setRank(ranks, upgrades::Id::Broad, 2);
        setRank(ranks, upgrades::Id::Quick, 2);
        setRank(ranks, upgrades::Id::Rhythm, 1);
        setRank(ranks, upgrades::Id::Heartwood, 2);
        if (kind == boss::Kind::Heart) {
            setRank(ranks, upgrades::Id::Quick, 4);
            setRank(ranks, upgrades::Id::Rhythm, 2);
            setRank(ranks, upgrades::Id::Heartwood, 3);
        }
    } else if (build == Build::NoRazorMobility) {
        setRank(ranks, upgrades::Id::Living, 3);
        setRank(ranks, upgrades::Id::Broad, 2);
        setRank(ranks, upgrades::Id::Quick, 5);
        setRank(ranks, upgrades::Id::Rhythm, 3);
        setRank(ranks, upgrades::Id::Heartwood, 3);
    } else if (build == Build::Mixed) {
        setRank(ranks, upgrades::Id::Razor, 3);
        setRank(ranks, upgrades::Id::Living, 3);
        setRank(ranks, upgrades::Id::Broad, 2);
        setRank(ranks, upgrades::Id::Quick, 5);
        setRank(ranks, upgrades::Id::Rhythm, 3);
        setRank(ranks, upgrades::Id::Heartwood, 5);
        setRank(ranks, upgrades::Id::Magnet, 1);
    }
    return ranks;
}

struct SilkPoint { Point p{}; int ageTicks = 0; };

struct BotResult {
    boss::Kind kind{};
    Build build{};
    int rankPoints = 0;
    int combatTicks = 0;
    int startPlayerHp = 0, playerHp = 0, bossHp = 0;
    int playerDamage = 0;
    bool playerAlive = true, bossAlive = true;
    std::array<int, 7> patternStarts{};
    std::uint64_t warnings = 0, supportsReleased = 0;
    threadlight::rootwarden::Metrics metrics{};
};

BotResult runBot(boss::Kind kind, Build build, bool readWarnings=false) {
    const upgrades::Ranks ranks = makeBuild(build, kind);
    const upgrades::Stats stats = upgrades::derive(ranks);
    const int rankPoints = rankTotal(ranks);
    const Arena arena = kArena;
    const Point center = kCenter;
    boss::State state;
    boss::start(state, center, arena, kind);
    combat::PlayerHealth health{stats.maxHp, stats.maxHp, 0};
    echo::State afterimage;
    std::deque<SilkPoint> thread;
    BotResult result;
    result.kind = kind;
    result.build = build;
    result.rankPoints = rankPoints;
    result.startPlayerHp = health.hp;
    constexpr int kTicksPerSecond = 120;
    const int tickLimit = (kind == boss::Kind::Hollow ? 240 : 360) * kTicksPerSecond;
    const int dashDurationTicks = static_cast<int>(std::ceil(
        threadlight::kDashDurationSeconds * kTicksPerSecond));
    int dashCooldownTicks = 0, dashTicks = 0;
    float angle = 0.0f;
    Point player{center.x + 155.0f, center.y}, previousPlayer = player;
    const int silkLifetime = static_cast<int>(std::ceil(stats.activeSilkSeconds * kTicksPerSecond));

    for (int tick = 0; tick < tickLimit && boss::alive(state) && health.alive(); ++tick) {
        combat::tick(health); // Player immunity decays once every combat update.
        if (dashCooldownTicks > 0) --dashCooldownTicks;
        if (dashCooldownTicks == 0 && dashTicks == 0) {
            echo::beginDash(afterimage, player, true, true);
            dashTicks = dashDurationTicks;
            dashCooldownTicks = static_cast<int>(std::ceil(stats.dashCooldownSeconds * kTicksPerSecond));
        }
        const bool dashActive = dashTicks > 0;
        angle += stats.moveSpeed / 225.0f / static_cast<float>(kTicksPerSecond);
        const float desiredRadius = boss::exposed(state) ? boss::vulnerableRadius(state) - 3.0f : 155.0f;
        Point goal = boss::clamp({state.p.x + desiredRadius * std::cos(angle),
                                        state.p.y + desiredRadius * std::sin(angle)}, arena,
                                       kPlayerRadius);
        // Observable committed warning geometry only; no prediction of future aims.
        if(readWarnings&&!boss::exposed(state)&&state.stage!=boss::Stage::Transition){
            Point axis=state.patternAxis;
            if(kind==boss::Kind::Hollow&&state.corridor.enabled){
                if((player.x-state.p.x)*axis.x+(player.y-state.p.y)*axis.y<0)axis=scale(axis,-1.f);
                goal=add(state.p,scale(axis,boss::vulnerableRadius(state)-3.f));
            }else if(kind==boss::Kind::Heart){
                if(state.stage==boss::Stage::StepWarning||state.stage==boss::Stage::StepActive){
                    const Point perpendicular{-axis.y,axis.x};
                    const float sign=(player.x-state.stepEnd.x)*perpendicular.x+(player.y-state.stepEnd.y)*perpendicular.y<0?-1.f:1.f;
                    goal=add(state.stepEnd,scale(perpendicular,110.f*sign));
                }else goal=add(state.p,scale(axis,180.f));
            }
            goal=boss::clamp(goal,arena,kPlayerRadius);
        }
        const Point toward = sub(goal, player);
        const float remaining = distance(goal, player);
        const float speed = stats.moveSpeed * (dashActive ? 3.7f : 1.0f);
        const float step = std::min(remaining, speed / static_cast<float>(kTicksPerSecond));
        previousPlayer = player;
        if (remaining > 1.0e-5f) player = add(player, scale(toward, step / remaining));
        player = boss::clamp(player, arena, kPlayerRadius);
        if (dashActive) {
            echo::appendDash(afterimage, previousPlayer, player, true);
            if (--dashTicks == 0) echo::finishDash(afterimage, true, true);
        }

        if (thread.empty() || distance(thread.back().p, player) > 9.0f)
            thread.push_back({player, 0});
        for (SilkPoint& point : thread) ++point.ageTicks;
        while (!thread.empty() &&
               (thread.front().ageTicks > silkLifetime || thread.size() > 520)) thread.pop_front();

        const boss::Events events = boss::tick(state, player, arena, 0.0f);
        if (events.warning) {
            const int pattern = static_cast<int>(state.spatialPattern);
            if (pattern >= 0 && pattern < static_cast<int>(result.patternStarts.size()))
                ++result.patternStarts[static_cast<std::size_t>(pattern)];
        }

        bool silkTouched = false;
        for (std::size_t i = 1; i < thread.size(); ++i) {
            if (thread[i].ageTicks > silkLifetime ||
                !boss::silkContact(state, thread[i - 1].p, thread[i].p, stats.silkHalfWidth)) continue;
            silkTouched = true;
            (void)boss::applySilk(state, stats.silkDamage);
            break;
        }
        if (silkTouched) ++state.metrics.contactTicks;
        if (boss::alive(state) && echo::claimContact(afterimage, state.hazardOwner,
                                                      state.previousP, state.p,
                                                      boss::vulnerableRadius(state))) {
            (void)boss::applySignature(state, echo::damage(stats.silkDamage, true));
        }
        echo::advance(afterimage, true);

        combat::IncomingHit incoming;
        incoming.consider(boss::incomingDamage(state, previousPlayer, player, kPlayerRadius),
                          kPlayerContactId);
        const combat::IncomingHit fieldHit = state.fields.tick(previousPlayer, player, kPlayerRadius);
        incoming.consider(fieldHit.damage, fieldHit.sourceId);
        const int taken = combat::applyIncoming(health, incoming, dashActive);
        state.metrics.playerDamage += taken;
        result.playerDamage += taken;
        ++result.combatTicks;
    }

    result.playerHp = health.hp;
    result.bossHp = state.health.hp;
    result.playerAlive = health.alive();
    result.bossAlive = boss::alive(state);
    result.warnings = state.warnings;
    result.supportsReleased = state.supportsReleased;
    result.metrics = state.metrics;
    return result;
}

void printBot(const BotResult& result) {
    const char* outcome = !result.bossAlive ? "boss_defeated" :
                          !result.playerAlive ? "player_died" : "time_cap";
    std::cout << "m6_bot boss=" << (result.kind == boss::Kind::Hollow ? "hollow" : "heart")
              << " build=" << buildName(result.build) << " ranks=" << result.rankPoints
              << " outcome=" << outcome << " ticks=" << result.combatTicks
              << " seconds=" << result.combatTicks / 120.0
              << " player_hp=" << result.playerHp << "/" << result.startPlayerHp
              << " player_damage=" << result.playerDamage
              << " boss_hp=" << result.bossHp
              << " ordinary_hits=" << result.metrics.hits
              << " exposed_hits=" << result.metrics.exposedHits
              << " silk_damage=" << result.metrics.silkDamage
              << " echo_hits=" << result.metrics.signatureHits
              << " echo_damage=" << result.metrics.signatureDamage
              << " patterns=" << result.warnings
              << " cycles=" << result.metrics.cycles
              << " supports_released=" << result.supportsReleased
              << " support_bodies=omitted extra_signature=Afterimage_only\n";
}

void helperBotMeasurementsAreObservationsOnly() {
    const std::array<std::pair<boss::Kind, Build>, 6> cases{{
        {boss::Kind::Hollow, Build::Baseline},
        {boss::Kind::Hollow, Build::Offense},
        {boss::Kind::Hollow, Build::NoRazorMobility},
        {boss::Kind::Heart, Build::Baseline},
        {boss::Kind::Heart, Build::Offense},
        {boss::Kind::Heart, Build::Mixed}
    }};
    for (const auto& [kind, build] : cases) {
        const upgrades::Ranks ranks = makeBuild(build, kind);
        const int points = rankTotal(ranks);
        CHECK((build == Build::Baseline && points == 0) ||
              (build == Build::Offense && points == 12 &&
               kind == boss::Kind::Hollow) ||
              (build == Build::Offense && points == 16 &&
               kind == boss::Kind::Heart) ||
              (build == Build::NoRazorMobility && points == 16) ||
              (build == Build::Mixed && points == 22));
        if (build == Build::NoRazorMobility)
            CHECK(upgrades::rank(ranks, upgrades::Id::Razor) == 0);
        const BotResult result = runBot(kind, build);
        std::cout<<"policy=blind_orbit "; printBot(result);
        const BotResult aware=runBot(kind,build,true);
        std::cout<<"policy=committed_warning "; printBot(aware);
        CHECK(result.combatTicks > 0);
        CHECK(result.combatTicks <= (kind == boss::Kind::Hollow ? 240 : 360) * 120);
        CHECK(result.metrics.contactTicks <= static_cast<std::uint64_t>(result.combatTicks));
        CHECK(result.metrics.exposedHits <= result.metrics.hits);
        CHECK(result.playerDamage == result.metrics.playerDamage);
        CHECK(result.bossHp >= 0 && result.bossHp <= boss::maxHp(kind));
        CHECK(result.playerHp >= 0 && result.playerHp <= result.startPlayerHp);
    }
}

} // namespace

int main() {
    phaseThresholdsAndFiniteSupportBudget();
    phaseAndDeathClearCommittedDanger();
    hollowWarningGeometryAndFullActiveLifetime();
    heartPulseMinimumAreaWarningAndActiveLifetime();
    finalHeartSequenceAndPrefinalForkPasses();
    hollowSpokesFallbackKeepsWideOpposedGapCorridor();
    heartLungeRemapKeepsAimAndSixHundredSpeedCap();
    helperBotMeasurementsAreObservationsOnly();

    if (failures != 0) {
        std::cerr << failures << " M6 boss check(s) failed across " << checks << " checks\n";
        return 1;
    }
    std::cout << "M6 boss checks passed: " << checks << " checks\n";
    return 0;
}
