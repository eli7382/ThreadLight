# Unlimited FPS responsiveness investigation

Follow-up to optimization commit `bfc41ee`, 27 October 2026.

## Observed problem and recovery

The owner reported extremely slow responses throughout the game with Unlimited,
and later across the whole Windows desktop. Selecting 30/60/120 FPS immediately
restored responsiveness. The owner also reports that 120 FPS is not fully smooth.
This points beyond simulation input handling to resource contention and/or frame
presentation, but does not identify the responsible CPU, GPU or driver component.

An uncapped reproduction instance added load during investigation. All ThreadLight
processes were then stopped and the saved Unlimited setting was reset to 120 FPS
for safe reopening. Other settings and run records were preserved. No further
uncapped gameplay tests were run. The owner explicitly requested that true
Unlimited remain available and that 240 FPS be added separately.

## Evidence

The first 5,999 measured title-screen intervals in the reproduction CSV averaged:

| Metric | Mean |
| --- | ---: |
| Frame-start interval | 2.764 ms |
| Draw-submission region | 0.615 ms |
| EndDrawing/presentation region | 2.003 ms |
| CPU-loop frame rate | 361.74 FPS |

These wall-time regions are not CPU utilization or GPU execution measurements.
The capture used the existing opt-in recorder, which itself adds overhead.
Hundreds of submitted frames per second do not prove that the display shows
fresh frames with low latency. No GPU-utilization/queue-depth trace was collected
before stopping the process, and contention cannot yet be separated from a driver
problem. The rendering backend was Intel Iris Xe / OpenGL 3.3.

Code review confirmed that the saved value zero reaches `SetTargetFPS(0)`.
raylib 5.5's `EndDrawing` submits its batch, swaps, optionally waits for a cap,
then polls input every frame. There is no branch disabling event polling in
Unlimited mode. The game's 120 Hz simulation accumulator and press buffers are
independent of the selected rendering cap. Those findings do not support an
Unlimited-specific input-polling disable as the cause.
[raylib frame-control implementation](https://github.com/raysan5/raylib/blob/5.5/src/rcore.c).

## Implemented low-risk changes

- Added a separate **240 FPS** choice, including keyboard cycling, mouse chips,
  validation and persistence. **Unlimited still stores zero**, and focused gameplay
  passes zero to raylib; it is not silently converted to a fixed cap.
- Focused menus use at most 60 FPS, respecting a selected 30 FPS cap. Unfocused
  and minimized windows use 30 FPS. Hidden regression fixtures retain 60 FPS.
- Centralized effective pacing immediately before presentation. The target changes
  only when necessary, and Windows timer resolution follows the effective cap.
  Saving audio/settings no longer reapplies a raw FPS preference over this policy.
- Startup stays at 60 FPS while on the title screen, even when Unlimited is saved.

This prevents unnecessary menu/background load. It does **not** establish that
Unlimited foreground gameplay is now safe or responsive. No gameplay, balance,
movement, dash or simulation-rate change was made.

## Verification

- Windows x64 Release build succeeded; full CTest **36/36 passed**.
- Added policy coverage for foreground/menu/background/minimized/hidden fixtures,
  and 240 FPS persistence/cycling while preserving zero as Unlimited.
- Two isolated five-second title probes, saved at 240 and Unlimited respectively,
  both logged only a 16.667 ms render target and approximately 16.59 ms mean frame
  intervals. Their settings/records were isolated from the owner's profile. Each
  test process had an explicit five-second stop; neither entered uncapped play.
- Real 240 FPS gameplay, new menu-chip visual interaction, and foreground
  Unlimited input responsiveness remain unverified. UI automation could not run
  because its app-approval request timed out. No OS input-injection workaround
  was used.
- Luna High reviewed timing/input and researched GPU synchronization, then added
  the pure pacing helper/tests and independently reviewed integration.

## Historical research proposal — excluded from this integration

This proposal was later prototyped on `codex/gpu-queue-prototype-20261027` and
researched on `codex/presentation-pacing-20261027`. Those branches remain intact;
the experimental implementation, dependencies and tests are excluded from
`develop`. The following records the original research design:

1. Flush raylib's deferred geometry before inserting a frame-completion fence.
2. Keep a ring of at most two outstanding frame fences. Wait for the oldest
   command prefix to finish before allowing additional submission. Use finite
   waits, with explicit timeout/error handling and context-thread cleanup.
3. Load supported OpenGL synchronization entry points after context creation on
   Windows. Fall back cleanly if unavailable; never spin indefinitely or silently
   report success after an error.
4. Record bounded diagnostics for GPU wait time, frame intervals, late simulation
   steps and input/presentation latency. Compare current capped modes and this
   prototype under a short watchdog-controlled test, initially at capped rates.
   Do not run another unrestricted reproduction on the owner's desktop.

Estimated implementation/validation effort: roughly 1–2 engineering days; hardware
coverage may require more. Expected benefit is less GL command backlog and CPU
run-ahead, potentially improving latency without a fixed FPS ceiling. The size of
that benefit is unknown because queue depth has not been measured.

Risks: a GL fence signals command completion, not DWM/display presentation. It
therefore cannot promise to bound the entire compositor queue or reserve GPU
capacity for other apps. Waiting can delay input polling or reduce CPU/GPU overlap.
Driver behavior, context ownership and cleanup need targeted tests. An unconditional
`glFinish` or tight `SwitchToThread` poll is not a suitable production shortcut.
[Khronos synchronization semantics](https://wikis.khronos.org/opengl/Sync_Object),
[Microsoft GL entry-point loading](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress),
[Microsoft scheduler-yield limitations](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-switchtothread).

Investigate the reported 120 FPS smoothness separately with actual frame-interval
percentiles, display refresh information and presentation timing. An unsynchronized
120 FPS submission rate is not proof of even on-screen pacing. A separately chosen
VSync option is a possible experiment, but it changes presentation behavior and
must not masquerade as Unlimited. No VSync behavior was changed in this patch.

The approved VSync controls and independent diagnostics are documented in
[PRESENTATION-PACING.md](PRESENTATION-PACING.md). Neither those controls nor the
excluded experiment establish a fix for true Unlimited responsiveness.
