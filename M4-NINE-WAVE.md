# M4 nine-wave implementation contracts

Accepted base: `001e56238cc91ef237389745ad661c64348186d0`.
Branch: `codex/phase34-m4`. Scope: Afterimage, Spitter, Waves 8-9 only.

The lead owns shared combat/chapter stats, coordination, encounter data,
main/input/render integration, CMake, docs and acceptance. Workers have disjoint
new-header/test ownership. No worker changes main or Git refs.

- Signature ownership is independent of ordinary credits and core pickup. The
  Rootwarden collection -> ordinary choices -> one signature card -> Wave 8 chain
  validates survival and grants once. Normal death clears run ownership.
- Afterimage records the actual combat dash path. Completed nonzero movement
  creates an echo for 78 ticks, half-width8. Each stable target ID is damaged once;
  round nearest integer hundredth once (3/4 ordinary, 3/8 bosses). It never reads,
  resets or consumes the normal silk gate. No recursive procs or player benefits.
- Spitter windup108 ticks, soft aim30 then locked78; one180-speed radius5 projectile,
  360-tick lifetime, 14 damage. Seek cooldown324 ticks gives432-tick launch cadence
  absent deferral. Movement72 maintains approximate200-300 range, stationary windup.
- Projectile slots64 include reservations. A slot is reserved before showing a
  warning. A reserved ticket can launch once; death cancels an unfired ticket only.
  Swept physical contact consumes the projectile regardless of player immunity.
  Caller alone submits impacts to the common IncomingHit owner.
- Chapter II coordinates at most two special commitments across Skitter and
  Spitter. Persistent FIFO readiness with stable-ID ties prevents starvation;
  Spitter warning starts/therefore launches are separated by at least42 ticks.
  No catch-up firing or shortened telegraphs. Chapter I keeps M3's existing logic.
- All offensive echo/projectile/attack state clears at safe encounter boundaries;
  pause/selection freezes clocks and resize preserves committed geometry.
- After Wave9 collection, stop with Nine-Wave Slice Complete and preserve ledger
  balances/credits/live pickups. No unusable post-slice upgrades or campaign victory.

## Encounters and provisional balance

Chapter I schedules, combat values, Rootwarden HP/attacks/exposure, player ranks,
offers, spark thresholds, cleanup and collection rules retain accepted M3 behavior.
No healing is added on boss defeat.

| Wave | Spawning | Composition | Identity and timing |
|---|---|---|---|
| 8 Two Needles | 26 s | 18 Hunters, 2 Brutes, 4 Skitters, 2 Spitters | Alternating E/W pursuit at 1.3 s intervals; Brutes at 2/14 s; N/S Skitters at 5/10/15/20 s; N/S Spitters at 4/12 s. |
| 9 Turning Watch | 28 s | 8 Hunters, 2 Brutes, 8 Skitters, 8 Spitters | Four sector relays beginning 0/6.5/13/19.5 s; Spitters at +0/+2 s, opposite Hunters +.5/+1.5 s, side Skitters +2.5/+3.5 s; Brutes +4 s in sectors 0/2. |

All directions receive the existing small seeded angle offset and safe-spawn
projection. The last scheduled entry precedes spawning end; no adaptive rewards.

| Chapter II enemy | HP | Contact HP | Movement | Sparks / score |
|---|---:|---:|---:|---:|
| Hunter | 2 | 11 | 105 | 1 / 10 |
| Brute | 4.5 | 18 | 70 | 2 / 35 |
| Skitter | 2 | 13 | 125 flank, accepted 260 burst | 1 / 15 |
| Spitter | 3 | 12 | 72 seek, stationary warning | 2 / 30 |

Health/damage use integer hundredths. Afterimage therefore deals .75 HP at base
silk damage1, .98 at Razor I (1.30), 1.24 at Razor II (1.65), 1.50 at Razor III
(2.00). Boss values are .38/.49/.62/.75. There is no extra exposed-core bonus for
signature hits. Ordinary silk retains its 33-tick per-target gate. An echo can
finish a wounded enemy during that gate; it cannot repeatedly hit the same ID.
Its 32-point path budget covers the accepted dash; 128 contact claims fail closed.
No new player control, projectile interception, dash refund or immunity is added.

Perfect collection ceilings: normal Chapter I128 + finite Hunters4 + core10 +
Wave8 30 + Wave9 36 =208 spark value. Thresholds20/30/40/48/48 issue at most five
ordinary credits with22 remainder; the final unspent credit can remain recorded.
These are arithmetic ceilings, not measured player earnings or balance targets.

