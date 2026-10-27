# Presentation controls and bounded diagnostics

This is the approved, GPU-queue-independent functionality transplanted from
`6bb5f74`. The experimental synchronization implementation is not included.

## Player settings

Video settings offer 30, 60, 120, 240 and Unlimited software FPS limits. VSync is
a separate saved toggle, default OFF when absent or invalid in existing settings.
Keyboard and mouse controls both support it. VSync requests display-synchronized
presentation; drivers may override the request, and output can be limited to the
display refresh even with Unlimited selected. With VSync OFF, focused Unlimited
still passes zero to raylib without a hidden finite cap.

Menus use at most 60 FPS (respecting a selected 30 FPS limit), and unfocused or
minimized windows use 30 FPS. The deterministic simulation remains at 120 Hz.
The existing settings file and record fields are preserved by the settings codec.

## Optional diagnostic tools

`--pacing-check <30|60|120|240> --pacing-vsync <on|off>` runs a five-second visible,
silent utility fixture with scripted movement/dashes. It never saves the user's
profile or exports screenshots. Zero/Unlimited and incompatible QA combinations
are rejected. The test cap stays finite even if the diagnostic loses focus.
`--settings-render-check --m7-min-window` is a separate hidden settings screenshot
fixture. Neither mode changes normal gameplay behavior.

The existing opt-in CSV keeps its original 25 columns and appends six independent
presentation fields: configured/effective FPS, requested VSync, monitor refresh,
focus at update and system-QPC frame-start milliseconds. There are no experimental
queue fields. Wall times are not GPU execution or physical input-latency measures.

`tools/check_presentation.ps1` runs finite tests with isolated APPDATA, a
nine-second game watchdog, PID-filtered PresentMon output and unique temporary
ETW sessions. Cleanup addresses only the capture's own session. The default cases
are 60/120 FPS, each with VSync off/on. Input tracking is disabled for automation;
no raw system ETL or keyboard/mouse timing is saved.

PresentMon is an optional external diagnostic, not a build/runtime dependency.
The previously verified official portable 2.6.0 x64 executable has SHA-256
`b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af`.
Get it from the [official release](https://github.com/GameTechDev/PresentMon/releases/tag/v2.6.0)
and place it in `build-windows-msvc/tools/`, or pass its path with `-PresentMon`.

After building the Windows preset:

```powershell
.\tools\check_presentation.ps1 -Limits 60
python .\tools\analyze_presentation.py <capture-directory>\captures.csv
```

The dependency-free Python analyzer expects v2 metrics and uses common system-QPC
bounds, trimming 500 ms of warm-up and 250 ms at the tail. The collector also
supports the alternate `-Metrics v1` format for manual inspection.

## Limits of the earlier investigation

The earlier short tests on a 60 Hz Intel Iris Xe system found healthy app update
intervals but anomalous OpenGL display attribution at 240 FPS without VSync.
Requesting VSync produced reported display durations near 16.7 ms. OpenGL records
were `Other` / `Composed: Copy with GPU GDI`, with reduced PresentMon visibility.
Unavailable display rows were not treated as a measured dropped-frame count.
[Metric definitions](https://github.com/GameTechDev/PresentMon/blob/v2.6.0/README-ConsoleApplication.md).

The exact driver/compositor cause versus ETW attribution error remains unresolved.
No claim is made that true Unlimited responsiveness is fixed, or that physical
input-to-photon latency was measured. No unsafe uncapped test is part of these
integration checks. Experimental research remains on its separate branches.