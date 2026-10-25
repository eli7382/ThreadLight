# M3 Chapter I implementation contracts

Accepted base: `8367abe7b137ca9cec570ff4411bb9acbe4e5e83`.
Feature branch: `codex/phase34-m3`. Scope: Chapter I, Waves 1-7 only.

The lead owns chapter data/scheduling, Rootwarden, shared combat changes,
integration, telemetry, CMake, documentation and acceptance decisions.
Luna High owns the isolated new chapter HUD helper/tests and then audio definitions/tests.
Luna Max owns new rootwarden_test.cpp and chapter_test.cpp after contracts freeze,
then rootwarden_scenario_test.cpp as a separate bounded task.
Workers never edit main, shared Git refs, or another worker's files.

- `chapter_one.h`: deterministic finite spawn packets, seven encounter definitions.
- `rootwarden.h`: pure 120 Hz state machine, locked attack geometry, boss health,
  exposure damage, finite support-event requests, and resize remapping. No reward ledger.
- `combat.h`: common incoming-hit gate and swept collision; no boss reward ownership.
- `reward_ledger.h`: unchanged sole owner of kills, core/pickup value and ordinary credits.
- `chapter_hud.h`: pure logical geometry and authoritative dash view, no simulation mutation.
- `main.cpp`: entity ownership, packet consumption, boss/support/core integration, rendering.

Prototype completion freezes at PendingRewards after safe boss collection. Pending
ordinary credits and uncollected pickups remain recorded; no unusable choice,
forged credit resolution, signature, automatic banking, or boss healing occurs.
M4 will insert ordinary/signature selection before advancing to Chapter II.

## Chapter data and encounter identity

| Wave | Spawn seconds | H / B / K | Schedule |
| --- | --- | --- | --- |
| 1 First Thread | 20 | 12 / 0 / 0 | Alternating E/W, one every1.5s after0.5s |
| 2 Heavy Ends | 22 | 14 / 2 / 0 | Two11s streams, each Brute first then seven Hunters |
| 3 Four Corners | 24 | 12 / 4 / 0 | Four corner packets6s apart; Brute plus three staggered Hunters |
| 4 Side Stitch | 24 | 12 / 3 / 5 | Alternating H pursuit; isolated N/S Skitters4s apart, three Brutes |
| 5 Crossgrain | 26 | 12 / 4 / 6 | Opposing H packets at0/8/16s, separate Brute/Skitter entries |
| 6 Three Beats | 28 | 12 / 5 / 7 | Early H tide, middle K pressure, late durable B entries |
| 7 Rootwarden | No timeout | At most4 support H total | Two event-triggered pairs, never replenished |

Packet directions use small seeded angle jitter; safe-spawn correction takes
precedence if the player occupies an entry. Budgets never depend on RNG or player
performance. All normal entries precede their spawning deadlines; there are110
ordinary enemies worth128 sparks. Costs20+30+40=90 permit at most3 pre-boss ordinary
credits; a fourth needs138 cumulative sparks. No hidden HP/wave multipliers.
Skitter commitments remain capped at two. Existing Hunter/Brute/Skitter stats and
M2 rank values are unchanged. The shared legacy wave-count/duration accessors now
read Chapter I data rather than retaining obsolete five-wave values.

## Rootwarden experiment

Health70; danger core radius26, vulnerable outer radius54; contact/root damage18.
The visible outer roots are damageable outside the smaller dangerous center. The
boss is vulnerable to normal silk throughout; exposed recovery adds1 damage using
the same33-tick target hit gate. No per-segment stacking, signatures, cooldown
refunds, armor, automatic healing, permanent barriers, or boss timeout.

- Stalk:90 ticks, speed85 toward the player until82 units away.
- Warning:108 ticks (0.9s), geometry locked when warning begins.
- Root Reach: two branches at +/-0.60rad in Phase I, three at -0.75/0/+0.75rad in
  Phase II. Each extends up to180 units from a root base26 from the center;
  half-width20. Clip its ray to arena bounds before warning. Active growth42 ticks.
- Gathering Step: up to140 units toward/past a locked target, clipped before warning;
  movement108 ticks. No homing, retargeting, or extra charge.
- Recovery:192 ticks (1.6s), visible warm opened bark and +1 ordinary-silk damage.
- Phase II starts strictly below55% HP (below38.5); transition108 ticks, cancels
  pending geometry and disables body damage during the transition. Damage remains
  possible, subject to the existing hit gate. No forced minimum cycle count.
- Two Hunters are released after the first completed recovery; two on Phase II.
  Requests are bounded across the entire encounter, even if events occur out of order.

