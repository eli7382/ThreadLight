# M5: Chapters II-III implementation contracts

Accepted base: 9154b5e5cac544824b0c7f245c2de1dc400dac87.
Feature branch: codex/phase34-m5. No develop/main integration is authorized.

Scope follows the Final Design Revision roadmap: extend the accepted nine-wave
slice through Wave21, with Needle Dancer14, Bulwark15, Thorn Conductor21.
Implement the deferred small Crossweave checkpoint as the usable Wave14 signature.
Rootwarden's single Afterimage reward and Waves1-9 remain unchanged. Wave21 ends
at Three-Chapter Prototype Complete, preserving pending credits and pickups;
no unusable final reward screen, permanent healing, Sower or ChaptersIV-V.

Ownership: lead owns main, combat, presentation, boss state machines, CMake,
integration and docs. Bounded workers own new crossweave/bulwark helpers/tests,
or chapter schedule data/tests only. No worker edits shared integration files.

Shared rules:120Hz; cleanup exactly1440ticks; collection480/600ticks; manual
contact pickups with2160combat-tick lifetime;20/30/40/48 thresholds; deferred
ordinary credits, death precedence and all accepted input/window/audio behavior.

Crossweave helper contract: pure geometry/travel/clock state, no HP/rewards.
Historical segments have stable monotonically increasing IDs and age ticks.
Cross player movement with nonadjacent older segment (72..420ticks), after160
travel;300tick cooldown; qualifying segment consumed even when waiting; one
pulse per tick radius110, up to6 nearest targets, ID tie-break. Normal damage1x
silk; boss50%, rounded once. No proc recursion, no old-trail reactivation.

Bulwark helper contract:288ticks guarded/192 exposed, always damageable.
Incoming player damage multiplied by3/5 or5/4 nearest-half-up in hundredths.
No HP/gate/reward ownership; omnidirectional, no reflection or allied protection.
ChapterIII HP700/radius23/speed52/contact2000/sparks3/score45.

Boss contracts: shared incoming-damage arbitration and once-only ledger; no
timeout, infinite supports, permanent barrier, hidden aim or mandatory upgrade.
Dancer provisional135HP/70%-35% phases,96tick warning,34tick committed strike,
speed<=600,168tick ordinary/240tick final recovery. Four Skitters finite; maximum
two active support bodies, queue releases across transitions.
Conductor provisional210HP/65%-30% phases;108tick committed warnings; six-shot
fans, sequential destructible rewardless tuning buds and reachable-gap volleys.
Finite4Hunters+2Spitters. No untelegraphed projectile activation/capacity overflow.
Later boss contacts/exposures and signature contribution must be measured with
realistic carry-in fixtures, not maximum rank assumptions. Accepted Rootwarden
uses its existing implementation and values unchanged.

Internal checkpoints:1 schedules/stats/Bulwark;2 Crossweave/reward continuation;
3 bosses/geometry/telegraphs and separate scenario fixtures;4 Windows integration,
independent review, full regression, reviewable feature commit/push.

## Encounter data and progression

| Wave | Spawn seconds | Budget | Main scheduling decision |
|---|---:|---|---|
|10 Slow Caravan|26|H8 B12 K2 P2|Brutes enter from one half; steady lighter pursuit gives room to plan cuts.|
|11 Burst Windows|30|H6 B2 K20 P2|Four separated five-Skitter packets; no increase to commitment capacity.|
|12 Return Route|28|H18 B4 K4 P4|Alternating pursuit streams with spaced side threats.|
|13 Commitment Test|32|H10 B6 K8 P8|Ranged opening, then flankers, then pursuit/durable entries.|
|14 Needle Dancer|Boss|Four finite Skitters|Prediction, perpendicular exits, lateral countersteps and chained warnings.|
|15 Opening Plates|28|H16 B4 K6 P4 U2|Two separated Bulwarks; their always-damageable guard cycle becomes visible.|
|16 Firing Relay|30|H8 B2 K6 P12 U2|Three four-Spitter relays change sectors; excess warnings queue.|
|17 Moving Anchors|30|H8 B9 K4 P2 U6|Slow durable groups, sparse ranged pressure.|
|18 Loose Weft|32|H6 B2 K18 P4 U2|Three separated Skitter packets after the durable encounter.|
|19 Gathering Tide|30|H21 B6 K6 P2 U2|Hunters enter early; later pressure thins for route/collection decisions.|
|20 Call and Response|34|H10 B6 K8 P6 U5|Three acts alternate origins and answer flank threats with pursuit.|
|21 Thorn Conductor|Boss|Four Hunters + two Spitters|Read multiple origins and optionally cut rewardless tuning buds.|

