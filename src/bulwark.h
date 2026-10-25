#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace threadlight::bulwark {

constexpr int kGuardTicks = 288;
constexpr int kExposureTicks = 192;

struct State {
    int remainingTicks = kGuardTicks;
    bool guarded = true;
};

inline void clear(State& state) {
    state = State{};
}

// Guard and exposure repeat as a bounded two-phase cycle. Paused and
// noncombat updates do not consume phase time.
inline void tick(State& state, bool combat) {
    if (!combat) return;
    if (state.remainingTicks > 0) --state.remainingTicks;
    if (state.remainingTicks > 0) return;

    state.guarded = !state.guarded;
    state.remainingTicks = state.guarded ? kGuardTicks : kExposureTicks;
}

// Apply guard/exposure and an optional source multiplier as one rational
// product, then round only once to nearest with positive halves upward. The
// quotient decomposition keeps 32-bit configuration factors within int64_t.
inline int damage(int baseCenti,
                  const State& state,
                  int sourceNumerator = 1,
                  int sourceDenominator = 1) {
    if (baseCenti <= 0 || sourceNumerator <= 0 || sourceDenominator <= 0) return 0;

    const std::int64_t guardNumerator = state.guarded ? 3 : 5;
    const std::int64_t guardDenominator = state.guarded ? 5 : 4;
    const std::int64_t denominator = static_cast<std::int64_t>(sourceDenominator) * guardDenominator;
    const std::int64_t sourceProduct = static_cast<std::int64_t>(baseCenti) * sourceNumerator;
    const std::int64_t whole = sourceProduct / denominator;
    const std::int64_t remainder = sourceProduct % denominator;

    if (whole > std::numeric_limits<int>::max() / guardNumerator) {
        return std::numeric_limits<int>::max();
    }
    const std::int64_t wholeAfterGuard = whole * guardNumerator;
    const std::int64_t partial = remainder * guardNumerator;
    const std::int64_t roundedPartial = (partial + denominator / 2) / denominator;
    const std::int64_t result = wholeAfterGuard + roundedPartial;
    return static_cast<int>(std::min<std::int64_t>(result, std::numeric_limits<int>::max()));
}

} // namespace threadlight::bulwark
