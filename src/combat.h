#pragma once

#include "game_geometry.h"
#include <algorithm>
#include <cstdint>

namespace threadlight::combat {

// Health/damage are integer hundredths. Rendering never writes these records.
constexpr int kSilkHitIntervalTicks = 33; // Existing nominal 0.27 s at 120 Hz.
constexpr int kPlayerImmunityTicks = 94; // Existing nominal 0.78 s at 120 Hz.
enum class EnemyKind { Hunter, Brute, Skitter, Spitter, BriarCaller, Sower };

struct EnemyDefinition { int hp, contactDamage, sparkValue, killScore; float radius, speed; };
inline EnemyDefinition definition(EnemyKind kind, int chapter = 1) {
    if(chapter>=4){
        const bool final=chapter>=5;
        switch(kind){
        case EnemyKind::Hunter:return {final?350:300,final?1400:1300,1,10,14.f,final?135.f:125.f};
        case EnemyKind::Brute:return {final?900:750,final?2400:2200,2,35,20.f,final?80.f:78.f};
        case EnemyKind::Skitter:return {final?350:300,final?1800:1700,1,15,11.f,final?145.f:140.f};
        case EnemyKind::Spitter:return {final?600:500,final?1500:1400,2,30,15.f,final?88.f:84.f};
        case EnemyKind::BriarCaller:return {final?700:600,final?1300:1200,3,45,16.f,final?78.f:72.f};
        case EnemyKind::Sower:return {final?600:500,final?1700:1500,2,30,16.f,final?76.f:64.f};
        }
    }
    if(chapter>=3){
        switch(kind){
        case EnemyKind::Hunter:return {250,1200,1,10,14.f,115.f};
        case EnemyKind::Brute:return {600,2000,2,35,20.f,74.f};
        case EnemyKind::Skitter:return {250,1500,1,15,11.f,135.f};
        case EnemyKind::Spitter:return {400,1300,2,30,15.f,78.f};
        case EnemyKind::BriarCaller:return {500,1100,3,45,16.f,68.f};
        case EnemyKind::Sower:return {500,1500,2,30,16.f,64.f};
        }
    }
    if(chapter>=2){
        switch(kind){
        case EnemyKind::Hunter:return {200,1100,1,10,14.f,105.f};
        case EnemyKind::Brute:return {450,1800,2,35,20.f,70.f};
        case EnemyKind::Skitter:return {200,1300,1,15,11.f,125.f};
        case EnemyKind::Spitter:return {300,1200,2,30,15.f,72.f};
        case EnemyKind::BriarCaller:return {400,1000,3,45,16.f,64.f};
        case EnemyKind::Sower:return {500,1500,2,30,16.f,64.f};
        }
    }
    switch (kind) {
    case EnemyKind::Hunter: return {100, 1000, 1, 10, 14.f, 95.f};
    case EnemyKind::Brute: return {300, 1600, 2, 35, 20.f, 66.f};
    case EnemyKind::Skitter: return {150, 1200, 1, 15, 11.f, 115.f};
    case EnemyKind::Spitter: return {300,1200,2,30,15.f,72.f};
    case EnemyKind::BriarCaller: return {350,1000,3,45,16.f,60.f};
    case EnemyKind::Sower: return {500,1500,2,30,16.f,64.f};
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
inline bool canReceiveIncoming(const PlayerHealth& player, bool dashProtected) {
    return player.alive() && !dashProtected && player.immunityTicks <= 0;
}
inline void tick(EnemyHealth& target) { target.silkCooldownTicks = std::max(0, target.silkCooldownTicks - 1); }
inline void tick(PlayerHealth& target) { target.immunityTicks = std::max(0, target.immunityTicks - 1); }

enum class SilkHit { None, Damaged, Killed };
// One HP/death calculation for every player damage source. Source-specific gates
// are separate: an echo never consumes or resets the ordinary silk cooldown.
inline SilkHit applyDamage(EnemyHealth& target, int damage) {
    if(!target.alive()||damage<=0)return SilkHit::None;
    target.hp=std::max(0,target.hp-damage);
    return target.alive()?SilkHit::Damaged:SilkHit::Killed;
}
inline SilkHit applySilk(EnemyHealth& target, int damage) {
    if(target.silkCooldownTicks>0)return SilkHit::None;
    const auto result=applyDamage(target,damage);
    if(result!=SilkHit::None)target.silkCooldownTicks=kSilkHitIntervalTicks;
    return result;
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
inline int applyIncoming(PlayerHealth& player, IncomingHit hit, bool dashProtected, int* barrierCenti=nullptr,int reductionPercent=0) {
    if (!canReceiveIncoming(player,dashProtected) || hit.damage <= 0 || !hit.sourceId) return 0;
    hit.damage=std::max(1,static_cast<int>((static_cast<std::int64_t>(hit.damage)*(100-std::clamp(reductionPercent,0,24))+50)/100));
    if(barrierCenti){const int absorbed=std::min(std::max(0,*barrierCenti),hit.damage);*barrierCenti-=absorbed;hit.damage-=absorbed;}
    if(hit.damage<=0)return 0; // Absorption grants no additional immunity.
    const int taken = std::min(player.hp, hit.damage);
    player.hp -= taken;
    player.immunityTicks = kPlayerImmunityTicks;
    return taken;
}

} // namespace threadlight::combat
