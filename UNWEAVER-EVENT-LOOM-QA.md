# Event Loom, timed phases and active late-wave AI

Comparison baseline: `77ba368`. Native C++20/raylib, private Windows x64 Debug QA build. Earlier QA packages and trusted Release binaries are preserved.

## Gameplay changes

- **Arena-length shots:** Unweaver shrapnel and ordinary Spitter shots now end at arena boundaries instead of fixed lifetimes. The old ranges were approximately 525 px for ordinary boss shards and 540 px for Spitters. Player hits, Needlecatch and safe phase/wave cleanup still remove shots. Other legacy boss projectile specifications are unchanged. Warnings extend to the arena boundary too.
- **One-minute phase clock:** surviving Unweaver enters Phase II at 7,200 simulation ticks, III at 14,400 and IV at 21,600: 1:00, 2:00 and 3:00. The clock begins with active Wave 35 combat, excludes pause/countdown and includes its safe transitions. HP and experience tokens no longer gate phase transitions. As requested, zero HP wins immediately in any phase; no minimum three-minute wait or banked lethal damage remains. Existing simultaneous player-loss precedence is preserved.
- **Radial variation:** Halo alternates fast and slow petals, while Eclipse's later rotating rings accelerate and overtake earlier rings. Rhythm retains asynchronous angular cohorts, Predicted Escape uses committed prediction-based orientations, and Convergence retains interleaved rotated rings. All angular slots remain represented, without the old broad empty sectors. Individual shots keep travelling after the cast ends, letting the boss reposition while distant shards remain dangerous.
- **The Event Loom:** four eye portals unfold across the arena. Two staggered cohorts of eight total spindle pulses follow locked quadratic paths through a braided, animated lattice. Only the bright moving heads cause damage; the ghosted ribbons and geometric framework are warnings. Pulse heads use the same swept collision and Needlecatch pipeline as other projectiles. Boss damageability remains intact. This introduces curved arena-spanning projectile movement rather than another straight radial or ground-field attack.
- **Reactive combinations:** uncommitted movement can switch from hunting to baiting when chased, or flank after a reversal. Dash prediction accounts for the dash's remaining duration rather than extrapolating dash speed indefinitely. A hit or observed dash can prompt one bounded follow-up hunt/interception, with a separate warning for a charge. Earned formation-break openings take priority over counters.

Direct phase fixtures retain preset remaining HP and begin their clock at that phase's minute. Ordinary fights never refill or change HP at a phase transition. The HUD separates remaining health from the phase clock and no longer marks health thresholds as phase gates.

## Waves 29–34

Live movement intent is reconsidered every 1.5 simulated seconds and on deployment reviews. Pursuit, interception, encirclement and protection produce different movement destinations based on current observed play. Hunters use predicted cutoffs; uncommitted Skitters can take flanking/cutoff routes; Brutes retain physical Caller protection. Committed Skitter bursts and projectile warnings are not retargeted.

Future deployments can be reviewed six times per wave: normally every five seconds, or after 2.5 seconds when behavior changes substantially. Lane adjustments are bounded to 0.8 radians and timing to 48 ticks, with a 120-tick commitment horizon and a fixed 125% weighted burst ceiling. Enemy counts, composition totals, rewards, scores, wave duration and final deployment deadline remain unchanged. No active enemy is teleported, and movement speed/damage/HP have not been increased to simulate intelligence.

## Safety and implementation

`ranged_combat.h` adds explicit boundary travel and quadratic paths to the existing fixed 64-slot pool. Curve velocity describes the exact next fixed-step sweep so Needlecatch and player collision agree. Resize safely cancels curved paths. No new dependency, runtime service or unbounded collection is introduced.

`unweaver.h` caches per-ray boundary flight times, validates longer trajectories and snapshots its own lingering straight shots as well as Spitter shots. Charges can combine with existing straight shots only after the same swept response check. Existing curved Loom pulses finish before a new major commitment. Support positions/commitments remain controlled during admitted major attacks and charges; cleanup cancels all hostile ownership safely.

Runtime response checks remain conservative bounded policies, not a proof of every possible human encounter. In particular, future uncommitted pursuit is not a global guarantee about the player's later choices. Human readability, difficulty and fun still require owner playtesting.

## Verification

- All **44 Debug CTest suites passed**. Exact 59.99/60, 120 and 180-second boundaries, pause-style non-advancement, no-damage progression, early lethal defeat in every phase, stale epochs and terminal idempotency are covered. The combat test executes the real boss clock through 21,600 ticks and checks safe transitions and final/early victory accounting.
- Projectile tests prove travel beyond the old lifetime to a distant boundary, final-step collision, ordinary legacy expiry, exact curved positions/next velocity, interception and resize cancellation. Event Loom tests cover center/corner positions with ready/exhausted dash, admitted response replay, locked geometry, actual curved Needlecatch, phase cancellation, fixed capacity and earned-opening priority (14,393 checks).
- Native Event Loom warning, active and composed frames were exported and inspected. All eight spindles and the braided routes are present in the composed scene. This is actual rendering evidence, not manual gameplay acceptance.
- A native Wave 29 runner-prior scenario performed six future-deployment reviews, changing 34, 29, 26, 16, 12 and 7 future entries, and switched live intent between interception and encirclement. The automated player died at tick 3,134; this is not a complete wave/campaign playthrough.
- Fifteen-second native real-time Debug samples on Intel Iris Xe, after two-second warmup: Event Loom simulation p99 **0.251 ms/rendered frame**, draw submission p99 **1.444 ms**; radial sample simulation p99 **0.200 ms**, maximum **4.207 ms**, draw submission p99 **1.807 ms**. These samples had zero or one fixed step per rendered frame. Peak sampled private memory was **58.6 MB** and peak sampled projectiles **53**. Startup discarded time occurred (about 960 ms and 278 ms respectively), with none after warmup. These are short CPU submission measurements, not GPU timings, leak proofs or older-hardware certification.
- The automated player stayed alive through both 1,800-tick boss samples. This does not prove a complete revised native victory or human balance. Existing campaign, earlier boss, signature, reward and cosmetic regression suites pass; no manual full campaign or legitimate cosmetic unlock was performed.

Local evidence is under `portable-release/event-loom-qa`, with the test/build logs in `portable-release/event-loom-*.txt` and `event-loom-build.log`. Developer runs suppress profile writes and genuine campaign records/unlocks. Seventeen packaged launcher checks passed across all selectable waves, all four boss phases, builds/seeds, attack overrides and optional frame logging; the normal profile hash stayed unchanged.

## Owner controls

Use **Start-Unweaver.cmd** for normal varied combat, **Test-Event-Loom.cmd** to force the new attack, **Select-Scenario.cmd** for waves/builds/phases/seeds, and **Start-Wave-29.cmd** for continuous late-campaign play. F5 restarts the current fixture. Each launcher saves arguments and diagnostics under `Logs`; add `-Performance` to QA.ps1 for a frame CSV.

Examples: `QA.ps1 -Wave 35 -Phase 2 -Mechanic 5 -Build mixed`, or `QA.ps1 -Wave 35 -Attack 1 -Build mobility`. Forced attack fixtures intentionally restrict repertoire; use the ordinary launcher to assess adaptation and combinations.
