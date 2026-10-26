#pragma once

#include "signature_reward.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace threadlight::signature_catalog {

enum class Id : std::uint8_t {
    Afterimage = 0, Crossweave = 1, Cocoon = 2,
    ResonantLoom = 3, Slipknot = 4, AnchorStitch = 5, Count = 6
};
enum class Category : std::uint8_t { Offensive, Utility, Invalid };

constexpr std::size_t kCount = static_cast<std::size_t>(Id::Count);
constexpr std::size_t kOfferSize = 3;

struct Offer {
    std::uint64_t bossSource = 0;
    std::array<Id, kOfferSize> choices{};
    int count = 0;
};

struct State {
    std::array<bool, kCount> owned{};
    std::uint32_t rng = 0x6d2b79f5u;
    std::uint64_t lastPreparedBossSource = 0;
    Offer current{};
    bool pending = false;
};

inline constexpr std::array<Id, 4> kOffensive{
    Id::Afterimage, Id::Crossweave, Id::ResonantLoom, Id::AnchorStitch};
inline constexpr std::array<Id, 2> kUtility{Id::Cocoon, Id::Slipknot};

inline constexpr std::size_t index(Id id) { return static_cast<std::size_t>(id); }
inline constexpr bool valid(Id id) { return index(id) < kCount; }

inline Category category(Id id) {
    if (!valid(id)) return Category::Invalid;
    return id == Id::Cocoon || id == Id::Slipknot ? Category::Utility : Category::Offensive;
}

inline std::string_view name(Id id) {
    switch (id) {
    case Id::Afterimage: return "Afterimage";
    case Id::Crossweave: return "Crossweave";
    case Id::Cocoon: return "Cocoon";
    case Id::ResonantLoom: return "Resonant Loom";
    case Id::Slipknot: return "Slipknot";
    case Id::AnchorStitch: return "Anchor Stitch";
    case Id::Count: break;
    }
    return "Unknown";
}

inline std::array<std::string_view, 4> descriptionLines(Id id) {
    switch (id) {
    case Id::Afterimage:
        return {"Dash leaves a damaging echo.", "Each enemy can be hit once.",
                "Echo lasts 0.65 seconds.", "Path width: 16 units."};
    case Id::Crossweave:
        return {"Cross older silk for a pulse.", "6 targets / 2.5s cooldown.",
                "Travel: 160 units.", "Pulse radius: 110 units."};
    case Id::Cocoon:
        return {"Close a simple silk loop.", "12 HP barrier / lasts 5s.",
                "240 travel / 3500 loop area.", "Cooldown: 8 seconds."};
    case Id::ResonantLoom:
        return {"Every fifth ordinary hit bursts.", "Up to 3 nearby enemies.",
                "Range: 180 units.", "Cooldown: 2 seconds."};
    case Id::Slipknot:
        return {"Ordinary hits slow target.", "25% slower for 0.75 seconds.",
                "Boss Stalk: 8% slower.", "Per target: 2.5s cooldown."};
    case Id::AnchorStitch:
        return {"Hit the same enemy twice.", "Complete within 1.2 seconds.",
                "Cooldown: 4 seconds.", "Boss damage is halved."};
    case Id::Count: break;
    }
    return {"Unavailable", "", "", ""};
}

inline std::uint32_t nextRandom(State& state) {
    if (state.rng == 0) state.rng = 0x6d2b79f5u;
    std::uint32_t x = state.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state.rng = x;
    return x;
}

inline bool contains(const Offer& offer, Id id) {
    for (int i = 0; i < offer.count; ++i) if (offer.choices[static_cast<std::size_t>(i)] == id) return true;
    return false;
}

inline bool owns(const State& state, Id id) {
    return valid(id) && state.owned[index(id)];
}

inline int ownedCount(const State& state) {
    int result = 0;
    for (bool owned : state.owned) result += owned ? 1 : 0;
    return result;
}

// One offer is generated per increasing, stable boss reward source. Repeated
// presentation calls return the same pending card order without advancing RNG.
inline Offer prepare(State& state, std::uint64_t bossSource) {
    if (state.pending) return state.current;
    if (!bossSource || bossSource <= state.lastPreparedBossSource || ownedCount(state) >= static_cast<int>(kCount))
        return {};

    std::array<Id, kCount> available{};
    std::size_t availableCount = 0;
    for (std::size_t i = 0; i < kCount; ++i)
        if (!state.owned[i]) available[availableCount++] = static_cast<Id>(i);
    if (availableCount == 0) return {};

    Offer offer;
    offer.bossSource = bossSource;
    auto addCategory = [&](Category wanted) {
        std::array<Id, kCount> candidates{};
        std::size_t candidateCount = 0;
        for (std::size_t i = 0; i < availableCount; ++i) {
            const Id id = available[i];
            if (category(id) == wanted && !contains(offer, id)) candidates[candidateCount++] = id;
        }
        if (candidateCount == 0 || offer.count == static_cast<int>(kOfferSize)) return;
        const Id selected = candidates[nextRandom(state) % candidateCount];
        offer.choices[static_cast<std::size_t>(offer.count++)] = selected;
    };

    addCategory(Category::Offensive);
    addCategory(Category::Utility);
    while (offer.count < static_cast<int>(kOfferSize) &&
           offer.count < static_cast<int>(availableCount)) {
        std::array<Id, kCount> candidates{};
        std::size_t candidateCount = 0;
        for (std::size_t i = 0; i < availableCount; ++i)
            if (!contains(offer, available[i])) candidates[candidateCount++] = available[i];
        if (candidateCount == 0) break;
        offer.choices[static_cast<std::size_t>(offer.count++)] = candidates[nextRandom(state) % candidateCount];
    }
    for (int i = offer.count - 1; i > 0; --i) {
        const int other = static_cast<int>(nextRandom(state) % static_cast<std::uint32_t>(i + 1));
        std::swap(offer.choices[static_cast<std::size_t>(i)], offer.choices[static_cast<std::size_t>(other)]);
    }

    state.current = offer;
    state.pending = true;
    state.lastPreparedBossSource = bossSource;
    return state.current;
}

// The legacy reward remains authoritative for phase, survival and FIFO credit
// order. Catalog ownership changes only after every catalog-side check passes.
inline bool accept(State& state, signature_reward::RewardState& legacyReward,
                   foundation::Phase phase, bool playerAlive,
                   std::uint64_t pendingOrdinaryCredits, int choice) {
    if (!playerAlive) {
        (void)signature_reward::accept(legacyReward, phase, false, pendingOrdinaryCredits);
        state.pending = false;
        return false;
    }
    if (phase != foundation::Phase::PendingRewards || pendingOrdinaryCredits != 0 ||
        !state.pending || !legacyReward.pending || legacyReward.owned ||
        legacyReward.source == 0 || legacyReward.source != state.current.bossSource ||
        choice < 0 || choice >= state.current.count || choice >= static_cast<int>(kOfferSize)) return false;

    const Id selected = state.current.choices[static_cast<std::size_t>(choice)];
    if (!valid(selected) || state.owned[index(selected)]) return false;
    if (!signature_reward::accept(legacyReward, phase, true, pendingOrdinaryCredits)) return false;
    state.owned[index(selected)] = true;
    state.pending = false;
    return true;
}

} // namespace threadlight::signature_catalog
