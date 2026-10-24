#include "game_logic.h"
#include "game_timing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}
#define CHECK(expression) check((expression), #expression, __LINE__)

struct SimulationResult {
    int ticks;
    float distance;
    float cooldown;
    float invulnerability;
    float enemyDistance;
    float waveElapsed;
    float silkAge;
    float particleSpeed;
    int dashTicks;
    int activeSilkTicks;
};

SimulationResult simulate(double renderFps) {
    threadlight::timing::FixedStepClock clock;
    constexpr float step = static_cast<float>(threadlight::timing::kFixedStepSeconds);
    constexpr float duration = 4.0f;
    constexpr float movementSpeed = 255.0f;
    constexpr float dashMultiplier = 3.7f;
    float dashRemaining = threadlight::kDashDurationSeconds;
    float cooldown = threadlight::kDashCooldownSeconds;
    float invulnerability = threadlight::kDashDurationSeconds;
    float distance = 0.0f;
    float enemyDistance = 0.0f;
    float waveElapsed = 0.0f;
    float silkAge = 0.0f;
    float particleSpeed = 100.0f;
    int ticks = 0;
    int dashTicks = 0;
    int activeSilkTicks = 0;
    const int frames = static_cast<int>(std::lround(renderFps * duration));
    for (int frame = 0; frame < frames; ++frame) {
        const int steps = clock.advance(1.0 / renderFps);
        for (int i = 0; i < steps; ++i) {
            const bool dashing = threadlight::dashProtectsCollisionThisFrame(dashRemaining);
            if (dashing) ++dashTicks;
            distance += movementSpeed * (dashing ? dashMultiplier : 1.0f) * step;
            enemyDistance += 95.0f * step;
            waveElapsed += step;
            silkAge += step;
            if (silkAge <= 1.2f) ++activeSilkTicks;
            particleSpeed *= threadlight::timing::dampingMultiplier(.96f, 60.0f, step);
            if (dashing) dashRemaining = std::max(0.0f, dashRemaining - step);
            cooldown = std::max(0.0f, cooldown - step);
            invulnerability = std::max(0.0f, invulnerability - step);
            ++ticks;
        }
    }
    return {ticks, distance, cooldown, invulnerability, enemyDistance,
            waveElapsed, silkAge, particleSpeed, dashTicks, activeSilkTicks};
}

void testTimeStepScheduleAcrossConfiguredRates() {
    const auto at30 = simulate(30.0);
    const auto at60 = simulate(60.0);
    const auto at120 = simulate(120.0);
    const auto unlimited = simulate(1000.0);
    CHECK(at30.ticks == 480);
    CHECK(at60.ticks == at30.ticks);
    CHECK(at120.ticks == at30.ticks);
    CHECK(unlimited.ticks == at30.ticks);
    CHECK(std::abs(at30.distance - at60.distance) < 0.01f);
    CHECK(std::abs(at30.distance - at120.distance) < 0.01f);
    CHECK(std::abs(at30.distance - unlimited.distance) < 0.01f);
    CHECK(std::abs(at30.cooldown - at120.cooldown) < 0.001f);
    CHECK(std::abs(at30.invulnerability - unlimited.invulnerability) < 0.001f);
    CHECK(std::abs(at30.enemyDistance - at120.enemyDistance) < 0.01f);
    CHECK(std::abs(at30.waveElapsed - unlimited.waveElapsed) < 0.001f);
    CHECK(std::abs(at30.silkAge - at60.silkAge) < 0.001f);
    CHECK(std::abs(at30.particleSpeed - at120.particleSpeed) < 0.001f);
    CHECK(at30.dashTicks == 23);
    CHECK(at30.dashTicks == at60.dashTicks);
    CHECK(at30.dashTicks == at120.dashTicks);
    CHECK(at30.activeSilkTicks == 144);
    CHECK(at30.activeSilkTicks == at120.activeSilkTicks);
}

void testHitchesAreBoundedAndFpsSmoothingIsStable() {
    threadlight::timing::FixedStepClock clock;
    CHECK(clock.advance(1.0) == threadlight::timing::kMaxStepsPerFrame);
    CHECK(clock.remainder() < threadlight::timing::kFixedStepSeconds);
    CHECK(threadlight::timing::smoothMeasuredFps(60.0f, 1.0f / 60.0f) == 60.0f);
    const float smoothed = threadlight::timing::smoothMeasuredFps(60.0f, 1.0f / 30.0f);
    CHECK(smoothed > 30.0f);
    CHECK(smoothed < 60.0f);
    CHECK(threadlight::timing::smoothMeasuredFps(smoothed, 0.0f) == smoothed);
    CHECK(threadlight::timing::dampingMultiplier(.96f,60.0f,0.0f) == 1.0f);
    const float sixtyFrameDamping=std::pow(threadlight::timing::dampingMultiplier(.96f,60.0f,1.0f/60.0f),60.0f);
    const float oneTwentyFrameDamping=std::pow(threadlight::timing::dampingMultiplier(.96f,60.0f,1.0f/120.0f),120.0f);
    CHECK(std::abs(sixtyFrameDamping-oneTwentyFrameDamping)<0.001f);
}
}

int main() {
    testTimeStepScheduleAcrossConfiguredRates();
    testHitchesAreBoundedAndFpsSmoothingIsStable();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Frame-rate policy and timing checks passed.\n";
}
