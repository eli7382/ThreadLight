#include "area_hazards.h"
#include "sower.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace threadlight;
using area_hazards::Bounds;
using area_hazards::Corridor;
using area_hazards::PatchSpec;
using area_hazards::Pool;
using area_hazards::PoolKind;
using area_hazards::Shape;
using area_hazards::SlotState;
using foundation::Point;

int checks = 0;

void check(bool condition, const char* label) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

bool near(float a, float b, float epsilon = 0.02f) {
    return std::abs(a - b) <= epsilon;
}

PatchSpec disk(std::uint64_t owner, Point center, float radius = 10.0f,
               int warning = 1, int active = 3, int damage = 500) {
    PatchSpec spec;
    spec.shape = Shape::Disk;
    spec.ownerId = owner;
    spec.center = center;
    spec.radius = radius;
    spec.warningTicks = warning;
    spec.activeTicks = active;
    spec.damage = damage;
    return spec;
}

area_hazards::Ticket activate(Pool& pool, PatchSpec spec, Bounds bounds,
                              Corridor corridor = {}, float externalArea = 0.0f) {
    const auto ticket = pool.add(spec, bounds, corridor, externalArea);
    if (!ticket) return 0;
    const Point center = spec.center;
    pool.tick(center, center, 0.0f); // creation update is warning-fresh
    pool.tick(center, center, 0.0f); // the configured warning finishes here
    return ticket;
}

void warningAndLifetime() {
    Pool pool;
    const Bounds bounds{0, 400, 0, 400};
    const Point inside{100, 100};
    const auto ticket = pool.add(disk(1, inside, 10, 108, 3, 700), bounds);
    check(ticket != 0, "disk warning reservation");
    check(pool.tick(inside, inside, 2).damage == 0, "creation tick skips warning");
    check(pool.items()[0].warningRemaining == 108, "creation tick preserves all warning ticks");
    for (int i = 0; i < 107; ++i)
        check(pool.tick(inside, inside, 2).damage == 0, "warning cannot damage early");
    check(pool.items()[0].state == SlotState::Warning &&
          pool.items()[0].warningRemaining == 1, "warning remains until final tick");
    check(pool.tick(inside, inside, 2).damage == 0, "activation tick cannot damage");
    check(pool.items()[0].state == SlotState::Active &&
          pool.items()[0].activeRemaining == 3, "full warning activates with full active lifetime");
    for (int i = 0; i < 3; ++i)
        check(pool.tick(inside, inside, 2).damage == 700, "disk has full configured active lifetime");
    check(pool.count(SlotState::Empty) == static_cast<int>(area_hazards::kPoolSlots),
          "active patch expires after exact lifetime");
}

void sweptDiskCapsuleAndAnnulus() {
    const Bounds bounds{0, 400, 0, 400};
    Pool diskPool;
    check(activate(diskPool, disk(2, {200, 200}, 10), bounds) != 0, "activate disk sweep fixture");
    const auto diskHit = diskPool.tick({100, 200}, {300, 200}, 3);
    check(diskHit.damage == 500 && diskHit.sourceId == 2, "disk sweeps player motion");

    Pool capsulePool;
    PatchSpec capsule = disk(3, {200, 200});
    capsule.shape = Shape::Capsule;
    capsule.a = {200, 60}; capsule.b = {200, 340}; capsule.halfWidth = 4.0f;
    check(activate(capsulePool, capsule, bounds) != 0, "activate capsule fixture");
    check(capsulePool.tick({100, 200}, {300, 200}, 3).damage == 500,
          "capsule collision sweeps crossing player path");

    Pool ringPool;
    PatchSpec expanding = disk(4, {200, 200});
    expanding.shape = Shape::Annulus;
    expanding.radiusStart = 10.0f; expanding.radiusEnd = 100.0f;
    expanding.ringHalfWidth = 2.0f; expanding.activeTicks = 10;
    expanding.gapHalfAngleRadians = 0.0f;
    check(activate(ringPool, expanding, bounds) != 0, "activate expanding annulus fixture");
    check(ringPool.tick({200, 200}, {200, 200}, 0).damage == 0,
          "annulus does not hit before radial sweep reaches player");
    check(near(area_hazards::activeRadius(ringPool.items()[0]), 19.0f),
          "renderer radius helper exposes the current expanding radius");
    check(ringPool.tick({218, 200}, {222, 200}, 0).damage == 500,
          "annulus collision combines player and radial motion");

    Pool gapPool;
    PatchSpec ring = disk(5, {200, 200});
    ring.shape = Shape::Annulus;
    ring.radiusStart = ring.radiusEnd = 50.0f;
    ring.ringHalfWidth = 4.0f;
    ring.activeTicks = 4;
    ring.gapDirection = {1, 0};
    ring.gapHalfAngleRadians = 0.35f;
    check(activate(gapPool, ring, bounds) != 0, "activate opposed-gap annulus fixture");
    check(gapPool.tick({0, 200}, {400, 200}, 2).damage == 0,
          "swept path through both fixed opposed gaps stays safe");
    check(gapPool.tick({0, 230}, {400, 230}, 2).damage == 500,
          "path outside fixed gaps takes annulus damage");
}