H=Hunter, B=Brute, K=Skitter, P=Spitter, U=Bulwark. Every new scheduled spawn
precedes the end by at least two seconds. Schedule storage64; actual maximum37.
Waves1-9 retain their accepted definitions, seeded angles and spawn times.

Chapter III applies only from Wave15: Hunter2.5HP/speed115/contact12;
Brute6/74/20; Skitter2.5/135 flank/15 (accepted burst260); Spitter4/78/13;
Bulwark7/52/20/radius23. Spark values1/2/1/2/3, scores10/35/15/30/45.
Spitter warning, projectile speed180, damage14 and lifetime360 remain unchanged.
Ordinary player ranks, movement315 maximum, dash multiplier3.7, immunity and
cooldowns retain accepted M4 values. No additional active control is introduced.

Rootwarden -> five-second manual collection -> ordinary credits -> Afterimage.
Dancer -> same safe collection/credits -> one honest Crossweave card -> Wave15.
Each signature has its own once-only reward identity; cores are optional for
signature acquisition. No unavailable alternatives or automatic spark banking.
Conductor -> safe collection -> Three-Chapter Prototype Complete. Ordinary
credits and pickup value remain recorded without an unusable final upgrade.
Spark Magnet is excluded from the last pre-Conductor choice, when no future
ordinary spending boundary remains in this prototype. Normal permadeath starts1.

Normal spark ceilings are128/222/292 by chapter. Add finite supports4/4/8 and
three manually collected10-spark cores:688 total, at most15 ordinary credits and
22 remainder. This arithmetic ceiling assumes every kill/pickup, not realistic
player income. No thresholds/drop values were changed to force an upgrade count.
Full spatial campaign earnings and healing expenditure still need human logs.

## Boss behavior and damage access

`chapter_boss.h` contains a small shared record and two explicit state machines;
Rootwarden delegates to the accepted `rootwarden.h` implementation. A seeded
facade-equivalence regression compares the delegated state/events with the legacy
implementation. No generalized boss scripting/entity framework is added.

Needle Dancer:135HP; phase changes below70%/35%,108-tick boss transitions.
One/two/three individually warned strikes per chain. Warnings96ticks; default
strike34ticks and <=170 travel, <=600speed. Between strikes, a48-tick lateral
counterstep moves48 units without contact damage, then grants24 warning ticks of
body-contact escape grace. Recovery168ticks, or240 after the final three-strike
chain. Vulnerable radius52, dangerous core22; contact20, strike22. Resize maps the
committed path and can extend its remaining duration to preserve the600 speed
cap; it never retargets or shortens the warning. Four support Skitters queue in
pairs at phase changes, with at most two support bodies alive.

Thorn Conductor:210HP; phases below65%/30%,108-tick boss transitions. Vulnerable
radius66/core28/contact22.108-tick locked scores alternate six-shot fans, three
sequential bud-origin shots42ticks apart, and seven of twelve ring rays, leaving
a large omitted sector centered on the player's locked bearing. Final phase can
combine a fan and an84-tick delayed bud shot. Each live bud has2HP, no contact
collision/rewards, and can be hit by ordinary silk or signatures. Cutting a bud
cancels its reserved, unfired shot; existing projectiles persist until normal
impact/expiry or phase/death cleanup. Fans/rings deal20, bud shots24; speed180,
radius5, lifetime360. Recovery216ticks or240 in Phase III. Supports are finite
four Hunters followed by two Spitters across the first recovery/phase changes.

