#pragma once

#include <algorithm>
#include <cmath>
#include "wave_foundation.h"

namespace threadlight::timing {

constexpr double kFixedStepSeconds = 1.0 / foundation::kTicksPerSecond;
constexpr int kMaxStepsPerFrame = 8;

// Menus and background windows use a fixed low cap while focused gameplay
// preserves the user's selected limit.
inline int effectiveFrameRateLimit(int requested, bool playing, bool focused,
                                   bool minimized, bool hiddenFixture = false) {
    if (hiddenFixture) return 60;
    if (!focused || minimized) return 30;
    if (playing) return requested;
    return 30;
}

// Retain brief direction presses until a simulation step consumes them.
class MovementPressBuffer {
public:
    void add(unsigned presses) { pending_ |= presses & 15u; }
    unsigned consume() { const unsigned result = pending_; pending_ = 0; return result; }
    void clear() { pending_ = 0; }
private:
    unsigned pending_ = 0;
};

class FixedStepClock {
public:
    int advance(double frameSeconds) {
        // A bad clock sample must not poison the accumulator or the tick count.
        if (!std::isfinite(frameSeconds) || frameSeconds <= 0.0) {
            discardedSeconds_ = 0.0;
            return 0;
        }
        const double boundedFrame = std::clamp(frameSeconds, 0.0,
                                                kFixedStepSeconds * kMaxStepsPerFrame);
        const double capacity = kFixedStepSeconds * kMaxStepsPerFrame;
        discardedSeconds_ = std::max(0.0, frameSeconds - boundedFrame) +
                            std::max(0.0, accumulator_ + boundedFrame - capacity);
        accumulator_ = std::min(accumulator_ + boundedFrame,
                                capacity);
        const int steps = static_cast<int>((accumulator_ + 1.0e-10) / kFixedStepSeconds);
        accumulator_ -= steps * kFixedStepSeconds;
        return steps;
    }

    void clear() { accumulator_ = 0.0; discardedSeconds_ = 0.0; }
    double remainder() const { return accumulator_; }
    // Time dropped by the most recent advance, excluding intentional clears.
    double discardedSeconds() const { return discardedSeconds_; }
    float presentationAlpha() const {
        return static_cast<float>(std::clamp(accumulator_ / kFixedStepSeconds, 0.0, 1.0));
    }

private:
    double accumulator_ = 0.0;
    double discardedSeconds_ = 0.0;
};

inline float smoothMeasuredFps(float previous, float frameSeconds) {
    if (!std::isfinite(frameSeconds) || frameSeconds <= 0.0f) return previous;
    const float measured = 1.0f / frameSeconds;
    if (!std::isfinite(measured)) return previous;
    const float blend = 1.0f - std::exp(-frameSeconds * 4.0f);
    return previous + (measured - previous) * blend;
}

inline float initialMeasuredFps(float frameSeconds) {
    const float measured = std::isfinite(frameSeconds) && frameSeconds > 0.0f ? 1.0f / frameSeconds : 60.0f;
    return std::isfinite(measured) ? measured : 60.0f;
}

inline float dampingMultiplier(float multiplierAtReferenceFrame,
                               float referenceFramesPerSecond,
                               float elapsedSeconds) {
    if (elapsedSeconds <= 0.0f) return 1.0f;
    return std::pow(std::clamp(multiplierAtReferenceFrame, 0.0f, 1.0f),
                    std::max(0.0f, referenceFramesPerSecond) * elapsedSeconds);
}

} // namespace threadlight::timing
