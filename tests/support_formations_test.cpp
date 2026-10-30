#include "../src/support_formations.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace sf = threadlight::support_formations;
namespace combat = threadlight::combat;

namespace {
int failures = 0;
void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAIL " << line << ": " << expression << '\n';
        ++failures;
    }
}
#define CHECK(x) check(static_cast<bool>(x), #x, __LINE__)

void checkFiniteConfig(sf::Kind kind, int expectedCount) {
    const auto config = sf::configure(kind, sf::kMaxBudget + 4, sf::kMaxCapacity + 4);
    CHECK(config.memberCount == expectedCount);
    CHECK(config.memberCount <= sf::kMaxMembers);
    CHECK(config.budget == sf::kMaxBudget);
    CHECK(config.capacity == sf::kMaxCapacity);
    for (int i = 0; i < config.memberCount; ++i) {
        CHECK(config.members[i].deployTick >= 0);
        CHECK(std::isfinite(config.members[i].offset.x));
        CHECK(std::isfinite(config.members[i].offset.y));
    }
    CHECK(sf::memberForOrdinal(config, -1) == nullptr);
    CHECK(sf::memberForOrdinal(config, config.budget) == nullptr);
    CHECK(!sf::canRelease(config, 0, config.capacity));
    CHECK(!sf::canRelease(config, config.budget, 0));
    CHECK(sf::canRelease(config, 0, 0));
}

void testTemplateIdentitiesRolesAndTiming() {
    checkFiniteConfig(sf::Kind::RoyalGuard, 2);
    checkFiniteConfig(sf::Kind::PredatorsNet, 2);
    checkFiniteConfig(sf::Kind::ThornDominion, 3);
    checkFiniteConfig(sf::Kind::ShrapnelCage, 2);

    const auto royal = sf::configure(sf::Kind::RoyalGuard, 8, 4);
    CHECK(royal.members[0].kind == combat::EnemyKind::Brute);
    CHECK(royal.members[0].role == sf::Role::Guard);
    CHECK(royal.members[1].kind == combat::EnemyKind::BriarCaller);
    CHECK(royal.members[1].role == sf::Role::Caller);
    CHECK(royal.members[0].offset.x > 0.f && royal.members[1].offset.x < 0.f);
    CHECK(sf::deploymentTick(royal, 0) == 0);
    CHECK(sf::deploymentTick(royal, 1) == 150);
    CHECK(sf::deploymentTick(royal, 2) == sf::kRepeatDelayTicks);
    CHECK(sf::memberForOrdinal(royal, 2)->kind == combat::EnemyKind::Brute);

    const auto predators = sf::configure(sf::Kind::PredatorsNet, 8, 4);
    CHECK(predators.members[0].kind == combat::EnemyKind::Hunter);
    CHECK(predators.members[1].kind == combat::EnemyKind::Skitter);

    const auto thorns = sf::configure(sf::Kind::ThornDominion, 8, 4);
    CHECK(thorns.members[0].kind == combat::EnemyKind::Brute);
    CHECK(thorns.members[1].kind == combat::EnemyKind::BriarCaller);
    CHECK(thorns.members[2].kind == combat::EnemyKind::Sower);

    const auto shrapnel = sf::configure(sf::Kind::ShrapnelCage, 8, 4);
    CHECK(shrapnel.members[0].kind == combat::EnemyKind::Spitter);
    CHECK(shrapnel.members[1].kind == combat::EnemyKind::Spitter);
    CHECK(sf::configure(sf::Kind::ShrapnelCage, 100, 100).budget == sf::kMaxBudget);

    const auto none = sf::configure(sf::Kind::RoyalGuard, -4, -2);
    CHECK(none.budget == 0 && none.capacity == 0);
    CHECK(!sf::canRelease(none, 0, 0));
}

