# Opt-in GPU queue prototype

Approved follow-up to `1641331`, isolated on
`codex/gpu-queue-prototype-20261027`. The previous play-test build and feature
branch remain unchanged. No merge into `develop` or `main` was performed.

## Behavior

The new `--gpu-queue` switch enables the prototype only during focused gameplay,
or during explicitly hidden render-check fixtures for bounded verification.
Normal launches retain the existing renderer. The separate 240 FPS option and
true Unlimited remain available; this prototype does not introduce a hidden FPS
ceiling or change the deterministic 120 Hz simulation.

At the start of a frame, a fixed two-slot FIFO waits on its oldest fence only
when both slots are occupied. It makes one `glClientWaitSync` call with a 5 ms
API timeout, without a polling loop. After `EndDrawing` has submitted raylib's
deferred batch, swapped and polled input, the adapter inserts `glFenceSync` and
flushes. A fence covers the preceding GL command stream, not verified display
presentation. Queue shutdown and all fence deletion occur before `CloseWindow`.

The Windows adapter loads the three synchronization functions through WGL only
after context creation. It checks the originating context and thread before GL
calls and rejects invalid loader addresses. Other platforms currently report
the opt-in adapter as unavailable. No GL synchronization calls occur when the
switch is absent. Backend initialization allocates once; the FIFO does not
allocate per frame.

A null fence, wait failure, timeout, or FIFO protocol violation latches an error
and clears owned handles. Main stops the run and returns code 5 instead of
continuing unbounded. Unsupported initialization returns code 4. Neither path
silently changes Unlimited to another setting. A 5 ms API timeout is not a hard
real-time guarantee against OS scheduling delays or a malfunctioning driver.

References: [Khronos synchronization semantics](https://wikis.khronos.org/opengl/Sync_Object),
[Microsoft WGL loading](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress).

## Diagnostics

The existing optional `--perf-log <new.csv>` capture appends three columns:
`gpu_queue_active`, `gpu_queue_depth`, and `gpu_queue_wait_wall_ms`. Existing
columns retain their order. The update wall-time region includes the start-of-
frame queue check; the separate wait column identifies that subset. The present
region ends immediately after `EndDrawing` and excludes subsequent fence work.
Frame-start intervals include the full loop. Without the switch, all three new
columns are zero.

An opt-in shutdown summary reports fence submissions, waits, maximum depth,
timeouts/errors and total/maximum wait wall time. A begin-frame failure is
reported in that summary even though the incomplete frame has no CSV row.
These measurements do not establish GPU utilization, compositor queue depth,
or input-to-display latency.

## Validation performed

Windows x64 Release built with MSVC 19.44.35229 and the pinned raylib 5.5 package.
Full CTest: **38/38 passed**. New tests cover FIFO ordering/depth, exact timeout
arguments, null-fence creation, timeout/wait failure cleanup, failure latching,
enable/disable transitions, idempotent shutdown, disabled zero-call behavior,
protocol overflow and missing-context adapter initialization. Existing campaign,
boss, signature, reward, input, timing, settings and audio suites passed.

The reusable `tools/check_gpu_queue.ps1` runs utility and Briar Heart fixtures
with the queue off and on. Every run is hidden, silent, fixed at 60 FPS and uses
isolated settings. A ten-second external watchdog terminates its specific child
process if it fails to exit, and aborts further fixtures. It checks successful
exit, active queue rows, the two-slot limit, zero queue failures, and absence of
an uncapped target. No unrestricted foreground reproduction was run.

Measured on Intel Iris Xe / OpenGL 3.3:

| Fixture | Queue | Frames | Active rows | Peak records | Mean wait ms | Maximum wait ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Utility | Off | 120 | 0 | 0 | 0 | 0 |
| Utility | On | 121 | 121 | 2 | 0.02696 | 0.3714 |
| Briar Heart | Off | 145 | 0 | 0 | 0 | 0 |
| Briar Heart | On | 142 | 142 | 2 | 0.02608 | 0.1580 |

Both queue runs reported zero timeouts/errors. Their capped interval p50 upper
bound was 16.7 ms. Utility had no discarded simulation time. Heart discarded
54.02 ms without the queue and 63.56 ms with it; these fixtures synchronously
read/export screenshots and are not valid gameplay frame-pacing benchmarks.
The tiny waits at 60 FPS mainly demonstrate the working synchronization path,
not a latency or throughput improvement under saturation.

Captures, logs and the machine-readable summary are under
`build-windows-msvc/qa/gpu-queue-a50a26b8fa2341d8bf42e9495b2f57fa/` locally.
The script creates a fresh output directory on each invocation.

## Running the prototype

From this worktree after configuring/building the Windows preset:

```powershell
# Bounded, capped verification; no manual input needed:
.\tools\check_gpu_queue.ps1

# Opt-in interactive launch (begins with the saved gameplay FPS preference):
.\build-windows-msvc\Release\Threadlight.exe --gpu-queue --perf-log .\new-queue-run.csv
```

Begin interactive evaluation capped. Foreground Unlimited was not re-tested and
is not established as safe by these results. The normal executable without the
flag has the prototype disabled. The owner's saved gameplay cap remains 120 FPS;
the automated fixtures do not modify their profile.

## Limits and next evidence

This is a validated capped prototype, **not a confirmed fix** for the original
uncapped desktop slowdown. A GL completion fence cannot prove that DWM has
displayed a frame, cannot guarantee full presentation queue control, and does not
reserve GPU headroom for other applications. A wait can delay processing events
that arrive after raylib's previous input poll. The strict timeout intentionally
stops rather than tolerating sustained backlog; it may reject otherwise functional
slow hardware. Cross-vendor behavior and context-loss runtime paths remain open.

Next evidence needed is input-to-display/frame-presentation timing, GPU queue and
utilization data, display refresh information, and a short separately supervised
interactive comparison. No claim is made that the reported 120 FPS unevenness
has been resolved. No VSync or driver settings were changed.

Luna High implemented the pure queue and failure tests. A separate Luna High
reviewed Windows API signatures, frame placement, fail-closed handling and
cleanup. The lead integrated the adapter/CLI/CSV, built Release, executed capped
watchdog checks and the full regression suite, and reviewed the results.
