# Unweaver combat repertoire and live Director QA

Historical implementation and QA snapshot. The current authoritative behavior, final verification status and owner controls are consolidated in [UNWEAVER-SIEGE-QA.md](UNWEAVER-SIEGE-QA.md); use that guide for the final encounter.

Baseline: `b6f962c` on `codex/unweaver-architecture-review-20261030`. Earlier Developer QA packages remain intact. This revision changes neither boss HP/raw damage nor ordinary movement speed (155 px/s). It introduces no runtime services or dependencies.

## Diagnosis and implementation

`unweaver::commit` previously wrapped almost every non-seal mechanic in a major radial sequence. All non-final families used two full rings with relatively small orientation/timing differences. Attack objectives could repeatedly override the family choice. Positioning also generally sought distance rather than deliberate contact. The instrumented nine-policy baseline spent 45,083 of 81,333 ticks (55.4%) in committed major radials; all swept body contacts occurred against the player chasing the boss. This supports repetition concerns, but does not establish a universal 90% rate.

`src/unweaver.h` now owns seven utility-selected tactical options: radial, interception charge, contact hunt, flank, focused fan, bait/counter and standalone weave trap. Soft recency penalties prevent repeated high-scoring actions from monopolizing selection. Distance, forecast reachability, repetition, perimeter use, dash habits and offense affect choices. This is utility selection, not a fixed rotation. Mandatory phase experiences and explicit developer attack overrides still take precedence.

Hunts seek contact instead of a standoff. Flanks seek an approach around the movement trajectory. Bait briefly gives ground before closing and countering; it has a finite duration. Recoveries retain damage bonuses while allowing close pressure. Charges aim at a confidence-weighted forecast, lock their endpoint before the warning and refuse a target too far away to threaten. A charge can cover up to 260 px over the existing 42 active ticks, versus the old 180 px; this is an attack-specific lunge change, not a generic speed boost. Its 66-tick warning and swept body path are checked before commitment.

`predict` uses recent trajectory curvature and velocity, limits lead after reversals and reduces trust after forecast misses. Interception outcomes feed back into prediction error and action fatigue. A half-second forecast is also compared with a plain velocity projection for diagnostics. Already committed endpoints, ray directions and echo paths never retarget.

Standalone Echo Hunt, severance, corruption and seals no longer require an accompanying radial. Focused fans are two narrow seven-shot groups with a 72-tick warning and shorter projectile lifetime. Neither focused nor standalone attacks earn the mandatory radial token. `src/main.cpp` preserves support snapshots and offensive coordination during validated charges as well as volleys; recovery releases formation holds. Phase cleanup clears the new pending tactical state.

## Distinct full-circle families

| Family | Geometry and tactical rhythm |
|---|---|
| Shattered Halo | One simultaneous 18-direction ring; precision spacing, short sequence. |
| Rotating Eclipse | Three 18-direction rings, half-slot rotations, 80 ticks between rings; successive escape timing. |
| Fractured Rhythm | Eighteen directions split into three asynchronous angular cohorts, 42 ticks apart. |
| Predicted Escape | Two full rings 90 ticks apart, orientation derived from the player's predicted motion and orbit tendency. |
| Final Convergence | Three rotated full rings with interleaved odd/even releases, combined with Phase IV's echo/toolkit. |

All families populate the complete angular set across their sequence. The maximum remains 54 planned rays. Family utility scores and recency replace objective-driven repetition. Warnings expose each locked subgroup for its full windup before emission; labels, cadence, pulse groups and cue colors distinguish them. Needlecatch and ordinary boss damageability remain unchanged. There are no omitted broad safe sectors.

## Live Waves 29–34

The existing initial Director schedules and wave identities remain. Three reviews occur at quarter-wave intervals. `campaign::reassess` changes only unspawned entries more than 120 ticks away, using current heading, repetition, perimeter and offense evidence. It changes role-dependent approach lanes by at most 0.5 radians and packet timing by at most 24 ticks from the supplied schedule. Each review starts eligible future entries from the initial adaptive schedule, preventing cumulative drift.

Counts, enemy composition, spark/score totals, encounter duration and final deployment deadline remain exact. Composition substitutions were intentionally not added: coordinated deployment and timing supply adaptation while retaining the economy the owner liked. Active units and commitments are never teleported or retargeted. Retiming is rejected when its weighted 240-tick burst exceeds 125% of the initial adaptive schedule; the fixed ceiling prevents reviews from compounding threat multipliers. Existing Brute protection and Hunter interception roles remain in use.

## Comparative evidence

Same nine policies, four builds and two seeds, each repeated exactly (72 pairs), starting in Phase II for ten-second cases:

| Metric | Baseline | Revised |
|---|---:|---:|
| Simulated ticks before death/cap | 81,333 | 81,163 |
| Major radial occupancy | 55.4% | 45.9% |
| Player damage, centi-HP | 220,700 | 272,600 |
| Swept body contact ticks | 3,285 | 3,982 |
| Boss damage, centi-HP | 49,310 | 103,930 |

