#pragma once

#include <algorithm>
#include <cstdint>

namespace threadlight::signature_effects {

constexpr int kSlipknotSlowTicks = 90;
constexpr int kSlipknotCooldownTicks = 300;
constexpr int kAnchorWindowTicks = 144;
constexpr int kAnchorCooldownTicks = 480;
constexpr float kSlipknotLocomotionFactor = 0.75f;
constexpr float kSlipknotBossStalkFactor = 0.92f;

// Keep one state per enemy. The factor is for locomotion only; callers must
// leave attacks, projectiles and already committed paths on their normal clocks.
struct SlowState {
    int remainingTicks = 0;
    int cooldownTicks = 0;
};

inline void tick(SlowState& state, bool active) {
    if (!active) return;
    state.remainingTicks = std::max(0, state.remainingTicks - 1);
    state.cooldownTicks = std::max(0, state.cooldownTicks - 1);
}

// The caller invokes this only for an actual ordinary silk HP hit. With a
// ready cooldown, Slipknot always applies its 25% locomotion reduction.
inline bool onOrdinaryHit(SlowState& state, bool owned) {
    state.remainingTicks = std::max(0, state.remainingTicks);
    state.cooldownTicks = std::max(0, state.cooldownTicks);
    if (!owned || state.cooldownTicks > 0) return false;
    state.remainingTicks = kSlipknotSlowTicks;
    state.cooldownTicks = kSlipknotCooldownTicks;
    return true;
}

// Apply this only to ordinary pursuit velocity or a boss's Stalk movement.
inline float factor(const SlowState& state, bool boss) {
    if (state.remainingTicks <= 0) return 1.0f;
    return boss ? kSlipknotBossStalkFactor : kSlipknotLocomotionFactor;
}

// One mark is kept until its window expires; hits on a different target do
// not replace it. The cooldown starts with the first positive ordinary hit,
// so an expired or abandoned mark never refunds the cooldown.
struct AnchorState {
    std::uint64_t targetId = 0;
    int windowTicks = 0;
    int cooldownTicks = 0;
};

inline void tick(AnchorState& state, bool active) {
    if (!active) return;
    state.windowTicks = std::max(0, state.windowTicks - 1);
    state.cooldownTicks = std::max(0, state.cooldownTicks - 1);
    if (state.windowTicks == 0) state.targetId = 0;
}

inline bool onOrdinaryHit(AnchorState& state, bool owned, std::uint64_t targetId,
                          int actualHpDamage, bool rewardlessBud = false) {
    state.windowTicks = std::max(0, state.windowTicks);
    state.cooldownTicks = std::max(0, state.cooldownTicks);
    if (!owned || !targetId || actualHpDamage <= 0 || rewardlessBud) return false;
    if (state.windowTicks > 0) {
        if (targetId != state.targetId) return false;
        state.windowTicks = 0;
        state.targetId = 0;
        return true;
    }
    if (state.cooldownTicks > 0) return false;
    state.targetId = targetId;
    state.windowTicks = kAnchorWindowTicks;
    state.cooldownTicks = kAnchorCooldownTicks;
    return false;
}

// Normal targets take base damage; bosses receive one half, rounded half up.
inline int damage(int baseDamage, bool boss) {
    if (baseDamage <= 0) return 0;
    return boss ? baseDamage / 2 + baseDamage % 2 : baseDamage;
}

} // namespace threadlight::signature_effects
