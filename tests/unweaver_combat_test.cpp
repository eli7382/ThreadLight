#include "chapter_boss.h"
#include "crossweave.h"
#include "game_logic.h"
#include "game_layout.h"
#include "needle_interception.h"
#include "player_upgrades.h"
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
    CHECK(state.rayCount == (attack == uw::Attack::FinalConvergence ? 54 : 36));
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
    std::array<uw::Ray, 54> committed{};
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

enum class CorePolicy { OrbitCW, OrbitCCW, PerimeterCW, PerimeterCCW, DashSpam,
                        DashConserve, AggressiveSilk, DefensiveMovement, ReverseAfterCounter };
enum class CoreBuild { Baseline, Offense, Mobility, Defense };
constexpr int kCoreStressTicks=1200; // Ten real seconds at 120 Hz.
constexpr float kCoreDt=1.f/120.f;
constexpr root::Arena kCorePlayerBounds{28.f,896.f,28.f,480.f};
const char* policyName(CorePolicy p){
    switch(p){case CorePolicy::OrbitCW:return "orbit-cw";case CorePolicy::OrbitCCW:return "orbit-ccw";
    case CorePolicy::PerimeterCW:return "perimeter-cw";case CorePolicy::PerimeterCCW:return "perimeter-ccw";
    case CorePolicy::DashSpam:return "dash-spam";case CorePolicy::DashConserve:return "dash-conserve";
    case CorePolicy::AggressiveSilk:return "aggressive-silk";case CorePolicy::DefensiveMovement:return "defensive";
    case CorePolicy::ReverseAfterCounter:return "reverse-after-counter";}return "invalid";
}
const char* buildName(CoreBuild b){switch(b){case CoreBuild::Baseline:return "baseline";case CoreBuild::Offense:return "offense";
    case CoreBuild::Mobility:return "mobility";case CoreBuild::Defense:return "defense";}return "invalid";}
threadlight::upgrades::Ranks coreRanks(CoreBuild b){
    namespace up=threadlight::upgrades;up::Ranks r;
    if(b==CoreBuild::Offense){r.value[up::index(up::Id::Razor)]=5;r.value[up::index(up::Id::Broad)]=3;r.value[up::index(up::Id::Living)]=2;}
    if(b==CoreBuild::Mobility){r.value[up::index(up::Id::Quick)]=5;r.value[up::index(up::Id::Rhythm)]=3;r.value[up::index(up::Id::Longstride)]=3;}
    if(b==CoreBuild::Defense){r.value[up::index(up::Id::Heartwood)]=5;r.value[up::index(up::Id::Ironbark)]=5;
        r.value[up::index(up::Id::Quick)]=2;r.value[up::index(up::Id::Rhythm)]=1;}
    return r;
}
Point coreDirection(Point delta){
    if(uw::length(delta)<.01f)return {};
    const float angle=std::round(uw::angleOf(delta)/(uw::kPi*.25f))*uw::kPi*.25f;return uw::direction(angle);
}
struct CoreOutcome {
    int ticks=0,playerHp=0,bossHp=0,bossDamage=0,playerDamage=0,ordinaryHits=0,dashStarts=0;
    int acceptedPlans=0,counterAttempts=0,counterSuccesses=0,styleChanges=0,styleMask=0,objectiveMask=0,reversed=0;
    std::uint64_t pathHash=0;float movement=0,dashUse=0,perimeter=0,offense=0,counterRate=0,responseRate=0;
    uw::Style style=uw::Style::Predator;uw::Objective objective=uw::Objective::Engage;
};
bool sameCore(const CoreOutcome&a,const CoreOutcome&b){
    return a.ticks==b.ticks&&a.playerHp==b.playerHp&&a.bossHp==b.bossHp&&a.bossDamage==b.bossDamage&&
        a.playerDamage==b.playerDamage&&a.ordinaryHits==b.ordinaryHits&&a.dashStarts==b.dashStarts&&
        a.acceptedPlans==b.acceptedPlans&&a.counterAttempts==b.counterAttempts&&a.counterSuccesses==b.counterSuccesses&&
        a.styleChanges==b.styleChanges&&a.styleMask==b.styleMask&&a.objectiveMask==b.objectiveMask&&a.reversed==b.reversed&&a.pathHash==b.pathHash&&
        a.movement==b.movement&&a.dashUse==b.dashUse&&a.perimeter==b.perimeter&&a.offense==b.offense&&
        a.counterRate==b.counterRate&&a.responseRate==b.responseRate&&a.style==b.style&&a.objective==b.objective;
}

