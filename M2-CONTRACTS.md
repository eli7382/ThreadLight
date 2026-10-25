# M2 implementation boundaries

Base: accepted M1 `115b323df60d1196fee6274414e7a535e74b0612`.
Branch: `codex/phase34-m2`. Five-wave development harness only.

The lead owns shared combat, ranked upgrades/offers, main integration, CMake, and
existing regression adapters. M1 ledger/lifecycle policies are not redesigned.

- `game_geometry.h`: shared logical-space Point and squared-distance operations;
  the existing M1 pickup API retains its namespace and behavior.
- `combat.h`: integer hundredths for HP/damage, one silk gate per enemy, one
  post-hit gate per player, swept collisions, deterministic strongest-contact selection.
  EnemyHealth owns enemy HP/gate. PlayerHealth owns player HP/maximum/gate.
  Only the M1 RewardLedger owns kill/score/pickup/credit accounting.
- `player_upgrades.h`: seven permanent rank records plus repeatable Restitch;
  derive stats from ranks, never additive permanent mutation. Credit resolution and
  rank/effect application validate together. Fixed seeded offer history is separate
  from spawn/cosmetic randomness; shown offers do not regenerate on redraw.
- `skitter.h`: isolated movement state machine, no HP/reward/rendering ownership.
  Interface uses `foundation::Point`, `skitter::Bounds {left,right,top,bottom}`,
  `Phase {Flank,Windup,Burst,Recover}`, and `State` with phase, ticksRemaining,
  burstStart, burstEnd, lockedDirection, side. `tick(State&, Point& position,
  Point& velocity, Point player, Point playerMotion, Bounds, bool canCommit)`
  returns flags telegraphStarted/burstStarted/recovered. `committed(State)` counts
  Windup or Burst. Timings: flank255, warning84, burst39, recovery78 ticks.
  Speed115; burst260; flanking offset90; no motion during warning/recovery;
  windup locks a straight endpoint clipped along its ray into bounds; burst never homes.
  No commit while canCommit=false. Caller allows at most two commitments.
- `upgrade_layout.h`: pure logical-space panel/card rectangles and hit testing;
  no raylib, simulation, offers, or raw native framebuffer coordinates.

Worker ownership is disjoint: Skitter worker owns only skitter.h and skitter_test.cpp;
UI helper worker owns only upgrade_layout.h and upgrade_layout_test.cpp.
Workers do not edit main, CMake, other tests, or Git refs. The lead reviews their work.

Initial ranks use the final design values: Razor V, Living III, Broad III, Quick V,
Rhythm III, Heartwood V, Magnet III; Restitch restores35 HP when useful. No Ironbark,
Longstride, bosses, signatures, Spitter, other new enemies, or complete campaign.

## Implemented behavior and provisional values

All HP/damage values in this section use display units; the combat records store
integer hundredths. No enemy HP scaling or campaign expansion is added in M2.

| Upgrade | Rank 0 | Successive ranks | Cap |
| --- | --- | --- | --- |
| Razor Filament, silk damage | 1.00 | 1.30 / 1.65 / 2.00 / 2.35 / 2.70 | V |
| Living Thread, active seconds | 1.20 | 1.50 / 1.80 / 2.10 | III |
| Broadweave, collision half-width | 5 | 7 / 9 / 11 | III |
| Quickstep, movement units/s | 255 | 269 / 282 / 294 / 305 / 315 | V |
| Dash Rhythm, cooldown seconds | 1.85 | 1.70 / 1.55 / 1.40 | III |
| Heartwood, maximum HP | 100 | 112 / 124 / 136 / 148 / 160 | V |
| Spark Magnet, attraction / initial pull | 110 / 220 | 145 / 270; 180 / 320; 220 / 380 | III |
| Restitch | -- | Restore up to 35 HP; eligible at 15+ missing HP | Unlimited recovery |

Heartwood adds 12 current and maximum HP, preserving the absolute HP deficit.
Other rank selections do not heal. Restitch cannot revive. Dash duration and its
3.7 movement multiplier are unchanged; maximum movement is 315 and minimum cooldown
1.40 seconds. A cooldown already running is not reset when choosing Rhythm.
Living Thread also delays silk's visual cooling; Broadweave widens its drawing.
The harmless silk lifetime remains 5.5 seconds, with the existing 520-knot limit.

