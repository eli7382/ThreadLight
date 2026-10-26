# Threadlight: The Briar Circuit

A small, self-contained top-down survival game built in C++20 with raylib 5.5. Weave a fading silk wake through the hunters and collect sparks manually.

This branch is the **M7 campaign acceptance candidate**, based on accepted M6 `4ec554b11024d477ba31bb825e5f8756321de881`. Play five chapters and 35 waves, ending with Briar Heart. M7 completes the ordinary catalog with Ironbark, Longstride, Needlecatch and Bloodstitch, and replaces staged single gifts with three-choice rewards from six signatures. Player base health remains 100 HP. New balance is provisional; automated tests do not replace a full human campaign acceptance run. See [M7-CAMPAIGN-ACCEPTANCE.md](M7-CAMPAIGN-ACCEPTANCE.md) for exact rules, evidence and acceptance checks. Earlier milestone documents remain historical records.

## Play

- **WASD** or **arrow keys**: move
- **Space**: dash (briefly invulnerable)
- **P**: pause and resume during play; **Esc** does not pause or exit during play
- Losing window focus or minimizing automatically pauses active play. Return to the game and resume intentionally.
- During an evolution or signature choice, **A / D** or **Left / Right** move between cards, **S** selects the middle, and **1 / 2 / 3** select directly; confirm with **Enter / Space** or the button.
- **Enter** ends a safe collection period early; uncollected sparks carry over without being awarded
- **Enter** starts and retries; from pause, **R** requests a restart and **Esc** requests a return to title. Both ask for confirmation.
- In the pause menu, choose **Upgrades & Signatures** or press **I** to inspect your build; **I** also opens it from the run recap.
- **Esc** on the title opens an exit confirmation; **Esc** in that dialog cancels

All visuals and sound effects are generated at runtime. No external art or audio assets are needed.

On the first normal game, a separate practice tutorial introduces movement, dash, silk combat, and spark collection with ten hunters. Hold **B** for three seconds to skip, or press **Enter** after defeating all ten to begin the campaign. Practice progress is reset before the run starts; completing or skipping the tutorial is saved in settings. Development fixtures bypass the tutorial.

The main menu's enemy codex unlocks entries as enemies are encountered in normal play. Best Run records completed normal runs, ranking victory first, then the furthest wave, then score. Development fixtures do not update either record.

## Audio (Phase 2.1)

Effects are synthesized at startup and cover dash, silk hits and kills, spark collection, player damage, evolution, victory, and defeat. Each cue uses a short shaped envelope, layered tones, and slight pitch variation. The mixer keeps at most eight effects active; important feedback can replace a lower-priority sound, and simultaneous voices are attenuated to leave mix headroom. There is no background music.

Open **Settings** from the title or pause menu. The Video section selects a 30, 60, 120, or Unlimited frame-rate limit and toggles the measured FPS display. The Audio section provides a mouse-operated SFX slider and Left / Right keyboard adjustment in 5% steps. Volume changes apply immediately. Settings are saved under the current user's `%APPDATA%\ThreadLight\settings.ini`.

M1-M6 are accepted; M7 is a feature-branch candidate for owner acceptance. Historical Phase 2.2 pre-release package notes describe that older test package, not the status of the approved source baseline. The root `Threadlight.exe` is preserved and is not rebuilt by these changes; launch the new executable from its build directory.

### Manual listening check

Listen at a comfortable system volume, then try the Settings SFX slider at low, default, and maximum settings. Check that dash has a soft descending airy attack; enemy hits stay light while kills have a fuller resolving cue; collected sparks sound warm and understated without disappearing; player damage is lower and darker; evolution rises; and victory and defeat are clearly different. Trigger several events close together and listen for clipping, abrupt volume pumping, or a cue that masks player damage. Also confirm the red impact pulse and expanding ring are clear without flashing, while the dash remains visibly cyan. These perceptual checks need a human listener and are not covered by the automated tests.

## Windows build

To build from source, install Visual Studio 2022 C++ tools, CMake, and an MSVC-compatible raylib 5.5 package with a CMake configuration and a matching static CRT (for example, the `x64-windows-static` raylib package from vcpkg). From this directory, configure and build:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