CoreOutcome runRealCore(CorePolicy policy,CoreBuild build,std::uint32_t seed){
    namespace up=threadlight::upgrades;namespace obs=threadlight::observation;
    const auto stats=up::derive(coreRanks(build));threadlight::combat::PlayerHealth playerHealth{stats.maxHp,stats.maxHp,0};
    root::State boss;root::start(boss,{462.f,254.f},kArena,root::Kind::Unweaver);boss.phase=boss.unweaver.progress.phase=2;
    auto& u=boss.unweaver;u.playerBounds=kCorePlayerBounds;u.playerSpeed=stats.moveSpeed;u.playerDashMultiplier=stats.dashMultiplier;
    u.silkActiveTicks=static_cast<int>(stats.activeSilkSeconds*120.f);
    obs::State observations;const float startAngle=seed==0x13579bdfu?.37f:1.19f;const Point center{462.f,254.f};
    Point player=uw::clamp({center.x+210.f*std::cos(startAngle),center.y+155.f*std::sin(startAngle)},kCorePlayerBounds);
    Point previousPlayer=player;float dashTime=0.f,dashCooldown=stats.dashCooldownSeconds;
    std::array<Point,64> silkTrail{};std::array<int,64> silkTick{};int silkCount=1;silkTrail[0]=player;
    int waypoint=static_cast<int>(seed%4u),responseWindow=0,responseMoved=0;bool reversed=false;
    const std::array<Point,4> corners{{{kCorePlayerBounds.left+18,kCorePlayerBounds.top+18},
        {kCorePlayerBounds.right-18,kCorePlayerBounds.top+18},{kCorePlayerBounds.right-18,kCorePlayerBounds.bottom-18},
        {kCorePlayerBounds.left+18,kCorePlayerBounds.bottom-18}}};
    CoreOutcome out;out.pathHash=static_cast<std::uint64_t>(seed);uw::Style priorStyle=u.style;
    for(int tick=0;tick<kCoreStressTicks&&playerHealth.alive()&&!u.progress.defeated;++tick){
        threadlight::combat::tick(playerHealth);dashCooldown=std::max(0.f,dashCooldown-kCoreDt);previousPlayer=player;
        auto orbit=[&](int sign){const Point r=uw::sub(player,center);return coreDirection({-r.y*sign,r.x*sign});};
        Point command{};bool dashPressed=false;
        switch(policy){
        case CorePolicy::OrbitCW:command=orbit(1);break;
        case CorePolicy::OrbitCCW:command=orbit(-1);break;
        case CorePolicy::PerimeterCW:case CorePolicy::PerimeterCCW:{
            const int sign=policy==CorePolicy::PerimeterCW?1:-1;const Point target=corners[static_cast<std::size_t>(waypoint)];
            if(uw::length(uw::sub(target,player))<22.f)waypoint=(waypoint+sign+4)%4;
            command=coreDirection(uw::sub(corners[static_cast<std::size_t>(waypoint)],player));break;}
        case CorePolicy::DashSpam:{const float a=static_cast<float>((tick/90+seed%8u)%8u)*uw::kPi*.25f;command=uw::direction(a);dashPressed=true;break;}
        case CorePolicy::DashConserve:{
            command=orbit(-1);const Point end=uw::clamp(uw::add(player,uw::scale(command,stats.moveSpeed*.2f)),kCorePlayerBounds);
            bool danger=root::incomingDamage(boss,player,end,threadlight::layout::kPlayerRadius)>0||
                ((boss.stage==root::Stage::RootWarning||boss.stage==root::Stage::RootActive)&&boss.remaining<=18);
            for(const auto& shot:boss.bullets.items())if(shot.state==ranged::SlotState::Flying){
                const Point future=uw::add(shot.p,uw::scale(shot.velocity,.16f));
                danger|=threadlight::combat::bodyContact(player,end,threadlight::layout::kPlayerRadius+shot.spec.radius,shot.p,future,0.f);}
            dashPressed=danger;break;}
        case CorePolicy::AggressiveSilk:{const Point toBoss=uw::sub(boss.p,player),side{-toBoss.y,toBoss.x};
            command=coreDirection(uw::add(toBoss,uw::scale(side,.28f*std::sin(tick*.045f))));break;}
        case CorePolicy::DefensiveMovement:{float best=-1.e30f;
            for(int d=0;d<8;++d){const Point candidate=uw::direction(d*uw::kPi*.25f);
                const Point end=uw::clamp(uw::add(player,uw::scale(candidate,stats.moveSpeed*.18f)),kCorePlayerBounds);
                float score=-std::abs(uw::length(uw::sub(end,boss.p))-245.f);
                for(const auto& shot:boss.bullets.items())if(shot.state==ranged::SlotState::Flying){
                    const Point future=uw::add(shot.p,uw::scale(shot.velocity,.18f));
                    if(threadlight::combat::bodyContact(player,end,threadlight::layout::kPlayerRadius+shot.spec.radius,shot.p,future,0.f))score-=10000.f;}
                if(score>best){best=score;command=candidate;}}
            const Point end=uw::clamp(uw::add(player,uw::scale(command,stats.moveSpeed*.18f)),kCorePlayerBounds);
            dashPressed=root::incomingDamage(boss,player,end,threadlight::layout::kPlayerRadius)>0;break;}
        case CorePolicy::ReverseAfterCounter:{command=orbit(reversed?-1:1);
            dashPressed=u.planCommitted&&u.safeUsesDash&&u.clock%4==0&&u.clock>=u.warningTicks-12&&u.clock<u.warningTicks+120;break;}
        }
        if(dashPressed&&uw::length(command)>.01f&&dashCooldown<=0.f){dashTime=threadlight::kDashDurationSeconds;
            dashCooldown=stats.dashCooldownSeconds;++out.dashStarts;}
        const bool dashActive=threadlight::dashProtectsCollisionThisFrame(dashTime);
        player=uw::clamp(uw::add(player,uw::scale(command,stats.moveSpeed*(dashActive?stats.dashMultiplier:1.f)*kCoreDt)),kCorePlayerBounds);
        u.playerVelocity=uw::scale(uw::sub(player,previousPlayer),120.f);
        observations.observe(previousPlayer,player,dashActive,dashPressed&&dashActive,
            {kCorePlayerBounds.left,kCorePlayerBounds.top,kCorePlayerBounds.right,kCorePlayerBounds.bottom});
        u.profile=observations.summary();const int plansBefore=u.plans;
        const auto events=root::tick(boss,player,kArena);
        if(events.warning&&u.planCommitted){responseWindow=36;responseMoved=0;}
        if(responseWindow>0){if(dashActive||uw::length(uw::sub(player,previousPlayer))>.75f)responseMoved=1;
            if(--responseWindow==0)observations.onThreatResponse(responseMoved!=0);}
        if(u.plans!=plansBefore){const int obj=static_cast<int>(u.objective);if(obj>=0&&obj<5)out.objectiveMask|=1<<obj;
            const int style=static_cast<int>(u.style);if(style>=0&&style<3)out.styleMask|=1<<style;
            if(u.style!=priorStyle)++out.styleChanges;priorStyle=u.style;}

        silkCount=std::min(static_cast<int>(silkTrail.size()),silkCount+1);
        for(int i=silkCount-1;i>0;--i){silkTrail[static_cast<std::size_t>(i)]=silkTrail[static_cast<std::size_t>(i-1)];
            silkTick[static_cast<std::size_t>(i)]=silkTick[static_cast<std::size_t>(i-1)];}
        silkTrail[0]=player;silkTick[0]=tick;bool hitBoss=false;
        for(int i=1;i<silkCount;++i){if(tick-silkTick[static_cast<std::size_t>(i)]>static_cast<int>(stats.activeSilkSeconds*120.f))break;
            if(!root::silkContact(boss,silkTrail[static_cast<std::size_t>(i)],silkTrail[static_cast<std::size_t>(i-1)],stats.silkHalfWidth))continue;
            const auto hit=root::applySilk(boss,stats.silkDamage);if(hit.hit!=threadlight::combat::SilkHit::None){hitBoss=true;observations.onOffense(obs::OffenseTarget::Other);}break;}
        out.ordinaryHits+=hitBoss?1:0;

        threadlight::combat::IncomingHit incoming;
        if(!dashActive&&threadlight::combat::canReceiveIncoming(playerHealth,false))
            incoming.consider(root::incomingDamage(boss,previousPlayer,player,threadlight::layout::kPlayerRadius),1);
        const auto impacts=root::projectileTick(boss,previousPlayer,player,threadlight::layout::kPlayerRadius,kArena);
        for(int i=0;i<impacts.count;++i)incoming.consider(impacts.items[static_cast<std::size_t>(i)].damage,
                                                         impacts.items[static_cast<std::size_t>(i)].sourceId);
        const int damage=threadlight::combat::applyIncoming(playerHealth,incoming,dashActive,nullptr,stats.damageReductionPercent);
        if(damage){observations.onIncoming(damage,dashActive);boss.metrics.playerDamage+=damage;out.playerDamage+=damage;
            if(u.planCommitted)u.hitDuringSequence=1;}
        if(u.resultPending){const bool success=u.hitDuringSequence==0;observations.onCounterResult(success);
            ++out.counterAttempts;out.counterSuccesses+=success?1:0;
            if(policy==CorePolicy::ReverseAfterCounter&&success)reversed=true;
            u.resultPending=false;}
        u.profile=observations.summary();
        (void)uw::finish(boss);
        if(dashActive)dashTime=std::max(0.f,dashTime-kCoreDt);

        CHECK(std::isfinite(player.x)&&std::isfinite(player.y)&&player.x>=kCorePlayerBounds.left&&player.x<=kCorePlayerBounds.right&&
              player.y>=kCorePlayerBounds.top&&player.y<=kCorePlayerBounds.bottom);
        CHECK(u.rayCount>=0&&u.rayCount<=static_cast<int>(u.rays.size()));
        CHECK(boss.bullets.flyingCount()<=ranged::kProjectileCapacity&&boss.bullets.reservedCount()<=ranged::kProjectileCapacity);
        for(int i=0;i<u.rayCount;++i){const auto&r=u.rays[static_cast<std::size_t>(i)];CHECK(std::isfinite(r.origin.x)&&std::isfinite(r.origin.y)&&
            std::isfinite(r.direction.x)&&std::isfinite(r.direction.y));}
        for(const auto&p:boss.bullets.items())if(p.state!=ranged::SlotState::Empty)
            CHECK(std::isfinite(p.p.x)&&std::isfinite(p.p.y)&&std::isfinite(p.velocity.x)&&std::isfinite(p.velocity.y));
        out.ticks=tick+1;
    }
    out.playerHp=playerHealth.hp;out.bossHp=boss.health.hp;out.bossDamage=boss.health.maxHp-boss.health.hp;
    out.style=u.style;out.objective=u.objective;
    out.acceptedPlans=static_cast<int>(boss.warnings);out.reversed=reversed?1:0;
    if(out.acceptedPlans==0)
        std::cerr<<"core scenario ended before a plan policy="<<policyName(policy)<<" build="<<buildName(build)<<" seed="<<seed
                 <<" ticks="<<out.ticks<<" player_hp="<<out.playerHp<<'\n';
    out.pathHash^=static_cast<std::uint64_t>(std::lround((player.x+player.y)*1000.f));
    const auto finalProfile=observations.summary();
    out.movement=finalProfile.movement;out.dashUse=finalProfile.dashUse;out.perimeter=finalProfile.perimeter;
    out.offense=finalProfile.offense;out.counterRate=finalProfile.counterSuccess;out.responseRate=finalProfile.responseRate;
    observations.reset();CHECK(observations.movementCount()==0&&observations.summary().confidence==0.f&&observations.summary().offense==0.f);
    return out;
}

