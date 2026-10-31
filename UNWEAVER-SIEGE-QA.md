# Unweaver: final encounter guide and QA record

This is the authoritative Unweaver gameplay, implementation and QA guide for this branch. Earlier Event Loom and siege reports recorded intermediate snapshots; their distinct evidence is retained below as historical evidence and does not override the final behavior described here.

## Encounter timing

The Unweaver fight clock advances in active fixed simulation ticks on Wave 35. Pause and the pre-fight countdown do not advance it. Phase transitions include their safe transition ticks. A surviving boss enters Phase II at 1:00 (7,200 ticks), Phase III at 2:00 (14,400 ticks), and Phase IV at 3:00 (21,600 ticks). HP and experience-token gates do not delay these transitions. Zero boss HP wins immediately in any phase; simultaneous player-loss precedence is unchanged.

**Kill Mode is intentionally hidden until 5:00.** At 36,000 active ticks in Phase IV, deterministic Overdrive enables shorter readable warnings, short recovery, more aggressive contact objectives, up to three follow-ups, and bounded support pressure (six active supports and twelve formation events). The HUD does not announce Kill Mode. Ten-second warned pressure expansions eventually cover the arena at about 40 seconds after activation. This is a deliberate sudden-death mechanism; it does not establish a universal survival window or guarantee that tactical adaptation alone will cause a kill.

Overdrive's action utility is limited to HP damage from a tagged Unweaver projectile, attributed to the action captured when that projectile was committed. The tag follows a lingering shot if the boss has since selected another action. When several hits are gathered, the existing greatest-damage/lowest-source-ID selection remains authoritative, and only that winning hit's tag is considered. Support, ground, contact, barrier, echo, strand and Kill Mode pressure damage carry no action tag and cannot train an unrelated current action. A kill bonus is attached only when a tagged damaging shot is the selected hit that kills the player. Reward/attempt scales and combat damage are unchanged. This is bounded in-run tactical feedback, not persistent learning.

## Combat and presentation

- The Event Loom uses four reactive folds. Each fold emits two crossing curved spindle pulses (eight total). Later pairs read the updated player movement and prediction; every pair's displayed paths lock for that pair's warning. Only the bright moving heads damage. Pulses continue from their endpoints along the tangent to arena bounds and use swept collision and Needlecatch.
- Fracture barriers use three split partitions: cyan solids block movement and hostile shots; pink spiked sections are traversable contact hazards. Walls warn for one second, last six seconds and have passage gaps. Warnings do not track the player. Friendly silk remains intact; movement can still break a route, and barriers do not make the boss invulnerable.
- Supports move during committed attacks, steer around the boss footprint and detour around solid wall endpoints within ordinary movement and arena limits. Existing support windups and aiming remain. Normal casts permit one support offensive commitment; Kill Mode permits two.
- Boss body artwork, silk damage radius and contact radius are scaled to 88% (66.88 px vulnerable radius, 28.16 px contact radius); warnings, projectiles and barriers retain world scale. No generic HP, contact damage, or movement-speed increase was added.
- Phase IV formations can remain mobile during warnings and active attacks. Existing encounter cleanup, legitimate reward flow, upgrades and early-victory behavior remain in the normal campaign.

## Verification status

This section is updated after the final candidate's verification. Never interpret prior intermediate evidence below as a final candidate pass.

### Historical intermediate evidence

- **Event Loom snapshot (baseline `77ba368`):** 44/44 Debug CTest suites passed. Exact 1/2/3-minute boundaries, pause/non-advancement, no-damage progression, early victory, projectile boundary travel and curved paths were tested. Native warning/active/composed frames were inspected. Two automated 1,800-tick boss samples survived; a Wave 29 runner-prior scenario died at tick 3,134. No full manual campaign or complete revised native victory was established.
- **Siege snapshot (baseline `16c0ad9`):** 46/46 MSVC Debug CTest suites passed. Barrier, pressure, utility and cleanup regressions passed. A native defense run starting at 5:00 lost after 1,865 active ticks (15.5 seconds); pressure-only instrumentation used a stationary 16-rank defense build with a 50 HP test bonus. These are controlled automated scenarios, not human survival guarantees. Seventeen packaged launcher startup/stepping checks passed; this was not a full campaign playthrough.
- Earlier performance samples were short Debug CPU-submission samples on Intel Iris Xe, not GPU timings or older-hardware certification. Earlier framebuffer inspection establishes rendered output, not human readability, fun or manual play acceptance.

### Final candidate evidence

- Normal QA-OFF Windows x64 Debug and Release builds both succeeded with VS 2022/MSVC 19.38 and pinned raylib 5.5, in fresh isolated build directories. All 46 CTest suites passed in each configuration; this includes the delayed-projectile provenance and incoming-hit winner regressions.
- `unweaver_combat_test::fourSequentialPhasesAndLethalOverflow` runs the encounter through 21,600 fixed ticks, checks the exact 1:00/2:00/3:00 boundaries and immediate, idempotent early/final lethal defeat. Campaign foundation scenario tests exercise deterministic normal lifecycle progression; they are scripted simulation, not a human campaign.
- `settings_test` checks the Violet Ascendant grant gate: victory on completed wave 35 in a normal non-tutorial, non-fixture run grants once; losses, wave 34, tutorial and fixtures do not. It also verifies palette selection persists. The test isolates settings policy; it does not simulate a complete real-time Wave 35 win or write the owner's profile.
- The native completion path at `main.cpp` only considers grants after a normal `Won`/`Lost` transition with `!tutorial`, `bossFixture < 0` and `waveFixture < 0`; it passes `wave+1` to `grantVioletAscendant`. Thus QA fixture victories cannot award it. The separate QA-OFF build compiles this normal game path; no real owner profile was changed.
- The candidate changes no player or boss HP, attack damage, movement speed, warning/recovery timings outside the already-authored five-minute Kill Mode behavior, or reward scales. No manual full campaign or human combat acceptance is claimed.
- Fetched `origin/develop` at `4dead78a30b88f7536b385da82c572d4132db8e9`; it remains an ancestor of the feature candidate. Merge is held for final review. Nothing was pushed.

## Owner QA controls

From `tools/developer-qa`, use `Start-Unweaver.cmd` for normal varied combat; `Test-Event-Loom.cmd` or `Test-Barriers.cmd` for forced mechanics; `Test-Kill-Mode.cmd` for a direct Phase IV start at the hidden 5:00 threshold; and `Start-Wave-29.cmd` for continuous late-campaign play. `Select-Scenario.cmd` selects wave/build/phase/seed. F5 restarts the fixture; P pauses. Developer scenarios suppress profile writes and genuine campaign unlocks/rewards. The ignored package launcher at `portable-release/Unweaver-Siege-QA-20261031/Play-Final-Boss.exe` starts the preserved `ThreadLight-Unweaver-DeveloperQA.exe` Debug build; neither is the trusted Release game.

Examples: `./QA.ps1 -Wave 35 -FightSeconds 300 -Build defense -Seed 9`, `./QA.ps1 -Wave 35 -Phase 3 -Mechanic 6 -Build mixed`, or `./QA.ps1 -Wave 32 -Build mobility -Seed 42`. Add `-Performance` for a frame CSV. Forced mechanics and direct phase starts are fixtures, not ordinary campaign evidence. `portable-release/Unweaver-Siege-QA-20261031/Play-Final-Boss.exe` is an ignored QA wrapper for the preserved existing release executable; do not treat it as a tracked artifact.
