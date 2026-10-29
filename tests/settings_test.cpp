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
    CHECK(!defaults.vsync);
    CHECK(defaults.sfxVolumePercent == 78);
    CHECK(!defaults.tutorialCompleted);
    CHECK(defaults.selectedPalette == threadlight::cosmetics::PaletteId::OriginalCyan);
    CHECK(!defaults.violetAscendantUnlocked);
    CHECK(validFrameRateLimit(30));
    CHECK(validFrameRateLimit(60));
    CHECK(validFrameRateLimit(120));
    CHECK(validFrameRateLimit(240));
    CHECK(validFrameRateLimit(0));
    CHECK(!validFrameRateLimit(144));
    CHECK(nextFrameRateLimit(60, 1) == 120);
    CHECK(nextFrameRateLimit(120, 1) == 240);
    CHECK(nextFrameRateLimit(240, 1) == 0);
    CHECK(nextFrameRateLimit(0, -1) == 240);
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
    CHECK(!missing.vsync);
    CHECK(missing.sfxVolumePercent == 78);

    AppSettings saved{120, false, 35, true};
    saved.codexSeen=65;saved.bestWave=21;saved.bestScore=12345;saved.bestKills=67;saved.bestWon=false;
    saved.vsync=true;
    CHECK(save(path, saved));
    const auto loaded = load(path);
    CHECK(loaded.frameRateLimit == 120);
    CHECK(!loaded.showFps);
    CHECK(loaded.vsync);
    CHECK(loaded.sfxVolumePercent == 35);
    CHECK(loaded.tutorialCompleted);
    CHECK(loaded.codexSeen==65 && loaded.bestWave==21 && loaded.bestScore==12345 && loaded.bestKills==67 && !loaded.bestWon);
    CHECK(isBetterRun(loaded,false,22,0));
    CHECK(!isBetterRun(loaded,false,20,999999));
    CHECK(isBetterRun(loaded,false,21,12346));
    CHECK(!isBetterRun(loaded,false,21,12345));
    CHECK(isBetterRun(loaded,true,35,0));
    auto victory=loaded;victory.bestWon=true;victory.bestWave=35;
    CHECK(!isBetterRun(victory,false,35,999999));

    auto highRate=saved;highRate.frameRateLimit=240;
    CHECK(save(path,highRate));
    CHECK(load(path).frameRateLimit==240);
    highRate.frameRateLimit=0;
    CHECK(save(path,highRate));
    CHECK(load(path).frameRateLimit==0); // Unlimited remains a distinct saved choice.

    auto vsyncOff=saved;
    vsyncOff.vsync=false;
    CHECK(save(path,vsyncOff));
    const auto vsyncOffLoaded=load(path);
    CHECK(!vsyncOffLoaded.vsync);
    CHECK(vsyncOffLoaded.frameRateLimit==saved.frameRateLimit);
    CHECK(vsyncOffLoaded.showFps==saved.showFps);
    CHECK(vsyncOffLoaded.sfxVolumePercent==saved.sfxVolumePercent);
    CHECK(vsyncOffLoaded.tutorialCompleted==saved.tutorialCompleted);
    CHECK(vsyncOffLoaded.codexSeen==saved.codexSeen);
    CHECK(vsyncOffLoaded.bestWave==saved.bestWave);
    CHECK(vsyncOffLoaded.bestScore==saved.bestScore);
    CHECK(vsyncOffLoaded.bestKills==saved.bestKills);
    CHECK(vsyncOffLoaded.bestWon==saved.bestWon);

    auto paletteSettings=saved;
    CHECK(!selectPalette(paletteSettings,threadlight::cosmetics::PaletteId::VioletAscendant));
    CHECK(paletteSettings.selectedPalette==threadlight::cosmetics::PaletteId::OriginalCyan);
    CHECK(!grantVioletAscendant(paletteSettings,true,35,true,false)); // Fixtures cannot award it.
    CHECK(!grantVioletAscendant(paletteSettings,true,35,false,true)); // Tutorial cannot award it.
    CHECK(!grantVioletAscendant(paletteSettings,false,35,false,false));
    CHECK(!grantVioletAscendant(paletteSettings,true,34,false,false));
    CHECK(grantVioletAscendant(paletteSettings,true,35,false,false));
    CHECK(!grantVioletAscendant(paletteSettings,true,35,false,false)); // Permanent, first-win signal.
    CHECK(selectPalette(paletteSettings,threadlight::cosmetics::PaletteId::VioletAscendant));
    CHECK(save(path,paletteSettings));
    const auto paletteLoaded=load(path);
    CHECK(paletteLoaded.violetAscendantUnlocked);
    CHECK(paletteLoaded.selectedPalette==threadlight::cosmetics::PaletteId::VioletAscendant);

    const AppSettings updated{30, true, 80};
    CHECK(save(path, updated)); // Replacing an existing settings file is supported.
    const auto updatedSettings = load(path);
    CHECK(updatedSettings.frameRateLimit == 30);
    CHECK(updatedSettings.showFps);
    CHECK(updatedSettings.sfxVolumePercent == 80);
    CHECK(!updatedSettings.tutorialCompleted);
    {
        std::ofstream legacy(path, std::ios::binary | std::ios::trunc);
        legacy << "frame_rate_limit=120\nshow_fps=0\nsfx_volume_percent=40\n";
    }
    const auto legacySettings=load(path);
    CHECK(legacySettings.frameRateLimit==120);
    CHECK(!legacySettings.showFps);
    CHECK(legacySettings.sfxVolumePercent==40);
    CHECK(!legacySettings.vsync); // Older files have no vsync key.
    CHECK(legacySettings.selectedPalette==threadlight::cosmetics::PaletteId::OriginalCyan);
    CHECK(!legacySettings.violetAscendantUnlocked);

    {
        std::ofstream historical(path,std::ios::binary|std::ios::trunc);
        historical << "best_wave=35\nbest_won=1\n";
    }
    const auto historicalVictory=load(path);
    CHECK(historicalVictory.violetAscendantUnlocked); // Reliably identifiable legacy full victory.

    {
        std::ofstream locked(path,std::ios::binary|std::ios::trunc);
        locked << "selected_palette=1\n";
    }
    const auto lockedSelection=load(path);
    CHECK(lockedSelection.selectedPalette==threadlight::cosmetics::PaletteId::OriginalCyan);
    CHECK(!lockedSelection.violetAscendantUnlocked);

    {
        std::ofstream invalid(path, std::ios::binary | std::ios::trunc);
        invalid << "frame_rate_limit=144\nshow_fps=maybe\nvsync=2\nsfx_volume_percent=900\n";
    }
    const auto recovered = load(path);
    CHECK(recovered.frameRateLimit == 60);
    CHECK(recovered.showFps);
    CHECK(!recovered.vsync);
    CHECK(recovered.sfxVolumePercent == 78);
    {
        std::ofstream invalidVsync(path, std::ios::binary | std::ios::trunc);
        invalidVsync << "vsync=maybe\n";
    }
    CHECK(!load(path).vsync);

    {
        std::ofstream oversized(path, std::ios::binary | std::ios::trunc);
        oversized << std::string(5000, 'x');
    }
    const auto oversizedRecovery = load(path);
    CHECK(oversizedRecovery.frameRateLimit == 60);
    CHECK(oversizedRecovery.showFps);
    CHECK(!oversizedRecovery.vsync);
    CHECK(oversizedRecovery.sfxVolumePercent == 78);

    std::error_code error;
    std::filesystem::remove_all(directory, error);
    CHECK(!error);
}
void testInterruptedDragsPersistAtBoundaries() {
    using namespace threadlight::settings;
    const auto directory=std::filesystem::current_path()/
        ("ThreadLightDragTest-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path=directory/"settings.ini";
    // inside release, outside release, Esc, Tab, menu navigation, focus loss,
    // and shutdown all end an edit; continuous held frames never flush it.
    for(int interruption=0;interruption<7;++interruption) {
        AppSettings live;
        PendingEdits edits;
        int writes=0;
        auto persist=[&](){++writes;return save(path,live);};
        edits.dragging=true;
        for(int frame=0;frame<100;++frame) {
            live.sfxVolumePercent=frame%101;
            edits.dirty=true;
            if(edits.finishDrag(true,true,true,false))edits.flush(persist);
        }
        CHECK(writes==0);
        live.sfxVolumePercent=10+interruption*10; // Last live/release sample.
        const bool audioScreen=interruption!=3&&interruption!=4&&interruption!=6;
        const bool focused=interruption!=5;
        const bool down=interruption!=0&&interruption!=1;
        const bool key=interruption==2||interruption==3;
        CHECK(edits.finishDrag(audioScreen,focused,down,key));
        CHECK(!edits.dragging);
        CHECK(edits.flush(persist));
        CHECK(writes==1);
        CHECK(!edits.dirty);
        CHECK(edits.flush(persist)); // shutdown after an already saved edit
        CHECK(writes==1);
        CHECK(load(path).sfxVolumePercent==live.sfxVolumePercent);
        CHECK(!edits.finishDrag(true,true,true,false)); // No stale drag on re-entry.
    }
    PendingEdits failed;
    failed.dirty=true;
    CHECK(!failed.flush([](){return false;}));
    CHECK(failed.dirty);
    CHECK(failed.flush([](){return true;}));
    CHECK(!failed.dirty);
    std::error_code error;
    std::filesystem::remove_all(directory,error);
    CHECK(!error);
}

}

int main(int argc,char** argv) {
    if(argc==3) {
        using namespace threadlight::settings;
        const std::filesystem::path path=argv[2];
        if(std::string_view(argv[1])=="--write-interrupted") {
            PendingEdits edits{true,true};
            AppSettings live{120,false,35};
            if(!edits.finishDrag(false,false,true,false))return EXIT_FAILURE;
            return edits.flush([&](){return save(path,live);})?EXIT_SUCCESS:EXIT_FAILURE;
        }
        if(std::string_view(argv[1])=="--verify-interrupted") {
            const auto loaded=load(path);
            return loaded.frameRateLimit==120&&!loaded.showFps&&loaded.sfxVolumePercent==35?EXIT_SUCCESS:EXIT_FAILURE;
        }
        return EXIT_FAILURE;
    }
    testDefaultsAndLimits();
    testPersistenceAndInvalidRecovery();
    testInterruptedDragsPersistAtBoundaries();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Settings and persistence checks passed.\n";
}