PatchSpec annulus(float start,float end,float width,int ticks=120,float gap=.9f) {
    PatchSpec spec=disk(90,{0,0},1,1,ticks,2400);
    spec.shape=Shape::Annulus;spec.radiusStart=start;spec.radiusEnd=end;
    spec.ringHalfWidth=width;spec.gapDirection={1,0};spec.gapHalfAngleRadians=gap;
    return spec;
}

void exactAnnularSectorAndTimeCollision() {
    const Bounds arena{-1000,1000,-1000,1000};
    auto hit=[&](PatchSpec spec,Point previous,Point current,float radius){
        Pool pool(PoolKind::Boss);
        check(activate(pool,spec,arena)!=0,"reserve exact annulus fixture");
        return pool.tick(previous,current,radius).damage!=0;
    };
    const auto hollow=annulus(119,119,29);
    check(!hit(hollow,{58,56},{58,56},12),
          "empty-center disk overhang cannot close a safe ring corridor");
    check(!hit(hollow,{-200,0},{200,0},12),
          "fast motion through opposed gaps and empty center remains safe");
    check(hit(hollow,{0,-200},{0,200},2),
          "fast motion across closed ring arcs cannot tunnel");
    const auto heart=annulus(50,300,14,120,.8f);
    check(!hit(heart,{0,76.1f},{0,78.225f},12),
          "outward walking outruns the expanding pulse without a false hit");
    check(hit(heart,{0,80},{0,70},12),
          "inward motion meets the expanding pulse at shared event time");
    check(hit(heart,{0,77},{0,77},12),
          "expansion reaches a stationary player between tick endpoints");

    const float cosine=std::cos(.9f),sine=std::sin(.9f);
    auto gapPoint=[&](float clearance){return Point{119*cosine+clearance*sine,
                                                    119*sine-clearance*cosine};};
    const auto tangent=gapPoint(12),safe=gapPoint(12.02f),overlap=gapPoint(11.98f);
    check(hit(hollow,tangent,tangent,12),"gap boundary tangency counts as physical contact");
    check(!hit(hollow,safe,safe,12),"positive gap-edge clearance stays safe");
    check(hit(hollow,overlap,overlap,12),"gap-edge overlap hits the closed sector");
    check(hit(hollow,{0,78},{0,78},12),"inner closed-arc tangency is retained");
    check(!hit(hollow,{0,77.98f},{0,77.98f},12),"clearance inside the empty center stays safe");
    // Start in the gap and cross a radial side between movement endpoints.
    const Point sideA{119*cosine+20*sine,119*sine-20*cosine};
    const Point sideB{119*cosine-20*sine,119*sine+20*cosine};
    check(hit(hollow,sideA,sideB,2),"swept radial-edge contact uses its linear event");
    const Point outward{(cosine+sine)*.7071067812f,(sine-cosine)*.7071067812f};
    const Point tangentMotion{(cosine-sine)*.7071067812f,(sine+cosine)*.7071067812f};
    auto cornerPath=[&](float clearance,float sign){return Point{
        148*cosine+clearance*outward.x+sign*20*tangentMotion.x,
        148*sine+clearance*outward.y+sign*20*tangentMotion.y};};
    const auto cornerA=cornerPath(12,-1),cornerB=cornerPath(12,1);
    check(!hit(hollow,cornerA,cornerA,12)&&!hit(hollow,cornerB,cornerB,12),
          "corner tangency fixture has safe movement endpoints");
    check(hit(hollow,cornerA,cornerB,12),
          "interior-time radial corner tangency uses its quadratic event");
    check(!hit(hollow,cornerPath(12.02f,-1),cornerPath(12.02f,1),12),
          "positive clearance around a radial corner stays safe throughout motion");

    const auto shrinking=annulus(100,10,2,10,0);
    check(!hit(shrinking,{0,95.9f},{0,86.9f},2),
          "inward movement ahead of a contracting ring preserves time correlation");
    check(hit(shrinking,{0,95},{0,95},2),"contracting ring reaches a stationary player");
    check(hit(annulus(10,20,14,1),{0,0},{0,0},2),
          "expansion handles an inner radius clamped to zero during the tick");
    check(hit(annulus(20,10,14,1),{0,0},{0,0},2),
          "contraction handles the inner-radius zero breakpoint");

    Pool resized(PoolKind::Boss);auto diagonal=hollow;diagonal.gapDirection={1,1};
    check(activate(resized,diagonal,arena)!=0,"activate annulus resize fixture");
    check(resized.tick({0,119},{0,119},12,false).damage==0&&
          resized.items()[0].activeElapsed==0,"pause freezes annular event clocks");
    resized.remap(arena,{-2000,2000,-1000,1000});
    check(near(area_hazards::activeRadius(resized.items()[0]),119),
          "resize retains the numeric ring radius");
    constexpr float inverseRootFive=.4472135955f;
    const Point safeAxis{238*inverseRootFive,119*inverseRootFive};
    const Point closedAxis{-119*inverseRootFive,238*inverseRootFive};
    check(resized.tick(safeAxis,safeAxis,12).damage==0,
          "anisotropic resize uses the stored remapped safe-gap direction");
    check(resized.tick(closedAxis,closedAxis,12).damage==2400,
          "remapped closed sectors retain physical collision");
}

