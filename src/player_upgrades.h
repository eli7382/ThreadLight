#pragma once

#include "combat.h"
#include "reward_ledger.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace threadlight::upgrades {

enum class Id : int {
    Razor, Living, Broad, Quick, Rhythm, Heartwood, Magnet,
    Ironbark, Longstride, Needlecatch, Bloodstitch, Restitch, Count
};
constexpr int kPermanentCount = 11;
struct Ranks { std::array<int, kPermanentCount> value{}; };
constexpr int index(Id id) { return static_cast<int>(id); }
inline int maximum(Id id) {
    constexpr int caps[] = {5, 3, 3, 5, 3, 5, 3, 5, 3, 3, 3};
    const int i = index(id);
    return i >= 0 && i < kPermanentCount ? caps[i] : 0;
}
inline int rank(const Ranks& ranks, Id id) {
    const int i = index(id);
    return i >= 0 && i < kPermanentCount ? std::clamp(ranks.value[i], 0, maximum(id)) : 0;
}
inline const char* name(Id id) {
    constexpr const char* names[] = {
        "RAZOR FILAMENT", "LIVING THREAD", "BROADWEAVE", "QUICKSTEP", "DASH RHYTHM",
        "HEARTWOOD", "SPARK MAGNET", "IRONBARK", "LONGSTRIDE", "NEEDLECATCH",
        "BLOODSTITCH", "RESTITCH"
    };
    return index(id) >= 0 && index(id) < index(Id::Count) ? names[index(id)] : "";
}

struct Stats {
    int silkDamage = 100, maxHp = 10000;
    float activeSilkSeconds = 1.2f, silkHalfWidth = 5.f;
    float moveSpeed = 255.f, dashCooldownSeconds = 1.85f;
    float pickupRadius = 110.f, pickupPullSpeed = 220.f;
    int damageReductionPercent = 0;
    float dashMultiplier = 3.7f;
    int needleCooldownTicks = 0;
    int bloodKillsPerHeal = 0;
    int bloodWaveCap = 0; // Centi-HP, matching PlayerHealth.
};
inline Stats derive(const Ranks& ranks) {
    constexpr int damage[] = {100, 130, 165, 200, 235, 270};
    constexpr float lifetime[] = {1.2f, 1.5f, 1.8f, 2.1f};
    constexpr float width[] = {5.f, 7.f, 9.f, 11.f};
    constexpr float speed[] = {255.f, 269.f, 282.f, 294.f, 305.f, 315.f};
    constexpr float cooldown[] = {1.85f, 1.70f, 1.55f, 1.40f};
    constexpr float radius[] = {110.f, 145.f, 180.f, 220.f};
    constexpr float pull[] = {220.f, 270.f, 320.f, 380.f};
    constexpr int damageReduction[] = {0, 6, 11, 16, 20, 24};
    constexpr float dashMultiplier[] = {3.7f, 4.f, 4.3f, 4.6f};
    constexpr int needleCooldown[] = {0, 600, 480, 360};
    constexpr int bloodKillsPerHeal[] = {0, 5, 4, 3};
    constexpr int bloodWaveCap[] = {0, 900, 1200, 1500};
    Stats result;
    result.silkDamage = damage[rank(ranks, Id::Razor)];
    result.activeSilkSeconds = lifetime[rank(ranks, Id::Living)];
    result.silkHalfWidth = width[rank(ranks, Id::Broad)];
    result.moveSpeed = speed[rank(ranks, Id::Quick)];
    result.dashCooldownSeconds = cooldown[rank(ranks, Id::Rhythm)];
    result.maxHp += 1200 * rank(ranks, Id::Heartwood);
    result.pickupRadius = radius[rank(ranks, Id::Magnet)];
    result.pickupPullSpeed = pull[rank(ranks, Id::Magnet)];
    result.damageReductionPercent = damageReduction[rank(ranks, Id::Ironbark)];
    result.dashMultiplier = dashMultiplier[rank(ranks, Id::Longstride)];
    result.needleCooldownTicks = needleCooldown[rank(ranks, Id::Needlecatch)];
    result.bloodKillsPerHeal = bloodKillsPerHeal[rank(ranks, Id::Bloodstitch)];
    result.bloodWaveCap = bloodWaveCap[rank(ranks, Id::Bloodstitch)];
    return result;
}

inline bool eligible(Id id, const Ranks& ranks, const combat::PlayerHealth& health,
                     bool economyUseful = true, bool projectilesUseful = true) {
    if (!health.alive()) return false;
    if (id == Id::Restitch) return health.maxHp - health.hp >= 1500;
    if (id == Id::Needlecatch && !projectilesUseful) return false;
    if (maximum(id) == 0 || rank(ranks, id) >= maximum(id)) return false;
    return id != Id::Magnet || economyUseful;
}

