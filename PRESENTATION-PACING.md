# Synchronized-presentation investigation — 27 October 2026

Isolated on `codex/presentation-pacing-20261027`, based on GPU prototype `84a7652`.
Earlier play-test builds and branches, `develop`, and `main` were not modified.

## What this investigation establishes

Windows and the game report a 60 Hz Intel Iris Xe display at 1920×1080. Capped
scripted tests show a repeatable mismatch at 240 FPS with VSync off: the game
continues updating/submitting frames roughly every 4.2 ms, while PresentMon's
display attribution becomes sparse and reports very long display durations.
The pattern also appears with the two-fence GPU queue enabled and in the
alternate PresentMon metric format. With VSync requested, reported presentation
durations remain close to 16.7 ms and submissions fall to roughly 60 per second.

This is useful evidence of a presentation-path/tracking problem, consistent with
the owner's poor responsiveness report. It is **not proof of an Intel driver bug
or a physical half-second screen freeze**. The current traces cannot fully
separate compositor/driver behavior from OpenGL ETW attribution error. No test
here establishes a fix for true uncapped foreground gameplay or measures physical
input-to-photon latency.

## Implemented

- Added a persistent **VSync** toggle to Video settings, usable with keyboard or
  mouse. It is separate from the 30/60/120/240/Unlimited software cap. Existing
  profiles and missing/invalid VSync keys default to off. Live changes request
  raylib's swap interval without changing the selected software FPS preference.
- Adjusted keyboard focus traversal and verified the minimum-size settings
  layout. The new control explicitly explains its effect on Unlimited.
- Added `--pacing-check <30|60|120|240> --pacing-vsync <on|off>`: a visible,
  silent, five-second utility fixture with scripted movement/dashes, no screenshot
  export and no profile writes. It rejects zero/Unlimited and incompatible QA
  options. Its finite cap stays enforced throughout the test.
- Added a separate hidden `--settings-render-check` fixture for the settings UI.
- Extended optional CSV capture with configured/effective FPS, requested VSync,
  monitor refresh, focus state and system QPC frame-start timestamps. PresentMon
  and app captures can therefore use the same time window. Normal gameplay
  retains the 120 Hz simulation; the synthetic controls apply only in the explicit
  pacing fixture.
- Added a PID-filtered PresentMon collection harness and a standard-library-only
  Python analysis script. Neither tool is a game runtime dependency.