void capacityCorridorsAndArea() {
    const Bounds bounds{0, 1000, 0, 1000};
    Pool pool;
    check(pool.add(disk(1, {100, 100}), bounds) != 0, "first owner patch");
    check(pool.add(disk(1, {150, 100}), bounds) != 0, "second owner patch");
    check(pool.add(disk(1, {200, 100}), bounds) == 0, "ordinary owner cap is two");
    for (std::uint64_t owner = 2; owner <= 3; ++owner) {
        check(pool.add(disk(owner, {100.0f + owner * 60.0f, 200}), bounds) != 0,
              "ordinary pool accepts distinct owner");
        check(pool.add(disk(owner, {150.0f + owner * 60.0f, 200}), bounds) != 0,
              "ordinary pool accepts second patch per owner");
    }
    check(pool.liveCount() == 6 && pool.occupiedSlotCount() == 6,
          "ordinary global capacity is six");
    check(pool.add(disk(4, {400, 400}), bounds) == 0, "ordinary pool denies seventh live patch");

    Pool boss(PoolKind::Boss);
    for (int i = 0; i < 4; ++i)
        check(boss.add(disk(50, {100.0f + i * 80.0f, 100}), bounds) != 0,
              "boss pool accepts four patches per owner");
    check(boss.add(disk(50, {600, 100}), bounds) == 0, "boss pool capacity is four");

    Corridor route{true, {80, 300}, {192, 300}, 12.0f};
    Pool corridorPool;
    check(corridorPool.add(disk(7, {200, 300}, 20), bounds, route) == 0,
          "placement rejects player-width protected capsule");
    check(corridorPool.add(disk(7, {300, 400}, 20), bounds, route) != 0,
          "protected corridor does not reject distant placement");

    Pool unsafePool;
    const Corridor unsafeRoute{true, {50, 300}, {150, 300}, 12};
    check(unsafePool.add(disk(70, {100, 300}, 10, 5, 20), bounds) != 0,
          "unsafe-corridor warning fixture");
    check(activate(unsafePool, disk(71, {100, 300}, 10, 1, 20), bounds) != 0,
          "unsafe-corridor active fixture");
    unsafePool.cancelUnsafeCorridors(unsafeRoute);
    check(unsafePool.count(SlotState::Warning) == 0 &&
          unsafePool.count(SlotState::Fading) == 1,
          "corridor commit cancels warnings and harmlessly fades conflicting active patches");
    check(unsafePool.tick({100, 300}, {100, 300}, 0).damage == 0,
          "corridor cancellation leaves no hidden damage");

    Pool bossCorridor(PoolKind::Boss);
    PatchSpec opposed = disk(8, {500, 500});
    opposed.shape = Shape::Annulus;
    opposed.radiusStart = opposed.radiusEnd = 100;
    opposed.ringHalfWidth = 8;
    opposed.gapDirection = {1, 0};
    opposed.gapHalfAngleRadians = 1.0f;
    Corridor throughBoss{true, {0, 500}, {1000, 500}, 68};
    Corridor acrossClosedPart{true, {500, 0}, {500, 1000}, 12};
    check(bossCorridor.add(opposed, bounds, throughBoss) != 0,
          "opposed annulus gaps preserve center-crossing corridor");
    Pool closedGap(PoolKind::Boss);
    check(closedGap.add(opposed, bounds, acrossClosedPart) == 0,
          "annulus placement rejects route through closed ring section");

    const Bounds minHeartArena{0, 924, 0, 508};
    Pool clippedBossRing(PoolKind::Boss);
    PatchSpec heartRing = disk(81, {462, 254});
    heartRing.shape = Shape::Annulus;
    heartRing.radiusStart = 50; heartRing.radiusEnd = 300;
    heartRing.ringHalfWidth = 14;
    heartRing.gapHalfAngleRadians = 0.8f;
    check(clippedBossRing.add(heartRing, minHeartArena) != 0,
          "boss annulus may clip arena edge at minimum Heart bounds");
    check(clippedBossRing.areaOccupied(minHeartArena) <
          (924.0f * 508.0f * area_hazards::kMaxOccupiedFraction),
          "expanding ring area uses maximum instantaneous band with gaps");

    Pool areaPool;
    check(areaPool.add(disk(9, {300, 300}, 239), bounds) != 0,
          "area just below eighteen percent is accepted");
    Pool overArea;
    check(overArea.add(disk(9, {300, 300}, 240), bounds) == 0,
          "area above eighteen percent is rejected");
    Pool externalPool;
    check(externalPool.add(disk(10, {100, 100}, 100), bounds) != 0,
          "world-area fixture accepted");
    const float oneDiskArea = externalPool.areaOccupied(bounds);
    check(near(oneDiskArea, 31415.93f, 1.0f), "areaOccupied reports world-area units");
    check(near(externalPool.areaOccupied(bounds, 123.5f), oneDiskArea + 123.5f),
          "areaOccupied includes other-pool external area");
    check(externalPool.add(disk(11, {500, 500}, 200), bounds, {}, 60000.0f) == 0,
          "external occupied area participates in combined cap");
}

