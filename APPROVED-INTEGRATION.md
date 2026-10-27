# Approved integration — 27 October 2026

Prepared on a temporary integration branch/worktree from verified remote
`develop` at `2fbdbf4f37da89489b648ce6c3d97f87efe8bd79`.

## Selection

- Retains exact ancestor commits `bfc41ee` and `1641331`: verified optimizations,
  correctness fixes, regressions, optional benchmark, 240 FPS, and menu/background
  pacing. Their collision/signature/timing implementations and CMake targets are
  unchanged by the final transplant.
- Transplants the approved parts of `6bb5f74`: saved VSync default OFF, keyboard/
  mouse controls, settings compatibility, presentation CSV fields and independent
  bounded diagnostics. Documentation now describes the integrated functionality.
- Excludes experimental ancestor `84a7652`: no GPU-fence module, runtime switch,
  queue telemetry, prototype test, or extra synchronization linker dependency.
  Experimental branches remain available unchanged for research.
- Preserves software cap choices 30/60/120/240/Unlimited, existing setting/record
  fields, and deterministic 120 Hz simulation. VSync's display-refresh constraint
  is explicit and separately selected; it does not rewrite the software cap.

## Verification

- Windows x64 Release built in the integration worktree with MSVC 19.44.35229,
  pinned toolset 14.44.35207, and the existing MSVC/static-CRT raylib 5.5 package.
  No dependency installation or existing build-directory modification was needed.
- Full applicable CTest: **36/36 passed**. The two experimental-only suites are
  absent. Coverage includes settings migration/persistence, all FPS choices,
  VSync default/round trips, menu focus, auto-pause/background policy, render-rate
  equivalence, campaign, bosses, signatures, rewards, visuals and audio ownership.
- Approved optional collision benchmark built and ran; all query outcomes and
  checksums matched its reference. No new optimization was introduced.
- Four five-second 60/120 FPS × VSync off/on scenarios passed with isolated
  profiles, PID-filtered PresentMon 2.6.0 captures and external watchdogs. Recorded
  targets/VSync state matched the requests and the analyzer accepted the
  queue-independent 31-column app CSV. Captures are local under
  `build-windows-msvc/qa/presentation-734d210816a14812a03b67c015d7bcf4/`.
- The outer test wrapper initially inspected a stale PowerShell native-exit-code
  variable after a successful script. The saved manifests independently confirm
  all four game and capture clients exited 0; no graphics rerun was needed.
- Hidden minimum-size settings checks loaded all five FPS preferences. Legacy
  files omitted VSync and loaded OFF; an ON case also passed. These fixtures stayed
  capped at 60 even while displaying an Unlimited preference. Both ON/OFF screens
  were inspected. The real profile's last-write timestamp predates integration.
- Removed runtime option and invalid/unscoped pacing arguments return code 2
  before window creation. Static source, test, tool and CMake audits found no
  residual experimental synchronization dependencies; an independent Luna High
  review found no integration blocker. Diff/PowerShell/Python checks passed.

## Limits

Unlimited responsiveness is **not claimed fixed**. No unsafe uncapped run,
physical input-latency measurement, listening test, or human campaign playthrough
was performed. OpenGL PresentMon attribution limitations remain documented in
`PRESENTATION-PACING.md`. Existing unrelated builds and files were left in place;
`main` is outside this integration.
