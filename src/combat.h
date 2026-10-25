#pragma once

#include "game_geometry.h"
#include <algorithm>
#include <cstdint>

namespace threadlight::combat {

// Health/damage are integer hundredths. Rendering never writes these records.
constexpr int kSilkHitIntervalTicks = 33; // Existing nominal 0.27 s at 120 Hz.
constexpr int kPlayerImmunityTicks = 94; // Existing nominal 0.78 s at 120 Hz.
enum class EnemyKind { Hunter, Brute, Skitter };

struct EnemyDefinition { int hp, contactDamage, sparkValue, killScore; float radius, speed; };
inline EnemyDefinition definition(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Hunter: return {100, 1000, 1, 10, 14.f, 95.f};
    case EnemyKind::Brute: return {300, 1600, 2, 35, 20.f, 66.f};
    case EnemyKind::Skitter: return {150, 1200, 1, 15, 11.f, 115.f};
    }
    return {100, 1000, 1, 10, 14.f, 95.f};
}

struct EnemyHealth {
    int hp = 100, maxHp = 100, silkCooldownTicks = 0;
    bool alive() const { return hp > 0; }
};
struct PlayerHealth {
    int hp = 10000, maxHp = 10000, immunityTicks = 0;
    bool alive() const { return hp > 0; }
};
inline void tick(EnemyHealth& target) { target.silkCooldownTicks = std::max(0, target.silkCooldownTicks - 1); }
inline void tick(PlayerHealth& target) { target.immunityTicks = std::max(0, target.immunityTicks - 1); }

enum class SilkHit { None, Damaged, Killed };
inline SilkHit applySilk(EnemyHealth& target, int damage) {
    if (!target.alive() || target.silkCooldownTicks > 0 || damage <= 0) return SilkHit::None;
    target.hp = std::max(0, target.hp - damage);
    target.silkCooldownTicks = kSilkHitIntervalTicks;
    return target.alive() ? SilkHit::Damaged : SilkHit::Killed;
}

inline bool silkContact(foundation::Point previousEnemy, foundation::Point enemy, float enemyRadius,
                        foundation::Point a, foundation::Point b, float silkHalfWidth) {
    const float radius = std::max(0.f, enemyRadius) + std::max(0.f, silkHalfWidth);
    // Cheap broad phase keeps the existing bounded silk population inexpensive.
    if (std::max(previousEnemy.x, enemy.x) + radius < std::min(a.x, b.x) ||
        std::min(previousEnemy.x, enemy.x) - radius > std::max(a.x, b.x) ||
        std::max(previousEnemy.y, enemy.y) + radius < std::min(a.y, b.y) ||
        std::min(previousEnemy.y, enemy.y) - radius > std::max(a.y, b.y)) return false;
    return foundation::segmentsDistanceSquared(previousEnemy, enemy, a, b) <= radius * radius;
}
inline bool bodyContact(foundation::Point previousPlayer, foundation::Point player, float playerRadius,
                        foundation::Point previousEnemy, foundation::Point enemy, float enemyRadius) {
    const foundation::Point a{previousPlayer.x - previousEnemy.x, previousPlayer.y - previousEnemy.y};
    const foundation::Point b{player.x - enemy.x, player.y - enemy.y};
    const float radius = std::max(0.f, playerRadius) + std::max(0.f, enemyRadius);
    return foundation::segmentDistanceSquared({}, a, b) <= radius * radius;
}

// Gather all live contacts before applying one hit. Container order cannot choose
// a weaker hit and make the player immune to a stronger simultaneous contact.
struct IncomingHit {
    int damage = 0;
    std::uint64_t sourceId = 0;
    void consider(int candidateDamage, std::uint64_t candidateId) {
        if (!candidateId || candidateDamage <= 0) return;
        if (candidateDamage > damage || (candidateDamage == damage && candidateId < sourceId)) {
            damage = candidateDamage; sourceId = candidateId;
        }
    }
};
inline int applyIncoming(PlayerHealth& player, IncomingHit hit, bool dashProtected) {
    if (!player.alive() || dashProtected || player.immunityTicks > 0 || hit.damage <= 0 || !hit.sourceId) return 0;
    const int taken = std::min(player.hp, hit.damage);
    player.hp -= taken;
    player.immunityTicks = kPlayerImmunityTicks;
    return taken;
}

} // namespace threadlight::combat
