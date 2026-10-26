# Threadlight: The Briar Circuit

A small, self-contained top-down survival game built in C++20 with raylib 5.5. Weave a fading silk wake through the hunters and collect sparks manually.

This branch is the **M6 35-wave development campaign**, based on accepted M5 `f210e2cb43dce0abb4f6274520bb0d356c4fe735`. Earlier Waves 1-21 retain their accepted encounter data. Chapters IV-V add Sowers, temporary ground hazards, Hollow Weaver on Wave 28, and Briar Heart on Wave 35. Defeating the Heart wins the campaign. Staged signature rewards are Afterimage (7), Crossweave (14), Cocoon (21), and Resonant Loom (28), independently of manually collecting boss cores. All new balance remains provisional. See [M6-CHAPTERS-IV-V.md](M6-CHAPTERS-IV-V.md) for contracts, evidence, limitations and acceptance checks. Earlier milestone documents remain historical records.

## Play

- **WASD** or **arrow keys**: move
- **Space**: dash (briefly invulnerable)
- **P**: pause and resume during play; **Esc** does not pause or exit during play
- During an evolution choice, select with **1 / 2 / 3**, **A / D / S**, **Left / Right**, or the mouse; confirm with **Enter / Space** or the **Confirm Evolution** button
- **Enter** ends a safe collection period early; uncollected sparks carry over without being awarded
- **Enter** starts and retries; **R** restarts from pause; **Esc** returns to title while paused
- **Esc** on the title opens an exit confirmation; **Esc** in that dialog cancels

All visuals and sound effects are generated at runtime. No external art or audio assets are needed.

## Audio (Phase 2.1)

Effects are synthesized at startup and cover dash, silk hits and kills, spark collection, player damage, evolution, victory, and defeat. Each cue uses a short shaped envelope, layered tones, and slight pitch variation. The mixer keeps at most eight effects active; important feedback can replace a lower-priority sound, and simultaneous voices are attenuated to leave mix headroom. There is no background music.

Open **Settings** from the title or pause menu. The Video section selects a 30, 60, 120, or Unlimited frame-rate limit and toggles the measured FPS display. The Audio section provides a mouse-operated SFX slider and Left / Right keyboard adjustment in 5% steps. Volume changes apply immediately. Settings are saved under the current user's `%APPDATA%\ThreadLight\settings.ini`.

M1-M5 are accepted; M6 is a feature-branch candidate for owner acceptance. Historical Phase 2.2 pre-release package notes describe that older test package, not the status of the approved source baseline. The root `Threadlight.exe` is preserved and is not rebuilt by these changes; launch the new executable from its build directory.

### Manual listening check

Listen at a comfortable system volume, then try the Settings SFX slider at low, default, and maximum settings. Check that dash has a soft descending airy attack; enemy hits stay light while kills have a fuller resolving cue; collected sparks sound warm and understated without disappearing; player damage is lower and darker; evolution rises; and victory and defeat are clearly different. Trigger several events close together and listen for clipping, abrupt volume pumping, or a cue that masks player damage. Also confirm the red impact pulse and expanding ring are clear without flashing, while the dash remains visibly cyan. These perceptual checks need a human listener and are not covered by the automated tests.

## Windows build

To build from source, install Visual Studio 2022 C++ tools, CMake, and raylib 5.5 with a CMake package configuration (for example, the `x64-windows` raylib package from vcpkg). From this directory, configure and build:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Then run `build\Release\Threadlight.exe`. If raylib is installed outside vcpkg, configure `CMAKE_PREFIX_PATH` to its package prefix instead.

## Regression tests

Configure the thirty regression executables without the game executable with:

```powershell
cmake -S . -B build-tests -DTHREADLIGHT_BUILD_GAME=OFF
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

The foundation suites cover lifecycle boundaries, manual pickups, credit accounting, and a deterministic helper scenario at multiple render rates. M2 adds focused combat/rank/offer tests, Skitter state and render-schedule tests, and upgrade-card layout/hit testing. M3 adds chapter schedules, Rootwarden state/geometry, dash/boss HUD, and explicitly scripted geometric boss scenarios. M4 adds completed-dash echo/signature accounting, ranged projectile/AI invariants, combined attack coordination and render-rate integration, and explicitly scripted Chapter II combat scenarios. M5 adds chapter budgets, Crossweave/Bulwark boundaries, boss state/geometry/scenarios and combined boss/support coordination with render-rate checks. M6 adds chapter budgets, Sower and swept ground-hazard geometry, Cocoon/Resonance, spatial boss invariants and terminal reward/resize integration. No simulation result is a human playtest. The legacy logic suite now retains movement/spawn/control checks; its superseded automatic-wave-upgrade assertions have been removed. A passing test run does not establish balance, visual quality, or a human playthrough.

The preserved root executable was built with MSVC 19.44.35229.0, Windows x64, and raylib 5.5. Raylib's license is included in `RAYLIB-LICENSE.txt`.


## Full development campaign and isolated fixtures

Launch `build-phase34-m6-msvc-x64\Release\Threadlight.exe` for normal play from Wave 1.
Manual collection, deferred ordinary credits, four staged signature cards and permadeath remain.
There are no campaign checkpoints, automatic spark banking or boss healing.

For isolated acceptance testing only:

```powershell
.\build-phase34-m6-msvc-x64\Release\Threadlight.exe --m6-telemetry --m6-seed 7382 --m6-sower-fixture mixed
.\build-phase34-m6-msvc-x64\Release\Threadlight.exe --m6-telemetry --m6-seed 7382 --m6-hollow-fixture mobility
.\build-phase34-m6-msvc-x64\Release\Threadlight.exe --m6-telemetry --m6-seed 7382 --m6-heart-fixture mixed
```

Fixture names: `zero`, `offense`, `defense`, `mobility`, `mixed`. New Sower/Hollow
fixtures have 0 or 12 ordinary ranks and three signatures; Heart fixtures have
0 or 16 ranks and four signatures. These are visibly labeled **DEV FIXTURE**,
use normal health and damage, and are not campaign checkpoints. Existing M3-M5
fixtures remain available with their earlier rank budgets. Run without fixture
options to test normal progression and permadeath.

The development-only `--m6-render-check` option requires a Hollow/Heart fixture.
It creates a hidden, silent window, captures committed warning/active renders to
`m6-warning.png` and `m6-active.png` in the working directory, then exits. Use a
new output directory. It does not save settings or simulate a human playthrough.
