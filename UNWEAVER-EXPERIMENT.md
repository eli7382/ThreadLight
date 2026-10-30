# ThreadLight: Unweaver experiment

Baseline: `4dead78a30b88f7536b385da82c572d4132db8e9`.
Branch: `codex/unweaver-architecture-review-20261030`. Never merge automatically.

## Delivered implementation

- Foundation: cumulative single-credit vitality and sequential mandatory experiences implemented/tested.
- Playable boss: all five radial families, positioning, Hunt interception, Echo Hunt, severance/corruption, Binding Seal, finite formations, Phase IV transformation implemented.
- Intelligence: campaign-local observations, confidence/objectives, style hysteresis, counter outcomes and target-sensitive formations implemented.
- Campaign: adaptive deployments on indices 28-33; earlier bosses enable independent experimental support configs. No normal spawn counts/durations changed.
- Integration: end-tick hostile cancellation shared with regression test. Resize also clears hostile state before retelegraphing.
- Isolated MSVC x64 Release build succeeds; all 42 CTest suites pass (final run: 2.66 seconds).
- Current 840-HP encounter: actual executable defense autoplayer won at tick 22256, including 90 countdown ticks: 184.72 simulated combat seconds, 77.5 player HP remaining. It executed 35 admitted plans, 612 shots, 19 echoes, 8 cuts, 16 corruptions and four formation events. Accelerated presentation preserves the 120-Hz simulation; this is not human pacing evidence.
- Stationary zero-upgrade player died in Phase I at tick 1833 (14.53 combat seconds), with boss vitality untouched. No HP-dependent weakening is used.
- Final render smoke: 14 cases, 26 PNG exports, every process exit zero. Includes all four phases, moving echoes, severance, corruption, Thorn Dominion, earlier bosses, two Director profiles, settings and Violet presentation. Representative actual framebuffers were inspected, including the final Thorn Dominion scene; exports do not prove moving readability.
- Signature restoration regression fixed: disabled silk retains its stable history ID, so restoring corrupted silk cannot repeat consumed Crossweave claims; disabled edges cannot complete a Cocoon loop. Previously earned barriers survive.
- Windows Computer Use helper failed twice on initialization (kernel asset path / OS error 3). No interactive or listening QA claimed. Use real executable fixtures and exported framebuffer captures.

## Contracts

- Effective damage is credited once. Zero vitality can precede encounter completion; HUD explicitly signals remaining sequences. No HP refill/fake damage/procs.
- A phase-specific mechanic must execute. Seal expiry counts; solving it is optional.
- Major radial sequences have no seal reduction; their reserved/flying projectile lifetime must finish before another seal can start.
- Only end-tick/resize boundaries clear hostile pools, reservations, support accounts/queue and commitments. Legitimate pickups, friendly resources/upgrades and observations survive.
- Supports hold positions during admitted major volleys. Offensive commitments wait for compatible slots; this keeps modeled body paths stable, without HP-dependent mercy.
- Boss support currency/score shares the original encounter reward ceiling, including Caller fusion; zero-currency reinforcements retain stable combat IDs and genuine kill/healing semantics. Dismissal is never a kill.
- Director preserves exact enemy counts/composition/reward/score totals, authored clocks and weighted burst envelope. It adapts deployment angle/timing and positional roles rather than inventing new economy classes.
- Runtime witnesses are bounded modeled responses, not mathematical or human fairness guarantees.
- Experimental developer runs never save profile settings or award campaign/cosmetic records.

## Ownership and consequential choices

- `src/unweaver_phase.h`: single-credit cumulative damage and mandatory sequential experiences. Excess damage remains credited rather than becoming another later damage event. HUD shows truthful remaining vitality and an explicit sequences-remain state at zero. Final death/reward occurs once, after Phase IV finishes.
- `src/unweaver.h`: bounded strategic utilities, style hysteresis, positioning, locked attack plans, resource reservations, mechanic execution and conservative response witnesses. Five radial families reuse validated ring components; telegraphed origins/directions do not retarget. Major volleys remain fully damageable. Broken seals, expiry, counters and overextension provide different openings.
- `src/player_observation.h` / `src/campaign_director.h`: bounded campaign-local statistics and deterministic late-wave deployment transforms. Restart resets observations. The Director preserves composition totals as well as counts; composition substitution is intentionally deferred to avoid changing the established reward/threat economy. Placement, approach, roles and bounded timing are adaptive.
- `src/support_formations.h`: positional formations and independent earlier-boss configurations, without invented shields. Boss support reward allocations share the old encounter ceilings. `src/reward_ledger.h` preserves those allocations through Caller fusion and zero-currency kills.
- `src/main.cpp`: existing combat order, real movement/upgrades, silk and signatures, Needlecatch, support commitments, reward delivery and campaign integration. `src/encounter_cleanup.h` cancels all hostile ownership at an end-tick boundary, without awarding dismissals or discarding earned pickups.
- `src/unweaver_render.h`: procedural organic/geometric silhouette, locked geometric warnings and Phase IV transformation, retaining native raylib rendering and existing audio.

The existing offensive coordinator remains the shared admission mechanism. Supports stop repositioning during admitted major volleys and new commitments wait for compatible slots. This deliberately bounds the modeled combination instead of trying to predict arbitrary simultaneous independent AI trajectories. Damaging and killing those supports remains possible.

## Validation and limits

