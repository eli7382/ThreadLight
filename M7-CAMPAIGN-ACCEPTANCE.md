# M7 campaign acceptance candidate

Baseline: owner-accepted M6 `4ec554b11024d477ba31bb825e5f8756321de881`.
Feature branch: `codex/phase34-m7`. Develop/main and previous builds are preserved.
The campaign remains35 waves. This milestone completes progression choices and
adds the owner's Needlecatch/Bloodstitch request. It is a candidate for human
acceptance, not an automatic release or a claim of measured human balance.

## Module ownership and scope

Lead: main/combat/ranged/projectile interception, integration, HUD, CMake,
Windows builds, verification and handoff. Luna Max: ordinary catalog/utilities
and signature catalog/effects in disjoint headers/tests. Luna High: bounded
campaign/offer model. Independent Sol High: shared-combat/integration review.
No new framework, controls, bosses, permanent barriers, music or waves.

## Ordinary progression

Earlier seven permanent upgrades and Restitch values are unchanged. Added:

| Upgrade | Rank I | Rank II | Rank III | Rank IV | Rank V |
|---|---|---|---|---|---|
| Ironbark |6% less damage|11%|16%|20%|24%|
| Longstride |4.0x dash speed|4.3x|4.6x|--|--|
| Needlecatch |one interception /5s|one /4s|one /3s|--|--|
| Bloodstitch |3HP /5 ordinary kills;9HP cap|3HP /4 kills;12HP cap|3HP /3 kills;15HP cap|--|--|

Base health100HP, max Heartwood160HP. Base dash3.7x, movement315maximum,
minimum cooldown1.4s; dash duration/protection unchanged. Longstride changes
travel only. Ironbark applies once after dash/immunity gates, before Cocoon,
using integer half-up rounding and a minimum1centiHP for positive damage.

Needlecatch acts on a projectile, without immunity, refunds, damage or rewards.
Historical currently active silk only; just-laid strands cannot retroactively
intercept earlier contact during the same tick. Choose one chronological
interception across ordinary and boss pools, stable pool/ticket tie-breaking.
A physical player impact occurring earlier or at the same time wins, even if
the player happens to be immune. Reserved telegraphs, floor hazards and melee
are not projectiles. Cooldown begins only after actual removal, freezes outside
combat and resets at encounter boundaries. Needlecatch is not offered until
the next encounter can contain projectiles (Wave8).

Bloodstitch counts only a successful once-only ordinary-silk defeat. Procs,
Anchor bonus kills, buds and timeout removal do not count. Groups at fullHP
are consumed without storing a heal. Only actual healing uses the wave cap;
partial recovery clamps to missingHP and remaining cap. Kill progress/cap reset
at the next encounter; cleanup remains part of the current encounter. No boss
restoration or healing at transitions is introduced.

There are11 permanent upgrades and41 total available ranks; a successful run
cannot simply max everything. Existing1.15 RankV weights,1.35 investment boost,
three-offer drought protection, discovery and recovery rules remain. Thresholds
20/30/40/48 and manual-only collection remain; no economy rubber-banding.

## Signatures and feedback

Each of the first four bosses offers3 distinct unowned signatures. Reserve an
offensive and utility choice when each category has candidates, draw the third
from the rest, then shuffle on an independent seeded RNG. Cache the offer once
per boss reward ID. Manual collection and all pending ordinary credits precede
the gift; acquisition does not depend on the core. Wave35 grants no gift.

Pool: Afterimage, Crossweave, Cocoon, Resonant Loom (accepted effects preserved),
plus slow-only Slipknot and Anchor Stitch. Four acquisitions leave3 candidates
for the fourth gift, so three usable cards remain available.

Slipknot: ordinary HP hit slows ordinary locomotion25% for90 combat ticks,
per-enemy300tick cooldown. Boss Stalk pursuit slows8%; committed trajectories,
warning clocks, countersteps, projectile speed and attack cadences stay intact.
Anchor: one global target mark; a second ordinary HP hit within144ticks deals
1x base silk damage (.5x to bosses), four-second cooldown starts at the first
mark. Target swaps do not replace the mark; failure/death clears the mark but
never refunds cooldown. No proc can mark, charge, slow or recursively trigger.

