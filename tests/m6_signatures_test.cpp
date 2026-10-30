#include "cocoon.h"
#include "resonant_loom.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using threadlight::foundation::Point;
namespace cw = threadlight::crossweave;
namespace co = threadlight::cocoon;
namespace loom = threadlight::resonant_loom;

struct LoopFixture {
    std::array<cw::Segment, 3> history{};
    Point previous{};
    Point current{};
};

LoopFixture rectangle(std::uint64_t firstId, float width, float height,
                      int oldAge = cw::kMinimumSegmentAgeTicks) {
    constexpr float crossingX = 50.0f;
    const float farX = crossingX + width;
    LoopFixture fixture;
    fixture.history = {{
        {firstId, {0.0f, 0.0f}, {farX, 0.0f}, oldAge},
        {firstId + 1, {farX, 0.0f}, {farX, height}, 2},
        {firstId + 2, {farX, height}, {crossingX, height}, 1}
    }};
    fixture.previous = {crossingX, height};
    fixture.current = {crossingX, -10.0f};
    return fixture;
}

co::Formation trigger(co::State& state, const LoopFixture& fixture,
                      bool combat = true, bool owned = true) {
    return co::tick(state, fixture.previous, fixture.current, fixture.history, combat, owned);
}

void disabledSilkRetainsClaimsAndBreaksLoops(){
    cw::State crossing;crossing.travelDistance=cw::kTravelDistance;
    std::array<cw::Segment,1> segment{{{900,{0,-50},{0,50},100}}};
    CHECK(cw::tick(crossing,{-10,0},{10,0},segment,true,true).triggered);
    segment[0].available=false;
    CHECK(!cw::tick(crossing,{10,0},{10,0},segment,true,true).triggered);
    CHECK(cw::claimed(crossing,900));
    segment[0].available=true;crossing.cooldownTicks=0;crossing.travelDistance=cw::kTravelDistance;
    CHECK(!cw::tick(crossing,{10,0},{-10,0},segment,true,true).triggered);
    auto loop=rectangle(910,100,100);co::State intact;
    CHECK(trigger(intact,loop).formed);
    loop.history[1].available=false;co::State broken;
    CHECK(!trigger(broken,loop).formed);
    loop.history[1].available=true;loop.history[0].available=false;
    CHECK(!trigger(intact,loop).formed);
    CHECK(cw::claimed(intact.detector,910));
    loop.history[0].available=true;intact.barrierCenti=intact.remainingTicks=intact.cooldownTicks=0;
    CHECK(!trigger(intact,loop).formed);
}

void cocoonAreaAndPathBoundaries() {
    co::State areaBoundary;
    const LoopFixture areaFixture = rectangle(100, 35.0f, 100.0f);
    const co::Formation area = trigger(areaBoundary, areaFixture);
    CHECK(area.formed && area.segmentId == 100);
    CHECK(area.area == co::kMinimumEnclosedArea);
    CHECK(area.pathLength == 270.0);
    CHECK(areaBoundary.barrierCenti == co::kBarrierCapacityCenti);
    CHECK(areaBoundary.remainingTicks == co::kBarrierDurationTicks);
    CHECK(areaBoundary.cooldownTicks == co::kCooldownTicks);

    co::State pathBoundary;
    const LoopFixture pathFixture = rectangle(200, 60.0f, 60.0f);
    const co::Formation path = trigger(pathBoundary, pathFixture);
    CHECK(path.formed && path.area == 3600.0);
    CHECK(path.pathLength == co::kMinimumClosedPath);
}

