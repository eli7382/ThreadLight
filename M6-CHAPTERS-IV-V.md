# M6: Chapters IV-V implementation contracts

Accepted M5 f210e2cb43dce0abb4f6274520bb0d356c4fe735 was fast-forwarded and
pushed to develop by owner authorization. M6 is isolated on codex/phase34-m6.
Main, running executables, previous build directories and unrelated work stay intact.

Scope: Waves22-35, Sower, Hollow Weaver28, Briar Heart35, Cocoon reward21,
Resonant Loom reward28, actual victory35. Earlier encounter data, controls,
ranked upgrades, manual pickups, exact1440-tick cleanup and ledger ownership stay.
Keep the staged one-card signature screens; the full six-signature offer pool,
remaining catalog and release acceptance are not silently declared complete.
No boss healing, checkpoints, new active control, Sower barriers or permanent walls.

Lead owns shared stats/damage, boss machines, main, CMake, integration and docs.
Workers have disjoint new hazard/Sower, Cocoon/Resonance or encounter-data ownership.
Use120Hz integer clocks, stable IDs, finite bounded pools and shared HP resolution.

Hazard contract: reusable disk/capsule/annulus-with-fixed-safe-gaps geometry.
One ordinary pool and one boss-owned pool use the same pure collision code.
Ordinary maximum6 patches,2 per Sower; boss maximum4;12 slots per pool including
harmless fades. Combined conservative occupied area <=18% of playable area.
An add reserves a complete warning before any damage. Denied casts wait; no
shortened warning or invisible activation. Annulus safe gaps are fixed while
active; collision sweeps both player motion and expanding radial intervals.
All shapes render from the same stored geometry used by collision.

Sower: introduced22; HP5/6,speed64/76,radius16,contact15/17,sparks2,score30.
Cast warning108ticks (aim soft30, locked78); interval540/480ticks; disk radius45/55,
active336/384ticks, damage18/20. No more than2 simultaneous special commitments
across K/P/S. Source death cancels warnings; existing patches become harmless
and fade30ticks. Timeout/collection removes everything immediately.
Placement rejects a protected112-unit escape corridor plus player-radius margin.
Hollow pattern commitment cancels/fades conflicting ordinary patches, consistently
preserving its displayed safe route. No collision barriers or player trapping.

Cocoon: an old nonadjacent silk crossing enclosing >=3500 area with >=240 path
travel grants one12-HP barrier for600ticks; cooldown960ticks. No stacking,
refreshing, healing, refunds or new immunity. Each crossing consumed. Shield
absorption occurs only after existing dash/immunity gates; a fully absorbed hit
does not create extra immunity. Partial HP damage retains the existing94-tick gate.
Helpers never own HP/rewards. Pause freezes clocks, transitions clear temporary
shield/proc state, resize preserves or safely invalidates pending geometry.

Resonant Loom: every fifth actual ordinary silk HP hit, excluding buds/procs,
emits .6x silk damage to the source and up to2 neighbors within180. Minimum240
ticks; cooldown hits do not bank charges. Boss damage is .3x base with one rounding.
Target limit/distance/stable-ID ordering and death rewards remain shared/once-only.

Hollow provisional285HP, phases65/30%,120-tick transitions,120-tick warnings,
360-tick floor patterns,240-tick recovery. Fold seams, opposed-gap ring and
reoriented future patterns stay open; no active gap rotates closed. Finite4H+2S.
Heart provisional360HP, phases75/50/25%,120-tick transitions; pulse, >=102-tick
lunge warning, >=114-tick fork warnings; final lane/lunge/pulse/fork chain then
360-tick recovery. Finite4H+2K+2P, at most3 live supports. No timeout/enrage.
Prototype +1 ordinary-silk exposure and50% signature boss resistance retained.

Internal checkpoints:1 shared hazards/Sower/data;2 new signatures/accounting;
3 spatial bosses/collision/fairness fixtures;4 independent review, Windows build,
focused/full regression, preservation checks and feature-branch handoff. No M7.


## Verification and review evidence

Windows Release: MSVC19.44.35229.0, VS2022 x64 toolset14.44.35207,
Windows SDK10.0.26100.0, C++20 and raylib5.5. New build directory:
`build-phase34-m6-msvc-x64`. The game and all30 test executables built.
Focused M6/hazard CTest:5/5 passed. Full Release CTest:30/30 passed,
0 failures,3.36seconds. These include the earlier25 regression suites.
Hazard checks358; boss checks784; M6 integration checks1958. New helper tests
also compiled with g++ O2/NDEBUG and strict warnings; checks remain active.

Independent Sol review found and lead corrected an empty-offer progression stall
before Heart, plus resized ordinary and boss fields overlapping Hollow's safe
corridor. Both have focused production-helper regressions. Annular collision
uses synchronized analytical event times, avoiding false hits on valid outward
movement and on player disks protruding into the empty inner ring. Geometric
boundary tests cover tangency, contracting/expanding rings and resize.
Cocoon rejects self-intersecting, retraced and repeated-winding polygons rather
than treating their algebraic area as a legitimate enclosed region. Main's
sampled history includes the real virtual tail to the previous player position.

