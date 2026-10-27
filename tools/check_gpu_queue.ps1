param(
    [string]$Executable = (Join-Path $PSScriptRoot '../build-windows-msvc/Release/Threadlight.exe')
)
$ErrorActionPreference = 'Stop'
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$outputRoot = Join-Path $PSScriptRoot ('../build-windows-msvc/qa/gpu-queue-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $outputRoot).Path
$previousAppData = $env:APPDATA
$results = @()
try {
    foreach ($fixture in @('utility', 'heart')) {
        foreach ($queue in @($false, $true)) {
            $caseName = $fixture + $(if ($queue) { '-queue' } else { '-baseline' })
            $caseRoot = Join-Path $outputRoot $caseName
            New-Item -ItemType Directory -Path (Join-Path $caseRoot 'ThreadLight') | Out-Null
            $env:APPDATA = $caseRoot
            [System.IO.File]::WriteAllText((Join-Path $caseRoot 'ThreadLight/settings.ini'), "frame_rate_limit=60`nshow_fps=0`nsfx_volume_percent=0`ntutorial_completed=1`n")
            $capture = Join-Path $caseRoot 'frames.csv'
            $stdout = Join-Path $caseRoot 'stdout.log'
            $arguments = @('--m7-render-check', '--perf-log', ('"' + $capture + '"'))
            if ($fixture -eq 'utility') { $arguments += '--m7-utility-fixture' }
            else { $arguments += @('--m6-heart-fixture', 'mixed') }
            if ($queue) { $arguments += '--gpu-queue' }
            $game = Start-Process -FilePath $executablePath -ArgumentList $arguments -WorkingDirectory $caseRoot -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError (Join-Path $caseRoot 'stderr.log') -PassThru
            try {
                if (-not $game.WaitForExit(10000)) {
                    $game.Kill()
                    [void]$game.WaitForExit(2000)
                    throw "Watchdog stopped $caseName after ten seconds. No further fixtures will run."
                }
                if ($game.ExitCode -ne 0) { throw "$caseName exited with code $($game.ExitCode). See $caseRoot" }
            } finally {
                if (-not $game.HasExited) { $game.Kill() }
            }
            $rows = @(Import-Csv -LiteralPath $capture)
            if ($rows.Count -lt 3) { throw "No useful frame capture for $caseName" }
            $activeRows = @($rows | Where-Object { $_.gpu_queue_active -eq '1' })
            $peakDepth = ($rows | Measure-Object -Property gpu_queue_depth -Maximum).Maximum
            if ($queue -and ($activeRows.Count -eq 0 -or $peakDepth -ne 2)) { throw "Queue did not exercise its two-fence path in $caseName" }
            if (-not $queue -and ($activeRows.Count -ne 0 -or $peakDepth -ne 0)) { throw "Baseline unexpectedly enabled queue in $caseName" }
            if ($peakDepth -gt 2) { throw "Queue depth exceeded two in $caseName" }
            $log = [System.IO.File]::ReadAllText($stdout)
            if ($log -match 'Target time per frame: 0\.000') { throw "Unexpected uncapped target in $caseName" }
            if ($queue -and $log -notmatch 'GPUQUEUE submitted=\d+ waits=\d+ max_depth=2 timeouts=0 errors=0') { throw "Queue reported failure in $caseName" }
            $results += [pscustomobject]@{
                Case = $caseName
                Frames = $rows.Count
                ActiveFrames = $activeRows.Count
                MaxDepth = $peakDepth
                MaxWaitMs = ($rows | Measure-Object -Property gpu_queue_wait_wall_ms -Maximum).Maximum
                MeanWaitMs = ($rows | Measure-Object -Property gpu_queue_wait_wall_ms -Average).Average
                DiscardedMs = ($rows | Measure-Object -Property discarded_ms -Sum).Sum
            }
        }
    }
} finally { $env:APPDATA = $previousAppData }
$results | Export-Csv -LiteralPath (Join-Path $outputRoot 'summary.csv') -NoTypeInformation
$results | Format-Table -AutoSize
Write-Output "Captures: $outputRoot"