void cocoonRejectsSmallLoopsButConsumesTheirCrossings() {
    co::State state;
    const LoopFixture smallArea = rectangle(300, 30.0f, 100.0f);
    const co::Formation tooSmall = trigger(state, smallArea);
    CHECK(!tooSmall.formed);
    CHECK(cw::claimed(state.detector, 300));
    CHECK(state.barrierCenti == 0 && state.cooldownTicks == 0);

    const LoopFixture smallPath = rectangle(400, 58.0f, 61.0f);
    const co::Formation tooShort = trigger(state, smallPath);
    CHECK(!tooShort.formed);
    CHECK(cw::claimed(state.detector, 400));

    // Failed geometry consumes only the IDs it crossed; a later, distinct
    // stable-ID loop can still form the one barrier.
    const LoopFixture validLater = rectangle(500, 60.0f, 60.0f);
    const co::Formation later = trigger(state, validLater);
    CHECK(later.formed && later.segmentId == 500);
}

void cocoonRejectsIneligibleAndEndpointCrossings() {
    co::State expiredState;
    const LoopFixture expired = rectangle(600, 60.0f, 60.0f,
                                          cw::kMaximumSegmentAgeTicks + 1);
    CHECK(!trigger(expiredState, expired).formed);
    CHECK(!cw::claimed(expiredState.detector, 600));

    co::State endpointState;
    endpointState.detector.travelDistance = cw::kTravelDistance;
    const std::array<cw::Segment, 1> endpoint{{
        {700, {0.0f, 0.0f}, {10.0f, 0.0f}, 100}
    }};
    CHECK(!co::tick(endpointState, {0.0f, -5.0f}, {0.0f, 5.0f}, endpoint, true, true).formed);
    CHECK(!cw::claimed(endpointState.detector, 700));
    CHECK(endpointState.detector.pendingCount == 0);
    CHECK(!co::tick(endpointState, {0.0f, 5.0f}, {0.0f, -5.0f}, endpoint, true, true).formed);
    CHECK(!cw::claimed(endpointState.detector, 700));
}

void cocoonPendingTouchReverseAndExpiry() {
    co::State state;
    const std::array<cw::Segment, 1> knot{{
        {710, {-10.0f, 0.0f}, {10.0f, 0.0f}, 100}
    }};
    CHECK(!co::tick(state, {0.0f, -2.125f}, {0.0f, 0.0f}, knot, true, true).formed);
    CHECK(state.detector.pendingCount == 1 && !cw::claimed(state.detector, 710));
    CHECK(!co::tick(state, {0.0f, 0.0f}, {0.0f, -2.125f}, knot, true, true).formed);
    CHECK(state.detector.pendingCount == 0 && !cw::claimed(state.detector, 710));

    CHECK(!co::tick(state, {0.0f, -2.125f}, {0.0f, 0.0f}, knot, true, true).formed);
    const std::array<cw::Segment, 1> expired{{
        {710, {-10.0f, 0.0f}, {10.0f, 0.0f}, cw::kMaximumSegmentAgeTicks + 1}
    }};
    CHECK(!co::tick(state, {0.0f, 0.0f}, {0.0f, 2.125f}, expired, true, true).formed);
    CHECK(state.detector.pendingCount == 0 && !cw::claimed(state.detector, 710));
}

void cocoonExaminesAllSameTickCrossings() {
    // ID 800 closes only 1250 area. ID 804 encloses 5000 area and is crossed
    // in the same movement. Crossweave's one-result pulse selects ID 800, so
    // Cocoon must inspect all newly consumed IDs to find the valid later loop.
    const std::array<cw::Segment, 7> history{{
        {800, {0.0f, 0.0f}, {100.0f, 0.0f}, 78},
        {801, {100.0f, 0.0f}, {100.0f, -37.5f}, 77},
        {802, {100.0f, -37.5f}, {0.0f, -37.5f}, 76},
        {803, {0.0f, -37.5f}, {0.0f, 50.0f}, 75},
        {804, {0.0f, 50.0f}, {100.0f, 50.0f}, 74},
        {805, {100.0f, 50.0f}, {100.0f, 150.0f}, 73},
        {806, {100.0f, 150.0f}, {50.0f, 150.0f}, 72}
    }};
    co::State state;
    const co::Formation formation = co::tick(state, {50.0f, 150.0f}, {50.0f, -10.0f},
                                              history, true, true);
    CHECK(cw::claimed(state.detector, 800) && cw::claimed(state.detector, 804));
    CHECK(formation.formed && formation.segmentId == 804);
    CHECK(formation.area == 5000.0 && formation.pathLength == 300.0);
}

