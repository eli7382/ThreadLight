# M1 Foundation Handoff

## Scope and status

M1 is a development harness for the new wave lifecycle, spark pickup clock, and reward
ledger. It is not the campaign implementation. The intended harness has five existing
legacy waves, the current legacy pacing/spawn rules, and the fixed legacy set of three
choices.

This handoff describes M1 based on `997ed098471c49883477e3d9c687bc382121ba22`
on `codex/phase34-foundation`. Automated verification results and their limits are
recorded below. Human Windows acceptance remains pending.

M1 does not implement the planned 35-wave campaign, bosses, signature abilities, ranked
upgrade catalog, or boss healing. The three temporary choices are Restitch (restore 35
vitality), Razor Filament (+0.65 silk damage), and Quickstep (+24 movement speed and
upgraded dash). They exist only to keep this harness playable; M2 owns progression and
offers.

## Ownership and contracts

| Module | Owns | Contract |
| --- | --- | --- |
| `src/wave_foundation.h` | 120 Hz encounter phase clock and phase transitions | The caller processes the current tick's combat/pickups first, then calls `WaveLifecycle::tick` once. Do not call it while paused. |
| `src/reward_ledger.h` | Run-scoped enemy/pickup accounting, score, spark remainder, credit issuance and resolution | IDs are monotonic in a run, zero is invalid, and each enemy has one terminal accounting path. Despawn is not defeat. |
| `src/spark_pickup.h` | Pickup motion, attraction, swept contact, combat-only age and terminal result | Call once per active simulation tick; pass previous/current player positions and whether the tick is combat. Preserve pickup state and position between encounters. |
| `src/main.cpp` | Owns gameplay entities and composes the modules | Register spawns, report kills/removals, pass authoritative pickup contact to the ledger, drive phases, and show one queued legacy choice per pending credit. |

### Encounter timing and boundaries

- Countdown is 90 ticks by default. It runs before spawning and does not increment
  spawning elapsed time.
- A normal encounter uses the existing legacy wave duration converted to 120 Hz ticks. The
  spawning-duration boundary enters Cleanup after that tick's combat processing.
- Cleanup lasts at most exactly 1,440 combat updates. On the 1,440th cleanup update, any
  surviving enemies are removed and recorded as despawns; they do not become kills, sparks,
  score, or credits. If no enemies remain earlier, collection starts on that tick.
- Collection lasts 480 ticks for a normal encounter or 600 ticks for a boss encounter. M1
  only uses normal encounters. It may end sooner when the player presses Enter or when no
  live pickups remain.
- A tick's clocks belong to the phase at its start. Therefore the tick that enters a new
  phase is not also counted as a tick in that new phase.
- Lethal combat ticks still finish lifecycle/removal bookkeeping before entering Lost.
  Defeat takes precedence over opening rewards or advancing to another encounter.
- `finishRewards` completes an encounter only from PendingRewards and only when no credits
  remain. The caller advances to the next legacy wave after completion.

### Pickup and reward accounting

- A pickup's lifetime is 2,160 combat ticks. Its age advances only while `combat` and
  `active` are both true. Pause/screens freeze the simulation loop and pickup clocks;
  non-combat collection ticks do not age pickups.
- Pickup contact is swept between the player's previous and current positions, including
  the pickup's movement. Contact is checked before lifetime expiry, so contact on the last
  (2,160th) combat tick wins.
- On encounter boundaries, live pickups carry over with their position, attraction, and
  combat age intact. Spark remainder also carries over between waves in the run. Neither is
  banked/reset at an encounter boundary; a full run reset clears the ledger.
- `RewardLedger::collectPickup` is called only after `tickPickup` reports Collected.
  Expiry and collection are terminal and idempotent; timeout despawn of an enemy does not
  create a pickup.
- Credit thresholds are selected by total credits already issued: 20 sparks for the first,
  30 for the second, 40 for the third, then 48 for each subsequent credit. Collection can
  issue multiple credits if the accumulated remainder crosses multiple thresholds.
- Pending credits resolve FIFO: only `nextCredit()` may resolve, and `resolveCredit`
  accepts it only in `Phase::PendingRewards`. The UI displays one `shownCredit` at a time.
  Early Enter in Collection advances the phase; it does not grant a reward or skip a pending
  choice.

### Pause, screens, and controls

Only Play advances fixed simulation ticks. Pause, title/settings/exit-confirmation
screens, upgrade choices, and win/loss screens freeze encounter and pickup clocks. Pausing
also clears accumulated fixed-step time and queued one-shot inputs. Returning to Play
resumes the same phase and pickup state.

M1 retains the existing controls: WASD/arrows move, Space dashes, P pauses, Enter
continues collection or confirms the shown legacy choice, 1/2/3 or A/D/S selects a choice,
and Enter/R restarts after win/loss. Verify these against the current UI and existing
control behavior during manual review.

## Build and runtime reproduction