| Enemy | HP | Contact damage | Radius | Speed | Spark value / kill score |
| --- | --- | --- | --- | --- | --- |
| Hunter | 1 | 10 | 14 | 95 | 1 / 10 |
| Brute | 3 | 16 | 20 | 66 | 2 / 35 |
| Skitter | 1.5 | 12 | 11 | 115 flank, at most 260 burst without resizing | 1 / 15 |

Hunter/Brute retain their pursuit behavior. Skitters flank relative to movement,
then stop for a 0.70-second locked-lane warning, burst for 0.325 seconds, and stop
for 0.65-second recovery. Their flank timer is 2.125 seconds. Maximum burst travel
is 84.5 units, shortened near walls. At most two Skitters may wind up/burst together;
others continue flanking until a slot opens. A coral triangle and bounded lane
separate them from legacy foes. All phases remain vulnerable to ordinary silk;
move perpendicular to the warning, weave across the lane, or dash through it.
Spawns remain safe and deterministic. Wave 4 guarantees one initial Skitter;
thereafter Waves 4/5 use 20% Skitters, 30% Brutes, 50% Hunters. Earlier composition,
spawn intervals, durations, and five-wave completion remain the legacy harness.

Enemy silk gates are exactly 33 ticks; player post-hit immunity is 94 ticks.
Swept silk and relative swept body contact prevent tunneling. Enemy death is
resolved before its body-contact contribution; strongest simultaneous live contact
wins, with lowest reward ID breaking equal-damage ties. One player gate owns all
incoming contact damage. A broad-phase rejection bounds unnecessary geometry work.
The ledger alone terminally accounts for deaths and timeout removals.

### Offers and credit resolution

Three distinct eligible cards normally appear. Maximum ranks leave the pool.
Rank-V base weight is 1.15 times Rank-III; invested ranks receive a 1.35 multiplier.
An invested eligible rank absent for three consecutive offers becomes eligible for
the single longest-absence reservation (seeded tie break); competing investments
can delay each other. The third slot reserves discovery when the earlier slots
contain an investment but no unowned upgrade. There is no focus selector.

Restitch weight rises with missing HP. At 35+ missing HP, a qualifying offer that
omits it makes the next qualifying offer include it. Recovery is never offered at
full health or for less than 15 missing HP. Spark Magnet is excluded after the
last harness encounter because no collecting combat follows it.

Offer RNG is xorshift32, seeded independently from spawns; generating a credit's
offer updates history exactly once. Redraws cannot reroll it. FIFO ledger resolution
and eligibility validate before applying one rank/recovery. Queued credits each get
a fresh offer using the preceding choice's ranks and HP. Menus freeze combat and
pickup clocks. Mouse and keyboard use the same logical-space card geometry.

Development-only exhaustion safeguard: if every eligible permanent rank is maxed
and useful recovery is unavailable, a zero-card screen explicitly offers **DISCARD
CREDIT**. Only confirmation consumes it, without stats/healing/rewards; duplicate
confirmation cannot consume that credit again. This cannot naturally occur within
the current five-wave reward budget. It is not a final full-campaign overflow policy.
The final legacy encounter still spends queued credits before harness completion,
as in M1; it is not the eventual final-boss victory/reward flow.

### Preserved M1 contracts

`wave_foundation.h`, `reward_ledger.h`, timing, settings, menu infrastructure, and
audio synthesis/mixing are unchanged. Cleanup remains exactly at most 1,440 ticks;
timeout removal calls only ledger despawn, never death/drop/effects. Pickups still
require contact, age for 2,160 combat ticks, freeze during transitions/screens, and
carry over without automatic banking. Collection remains 480 normal/600 boss ticks
(the harness has no boss encounters), with early continuation. Thresholds remain
20, 30, 40, then 48. There is no boss healing.

Resizing remaps Skitter position and both locked endpoints between enemy-center
bounds, preserving phase/tick progress and updating direction. The lane stretches
with the resized arena; this is a coordinate remap rather than retargeting the player.
Existing interpolation history is recaptured after resize, as in M1.

## Verification and reproduction

