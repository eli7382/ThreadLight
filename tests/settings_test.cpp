#include "app_settings.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

void testDefaultsAndLimits() {
    using namespace threadlight::settings;
    const auto platformPath = userSettingsPath();
    if (!platformPath.empty()) {
        CHECK(platformPath.filename() == "settings.ini");
        CHECK(platformPath.parent_path().filename() == "ThreadLight");
    }
    const AppSettings defaults;
    CHECK(defaults.frameRateLimit == 60);
    CHECK(defaults.showFps);
    CHECK(defaults.sfxVolumePercent == 78);
    CHECK(validFrameRateLimit(30));
    CHECK(validFrameRateLimit(60));
    CHECK(validFrameRateLimit(120));
    CHECK(validFrameRateLimit(0));
    CHECK(!validFrameRateLimit(144));
    CHECK(nextFrameRateLimit(60, 1) == 120);
    CHECK(nextFrameRateLimit(120, 1) == 0);
    CHECK(nextFrameRateLimit(0, 1) == 30);
    CHECK(nextFrameRateLimit(30, -1) == 0);
    CHECK(adjustSfxVolume(78, 1) == 83);
    CHECK(adjustSfxVolume(78, -1) == 73);
    CHECK(adjustSfxVolume(100, 1) == 100);
    CHECK(adjustSfxVolume(0, -1) == 0);
    CHECK(quantizeSfxVolume(0) == 0);
    CHECK(quantizeSfxVolume(2) == 0);
    CHECK(quantizeSfxVolume(3) == 5);
    CHECK(quantizeSfxVolume(97) == 95);
    CHECK(quantizeSfxVolume(98) == 100);
}

void testPersistenceAndInvalidRecovery() {
    using namespace threadlight::settings;
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::current_path() /
                           ("ThreadLightSettingsTest-" + std::to_string(unique));
    const auto path = directory / "settings.ini";

    const auto missing = load(path);
    CHECK(missing.frameRateLimit == 60);
    CHECK(missing.showFps);
    CHECK(missing.sfxVolumePercent == 78);

    AppSettings saved{120, false, 35};
    CHECK(save(path, saved));
    const auto loaded = load(path);
    CHECK(loaded.frameRateLimit == 120);
    CHECK(!loaded.showFps);
    CHECK(loaded.sfxVolumePercent == 35);

    const AppSettings updated{30, true, 80};
    CHECK(save(path, updated)); // Replacing an existing settings file is supported.
    const auto updatedSettings = load(path);
    CHECK(updatedSettings.frameRateLimit == 30);
    CHECK(updatedSettings.showFps);
    CHECK(updatedSettings.sfxVolumePercent == 80);

    {
        std::ofstream invalid(path, std::ios::binary | std::ios::trunc);
        invalid << "frame_rate_limit=144\nshow_fps=maybe\nsfx_volume_percent=900\n";
    }
    const auto recovered = load(path);
    CHECK(recovered.frameRateLimit == 60);
    CHECK(recovered.showFps);
    CHECK(recovered.sfxVolumePercent == 78);

    {
        std::ofstream oversized(path, std::ios::binary | std::ios::trunc);
        oversized << std::string(5000, 'x');
    }
    const auto oversizedRecovery = load(path);
    CHECK(oversizedRecovery.frameRateLimit == 60);
    CHECK(oversizedRecovery.showFps);
    CHECK(oversizedRecovery.sfxVolumePercent == 78);

    std::error_code error;
    std::filesystem::remove_all(directory, error);
    CHECK(!error);
}
}

int main() {
    testDefaultsAndLimits();
    testPersistenceAndInvalidRecovery();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Settings and persistence checks passed.\n";
}