void testEarlierBossWaveOverridesAreFiniteAndOrdered() {
    const auto wave14 = sf::earlierBossWave(14);
    CHECK(wave14.kind == sf::Kind::EarlierWave14);
    CHECK(wave14.memberCount == sf::kMaxMembers);
    CHECK(wave14.budget == 8 && wave14.capacity == 4);
    const combat::EnemyKind expected14[] = {
        combat::EnemyKind::Skitter, combat::EnemyKind::Hunter,
        combat::EnemyKind::Skitter, combat::EnemyKind::Brute,
        combat::EnemyKind::Skitter};
    for (int i = 0; i < wave14.memberCount; ++i)
        CHECK(wave14.members[i].kind == expected14[i]);
    CHECK(wave14.members[0].offset.x == 0.f && wave14.members[0].offset.y == 0.f);
    CHECK(wave14.members[2].offset.x == 0.f && wave14.members[2].offset.y == 0.f);
    CHECK(wave14.members[4].offset.x == 0.f && wave14.members[4].offset.y == 0.f);
    CHECK(sf::deploymentTick(wave14, 7) >= 0);
    CHECK(sf::memberForOrdinal(wave14, 8) == nullptr);

    const auto wave21 = sf::earlierBossWave(21);
    CHECK(wave21.kind == sf::Kind::EarlierWave21);
    CHECK(wave21.memberCount == 4 && wave21.budget == 8 && wave21.capacity == 4);
    const combat::EnemyKind expected21[] = {
        combat::EnemyKind::Brute, combat::EnemyKind::BriarCaller,
        combat::EnemyKind::Hunter, combat::EnemyKind::Skitter};
    for (int i = 0; i < wave21.memberCount; ++i)
        CHECK(wave21.members[i].kind == expected21[i]);

    const auto wave28 = sf::earlierBossWave(28);
    CHECK(wave28.kind == sf::Kind::EarlierWave28);
    CHECK(wave28.memberCount == sf::kMaxMembers);
    CHECK(wave28.budget == 8 && wave28.capacity == 4);
    const combat::EnemyKind expected28[] = {
        combat::EnemyKind::Sower, combat::EnemyKind::Spitter,
        combat::EnemyKind::Brute, combat::EnemyKind::Skitter,
        combat::EnemyKind::BriarCaller};
    for (int i = 0; i < wave28.memberCount; ++i)
        CHECK(wave28.members[i].kind == expected28[i]);
    CHECK(sf::deploymentTick(wave28, 7) >= 0);
    CHECK(sf::memberForOrdinal(wave28, 8) == nullptr);

    CHECK(sf::earlierBossWave(13).memberCount == 0);
    CHECK(sf::earlierBossWave(14).budget <= sf::kMaxBudget);
    CHECK(sf::earlierBossWave(21).budget <= sf::kMaxBudget);
    CHECK(sf::earlierBossWave(28).budget <= sf::kMaxBudget);
}

void testDesiredPositionsKeepGuardBetweenCallerAndPlayer() {
    const auto royal = sf::configure(sf::Kind::RoyalGuard, 2, 2);
    const sf::Point anchor{500.f, 400.f}, player{900.f, 400.f};
    const auto guard = sf::desiredPosition(royal.members[0], anchor, player);
    const auto caller = sf::desiredPosition(royal.members[1], anchor, player);
    CHECK(guard.x > anchor.x);
    CHECK(caller.x < anchor.x);
    CHECK(caller.x < guard.x && guard.x < player.x);

    const auto vertical = sf::desiredPosition(royal.members[0], anchor, {500.f, 900.f});
    CHECK(vertical.y > anchor.y);
    CHECK(std::fabs(vertical.x - anchor.x) < 0.01f);
}

void testSafePlacementHonorsBoundsAndSeparation() {
    const sf::Bounds bounds{0.f, 0.f, 1280.f, 800.f};
    const sf::Point player{640.f, 400.f};
    const auto direct = sf::safePlacement({900.f, 400.f}, bounds, player, 20.f, 100.f);
    CHECK(direct.valid);
    CHECK(direct.point.x >= 20.f && direct.point.x <= 1260.f);
    CHECK(direct.point.y >= 20.f && direct.point.y <= 780.f);
    CHECK(std::hypot(direct.point.x - player.x, direct.point.y - player.y) >= 100.f);

    const auto corrected = sf::safePlacement({641.f, 400.f}, bounds, player, 20.f, 100.f);
    CHECK(corrected.valid);
    CHECK(std::hypot(corrected.point.x - player.x, corrected.point.y - player.y) >= 100.f);

    const auto edge = sf::safePlacement({-500.f, 400.f}, bounds, player, 20.f, 100.f);
    CHECK(edge.valid);
    CHECK(edge.point.x >= 20.f && edge.point.x <= 1260.f);
    CHECK(std::hypot(edge.point.x - player.x, edge.point.y - player.y) >= 100.f);

    const auto impossible = sf::safePlacement({50.f, 50.f}, {0.f, 0.f, 10.f, 10.f},
                                               {5.f, 5.f}, 0.f, 100.f);
    CHECK(!impossible.valid);
    CHECK(!sf::safePlacement({0.f, 0.f}, bounds, player, -1.f, 1.f).valid);
}
} // namespace

int main() {
    testTemplateIdentitiesRolesAndTiming();
    testEarlierBossWaveOverridesAreFiniteAndOrdered();
    testDesiredPositionsKeepGuardBetweenCallerAndPlayer();
    testSafePlacementHonorsBoundsAndSeparation();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Support formation tests passed\n";
    return EXIT_SUCCESS;
}
