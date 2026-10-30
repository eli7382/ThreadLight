#include "chapter_boss.h"
#include "crossweave.h"
#include "game_logic.h"
#include "game_layout.h"
#include "needle_interception.h"
#include "silk_utilities.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace root = threadlight::chapter_boss;
namespace uw = threadlight::unweaver;
namespace cross = threadlight::crossweave;
namespace ranged = threadlight::ranged;
namespace needle = threadlight::needle_interception;
namespace silk = threadlight::silk_utilities;
using Point = threadlight::foundation::Point;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed: " #condition " at " << __FILE__ << ':' << __LINE__ << '\n'; \
    std::abort(); } } while (false)

namespace {
constexpr root::Arena kArena{12.f, 912.f, 12.f, 496.f};
constexpr Point kPlayer{690.f, 254.f};
int gTicks = 0;
int gPeakRays = 0;

root::State makeUnweaver(Point position = {460.f, 254.f}) {
    root::State boss;
    root::start(boss, position, kArena, root::Kind::Unweaver);
    boss.unweaver.playerBounds = kArena;
    boss.unweaver.playerSpeed = 255.f;
    boss.unweaver.playerDashMultiplier = 3.7f;
    return boss;
}

void step(root::State& boss, Point player) {
    root::tick(boss, player, kArena);
    if (boss.kind == root::Kind::Unweaver) uw::finish(boss);
    ++gTicks;
}

bool driveToWarning(root::State& boss, Point player, int maximumTicks = 1200) {
    for (int i = 0; i < maximumTicks; ++i) {
        if (boss.stage == root::Stage::RootWarning && boss.unweaver.planCommitted) return true;
        step(boss, player);
    }
    return boss.stage == root::Stage::RootWarning && boss.unweaver.planCommitted;
}

void driveToHistory(root::State& boss, Point player) {
    for (int i = 0; i < 60; ++i) step(boss, player);
    CHECK(boss.unweaver.historyCount >= 24);
}

std::array<cross::Segment, 3> validStrands() {
    return {{{1, {230.f, 180.f}, {330.f, 180.f}, 120},
             {2, {230.f, 240.f}, {330.f, 240.f}, 160},
             {3, {230.f, 300.f}, {330.f, 300.f}, 200}}};
}

void selectWhenRequested(root::State& boss, Point player) {
    if (!boss.unweaver.needsStrands) return;
    const auto strands = validStrands();
    uw::selectStrands(boss.unweaver, std::span<const cross::Segment>(strands), player);
}

void pumpProjectiles(root::State& boss, Point player) {
    (void)root::projectileTick(boss, player, player, 12.f, kArena);
}

bool runToExperiencePair(root::State& boss, Point player, int maximumTicks = 3600) {
    for (int i = 0; i < maximumTicks; ++i) {
        selectWhenRequested(boss, player);
        root::tick(boss, player, kArena);
        pumpProjectiles(boss, player);
        if (boss.stage == root::Stage::RootActive && boss.unweaver.sealTicks > 0) {
            for (int bud = 0; bud < boss.budCount; ++bud)
                if (boss.buds[bud].health.alive()) CHECK(root::damageBud(boss, bud, 1000, false) == threadlight::combat::SilkHit::Killed);
        }
        ++gTicks;
        if (boss.unweaver.progress.experiences == 3u) return true;
        uw::finish(boss);
    }
    return boss.unweaver.progress.experiences == 3u;
}

void familyAdmissionAndExecution(uw::Attack attack) {
    auto boss = makeUnweaver();
    auto& state = boss.unweaver;
    state.forcedAttack = static_cast<int>(attack);
    state.forcedMechanic = static_cast<int>(uw::Mechanic::Hunt);
    state.huntExecuted = true;
    driveToHistory(boss, kPlayer);
    CHECK(driveToWarning(boss, kPlayer));
    CHECK(state.planCommitted);
    CHECK(state.rayCount > 0 && state.rayCount <= static_cast<int>(state.rays.size()));
    gPeakRays = std::max(gPeakRays, state.rayCount);
    CHECK(state.rayCount == (attack == uw::Attack::FinalConvergence ? 36 : 24));
    CHECK(state.warningTicks == 102);
    CHECK(state.sequenceTicks <= state.warningTicks + 54 * 2 + 320);
    CHECK(boss.p.x >= kArena.left + root::vulnerableRadius(boss));
    CHECK(boss.p.x <= kArena.right - root::vulnerableRadius(boss));
    CHECK(boss.p.y >= kArena.top + root::vulnerableRadius(boss));
    CHECK(boss.p.y <= kArena.bottom - root::vulnerableRadius(boss));

    bool foundUnprotectedRoute = false, foundDashRoute = false;
    for (int route = 0; route < 8; ++route) {
        foundUnprotectedRoute |= uw::witness(state, kPlayer, 16.f, route, false);
        foundDashRoute |= uw::witness(state, kPlayer, 16.f, route, true);
    }
    CHECK(foundUnprotectedRoute || foundDashRoute); // Same bounded geometry used by admission.
    for (int i = 0; i < state.rayCount; ++i) {
        const auto& ray = state.rays[static_cast<std::size_t>(i)];
        CHECK(ray.ticket != 0 && ranged::detail::finite(ray.origin) && ranged::detail::finite(ray.direction));
        CHECK(std::abs(std::sqrt(ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y) - 1.f) < .002f);
    }

    // The committed bearings and captured echo remain fixed while the target moves.
    std::array<uw::Ray, 48> committed{};
    for (int i = 0; i < state.rayCount; ++i) committed[static_cast<std::size_t>(i)] = state.rays[static_cast<std::size_t>(i)];
    const uw::Echo lockedEcho = state.echo;
    for (int i = 0; i < 30; ++i) {
        const Point movingPlayer{std::clamp(kPlayer.x + static_cast<float>((i % 7) * 24 - 72), kArena.left, kArena.right),
                                 std::clamp(kPlayer.y + static_cast<float>((i % 5) * 17 - 34), kArena.top, kArena.bottom)};
        step(boss, movingPlayer);
        pumpProjectiles(boss, movingPlayer);
    }
    CHECK(state.rayCount > 0);
    for (int i = 0; i < state.rayCount; ++i) {
        const auto& before = committed[static_cast<std::size_t>(i)];
        const auto& after = state.rays[static_cast<std::size_t>(i)];
        CHECK(before.origin.x == after.origin.x && before.origin.y == after.origin.y);
        CHECK(before.direction.x == after.direction.x && before.direction.y == after.direction.y);
        CHECK(before.fireTick == after.fireTick);
    }
    CHECK(state.echo.count == lockedEcho.count);

    // Complete the real warning/launch/sequence path and count bounded outputs.
    int ticks = 0;
    while (boss.stage != root::Stage::Recover && ticks < 900) {
        step(boss, kPlayer);
        pumpProjectiles(boss, kPlayer);
        ++ticks;
    }
    CHECK(boss.stage == root::Stage::Recover);
    CHECK(state.radialExecuted && state.shots > 0);
    CHECK(boss.bullets.reservedCount() == 0);
    std::cout << "unweaver attack=" << uw::attackName(attack)
              << " rays=" << state.rayCount << " launched=" << state.shots
              << " plan_ticks=" << ticks << '\n';
}

void echoActivationAndHostility() {
    auto boss = makeUnweaver();
    auto& state = boss.unweaver;
    state.forcedMechanic = static_cast<int>(uw::Mechanic::Echo);
    state.forcedAttack = static_cast<int>(uw::Attack::RotatingEclipse);
    state.huntExecuted = true;
    driveToHistory(boss, kPlayer);
    CHECK(driveToWarning(boss, kPlayer));
    CHECK(state.echo.count >= 2 && state.echo.warning == state.warningTicks);
    const Point a = state.echo.path[0];
    CHECK(uw::hostileDamage(boss, a, a, 12.f) == 0); // Warning path cannot damage yet.
    int guard = 0;
    while (state.echo.warning > 0 && guard++ < 110) step(boss, kPlayer);
    CHECK(state.echo.warning == 0 && state.echo.active > 0);
    CHECK(uw::hostileDamage(boss, a, a, 12.f) == 0); // Fresh activation cannot hit on its first update.
    step(boss, kPlayer);
    CHECK(!state.echo.fresh && state.echo.active > 0);
    CHECK(uw::hostileDamage(boss, a, a, 12.f) >= 2200);
}

void strandCutsCorruptionAndExpiry() {
    const std::array<cross::Segment, 4> input{{
        {10, {10, 10}, {90, 10}, 421}, // Expired geometry.
        {0, {10, 20}, {90, 20}, 20},
        {11, {40, 30}, {41, 30}, 20}, // Degenerate by the selector's minimum length.
        {12, {120, 100}, {250, 100}, 20}}};
    for (const auto mechanic : {uw::Mechanic::Sever, uw::Mechanic::Corrupt}) {
        auto boss = makeUnweaver();
        auto& state = boss.unweaver;
        state.mechanic = mechanic;
        uw::selectStrands(state, std::span<const cross::Segment>(input), kPlayer);
        CHECK(state.strands[0].id == 12);
        CHECK(state.strands[1].id == 0);
        CHECK(uw::friendly(state, 12)); // Warning is not active geometry.
        CHECK(uw::friendly(state, 10) && uw::friendly(state, 99)); // Expired/unselected IDs stay friendly.
        for (int tick = 0; tick < 102; ++tick) step(boss, kPlayer);
        CHECK(state.mechanicExecuted);
        CHECK(mechanic == uw::Mechanic::Sever ? state.cuts == 1 : state.corruptions == 1);
        CHECK(!uw::friendly(state, 12));
        CHECK(uw::friendly(state, 10) && uw::friendly(state, 99));
        if (mechanic == uw::Mechanic::Corrupt) {
            for (int tick = 0; tick < 180; ++tick) step(boss, kPlayer);
            CHECK(uw::friendly(state, 12)); // Corruption expires with its active geometry.
        }
    }
}

void sealDamageAndOrdinaryCooldown() {
    auto boss = makeUnweaver();
    auto& state = boss.unweaver;
    state.progress.phase = boss.phase = 3;
    state.progress.experiences = 1u; // Completed prior radial exchange; the seal is now due.
    state.huntExecuted = true;
    state.forcedMechanic = static_cast<int>(uw::Mechanic::Seal);
    driveToHistory(boss, kPlayer);
    CHECK(driveToWarning(boss, kPlayer));
    CHECK(state.sealTicks == 420 && boss.budCount == 2);

    const auto signatureA = root::applySignature(boss, 1000);
    const auto signatureB = root::applySignature(boss, 1000);
    CHECK(signatureA.damage == 550 && signatureB.damage == 550); // Signature damage applies the seal once.
    const auto silkA = root::applySilk(boss, 1000);
    const auto silkB = root::applySilk(boss, 1000);
    CHECK(silkA.damage == 550 && silkB.damage == 0);
    CHECK(boss.health.silkCooldownTicks == threadlight::combat::kSilkHitIntervalTicks);
    for (int tick = 0; tick < threadlight::combat::kSilkHitIntervalTicks; ++tick) step(boss, kPlayer);
    CHECK(boss.health.silkCooldownTicks == 0);
    CHECK(root::applySilk(boss, 1000).damage == 550);

    while (boss.stage == root::Stage::RootWarning) step(boss, kPlayer);
    for (int bud = 0; bud < boss.budCount; ++bud)
        CHECK(root::damageBud(boss, bud, 1000, false) == threadlight::combat::SilkHit::Killed);
    step(boss, kPlayer);
    CHECK(state.sealBroken && state.mechanicExecuted && state.progress.experiences == 3u);
    CHECK(boss.stage == root::Stage::Recover);
}

void actualNeedlecatchAgainstUnweaverShot() {
    auto boss = makeUnweaver();
    auto& state = boss.unweaver;
    state.forcedAttack = static_cast<int>(uw::Attack::ShatteredHalo);
    state.forcedMechanic = static_cast<int>(uw::Mechanic::Hunt);
    state.huntExecuted = true;
    driveToHistory(boss, kPlayer);
    CHECK(driveToWarning(boss, kPlayer));
    int guard = 0;
    while (boss.bullets.flyingCount() == 0 && guard++ < 180) {
        step(boss, kPlayer);
        pumpProjectiles(boss, kPlayer);
    }
    CHECK(boss.bullets.flyingCount() > 0);
    const ranged::Projectile* projectile = nullptr;
    for (const auto& shot : boss.bullets.items()) if (shot.state == ranged::SlotState::Flying) { projectile = &shot; break; }
    CHECK(projectile != nullptr);
    const Point velocity = projectile->velocity;
    const Point end{projectile->p.x + velocity.x / 120.f, projectile->p.y + velocity.y / 120.f};
    const Point middle{(projectile->p.x + end.x) * .5f, (projectile->p.y + end.y) * .5f};
    const Point direction = ranged::detail::unitOr(velocity);
    const Point normal{-direction.y, direction.x};
    const needle::Segment activeSilk{{middle.x - normal.x * 40.f, middle.y - normal.y * 40.f},
                                     {middle.x + normal.x * 40.f, middle.y + normal.y * 40.f}};
    const Point distantPlayer{middle.x + direction.x * 120.f, middle.y + direction.y * 120.f};
    const auto hit = needle::find(ranged::Pool{}, boss.bullets,
        std::span<const needle::Segment>(&activeSilk, 1), 5.f, distantPlayer, distantPlayer, 12.f,
        {kArena.left, kArena.right, kArena.top, kArena.bottom});
    CHECK(hit.id == projectile->id && hit.pool == 1);
    CHECK(boss.bullets.intercept(hit.id));
    silk::NeedlecatchState catchState;
    CHECK(silk::trigger(catchState, 3));
    CHECK(catchState.cooldownTicks == 360 && !silk::ready(catchState, 3));
    CHECK(!silk::trigger(catchState, 3));
    for (int tick = 0; tick < 360; ++tick) silk::tick(catchState);
    CHECK(silk::ready(catchState, 3));
    CHECK(!boss.bullets.intercept(hit.id));
}

void cancellationClearsCommittedWork() {
    auto boss = makeUnweaver();
    boss.unweaver.forcedAttack = static_cast<int>(uw::Attack::FinalConvergence);
    boss.unweaver.forcedMechanic = static_cast<int>(uw::Mechanic::Hunt);
    boss.unweaver.huntExecuted = true;
    driveToHistory(boss, kPlayer);
    CHECK(driveToWarning(boss, kPlayer));
    CHECK(boss.bullets.reservedCount() > 0);
    root::cancel(boss);
    CHECK(boss.stage == root::Stage::Defeated);
    CHECK(boss.bullets.reservedCount() == 0 && boss.bullets.flyingCount() == 0);
    CHECK(boss.unweaver.rayCount == 0 && boss.unweaver.echo.count == 0);
}

void fourSequentialPhasesAndLethalOverflow() {
    auto boss = makeUnweaver();
    CHECK(runToExperiencePair(boss, kPlayer)); // Real Hunt step plus radial sequence.
    CHECK(boss.unweaver.progress.experiences == 3u);
    const auto lethal = root::applySignature(boss, 90000);
    CHECK(lethal.damage == boss.unweaver.progress.maximum && boss.unweaver.progress.credited == boss.unweaver.progress.maximum);
    auto boundary = uw::finish(boss);
    CHECK(boundary.phaseChanged && !boundary.defeated && boss.phase == 2);

    for (int phase = 2; phase <= 4; ++phase) {
        int guard = 0;
        while (boss.stage == root::Stage::Transition && guard++ < 220) step(boss, kPlayer);
        CHECK(boss.phase == phase && boss.stage == root::Stage::Stalk);
        CHECK(boss.unweaver.progress.vitality() == 0 && !boss.unweaver.progress.defeated);
        CHECK(runToExperiencePair(boss, kPlayer));
        CHECK(boss.unweaver.progress.experiences == 3u);
        boundary = uw::finish(boss);
        if (phase < 4) CHECK(boundary.phaseChanged && !boundary.defeated && boss.phase == phase + 1);
        else CHECK(boundary.defeated && boss.unweaver.progress.defeated && boss.stage == root::Stage::Defeated);
    }
}

void deterministicStyleSelection() {
    auto make = [](float movement, float offense, float response) {
        auto boss = makeUnweaver();
        boss.unweaver.profile.movement = movement;
        boss.unweaver.profile.offense = offense;
        boss.unweaver.profile.responseRate = response;
        boss.phase = boss.unweaver.progress.phase = 3;
        boss.unweaver.style = uw::Style::Predator;
        for (int i = 0; i < 3; ++i) uw::chooseStyle(boss.unweaver, 3);
        return boss.unweaver.style;
    };
    const auto mobileA = make(1.f, 0.f, .1f), mobileB = make(1.f, 0.f, .1f);
    const auto responsiveA = make(0.f, 1.f, 1.f), responsiveB = make(0.f, 1.f, 1.f);
    CHECK(mobileA == mobileB && responsiveA == responsiveB);
    CHECK(mobileA != responsiveA);
}

struct AdmissionCase { const char* name; Point player; };
struct MobilityCase { const char* name; float speed, dashMultiplier; };
int gReplayFailures = 0;
int gMatrixAccepted = 0;
int gMatrixRejected = 0;
int gProtectedProjectileContacts = 0;
int gSelectedDashRoutes = 0;
int gSelectedNoDashRoutes = 0;
int gAcceptedDashReady = 0;
int gAcceptedDashExhausted = 0;

void replayAdmittedRoute(uw::Attack attack, const AdmissionCase& scenario,
                         const MobilityCase& mobility, bool dashReady) {
    auto boss = makeUnweaver();
    boss.phase = boss.unweaver.progress.phase = 2;
    auto& u = boss.unweaver;
    u.playerBounds = kArena;
    u.playerSpeed = mobility.speed;
    u.playerDashMultiplier = mobility.dashMultiplier;
    u.dashCooldown = dashReady ? 0.f : threadlight::kDashCooldownSeconds;
    u.dashRemaining = 0.f;
    u.playerVelocity = {mobility.speed * .55f, mobility.speed * .2f};
    u.forcedAttack = static_cast<int>(attack);
    u.forcedMechanic = static_cast<int>(uw::Mechanic::Echo);
    // Static, prior player history yields a real captured echo at commit.
    for (int i = 0; i < 60; ++i) uw::record(u, scenario.player);
    CHECK(!threadlight::combat::bodyContact(scenario.player,scenario.player,threadlight::layout::kPlayerRadius,
                                             boss.p,boss.p,32.f));
    const bool admitted = uw::commit(boss,scenario.player,kArena);
    if (!admitted) { ++gMatrixRejected; return; }
    ++gMatrixAccepted;
    if (dashReady) ++gAcceptedDashReady; else ++gAcceptedDashExhausted;
    if (u.safeUsesDash) ++gSelectedDashRoutes; else ++gSelectedNoDashRoutes;
    CHECK(u.planCommitted && u.mechanic == uw::Mechanic::Echo && u.echo.count >= 2);
    CHECK(u.safeRoute >= 0 && u.safeRoute < 8);
    CHECK(u.safeUsesDash || dashReady || u.dashCooldown > 0.f);

    Point player = scenario.player;
    Point held = uw::length(u.playerVelocity)>.01f?uw::unit(u.playerVelocity):Point{};
    float dashTime = 0.f;
    float dashCooldown = u.dashCooldown;
    int firstUnsafeTick = -1;
    int unsafeDamage = 0;
    int unsafeProjectileHits = 0;
    // The plan is replayed only up to, but not including, its Recover transition.
    for (int frame = 0; frame < u.sequenceTicks - 1 &&
         (boss.stage == root::Stage::RootWarning || boss.stage == root::Stage::RootActive); ++frame) {
        dashCooldown = std::max(0.f,dashCooldown-static_cast<float>(threadlight::timing::kFixedStepSeconds));
        // Production moves on the committed command using the pre-AI clock,
        // then updates player velocity and ticks the boss/projectiles.
        const int clock = u.clock;
        const Point movement = uw::responseDirection(u,player,clock,u.safeRoute,held);
        held = movement;
        const bool dashPressed = u.safeUsesDash && dashTime <= 0.f && dashCooldown <= 0.f &&
                                 clock % 4 == 0 && clock >= u.warningTicks-12 && clock < u.warningTicks+120;
        if (dashPressed) { dashTime=threadlight::kDashDurationSeconds;dashCooldown=threadlight::kDashCooldownSeconds; }
        const bool dashProtected = threadlight::dashProtectsCollisionThisFrame(dashTime);
        const Point previous = player;
        const float multiplier = dashProtected ? mobility.dashMultiplier : 1.f;
        player = uw::clamp(uw::add(player,uw::scale(movement,mobility.speed*multiplier/120.f)),u.playerBounds);
        u.playerVelocity = uw::scale(uw::sub(player,previous),120.f);
        root::tick(boss,player,kArena);

        const int incoming = root::incomingDamage(boss,previous,player,threadlight::layout::kPlayerRadius);
        const auto impacts = root::projectileTick(boss,previous,player,threadlight::layout::kPlayerRadius,kArena);
        // Dash contacts remain protected by the real game collision gate. Count
        // them separately so they are visible without calling them unsafe hits.
        if (dashProtected) gProtectedProjectileContacts += impacts.count;
        else if (incoming > 0 || impacts.count > 0) {
            if (firstUnsafeTick < 0) firstUnsafeTick = clock;
            unsafeDamage = std::max(unsafeDamage,incoming);
            unsafeProjectileHits += impacts.count;
        }
        if (dashProtected) dashTime=std::max(0.f,dashTime-static_cast<float>(threadlight::timing::kFixedStepSeconds));
        (void)uw::finish(boss);
    }
    if (firstUnsafeTick >= 0) {
        ++gReplayFailures;
        std::cerr << "UNWEAVER replay failure attack=" << uw::attackName(attack)
                  << " position=" << scenario.name << " mobility=" << mobility.name
                  << " dash_ready=" << dashReady << " route=" << u.safeRoute
                  << " uses_dash=" << u.safeUsesDash << " tick=" << firstUnsafeTick
                  << " damage=" << unsafeDamage << " projectile_hits=" << unsafeProjectileHits
                  << " player=" << player.x << ',' << player.y << '\n';
    }
    CHECK(boss.stage != root::Stage::Recover);
}

void admissionAndActualReplayMatrix() {
    constexpr std::array<AdmissionCase,12> positions{{
        {"northwest",{28,28}}, {"northeast",{896,28}}, {"southwest",{28,480}}, {"southeast",{896,480}},
        {"west-edge",{28,254}}, {"east-edge",{896,254}}, {"north-edge",{460,28}}, {"south-edge",{460,480}},
        {"near-east",{515,254}}, {"near-west",{405,254}}, {"near-south",{460,309}}, {"near-north",{460,199}}
    }};
    constexpr std::array<MobilityCase,2> mobility{{
        {"baseline",255.f,3.7f}, {"maximum",315.f,4.6f}
    }};
    constexpr std::array<uw::Attack,5> attacks{{uw::Attack::ShatteredHalo,uw::Attack::RotatingEclipse,
        uw::Attack::FracturedRhythm,uw::Attack::PredictedEscape,uw::Attack::FinalConvergence}};
    std::array<int,5> admittedByFamily{};
    for (std::size_t family=0;family<attacks.size();++family) {
        for (const auto& position:positions) for (const auto& move:mobility) for (const bool dashReady:{false,true}) {
            const int before=gMatrixAccepted;
            replayAdmittedRoute(attacks[family],position,move,dashReady);
            admittedByFamily[family]+=gMatrixAccepted-before;
        }
        std::cout << "admission family=" << uw::attackName(attacks[family])
                  << " accepted=" << admittedByFamily[family] << " of=" << positions.size()*mobility.size()*2 << '\n';
        CHECK(admittedByFamily[family] > 0);
    }
    std::cout << "admission matrix accepted=" << gMatrixAccepted << " rejected=" << gMatrixRejected
              << " accepted_dash_ready=" << gAcceptedDashReady << " accepted_dash_exhausted=" << gAcceptedDashExhausted
              << " selected_dash_routes=" << gSelectedDashRoutes << " selected_no_dash_routes=" << gSelectedNoDashRoutes
              << " replay_scope=radial_echo protected_projectile_contacts=" << gProtectedProjectileContacts
              << " replay_failures=" << gReplayFailures << '\n';
    CHECK(gReplayFailures == 0);
}

struct SyntheticReplay { int unprotectedContacts=0, protectedContacts=0, firstContactTick=-1; };
SyntheticReplay replaySyntheticField(uw::State state, Point start, bool useDash) {
    SyntheticReplay result;
    Point player=start,held=uw::length(state.playerVelocity)>.01f?uw::unit(state.playerVelocity):Point{};
    float dashTime=0.f,dashCooldown=state.dashCooldown;
    const auto& threat=state.externalFields[0];
    for(int tick=0;tick<state.sequenceTicks;++tick){
        dashCooldown=std::max(0.f,dashCooldown-1.f/120.f);
        const Point movement=uw::responseDirection(state,player,tick,state.safeRoute,held);
        held=movement;
        const bool press=useDash&&tick%4==0&&dashTime<=0.f&&dashCooldown<=0.f&&
                         tick>=state.warningTicks-12&&tick<state.warningTicks+120;
        if(press){dashTime=threadlight::kDashDurationSeconds;dashCooldown=threadlight::kDashCooldownSeconds;}
        const bool protectedNow=threadlight::dashProtectsCollisionThisFrame(dashTime);
        const Point previous=player;
        player=uw::clamp(uw::add(player,uw::scale(movement,state.playerSpeed*
                    (protectedNow?state.playerDashMultiplier:1.f)/120.f)),state.playerBounds);
        const bool active=tick>=threat.warning&&tick<threat.lifetime;
        const bool contact=active&&threadlight::combat::silkContact(previous,player,12.f,
                    threat.a,threat.b,threat.width);
        if(contact){
            if(result.firstContactTick<0)result.firstContactTick=tick;
            if(protectedNow)++result.protectedContacts;else ++result.unprotectedContacts;
        }
        if(protectedNow)dashTime=std::max(0.f,dashTime-1.f/120.f);
    }
    return result;
}

void syntheticDashWindowValidatorStress() {
    uw::State base;
    base.playerBounds={kArena.left,kArena.right,kArena.top,kArena.bottom};
    base.playerSpeed=255.f;base.playerDashMultiplier=3.7f;base.playerVelocity={180.f,0.f};
    base.warningTicks=102;base.sequenceTicks=160;base.anchor={460.f,254.f};base.gap={1.f,0.f};
    base.attack=uw::Attack::ShatteredHalo;base.safeRoute=0;
    base.externalFieldCount=1;
    base.externalFields[0]={{kArena.left,254.f},{kArena.right,254.f},2000.f,100,108};
    constexpr Point start{690.f,254.f};

    for(int route=0;route<8;++route)CHECK(!uw::witness(base,start,12.f,route,false));

    auto ready=base;ready.dashCooldown=0.f;
    int selectedRoute=-1;
    for(int route=0;route<8;++route)if(uw::witness(ready,start,12.f,route,true)){selectedRoute=route;break;}
    CHECK(selectedRoute>=0);
    ready.safeRoute=selectedRoute;
    const auto protectedReplay=replaySyntheticField(ready,start,true);
    CHECK(protectedReplay.protectedContacts==8);
    CHECK(protectedReplay.unprotectedContacts==0);

    auto exhausted=base;exhausted.dashCooldown=threadlight::kDashCooldownSeconds;
    for(int route=0;route<8;++route)CHECK(!uw::witness(exhausted,start,12.f,route,true));
    exhausted.safeRoute=selectedRoute;
    const auto rejectedReplay=replaySyntheticField(exhausted,start,true);
    CHECK(rejectedReplay.unprotectedContacts==8&&rejectedReplay.firstContactTick==100);
    std::cout<<"synthetic validator stress=full-arena-capsule window=100..107 dash_route="
             <<selectedRoute<<" protected_contacts="<<protectedReplay.protectedContacts
             <<" exhausted_contacts="<<rejectedReplay.unprotectedContacts<<'\n';
}
}

int main() {
    familyAdmissionAndExecution(uw::Attack::ShatteredHalo);
    familyAdmissionAndExecution(uw::Attack::RotatingEclipse);
    familyAdmissionAndExecution(uw::Attack::FracturedRhythm);
    familyAdmissionAndExecution(uw::Attack::PredictedEscape);
    familyAdmissionAndExecution(uw::Attack::FinalConvergence);
    echoActivationAndHostility();
    strandCutsCorruptionAndExpiry();
    sealDamageAndOrdinaryCooldown();
    actualNeedlecatchAgainstUnweaverShot();
    cancellationClearsCommittedWork();
    fourSequentialPhasesAndLethalOverflow();
    deterministicStyleSelection();
    admissionAndActualReplayMatrix();
    syntheticDashWindowValidatorStress();
    std::cout << "Unweaver headless integration passed: ticks=" << gTicks
              << " attack_families=5 peak_rays=" << gPeakRays << '\n';
}
