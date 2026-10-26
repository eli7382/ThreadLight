#pragma once

#include "wave_foundation.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace threadlight::foundation {

using RewardId = std::uint64_t;
using CreditId = std::uint64_t;

struct RewardCounters {
    std::uint64_t spawned{}, killed{}, despawned{};
    std::uint64_t pickupsCreated{}, pickupsCollected{}, pickupsExpired{};
    std::uint64_t sparksCreated{}, sparksCollected{}, sparksExpired{};
    std::uint64_t creditsIssued{}, creditsResolved{};
};

struct KillReward { RewardId id; int sparkValue; };

// Run-owned, independent of rendering and wave scheduling. Each registered enemy
// owns at most one valued pickup. IDs are monotonic within a run; zero is invalid.
// Removal cannot be confused with defeat. Counts retain terminal records until
// run reset, making retries of death/pickup/expiry events harmless.
class RewardLedger {
public:
    RewardId registerEnemy(int sparkValue, int killScore) {
        if (sparkValue <= 0 || killScore < 0) return 0;
        enemies_.push_back({State::Alive, sparkValue, killScore});
        ++counters_.spawned;
        return enemies_.size();
    }

    std::optional<KillReward> defeatEnemy(RewardId id) {
        auto* enemy = find(id);
        if (!enemy || enemy->state != State::Alive) return {};
        enemy->state = State::Pickup;
        ++counters_.killed;
        ++counters_.pickupsCreated;
        counters_.sparksCreated += enemy->value;
        score_ += enemy->score;
        return KillReward{id, enemy->value};
    }

    bool despawnEnemy(RewardId id) {
        auto* enemy = find(id);
        if (!enemy || enemy->state != State::Alive) return false;
        enemy->state = State::Despawned;
        ++counters_.despawned;
        return true;
    }

    // Final victory records the kill/score once without creating an unusable
    // pickup. It cannot be confused with rewardless cleanup despawning.
    bool defeatEnemyWithoutPickup(RewardId id) {
        auto* enemy=find(id);
        if(!enemy||enemy->state!=State::Alive)return false;
        enemy->state=State::Defeated;++counters_.killed;score_+=enemy->score;
        return true;
    }

    // Called ONLY after the authoritative pickup-contact helper reports contact.
    bool collectPickup(RewardId id) {
        auto* enemy = find(id);
        if (!enemy || enemy->state != State::Pickup) return false;
        enemy->state = State::Collected;
        ++counters_.pickupsCollected;
        counters_.sparksCollected += enemy->value;
        score_ += 5ULL * enemy->value;
        remainder_ += enemy->value;
        while (remainder_ >= nextThreshold()) {
            remainder_ -= nextThreshold();
            ++counters_.creditsIssued;
        }
        return true;
    }

    bool expirePickup(RewardId id) {
        auto* enemy = find(id);
        if (!enemy || enemy->state != State::Pickup) return false;
        enemy->state = State::Expired;
        ++counters_.pickupsExpired;
        counters_.sparksExpired += enemy->value;
        return true;
    }

    bool resolveCredit(CreditId id, Phase phase) {
        if (phase != Phase::PendingRewards || id == 0 || id != nextCredit()) return false;
        ++counters_.creditsResolved;
        return true;
    }

    int nextThreshold() const {
        switch (counters_.creditsIssued) {
        case 0: return 20;
        case 1: return 30;
        case 2: return 40;
        default: return 48;
        }
    }
    CreditId nextCredit() const { return pendingCredits() ? counters_.creditsResolved + 1 : 0; }
    std::uint64_t pendingCredits() const { return counters_.creditsIssued - counters_.creditsResolved; }
    std::int64_t remainder() const { return remainder_; }
    std::uint64_t score() const { return score_; }
    const RewardCounters& counters() const { return counters_; }

private:
    enum class State { Alive, Pickup, Collected, Expired, Despawned, Defeated };
    struct Account { State state; int value; int score; };
    Account* find(RewardId id) { return id && id <= enemies_.size() ? &enemies_[id - 1] : nullptr; }
    std::vector<Account> enemies_;
    RewardCounters counters_{};
    std::int64_t remainder_ = 0;
    std::uint64_t score_ = 0;
};

} // namespace threadlight::foundation
