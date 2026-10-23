#include "visual_feedback.h"

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

void testImpactPulse() {
    using namespace threadlight::visual;
    CHECK(damageImpactStrength(0.0f) == 0.0f);
    CHECK(damageImpactStrength(kDamageFeedbackDuration) == 1.0f);
    CHECK(damageImpactStrength(kDamageFeedbackDuration * 0.5f) == 0.5f);
    CHECK(damageImpactStrength(kDamageFeedbackDuration * 0.75f) >
          damageImpactStrength(kDamageFeedbackDuration * 0.25f));
}

void testRingExpandsAndStaysBounded() {
    using namespace threadlight::visual;
    constexpr float playerRadius = 12.0f;
    const float startRadius = damageRingRadius(playerRadius, kDamageFeedbackDuration);
    const float middleRadius = damageRingRadius(playerRadius, kDamageFeedbackDuration * 0.5f);
    const float endRadius = damageRingRadius(playerRadius, 0.0f);
    CHECK(startRadius == playerRadius + 6.0f);
    CHECK(middleRadius > startRadius);
    CHECK(endRadius == playerRadius + 40.0f);
    CHECK(middleRadius < endRadius);
    CHECK(damageRingProgress(kDamageFeedbackDuration * 0.5f) == 0.5f);
}
}

int main() {
    testImpactPulse();
    testRingExpandsAndStaysBounded();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "All visual feedback checks passed.\n";
    return EXIT_SUCCESS;
}
