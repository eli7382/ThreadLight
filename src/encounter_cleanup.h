#pragma once

#include "area_hazards.h"
#include "attack_coordinator.h"
#include "chapter_boss.h"
#include "ranged_combat.h"
#include "reward_ledger.h"
#include <span>
#include <vector>

namespace threadlight::encounter_cleanup {

// Call only at the authoritative combat end-tick boundary, after all foe and
// projectile loops have finished. Do not call while iterating any supplied
// collection: this operation despawns ledger owners and then clears the foe vector.
// Pickup accounts are already terminal to the enemy ledger and remain collectible.
template<class Foe, class Thread>
inline void cancelHostiles(chapter_boss::State& boss, std::vector<Foe>& foes,
                           ranged::Pool& ordinaryProjectiles, area_hazards::Pool& groundFields,
                           attacks::Coordinator& coordinator, foundation::RewardLedger& rewards,
                           int& supportQueued, std::span<Thread> silk) {
    chapter_boss::clearThreats(boss);
    for (const auto& foe : foes)
        if (foe.rewardId) rewards.despawnEnemy(foe.rewardId);
    foes.clear();
    ordinaryProjectiles.clear();
    groundFields.clear();
    coordinator.clear();
    supportQueued = 0;
    for (auto& knot : silk) knot.corruptedTicks = 0;
}

} // namespace threadlight::encounter_cleanup
