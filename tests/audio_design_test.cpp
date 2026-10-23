#include "audio_design.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

void testWaveforms() {
    using namespace threadlight::audio;
    for (int i = 0; i < kEventCount; ++i) {
        const auto event = static_cast<Event>(i);
        const auto pcm = synthesize(event);
        CHECK(pcm.size() == static_cast<std::size_t>(duration(event) * kSampleRate));
        CHECK(!pcm.empty());
        CHECK(pcm.front() == 0);
        CHECK(pcm.back() == 0);
        const auto peak = std::max_element(pcm.begin(), pcm.end(), [](auto a, auto b) {
            return std::abs(static_cast<int>(a)) < std::abs(static_cast<int>(b));
        });
        CHECK(peak != pcm.end());
        CHECK(std::abs(static_cast<int>(*peak)) <= 32767 * 72 / 100 + 1);
        CHECK(std::any_of(pcm.begin(), pcm.end(), [](auto sample) { return sample != 0; }));
    }
}

void testEventsAreDistinctAndTunable() {
    using namespace threadlight::audio;
    const auto hit = synthesize(Event::EnemyHit);
    const auto death = synthesize(Event::EnemyDeath);
    const auto spark = synthesize(Event::Spark);
    const auto damage = synthesize(Event::PlayerDamage);
    CHECK(hit != death);
    CHECK(spark != damage);
    CHECK(synthesize(Event::Dash, 0.97f) != synthesize(Event::Dash, 1.03f));
}

void testMasterVolumeControl() {
    using namespace threadlight::audio;
    CHECK(masterVolumeStep(0.50f, 1) > 0.54f);
    CHECK(masterVolumeStep(0.50f, -1) < 0.46f);
    CHECK(masterVolumeStep(1.0f, 1) == 1.0f);
    CHECK(masterVolumeStep(0.0f, -1) == 0.0f);
    CHECK(volumeAdjustmentDirection(true, true, false) == -1);
    CHECK(volumeAdjustmentDirection(true, false, true) == 1);
    CHECK(volumeAdjustmentDirection(false, true, false) == 0);
    CHECK(volumeAdjustmentDirection(true, true, true) == 0);
    CHECK(volumeAdjustmentDirection(true, false, false) == 0);
}

void testSparkIsSofterAndVaried() {
    using namespace threadlight::audio;
    const float reductionDb = 20.0f * std::log10(eventGain(Event::Spark) / kPreviousSparkGain);
    CHECK(reductionDb <= -4.0f);
    CHECK(reductionDb >= -6.0f);
    CHECK(duration(Event::Spark) > 0.16f);
    CHECK(synthesize(Event::Spark, 0.94f) != synthesize(Event::Spark, 1.06f));
}

void testPriorityOrdering() {
    using namespace threadlight::audio;
    CHECK(priority(Event::Victory) > priority(Event::Dash));
    CHECK(priority(Event::PlayerDamage) > priority(Event::Spark));
    CHECK(priority(Event::EnemyDeath) > priority(Event::EnemyHit));
}

void testWorstCaseVoiceMixLeavesHeadroom() {
    using namespace threadlight::audio;
    for (int voices = 1; voices <= kMaximumVoices; ++voices) {
        const float worstCasePeak = kSamplePeakLimit * kMaximumEventGain *
            mixHeadroom(voices) * static_cast<float>(voices);
        CHECK(worstCasePeak < 1.0f);
        CHECK(worstCasePeak < 0.75f);
    }
    CHECK(mixHeadroom(0) == 0.0f);
}
}

int main() {
    testWaveforms();
    testEventsAreDistinctAndTunable();
    testMasterVolumeControl();
    testSparkIsSofterAndVaried();
    testPriorityOrdering();
    testWorstCaseVoiceMixLeavesHeadroom();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "All audio design checks passed.\n";
    return EXIT_SUCCESS;
}