void cocoonRejectsRepeatedWindingAndSelfIntersections() {
    const std::array<cw::Segment, 9> repeatedWinding{{
        {1300, {-22.5f, 0.0f}, {22.5f, 0.0f}, 100},
        {1301, {22.5f, 0.0f}, {22.5f, 45.0f}, 99},
        {1302, {22.5f, 45.0f}, {-22.5f, 45.0f}, 98},
        {1303, {-22.5f, 45.0f}, {-22.5f, 0.0f}, 97},
        {1304, {-22.5f, 0.0f}, {22.5f, 0.0f}, 96},
        {1305, {22.5f, 0.0f}, {22.5f, 45.0f}, 95},
        {1306, {22.5f, 45.0f}, {-22.5f, 45.0f}, 94},
        {1307, {-22.5f, 45.0f}, {-22.5f, -2.0f}, 93},
        {1308, {-22.5f, -2.0f}, {0.0f, -2.0f}, 92}
    }};
    co::ClosedPath metrics;
    CHECK(co::closedPathMetrics({0.0f, 0.0f}, {0.0f, -2.0f}, repeatedWinding, 0, metrics));
    CHECK(metrics.area == 4095.0 && metrics.pathLength == 364.0);
    CHECK(metrics.area >= co::kMinimumEnclosedArea &&
          metrics.pathLength >= co::kMinimumClosedPath);
    CHECK(!co::simplePolygon(std::span<const Point>(metrics.vertices.data(), metrics.vertexCount)));

    co::State state;
    const co::Formation formation = co::tick(state, {0.0f, -2.0f}, {0.0f, 2.0f},
                                              repeatedWinding, true, true);
    CHECK(!formation.formed);
    CHECK(cw::claimed(state.detector, 1300));
    CHECK(state.barrierCenti == 0);

    const std::array<Point, 4> figureEight{{
        {-100.0f, -100.0f}, {100.0f, 100.0f}, {-100.0f, 100.0f}, {100.0f, -100.0f}
    }};
    const std::array<Point, 6> selfTouch{{
        {0.0f, 0.0f}, {100.0f, 0.0f}, {100.0f, 100.0f},
        {0.0f, 100.0f}, {0.0f, 0.0f}, {-100.0f, 100.0f}
    }};
    const std::array<Point, 5> concaveSimple{{
        {0.0f, 0.0f}, {100.0f, 0.0f}, {100.0f, 100.0f},
        {50.0f, 50.0f}, {0.0f, 100.0f}
    }};
    const std::array<Point, 5> collinearContinuation{{
        {0.0f, 0.0f}, {50.0f, 0.0f}, {100.0f, 0.0f},
        {100.0f, 100.0f}, {0.0f, 100.0f}
    }};
    const std::array<Point, 5> collinearBacktrack{{
        {0.0f, 0.0f}, {100.0f, 0.0f}, {50.0f, 0.0f},
        {50.0f, 100.0f}, {0.0f, 100.0f}
    }};
    const std::array<Point, 6> zeroLengthAndClosure{{
        {0.0f, 0.0f}, {0.0f, 0.0f}, {100.0f, 0.0f},
        {100.0f, 100.0f}, {0.0f, 100.0f}, {0.0f, 0.0f}
    }};
    CHECK(!co::simplePolygon(figureEight));
    CHECK(!co::simplePolygon(selfTouch));
    CHECK(co::simplePolygon(concaveSimple));
    CHECK(co::simplePolygon(collinearContinuation));
    CHECK(!co::simplePolygon(collinearBacktrack));
    CHECK(co::simplePolygon(zeroLengthAndClosure));
}