Both experimental bosses currently use +1 ordinary-silk damage during recovery.
This is a per-prototype experiment, not a universal rule for unimplemented bosses.
Offensive signatures receive50% boss resistance, with no exposure bonus.

New boss/support admission shares explicit helper policy with production/tests.
Dancer reserves one special-attack slot during chains; Conductor excludes new
support windups during its own score. Existing support warnings finish naturally.
Before starting a score, Conductor waits for support shots to clear; a final
Stalk priority window prevents newly queued supports from starving the boss.
No warning is shortened to catch up. Existing supports remain dangerous during boss phase transitions; these are not player-immunity windows. Pause and selection freeze all clocks.

## Crossweave, Bulwark and boundaries

Crossweave uses the retained visible silk history, not only currently damaging
silk. Crossing a nonadjacent segment aged.6-3.5s can pulse after160 actual travel
and a2.5s cooldown. Crossed segments are consumed even during cooldown. Strict
historical-endpoint rejection prevents knot farming; pending side tracking
preserves crossings split at an exact120Hz movement endpoint while rejecting
touch-and-reverse or small line jitter. Claims and pending touches are bounded640
against a520-point trail. Pending geometry remaps during resize; clamping cannot
manufacture a crossing. Pause does not advance travel, cooldown or pending state.

Pulse radius110; choose at most six intersecting targets by center distance then
stable ID, including optional buds. Shared target selection is tested and used
by production. Each pulse damages each selected target once; no signature procs
another. A faint expanding ring and restrained existing cue mark the pulse.

Bulwark is always damageable:288 guarded ticks at60%,192 exposed at125%.
Source and guard multipliers combine before one integer-hundredth rounding:
base100 ordinary gives60/125; Afterimage gives45/94. Shield colors/plates change
to warm exposed cores. There is no reflection, allied shield or build requirement.

`combat.h` retains HP and incoming-hit ownership; dead targets and canceled boss
threats cannot submit residual contact damage. `RewardLedger` alone awards kills,
score, pickups and credits. Buds never register a reward. `ranged_combat.h` adds
an optional validated ShotSpec so boss and ordinary projectiles use the same swept
physical-impact code, consuming shots even when the player is protected.
Ordinary and boss projectile pools each hold64 slots (128 reserved/flying total
hard bound); practical content counts are much lower. Existing520 silk, particle,
death-effect, mixer and interpolation budgets remain intact.

## Verification and measured limitations

Windows Release build uses MSVC19.44.35229.0/toolset14.44.35207,
Windows SDK10.0.26100.0 and raylib5.5 in a new M5 build directory.
Full regression:25/25 passed, including the21 accepted M4 suites and4 new M5
suites. New checks remain active under NDEBUG; the lead corrected a worker's
initial assert-only chapter test before recording final Release results.

Checks include exact new compositions/deadlines/chapter boundaries, Rootwarden
equivalence, Dancer strike/phase/counterstep/resize boundaries, Conductor score
timing/gaps/bud cancellation, finite support types, reservations/custom projectile
damage and immunity, independent signature rewards, ledger ceilings and final
retention. Combined boss/support simulations run6000ticks at30/144 render FPS and
compare attack schedules while checking capacity and lack of starvation.
Crossweave regressions include exact tick endpoints, touch reversals, jitter,
pause, pending-point resize, clamp-only false triggers, consumption, expiry,
overflow and nearest-six ordering. Legacy1440-tick cleanup/manual collection,
offer determinism, controls, settings, audio and presentation tests still pass.

Scripted boss scenarios are **helper damage-access evidence**, not human balance
or full-encounter survival. They execute derived stats, limited movement, real
silk/echo geometry, boss clocks/projectiles and player immunity. They omit support
bodies and Crossweave, count support release events only, and assume full starting
health. The state-aware policy approaches the visible exposed skirt during
recovery; the comparison policy circles a fixed225-radius world-space orbit.
No HP/damage values were changed to make these bots win.

