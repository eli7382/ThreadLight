#pragma once

#include "wave_foundation.h"

#include <cstdint>

namespace threadlight::signature_reward {

struct RewardState {
    std::uint64_t source = 0;
    bool pending = false;
    bool owned = false;
};

// A nonzero boss reward can be queued once, and only while the player survives.
inline bool queue(RewardState& state, std::uint64_t bossRewardId, bool playerAlive) {
    if (!playerAlive || bossRewardId == 0 || state.source != 0 ||
        state.pending || state.owned) return false;
    state.source = bossRewardId;
    state.pending = true;
    return true;
}

// Ordinary credits retain FIFO priority. Core pickup state is intentionally
// absent: accepting the signature depends only on the pending reward boundary.
inline bool accept(RewardState& state, foundation::Phase phase, bool playerAlive,
                   std::uint64_t pendingOrdinaryCredits) {
    if (!playerAlive) {
        state.pending = false;
        state.owned = false;
        return false;
    }
    if (phase != foundation::Phase::PendingRewards || !state.pending ||
        state.source == 0 || state.owned || pendingOrdinaryCredits != 0) return false;
    state.pending = false;
    state.owned = true;
    return true;
}

} // namespace threadlight::signature_reward
