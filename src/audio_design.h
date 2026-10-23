#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace threadlight::audio {

enum class Event : std::uint8_t {
    Dash,
    EnemyHit,
    EnemyDeath,
    Spark,
    PlayerDamage,
    Evolution,
    Victory,
    Defeat,
    Count
};

constexpr int kEventCount = static_cast<int>(Event::Count);
constexpr int kVoiceVariants = 2;
constexpr int kMaximumVoices = 8;
constexpr unsigned int kSampleRate = 24000;
constexpr float kSamplePeakLimit = 0.72f;
constexpr float kMaximumEventGain = 0.76f;
constexpr float kPreviousSparkGain = 0.52f;
constexpr float kSparkGainReductionDb = 5.0f;

inline float mixHeadroom(int activeVoices) {
    return activeVoices > 0
        ? 0.48f / std::sqrt(static_cast<float>(activeVoices)) : 0.0f;
}

inline int priority(Event event) {
    switch (event) {
    case Event::PlayerDamage: return 3;
    case Event::Evolution:
    case Event::Victory:
    case Event::Defeat: return 4;
    case Event::Dash:
    case Event::EnemyDeath: return 2;
    case Event::EnemyHit:
    case Event::Spark: return 1;
    case Event::Count: break;
    }
    return 0;
}

inline float duration(Event event) {
    switch (event) {
    case Event::Dash: return 0.23f;
    case Event::EnemyHit: return 0.09f;
    case Event::EnemyDeath: return 0.24f;
    case Event::Spark: return 0.19f;
    case Event::PlayerDamage: return 0.26f;
    case Event::Evolution: return 0.48f;
    case Event::Victory: return 0.88f;
    case Event::Defeat: return 0.72f;
    case Event::Count: break;
    }
    return 0.1f;
}

inline float masterVolumeStep(float volume, int direction) {
    return std::clamp(volume + static_cast<float>(direction) * 0.05f, 0.0f, 1.0f);
}

inline int volumeAdjustmentDirection(bool paused, bool leftPressed, bool rightPressed) {
    if (!paused || leftPressed == rightPressed) return 0;
    return leftPressed ? -1 : 1;
}

inline float eventGain(Event event) {
    switch (event) {
    case Event::Dash: return 0.72f;
    case Event::EnemyHit: return 0.38f;
    case Event::EnemyDeath: return 0.62f;
    case Event::Spark:
        return kPreviousSparkGain * std::pow(10.0f, -kSparkGainReductionDb / 20.0f);
    case Event::PlayerDamage: return 0.76f;
    case Event::Evolution: return 0.62f;
    case Event::Victory: return 0.72f;
    case Event::Defeat: return 0.65f;
    case Event::Count: break;
    }
    return 0.5f;
}

inline float clampSample(float sample) {
    return std::clamp(sample, -kSamplePeakLimit, kSamplePeakLimit);
}

inline std::vector<std::int16_t> synthesize(Event event, float pitchScale = 1.0f,
                                             unsigned int sampleRate = kSampleRate) {
    constexpr float pi = 3.14159265358979323846f;
    const float length = duration(event);
    const unsigned int frameCount = static_cast<unsigned int>(length * sampleRate);
    std::vector<std::int16_t> pcm(frameCount);
    const auto phase = [pi](float startHz, float endHz, float t, float lengthSeconds) {
        return 2.0f * pi * (startHz * t + 0.5f * (endHz - startHz) * t * t / lengthSeconds);
    };

    for (unsigned int i = 0; i < frameCount; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        const float position = t / length;
        const float attackSeconds = event == Event::Spark ? 0.018f : 0.008f;
        const float releaseSeconds = event == Event::Spark ? 0.085f : std::min(0.09f, length * 0.28f);
        const float attack = std::clamp(t / attackSeconds, 0.0f, 1.0f);
        const float release = std::clamp((length - t) / releaseSeconds, 0.0f, 1.0f);
        const float envelope = attack * attack * (3.0f - 2.0f * attack) * release * release * (3.0f - 2.0f * release);
        float sample = 0.0f;

        switch (event) {
        case Event::Dash: {
            const float p = phase(410.0f * pitchScale, 165.0f * pitchScale, t, length);
            sample = 0.43f * std::sin(p) + 0.13f * std::sin(2.01f * p);
            sample *= 0.58f + 0.42f * (1.0f - position);
            break;
        }
        case Event::EnemyHit: {
            const float p = phase(315.0f * pitchScale, 205.0f * pitchScale, t, length);
            sample = 0.40f * std::sin(p) + 0.12f * std::sin(2.0f * p);
            sample *= (1.0f - 0.35f * position);
            break;
        }
        case Event::EnemyDeath: {
            const float low = phase(365.0f * pitchScale, 190.0f * pitchScale, t, length);
            const float high = phase(510.0f * pitchScale, 315.0f * pitchScale, t, length);
            sample = 0.31f * std::sin(low) + 0.17f * std::sin(high);
            break;
        }
        case Event::Spark: {
            const float p = phase(555.0f * pitchScale, 755.0f * pitchScale, t, length);
            const float warmth = phase(310.0f * pitchScale, 415.0f * pitchScale, t, length);
            sample = 0.30f * std::sin(p) + 0.13f * std::sin(warmth) + 0.045f * std::sin(1.5f * p);
            break;
        }
        case Event::PlayerDamage: {
            const float low = phase(172.0f * pitchScale, 83.0f * pitchScale, t, length);
            const float color = phase(246.0f * pitchScale, 126.0f * pitchScale, t, length);
            sample = 0.39f * std::sin(low) + 0.095f * std::sin(color);
            break;
        }
        case Event::Evolution: {
            const float a = phase(350.0f * pitchScale, 470.0f * pitchScale, t, length);
            const float b = phase(525.0f * pitchScale, 705.0f * pitchScale, t, length);
            const float c = phase(700.0f * pitchScale, 940.0f * pitchScale, t, length);
            const float swell = 0.72f + 0.28f * std::sin(pi * position);
            sample = (0.20f * std::sin(a) + 0.14f * std::sin(b) + 0.075f * std::sin(c)) * swell;
            break;
        }
        case Event::Victory: {
            const float a = phase(392.0f * pitchScale, 523.25f * pitchScale, t, length);
            const float b = phase(493.88f * pitchScale, 659.25f * pitchScale, t, length);
            const float c = phase(587.33f * pitchScale, 783.99f * pitchScale, t, length);
            const float swell = 0.76f + 0.24f * std::sin(pi * position * 0.72f);
            sample = (0.17f * std::sin(a) + 0.13f * std::sin(b) + 0.09f * std::sin(c)) * swell;
            break;
        }
        case Event::Defeat: {
            const float a = phase(294.0f * pitchScale, 110.0f * pitchScale, t, length);
            const float b = phase(220.0f * pitchScale, 82.0f * pitchScale, t, length);
            sample = 0.26f * std::sin(a) + 0.11f * std::sin(b);
            break;
        }
        case Event::Count: break;
        }

        sample *= envelope;
        sample = clampSample(sample);
        const int scaled = static_cast<int>(std::lround(sample * 32767.0f));
        pcm[i] = static_cast<std::int16_t>(std::clamp(scaled, -32767, 32767));
    }
    return pcm;
}

} // namespace threadlight::audio