| State-aware fixture | Ranks | Razor | Boss defeat seconds | Player HP at end |
|---|---:|---:|---:|---:|
|Dancer defense|6|0|34.41|136/136|
|Dancer mobility|6|0|36.43|100/100|
|Dancer hybrid|6|II|24.33|112/112|
|Conductor defense|9|0|46.84|88/148|
|Conductor mobility|9|0|51.05|40/100|
|Conductor hybrid|9|III|32.68|112/112|

Zero-rank approach fixtures also defeated the helpers at38.23/57.83seconds,
but had Afterimage. The fixed outer orbit was much worse: low-rank fixtures
reached240seconds with bosses alive; one nine-rank Conductor fixture died at
226.68seconds. These expose the risk of long, unproductive kiting and the
importance of readable recovery access. Ideal state-aware Dancer kills can be
fast; support pressure and actual human reaction remain acceptance gates.

Actual Windows desktop smoke inspected both new boss fixtures, their health/name
feedback, Dancer's stored lane and Conductor's fan/bud warning geometry, recorded
attacks/damage/defeat, and clean audio/window shutdown. It did not complete a
human21-wave run or perceptually certify audio, resize, DPI or difficulty.

Reproduce helper-only stress with
`build-phase34-m5-msvc-x64/Release/ThreadlightM5IntegrationTests.exe --perf`.
1000 synthetic updates with520 silk segments/37 targets measured mean771.6us,
p951257.4us, max1980.4us on this host (MSVC Release). This checks CPU helper
headroom, not full-game GPU/frame pacing or an across-hardware guarantee.

Applied settings were checked in runtime metadata: lead GPT-6.1 Sol High;
Luna Max owned Crossweave/Bulwark and bounded boss tests; Luna High owned wave
data/active Release tests; independent Sol High reviewed boss/shared integration.
Lead reviewed all diffs, corrected and extended scenario coverage, built Windows,
and verified reported results. Review found/fixed strike retiming, final motion
ticks, exact crossing endpoints and pending-touch resize cases. No review blocker
remained after the final read-only pass.

## Owner acceptance

Launch `build-phase34-m5-msvc-x64/Release/Threadlight.exe` normally, or use
`--m5-dancer-fixture mixed` / `--m5-conductor-fixture defense` with
`--m5-telemetry --m5-seed 7382`. Names zero/offense/defense/mobility/mixed work;
mutually exclusive fixtures are explicitly labeled M5/DEV FIXTURE. Dancer presets
have0/6 ranks and Afterimage; Conductor presets0/10 and both signatures. Fixture
ranks are synthetic carry-in, not fabricated ledger credits or checkpoints.

1. Accept Dancer separately: fresh warnings for every chain strike, harmless
   countersteps, safe outer damage access, phase changes and at most two supports.
2. After Dancer: optional manual10-spark core, five-second safe collection,
   ordinary choices first, Crossweave once, no healing, no confirm-key dash, Wave15.
3. Cross old silk deliberately: visible bounded pulse and useful damage; reversing
   on a knot or pausing/resizing must not farm pulses. Inspect Bulwark guard/expose
   readability and afterimage interaction with a low-offense build.
4. Accept Conductor separately: readable fans, gap volleys and ordered bud lanes;
   cut buds with silk, check no rewards, dodge at base mobility. Judge support
   pressure during recovery and whether the finale feels harder than Dancer.
5. Play the full three chapters: new encounter pacing, manual pickup income,
   recovery spending, final credit retention, permadeath, pause/resume, resizing,
   minimum window/startup DPI and audio under busy combat.

M5 ends at21. No Sower, later chapters/bosses, Cocoon or other signature pool,
catalog expansion, release changes or boss healing is included. Full-campaign
economy, late-game difficulty, full-encounter human acceptance and dedicated
GPU/memory stress remain unvalidated. Accepted startup window mapping and its
existing dynamic-DPI limitation are retained. Do not treat this branch as a
release: owner acceptance is required before develop integration; main is unchanged.