All damage uses the shared IncomingHit/player-immunity/dash owner. A lethal silk
hit cancels the boss before its body/hazards contribute that tick, consistent with
M2 enemy death ordering. The caller still gives player defeat precedence before
opening rewards or declaring prototype completion. Canceled attacks have no
collision path. Surviving support removal does not emit deaths, drops or score.

The boss is registered once in the reward ledger:10-spark core value and a
provisional250 kill score; pickup contact uses existing5-points-per-spark scoring.
Only defeat creates the core. It is a distinct visible pickup, subject to manual
contact, attraction and the existing combat-time lifetime. Boss collection is at
most600 ticks, may finish early, and never banks the core. Other live pickups remain.
No20-HP restoration is implemented.

At the final PendingRewards boundary, the prototype enters its complete screen
without resolving/forfeiting pending credits or deleting pickups. It does not claim
campaign victory. A later chapter can insert ordinary/signature choices at this
same safe boundary; there is no separate reward implementation for the prototype.
A pending credit before Rootwarden may offer useful Restitch in the first queued
choice if35+ HP is missing. It neither creates a free credit nor heals automatically.

## Presentation and controls

Rootwarden has a procedural six-root/plate silhouette, a visibly dangerous center,
outer vulnerability ring, two attack families, recovery/transition colors, and
new restrained procedural warning/phase sounds. The existing mixer, eight-voice
budget, volume settings, and legacy sounds are unchanged.

Warning and active collision read the same stored capsule endpoints. Root Reach
now reaches its full committed endpoint on the final active frame (corrected during
independent review). Resize remaps locked geometry without updating its target or
advancing phase clocks. Existing interpolation is preserved. Boss name/bar/phase
fit within the existing HUD, avoiding playfield obstruction. Normal collection has
a prominent footer countdown; combat-only expiry warning stops during collection.

The80-unit dash track sits beside SPACE DASH. Fill uses the cooldown captured on
activation; choosing Rhythm does not reset that cooldown or change its denominator.
Readiness uses simulation state, not interpolated fill. A stationary player sees
READY / MOVE. Pause/evolution/settings render frozen subdued values. No new controls.

## Reproduction and verification