Then run `build\Release\Threadlight.exe`. If raylib is installed outside vcpkg, configure `CMAKE_PREFIX_PATH` to its package prefix instead.

For a reproducible build with this project's MSVC-compatible static raylib package,
install MSVC toolset **14.44.35207** and CMake 3.21 or newer, then point the preset
at the package root (it must contain `include/raylib.h`, `lib/raylib.lib`, and
`lib/cmake/raylib/raylib-config.cmake`):

```powershell
$env:THREADLIGHT_RAYLIB_ROOT = 'C:\AI-Game-Benchmark\shared\raylib-5.5'
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The preset uses a new `build-windows-msvc` directory and the matching static CRT.
Use a separate build directory when choosing a different installed toolset or raylib
package. MSVC cannot link a MinGW `.a` archive; configuration reports that mismatch.

## Opt-in performance capture

Run the new executable with `--perf-log` followed by a **new**, writable CSV path:

```powershell
.\build-windows-msvc\Release\Threadlight.exe --perf-log C:\Temp\threadlight-run.csv
```

The game refuses to overwrite an existing capture. Normal launches do not record
diagnostics. Capture representative early and late waves and boss encounters at
the desired frame limit; repeat the same route and settings for comparisons.
The CSV contains frame-start intervals, raylib frame time, wall time spent in
update, simulation, draw submission and `EndDrawing`, simulation steps and time
discarded by the existing eight-step cap, frame-start mode, post-update phase and
wave, framebuffer dimensions, entity counts and once-per-second Windows process
working-set/private-memory samples. `elapsed_s` starts before window initialization,
so the first row gives a rough startup-to-first-loop time. Missing memory readings
are blank. The last known memory sample is repeated between samples.

On a normal exit, the log output reports frame-interval p50/p95/p99 for all
captured modes using bounded
0.1 ms histogram buckets (values above 1000 ms use the observed maximum as an
upper bound), the maximum, and total/count of frames with discarded simulation
time. The first frame has no preceding interval and is excluded from percentiles.
Intentional clock clears at pause, resize and screen transitions are not counted as
overload drops. Per-frame CSV writes and sampling add overhead, so compare like
captures and verify the result with profiling disabled. Timing columns are wall
times, not CPU utilization or GPU timings: draw submission and `EndDrawing` can
include driver work, GPU synchronization, buffer presentation, the frame limiter
and input polling. Draw submission also includes end-of-frame audio maintenance
and any fixture screenshot work. Filter CSV rows by `mode_at_start`, phase and
wave when comparing gameplay; use an external GPU profiler to measure GPU time and memory.
Hidden `--m7-render-check` fixtures may also be combined with `--perf-log` for a
smoke test, but their screenshots and capture work are not representative gameplay.

## Regression tests

Configure the thirty-five regression executables without the game executable with:

```powershell
cmake -S . -B build-tests -DTHREADLIGHT_BUILD_GAME=OFF
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

The foundation suites cover lifecycle boundaries, manual pickups, credit accounting, and a deterministic helper scenario at multiple render rates. M2 adds focused combat/rank/offer tests, Skitter state and render-schedule tests, and upgrade-card layout/hit testing. M3 adds chapter schedules, Rootwarden state/geometry, dash/boss HUD, and explicitly scripted geometric boss scenarios. M4 adds completed-dash echo/signature accounting, ranged projectile/AI invariants, combined attack coordination and render-rate integration, and explicitly scripted Chapter II combat scenarios. M5 adds chapter budgets, Crossweave/Bulwark boundaries, boss state/geometry/scenarios and combined boss/support coordination with render-rate checks. M6 adds chapter budgets, Sower and swept ground-hazard geometry, Cocoon/Resonance, spatial boss invariants and terminal reward/resize integration. M7 adds ranked utility/healing boundaries, six-signature seeded offers, chronological cross-pool interception, damage reduction, render-rate integration and an explicitly assumed campaign economy/offer model. When the game is built, an additional mocked raylib test checks that sound loading preserves the synthesized waveforms and does not free borrowed sample storage. No simulation result is a human playtest. The legacy logic suite now retains movement/spawn/control checks; its superseded automatic-wave-upgrade assertions have been removed. A passing test run does not establish balance, visual quality, or a human playthrough.

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
