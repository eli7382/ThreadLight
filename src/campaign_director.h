#pragma once

#include "chapter_one.h"
#include "player_observation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace threadlight::campaign {

constexpr int kFirstAdaptiveWave = 28; // displayed Waves 29..34
constexpr int kLastAdaptiveWave = 33;
constexpr int kMaximumTimingAdjustmentTicks = 300;

struct Decision {
    chapter::Schedule schedule{};
    int durationSeconds = 0;
    float timingShiftTicks = 0.f;
    float angleShiftRadians = 0.f;
    float confidence = 0.f;
};

// The director only reshapes authored entries. It never adds or removes an
// enemy, changes reward/score classes, or changes the authored encounter clock.
inline Decision direct(int wave, std::uint32_t seed, const observation::Summary& player) {
    Decision out;
    out.schedule = chapter::schedule(wave, seed);
    out.durationSeconds = chapter::encounter(wave).seconds;
    out.confidence = std::clamp(player.confidence, 0.f, 1.f);
    if (wave < kFirstAdaptiveWave || wave > kLastAdaptiveWave || out.schedule.count == 0) return out;

    const float certainty = out.confidence;
    const bool lowMovement = player.movement < .24f;
    const bool highMovement = player.movement > .62f;
    const std::uint32_t seedMix = seed ^ (static_cast<std::uint32_t>(wave + 1) * 0x9e3779b9u);
    const float seedUnit = static_cast<float>((seedMix ^ (seedMix >> 13)) % 2001u) / 1000.f - 1.f;
    const float profileTempo = (player.movement - .5f) * .08f +
                               (player.repetition - .3f) * .06f +
                               (player.perimeter - .4f) * .04f + player.orbitBias * .015f;
    const float waveTempo = (static_cast<float>(wave) - 30.5f) * .006f;
    const float tempo = std::clamp(1.f + (profileTempo + waveTempo + seedUnit * .015f) * certainty, .92f, 1.08f);
    // Rotate the entire authored lane arrangement: directional relationships
    // inside each warning packet remain intact while the wave gains a distinct
    // chapter-five approach identity.
    const float authoredPhase = .34f + static_cast<float>(wave - kFirstAdaptiveWave) * .10f;
    const float playerPhase = player.orbitBias * .12f + player.repetition * .12f +
                              (player.horizontalBias - player.verticalBias) * .20f + player.perimeter * .10f;
    out.angleShiftRadians = std::clamp(authoredPhase + playerPhase * certainty + seedUnit * .08f * certainty,
                                       -.95f, .95f);
    out.timingShiftTicks = 0.f;
    const int deadline = std::max(1, chapter::encounter(wave).seconds * 120 - 240);
    for (int i = 0; i < out.schedule.count; ++i) {
        auto& spawn = out.schedule.items[static_cast<std::size_t>(i)];
        const int originalTick = spawn.tick;
        spawn.tick = std::clamp(static_cast<int>(std::lround(static_cast<float>(spawn.tick) * tempo)), 0, deadline);
        spawn.angle += out.angleShiftRadians;
        out.timingShiftTicks = std::max(out.timingShiftTicks,
            static_cast<float>(std::abs(spawn.tick - originalTick)));
    }
    std::stable_sort(out.schedule.items.begin(), out.schedule.items.begin() + out.schedule.count,
                     [](const chapter::Spawn& a, const chapter::Spawn& b) { return a.tick < b.tick; });
    const auto base=chapter::schedule(wave,seed);
    const auto peak=[](const chapter::Schedule& schedule){int maximum=0;
        for(int i=0;i<schedule.count;++i){int sum=0;for(int j=0;j<schedule.count;++j)
            if(schedule.items[j].tick>=schedule.items[i].tick&&schedule.items[j].tick<schedule.items[i].tick+240){
                const auto d=combat::definition(schedule.items[j].kind,5);sum+=d.contactDamage+2*d.killScore+3*d.sparkValue;}
            maximum=std::max(maximum,sum);}return maximum;};
    // Admit retiming only inside the authored weighted burst envelope. A
    // rejected tempo retains the adaptive deployment angles, never alters HP.
    if(peak(out.schedule)>peak(base)*5/4){
        out.timingShiftTicks=0;for(int i=0;i<out.schedule.count;++i)out.schedule.items[i].tick=base.items[i].tick;
    }
    return out;
}

inline chapter::Schedule schedule(int wave, std::uint32_t seed, const observation::Summary& player) {
    return direct(wave, seed, player).schedule;
}

inline int peakThreat(const chapter::Schedule& value) {
        int peak = 0;
        for (int i = 0; i < value.count; ++i) {
            int load = 0;
            const int start = value.items[static_cast<std::size_t>(i)].tick;
            for (int j = 0; j < value.count; ++j) {
                const auto& candidate = value.items[static_cast<std::size_t>(j)];
                if (candidate.tick >= start && candidate.tick < start + 240) {
                    const auto stats = combat::definition(candidate.kind, 5);
                    load += stats.contactDamage + 2 * stats.killScore + 3 * stats.sparkValue;
                }
            }
            peak = std::max(peak, load);
        }
        return peak;
    }