## Ownership and integration

- `combat.h` owns HP/death and incoming-hit arbitration; signature and ordinary
  silk share damage calculation but not source-specific cooldowns.
- `ranged_combat.h` owns Spitter clocks, aiming and fixed projectile slots;
  `attack_coordinator.h` owns deterministic Chapter II admission only.
- `afterimage.h` owns path/TTL/contact claims, never health/rewards. The existing
  thread, spark and effect budgets remain; projectiles have64 fixed slots.
- `signature_reward.h` owns one signature reward identity. `RewardLedger` remains
  the independent, exact-once owner of kills, pickups, score and ordinary credits.
- `chapter_one.h` retains its historical filename and now holds the bounded
  nine-wave data. No generalized campaign or entity framework is introduced.
- `main.cpp` integrates input, movement, damage events, transitions, rendering,
  audio and resize transforms. Noncombat collection cancels all threats. Flying
  shots survive their source's death until impact/expiry or encounter cleanup.

The ordinary-credit screen and signature screen process input only when that
screen was active at frame start. Every mode transition clears queued actions
and fixed-step debt. Boss collection remains five seconds, manual and safe.
Wave9 ends before offering unusable ordinary credits. Retry resets all ownership.

## Development fixtures and telemetry

Normal executable: `build-phase34-m4-msvc-x64/Release/Threadlight.exe`.
`--m4-telemetry --m4-seed 7382` enables reproducible gameplay event logs; it does not
change rewards. Existing M1-M3 aliases remain compatible. Compact M4 summaries
report cumulative echo creation, contacts and HP damage in hundredths; events
record Spitter warning/lock, projectile launch/physical contact and signature
queue/accept transitions. Existing collection and ledger telemetry remains.

For isolated Windows testing, `--m4-wave-fixture zero|offense|defense|mobility|mixed`
starts Wave8 with Afterimage. Zero has no ordinary ranks; other fixtures have4:
RazorIII/LivingI, HeartwoodIII/LivingI, QuickII/RhythmII, or
RazorI/LivingII/HeartwoodI. M3 boss fixtures still start Rootwarden with0-3 ranks.
Fixtures have normal HP/damage, are labeled DEV FIXTURE and are not campaign
checkpoints; their synthetic carry-in ranks do not fabricate ledger credits.
Fixture options are mutually exclusive. Invalid CLI arguments exit2.

## Internal checkpoints and agents

1. Added bounded encounter data and Chapter II stat boundary; verified Chapter I
   compositions plus new budgets, spawn deadlines and deterministic schedules.
2. Integrated completed-dash echo, shared damage, ranged reservation/launch and
   signature reward ordering; ran focused invariant tests before presentation.
3. Added restrained echo/Spitter geometry, signature card, cues and development
   fixture; compiled and inspected actual Windows runtime.
4. Independent shared-combat/integration review, scripted scenario comparisons,
   final Release regression and feature-branch handoff.

Applied model/effort settings were checked in runtime turn metadata:
lead GPT-6.1 Sol High; Afterimage/signature and scenario worker GPT-6 Luna Max;
Spitter/projectile worker GPT-6 Luna Max; audio worker GPT-6 Luna High; independent
read-only integration reviewer GPT-6.1 Sol High. Lead reviewed all worker diffs
and ran the resulting Windows tests. Review found a minor loss-state ownership
discrepancy, corrected by clearing signature state on defeat; no blocker remained.

## Verification results (2026-10-25)

- Pinned Windows Release build: MSVC19.44.35229.0, toolset14.44.35207,
  Windows SDK10.0.26100.0, raylib5.5; configure/build exit0. New output directory
  only. Five focused suites passed before the full regression run.
- Final CTest Release: **21/21 passed, zero failed, 4.24 seconds**. This includes
  all17 accepted M3 suites and4 M4 suites: Afterimage, ranged, integration, scenario.
- Lead strict g++ C++20/O2/NDEBUG/-Wall/-Wextra/-Wpedantic/-Werror builds and runs
  passed for M4 integration/scenarios. Checks remain active in Release.
- Integration test compares coordination at30/60/120/144/240/1000 render FPS;
  shot schedules, warning schedules and first-commit order match. It verifies
  full108-tick warnings, >=42-tick ranged spacing and <=2 special commitments.