void deterministicRealCorePolicyStress(){
    constexpr std::array<CorePolicy,9> policies{{CorePolicy::OrbitCW,CorePolicy::OrbitCCW,CorePolicy::PerimeterCW,
        CorePolicy::PerimeterCCW,CorePolicy::DashSpam,CorePolicy::DashConserve,CorePolicy::AggressiveSilk,
        CorePolicy::DefensiveMovement,CorePolicy::ReverseAfterCounter}};
    constexpr std::array<CoreBuild,4> builds{{CoreBuild::Baseline,CoreBuild::Offense,CoreBuild::Mobility,CoreBuild::Defense}};
    constexpr std::array<std::uint32_t,2> seeds{{0x13579bdfu,0x2468ace0u}};
    int runs=0,ticks=0,plans=0,noPlanRuns=0,bossDamage=0,playerDamage=0,counters=0,styles=0,styleMask=0,reversals=0,objectives=0;
    for(const auto p:policies)for(const auto b:builds)for(const auto seed:seeds){
        const auto first=runRealCore(p,b,seed),again=runRealCore(p,b,seed);
        if(!sameCore(first,again))std::cerr<<"core nondeterminism policy="<<policyName(p)<<" build="<<buildName(b)<<" seed="<<seed<<'\n';
        CHECK(sameCore(first,again));++runs;ticks+=first.ticks;plans+=first.acceptedPlans;bossDamage+=first.bossDamage;
        playerDamage+=first.playerDamage;counters+=first.counterAttempts;styles+=first.styleChanges;styleMask|=first.styleMask;
        reversals+=first.reversed;objectives|=first.objectiveMask;
        noPlanRuns+=first.acceptedPlans==0?1:0;
    }
    CHECK(plans>0&&counters>0&&styleMask!=0&&objectives!=0);
    std::cout<<"real_core_policy_stress seconds=10 policies="<<policies.size()<<" builds="<<builds.size()
        <<" seeds="<<seeds.size()<<" repeated_runs="<<runs<<" ticks="<<ticks<<" accepted_plans="<<plans<<" no_plan_runs="<<noPlanRuns
        <<" boss_damage="<<bossDamage<<" player_damage="<<playerDamage<<" counter_results="<<counters
        <<" style_changes="<<styles<<" styles_seen_mask="<<styleMask<<" objective_mask="<<objectives<<" reversal_events="<<reversals
        <<" victory_claim=none\n";
}
}

void fullCircleAndPositioning(){
 for(int family=0;family<5;++family){uw::State u;uw::makeRays(u,{450,250},(uw::Attack)family,.17f);
  for(int i=0;i<18;++i)CHECK(uw::angleDistance(uw::angleOf(u.rays[i].direction),.17f+i*2*uw::kPi/18)<.001f);
 }
 auto boss=root::State{};boss.p={450,250};auto& u=boss.unweaver;u.playerVelocity={-255,0};
 uw::position(boss,{560,250},kArena,false);CHECK(u.goal.x<450); // Chase is met by spacing, not following.
 u.think=0;u.silkObstacleCount=1;u.silkObstacles[0]={Point{300,140},Point{600,140}};
 uw::position(boss,{560,250},kArena,true);CHECK(u.goal.y>180); // Penalize the dangerous silk side.
}
int main() {
 fullCircleAndPositioning();
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
    deterministicRealCorePolicyStress();
    std::cout << "Unweaver headless integration passed: ticks=" << gTicks
              << " attack_families=5 peak_rays=" << gPeakRays << '\n';
}