// Re-aim only deployments that are still safely in the future. This keeps the
// authored encounter clock and its resource budget intact while allowing a
// live player profile to influence later approach lanes.
inline int reassess(chapter::Schedule& current, int nextSpawnIndex, int elapsedTick,
                    const observation::Summary& player) {
    constexpr int kCommitHorizonTicks = 120;
    constexpr float kMaximumReviewAngleDelta = .5f;
    constexpr int kMaximumReviewTimingDeltaTicks = 24;
    const int begin = std::clamp(nextSpawnIndex, 0, current.count);
    const float confidence = std::clamp(player.confidence, 0.f, 1.f);
    if (begin >= current.count || confidence <= 0.f) return 0;

    const chapter::Schedule originalSchedule = current;
    const int originalDeadline = originalSchedule.items[static_cast<std::size_t>(current.count - 1)].tick;

    const float headingMagnitude = std::sqrt(player.horizontalBias * player.horizontalBias +
                                             player.verticalBias * player.verticalBias);
    const float heading = headingMagnitude > .05f
        ? std::atan2(player.verticalBias, player.horizontalBias) : 0.f;
    const float headingWeight = std::clamp(headingMagnitude, 0.f, 1.f) *
                                std::clamp(player.repetition, 0.f, 1.f) * .34f;
    const float perimeterWeight = std::clamp((player.perimeter - .35f) / .65f, 0.f, 1.f) * .30f;
    const float aggression = std::clamp((player.offense * .65f + player.counterSuccess * .2f +
                                         player.responseRate * .15f), 0.f, 1.f);
    const float timingSignal = (std::clamp(player.movement, 0.f, 1.f) - .5f) * .35f +
                               (std::clamp(player.perimeter, 0.f, 1.f) - .5f) * .30f +
                               (aggression - .5f) * .20f +
                               (std::clamp(player.repetition, 0.f, 1.f) - .5f) * .15f;
    const int timingDelta = static_cast<int>(std::lround(std::clamp(timingSignal, -.5f, .5f) * confidence *
                                                         kMaximumReviewTimingDeltaTicks * 2.f));
    for (int i = begin; i < current.count; ++i) {
        auto& spawn = current.items[static_cast<std::size_t>(i)];
        const int originalTick = originalSchedule.items[static_cast<std::size_t>(i)].tick;
        if (originalTick <= elapsedTick + kCommitHorizonTicks) continue;

        // Shift the uncommitted lane as a unit; preserve the authored end tick
        // and keep every deployment inside the original encounter deadline.
        if (originalTick != originalDeadline && timingDelta != 0) {
            const int firstUncommittedTick = std::max(0, elapsedTick + kCommitHorizonTicks + 1);
            spawn.tick = std::clamp(originalTick + timingDelta, firstUncommittedTick, originalDeadline);
        }

        float delta = 0.f;
        if (headingWeight > 0.f) {
            // Repeated travel headings pull the next approach away from that lane.
            delta += std::sin(spawn.angle - heading) * headingWeight;
        }
        if (perimeterWeight > 0.f) {
            // Perimeter-heavy play benefits from alternating approach arcs.
            delta += ((i & 1) == 0 ? -1.f : 1.f) * perimeterWeight;
        }
        if (aggression > 0.f) {
            // Role-specific lanes balance interception with protection;
            // enemy counts, threat and damage remain unchanged.
            float roleGain = .12f;
            switch (spawn.kind) {
            case combat::EnemyKind::Brute: roleGain = .20f; break;
            case combat::EnemyKind::Skitter: roleGain = -.18f; break;
            case combat::EnemyKind::Spitter: roleGain = .15f; break;
            case combat::EnemyKind::BriarCaller: roleGain = .08f; break;
            case combat::EnemyKind::Sower: roleGain = -.08f; break;
            case combat::EnemyKind::Hunter: roleGain = .12f; break;
            }
            delta += roleGain * (aggression - .35f);
        }
        delta = std::clamp(delta * confidence, -kMaximumReviewAngleDelta, kMaximumReviewAngleDelta);
        if (std::abs(delta) > 1.e-5f)
            spawn.angle = std::remainder(spawn.angle + delta, 2.f * chapter::kPi);
    }

    if (peakThreat(current) > peakThreat(originalSchedule) * 5 / 4) {
        for (int i = 0; i < current.count; ++i)
            current.items[static_cast<std::size_t>(i)].tick = originalSchedule.items[static_cast<std::size_t>(i)].tick;
    }

    int changed = 0;
    for (int i = 0; i < current.count; ++i) {
        const auto& spawn = current.items[static_cast<std::size_t>(i)];
        const auto& original = originalSchedule.items[static_cast<std::size_t>(i)];
        if (spawn.tick != original.tick ||
            std::abs(std::remainder(spawn.angle - original.angle, 2.f * chapter::kPi)) > 1.e-5f) {
            ++changed;
        }
    }
    return changed;
}

} // namespace threadlight::campaign
