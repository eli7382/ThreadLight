#pragma once

#include "combat.h"
#include <algorithm>

namespace threadlight::silk_utilities {

struct BloodstitchState {
    int kills = 0;       // Ordinary kills toward the next heal.
    int healedCenti = 0; // Actual healing used from this encounter's cap.
};

inline int onOrdinaryKill(BloodstitchState& state, int rank,
                          combat::PlayerHealth& player, bool ordinary = true) {
    constexpr int killsPerHeal[] = {0, 5, 4, 3};
    constexpr int waveCapCenti[] = {0, 900, 1200, 1500};
    if (!ordinary || !player.alive() || rank <= 0 || rank >= 4) return 0;

    const int threshold = killsPerHeal[rank];
    state.kills = std::max(0, state.kills) % threshold + 1;
    if (state.kills < threshold) return 0;
    state.kills = 0;

    const int capRemaining = std::max(0, waveCapCenti[rank] - std::max(0, state.healedCenti));
    const int missing = std::max(0, player.maxHp - player.hp);
    const int healed = std::min({300, capRemaining, missing});
    if (healed <= 0) return 0;
    player.hp += healed;
    state.healedCenti += healed;
    return healed;
}

struct NeedlecatchState {
    int cooldownTicks = 0;
};

inline void tick(NeedlecatchState& state, bool combat = true) {
    if (combat) state.cooldownTicks = std::max(0, state.cooldownTicks - 1);
}

inline bool ready(const NeedlecatchState& state, int rank) {
    return rank > 0 && rank < 4 && state.cooldownTicks <= 0;
}

inline bool trigger(NeedlecatchState& state, int rank) {
    constexpr int cooldownTicks[] = {0, 600, 480, 360};
    if (!ready(state, rank)) return false;
    state.cooldownTicks = cooldownTicks[rank];
    return true;
}

} // namespace threadlight::silk_utilities
