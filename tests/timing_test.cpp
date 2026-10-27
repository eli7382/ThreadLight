#include "game_logic.h"
#include "game_timing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

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
    for(double rate : {144.,165.,10000.}) {
        const auto result=simulate(rate);
        CHECK(result.ticks==at30.ticks);
        CHECK(std::abs(result.distance-at30.distance)<.01f);
        CHECK(std::abs(result.enemyDistance-at30.enemyDistance)<.01f);
        CHECK(result.dashTicks==at30.dashTicks);
        CHECK(result.activeSilkTicks==at30.activeSilkTicks);
        CHECK(std::abs(result.cooldown-at30.cooldown)<.001f);
        CHECK(std::abs(result.invulnerability-at30.invulnerability)<.001f);
        CHECK(std::abs(result.waveElapsed-at30.waveElapsed)<.001f);
        CHECK(std::abs(result.particleSpeed-at30.particleSpeed)<.001f);
    }
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
    CHECK(std::abs(clock.discardedSeconds() - (1.0 - 8.0 / 120.0)) < 1.e-9);
    CHECK(clock.remainder() < threadlight::timing::kFixedStepSeconds);
    clock.clear();
    CHECK(clock.discardedSeconds() == 0);
    CHECK(clock.advance(threadlight::timing::kFixedStepSeconds * .5) == 0);
    CHECK(clock.discardedSeconds() == 0);
    CHECK(clock.advance(threadlight::timing::kFixedStepSeconds * 8) == 8);
    CHECK(std::abs(clock.discardedSeconds() - threadlight::timing::kFixedStepSeconds * .5) < 1.e-9);
    CHECK(clock.advance(-1) == 0 && clock.discardedSeconds() == 0);
    CHECK(clock.advance(std::numeric_limits<double>::quiet_NaN()) == 0);
    CHECK(clock.advance(std::numeric_limits<double>::infinity()) == 0);
    CHECK(clock.remainder() == 0.0 && clock.discardedSeconds() == 0.0);
    CHECK(clock.advance(threadlight::timing::kFixedStepSeconds) == 1);
    CHECK(threadlight::timing::smoothMeasuredFps(60.0f, 1.0f / 60.0f) == 60.0f);
    const float smoothed = threadlight::timing::smoothMeasuredFps(60.0f, 1.0f / 30.0f);
    CHECK(smoothed > 30.0f);
    CHECK(smoothed < 60.0f);
    CHECK(threadlight::timing::smoothMeasuredFps(smoothed, 0.0f) == smoothed);
    CHECK(threadlight::timing::smoothMeasuredFps(smoothed,
          std::numeric_limits<float>::quiet_NaN()) == smoothed);
    CHECK(threadlight::timing::smoothMeasuredFps(smoothed,
          std::numeric_limits<float>::infinity()) == smoothed);
    CHECK(threadlight::timing::initialMeasuredFps(0.0f) == 60.0f);
    CHECK(threadlight::timing::initialMeasuredFps(std::numeric_limits<float>::denorm_min()) == 60.0f);
    CHECK(threadlight::timing::smoothMeasuredFps(smoothed,
          std::numeric_limits<float>::denorm_min()) == smoothed);
    CHECK(std::abs(threadlight::timing::initialMeasuredFps(1.0f/144.0f)-144.0f)<.01f);
    CHECK(threadlight::timing::dampingMultiplier(.96f,60.0f,0.0f) == 1.0f);
    const float sixtyFrameDamping=std::pow(threadlight::timing::dampingMultiplier(.96f,60.0f,1.0f/60.0f),60.0f);
    const float oneTwentyFrameDamping=std::pow(threadlight::timing::dampingMultiplier(.96f,60.0f,1.0f/120.0f),120.0f);
    CHECK(std::abs(sixtyFrameDamping-oneTwentyFrameDamping)<0.001f);
}
void testMovementPressesSurviveUntilSimulationTick() {
    threadlight::timing::MovementPressBuffer presses;
    presses.add(1); presses.add(4);
    CHECK(presses.consume() == 5);
    CHECK(presses.consume() == 0);
    presses.add(2); presses.clear();
    CHECK(presses.consume() == 0);
    presses.add(255); CHECK(presses.consume() == 15);
}

void testEffectiveFrameRateLimit() {
    using threadlight::timing::effectiveFrameRateLimit;

    // Foreground gameplay preserves the user's selected cap, including zero.
    CHECK(effectiveFrameRateLimit(30, true, true, false) == 30);
    CHECK(effectiveFrameRateLimit(60, true, true, false) == 60);
    CHECK(effectiveFrameRateLimit(120, true, true, false) == 120);
    CHECK(effectiveFrameRateLimit(0, true, true, false) == 0);
    CHECK(effectiveFrameRateLimit(240, true, true, false) == 240);

    // Focused menus are limited to 60, while honoring lower finite requests.
    CHECK(effectiveFrameRateLimit(30, false, true, false) == 30);
    CHECK(effectiveFrameRateLimit(60, false, true, false) == 60);
    CHECK(effectiveFrameRateLimit(120, false, true, false) == 60);
    CHECK(effectiveFrameRateLimit(0, false, true, false) == 60);
    CHECK(effectiveFrameRateLimit(240, false, true, false) == 60);

    // An unfocused or minimized window uses the low background rate.
    CHECK(effectiveFrameRateLimit(30, true, false, false) == 30);
    CHECK(effectiveFrameRateLimit(0, true, true, true) == 30);
    CHECK(effectiveFrameRateLimit(240, false, false, false) == 30);

    // Hidden fixtures keep their deterministic 60 FPS render schedule.
    CHECK(effectiveFrameRateLimit(0, true, true, false, true) == 60);
    CHECK(effectiveFrameRateLimit(240, false, false, true, true) == 60);
}
}

int main() {
    testTimeStepScheduleAcrossConfiguredRates();
    testHitchesAreBoundedAndFpsSmoothingIsStable();
    testMovementPressesSurviveUntilSimulationTick();
    testEffectiveFrameRateLimit();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Frame-rate policy and timing checks passed.\n";
}
