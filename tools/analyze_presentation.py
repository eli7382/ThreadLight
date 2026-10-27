#!/usr/bin/env python3
"""Summarize a PresentMon + ThreadLight pacing capture without dependencies.

The utility fixture writes a system QPC timestamp in both CSVs, allowing the
same PresentMon-defined warm-up/tail window to be selected in both sources.
No columns are treated as proof of optical or physical input latency.

Metric references:
  https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md
  https://github.com/GameTechDev/PresentMon#analyzing-opengl-and-vulkan-applications

PresentMon documents DisplayedTime as frame display duration (NA when no
display duration was observed). Its README also warns that OpenGL/Vulkan
applications reported as runtime "Other" have reduced presentation
instrumentation: CPUFramePacingStall is always zero and CPUFrameTime-derived
GPU/display latency metrics can be less accurate. This report therefore keeps
missing/invalid display durations distinct and labels DisplayLatency-derived
timestamps as estimates. Input tracking is explicitly disabled by the capture
harness, and none of these data are physical click-to-photon measurements.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from pathlib import Path
from typing import Iterable, Optional


WARMUP_MS = 500.0
TAIL_MS = 250.0


def number(value: object) -> Optional[float]:
    """Parse finite CSV numbers; blanks, PresentMon NA and infinities are absent."""
    if value is None:
        return None
    text = str(value).strip()
    if not text or text.upper() in {"NA", "N/A", "NAN", "NULL", "NONE"}:
        return None
    try:
        parsed = float(text)
    except (TypeError, ValueError):
        return None
    return parsed if math.isfinite(parsed) else None


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames:
            raise ValueError(f"CSV has no header: {path}")
        return list(reader)


def percentile(values: Iterable[float], fraction: float) -> Optional[float]:
    ordered = sorted(value for value in values if math.isfinite(value))
    if not ordered:
        return None
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def distribution(values: Iterable[float]) -> dict[str, Optional[float] | int]:
    finite = [value for value in values if math.isfinite(value)]
    return {
        "count": len(finite),
        "mean": statistics.fmean(finite) if finite else None,
        "p50": percentile(finite, 0.50),
        "p95": percentile(finite, 0.95),
        "p99": percentile(finite, 0.99),
        "max": max(finite) if finite else None,
    }


def need(row: dict[str, str], key: str, path: Path) -> float:
    value = number(row.get(key))
    if value is None:
        raise ValueError(f"Missing or invalid required {key!r} in {path}")
    return value


def summarize_case(case: dict[str, str], manifest_path: Path) -> dict[str, object]:
    app_path = Path(case["AppCsv"])
    pm_path = Path(case["PresentMonCsv"])
    if not app_path.is_absolute():
        app_path = (manifest_path.parent / app_path).resolve()
    if not pm_path.is_absolute():
        pm_path = (manifest_path.parent / pm_path).resolve()
    if not app_path.is_file() or not pm_path.is_file():
        raise FileNotFoundError(f"Capture file missing for {case.get('Case', '<unnamed>')}")

    app_all = read_csv(app_path)
    pm_all = read_csv(pm_path)
    app_qpc_key = "frame_start_qpc_ms"
    pm_qpc_key = "CPUStartQPCTime"
    if not app_all or not pm_all:
        raise ValueError(f"Capture has no data rows for {case.get('Case', '<unnamed>')}")
    if app_qpc_key not in app_all[0] or pm_qpc_key not in pm_all[0]:
        raise ValueError(
            f"Missing common QPC time columns ({app_qpc_key}, {pm_qpc_key}) "
            f"for {case.get('Case', '<unnamed>')}"
        )

    pm_times = [number(row.get(pm_qpc_key)) for row in pm_all]
    pm_times = [value for value in pm_times if value is not None]
    if len(pm_times) < 3:
        raise ValueError(f"Too few valid PresentMon QPC timestamps for {case.get('Case')}")
    pm_start = min(pm_times)
    pm_end = max(pm_times)
    window_start = pm_start + WARMUP_MS
    window_end = pm_end - TAIL_MS
    if window_end <= window_start:
        raise ValueError(f"Capture is too short after warm-up/tail trim for {case.get('Case')}")

    def inside(row: dict[str, str], key: str) -> bool:
        qpc = number(row.get(key))
        return qpc is not None and window_start <= qpc <= window_end

    pm = [row for row in pm_all if inside(row, pm_qpc_key)]
    app = [row for row in app_all if inside(row, app_qpc_key)]
    if len(pm) < 3 or len(app) < 3:
        raise ValueError(
            f"Too few rows in shared trimmed window for {case.get('Case')}: "
            f"app={len(app)}, PresentMon={len(pm)}"
        )

    app_intervals = [number(row.get("interval_ms")) for row in app]
    app_intervals = [value for value in app_intervals if value is not None and value > 0]
    discarded = [number(row.get("discarded_ms")) for row in app]
    discarded = [value for value in discarded if value is not None]
    steps = [number(row.get("steps")) for row in app]
    steps = [value for value in steps if value is not None]

    # PresentMon's DisplayedTime is a duration. Keep its unavailable and
    # non-positive cases explicit; do not relabel them as dropped frames.
    display_positive: list[float] = []
    display_missing = 0
    display_nonpositive = 0
    gpu_busy: list[float] = []
    display_starts: list[float] = []
    runtime_values: set[str] = set()
    mode_values: set[str] = set()
    sync_values: set[str] = set()
    for row in pm:
        runtime_values.add((row.get("PresentRuntime") or "").strip())
        mode_values.add((row.get("PresentMode") or "").strip())
        sync_values.add((row.get("SyncInterval") or "").strip())
        duration = number(row.get("DisplayedTime"))
        if duration is None:
            display_missing += 1
        elif duration > 0:
            display_positive.append(duration)
        else:
            display_nonpositive += 1

        busy = number(row.get("GPUBusy"))
        if busy is not None and busy >= 0:
            gpu_busy.append(busy)

        qpc = number(row.get(pm_qpc_key))
        latency = number(row.get("DisplayLatency"))
        if qpc is not None and latency is not None and latency >= 0:
            estimated_start = qpc + latency
            if window_start <= estimated_start <= window_end:
                display_starts.append(estimated_start)

    display_start_intervals = [
        later - earlier
        for earlier, later in zip(display_starts, display_starts[1:])
        if later > earlier
    ]
    window_seconds = (window_end - window_start) / 1000.0
    app_discarded_positive = [value for value in discarded if value > 0]

    return {
        "case": case.get("Case", ""),
        "frame_rate_limit": int(case["FrameRateLimit"]),
        "vsync": case["Vsync"].lower(),
        "game_pid": int(case["GameProcessId"]),
        "game_exit_code": int(case["GameExitCode"]),
        "presentmon_exit_code": int(case["PresentMonExitCode"]),
        "presentmon_metadata": {
            "runtime_values": sorted(runtime_values),
            "present_mode_values": sorted(mode_values),
            "sync_interval_values": sorted(sync_values),
            "input_tracking": "disabled by capture (--no_track_input); no input-latency result",
            "opengl_note": (
                "Runtime Other has reduced presentation instrumentation; "
                "DisplayLatency and GPU metrics below are observational estimates, "
                "not physical input/display measurements."
            ),
        },
        "window": {
            "rule": "PresentMon QPC first + 500 ms through last - 250 ms; same system QPC bounds for app CSV",
            "presentmon_first_qpc_ms": pm_start,
            "presentmon_last_qpc_ms": pm_end,
            "included_start_qpc_ms": window_start,
            "included_end_qpc_ms": window_end,
            "duration_s": window_seconds,
        },
        "application": {
            "rows": len(app),
            "present_submissions_per_s": len(app) / window_seconds,
            "interval_ms": distribution(app_intervals),
            "simulation_steps_sum": sum(steps),
            "discarded_simulation_ms_sum": sum(discarded),
            "rows_with_positive_discarded_ms": len(app_discarded_positive),
        },
        "presentmon": {
            "present_rows": len(pm),
            "present_submissions_per_s": len(pm) / window_seconds,
            "displayed_time_positive_rows": len(display_positive),
            "displayed_time_missing_na_or_nonfinite_rows": display_missing,
            "displayed_time_nonpositive_rows": display_nonpositive,
            "displayed_time_status_note": (
                "Positive means a finite positive DisplayedTime duration was recorded. "
                "Missing/invalid means unavailable or unresolved; these counts are not labeled dropped frames."
            ),
            "displayed_time_ms": distribution(display_positive),
            "displayed_time_at_least_25ms_count": sum(value >= 25.0 for value in display_positive),
            "gpu_busy_ms_estimate": distribution(gpu_busy),
            "display_start_interval_ms_estimate": distribution(display_start_intervals),
            "display_start_interval_note": (
                "Adjacent positive intervals from CPUStartQPCTime + DisplayLatency where both values were finite. "
                "This is an estimated display-start timestamp derived from a metric with reduced accuracy for Runtime Other; "
                "it is distinct from DisplayedTime, which is a duration."
            ),
        },
        "source_files": {"app_csv": str(app_path), "presentmon_csv": str(pm_path)},
    }


def flat_summary(case: dict[str, object]) -> dict[str, object]:
    app = case["application"]
    pm = case["presentmon"]
    window = case["window"]
    assert isinstance(app, dict) and isinstance(pm, dict) and isinstance(window, dict)
    app_interval = app["interval_ms"]
    displayed = pm["displayed_time_ms"]
    gpu = pm["gpu_busy_ms_estimate"]
    display_interval = pm["display_start_interval_ms_estimate"]
    assert isinstance(app_interval, dict) and isinstance(displayed, dict)
    assert isinstance(gpu, dict) and isinstance(display_interval, dict)
    return {
        "Case": case["case"],
        "FrameRateLimit": case["frame_rate_limit"],
        "Vsync": case["vsync"],
        "WindowSeconds": window["duration_s"],
        "AppRows": app["rows"],
        "AppSubmissionsPerSecond": app["present_submissions_per_s"],
        "AppIntervalP50Ms": app_interval["p50"],
        "AppIntervalP95Ms": app_interval["p95"],
        "AppIntervalP99Ms": app_interval["p99"],
        "AppIntervalMaxMs": app_interval["max"],
        "SimulationStepsSum": app["simulation_steps_sum"],
        "DiscardedSimulationMsSum": app["discarded_simulation_ms_sum"],
        "RowsWithPositiveDiscardedMs": app["rows_with_positive_discarded_ms"],
        "PresentMonRows": pm["present_rows"],
        "PresentMonSubmissionsPerSecond": pm["present_submissions_per_s"],
        "DisplayedTimePositiveRows": pm["displayed_time_positive_rows"],
        "DisplayedTimeMissingOrInvalidRows": pm["displayed_time_missing_na_or_nonfinite_rows"],
        "DisplayedTimeNonpositiveRows": pm["displayed_time_nonpositive_rows"],
        "DisplayedTimeMeanMs": displayed["mean"],
        "DisplayedTimeP50Ms": displayed["p50"],
        "DisplayedTimeP95Ms": displayed["p95"],
        "DisplayedTimeP99Ms": displayed["p99"],
        "DisplayedTimeMaxMs": displayed["max"],
        "DisplayedTimeAtLeast25MsCount": pm["displayed_time_at_least_25ms_count"],
        "GpuBusyMeanMsEstimate": gpu["mean"],
        "GpuBusyP50MsEstimate": gpu["p50"],
        "GpuBusyP95MsEstimate": gpu["p95"],
        "GpuBusyP99MsEstimate": gpu["p99"],
        "GpuBusyMaxMsEstimate": gpu["max"],
        "DisplayStartIntervalP50MsEstimate": display_interval["p50"],
        "DisplayStartIntervalP95MsEstimate": display_interval["p95"],
        "DisplayStartIntervalP99MsEstimate": display_interval["p99"],
        "DisplayStartIntervalMaxMsEstimate": display_interval["max"],
        "Runtime": "; ".join(case["presentmon_metadata"]["runtime_values"]),
        "PresentMode": "; ".join(case["presentmon_metadata"]["present_mode_values"]),
        "SyncInterval": "; ".join(case["presentmon_metadata"]["sync_interval_values"]),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path, help="captures.csv generated by check_presentation.ps1")
    args = parser.parse_args()
    manifest = args.manifest.resolve()
    rows = read_csv(manifest)
    if not rows:
        raise SystemExit(f"No captures listed in {manifest}")

    summaries = [summarize_case(row, manifest) for row in rows]
    flat_rows = [flat_summary(row) for row in summaries]
    json_path = manifest.parent / "analysis.json"
    csv_path = manifest.parent / "analysis.csv"
    json_path.write_text(json.dumps(summaries, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(flat_rows[0]))
        writer.writeheader()
        writer.writerows(flat_rows)

    print("Case              App interval p50/p95/p99 (ms) | PM DisplayedTime mean/p95 (ms) | display time positive/NA | GPU Busy p50/p95 (ms est.)")
    for item in summaries:
        app = item["application"]
        pm = item["presentmon"]
        interval = app["interval_ms"]
        duration = pm["displayed_time_ms"]
        gpu = pm["gpu_busy_ms_estimate"]
        print(
            f"{item['case']:<18} "
            f"{interval['p50']!s}/{interval['p95']!s}/{interval['p99']!s:<14} | "
            f"{duration['mean']!s}/{duration['p95']!s:<18} | "
            f"{pm['displayed_time_positive_rows']}/{pm['displayed_time_missing_na_or_nonfinite_rows']:<22} | "
            f"{gpu['p50']!s}/{gpu['p95']!s}"
        )
    print("DisplayedTime NA/invalid counts are unresolved/unavailable rows, not dropped-frame counts.")
    print("DisplayLatency-derived display intervals and GPU Busy are observational estimates; no physical input-latency result is present.")
    print(f"Analysis: {json_path}\nSummary: {csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
