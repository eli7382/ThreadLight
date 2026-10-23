#include "game_logic.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using threadlight::Mode;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "Check failed: " #condition << " at line " << __LINE__ << '\n'; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)

namespace {

void testAdvertisedWaveCountAndDurations() {
    CHECK(threadlight::kWaveCount == 5);
    for (int wave = 0; wave < threadlight::kWaveCount; ++wave) {
        CHECK(threadlight::waveDurationSeconds(wave) == 17.0f + wave * 3.0f);
        CHECK(!threadlight::waveIsComplete(wave,
            threadlight::waveDurationSeconds(wave), true));
        CHECK(!threadlight::waveIsComplete(wave,
            threadlight::waveDurationSeconds(wave) + 0.01f, false));
        CHECK(threadlight::waveIsComplete(wave,
            threadlight::waveDurationSeconds(wave) + 0.01f, true));
    }

    int completed = 0;
    int activeWave = 0;
    while (activeWave < threadlight::kWaveCount) {
        ++completed;
        const auto outcome = threadlight::resolvePlayOutcome(
            activeWave, 100.0f, true, false);
        if (activeWave + 1 == threadlight::kWaveCount) {
            CHECK(outcome.mode == Mode::Won);
            break;
        }
        CHECK(outcome.mode == Mode::Upgrade);
        CHECK(outcome.waveIndex == activeWave + 1);
        activeWave = outcome.waveIndex;
    }
    CHECK(completed == threadlight::kWaveCount);
}

void testOutcomePrecedence() {
    const auto lossBeforeVictory = threadlight::resolvePlayOutcome(4, 0.0f, true, true);
    CHECK(lossBeforeVictory.mode == Mode::Lost);
    CHECK(!lossBeforeVictory.consumeSparkUpgrade);

    const auto victoryBeforeSparkUpgrade = threadlight::resolvePlayOutcome(4, 1.0f, true, true);
    CHECK(victoryBeforeSparkUpgrade.mode == Mode::Won);
    CHECK(!victoryBeforeSparkUpgrade.consumeSparkUpgrade);
    CHECK(!victoryBeforeSparkUpgrade.countEvolution);

    const auto intermediateWaveAndSparks = threadlight::resolvePlayOutcome(2, 100.0f, true, true);
    CHECK(intermediateWaveAndSparks.mode == Mode::Upgrade);
    CHECK(intermediateWaveAndSparks.waveIndex == 3);
    CHECK(intermediateWaveAndSparks.consumeSparkUpgrade);
    CHECK(intermediateWaveAndSparks.countEvolution);

    const auto sparkOnly = threadlight::resolvePlayOutcome(2, 100.0f, false, true);
    CHECK(sparkOnly.mode == Mode::Upgrade);
    CHECK(sparkOnly.waveIndex == 2);
    CHECK(sparkOnly.consumeSparkUpgrade);

    const auto noTransition = threadlight::resolvePlayOutcome(2, 100.0f, false, false);
    CHECK(noTransition.mode == Mode::Play);
}

void testProgressionIsAppliedOnceAndChoiceResets() {
    int waveRewardWave = 0;
    float bankedEnergy = 7.0f;
    int waveRewardLevel = 1;
    int waveRewardChoice = 2;
    const auto waveReward = threadlight::resolvePlayOutcome(
        waveRewardWave, 100.0f, true, false);
    threadlight::applyProgress(waveReward, waveRewardWave, bankedEnergy,
                               waveRewardLevel, waveRewardChoice);
    CHECK(waveRewardWave == 1);
    CHECK(bankedEnergy == 7.0f);
    CHECK(waveRewardLevel == 2);
    CHECK(waveRewardChoice == 0);

    int wave = 2;
    float energy = 9.0f;
    int level = 4;
    int choice = 2;
    const auto outcome = threadlight::resolvePlayOutcome(wave, 100.0f, true, true);
    threadlight::applyProgress(outcome, wave, energy, level, choice);
    CHECK(wave == 3);
    CHECK(energy == 1.0f);
    CHECK(level == 5);
    CHECK(choice == 0);

    const auto victory = threadlight::resolvePlayOutcome(4, 100.0f, true, true);
    threadlight::applyProgress(victory, wave, energy, level, choice);
    CHECK(wave == 4);
    CHECK(energy == 1.0f);
    CHECK(level == 5);
}

void testPauseStopsThePlayFrame() {
    Mode mode = Mode::Play;
    CHECK(!threadlight::beginPlayFrame(mode, true));
    CHECK(mode == Mode::Pause);

    mode = Mode::Play;
    CHECK(threadlight::beginPlayFrame(mode, false));
    CHECK(mode == Mode::Play);

    mode = Mode::Upgrade;
    CHECK(!threadlight::beginPlayFrame(mode, false));
    CHECK(mode == Mode::Upgrade);
}

void testToneWaveFormatMatchesRaylibFieldOrder() {
    const auto format = threadlight::toneWaveFormat(2205u, 22050u);
    CHECK(format.frameCount == 2205u);
    CHECK(format.sampleRate == 22050u);
    CHECK(format.sampleSize == 16u);
    CHECK(format.channels == 1u);
}

void testDashTuningAndFrameBoundaryProtection() {
    CHECK(threadlight::kDashDurationSeconds == 0.19f);
    CHECK(threadlight::kDashCooldownSeconds == 1.85f);
    CHECK(threadlight::kUpgradedDashCooldownSeconds == 1.45f);
    CHECK(threadlight::dashProtectsCollisionThisFrame(0.19f));
    CHECK(threadlight::dashProtectsCollisionThisFrame(0.0001f));
    CHECK(!threadlight::dashProtectsCollisionThisFrame(0.0f));
}

void testSpawnsStayClearOfPlayerAtScreenEdges() {
    constexpr float width = 1280.0f;
    constexpr float height = 800.0f;
    constexpr float padding = 25.0f;
    constexpr float minDistance = 102.0f;
    constexpr float pi = 3.14159265358979323846f;
    const float playerPositions[][2] = {
        {40.0f, 95.0f}, {1240.0f, 95.0f},
        {40.0f, 760.0f}, {1240.0f, 760.0f},
        {1240.0f, 400.0f}, {640.0f, 760.0f},
        {640.0f, 400.0f}};

    for (const auto& player : playerPositions) {
        for (int angle = 0; angle < 72; ++angle) {
            const auto spawn = threadlight::chooseSafeSpawn(
                angle * pi / 36.0f, player[0], player[1], width, height,
                padding, 680.0f, 480.0f, minDistance);
            const float dx = spawn.x - player[0];
            const float dy = spawn.y - player[1];
            CHECK(std::sqrt(dx * dx + dy * dy) >= minDistance);
        }
    }

    for (float playerY = 95.0f; playerY <= 760.0f; playerY += 95.0f) {
        for (float playerX = 40.0f; playerX <= 1240.0f; playerX += 100.0f) {
            for (int angle = 0; angle < 24; ++angle) {
                const auto spawn = threadlight::chooseSafeSpawn(
                    angle * pi / 12.0f, playerX, playerY, width, height,
                    padding, 680.0f, 480.0f, minDistance);
                const float dx = spawn.x - playerX;
                const float dy = spawn.y - playerY;
                CHECK(std::sqrt(dx * dx + dy * dy) >= minDistance);
            }
        }
    }
}

} // namespace

int main() {
    testAdvertisedWaveCountAndDurations();
    testOutcomePrecedence();
    testProgressionIsAppliedOnceAndChoiceResets();
    testPauseStopsThePlayFrame();
    testToneWaveFormatMatchesRaylibFieldOrder();
    testDashTuningAndFrameBoundaryProtection();
    testSpawnsStayClearOfPlayerAtScreenEdges();
    std::cout << "ThreadLight logic regression tests passed\n";
}