void cocoonClocksPauseAndDoNotRefresh() {
    co::State state;
    const LoopFixture first = rectangle(900, 60.0f, 60.0f);
    CHECK(trigger(state, first).formed);
    CHECK(state.barrierCenti == 1200 && state.remainingTicks == 600 && state.cooldownTicks == 960);

    const int pausedRemaining = state.remainingTicks;
    const int pausedCooldown = state.cooldownTicks;
    const int pausedBarrier = state.barrierCenti;
    for (int i = 0; i < 20; ++i) {
        (void)co::tick(state, first.previous, first.current, first.history, false, true);
    }
    CHECK(state.remainingTicks == pausedRemaining && state.cooldownTicks == pausedCooldown);
    CHECK(state.barrierCenti == pausedBarrier);

    // Even an unrelated qualifying loop cannot refresh an active barrier.
    state.barrierCenti = 700; // Simulate prior absorption by shared combat.
    const LoopFixture second = rectangle(1000, 60.0f, 60.0f);
    CHECK(!trigger(state, second).formed);
    CHECK(cw::claimed(state.detector, 1000)); // Detector still consumes crossings while blocked.
    CHECK(state.barrierCenti == 700);
    CHECK(state.remainingTicks == pausedRemaining - 1);
    CHECK(state.cooldownTicks == pausedCooldown - 1);

    for (int i = 0; i < co::kBarrierDurationTicks - 1; ++i) {
        (void)co::tick(state, {50.0f, 0.0f}, {50.0f, 0.0f}, {}, true, true);
    }
    CHECK(state.remainingTicks == 0 && state.barrierCenti == 0);
    CHECK(state.cooldownTicks == co::kCooldownTicks - co::kBarrierDurationTicks);

    for (int i = 0; i < 359; ++i) {
        (void)co::tick(state, {50.0f, 0.0f}, {50.0f, 0.0f}, {}, true, true);
    }
    CHECK(state.cooldownTicks == 1);
    const LoopFixture atCooldownBoundary = rectangle(1100, 60.0f, 60.0f);
    CHECK(trigger(state, atCooldownBoundary).formed);
    CHECK(state.cooldownTicks == co::kCooldownTicks);

    co::clear(state);
    CHECK(state.barrierCenti == 0 && state.remainingTicks == 0 && state.cooldownTicks == 0);
    CHECK(state.detector.consumedCount == 0 && state.detector.pendingCount == 0);
}

void cocoonHistoryBoundFailsClosed() {
    std::array<cw::Segment, 520> supported{};
    for (std::size_t i = 0; i < supported.size(); ++i) {
        supported[i] = {static_cast<std::uint64_t>(1200 + i), {-10.0f, 0.0f}, {10.0f, 0.0f}, 100};
    }
    co::State withinBound;
    CHECK(!co::tick(withinBound, {0.0f, -2.0f}, {0.0f, 2.0f}, supported, true, true).formed);
    CHECK(withinBound.detector.consumedCount == supported.size());
    CHECK(!withinBound.detector.claimOverflow);

    std::array<cw::Segment, cw::kClaimCapacity + 1> excessive{};
    for (std::size_t i = 0; i < excessive.size(); ++i) {
        excessive[i] = {static_cast<std::uint64_t>(3000 + i), {-10.0f, 0.0f}, {10.0f, 0.0f}, 100};
    }
    co::State failClosed;
    CHECK(!co::tick(failClosed, {0.0f, -2.0f}, {0.0f, 2.0f}, excessive, true, true).formed);
    CHECK(failClosed.detector.consumedCount == cw::kClaimCapacity);
    CHECK(failClosed.detector.claimOverflow);
}

