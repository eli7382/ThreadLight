#include "../src/chapter_one.h"
#include "../src/reward_ledger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << line << ": " << expression << '\n';
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

using threadlight::chapter::Schedule;
using threadlight::chapter::Spawn;
using threadlight::combat::EnemyKind;

struct Composition {
    int hunters;
    int brutes;
    int skitters;
    int sparks;
};

constexpr std::array<int, 6> kDurationsSeconds{20, 22, 24, 24, 26, 28};
constexpr std::array<Composition, 6> kExpected{{
    {12, 0, 0, 12},
    {14, 2, 0, 18},
    {12, 4, 0, 20},
    {12, 3, 5, 23},
    {12, 4, 6, 26},
    {12, 5, 7, 29},
}};

int kindIndex(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Hunter: return 0;
    case EnemyKind::Brute: return 1;
    case EnemyKind::Skitter: return 2;
    case EnemyKind::Spitter: return 3;
    case EnemyKind::Bulwark: return 4;
    case EnemyKind::Sower: return 5;
    }
    return -1;
}

void waveDefinitionsAndBudgets() {
    using namespace threadlight;
    CHECK(chapter::kEncounterCount == 35);

    foundation::RewardLedger perfectCollection;
    int totalEnemies = 0;
    int totalSparkBudget = 0;

    for (int wave = 0; wave < 6; ++wave) {
        const chapter::Encounter encounter = chapter::encounter(wave);
        CHECK(encounter.seconds == kDurationsSeconds[static_cast<std::size_t>(wave)]);
        CHECK(!encounter.boss);

        const Schedule schedule = chapter::schedule(wave, 0x5a17u);
        const Composition expected = kExpected[static_cast<std::size_t>(wave)];
        const int expectedCount = expected.hunters + expected.brutes + expected.skitters;
        CHECK(schedule.count == expectedCount);
        CHECK(schedule.count >= 0);
        CHECK(static_cast<std::size_t>(schedule.count) <= schedule.items.size());

        std::array<int, 3> counts{};
        int waveSparks = 0;
        int previousTick = -1;
        const int boundedCount = std::clamp(schedule.count, 0, static_cast<int>(schedule.items.size()));
        for (int i = 0; i < boundedCount; ++i) {
            const Spawn& spawn = schedule.items[static_cast<std::size_t>(i)];
            CHECK(spawn.tick >= 0);
            CHECK(spawn.tick < encounter.seconds * foundation::kTicksPerSecond);
            CHECK(spawn.tick >= previousTick);
            CHECK(std::isfinite(spawn.angle));
            previousTick = spawn.tick;

            const int index = kindIndex(spawn.kind);
            CHECK(index >= 0 && index < static_cast<int>(counts.size()));
            if (index < 0 || index >= static_cast<int>(counts.size())) continue;
            ++counts[static_cast<std::size_t>(index)];

            const combat::EnemyDefinition definition = combat::definition(spawn.kind);
            waveSparks += definition.sparkValue;
            const foundation::RewardId id = perfectCollection.registerEnemy(
                definition.sparkValue, definition.killScore);
            CHECK(id != 0);
            const auto reward = perfectCollection.defeatEnemy(id);
            CHECK(reward.has_value());
            if (reward) CHECK(reward->sparkValue == definition.sparkValue);
            CHECK(perfectCollection.collectPickup(id));
        }

        CHECK(counts[0] == expected.hunters);
        CHECK(counts[1] == expected.brutes);
        CHECK(counts[2] == expected.skitters);
        CHECK(waveSparks == expected.sparks);
        totalEnemies += boundedCount;
        totalSparkBudget += waveSparks;
    }

    CHECK(totalEnemies == 110);
    CHECK(totalSparkBudget == 128);
    CHECK(totalSparkBudget <= 128);
    CHECK(perfectCollection.counters().sparksCollected == 128);
    CHECK(perfectCollection.counters().creditsIssued == 3);
    CHECK(perfectCollection.pendingCredits() == 3);
    CHECK(perfectCollection.remainder() == 38);
}

void deterministicBoundedSeedVariants() {
    using namespace threadlight::chapter;
    constexpr std::array<std::uint32_t, 8> seeds{
        0u, 1u, 0xffffffffu, 0x12345678u, 0xdeadbeefu, 0x9e3779b9u, 17u, 20261025u};

    for (int wave = 0; wave < 6; ++wave) {
        const Schedule reference = schedule(wave, seeds[0]);
        bool anyAngleVariant = false;
        for (const std::uint32_t seed : seeds) {
            const Schedule candidate = schedule(wave, seed);
            const Schedule repeated = schedule(wave, seed);
            CHECK(candidate.count == reference.count);
            CHECK(repeated.count == candidate.count);
            const int boundedCount = std::clamp(candidate.count, 0, static_cast<int>(candidate.items.size()));
            for (int i = 0; i < boundedCount; ++i) {
                const Spawn& entry = candidate.items[static_cast<std::size_t>(i)];
                const Spawn& again = repeated.items[static_cast<std::size_t>(i)];
                const Spawn& baseline = reference.items[static_cast<std::size_t>(i)];
                CHECK(entry.tick == baseline.tick);
                CHECK(entry.kind == baseline.kind);
                CHECK(entry.tick == again.tick);
                CHECK(entry.kind == again.kind);
                CHECK(entry.angle == again.angle);
                CHECK(std::isfinite(entry.angle));
                CHECK(std::fabs(entry.angle - baseline.angle) <= 0.1001f);
                if (entry.angle != baseline.angle) anyAngleVariant = true;
            }
        }
        CHECK(anyAngleVariant);
    }
}

void distinctEntryAndPacing() {
    using namespace threadlight::chapter;
    std::array<Schedule, 6> schedules{};
    for (int wave = 0; wave < 6; ++wave) schedules[static_cast<std::size_t>(wave)] = schedule(wave, 7u);

    for (std::size_t left = 0; left < schedules.size(); ++left) {
        for (std::size_t right = left + 1; right < schedules.size(); ++right) {
            const Schedule& a = schedules[left];
            const Schedule& b = schedules[right];
            bool sameEntry = a.count == b.count;
            const int prefixCount = std::min({4, a.count, b.count});
            for (int i = 0; i < prefixCount; ++i) {
                const Spawn& first = a.items[static_cast<std::size_t>(i)];
                const Spawn& second = b.items[static_cast<std::size_t>(i)];
                sameEntry = sameEntry && first.tick == second.tick && first.kind == second.kind;
            }
            CHECK(!sameEntry);
        }
    }
}

void bossHasNoFarmableSchedule() {
    using namespace threadlight::chapter;
    const Encounter boss = encounter(6);
    CHECK(boss.boss);
    CHECK(boss.seconds == 0);
    for (const std::uint32_t seed : {0u, 1u, 0xffffffffu}) {
        const Schedule scheduleForBoss = schedule(6, seed);
        CHECK(scheduleForBoss.count == 0);
    }
}

}  // namespace

int main() {
    waveDefinitionsAndBudgets();
    deterministicBoundedSeedVariants();
    distinctEntryAndPacing();
    bossHasNoFarmableSchedule();

    if (failures != 0) {
        std::cerr << failures << " chapter check(s) failed\n";
        return 1;
    }
    std::cout << "chapter wave checks passed\n";
    return 0;
}