Pinned Windows target: Visual Studio 2022, MSVC toolset `14.44.35207`, Windows SDK
`10.0.26100.0`, raylib `5.5`. CMake executable is under Visual Studio Community
`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`. From the repository root in a VS
2022 Developer PowerShell, configure and build with:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S . -B build-phase34-m1-msvc-x64 -G 'Visual Studio 17 2022' -A x64 -T version=14.44.35207 '-DCMAKE_SYSTEM_VERSION=10.0.26100.0' '-DCMAKE_PREFIX_PATH=C:/AI-Game-Benchmark/shared/raylib-5.5' -DBUILD_TESTING=ON
& $cmake --build build-phase34-m1-msvc-x64 --config Release
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir build-phase34-m1-msvc-x64 -C Release --output-on-failure
```

Launch from the repository root with the two options
as separate arguments:

```powershell
.\build-phase34-m1-msvc-x64\Release\Threadlight.exe --m1-telemetry --m1-seed 7382
```

`--m1-telemetry` enables raylib `TraceLog` stdout records; `--m1-seed 7382` fixes the
spawn RNG seed. Invalid or combined arguments are rejected. Compare scripted fixed-tick
input runs when checking deterministic event/counter sequences. Real manual input is
frame/window scheduled and can differ in tick timing and player path, so do not expect
identical gameplay traces across manual runs. Telemetry records seed, simulation tick,
wave, phase/elapsed ticks, event IDs/values, and ledger counters.

For a graphics-free reproducible scenario trace:

```powershell
.\build-phase34-m1-msvc-x64\Release\ThreadlightFoundationScenarioTests.exe --telemetry
```

This is explicitly a synthetic helper scenario, not recorded human play or measured
combat balance. Runtime `spark_*` fields count value units; `pickups_*` count objects.
`cleanup_timeout` records `value=1440`. Only actual defeat emits a death event; removal
never calls the death-effect/audio/drop path. Physical pickup contact alone adds pickup
score and spark value.

## Manual Windows checklist (owner acceptance pending)

- Confirm the title identifies the M1 development harness and clearly limits it to five
  legacy waves.
- Pause and resume during spawning, cleanup, and collection; verify phase elapsed time,
  enemy movement, pickup motion, and pickup age stop during pause and on non-Play screens.
- Carry an uncollected spark across an encounter transition; confirm its position/age
  persists and no remainder/credit state is reset or banked at the boundary.
- Allow cleanup to reach timeout; confirm survivors are removed as despawns with no kill,
  pickup, score, or credit reward. Check the 1,440-update boundary in telemetry.
- Check that attraction follows through turns/dashes and expires visibly; exact final-tick
  contact/expiry precedence is covered automatically rather than requiring 120 Hz manual timing.
- Press Enter early during collection; confirm it only opens PendingRewards. Resolve
  multiple credits one at a time in FIFO order using the fixed three legacy choices.
- Check ordinary movement, dash, pause, selection, confirmation, restart, and exit
  controls; inspect layout/mouse behavior at normal and high DPI/resized window sizes.
- Check event sound playback and settings volume behavior. Record unavailable audio
  devices or visual/manual evidence separately from build and scripted evidence.

## Verification record

- Lead runtime metadata: `gpt-6.1-sol` / `high`.
- Test worker: `gpt-6-luna` / `max`; documentation worker: `gpt-6-luna` / `high`.
  Lead independently checked parent-linked rollout `turn_context` metadata for both.
- Approved baseline: MSVC Release build succeeded; CTest **8/8 passed**.
- M1: MSVC 19.44.35229.0, C++20, x64 Release build succeeded; CTest **10/10 passed**.
  New checks stay active under NDEBUG. The lead reviewed both worker test sources and
  corrected their first scenario model before independently running them through CMake.
- The lead reviewed the final integration report and fixed lethal-tick bookkeeping;
  the final rebuild and all ten regression executables passed afterward. The helper trace
  matches across 30/60/120/144/165/1000 FPS schedules: 28 simulation ticks, three credits
  resolved on UI frames at frozen tick 28, eight spark remainder, score 525. These are
  synthetic fixture results, not measured gameplay. Invalid seed input exits with code 2.
- Candidate executable: `build-phase34-m1-msvc-x64/Release/Threadlight.exe`, SHA-256
  `E9631420712AA8E7B47D994FF03D1D86982A76A024ED77C7E5DDD2ECFC5FA89C`.
  PE imports are only WINMM/KERNEL32/USER32/GDI32/SHELL32. The preserved root executable's
  SHA-256 remains `CC25BF761D7BB3EA79A0D691CEF027735FAAB946236F3F262C5FD166B4F48FAB`.
- Sandbox SDK registry access failed (MSB4184); unchanged pinned configure/build commands
  succeeded through the approved toolchain path. Existing build directories were preserved.
- Desktop startup used isolated test APPDATA. Logs confirmed raylib 5.5, OpenGL/WASAPI
  initialization and a logical 1280x800 / framebuffer 1920x1050 window at startup scale 1.5.
  Computer Use app approval timed out before capture/input inspection. No interactive
  gameplay, resized-window, visual-quality, or perceptual-audio acceptance is claimed.
- No boss encounters/signatures, ranked progression, full campaign, boss restoration,
  final economy tuning, or premium dash HUD are part of M1.