Pinned toolchain: MSVC19.44.35229.0, toolset14.44.35207, Windows SDK10.0.26100.0,
raylib5.5, x64 Release C++20. Existing executables/build directories were preserved.

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake -S . -B build-phase34-m3-msvc-x64 -G 'Visual Studio 17 2022' -A x64 -T 'version=14.44.35207' '-DCMAKE_SYSTEM_VERSION=10.0.26100.0' '-DCMAKE_PREFIX_PATH=C:/AI-Game-Benchmark/shared/raylib-5.5'
& $cmake --build build-phase34-m3-msvc-x64 --config Release --parallel 4
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir build-phase34-m3-msvc-x64 -C Release -R 'Chapter|Rootwarden' --output-on-failure
& $ctest --test-dir build-phase34-m3-msvc-x64 -C Release --output-on-failure
.\build-phase34-m3-msvc-x64\Release\ThreadlightRootwardenScenarioTests.exe
```

Windows build succeeded. Focused new suites:4/4 passed. Full regression:17/17 passed,
zero failures. Checks remain active under NDEBUG. The lead also independently
compiled/ran the chapter, boss, HUD and scenario helpers using strict g++ C++20
warnings and NDEBUG. Tests cover exact data/deadlines, phase durations, strict HP
threshold, exposure/hit gates, committed geometry, final active endpoint, finite
supports, death/cancel, resize clock preservation, core accounting and manual
collection. Rootwarden schedules match at30/60/120/144/240/500 render FPS; existing
fixed-step/presentation suites cover their broader schedules. Baseline M1/M2
invariants remain in the full suite. The SDK registry restriction required the
approved desktop/toolchain execution path, not a source workaround.

Telemetry flags: --m3-telemetry and --m3-seed, retaining M1/M2 aliases. Boss records
include stage, phase, HP, combat ticks, successful hits, exposed hits, contact ticks,
cycles, damage dealt, total boss-encounter player damage and support requests.
Damage events retain source IDs. Collection records include ticks, early continuation,
created/collected/expired/live spark value, bank and pending credits. Exposure share
is exposed_hits/hits; contact duty and actual successful-hit rate can be calculated
separately. No per-frame log flood or performance-dependent rewards.

### Scripted geometric scenarios: observations, not human results

The scenarios use production boss, collision, stats, health, pickup and ledger
helpers, with explicit idealized movement policies and a small integration model.
They simulate real trail geometry, gated hits, Hunters, incoming damage and legal
dash protection; they do not inject boss hits or assume a DPS/contact percentage.
They are not the graphical app's complete input/render loop or human skill models.
Hunter/trail details were aligned to main during lead review.

All builds start at full derived HP with0-3 ranks. Two approach orientations were
run at1280x800 logical size; five builds also ran at960x640. Arena labels in the
scenario output exclude the existing HUD/footer (1280x672 and960x512).

| Build | Ranks | Approach kill seconds, normal arena | Remaining HP |
| --- | --- | --- | --- |
| Zero | None | 17.21 | 90/100 |
| Offensive | Razor III | 13.06-13.07 | 80-100/100 |
| Defensive | Heartwood III | 17.21 | 126/136 |
| Mobility | Quick II + Rhythm I | 17.18 | 80-100/100 |
| Mixed | Razor I + Living I + Heartwood I | 14.48 | 102/112 |

Five minimum-arena approach runs also won, in13.07-17.21s. Two more cautious zero-rank
routes won in19.96-22.03s. These17 scripted runs spawned four supports and created/
collected one core each. Most incoming damage was from supporting Hunters. The
separate reward fixture confirmed four rewardless support cancellations, no boss
timeout across180s, remote core retention through600 collection ticks, and an
unresolved credit preserved without automatic pickup banking.

Interpretation: access exists without Razor, but these state-aware routes are fast
and may expose an easy fight for practiced players. They are not measured typical
player durations or evidence of a target win rate. Keep70HP/+1 for owner testing;
do not inflate HP or impose damage caps to force an assumed duration. Human testing
must assess whether two-four cycles feel satisfying and whether broader/slower
routes remain readable and enjoyable. Injured entry, novice behavior and correlated
normal-wave collection failures remain gameplay uncertainties.

### Windows evidence and limits

The actual graphical boss fixture was launched with isolated settings, its title
and boss/HUD observed, and Root Reach warning geometry observed. Live warning/support/damage/defeat
telemetry was recorded.
A stationary unplayed character died; that is a runtime check, not a successful
playthrough. The final HUD uses larger new boss/dash labels. No human boss clear,
full seven-wave Windows playthrough, perceptual listening approval for the new
cues, or production performance percentile is claimed. Existing entity/effect
caps remain; this chapter schedules at most24 ordinary enemies in one wave and
four boss supports. Full-campaign stress/performance remains later work.

### Development-only boss fixtures

```powershell
.\build-phase34-m3-msvc-x64\Release\Threadlight.exe --m3-telemetry --m3-seed 7382 --m3-boss-fixture zero
```

Names:zero (no ranks), offense (RazorIII), defense (HeartwoodIII), mobility
(QuickII+RhythmI), mixed (RazorI+LivingI+HeartwoodI). This opt-in command uses normal
health, AI, collision, rewards and damage; it only chooses an isolated starting
encounter/build. It is visibly labeled, has no campaign carryover, and is not a
normal checkpoint. There is no automatic win, invulnerability or hidden reward.
Normal no-argument play and permadeath always start Wave1. Invalid arguments exit2.

## Orchestration and owner acceptance

Runtime turn metadata confirmed GPT-6.1 Sol/high lead; GPT-6 Luna/max invariant and
scenario worker; GPT-6 Luna/high HUD/audio worker; and a separate GPT-6.1 Sol/high
read-only reviewer. File ownership was disjoint. Lead reviewed diffs and independently
verified significant results. Independent review found the Root Reach endpoint
boundary and no other actionable correctness/scope defect in its reviewed snapshot.
The final development-fixture/UI/telemetry/offer delta was also independently
reviewed read-only, with no material issue found.

Prioritized manual Windows acceptance:

1. Play the normal chapter from Wave1: check the six encounter identities, collection
   time, spark carryover and earned rank pacing. Death must restart Wave1.
2. Fight Rootwarden with low offense and a three-rank build: verify warnings, locked
   step, outer-root damage access, both phases, exposure opportunities and enjoyment.
3. Pause/resize during warning, active step and recovery; inspect minimum size and
   available startup DPI scales. Check dash fill/readiness and mouse/keyboard offers.
4. Defeat it: attacks/supports disappear harmlessly; core requires contact; no free
   healing/signature/upgrade menu; final screen retains pending credits/live value.
5. Listen for the new warning/phase cues under combat and verify accepted volume,
   settings, damage feedback and frame pacing remain satisfactory.

M3 is a candidate for owner acceptance only. Do not merge develop or main, and do
not proceed to M4 without explicit authorization.