Runtime metadata confirms Luna Max for hazard/Sower, boss/integration tests and
Cocoon/Resonance helpers; Luna High for isolated encounter schedules; Sol High
for independent integration review and the reviewed annulus collision repair.
Lead owns boss machines, shared combat, terminal ledger semantics, main, render
integration, CMake, documentation, Windows verification and Git integration.

### Balance observations, not human playtests

The deterministic boss helper uses real bounded movement/dash, silk geometry,
HP gates, hazard collision and Afterimage. It omits support bodies and the other
three signatures. It is deliberately not a normal campaign playthrough. A
blind orbit policy died in all6 fixtures. A second policy follows the already
committed visible gap/lunge geometry, without predicting future warning aims:

| Boss/build | Ranks | Result | Seconds | HP remaining |
|---|---:|---|---:|---:|
| Hollow/zero |0|Defeated|67.08|28/100|
| Hollow/offense |12|Defeated|35.55|124/124|
| Hollow/no-Razor mobility |16|Defeated|60.84|136/136|
| Heart/zero |0|Player died|19.67|0/100|
| Heart/offense |16|Defeated|53.55|32/136|
| Heart/mixed |22|Defeated|50.13|134/160|

This supports damage access without mandatory Razor, but does not establish
human fairness, monotonic difficulty, full support pressure or campaign pacing.
Hollow's generous safe lane may be too easy once understood; Heart's unranked
failure is a low-power boundary observation, not an automatic tuning target.
All HP, warning cadence and new enemy scaling remain provisional.

The strict maximum scheduled normal reward is1290 integer sparks, plus34 from
finite supports and40 from four optional boss cores:1364 total. Complete killing
and manual collection could issue29 credits with26 remainder. This is a ceiling,
not an expected run; timeout removals, expiry, canceled supports and missed
pickups reduce it. Credits earned in the final boss are retained unspent. There
is no final boss core, healing, terminal upgrade screen or automatic banking.

Existing synthetic helper stress (520 strands,37 enemies,1000 steps) measured
mean1111.89us, p951529us, max2603.6us on this run. This is CPU helper evidence,
not full M6 frame-time/GPU or maximum-population acceptance. New schedules peak
at40 total scheduled enemies; hazard pools, projectiles and signature targets
are bounded. Full late-game GPU/readability and memory soak remain manual gates.

## Owner acceptance and intentionally deferred work

1. Launch the M6 executable, verify Chapter I-III behavior and actual transition
   after Conductor: manual collection, pending ordinary credits, then one Cocoon
   card. The core is optional; death never grants the signature.
2. Use Sower fixture and then normal Waves22-27: warning locks before damage,
   disks expire/cancel, Skitter/Spitter/Sower commitments stay readable, cleanup
   removes threats without rewards and pickups still carry over manually.
3. Use Hollow fixture with offense and mobility: test all phases and real support
   pressure, gaps, exposed silk access, pause and resizing down to minimum size.
   Confirm safe lanes remain safe after resize, then collection/credits/Loom.
4. Close a large simple silk loop for Cocoon. Verify12HP temporary barrier,
  8-second cooldown, no stacking/healing/immunity. Repeated tiny loops should
   not grant a barrier. Five ordinary HP hits trigger bounded Loom damage;
   echoes and other signature effects must not recursively charge it.
5. Play Waves29-35 and Heart with low/mixed builds: fresh warnings between chain
   beats, finite supports, no final core or meaningless menus; Wave35 defeat
   wins, player defeat retains precedence, retry starts Wave1.
6. Check 30/60/120/Unlimited presentation, DPI/resize, keyboard/mouse menus,
   collection countdown, dash indicator and procedural audio by human play.

No full35-wave human run, listening acceptance or maximum-load GPU soak was
performed by the agent. Hidden silent warning/active render checks are scripted
startup/presentation evidence only. Full catalog/signature offer-pool expansion,
release polish and campaign-wide human balancing remain later acceptance work.
Main and accepted develop are not advanced by this M6 candidate. Stop after M6.

Final hidden Windows captures for both new bosses were exported and inspected at
1920x1050 framebuffer resolution. Warning and active state startup completed and
both windows closed normally. Capture review corrected excessive warning opacity;
render-only QA reads actual framebuffer dimensions, without changing normal DPI
mapping. Artifacts are in `build-phase34-m6-msvc-x64/qa-final-hollow` and
`qa-final-heart`. No input was sent to the owner's running game. Invalid fixture
and render-check-without-fixture arguments both returned exit2 before startup.
All six preserved root/M1-M5 executable SHA256 hashes matched preparation.
The final stronger-shrink Fold regression also passed (1958 integration checks).
