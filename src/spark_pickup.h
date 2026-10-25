#pragma once

#include "reward_ledger.h"
#include "game_geometry.h"
#include <algorithm>
#include <cmath>

namespace threadlight::foundation {

enum class PickupResult { None, Collected, Expired };
struct PickupState {
    RewardId id = 0;
    int combatAgeTicks = 0;
    bool attracted = false;
    float pullSpeed = 220.0f;
    PickupResult terminal = PickupResult::None;
};

// One 120 Hz tick. active=false freezes motion, contact, and age. combat=false
// permits safe collection without expiry. Contact wins on the last lifetime tick.
// The caller preserves this state and position across encounter transitions.
inline PickupResult tickPickup(PickupState& state, Point& position,
                               Point previousPlayer, Point currentPlayer,
                               float playerRadius, bool combat, bool active,
                               float attractionRadius = 110.f, float initialPullSpeed = 220.f) {
    if (!active || !state.id || state.terminal != PickupResult::None) return PickupResult::None;
    const Point start = position;
    if (segmentDistanceSquared(position, previousPlayer, currentPlayer) <= attractionRadius * attractionRadius)
        state.attracted = true;
    if (state.attracted) {
        const float base = std::max(1.f, initialPullSpeed);
        state.pullSpeed = std::clamp(state.pullSpeed + base / (0.4f * kTicksPerSecond), base, 2.f * base);
        const float dx = currentPlayer.x - position.x, dy = currentPlayer.y - position.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance > 0.f) {
            const float fraction = std::min(1.f, state.pullSpeed / (kTicksPerSecond * distance));
            position.x += dx * fraction;
            position.y += dy * fraction;
        }
    }
    const Point relativeStart{start.x - previousPlayer.x, start.y - previousPlayer.y};
    const Point relativeEnd{position.x - currentPlayer.x, position.y - currentPlayer.y};
    const float contactRadius = std::max(0.f, playerRadius) + 12.f;
    if (segmentDistanceSquared({}, relativeStart, relativeEnd) <= contactRadius * contactRadius) {
        state.terminal = PickupResult::Collected;
        return state.terminal;
    }
    if (combat && ++state.combatAgeTicks >= kPickupLifetimeTicks) {
        state.terminal = PickupResult::Expired;
        return state.terminal;
    }
    return PickupResult::None;
}

} // namespace threadlight::foundation
