param(
    [Parameter(Mandatory)][ValidateSet(28, 34, 35)][int]$Wave,
    [Parameter(Mandatory)][ValidateSet(60, 120)][int]$Fps,
    [Parameter(Mandatory)][string]$Tag,
    [switch]$DisablePerfLog,
    [switch]$DisableGpuTimer
)

$root = $PSScriptRoot
$exe = Join-Path (Split-Path $root -Parent) 'build-phase34-m7-msvc-x64\Release\Threadlight.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Executable not found: $exe" }
$env:APPDATA = Join-Path $root "isolated-appdata-$Fps"
$kind = switch ($Wave) { 28 { 'hollow' } 34 { 'circuit' } 35 { 'heart' } }
$csv = Join-Path $root "$Tag.csv"
$stdout = Join-Path $root "$Tag.stdout.log"
$stderr = Join-Path $root "$Tag.stderr.log"
$arguments = "--late-game-pacing-check $Wave $Fps --pacing-vsync off"
if (-not $DisablePerfLog) {
    if (Test-Path -LiteralPath $csv) { throw "Refusing to overwrite capture: $csv" }
    $arguments += " --perf-log $csv"
}
if ($DisableGpuTimer -and -not $DisablePerfLog) { $arguments += ' --perf-no-gpu-timer' }

$watch = [System.Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath $exe -ArgumentList $arguments `
    -WorkingDirectory (Split-Path $exe) -PassThru `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$peakWorkingSet = 0L
$peakPrivateBytes = 0L
$cpuSeconds = 0.0
while (-not $process.WaitForExit(500)) {
    try {
        $sample = Get-Process -Id $process.Id -ErrorAction Stop
        $peakWorkingSet = [Math]::Max($peakWorkingSet, [long]$sample.WorkingSet64)
        $peakPrivateBytes = [Math]::Max($peakPrivateBytes, [long]$sample.PrivateMemorySize64)
        $cpuSeconds = [double]$sample.CPU
    } catch { }
}
$watch.Stop()
$process.Refresh()
try {
    $peakWorkingSet = [Math]::Max($peakWorkingSet, [long]$process.PeakWorkingSet64)
} catch { }
if ($process.HasExited -and $process.ExitCode -ne 0) {
    throw "ThreadLight exited with code $($process.ExitCode). See $stderr"
}
$record = [pscustomobject]@{
    tag = $Tag; wave = $Wave; encounter = $kind; fps = $Fps
    instrumentation = if ($DisablePerfLog) { 'off' } elseif ($DisableGpuTimer) { 'perf-log-no-gpu-timer' } else { 'perf-log-gpu-timer' }
    exit_code = $process.ExitCode; wall_seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 3)
    process_cpu_seconds_sampled = [Math]::Round($cpuSeconds, 3)
    cpu_one_core_percent = [Math]::Round(100.0 * $cpuSeconds / $watch.Elapsed.TotalSeconds, 2)
    peak_working_set_bytes_sampled = $peakWorkingSet
    peak_private_bytes_sampled = $peakPrivateBytes
    capture = if ($DisablePerfLog) { '' } else { "$Tag.csv" }
}
$summary = Join-Path $root 'overhead-samples.csv'
if (-not (Test-Path -LiteralPath $summary)) {
    $record | Export-Csv -NoTypeInformation -Encoding utf8 -Path $summary
} else {
    $record | Export-Csv -NoTypeInformation -Encoding utf8 -Append -Path $summary
}
$record | Format-List
