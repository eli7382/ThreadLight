# Performance and correctness pass — 27 October 2026

Historical verification report for `bfc41ee`; the approved improvements were
subsequently selected for integration into `develop`. Baseline and branch-state
statements below describe that original pass, not the later integration status.

Baseline: fetched `origin/develop` at `2fbdbf4f37da89489b648ce6c3d97f87efe8bd79`.
Feature branch: `codex/performance-quality-pass-20261027`, in its own worktree.
Existing checkouts, builds, `main`, and `develop` were left unchanged.

## Architecture and investigation

The game uses a main-loop coordinator with header-based, independently testable
gameplay rules. Rendering interpolates entity-owned presentation history while
simulation advances at 120 Hz with an eight-step catch-up cap. Enemy AI, projectile
reservations, attack scheduling, pickup accounting, signatures and boss hazards
use explicit state machines and stable IDs. Projectile/hazard/signature storage
is predominantly fixed-capacity; presentation entities use vectors. The reward
ledger intentionally retains terminal records until a run resets for idempotence.

The baseline already optimizes active-silk lookup, ordinary silk cooldown scans,
some hazard-area calculations, presentation copying, text wrapping, sound mixing,
and knot-distance checks. Those were retained. Existing CSV capture was reused;
no continuous profiling, new frame logging, or GPU synchronization was introduced.
The review covered rendering, ordinary silk and Afterimage contact geometry,
Crossweave/Cocoon/Needlecatch, allocation sites, input buffering, timing, rewards,
and recent optimization changes. No measured memory leak was established.

## Implemented

- **Afterimage contact bounds:** cache the completed path's bounding box at dash
  completion, refresh it after remapping, and clear it at lifecycle boundaries.
  Clearly separated swept targets skip the segment scan. Near targets still use
  the original exact distance predicate and claim bookkeeping. A rounding margin,
  invalid-cache fallback, and extreme-number fallback preserve boundary behavior.
  The cache adds a few scalar fields, without allocations.
- **Cocoon snapshots:** copy its large detector state only when formation is
  eligible. The detector still consumes crossings every owned combat tick,
  including barrier/cooldown ticks. This avoids copying arrays containing 640
  consumed IDs and 640 pending-crossing records on blocked ticks. This is a
  source-derived work reduction; no independent timing gain is claimed.
- **Attack queue:** sorting now runs only when a new request is appended. Pruning
  and granting preserve existing order, including same-tick ID tie-breaking.
- **Silk rendering:** reuse the exact shared Catmull–Rom endpoint between adjacent
  subdivisions and compute each knot's interpolated age once per span. This
  reduces explicit spline evaluations from six to four per stored segment;
  final draw order, widths, colors and collision geometry remain unchanged.
  Compiler optimization can already remove some redundant work, so this count
  is not a measured CPU-time saving.
- **Trail capacity:** reserve 521 knots for the 520-knot bound plus append-before-
  trim. This prevents trail vector growth during play, trading a slightly larger
  initial allocation for predictable capacity. Other vectors and builds are unchanged.
- **Numeric correctness:** reject non-finite frame deltas before integer tick
  conversion; preserve the accumulated valid remainder. FPS helpers also reject
  non-finite results from extremely small positive deltas. Histogram percentile
  queries reject non-finite fractions before integer conversion. Valid 120 Hz
  schedules and overload handling are preserved.
- **Reproducible benchmark:** an optional, OFF-by-default CPU executable compares
  the baseline Afterimage claim implementation with the production implementation.
  It does not instrument or run alongside normal gameplay.

## Measurements

Windows x64 Release, MSVC 19.44.35229 / pinned toolset 14.44.35207, raylib 5.5.
The benchmark uses a synthetic 32-point/31-segment completed echo, 2,048 swept
queries repeated 128 times per trial, five alternating-order trials, and median
nanoseconds per query. Claims reset in batches of 64 IDs. Every query outcome and
each trial's hit checksum must match. No build was running during this recorded run.

| Synthetic workload | Baseline ns/query | Revised ns/query | Ratio |
| --- | ---: | ---: | ---: |
| Distant targets | 1261.25 | 51.51 | 24.48x |
| Mixed positions | 1209.94 | 292.47 | 4.14x |
| Overlapping region | 1117.75 | 1080.75 | 1.03x |

The overlapping result is effectively close to baseline; a small difference is
not evidence of a robust improvement. A 31-segment echo exercises the storage
bound rather than establishing a typical gameplay path length. These are CPU
contact-query timings, not whole-game speedups, utilization, or GPU measurements.

An earlier per-segment broad phase was rejected: its first synthetic benchmark
was slower in every tested workload (about 19–30% more time). Its helper and
temporary tests were removed before integration. The retained whole-path check
amortizes validation and bound construction instead.

Utility render-check captures before and after the rendering edits both had a
frame-interval histogram p50 upper bound of 16.7 ms under the 60 FPS limiter and
zero discarded simulation time. This is smoke evidence, not a meaningful FPS
comparison. The revised Briar Heart capture recorded one 143.26 ms interval and
78.27 ms of discarded time. The preceding frame spent 143.06 ms in the drawing
region, which includes synchronous fixture screenshot work. Screenshot capture
is the likely cause, but a normal-play trace is needed to establish that.