void cancellationPauseRemapAndSlots() {
    const Bounds bounds{0, 400, 0, 400};
    Pool pool;
    const Point at{100, 100};
    const auto warning = pool.add(disk(20, at, 10, 108, 20), bounds);
    check(warning != 0, "warning exists before source death");
    pool.cancelOwner(20);
    check(pool.count(SlotState::Warning) == 0, "owner death cancels warning immediately");

    const auto active = activate(pool, disk(21, at, 10, 1, 50), bounds);
    check(active != 0, "active patch exists before source death");
    pool.cancelOwner(21);
    check(pool.count(SlotState::Fading) == 1 && pool.items()[0].fadeRemaining == 30,
          "owner death gives active patch a harmless thirty-tick fade");
    for (int i = 0; i < 31; ++i)
        check(pool.tick(at, at, 0).damage == 0, "harmless fade never damages");
    check(pool.count(SlotState::Fading) == 0, "harmless fade expires after thirty updates");

    const auto paused = pool.add(disk(22, at, 10, 2, 4), bounds);
    check(paused != 0, "pause fixture warning exists");
    pool.tick(at, at, 0, false);
    check(pool.items()[0].warningRemaining == 2 && pool.items()[0].fresh,
          "pause freezes warning and creation flags");
    pool.tick(at, at, 0);
    check(pool.items()[0].warningRemaining == 2, "creation update is still skipped after pause");

    Pool resized;
    const auto resizedTicket = resized.add(disk(23, {100, 120}, 10, 7, 9), bounds);
    check(resizedTicket != 0, "resize fixture added");
    const int warningTicksBefore = resized.items()[0].warningRemaining;
    resized.remap(bounds, {0, 800, 0, 800});
    check(near(resized.items()[0].spec.center.x, 200) &&
          near(resized.items()[0].spec.center.y, 240) &&
          near(resized.items()[0].spec.radius, 10), "resize remaps center and preserves logical radius");
    check(resized.items()[0].warningRemaining == warningTicksBefore,
          "resize preserves warning clock");

    Pool resizeBudget;
    const Bounds oldBounds{0, 1000, 0, 1000}, smallBounds{0, 100, 0, 100};
    const auto older = resizeBudget.add(disk(24, {300, 300}, 20), oldBounds);
    const auto newer = resizeBudget.add(disk(25, {700, 700}, 30), oldBounds);
    check(older != 0 && newer != 0, "resize area-budget fixtures added");
    resizeBudget.remap(oldBounds, smallBounds);
    check(resizeBudget.liveCount() == 1 &&
          resizeBudget.items()[0].ticket == older &&
          near(resizeBudget.items()[0].spec.radius, 20),
          "resize preserves logical size and cancels newest warning until area fits");
    Pool externalResize;
    check(externalResize.add(disk(26, {500, 500}, 20), oldBounds) != 0,
          "external resize-budget fixture added");
    externalResize.remap(oldBounds, smallBounds, 600.0f);
    check(externalResize.liveCount() == 0,
          "resize removes live patches until external pool area also fits");

    Pool slotPool;
    for (std::uint64_t owner = 1; owner <= 6; ++owner) {
        const auto t = activate(slotPool, disk(owner, {50.0f + owner * 40.0f, 100}, 5, 1, 100), bounds);
        check(t != 0, "slot fixture activates ordinary hazard");
        slotPool.cancelOwner(owner);
    }
    check(slotPool.count(SlotState::Fading) == 6, "six fading patches retain slots");
    for (std::uint64_t owner = 10; owner < 16; ++owner)
        check(slotPool.add(disk(owner, {50.0f + owner * 10.0f, 300}), bounds) != 0,
              "warnings can use remaining slots while fades render");
    check(slotPool.occupiedSlotCount() == 12, "twelve fixed slots include harmless fades");
    check(slotPool.add(disk(30, {350, 350}), bounds) == 0,
          "full twelve-slot pool rejects another reservation");
    slotPool.clear();
    check(slotPool.occupiedSlotCount() == 0, "clear removes warnings and fades immediately");
}