VSync is requested through raylib 5.5's supported window-state flag. Its GLFW
backend applies swap interval 1 for on and 0 for off; driver policy can override
requests. The logged flag is therefore named `vsync_requested`, not "confirmed".
[raylib backend source](https://github.com/raysan5/raylib/blob/5.5/src/platforms/rcore_desktop_glfw.c),
[GLFW buffer swapping](https://www.glfw.org/docs/3.3/window_guide.html#buffer_swap).

## Method and safety

Used the official portable **PresentMon 2.6.0 x64** console tool, downloaded from
its GitHub release and checked against the release API's SHA-256:
`b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af`.
No MSI, service installation, driver change or global graphics setting was used.
[Official release](https://github.com/GameTechDev/PresentMon/releases/tag/v2.6.0).

Each game exits after five seconds and has an external nine-second watchdog.
Settings live in a fresh per-case APPDATA directory. PresentMon targets that
specific child PID, uses a unique temporary ETW session, and terminates after
the capture/child exit. Cleanup addresses only its own session. Output contains
game-filtered CSV records; no raw system ETL is saved. Automated captures disable
input tracking to avoid collecting unrelated keyboard/mouse timing.

Initial cases were 60/120 FPS with VSync off/on. After those passed, 240 FPS
off/on was captured, then repeated with the GPU queue, then cross-checked using
the legacy metric schema. **No Unlimited run was performed.** All ten game/tool
cases exited successfully; the tests observed the selected finite caps and no
discarded simulation time in the analyzed windows. All recorded focus samples
were focused in the initial four cases and the 240/off check.

The v2 analysis trims the first 500 ms and last 250 ms of PresentMon QPC times,
then selects the same system-QPC interval in the app CSV. Percentiles use linear
interpolation. Short, single runs are exploratory evidence, not long-session
performance guarantees. Raw files, manifests, JSON and CSV summaries are retained
under `build-windows-msvc/qa/` locally.

## Results

The following are **app timing and PresentMon-reported durations**, not optical
measurements. `NA` display rows are kept unresolved; they are not converted into
a measured dropped-frame count.

| Software cap / VSync | App interval median / p99, ms | Reported display-duration median / p95, ms | Positive / unresolved display rows |
| --- | ---: | ---: | ---: |
| 60 / Off | 16.667 / 17.131 | 16.661 / 16.770 | 252 / 0 |
| 60 / On | 16.669 / 17.073 | 16.662 / 16.773 | 253 / 0 |
| 120 / Off | 8.345 / 9.037 | 16.66 / 16.776 | 254 / 246 |
| 120 / On | 16.638 / 17.839 | 16.66 / 16.758 | 254 / 0 |
| 240 / Off | 4.180 / 5.052 | **549.879 / 882.115 — anomalous** | **8 / 959** |
| 240 / On | 16.670 / 17.764 | 16.660 / 16.745 | 254 / 0 |

At 120/off, approximately half the submissions lack display duration, which is
compatible with 120 submissions feeding a 60 Hz display. The 240/off pattern is
not the ordinary one-in-four result: attributed display durations progressively
grow into hundreds of milliseconds. It needs independent corroboration before
treating those values as real held frames.

With the GPU queue enabled at 240/off, the repeat still had only 8 positive
display-duration rows and 935 unresolved rows. Reported mean/p95 duration was
549.81/831.39 ms, while the app interval median was 4.320 ms. The queue stayed at
two records, had zero errors/timeouts, and its maximum measured wait was only
0.103 ms. Thus that prototype did **not** eliminate this recorded phenomenon.
The paired VSync-on run again reported normal approximately 16.66 ms durations.

The v1-schema cross-check also reported sparse display changes at 240/off: six
positive display-change intervals in the trimmed window, mean 647.03 ms, maximum
1032.97 ms. Its VSync-on counterpart had 253 changes averaging 16.661 ms, maximum
17.318 ms. Both schemas share ETW/event analysis, so this rules out a v2 CSV-only
formatting issue, not a shared attribution problem.

All records used `PresentRuntime=Other`, `SyncInterval=-1`, and
`PresentMode=Composed: Copy with GPU GDI`. OpenGL's reduced PresentMon
instrumentation makes some CPU-derived GPU/display latency fields less reliable.
The copy-mode label does not mean the game uses software rendering. PresentMon
also emitted a process-query privilege warning; the expected same-account PIDs
and application names were nevertheless populated in these successful captures.
[Official OpenGL caveats](https://github.com/GameTechDev/PresentMon#analyzing-opengl-and-vulkan-applications),
[metric definitions](https://github.com/GameTechDev/PresentMon/blob/v2.6.0/README-ConsoleApplication.md).

Capture directories:

- `presentation-2cd712c6e614426f94d058e33072b6fd`: 60/120, v2.
- `presentation-32514d88bb3a4339bfe1dc37ed033a60`: 240, v2.
- `presentation-1a4d122cf33a40ad9ce03c723282c85b`: 240 with queue, v2.
- `presentation-47ff3f4fbb02468e8f7826ae317d8186`: 240, v1 cross-check.

## Engineering interpretation

The submitted-FPS number alone is unsuitable for judging this problem. The app
simulation and render loop kept up while display attribution diverged. Queue
limiting did not reproduce a large GPU command wait or resolve that divergence.
This makes presentation/composition behavior a more useful next target than
another rewrite of input buffering or the 120 Hz simulation.

VSync is a practical, explicit play-test option on this 60 Hz setup: it produced
the expected stable display durations and avoided the high-rate anomaly in the
capped tests. The 60/off baseline was already evenly paced in this short test;
there is no evidence here that VSync improves every 60 FPS run. VSync can trade
throughput/latency for synchronized delivery, and may cap actual output near
refresh even with Unlimited selected. True unsynchronized Unlimited remains
available when VSync is off.

The original desktop-wide slowdown could still involve resource contention,
driver behavior or composition scheduling. This investigation narrows the
problem but does not establish the exact mechanism. Direct screen capture or
external high-speed observation, supported presentation counters and a deliberate
input test would distinguish real stale display output from ETW misattribution.
No click-to-photon, optical latency or subjective smoothness improvement is claimed.

## Verification and reproduction

- Windows x64 Release build: passed; **38/38 CTest suites passed**.
- New persistence tests cover both VSync values, legacy omission, invalid values,
  and preservation of existing settings. Menu focus and CSV schema tests pass.
- Diagnostic CLI rejects zero FPS, unscoped VSync overrides and conflicting QA
  modes before opening a window.
- Settings were rendered at minimum window size and inspected; all five FPS
  choices and the VSync control fit. A help-text/border overlap found there was
  corrected.
- Existing gameplay, balance, audio, controls and deterministic simulation remain
  unchanged in normal launches. No manual campaign or listening test was performed.

Build using the Windows preset. Place the verified portable PresentMon EXE in
`build-windows-msvc/tools/`; it is not committed with the source. Then:

```powershell
.\tools\check_presentation.ps1                  # 60/120, off/on
.\tools\check_presentation.ps1 -Limits 240      # finite 240, off/on
.\tools\check_presentation.ps1 -Limits 240 -GpuQueue
.\tools\check_presentation.ps1 -Limits 240 -Metrics v1
python .\tools\analyze_presentation.py <v2-capture-directory>\captures.csv
```

The Python analyzer expects v2 columns. The v1 cross-check uses
`TimeInSeconds` to trim 0.5 s / 0.25 s and summarizes positive
`msBetweenDisplayChange`; the raw legacy rows remain available for inspection.

For manual play-testing, use this branch's executable and enable
**Settings → Video → VSync**. Start at 60 FPS on the verified 60 Hz display.
No CLI switch is required for the VSync option. User profile settings were not
changed by the automated tests.

Luna High added settings persistence/focus coverage and independently reviewed
the main integration. A separate Luna High researched PresentMon, wrote the
collection/analysis scripts and examined the OpenGL limitations. The lead
implemented runtime/UI/QA/clock integration, verified the tool hash, built and
ran the capped tests, inspected the settings image and reviewed the findings.
