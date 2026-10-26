# Threadlight: The Briar Circuit

A small, self-contained top-down survival game built in C++20 with raylib 5.5. Weave a fading silk wake through the hunters and collect sparks manually.

This branch is the **M7 campaign acceptance candidate**, based on accepted M6 `4ec554b11024d477ba31bb825e5f8756321de881`. Play five chapters and 35 waves, ending with Briar Heart. M7 completes the ordinary catalog with Ironbark, Longstride, Needlecatch and Bloodstitch, and replaces staged single gifts with three-choice rewards from six signatures. Player base health remains 100 HP. New balance is provisional; automated tests do not replace a full human campaign acceptance run. See [M7-CAMPAIGN-ACCEPTANCE.md](M7-CAMPAIGN-ACCEPTANCE.md) for exact rules, evidence and acceptance checks. Earlier milestone documents remain historical records.

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

M1-M6 are accepted; M7 is a feature-branch candidate for owner acceptance. Historical Phase 2.2 pre-release package notes describe that older test package, not the status of the approved source baseline. The root `Threadlight.exe` is preserved and is not rebuilt by these changes; launch the new executable from its build directory.

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

Configure the thirty-four regression executables without the game executable with:

```powershell
cmake -S . -B build-tests -DTHREADLIGHT_BUILD_GAME=OFF
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

The foundation suites cover lifecycle boundaries, manual pickups, credit accounting, and a deterministic helper scenario at multiple render rates. M2 adds focused combat/rank/offer tests, Skitter state and render-schedule tests, and upgrade-card layout/hit testing. M3 adds chapter schedules, Rootwarden state/geometry, dash/boss HUD, and explicitly scripted geometric boss scenarios. M4 adds completed-dash echo/signature accounting, ranged projectile/AI invariants, combined attack coordination and render-rate integration, and explicitly scripted Chapter II combat scenarios. M5 adds chapter budgets, Crossweave/Bulwark boundaries, boss state/geometry/scenarios and combined boss/support coordination with render-rate checks. M6 adds chapter budgets, Sower and swept ground-hazard geometry, Cocoon/Resonance, spatial boss invariants and terminal reward/resize integration. M7 adds ranked utility/healing boundaries, six-signature seeded offers, chronological cross-pool interception, damage reduction, render-rate integration and an explicitly assumed campaign economy/offer model. No simulation result is a human playtest. The legacy logic suite now retains movement/spawn/control checks; its superseded automatic-wave-upgrade assertions have been removed. A passing test run does not establish balance, visual quality, or a human playthrough.

The preserved root executable was built with MSVC 19.44.35229.0, Windows x64, and raylib 5.5. Raylib's license is included in `RAYLIB-LICENSE.txt`.


## Campaign and isolated development fixtures

Launch `build-phase34-m7-msvc-x64\Release\Threadlight.exe` for normal play from Wave 1.
Manual collection, deferred ordinary credits, four signature choices and permadeath remain.
There are no campaign checkpoints, automatic spark banking or boss healing.

Needlecatch's active silk intercepts one projectile, then cools down for 5/4/3
combat seconds at Rank I/II/III. It grants no player immunity. Bloodstitch heals
3 HP per 5/4/3 direct ordinary-silk kills, capped at 9/12/15 HP per encounter.
Signature kills, timeout removal and full-health kill groups do not bank healing.

For isolated acceptance testing only:

```powershell
.\build-phase34-m7-msvc-x64\Release\Threadlight.exe --m7-utility-fixture --m7-telemetry
.\build-phase34-m7-msvc-x64\Release\Threadlight.exe --m7-reward-fixture 1
.\build-phase34-m7-msvc-x64\Release\Threadlight.exe --m7-reward-fixture 4
.\build-phase34-m7-msvc-x64\Release\Threadlight.exe --m6-hollow-fixture mobility --m7-telemetry
.\build-phase34-m7-msvc-x64\Release\Threadlight.exe --m6-heart-fixture mixed --m7-telemetry
```

The utility fixture starts Wave8 at 55/100 HP with NeedlecatchIII,
BloodstitchIII, RazorI and LivingI. Its eight ranks deliberately isolate the
new powers; this is not a claimed normal Wave8 build. Reward fixtures (1-4)
start directly at a clearly labeled signature screen with representative prior
ownership. They bypass combat and collection solely for selection/UI testing;
they are not earned victories, checkpoints, or end-to-end progression evidence.

Existing M3-M6 enemy/boss fixture names `zero`, `offense`, `defense`, `mobility`,
`mixed` retain their earlier budgets and staged test ownership. Normal campaign
signatures are chosen through the new six-item pool instead. Run without fixture
options to test normal progression and permadeath.

`--m7-render-check` creates a hidden, silent window for a reward/utility fixture
(or existing M6 spatial-boss fixture), captures its frame and exits. Optional
`--m7-min-window` tests minimum-size geometry and requires render-check mode.
Use a new working directory for captures; settings are not saved. These scripted
checks do not establish human gameplay, sound quality or visual acceptance.