void loomOnlyCountsActualOrdinaryHpHits() {
    loom::State state;
    CHECK(!loom::onHit(state, true, false, loom::HitKind::OrdinarySilk));
    CHECK(!loom::onHit(state, true, true, loom::HitKind::SignatureProc));
    CHECK(!loom::onHit(state, true, true, loom::HitKind::RewardlessBud));
    CHECK(!loom::onHit(state, false, true, loom::HitKind::OrdinarySilk));
    CHECK(state.charges == 0);

    for (int i = 0; i < loom::kHitsPerBurst - 1; ++i) {
        CHECK(!loom::onHit(state, true, true, loom::HitKind::OrdinarySilk));
        CHECK(state.charges == i + 1);
    }
    CHECK(loom::onHit(state, true, true, loom::HitKind::OrdinarySilk));
    CHECK(state.charges == 0 && state.cooldownTicks == loom::kCooldownTicks);

    for (int i = 0; i < 12; ++i) {
        CHECK(!loom::onHit(state, true, true, loom::HitKind::OrdinarySilk));
    }
    CHECK(state.charges == 0 && state.cooldownTicks == loom::kCooldownTicks);
    loom::tick(state, false, true);
    CHECK(state.cooldownTicks == loom::kCooldownTicks);
    for (int i = 0; i < loom::kCooldownTicks - 1; ++i) loom::tick(state, true, true);
    CHECK(state.cooldownTicks == 1);
    CHECK(!loom::onHit(state, true, true, loom::HitKind::OrdinarySilk));
    loom::tick(state, true, true);
    CHECK(state.cooldownTicks == 0);
    for (int i = 0; i < loom::kHitsPerBurst - 1; ++i) {
        CHECK(!loom::onHit(state, true, true, loom::HitKind::OrdinarySilk));
    }
    CHECK(loom::onHit(state, true, true, loom::HitKind::OrdinarySilk));
    CHECK(state.cooldownTicks == loom::kCooldownTicks);
}

void loomSelectionAndDamageRounding() {
    const loom::Target source{5, {0.0f, 0.0f}, 1, 7};
    const std::array<loom::Candidate, 7> candidates{{
        {9, {100.0f, 0.0f}, 9, 1},
        {3, {0.0f, 100.0f}, 3, 1},
        {2, {-100.0f, 0.0f}, 2, 1},
        {12, {180.0f, 0.0f}, 12, 1},
        {14, {180.01f, 0.0f}, 14, 1},
        {5, {1.0f, 0.0f}, 5, 1},
        {0, {0.0f, 0.0f}, 0, 1}
    }};
    const loom::Selection selected = loom::selectTargets(source, candidates);
    CHECK(selected.count == 3);
    CHECK(selected.items[0].id == 5);
    CHECK(selected.items[1].id == 2 && selected.items[2].id == 3);

    CHECK(loom::damage(100) == 60);
    CHECK(loom::damage(100, true) == 30);
    CHECK(loom::damage(101) == 61);
    CHECK(loom::damage(101, true) == 30);
    CHECK(loom::damage(5) == 3);
    CHECK(loom::damage(5, true) == 2);
    CHECK(loom::damage(1) == 1 && loom::damage(1, true) == 0);
    CHECK(loom::damage(0) == 0 && loom::damage(-10, true) == 0);
    CHECK(loom::damage(std::numeric_limits<int>::max()) == 1288490188);
    CHECK(loom::damage(std::numeric_limits<int>::max(), true) == 644245094);
}

} // namespace

int main() {
    disabledSilkRetainsClaimsAndBreaksLoops();
    cocoonAreaAndPathBoundaries();
    cocoonRejectsSmallLoopsButConsumesTheirCrossings();
    cocoonRejectsIneligibleAndEndpointCrossings();
    cocoonPendingTouchReverseAndExpiry();
    cocoonExaminesAllSameTickCrossings();
    cocoonRejectsRepeatedWindingAndSelfIntersections();
    cocoonClocksPauseAndDoNotRefresh();
    cocoonHistoryBoundFailsClosed();
    loomOnlyCountsActualOrdinaryHpHits();
    loomSelectionAndDamageRounding();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All M6 signature helper tests passed\n";
    return 0;
}
