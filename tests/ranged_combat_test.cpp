#include "ranged_combat.h"

#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

namespace ranged = threadlight::ranged;
using threadlight::foundation::Point;

constexpr ranged::Bounds kWideBounds{0.0f, 2000.0f, 0.0f, 1200.0f};

bool near(float a, float b, float epsilon = 0.001f) {
    return std::abs(a - b) <= epsilon;
}

bool near(Point a, Point b, float epsilon = 0.001f) {
    return near(a.x, b.x, epsilon) && near(a.y, b.y, epsilon);
}

float length(Point vector) {
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

const ranged::Projectile* findTicket(const ranged::Pool& pool,
                                    ranged::Pool::Ticket ticket) {
    for (const auto& item : pool.items())
        if (item.id == ticket && item.state != ranged::SlotState::Empty) return &item;
    return nullptr;
}

void seekUsesRangeBandsAndDefersWithoutShorteningWindup() {
    ranged::State state;
    Point position{500.0f, 500.0f};
    Point velocity{8.0f, 9.0f};

    auto result = ranged::tick(state, position, velocity, {700.0f, 500.0f},
                               kWideBounds, true);
    CHECK(!result.warningStarted && !result.aimLocked && !result.shotReady);
    CHECK(near(position, {500.0f, 500.0f}));
    CHECK(near(velocity, {}));
    CHECK(state.remaining == ranged::kSeekTicks - 1);

    state.remaining = 20;
    ranged::tick(state, position, velocity, {900.0f, 500.0f}, kWideBounds, true);
    CHECK(near(position, {500.6f, 500.0f}));
    CHECK(near(velocity, {ranged::kSeekSpeed, 0.0f}));
    state.remaining = 20;
    ranged::tick(state, position, velocity, {550.0f, 500.0f}, kWideBounds, true);
    CHECK(position.x < 500.6f);
    CHECK(near(velocity, {-ranged::kSeekSpeed, 0.0f}));

    state.remaining = 2;
    position = {500.0f, 500.0f};
    velocity = {10.0f, 10.0f};
    result = ranged::tick(state, position, velocity, {700.0f, 500.0f},
                          kWideBounds, false);
    CHECK(!result.warningStarted);
    CHECK(state.remaining == 1);
    CHECK(!ranged::committed(state));
    CHECK(near(velocity, {}));
    for (int tick = 0; tick < 7; ++tick) {
        result = ranged::tick(state, position, velocity, {700.0f, 500.0f},
                              kWideBounds, false);
        CHECK(!result.warningStarted);
        CHECK(state.remaining == 1);
        CHECK(near(position, {500.0f, 500.0f}));
    }

    for (int tick = 0; tick < 4; ++tick) {
        const Point previous = position;
        result = ranged::tick(state, position, velocity, {900.0f, 500.0f},
                              kWideBounds, false);
        CHECK(!result.warningStarted && !result.aimLocked && !result.shotReady);
        CHECK(state.remaining == 1);
        CHECK(position.x > previous.x);
        CHECK(near(velocity, {ranged::kSeekSpeed, 0.0f}));
        CHECK(!ranged::committed(state));
    }

    result = ranged::tick(state, position, velocity, {700.0f, 500.0f},
                          kWideBounds, true);
    CHECK(result.warningStarted);
    CHECK(!result.aimLocked && !result.shotReady);
    CHECK(ranged::committed(state));
    CHECK(state.phase == ranged::Phase::Windup);
    CHECK(state.remaining == ranged::kWindupTicks);
    CHECK(!state.aimLocked);
    CHECK(near(velocity, {}));

    Point lockedTarget{};
    Point lockedDirection{};
    int cycleTicks = 0;
    for (int elapsed = 1; elapsed <= ranged::kWindupTicks; ++elapsed) {
        ++cycleTicks;
        const Point changingPlayer{700.0f + static_cast<float>(elapsed),
                                   500.0f + static_cast<float>(elapsed * 2)};
        result = ranged::tick(state, position, velocity, changingPlayer,
                              kWideBounds, false);
        CHECK(result.warningStarted == false);
        CHECK(result.aimLocked == (elapsed == ranged::kSoftAimTicks));
        CHECK(result.shotReady == (elapsed == ranged::kWindupTicks));
        CHECK(near(velocity, {}));
        if (elapsed < ranged::kSoftAimTicks) {
            CHECK(!state.aimLocked);
            CHECK(state.remaining == ranged::kWindupTicks - elapsed);
            CHECK(near(state.aimPoint, changingPlayer));
        } else if (elapsed == ranged::kSoftAimTicks) {
            lockedTarget = changingPlayer;
            lockedDirection = state.aimDirection;
            CHECK(state.aimLocked);
            CHECK(state.remaining == ranged::kWindupTicks - elapsed);
            CHECK(near(state.aimPoint, lockedTarget));
        } else if (elapsed < ranged::kWindupTicks) {
            CHECK(state.aimLocked);
            CHECK(state.remaining == ranged::kWindupTicks - elapsed);
            CHECK(near(state.aimPoint, lockedTarget));
            CHECK(near(state.aimDirection, lockedDirection));
        }
    }
    CHECK(!ranged::committed(state));
    CHECK(state.phase == ranged::Phase::Seek);
    CHECK(state.remaining == ranged::kSeekTicks);
    CHECK(!state.aimLocked);
    CHECK(near(state.aimPoint, lockedTarget));
    CHECK(near(state.aimDirection, lockedDirection));

    int seekUpdates = 0;
    while (state.remaining > 1 && seekUpdates < ranged::kSeekTicks) {
        result = ranged::tick(state, position, velocity, {1000.0f, 1000.0f},
                              kWideBounds, true);
        CHECK(!result.warningStarted && !result.aimLocked && !result.shotReady);
        ++seekUpdates;
        ++cycleTicks;
    }
    CHECK(seekUpdates == ranged::kSeekTicks - 1);
    CHECK(ranged::wantsCommit(state));
    result = ranged::tick(state, position, velocity, {1000.0f, 1000.0f},
                          kWideBounds, true);
    ++cycleTicks;
    CHECK(result.warningStarted);
    CHECK(seekUpdates + 1 == ranged::kSeekTicks);
    CHECK(cycleTicks == 432);
}

void seekMovementClampsAtBoundsAndMuzzleOffsetsAlongAim() {
    ranged::State state;
    Point position{0.1f, 50.0f};
    Point velocity{};
    ranged::tick(state, position, velocity, {-100.0f, 50.0f},
                 {0.0f, 100.0f, 0.0f, 100.0f}, true);
    CHECK(near(position, {0.7f, 50.0f}));
    CHECK(near(velocity, {ranged::kSeekSpeed, 0.0f}));

    const Point muzzle = ranged::muzzle({10.0f, 10.0f}, {0.0f, 5.0f});
    CHECK(near(muzzle, {10.0f, 30.0f}));
    CHECK(near(ranged::muzzle({10.0f, 10.0f}, {}), {30.0f, 10.0f}));
}

void poolReservationsCountTowardCapacityAndOnlyCancelBeforeLaunch() {
    ranged::Pool pool;
    CHECK(pool.reserve(0) == 0);
    CHECK(pool.freeSlots() == ranged::kProjectileCapacity);
    CHECK(pool.reservedCount() == 0);
    const auto first = pool.reserve(11);
    CHECK(first != 0);
    CHECK(pool.freeSlots() == ranged::kProjectileCapacity - 1);
    CHECK(pool.reservedCount() == 1);
    CHECK(pool.flyingCount() == 0);
    CHECK(pool.reserve(11) == 0);
    CHECK(pool.freeSlots() == ranged::kProjectileCapacity - 1);

    ranged::Pool::Ticket last = 0;
    for (std::uint64_t source = 12;
         source < 12 + ranged::kProjectileCapacity - 1; ++source)
        last = pool.reserve(source);
    CHECK(pool.reservedCount() == ranged::kProjectileCapacity);
    CHECK(pool.freeSlots() == 0);
    CHECK(pool.reserve(999) == 0);

    CHECK(pool.cancelReservation(first));
    CHECK(!pool.cancelReservation(first));
    CHECK(pool.freeSlots() == 1);
    const auto replacement = pool.reserve(11);
    CHECK(replacement != 0 && replacement != first);
    CHECK(pool.freeSlots() == 0);
    CHECK(pool.launch(replacement, {100.0f, 100.0f}, {3.0f, 4.0f}));
    CHECK(!pool.cancelReservation(replacement));
    CHECK(pool.flyingCount() == 1);
    CHECK(pool.reservedCount() == ranged::kProjectileCapacity - 1);

    const ranged::Projectile* launched = findTicket(pool, replacement);
    CHECK(launched != nullptr);
    if (launched) {
        CHECK(launched->state == ranged::SlotState::Flying);
        CHECK(near(launched->velocity, {108.0f, 144.0f}));
        CHECK(launched->remainingTicks == ranged::kProjectileLifetimeTicks);
        CHECK(launched->sourceId == 11);
    }
    CHECK(!pool.launch(replacement, {100.0f, 100.0f}, {1.0f, 0.0f}));

    // Source death does not own or remove a launched projectile. Only the
    // caller-held reservation is cancellable, and launch consumes that state.
    const auto noHit = pool.tick({1800.0f, 1100.0f}, {1800.0f, 1100.0f},
                                 10.0f, kWideBounds);
    CHECK(noHit.count == 0);
    CHECK(findTicket(pool, replacement) != nullptr);
    CHECK(pool.flyingCount() == 1);
    CHECK(findTicket(pool, replacement)->sourceId == 11);

    pool.clear();
    CHECK(pool.freeSlots() == ranged::kProjectileCapacity);
    CHECK(pool.flyingCount() == 0 && pool.reservedCount() == 0);
    const auto afterClear = pool.reserve(11);
    CHECK(afterClear > last && afterClear > replacement);
}

void sweptRelativeContactConsumesThePhysicalProjectile() {
    ranged::Pool pool;
    const auto ticket = pool.reserve(42);
    CHECK(pool.launch(ticket, {0.0f, 0.0f}, {1.0f, 0.0f}));

    const auto impacts = pool.tick({1.5f, -50.0f}, {1.5f, 50.0f}, 0.0f,
                                   {0.0f, 100.0f, -100.0f, 100.0f});
    CHECK(impacts.count == 1);
    CHECK(impacts.expired == 0);
    CHECK(impacts.items[0].sourceId == 42);
    CHECK(impacts.items[0].projectileId == ticket);
    CHECK(impacts.items[0].damage == 1400);
    CHECK(pool.flyingCount() == 0);
    CHECK(pool.freeSlots() == ranged::kProjectileCapacity);

    const bool immune = true;
    int playerHealth = 1000;
    if (impacts.count > 0 && !immune) playerHealth -= impacts.items[0].damage;
    CHECK(immune);
    CHECK(playerHealth == 1000);
}

void lifetimeExpiresOnTick360ButFinalTickContactWins() {
    const ranged::Bounds arena{0.0f, 10000.0f, 0.0f, 10000.0f};
    ranged::Pool expires;
    const auto ticket = expires.reserve(1);
    CHECK(expires.launch(ticket, {50.0f, 50.0f}, {1.0f, 0.0f}));
    for (int tick = 1; tick < ranged::kProjectileLifetimeTicks; ++tick) {
        const auto impacts = expires.tick({9000.0f, 9000.0f}, {9000.0f, 9000.0f},
                                          0.0f, arena);
        CHECK(impacts.count == 0 && impacts.expired == 0);
    }
    CHECK(expires.flyingCount() == 1);
    const auto lastItem = findTicket(expires, ticket);
    CHECK(lastItem && lastItem->remainingTicks == 1);
    const auto expired = expires.tick({9000.0f, 9000.0f}, {9000.0f, 9000.0f},
                                      0.0f, arena);
    CHECK(expired.count == 0);
    CHECK(expired.expired == 1);
    CHECK(expires.flyingCount() == 0);

    ranged::Pool finalContact;
    const auto finalTicket = finalContact.reserve(2);
    CHECK(finalContact.launch(finalTicket, {50.0f, 50.0f}, {1.0f, 0.0f}));
    for (int tick = 1; tick < ranged::kProjectileLifetimeTicks; ++tick)
        finalContact.tick({9000.0f, 9000.0f}, {9000.0f, 9000.0f}, 0.0f, arena);
    const auto contact = finalContact.tick({590.0f, 50.0f}, {590.0f, 50.0f},
                                           0.0f, arena);
    CHECK(contact.count == 1);
    CHECK(contact.expired == 0);
    CHECK(contact.items[0].projectileId == finalTicket);
    CHECK(finalContact.flyingCount() == 0);

    ranged::Pool leavesArena;
    const auto edgeTicket = leavesArena.reserve(3);
    CHECK(leavesArena.launch(edgeTicket, {9.0f, 50.0f}, {1.0f, 0.0f}));
    const auto outside = leavesArena.tick({900.0f, 900.0f}, {900.0f, 900.0f},
                                         0.0f, {0.0f, 10.0f, 0.0f, 100.0f});
    CHECK(outside.count == 0);
    CHECK(outside.expired == 1);
    CHECK(leavesArena.flyingCount() == 0);
}

void pauseAndRemapPreserveClocksAndTransformCommittedGeometry() {
    ranged::Pool pool;
    const auto ticket = pool.reserve(7);
    const float diagonal = 1.0f / std::sqrt(2.0f);
    CHECK(pool.launch(ticket, {100.0f, 50.0f}, {diagonal, diagonal}));
    auto frozen = pool.tick({1000.0f, 900.0f}, {1000.0f, 900.0f}, 20.0f,
                            {0.0f, 400.0f, 0.0f, 200.0f}, false);
    CHECK(frozen.count == 0 && frozen.expired == 0);
    const auto before = findTicket(pool, ticket);
    CHECK(before && near(before->p, {100.0f, 50.0f}));
    CHECK(before && before->remainingTicks == ranged::kProjectileLifetimeTicks);

    const ranged::Bounds from{0.0f, 400.0f, 0.0f, 200.0f};
    const ranged::Bounds to{0.0f, 800.0f, 0.0f, 600.0f};
    pool.remap(from, to);
    const auto mapped = findTicket(pool, ticket);
    CHECK(mapped && near(mapped->p, {200.0f, 150.0f}));
    CHECK(mapped && near(mapped->previousP, {200.0f, 150.0f}));
    CHECK(mapped && near(length(mapped->velocity), ranged::kProjectileSpeed));
    if (mapped) {
        CHECK(mapped->velocity.x > 0.0f && mapped->velocity.y > 0.0f);
        CHECK(mapped->remainingTicks == ranged::kProjectileLifetimeTicks);
    }
    pool.tick({700.0f, 500.0f}, {700.0f, 500.0f}, 0.0f, to, false);
    CHECK(findTicket(pool, ticket)->remainingTicks == ranged::kProjectileLifetimeTicks);

    ranged::State state;
    state.phase = ranged::Phase::Windup;
    state.remaining = 42;
    state.aimLocked = true;
    Point position{100.0f, 50.0f};
    Point velocity{12.0f, -5.0f};
    state.aimPoint = {200.0f, 50.0f};
    state.aimDirection = {1.0f, 0.0f};
    ranged::remap(state, position, velocity, from, to);
    CHECK(near(position, {200.0f, 150.0f}));
    CHECK(near(state.aimPoint, {400.0f, 150.0f}));
    CHECK(near(state.aimDirection, {1.0f, 0.0f}));
    CHECK(near(velocity, {}));
    CHECK(state.phase == ranged::Phase::Windup);
    CHECK(state.remaining == 42);
    CHECK(state.aimLocked);
}

} // namespace

int main() {
    seekUsesRangeBandsAndDefersWithoutShorteningWindup();
    seekMovementClampsAtBoundsAndMuzzleOffsetsAlongAim();
    poolReservationsCountTowardCapacityAndOnlyCancelBeforeLaunch();
    sweptRelativeContactConsumesThePhysicalProjectile();
    lifetimeExpiresOnTick360ButFinalTickContactWins();
    pauseAndRemapPreserveClocksAndTransformCommittedGeometry();

    if (failures != 0) {
        std::cerr << failures << " ranged combat checks failed\n";
        return 1;
    }
    std::cout << "ranged combat checks passed\n";
    return 0;
}