Gold signature cards share ordinary-card mouse/keyboard focus and hit testing.
Descriptions show effects and damage at current stats. Needle catch feedback
is a restrained burst/ring; Bloodstitch shows actual healing allowance and kill
progress. Slipknot and Anchor have distinct thin target markers. No extra active
buttons, glow flood or player-immunity effect is introduced.

## Acceptance gates

Automated checks establish accounting/collision/clock invariants, not enjoyment.
Human Windows acceptance still needs ordinary acquisition of the new upgrades,
Needlecatch path placement, Bloodstitch kill attribution/caps, signature choice
and progression through35, late-game support pressure, readability and audio,
pause/resize/DPI and minimum window behavior, and performance during full runs.
Do not merge todevelop or change main without owner approval.


## Verification evidence and limits

Pinned Windows Release build succeeded: MSVC19.44.35229.0/toolset14.44.35207,
SDK10.0.26100.0, C++20, raylib5.5. New directory:
`build-phase34-m7-msvc-x64`. Focused checks passed, then full CTest passed34/34,
0 failures,2.96seconds after the final optimization. Signature tests include
45136 active checks and1000 four-gift seeded runs; integration291 checks cover
cross-pool chronology, contact-first/ties, historical-vs-future silk, ordinary
kill ownership, Ironbark-before-Cocoon and deterministic30/60/120/144/240Hz
render schedules. New suites also passed strict g++ O2/NDEBUG/Werror builds.
Existing suites remain present; M6 exhaustion setup now caps all permanent IDs
except Magnet explicitly rather than assuming Magnet is the last permanent ID.

Independent Sol review found three integration issues before handoff: newborn
silk retroactively blocking earlier contact; dead Anchor marks; and89 instead
of90 possible slowed locomotion updates. All were corrected and reviewed.
The future-strand regression includes a control showing the incorrect future
strand WOULD intercept the same shot; historical silk does not. Anchor death
clears marks while retaining cooldown; post-kill ordinary processing cannot
create a mark on a corpse. Slow factor is sampled before its clock decrements.

Windows hidden/silent captures were exported and inspected for the first and
fourth signature cards at minimum-size startup, plus the utility HUD. Card
text, selected state and button geometry were readable. Review corrected the
confirmation button/instruction overlap for ordinary and signature screens.
Artifacts: `qa-gift-1`, `qa-gift-4`, `qa-utilities`, `qa-final-gift` under the M7
build. Invalid gift index, mixed fixture arguments, and minimum-window mode
without render-check each returned exit2 before startup. No input was sent to
the owner's existing game. These are scripted graphical checks, not human play.

Synthetic Needlecatch CPU stress uses128 in-flight projectiles,520 strands,
1000 repeated queries with a nonzero checksum. Final MSVC mean298.05us,
p95315.5us,max1438.2us on this run. Broad-phase AABB rejection and an exact
time-zero early exit reduced unnecessary scans. This is a bounded helper test;
it does not measure full-frame GPU performance or prove a late-game FPS target.
The feature can intercept at most one projectile per cooldown. Previous eight
executables' SHA256 hashes matched their pre-M7 snapshots.

Runtime metadata was checked directly for the last worker/lead turns:
- Lead and independent reviewer: `gpt-6.1-sol`, high.
- Catalog/utilities and signature/effects workers: `gpt-6-luna`, max.
- Bounded campaign/offer model worker: `gpt-6-luna`, high.
Workers reported changed files and focused commands; the lead reviewed their
code and independently rebuilt/reran significant checks on Windows.

## Quantitative balance review

The reproducible optional command is
`Release/ThreadlightM7CampaignTests.exe --model`; it passed on MSVC and completed
in2086ms. The default CTest entry runs only fast invariants. Output is saved at
`build-phase34-m7-msvc-x64/model-output.txt` and can be regenerated.

