#pragma once

#include <algorithm>

namespace threadlight::visual {

constexpr float kActiveSilkSeconds = 1.2f;
constexpr float kSilkLifetimeSeconds = 5.5f;
constexpr float kSmallDeathSeconds = 0.34f;
constexpr float kHeavyDeathSeconds = 0.88f;
constexpr float kEnemyHitFlashSeconds = 0.16f;
constexpr float kDamageShakeSeconds = 0.18f;
constexpr int kParticleLimit = 512;
constexpr int kDeathEffectLimit = 64;
constexpr int kSmallShardCount = 7;
constexpr int kHeavyShardCount = 10;

struct SilkAppearance {
    float ageBlend;
    float fade;
    float energy;
    float bodyOpacity;
    float coreOpacity;
    float haloOpacity;
};

inline float smooth01(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline SilkAppearance silkAppearance(float ageSeconds, float shimmer01) {
    const float age = std::clamp(ageSeconds, 0.0f, kSilkLifetimeSeconds);
    const float coolProgress = (age - kActiveSilkSeconds) / 0.48f;
    const float ageBlend = smooth01(coolProgress);
    const float fadeProgress = (age - kActiveSilkSeconds) /
                               (kSilkLifetimeSeconds - kActiveSilkSeconds);
    const float fade = age <= kActiveSilkSeconds
        ? 1.0f
        : 1.0f - smooth01(fadeProgress);
    const float shimmer = 0.97f + 0.03f * std::clamp(shimmer01, 0.0f, 1.0f);
    return {
        ageBlend,
        fade,
        (1.0f - 0.20f * ageBlend) * fade * shimmer,
        (0.84f - 0.48f * ageBlend) * fade,
        (0.86f - 0.48f * ageBlend) * fade,
        (0.16f - 0.09f * ageBlend) * fade
    };
}

inline float deathProgress(float ageSeconds, float durationSeconds) {
    if (durationSeconds <= 0.0f) return 1.0f;
    return smooth01(ageSeconds / durationSeconds);
}

inline float hitFlashStrength(float remainingSeconds) {
    return smooth01(remainingSeconds / kEnemyHitFlashSeconds);
}

inline float shakeEnvelope(float remainingSeconds, float durationSeconds) {
    if (durationSeconds <= 0.0f) return 0.0f;
    const float progress = std::clamp(remainingSeconds / durationSeconds, 0.0f, 1.0f);
    return progress * progress;
}

} // namespace threadlight::visual
