#pragma once

#include <algorithm>
#include <cmath>
#include "wave_foundation.h"

namespace threadlight::timing {

constexpr double kFixedStepSeconds = 1.0 / foundation::kTicksPerSecond;
constexpr int kMaxStepsPerFrame = 8;

class FixedStepClock {
public:
    int advance(double frameSeconds) {
        const double boundedFrame = std::clamp(frameSeconds, 0.0,
                                                kFixedStepSeconds * kMaxStepsPerFrame);
        accumulator_ = std::min(accumulator_ + boundedFrame,
                                kFixedStepSeconds * kMaxStepsPerFrame);
        const int steps = static_cast<int>((accumulator_ + 1.0e-10) / kFixedStepSeconds);
        accumulator_ -= steps * kFixedStepSeconds;
        return steps;
    }

    void clear() { accumulator_ = 0.0; }
    double remainder() const { return accumulator_; }
    float presentationAlpha() const {
        return static_cast<float>(std::clamp(accumulator_ / kFixedStepSeconds, 0.0, 1.0));
    }

private:
    double accumulator_ = 0.0;
};

inline float smoothMeasuredFps(float previous, float frameSeconds) {
    if (frameSeconds <= 0.0f) return previous;
    const float measured = 1.0f / frameSeconds;
    const float blend = 1.0f - std::exp(-frameSeconds * 4.0f);
    return previous + (measured - previous) * blend;
}

inline float dampingMultiplier(float multiplierAtReferenceFrame,
                               float referenceFramesPerSecond,
                               float elapsedSeconds) {
    if (elapsedSeconds <= 0.0f) return 1.0f;
    return std::pow(std::clamp(multiplierAtReferenceFrame, 0.0f, 1.0f),
                    std::max(0.0f, referenceFramesPerSecond) * elapsedSeconds);
}

} // namespace threadlight::timing
