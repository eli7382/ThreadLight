#include "chapter_boss.h"
#include "unweaver.h"
#include <array>
#include <cmath>
#include <iostream>

namespace root = threadlight::chapter_boss;
namespace uw = threadlight::unweaver;
using Point = threadlight::foundation::Point;

#define CHECK(condition) do { if (!(condition)) { \
    ++gFailures; \
    std::cerr << "CHECK failed: " #condition " at " << __FILE__ << ':' << __LINE__ << '\n'; \
    return; } } while (false)

namespace {
int gFailures = 0;
constexpr root::Arena kArena{0.f, 1000.f, 0.f, 600.f};

root::State makeUnweaver(Point position = {462.f, 254.f}, int phase = 2) {
    root::State boss;
    root::start(boss, position, kArena, root::Kind::Unweaver);
    boss.phase = boss.unweaver.progress.phase = phase;
    boss.unweaver.playerBounds = kArena;
    boss.unweaver.playerSpeed = 255.f;
    boss.unweaver.playerDashMultiplier = 3.7f;
    return boss;
}

float distance(Point a, Point b) { return uw::length(uw::sub(a, b)); }

void predictionTracksCircularMotionAndBoundsConfidence() {
    uw::State state;
    state.playerBounds = kArena;
    constexpr Point center{500.f, 300.f};
    constexpr float radius = 200.f;
    constexpr float omega = 1.4f;
    constexpr float dt = 1.f / 120.f;
    for (int i = -24; i <= 0; ++i) {
        const float angle = omega * static_cast<float>(i) * dt;
        uw::record(state, {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
    }
    const Point player{center.x + radius, center.y};
    state.playerVelocity = {0.f, radius * omega};
    constexpr float horizon = .5f;
    const Point actual{center.x + radius * std::cos(omega * horizon),
                       center.y + radius * std::sin(omega * horizon)};
    const Point curved = uw::predict(state, player, horizon, true);
    const Point linear = uw::predict(state, player, horizon, false);
    const float curvedError = distance(curved, actual);
    const float linearError = distance(linear, actual);
    std::cout << "unweaver circular_prediction curved_error=" << curvedError
              << " linear_error=" << linearError << '\n';
    CHECK(state.historyCount == 25);
    CHECK(curvedError < linearError * .35f);

    uw::State confident;
    confident.playerBounds = kArena;
    confident.playerVelocity = {120.f, 0.f};
    const Point origin{500.f, 300.f};
    const Point trusted = uw::predict(confident, origin, .8f);
    confident.predictionError = 600.f;
    const Point cautious = uw::predict(confident, origin, .8f);
    CHECK(distance(origin, cautious) < distance(origin, trusted));
    CHECK(std::abs(distance(origin, trusted) - 96.f) < .01f);
    CHECK(std::abs(distance(origin, cautious) - 57.6f) < .01f);

    uw::State reversing;
    reversing.playerBounds = kArena;
    reversing.historyCount = 25;
    reversing.historyHead = 25;
    reversing.history[0] = {400.f, 300.f};
    reversing.history[12] = {412.f, 300.f};
    reversing.history[24] = {400.f, 300.f};
    reversing.playerVelocity = {-100.f, 0.f};
    const Point afterReversal = uw::predict(reversing, {400.f, 300.f}, .85f);
    CHECK(distance(afterReversal, {400.f, 300.f}) <= 18.01f);
}

void chargeLocksItsTargetAndModelsFlyingShots() {
    auto boss = makeUnweaver({462.f, 254.f}, 1);
    auto& state = boss.unweaver;
    const Point initialPlayer{690.f, 254.f};
    CHECK(uw::charge(boss, initialPlayer, kArena));
    CHECK(!state.chargePreview);
    CHECK(state.chargeAttempts == 1);
    CHECK(boss.stage == root::Stage::StepWarning);
    CHECK(state.chargeEnd.x == boss.stepEnd.x && state.chargeEnd.y == boss.stepEnd.y);
    const Point lockedEnd = state.chargeEnd;
    auto admittedReplay = state;
    admittedReplay.chargePreview = true;
    CHECK(uw::witness(admittedReplay, initialPlayer, 12.f, state.safeRoute, state.safeUsesDash));
    for (int i = 0; i < 66; ++i) root::tick(boss, {700.f, 400.f}, kArena);
    CHECK(boss.stage == root::Stage::StepActive);
    for (int i = 0; i < 42; ++i) root::tick(boss, {700.f, 400.f}, kArena);
    CHECK(std::abs(boss.p.x - lockedEnd.x) < .01f && std::abs(boss.p.y - lockedEnd.y) < .01f);

    auto busy = makeUnweaver({462.f, 254.f}, 1);
    const auto ticket = busy.bullets.reserve(busy.nextSource++);
    CHECK(ticket != 0);
    CHECK(busy.bullets.launch(ticket, {180.f, 120.f}, {1.f, 0.f}, {180.f, 5.f, 500, 360}));
    CHECK(busy.bullets.flyingCount() == 1);
    CHECK(uw::charge(busy, initialPlayer, kArena));
    CHECK(busy.unweaver.ownShotCount == 1);
    CHECK(busy.bullets.flyingCount() == 1);
    CHECK(busy.unweaver.chargeAttempts == 1);
    CHECK(!busy.unweaver.chargePreview);
}

void actionSelectionAdaptsAndPenalizesRepeats() {
    auto farBoss = makeUnweaver();
    auto nearBoss = makeUnweaver();
    for (auto* boss : {&farBoss, &nearBoss}) {
        boss->unweaver.activeTicks = 100;
        boss->unweaver.radialTicks = 30;
        boss->unweaver.profile.repetition = .2f;
    }
    CHECK(uw::selectAction(farBoss, {850.f, 500.f}) == uw::Action::Hunt);
    CHECK(uw::selectAction(nearBoss, {600.f, 254.f}) == uw::Action::Charge);

    auto boss = makeUnweaver();
    auto& state = boss.unweaver;
    const Point farPlayer{850.f, 500.f};
    state.activeTicks = 100;
    state.radialTicks = 30;
    state.recentActions[static_cast<std::size_t>(uw::Action::EventLoom)] = 1.f;
    state.profile.perimeter = 1.f;
    state.recentActions[static_cast<std::size_t>(uw::Action::Hunt)] = .5f;
    CHECK(uw::selectAction(boss, farPlayer) == uw::Action::Flank);

    state.profile.perimeter = 0.f;
    state.profile.dashUse = .25f;
    state.recentActions[static_cast<std::size_t>(uw::Action::Hunt)] = 1.f;
    CHECK(uw::selectAction(boss, farPlayer) == uw::Action::Focused);

    state.profile.dashUse = 0.f;
    state.profile.offense = 5.f;
    CHECK(uw::selectAction(boss, farPlayer) == uw::Action::Bait);

    state.profile.offense = 0.f;
    state.activeTicks = 100;
    state.radialTicks = 50;
    state.recentActions.fill(0.f);
    state.recentActions[static_cast<std::size_t>(uw::Action::Hunt)] = 1.f;
    state.recentActions[static_cast<std::size_t>(uw::Action::Focused)] = .1f;
    CHECK(uw::selectAction(boss, farPlayer) == uw::Action::EventLoom);
    const auto afterFatigue = uw::selectAction(boss, farPlayer);
    CHECK(afterFatigue == uw::Action::Unique);
    CHECK(state.actionChoices[static_cast<std::size_t>(uw::Action::EventLoom)] == 1);
    CHECK(state.actionChoices[static_cast<std::size_t>(afterFatigue)] == 1);
}

void focusedAndUniquePlansDoNotEarnRadialExperience() {
    for (const bool unique : {false, true}) {
        auto boss = makeUnweaver();
        auto& state = boss.unweaver;
        state.unique = unique;
        state.focused = !unique;
        const Point player{690.f, 254.f};
        CHECK(uw::commit(boss, player, kArena));
        CHECK(state.planCommitted);
        CHECK(state.warningTicks == (unique ? 102 : 72));
        CHECK(state.rayCount == (unique ? 0 : 14));
        const int duration = state.sequenceTicks;
        for (int i = 0; i < duration + 2; ++i) root::tick(boss, player, kArena);
        CHECK(state.clock >= duration);
        CHECK(boss.stage == root::Stage::Recover);
        CHECK(!state.radialExecuted);
        CHECK((state.progress.experiences & 1u) == 0u);
        CHECK(unique ? state.shots == 0 : state.shots == 14);
    }
}
}

int main() {
    predictionTracksCircularMotionAndBoundsConfidence();
    chargeLocksItsTargetAndModelsFlyingShots();
    actionSelectionAdaptsAndPenalizesRepeats();
    focusedAndUniquePlansDoNotEarnRadialExperience();
    std::cout << "Unweaver tactical prediction, action choice, charge and progression checks passed\n";
    return gFailures == 0 ? 0 : 1;
}