Verified Windows configuration: MSVC 19.44.35229.0 / toolset 14.44.35207,
Windows SDK 10.0.26100.0, raylib 5.5, Release x64, C++20. Build separately from
all accepted executables:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S . -B build-phase34-m2-msvc-x64 -G 'Visual Studio 17 2022' -A x64 -T 'version=14.44.35207' '-DCMAKE_SYSTEM_VERSION=10.0.26100.0' '-DCMAKE_PREFIX_PATH=C:/AI-Game-Benchmark/shared/raylib-5.5'
& $cmake --build build-phase34-m2-msvc-x64 --config Release --parallel 4
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir build-phase34-m2-msvc-x64 -C Release -R 'CombatUpgrades|Skitter|UpgradeLayout' --output-on-failure
& $ctest --test-dir build-phase34-m2-msvc-x64 -C Release --output-on-failure
.\build-phase34-m2-msvc-x64\Release\Threadlight.exe --m2-telemetry --m2-seed 7382
```

Final focused suites: **3/3 passed**. Full regression: **13/13 passed, zero failed**.
Build completed successfully. New worker suites were also independently compiled
and executed using g++ C++20 with NDEBUG and strict warnings. Checks remain active
in Release. Combat tests cover exact hit gates, HP breakpoints, swept geometry,
strongest-hit ordering, ranks/caps, healing/death/FIFO credit boundaries, 400 seeded
first offers, cached generation, investment/recovery reservations, ten queued
credits with duplicate attempts, five-Skitter commitment pressure, exhausted offers,
and committed-lane resize. Skitter schedules match at 30/60/120/144/165/1000 render
FPS using the fixed-step clock. UI tests cover supported logical sizes down to
960x640, disjoint cards, gaps/half-open edges, and reduced card counts. Existing
M1 lifecycle, pickup, accounting, timing, settings, audio, and presentation tests pass.

Desktop startup/render smoke: M2 title observed, OpenGL 3.3 and WASAPI initialized,
isolated settings used, normal window close and audio/graphics shutdown observed.
The initial sandbox-only launch did not reach a usable window; the desktop-enabled
launch did. This smoke is not a combat playthrough or listening/balance acceptance.
Build logs and local smoke logs are untracked under the M2 build directory.

`--m1-telemetry` and `--m1-seed` remain compatible aliases. Existing M1 ledger event
format is preserved. M2 adds enemy kind/HP, windup/burst events and resolved rank,
derived damage/speed/cooldown/HP. Telemetry is opt-in, with no per-tick logging.

## Review, limitations, and owner checks

Architecture/integration/review: GPT-6.1 Sol, High. Skitter module/tests and independent
integration review: GPT-6 Luna, Max. Layout helper/tests: GPT-6 Luna, High. Models and
efforts were checked in runtime turn metadata. Worker ownership was disjoint; lead
reviewed changes, independently ran focused checks, and ran the Windows build/full
suite. Review found and prompted the exhaustion/resize fixes above; the worker
rechecked them read-only. Automated success is not balance or visual acceptance.

Balance remains provisional. The small legacy HP set cannot validate all offensive
ranks: Razor II changes Skitter from two contacts to one and Brute from three to two;
Razor I and several later ranks do not change these enemies' isolated kill counts.
Future roster/chapter HP breakpoints need measured tuning; no hidden damage effect
or enemy HP inflation was added to make those ranks appear stronger. The short harness
also cannot naturally reach every maximum. No full-campaign offer-distribution,
late-game performance, boss viability, or human perception claim is made here.

Owner manual acceptance, using the new build executable:

- Play through all five encounters; verify ordinary movement/dash/silk feel, damage
  feedback and pacing, with Skitters first appearing in Wave 4.
- Read a Skitter warning, change direction, and confirm the burst follows its locked
  lane; punish windup/recovery with silk, and test dash contact protection.
- Pause during a warning/burst and resize in each phase; verify no hidden timer
  advance, retarget, missing lane, or rendering/collision mismatch.
- Choose repeated ranks; check exact card numbers, rank pips, disappearance at maximum,
  separate Quickstep/Rhythm effects, Heartwood current/max HP, and useful Restitch.
- Use keyboard and mouse selection/confirmation at minimum and larger window sizes
  and normal/high startup DPI; check text fitting, card focus, and stationary-pointer
  behavior after keyboard input.
- Confirm pending choices remain safe and sequential; recheck manual pickup carryover
  without banking and cleanup survivor removal without rewards. Check audio/settings
  and perceived frame pacing at configured limits.

No bosses, signatures, Spitter, other new archetypes, complete catalog/campaign,
premium dash HUD, or M3+ work is included. Keep main/develop unchanged; feature branch
only, pending owner acceptance. Existing builds and unrelated untracked work remain.
