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

void testDevelopmentHarnessDurations() {
    CHECK(threadlight::kWaveCount == 9);
    for (int wave = 0; wave < threadlight::kWaveCount; ++wave)
        CHECK(threadlight::waveDurationSeconds(wave) == threadlight::chapter::encounter(wave).seconds);
    // Lifecycle, timeout, rewards and credits are tested against the production
    // foundation modules. Automatic wave evolutions no longer exist.
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
    testDevelopmentHarnessDurations();
    testPauseStopsThePlayFrame();
    testToneWaveFormatMatchesRaylibFieldOrder();
    testDashTuningAndFrameBoundaryProtection();
    testSpawnsStayClearOfPlayerAtScreenEdges();
    std::cout << "ThreadLight logic regression tests passed\n";
}
