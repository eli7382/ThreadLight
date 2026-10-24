#pragma once

#include <algorithm>
#include <cmath>

namespace threadlight {

constexpr int kWaveCount = 5;
constexpr float kDashDurationSeconds = 0.19f;
constexpr float kDashCooldownSeconds = 1.85f;
constexpr float kUpgradedDashCooldownSeconds = 1.45f;

struct ToneWaveFormat {
    unsigned int frameCount;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
};

inline ToneWaveFormat toneWaveFormat(unsigned int frameCount, unsigned int sampleRate) {
    return {frameCount, sampleRate, 16u, 1u};
}

inline bool dashProtectsCollisionThisFrame(float remainingDashSeconds) {
    // The frame that consumes the final fraction of dash time is still protected.
    return remainingDashSeconds > 0.0f;
}

enum class Mode { Title, Play, Pause, Settings, ExitConfirm, Upgrade, Lost, Won };

struct SpawnPoint {
    float x;
    float y;
};

struct Resolution {
    Mode mode;
    int waveIndex;
    bool consumeSparkUpgrade;
    bool countEvolution;
};

inline float waveDurationSeconds(int waveIndex) {
    return 17.0f + static_cast<float>(waveIndex) * 3.0f;
}

inline bool waveIsComplete(int waveIndex, float elapsedSeconds, bool spawnDue) {
    return spawnDue && elapsedSeconds > waveDurationSeconds(waveIndex);
}

inline bool beginPlayFrame(Mode& mode, bool pausePressed) {
    if (mode != Mode::Play) return false;
    if (pausePressed) {
        mode = Mode::Pause;
        return false;
    }
    return true;
}

// Loss wins over a simultaneous wave completion; final-wave victory wins over
// a spark threshold. A non-final wave reward and a spark reward share one choice.
inline Resolution resolvePlayOutcome(int waveIndex, float health,
                                     bool waveComplete, bool sparkUpgradeReady) {
    if (health <= 0.0f) return {Mode::Lost, waveIndex, false, false};

    if (waveComplete && waveIndex + 1 >= kWaveCount)
        return {Mode::Won, waveIndex, false, false};

    if (waveComplete || sparkUpgradeReady) {
        const int nextWave = waveComplete ? waveIndex + 1 : waveIndex;
        return {Mode::Upgrade, nextWave, sparkUpgradeReady, true};
    }

    return {Mode::Play, waveIndex, false, false};
}

inline void applyProgress(const Resolution& outcome, int& waveIndex, float& energy,
                          int& level, int& choice) {
    waveIndex = outcome.waveIndex;
    if (outcome.consumeSparkUpgrade) energy -= 8.0f;
    if (outcome.countEvolution) {
        ++level;
        choice = 0;
    }
}

inline SpawnPoint chooseSafeSpawn(float startAngle, float playerX, float playerY,
                                  float width, float height, float padding,
                                  float horizontalRadius, float verticalRadius,
                                  float minimumDistance) {
    constexpr float pi = 3.14159265358979323846f;
    constexpr int candidates = 72;
    const float minimumDistanceSquared = minimumDistance * minimumDistance;
    SpawnPoint farthest{width * 0.5f, height * 0.5f};
    float farthestDistanceSquared = -1.0f;

    for (int i = 0; i < candidates; ++i) {
        const float angle = startAngle + (2.0f * pi * static_cast<float>(i) / candidates);
        const SpawnPoint point{
            std::clamp(width * 0.5f + std::cos(angle) * horizontalRadius,
                       padding, width - padding),
            std::clamp(height * 0.5f + std::sin(angle) * verticalRadius,
                       padding, height - padding)};
        const float dx = point.x - playerX;
        const float dy = point.y - playerY;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared >= minimumDistanceSquared) return point;
        if (distanceSquared > farthestDistanceSquared) {
            farthest = point;
            farthestDistanceSquared = distanceSquared;
        }
    }

    return farthest;
}

} // namespace threadlight