- Phase tests cover threshold/overflow/duplicate events, mandatory tokens, sequential transitions, simultaneous lethal outcomes and single victory. Combat and cleanup regressions cover real boss damage, Needlecatch, warning activation, seal reduction, segment identities, cancellation, pending commitments and reward preservation.
- Radial admission/replay matrix: 240 combinations across five families, 12 positions, two mobility levels and two dash-readiness states; 220 admitted, 20 rejected, zero replay failures/unprotected contacts among admitted cases. Selected routes were dash-free. A separate synthetic validator stress exercises ready/exhausted dash immunity; it is not an authored attack or balance claim.
- Deterministic real-core policy stress: nine policies, four builds and two seeds, each repeated exactly (72 pairs); 81,738 ticks, 131 accepted plans, 65 counter outcomes and two behavior reversals. Five aggressive cases died before an admitted plan. These short ten-second cases stayed in Predator; separate profile tests cover other style choices and hysteresis. Long-run strategic switching still needs human observation.
- Runtime safety uses bounded candidate responses, reaction delay, four-tick input holds, actual movement/dash timing, collision radii and committed hostile snapshots. It can reject combinations without a modeled witness. It cannot prove arbitrary future player choices, human execution, every dynamic support interaction, or global reachability. No universal fairness guarantee is claimed.
- Actual Intel Iris Xe/OpenGL 3.3, 15-second normal-rate Phase IV sample: after two-second startup exclusion, simulation p99 0.210 ms per rendered frame (two fixed ticks), draw submission p99 0.871 ms, frame interval p99 16.954 ms. One startup discarded-time event of 992.936 ms occurred; none after warm-up. This is one machine/sample, not an older-hardware certification or GPU timing measurement.
- Full accelerated fight: peak sampled private memory 66,211,840 bytes (63.1 MiB), ending 65,880,064; simulation p99 1.314 ms per eight-tick frame. No discarded simulation time. Bounded pools are tested; finite samples cannot prove absence of leaks. That run did not select a Sower formation; the separate Thorn fixture exercises it.
- Existing profile SHA-256 remained unchanged through developer QA. Fixtures suppress settings/records/rewards; the legitimate campaign Violet Ascendant grant path remains in place and existing campaign/cosmetic suites pass. No full human campaign or legitimate cosmetic unlock was performed.

Detailed local evidence is isolated under `portable-release/unweaver-qa`, including `package-smoke/results.csv`, `final-signature-fix-victory/game.log`, and `final-performance-summary.json`. Old 600-HP tuning runs are superseded and are not final balance evidence.

## Developer controls

Build: existing pinned MSVC/raylib CMake workflow; isolated directory `portable-release/unweaver-build`.

- `--unweaver-fixture zero|offense|defense|mobility|mixed` starts Wave 35 directly.
- `--unweaver-phase 1|2|3|4` selects starting phase in that fixture.
- `--unweaver-attack 0..4`: Halo, Eclipse, Rhythm, Predicted Escape, Final Convergence.
- `--unweaver-mechanic 0..4`: Hunt, Echo, Severance, Corruption, Seal.
- `--unweaver-formation 0..3`: Royal Guard, Predator's Net, Thorn Dominion, Shrapnel Cage; queues that formation at fixture start.
- `--m7-seed N`: deterministic campaign/deployment seed.
- `--director-fixture 29..34 --director-style runner|aggressive|dasher|defensive`: late-wave fixture with a documented synthetic prior. Normal campaign observes only real play from Wave 1.
- Earlier fixtures: `--m5-dancer-fixture mixed`, `--m5-conductor-fixture mixed`, `--m6-hollow-fixture mixed`.
- `--m6-render-check` exports isolated warning/active PNGs in the working folder and exits; `--m7-min-window` selects minimum QA size.
- `--dev-capture` additionally captures the later composed scene; use only in a new QA folder.
- `--dev-autoplay --dev-ticks N` runs a bounded automated fixture player; `--dev-fast` advances eight unchanged fixed ticks per presentation for accelerated QA. This is not a normal gameplay option.
- `--perf-log NEW_PATH.csv --m7-telemetry` records diagnostics without overwriting an existing CSV.

## Human acceptance before any merge

1. Play the normal campaign: controls, upgrade choices, audio, menus/settings, earlier bosses and progression; restart and check that adaptive behavior learns the new run rather than retaining the old one.
2. Use mixed/offense/mobility/defense fixtures; judge pursuit, deceptive movement, reaction windows, radial gaps and whether deaths have understandable causes. Try both conserving and spending dash; Needlecatch should remove individual shots.
3. In Phase II, change direction after an echo warning. In Phase III, attack and ignore seal anchors, cross corrupted silk, and use Crossweave/Cocoon around cuts; verify readable warnings and distinct offensive opportunities.
4. In Phase IV and at minimum window size, judge silhouette, combined-threat readability, controls, pacing and the sequences-remain health presentation after burst damage. Exceptional damage should shorten phases without skipping their experiences.
5. Compare Director runner/defensive seeds, all support formations and Waves 14/21/28. Look for impossible traps, excessive waiting, spam or ineffective protective positioning. Accept the 3-5 minute pacing only after human play.

Known acceptance limits: no interactive computer control or listening QA was available; full campaign traversal, fun, human readability, real player difficulty and older Windows hardware performance remain unverified. This is an isolated playable experiment for owner evaluation, not permission to merge.
