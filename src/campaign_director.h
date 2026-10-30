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

} // namespace threadlight::campaign