- Helper tests cover once-only signature acceptance, alive/ordinary-credit gates,
  completed curved dash paths, no fabricated chords, TTL/pause/resize, once-per-ID
  claims, independent silk gates, rounded damage, reservations/full capacity,
  physical projectile consumption during immunity, swept relative contact and
  final-lifetime-tick contact. Legacy suites retain exact1440-tick cleanup,
  rewardless removal, manual pickups, expiry/carryover, deterministic offers,
  window mapping, controls, audio budgets and Rootwarden coverage.
- Actual desktop smoke: launched the new Windows executable with the mixed
  Wave8 fixture, inspected title/HUD/Spitter warning geometry, observed logged
  warning -> lock -> launch, physical pressure/damage and defeat, then clean
  audio/window shutdown. Audio device initialized. This was an automated desktop
  smoke, not a human playthrough, listening acceptance or resize/DPI certification.

Scripted measurements use actual W8/W9 schedules/lifecycle, shared combat,
special-enemy brains, projectile pool, echo paths and manual pickup helpers.
Hunter/Brute movement is a direct-pursuit proxy, spawns are projected onto a fixed
1280x720 arena, and the player follows fixed waypoints, dashing on cooldown.
Carry-in ranks stay fixed; spending a scripted credit does not add a rank.
No Rootwarden fight or evolution offering is simulated. Each combination is run
twice and its entire summary must match; this is repeatability within a build,
not a cross-platform floating-point determinism claim.

| Wide circuit, signature owned | Carry-in ranks | End HP | Echo hits / HP damage | Kills /52 |
|---|---:|---:|---:|---:|
| Zero | 0 | 23 | 4 /3.00 | 15 |
| Offense | 4 | 23 | 5 /7.50 | 25 |
| Defense | 4 | 59 | 5 /3.75 | 22 |
| Mobility | 4 | 45 | 8 /6.00 | 16 |
| Mixed | 3 | 35 | 5 /4.90 | 23 |

All wide-circuit pairings, with and without Afterimage, completed both waves;
52-57 shots launched but none hit that continuous wide route. Echo contributed
roughly7-14% of dealt damage and0-1 additional kills under this policy. All tight
central-loop pairings eventually died, with2-3 projectile contacts and substantial
body damage. These intentionally different policies show avoidable ranged fire,
route-dependent survival and bounded signature contribution. They establish
neither population survival rates nor that evasion-only play is satisfying.
Wide routes left27-38 timeout despawns without Afterimage; low engagement is a
balance risk for owner testing, not grounds to change accepted cleanup rules.

Lead review caught a missing immunity-timer tick in the initial worker scenario.
It was corrected, earlier survival numbers were discarded, and all measurements
above come from the corrected Windows run. The production loop already ticked
immunity correctly. Final Wave9 credits remain pending in the corrected scenario.

## Acceptance and limitations

Use the new executable, not the preserved root/M1/M2/M3 builds.

1. Normal run: finish Rootwarden, test collecting and missing its core in separate
   runs. Check five-second safe collection, ordinary choices first, one honest
   Afterimage card, one acquisition, then Wave8. Confirm no accidental dash from
   its confirm key, duplicate reward or HP restoration.
2. Dash through/around pursuers: the offensive route appears after dash completes,
   fades quickly and hits each enemy once. Check hit/kill feedback feels useful.
3. Play Waves8/9 with a realistic low-offense and a mixed build. Check committed
   Spitter shots can be dodged, combined warnings remain readable, and dashing
   through a shot consumes it. Judge both active weaving and wide-circuit kiting.
4. Pause during warning/projectile/echo; resume without a jump. Resize at minimum
   size and the intended startup DPI; inspect signature card text/hit testing,
   warning geometry, HUD and dash indicator. Listen to new cues in a busy mix.
5. Finish Wave9: Nine-Wave Slice Complete, no unusable evolution/signature screen.
   Die/retry: normal run restarts Wave1 and signature ownership is gone.

No full nine-wave human playthrough, perceptual audio acceptance, dedicated GPU
stress benchmark, or new multi-monitor/DPI acceptance was performed here. Existing
startup-coordinate architecture and its accepted dynamic-scaling limitation remain.
New combat numbers remain provisional. Other signatures, enemies, bosses, further
chapters, upgrades and permanent boss restoration are deferred. M4 is an acceptance
candidate on its feature branch; integration into develop requires owner acceptance.