struct Offer {
    foundation::CreditId credit = 0;
    std::array<Id, 3> cards{Id::Count, Id::Count, Id::Count};
    int count = 0;
};
struct OfferState {
    std::uint32_t rng = 0x9e3779b9u;
    std::array<int, kPermanentCount> absences{};
    bool recoveryMissed = false;
    Offer current{};
};
inline std::uint32_t nextRandom(OfferState& state) {
    if (!state.rng) state.rng = 0x9e3779b9u;
    auto x = state.rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return state.rng = x;
}
inline bool contains(const Offer& offer, Id id) {
    for (int i = 0; i < offer.count; ++i) if (offer.cards[i] == id) return true;
    return false;
}

// Exactly one generation per credit. Redraw, settings, and queued input cannot
// reroll an already shown offer. Subsequent credits use the updated ranks/health.
inline const Offer& prepareOffer(OfferState& state, const Ranks& ranks,
                                 const combat::PlayerHealth& health, foundation::CreditId credit,
                                 bool economyUseful = true, bool beforeBoss = false,
                                 bool projectilesUseful = true) {
    if (!credit || credit <= state.current.credit) return state.current;
    Offer offer; offer.credit = credit;
    int longest = 2;
    std::array<Id, kPermanentCount> overdue{}; int overdueCount = 0;
    for (int i = 0; i < kPermanentCount; ++i) {
        const Id id = static_cast<Id>(i);
        if (!eligible(id, ranks, health, economyUseful, projectilesUseful) || rank(ranks, id) == 0 || state.absences[i] < 3) continue;
        if (state.absences[i] > longest) { longest = state.absences[i]; overdueCount = 0; }
        if (state.absences[i] == longest) overdue[overdueCount++] = id;
    }
    if (overdueCount) offer.cards[offer.count++] = overdue[nextRandom(state) % overdueCount];
    const int missing = health.maxHp - health.hp;
    if (missing >= 3500 && (state.recoveryMissed || beforeBoss) &&
        eligible(Id::Restitch, ranks, health, economyUseful, projectilesUseful)) offer.cards[offer.count++] = Id::Restitch;
    while (offer.count < 3) {
        std::array<Id, index(Id::Count)> candidates{};
        std::array<int, index(Id::Count)> weights{};
        int count = 0, totalWeight = 0;
        bool ownedPresent = false, newPresent = false;
        for (int i = 0; i < offer.count; ++i) {
            if (offer.cards[i] == Id::Restitch) continue;
            (rank(ranks, offer.cards[i]) > 0 ? ownedPresent : newPresent) = true;
        }
        bool discoveryAvailable = false;
        if (offer.count == 2 && ownedPresent && !newPresent) {
            for (int i = 0; i < kPermanentCount; ++i) {
                const auto id = static_cast<Id>(i);
                discoveryAvailable |= eligible(id, ranks, health, economyUseful, projectilesUseful) && rank(ranks, id) == 0 && !contains(offer, id);
            }
        }
        for (int i = 0; i < index(Id::Count); ++i) {
            const auto id = static_cast<Id>(i);
            if (contains(offer, id) || !eligible(id, ranks, health, economyUseful, projectilesUseful)) continue;
            if (discoveryAvailable && (id == Id::Restitch || rank(ranks, id) != 0)) continue;
            int weight = id == Id::Restitch ? (missing < 3500 ? 15000 : missing < 6000 ? 30000 : 40000)
                                           : maximum(id) == 5 ? 11500 : 10000;
            if (id != Id::Restitch && rank(ranks, id) > 0) weight = weight * 135 / 100;
            candidates[count] = id; weights[count++] = weight; totalWeight += weight;
        }
        if (!count) break;
        int draw = static_cast<int>(nextRandom(state) % totalWeight);
        for (int i = 0; i < count; ++i) {
            if (draw < weights[i]) { offer.cards[offer.count++] = candidates[i]; break; }
            draw -= weights[i];
        }
    }
    for (int i = 0; i < kPermanentCount; ++i)
        state.absences[i] = contains(offer, static_cast<Id>(i)) ? 0 : std::min(1000, state.absences[i] + 1);
    state.recoveryMissed = missing >= 3500 && !contains(offer, Id::Restitch);
    state.current = offer;
    return state.current;
}