void sowerCommitAndStarvation() {
    using namespace sower;
    const Bounds arena{0, 400, 0, 400};
    check(configForChapter(4).castIntervalTicks == 540 &&
          configForChapter(4).activeTicks == 336 &&
          configForChapter(4).damage == 1800 && configForChapter(4).radius == 45,
          "chapter four Sower tuning");
    check(configForChapter(5).castIntervalTicks == 480 &&
          configForChapter(5).activeTicks == 384 &&
          configForChapter(5).damage == 2000 && configForChapter(5).radius == 55,
          "chapter five Sower tuning");

    Pool full;
    for (std::uint64_t owner = 1; owner <= 6; ++owner)
        check(full.add(disk(owner, {30.0f + owner * 45.0f, 50}), arena) != 0,
              "fill ordinary warning capacity");
    Config quick{4, 110, 2, 900, 64, 10};
    State state;
    start(state, 90, quick);
    Point position{100, 100};
    Point velocity{};
    const Point player{310, 100};
    TickEvents event = tick(state, position, velocity, player, {}, arena, full);
    check(!event.warningStarted && state.remaining == 1,
          "Sower seek reaches ready state without premature cast");
    event = tick(state, position, velocity, player, {}, arena, full);
    check(event.commitDenied && !event.warningStarted && state.phase == Phase::Seek &&
          state.remaining == 1, "full pool denial waits at ready state");

    Pool open;
    start(state, 90, quick);
    position = {100, 100}; velocity = {};
    tick(state, position, velocity, player, {}, arena, open);
    event = tick(state, position, velocity, player, {}, arena, open, {}, 0, false);
    check(event.commitDenied && !event.warningStarted && state.phase == Phase::Seek,
          "commit coordinator starvation creates no attack");
    event = tick(state, position, velocity, player, {}, arena, open);
    check(event.warningStarted && event.ticket != 0 && committed(state) &&
          open.count(SlotState::Warning) == 1,
          "Sower reserves complete patch before entering Windup");
    const auto reservedTicket = event.ticket;
    open.tick(player, player, 0); // Consume the commit update's creation skip.

    for (int i = 0; i < kSoftAimTicks; ++i) {
        const Point movingPlayer{player.x + i * 2.0f, player.y};
        event = tick(state, position, velocity, movingPlayer, {240, 0}, arena, open);
        open.tick(movingPlayer, movingPlayer, 0);
    }
    check(event.aimLocked && state.aimLocked && state.remaining == 78,
          "Sower locks after exactly thirty soft-aim ticks");
    const Point lockedCenter = open.items()[0].spec.center;
    check(!open.retargetWarning(reservedTicket, {20, 350}, arena) &&
          near(open.items()[0].spec.center.x, lockedCenter.x) &&
          near(open.items()[0].spec.center.y, lockedCenter.y),
          "reserved geometry rejects retarget after the soft window");
    for (int i = 0; i < 77; ++i) {
        const Point changedPlayer{20, 350};
        event = tick(state, position, velocity, changedPlayer, {-900, 300}, arena, open);
        open.tick(changedPlayer, changedPlayer, 0);
        check(!event.aimLocked, "locked phase does not emit another lock event");
    }
    check(state.remaining == 1 &&
          near(open.items()[0].spec.center.x, lockedCenter.x) &&
          near(open.items()[0].spec.center.y, lockedCenter.y),
          "Sower aim and reserved geometry stay fixed after lock");
    event = tick(state, position, velocity, {20, 350}, {}, arena, open);
    const auto poolHit = open.tick({20, 350}, {20, 350}, 0);
    check(event.shotReady && !committed(state) && poolHit.damage == 0 &&
          open.count(SlotState::Active) == 1,
          "full 108-tick windup activates reserved hazard");
    check(state.remaining == 2, "Sower interval clock resumes after windup");
}

} // namespace

int main() {
    warningAndLifetime();
    sweptDiskCapsuleAndAnnulus();
    exactAnnularSectorAndTimeCollision();
    capacityCorridorsAndArea();
    cancellationPauseRemapAndSlots();
    sowerCommitAndStarvation();
    std::cout << "area_hazards_test: " << checks << " checks passed\n";
    return 0;
}