## Validation

- Windows x64 Release game, all regression targets, and optional benchmark built.
- Full CTest: **36/36 passed**, including all campaign, boss, signature, reward,
  timing, input, UI, settings, audio-design and mocked audio-ownership suites.
- Added 20,000 deterministic cached-versus-uncached Afterimage queries, contact
  boundary/remap/extreme-coordinate checks, blocked Cocoon consumption checks,
  queue age/tie/cancellation coverage, and invalid numeric input coverage.
- Existing cross-render-rate tests retain identical simulation schedules.
- Hidden runtime fixtures passed: utility, fourth signature reward at minimum
  window size, Hollow Weaver mobility, and Briar Heart mixed build. Warning and
  active boss captures were generated. Minimum-size reward and Heart active
  screenshots were visually inspected. Renderer: Intel Iris Xe, OpenGL 3.3,
  150% Windows scaling. Fixtures disable audio and settings persistence.
- `git diff --check` passed. No balances, encounters, rewards or controls changed.

Local captures/logs are under `build-windows-msvc/qa/`; generated builds are ignored.
The README contains build and benchmark commands. No human 35-wave playthrough,
listening test, sustained memory soak, oldest-supported-GPU run, or full gameplay
GPU profile was performed. The static fixtures do not exercise a moving silk trail.

## Prioritized follow-up proposals — approval required before implementation

Benefits below are hypotheses or source-derived operation limits, not measured
frame-time promises. Effort estimates include implementation and focused checks,
and may expand after profiling.

1. **Measure GPU work and dense-signature CPU costs first** (roughly 0.5–1 day).
   Add optional, bounded counters for candidate pairs, claims and batch flushes,
   plus asynchronous GPU timing or an external capture. Existing wall timers mix
   submission, driver waits and presentation. Query availability must be checked
   later rather than blocking the frame. This distinguishes geometry submission,
   fill rate and simulation before choosing a redesign.
   [Khronos timer-query specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt).
2. **A silk-specific stroke emitter, then a persistent/instanced trail batch**
   (roughly 1 day for shared geometry; 3–5 days for a batch prototype).
   Each span currently repeats its line-length calculation across three layers.
   Sharing that calculation removes six square roots per stored segment, up to
   3,114 at the 519-segment hard bound. A custom batch could also reduce CPU API
   and vertex-generation work. Preserve per-span halo/body/core ordering, the
   current triangle geometry, MSAA, alpha blending, DPI and camera transforms.
   Wider ribbons or a shader replacing three layers require visual approval.
   raylib already batches primitives: thousands of shape API calls do not imply
   thousands of GPU draw calls. Count actual flushes before promising a batching gain.
   [raylib shape implementation](https://github.com/raysan5/raylib/blob/5.5/src/rshapes.c),
   [rlgl batching implementation](https://github.com/raysan5/raylib/blob/5.5/src/rlgl.h).
3. **Spatial indexing for strands and loop edges** (roughly 2–4 days).
   Trial a fixed-capacity uniform grid over active silk, shared by enemy contacts
   and Needlecatch. The hard bounds allow 64 scheduled enemies, up to 128
   projectile slots and 519 trail segments; actual active counts are much lower.
   Both existing contact paths already reject separated AABBs. Grid construction
   may cost more than it saves, as the rejected helper demonstrates. Keep exact
   swept tests, original candidate ordering and Needlecatch chronology.
   Separately, Cocoon's maximum 522-vertex polygon can require up to 135,459
   nonadjacent edge-pair checks per candidate. A conservative edge index could
   reduce rare spikes, but must preserve near-touch and self-intersection rejection.
4. **Crossweave ID indexing** (roughly 1–2 days).
   First record observed consumed/pending counts. Linear membership tests nested
   over history can become quadratic at the caps. A fixed-capacity index or
   ordered merge could reduce work, while retaining stable-ID crossing consumption,
   pending touches and overflow fail-closed behavior. Benefit depends on real counts.
5. **Particle tessellation or analytic sprites** (roughly 1–3 days).
   raylib's circles request 36 sectors; at the 512-particle cap this is 18,432
   sector iterations. Trial a smaller fan or analytic sprite under identical
   blending. Small particles may look unchanged, but altered outlines, halos and
   MSAA edges require image comparisons and approval. The cap is not an observed
   sustained particle count. See the raylib shape source linked above.

A general ECS/structure-of-arrays rewrite, multithreaded simulation, or GPU
collision system is lower priority. Current entity limits are small, and rewriting
state ownership would introduce substantial determinism, ordering and maintenance
risks without measured evidence of a corresponding bottleneck.

## Agent contributions

- **GPT-6 Luna High — collision:** silk/Afterimage investigation, both broad-phase
  candidates, equivalence coverage and standalone benchmark.
- **GPT-6 Luna High — runtime:** timing/profiling review, numeric guards, Cocoon
  snapshot optimization and independent integration review.
- **GPT-6 Luna Max — research:** raylib source research and rendering, spatial,
  polygon and signature-index proposals; no production edits.
- Lead: integration, queue/render/capacity changes, expanded equivalence tests,
  Windows builds, benchmark execution, runtime checks and final review. The three
  subagents were explicitly launched with the model/effort settings listed above.