inline bool resolveChoice(foundation::RewardLedger& ledger, foundation::Phase phase, const Offer& offer,
                          int choice, Ranks& ranks, combat::PlayerHealth& health,
                          bool economyUseful = true, bool projectilesUseful = true) {
    if (choice < 0 || choice >= offer.count || choice >= 3 || offer.count > 3 ||
        !eligible(offer.cards[choice], ranks, health, economyUseful, projectilesUseful)) return false;
    if (!ledger.resolveCredit(offer.credit, phase)) return false;
    const Id id = offer.cards[choice];
    if (id == Id::Restitch) health.hp = std::min(health.maxHp, health.hp + 3500);
    else {
        const int oldMaximum = derive(ranks).maxHp;
        ranks.value[index(id)] = rank(ranks, id) + 1;
        health.maxHp = derive(ranks).maxHp;
        health.hp = std::min(health.maxHp, health.hp + health.maxHp - oldMaximum);
    }
    return true;
}

// Development-harness overflow only: explicitly discard a credit when the whole
// subset is exhausted. Never substitute an unusable heal or silently consume it.
inline bool resolveExhaustedCredit(foundation::RewardLedger& ledger, foundation::Phase phase,
                                   const Offer& offer, const Ranks& ranks,
                                   const combat::PlayerHealth& health, bool economyUseful = true,
                                   bool projectilesUseful = true) {
    if (offer.count != 0 || !health.alive()) return false;
    for (int i = 0; i < index(Id::Count); ++i)
        if (eligible(static_cast<Id>(i), ranks, health, economyUseful, projectilesUseful)) return false;
    return ledger.resolveCredit(offer.credit, phase);
}

inline std::string effectText(Id id, const Ranks& ranks, const combat::PlayerHealth& health) {
    const Stats before = derive(ranks);
    Ranks next = ranks;
    if (maximum(id) > 0) next.value[index(id)] = std::min(maximum(id), rank(ranks, id) + 1);
    const Stats after = derive(next);
    char text[160]{};
    switch (id) {
    case Id::Razor: std::snprintf(text, sizeof(text), "Silk damage %.2f -> %.2f", before.silkDamage / 100.f, after.silkDamage / 100.f); break;
    case Id::Living: std::snprintf(text, sizeof(text), "Active silk %.2fs -> %.2fs", before.activeSilkSeconds, after.activeSilkSeconds); break;
    case Id::Broad: std::snprintf(text, sizeof(text), "Silk half-width %.0f -> %.0f", before.silkHalfWidth, after.silkHalfWidth); break;
    case Id::Quick: std::snprintf(text, sizeof(text), "Move speed %.0f -> %.0f", before.moveSpeed, after.moveSpeed); break;
    case Id::Rhythm: std::snprintf(text, sizeof(text), "Dash cooldown %.2fs -> %.2fs", before.dashCooldownSeconds, after.dashCooldownSeconds); break;
    case Id::Heartwood: std::snprintf(text, sizeof(text), "Max vitality %d -> %d; gain 12 HP", before.maxHp / 100, after.maxHp / 100); break;
    case Id::Magnet: std::snprintf(text, sizeof(text), "Attract %.0f -> %.0f; pull %.0f -> %.0f", before.pickupRadius, after.pickupRadius, before.pickupPullSpeed, after.pickupPullSpeed); break;
    case Id::Ironbark: std::snprintf(text, sizeof(text), "Reduce incoming damage %d%% -> %d%%", before.damageReductionPercent, after.damageReductionPercent); break;
    case Id::Longstride: std::snprintf(text, sizeof(text), "Dash distance %.1fx -> %.1fx", before.dashMultiplier, after.dashMultiplier); break;
    case Id::Needlecatch:
        if (before.needleCooldownTicks == 0)
            std::snprintf(text, sizeof(text), "Active silk catches 1 shot; %.1fs cooldown", after.needleCooldownTicks / 120.f);
        else
            std::snprintf(text, sizeof(text), "Projectile cooldown %.1fs -> %.1fs",
                before.needleCooldownTicks / 120.f, after.needleCooldownTicks / 120.f);
        break;
    case Id::Bloodstitch:
        if(before.bloodKillsPerHeal==0)std::snprintf(text,sizeof(text),"Ordinary silk: heal 3 HP per %d kills; cap %d HP/wave",after.bloodKillsPerHeal,after.bloodWaveCap/100);
        else std::snprintf(text,sizeof(text),"3 HP per %d -> %d silk kills; cap %d -> %d HP/wave",before.bloodKillsPerHeal,after.bloodKillsPerHeal,before.bloodWaveCap/100,after.bloodWaveCap/100);
        break;
    case Id::Restitch: std::snprintf(text, sizeof(text), "Restore %.0f HP (up to 35)", std::min(3500, health.maxHp - health.hp) / 100.f); break;
    default: break;
    }
    return text;
}

} // namespace threadlight::upgrades