These are explicit MODEL ASSUMPTIONS, not measured gameplay/skill estimates:
2000 paired seeds per scenario; kill/collection chances .60/.65, .78/.80,
.92/.93 with a shared run deviation of-6..+6 percentage points, and3 points less
kill probability for Brute/Bulwark. Missed kills despawn; missed pickups expire.
Bosses are assumed defeated, with bounded support rewards and optional10-value
cores at the first four. The model does not simulate combat survival, pickup
travel/lifetime, ability effects, or success-conditioned player skill. Magnet
sensitivity assumes+8 collection points from Wave10, not a measured gain; it
does not charge a rank cost, so it is a convenience sensitivity rather than a
build comparison. Production thresholds, schedules and rewards supply accounting.

| Manual collection scenario | Base credits P10/median/P90 | With assumed Magnet gain |
|---|---|---|
| Low |10/12/14|11/13/15|
| Medium |16/18/21|17/20/22|
| High |22/25/27|24/26/28|

The exact1364-spark ceiling still permits29 issued credits with26 remainder;
that is not expected progression and includes final-encounter credits that may
remain unspent. A low-collection run may enter late chapters with far fewer
ranks than the preliminary20-28 target. Keep accepted20/30/40/48 thresholds for
this candidate; do not hide rewards or retune accepted bosses from this model.
Observe actual successful Windows runs before considering a threshold change.

Offer distribution is modeled separately with exactly16/22/26 resolved credits,
2000 seeds per policy/budget/health condition, production drought/discovery rules
and no simulated boss gifts. Healthy primary-target completion:

| Policy / primary |16 choices|22 choices|26 choices|
|---|---:|---:|---:|
| Offense / RazorV |80.90%|97.55%|99.80%|
| Defense / HeartwoodV |78.50%|98.00%|99.80%|
| Mobility / QuickstepV |79.65%|98.05%|99.90%|
| Rotating hybrid / RazorV |1.60%|9.75%|24.35%|

The hybrid intentionally rotates investments; low primary completion is not a
failed specialization policy. Healthy runs averaged roughly2.2-2.8 fully maxed
upgrades at16 choices for specialist policies,4.1-4.8 at22, and5.4-5.8 at26.
A deliberately harsh repeated35HP-deficit-before-EVERY-offer stress spends
roughly10.3 of16 choices on Restitch, leaving only~5.6 permanent ranks. This
resets HP to a fixed deficit; it is not cumulative damage or an expected run.
It illustrates the recovery/progression tradeoff; it does not prove healing is
mandatory. Bloodstitch offers an aggressive bounded recovery alternative, whose
actual influence on collection and survival still requires playtesting.

## Prioritized manual acceptance

1. Normal play: earn Needlecatch/Bloodstitch through sparks and test ordinary
   pickup/credit flow. Place active silk in a shot's route: one catch, visible
   cooldown, no player immunity. Old harmless silk, melee and floor hazards
   must not be intercepted. Verify Bloodstitch3HP at5/4/3 direct silk kills and
   caps9/12/15; signature/timeout kills and full-health groups must not bank heals.
2. First and fourth boss gifts: three distinct useful choices; mouse and all
   existing keyboard selections; no duplicates, credit skips, rerolls or held
   confirm leaking into dash. Test choosing something other than Afterimage
   first and utility/offense combinations through the later bosses.
3. Slipknot/Anchor and Ironbark/Cocoon: readable marks, unchanged committed
   attacks, no recursive bursts/healing, no cooldown refunds after target death.
   Check Longstride overshooting, clamping and the unchanged dash protection.
4. Play a full35-wave run with realistic collection, plus a lower-offense late
   boss fixture. Check permadeath, final victory, pause/resize/DPI, menus/audio
   and full-game performance. Report credits purchased, healing expenditure and
   difficult waves; those observations should drive any final balance revision.

Release recommendation: technically ready for owner acceptance testing; human
campaign balance, listening/readability acceptance and full-load GPU/soak remain
open. No full M7 human playthrough or release approval is claimed. Stop after M7.
