# Unweaver intelligence QA revision

Supersedes the boss behavior measurements in UNWEAVER-EXPERIMENT.md. Baseline: 93e91cf. Director source and earlier-boss configurations are unchanged. HP, raw damage and movement speed are unchanged.

## Diagnosis and changes

- `unweaver::makeRays` omitted six of eighteen angular slots per ring. It now fills all eighteen: 36 shots normally, 54 in Final Convergence. Rings retain 54-tick staggering, rotating/predictive orientations and Fractured Rhythm's staggered subgroups. Reservations and ray storage now accommodate all 54.
- `chooseStyle` strongly favored Predator's movement score and only reconsidered at attack commitments. Scores now weigh repetition, offense, perimeter and response evidence, penalize ineffective strategies, and reconsider during positioning with hysteresis. The short policy matrix now visits Architect as well as Predator; it does not establish long-fight Puppeteer effectiveness.
- Stalk movement used a single offset pursuit goal, no silk geometry. `position` scores eight local candidates at 5 Hz against predicted movement, approach velocity, distance, perimeter and up to 32 sampled active silk segments, including swept crossings. Chasers elicit spacing and a once-per-cycle committed interception with the existing charge warning. Recovery retains full damage and bonuses while allowing positional movement instead of standing in persistent silk. Interception cannot repeat indefinitely before the radial attack.
- Later mechanic choice responds to offense/repetition/style instead of cycling mechanically through four entries.
- Support placement checked player clearance only, while the major-volley freeze also applied during recovery because planCommitted stayed set. New Unweaver placement searches bounded boss-clear positions; precommit separation resolves subsequent boss overlaps before the safety snapshot. Recovery releases the freeze. Major volleys still deliberately hold formations for the modeled body paths; this is a temporary safety constraint, not general navigation. Attacks and ordinary targeting resume outside those commitments. No physical boss obstacle or magical shield was added.
- HP credit and phase completion remain separate: health can cross later thresholds while an earlier phase's required experience is unfinished. No damage accounting, phase gates or victory rules were weakened to create difficulty.

## Fairness

Complete angular coverage is not a solid wall: normal inter-projectile spacing expands with radius. The 102-tick warning, 210 px/s speed, five-pixel shot radius and 54-tick ring spacing remain. Bounded response witnesses now aim between populated angular slots and test actual swept geometry, dash availability, echoes, silk and committed external threats before admission. Close configurations can be rejected rather than force damage. All projectiles remain individually Needlecatch-compatible; the boss remains damageable; commitments do not retarget. Ray warnings and phase-four outlines no longer advertise broad empty sectors.

## Evidence

- Debug build and all 42 regression suites pass. New checks cover full angular occupancy, chase spacing, silk-side avoidance and 64 deterministic boss-clear placement cases. Existing tests cover all five radial families, phase progression, cancellation, signatures, seal and rewards.
- Identical 240-case admission/replay matrix: baseline 220 admitted, zero replay failures; revised 215 admitted, zero replay failures. Revised admitted set uses ten dash routes (four protected projectile contacts); baseline selected none. Rejections are legitimate conservative planner outcomes, not proof the rejected situations are impossible.
- Same nine policies/four builds/two seeds, each replayed exactly: baseline 81,738 ticks, 49,670 boss damage, 227,150 player damage, zero style changes, two reversals. Revised 81,333 ticks, 49,310 boss damage, 220,700 player damage, 68 style changes across 72 runs, three reversals. These ten-second cases show different decisions, not an aggregate damage improvement; death censoring also differs. No claim of universal tactical superiority.
- Actual defense autoplayer: previous build won at tick 22,256; revised build died at tick 33,433 in Phase II, boss vitality 42,663. This demonstrates changed offensive access and pressure for that policy; the longer survival is not a pacing success criterion. A complete revised real-executable victory has not been demonstrated. Headless sequential experience/victory regressions pass; human exceptional-play winnability and pacing remain acceptance risks.
- Actual Debug accelerated run (eight fixed ticks per rendered frame), excluding first 120 frames: simulation mean 0.459 ms, p99 1.181 ms, max 15.622 ms; draw submission p99 1.146 ms. Peak sampled private memory 59,203,584 bytes; peak sampled projectiles 36. This was Phase I–II, not worst-case Phase IV or an older-hardware guarantee. CPU spikes and human input cadence need continued review.
- Actual 14-case framebuffer smoke passed; minimum-window Phase IV and Thorn Dominion captures inspected. Full-circle warnings and separated supports are visible. No interactive human or listening QA is claimed.

Local evidence: portable-release/ai-before.txt, ai-after.txt, overhaul-tests.txt, unweaver-overhaul-qa/fight.log, frames.csv and render/. The final tiny silk-crossing scoring refinement preserves the reported headless totals; render/performance measurements precede that refinement.

## Playtest priorities

Compare aggression with perimeter/reversing play, change tactics midfight, and judge whether moving recoveries retain rewarding attack opportunities. Check dash-exhausted close radial responses, Needlecatch, all four formations and Phase III–IV combinations. Report seed/build/phase plus attack state for apparent unavoidable deaths. The model is bounded and cannot guarantee every human response or arbitrary combined encounter.