These are finite, death-censored policies. Damage rises about 24%, but boss damage also rises: close engagements create both risk and offensive access. The first mandatory radial biases short samples upward. Longer sixty-second defense-build cases spend 21.1% in major radials, versus 52.9–59.1% previously. Their outcomes are mixed:

| Policy | Baseline player damage (HP) | Revised player damage (HP) | Baseline/revised contact ticks |
|---|---:|---:|---:|
| Clockwise orbit | 115.5 | 87.0 | 0 / 81 |
| Defensive movement | 115.5 | 82.5 | 0 / 0 |
| Reverse after defeating a counter | 66.0 | 85.5 | 0 / 34 |

Some evasive strategies are safer despite more varied pressure. No universal superiority or human difficulty claim follows from these totals. The longer samples meet the 20–40% tuning hypothesis; it is not an invariant or hidden difficulty scaling rule.

Controlled circular prediction error falls from 48.34 px (linear) to 2.84 px (curved). Reversal and low-confidence tests verify shortened lead. Native mixed Phase IV telemetry recorded mean half-second errors of 65.03 px versus 69.92 px for linear prediction: the benefit is smaller under changing play.

The baseline comparator was built from the old boss header in an isolated harness with the same policy code. Counter-observation semantics were corrected to match production in both comparator and revised runs. Evidence is under `portable-release/repertoire-baseline-extended.txt` and `repertoire-after.txt`.

## Verification and limits

- MSVC x64 Debug build and 43/43 suites pass, including four-phase lethal overflow/victory, real experience execution, fairness replay, locked charges, forecast quality, standalone token isolation, support placement, signatures, Needlecatch, cleanup, economy and changing Director profiles.
- Radial matrix: 199 of 240 admitted, zero replay failures; fourteen selected dash routes. Rejected cases reflect bounded policies, not proof that no human response exists. Charges also require an admitted swept response. These checks cannot guarantee arbitrary pursuit/support combinations or human execution.
- Fourteen native framebuffer scenarios passed (26 exports); representative Phase IV warnings and support scenes were inspected. Final packaged Fractured Rhythm and Convergence subgroup warnings were also exported and inspected. These captures do not establish moving readability or fun.
- Native Wave 29 run executed reviews at ticks 900 and 1800, changing 26 and 14 future entries before the automated player died. The timing refinement is also covered by deterministic schedule tests.
- The developer autoplayer was extended to respect validated charge routes and observed boss body motion. It receives no extra health, damage, immunity or rewards. Its results are not directly comparable with the older less-capable autoplayer, and its knowledge of admitted escape routes biases charge accuracy toward misses. One motion-aware defense run died in Phase II at tick 8765. Final packaged offense and mobility runs died in Phase II at ticks 6207 and 12721, with 25.1% and 20.1% radial occupancy respectively. A native Phase IV sample stayed alive through 1800 ticks with 124 HP remaining. A complete native revised victory has not been demonstrated; headless sequential victory tests pass. Human exceptional-play winnability and pacing remain acceptance risks.
- Fifteen-second native Debug Phase IV sample, Intel Iris Xe, configured 240 FPS: after two-second warmup, simulation p99 0.162 ms per rendered frame (0/1 fixed steps), draw submission p99 1.215 ms, interval p99 5.101 ms. Peak sampled private memory 58,875,904 bytes (~56.1 MiB), peak sampled projectiles 53. One startup discarded-time event totaled 982 ms; none after warmup. This is CPU submission timing on one machine, not GPU timing, a leak proof or older-hardware certification. Final logging and Director ceiling checks do not change the boss behavior measured in that sample.
- Final packaged Wave 29 run again executed two reviews with 26 and 14 future changes; its accelerated eight-tick frames had simulation p99 1.259 ms, maximum 1.549 ms, and peak sampled private memory 57,614,336 bytes. This includes actual review execution, not just helper microbenchmarks.
- Packaged launcher smoke checks cover all eleven selectable waves, all four phases, four builds, additional seeds and optional frame logging (sixteen launches total). Every process exited successfully. The normal profile hash remained unchanged; the trusted Release executable hash remained unchanged.

## Developer use and human acceptance

Use the packaged selector for builds/phases/waves/seeds, or the Wave 29, Unweaver and Phase IV shortcuts. F5 restarts the current fixture encounter. Logs include arguments/seed, state ticks, committed family counts, utility decisions, radial occupancy, body pressure and prediction outcomes. `QA.ps1 -Performance` adds a frame CSV. Counts of utility decisions include rejected choices; family counts are admitted plans, not necessarily completed attacks after a phase cancellation.

Developer scenarios suppress profile writes and legitimate progression/unlocks, including direct executable startup. Normal campaign reward paths remain in place. No full human campaign, legitimate Violet unlock, listening QA or manual input playthrough was performed.

Owner priorities: distinguish all five patterns while moving, try dash-exhausted defenses, reverse after predictions, bait charges, compare risk/reward during close recovery, verify support combinations and judge late-wave changes after switching aggression mid-wave. Human play decides whether this repertoire is fun, readable and appropriately difficult. Do not merge based on automated results alone.
