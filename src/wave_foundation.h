#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace threadlight::foundation {

constexpr int kTicksPerSecond = 120;
constexpr int kCountdownTicks = 90;
constexpr int kCleanupTicks = 1440;
constexpr int kNormalCollectionTicks = 480;
constexpr int kBossCollectionTicks = 600;
constexpr int kPickupLifetimeTicks = 2160;

enum class Phase { Countdown, Spawning, Cleanup, Collection, PendingRewards, Complete };

inline const char* phaseName(Phase phase) {
    switch (phase) {
    case Phase::Countdown: return "countdown";
    case Phase::Spawning: return "spawning";
    case Phase::Cleanup: return "cleanup";
    case Phase::Collection: return "collection";
    case Phase::PendingRewards: return "pending_rewards";
    case Phase::Complete: return "complete";
    }
    return "invalid";
}

inline bool isCombat(Phase phase) {
    return phase == Phase::Spawning || phase == Phase::Cleanup;
}

struct EncounterSpec {
    int spawningTicks = 2040;
    bool boss = false;
    int countdownTicks = kCountdownTicks;
};

struct WaveEvents {
    bool enteredCollection = false;
    bool enteredPendingRewards = false;
    bool timeoutDespawn = false;
    bool bossDefeated = false;
};

// Call once AFTER the current tick's combat/pickups resolve. The tick's clocks
// belong to the phase at its start. Timeout removes survivors at its end, so
// cleanup receives at most exactly 1440 combat updates. Never call while paused.
class WaveLifecycle {
public:
    void start(EncounterSpec spec) {
        spec_ = spec;
        spec_.spawningTicks = std::max(1, spec_.spawningTicks);
        spec_.countdownTicks = std::max(0, spec_.countdownTicks);
        phase_ = spec_.countdownTicks ? Phase::Countdown : Phase::Spawning;
        elapsed_ = 0;
    }

    WaveEvents tick(std::size_t enemiesAlive, bool bossDefeated = false,
                    bool continueCollection = false, bool hasLivePickups = true) {
        WaveEvents events;
        switch (phase_) {
        case Phase::Countdown:
            if (++elapsed_ >= spec_.countdownTicks) enter(Phase::Spawning);
            break;
        case Phase::Spawning:
            if (spec_.boss) {
                if (bossDefeated) {
                    events.bossDefeated = true;
                    collect(events);
                }
            } else if (++elapsed_ >= spec_.spawningTicks) {
                enter(Phase::Cleanup);
                if (enemiesAlive == 0) collect(events);
            }
            break;
        case Phase::Cleanup:
            if (enemiesAlive == 0) collect(events);
            else if (++elapsed_ >= kCleanupTicks) {
                events.timeoutDespawn = true;
                collect(events);
            }
            break;
        case Phase::Collection:
            if (++elapsed_ >= collectionTicks() || continueCollection || !hasLivePickups) {
                enter(Phase::PendingRewards);
                events.enteredPendingRewards = true;
            }
            break;
        case Phase::PendingRewards:
        case Phase::Complete:
            break;
        }
        return events;
    }

    // The caller must first resolve all credits (or explicitly end a final boss).
    bool finishRewards(bool rewardsRemain) {
        if (phase_ != Phase::PendingRewards || rewardsRemain) return false;
        enter(Phase::Complete);
        return true;
    }

    Phase phase() const { return phase_; }
    int elapsedTicks() const { return elapsed_; }
    int collectionTicks() const { return spec_.boss ? kBossCollectionTicks : kNormalCollectionTicks; }
    int remainingTicks() const {
        switch (phase_) {
        case Phase::Countdown: return std::max(0, spec_.countdownTicks - elapsed_);
        case Phase::Spawning: return spec_.boss ? 0 : std::max(0, spec_.spawningTicks - elapsed_);
        case Phase::Cleanup: return kCleanupTicks - elapsed_;
        case Phase::Collection: return collectionTicks() - elapsed_;
        default: return 0;
        }
    }

private:
    void enter(Phase phase) { phase_ = phase; elapsed_ = 0; }
    void collect(WaveEvents& events) { enter(Phase::Collection); events.enteredCollection = true; }
    EncounterSpec spec_{};
    Phase phase_ = Phase::Complete;
    int elapsed_ = 0;
};

} // namespace threadlight::foundation
