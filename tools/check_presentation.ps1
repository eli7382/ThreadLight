param(
    [string]$Executable = (Join-Path $PSScriptRoot '../build-windows-msvc/Release/Threadlight.exe'),
    [string]$PresentMon = (Join-Path $PSScriptRoot '../build-windows-msvc/tools/PresentMon-2.6.0-x64.exe'),
    [ValidateSet(30,60,120,240)]
    [int[]]$Limits = @(60,120),
    [ValidateSet('v1','v2')]
    [string]$Metrics = 'v2'
)

$ErrorActionPreference = 'Stop'

function Quote-ProcessArgument([string]$Value) {
    '"' + $Value.Replace('"', '\"') + '"'
}

function Start-HiddenProcess([string]$Path, [string[]]$Arguments, [string]$WorkingDirectory, [string]$StdoutPath, [string]$StderrPath) {
    $quotedArguments = @($Arguments | ForEach-Object { Quote-ProcessArgument $_ })
    Start-Process -FilePath $Path -ArgumentList ($quotedArguments -join ' ') `
        -WorkingDirectory $WorkingDirectory -WindowStyle Hidden `
        -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath -PassThru
}

$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$presentMonPath = (Resolve-Path -LiteralPath $PresentMon).Path
$outputRoot = Join-Path $PSScriptRoot ('../build-windows-msvc/qa/presentation-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $outputRoot).Path

# Start with 60/120 by default. Additional finite rates must be selected explicitly;
# neither the harness nor the game diagnostic accepts an uncapped value.
$cases = @(foreach($limit in $Limits){
    [pscustomobject]@{ Limit = $limit; Vsync = $false }
    [pscustomobject]@{ Limit = $limit; Vsync = $true }
})

$previousAppData = $env:APPDATA
$results = [System.Collections.Generic.List[object]]::new()
$failure = $null

try {
    foreach ($case in $cases) {
        $vsyncName = if ($case.Vsync) { 'on' } else { 'off' }
        $caseName = "$($case.Limit)fps-vsync-$vsyncName"
        $caseRoot = Join-Path $outputRoot $caseName
        $appDataRoot = Join-Path $caseRoot 'appdata'
        $settingsRoot = Join-Path $appDataRoot 'ThreadLight'
        New-Item -ItemType Directory -Path $settingsRoot | Out-Null

        # Isolate settings writes from the user's real profile.
        $env:APPDATA = $appDataRoot
        $settings = "version=1`nframe_rate_limit=$($case.Limit)`nshow_fps=0`nvsync=$(if ($case.Vsync) { 1 } else { 0 })`nsfx_volume_percent=0`ntutorial_completed=1`ncodex_seen=2047`nbest_wave=0`nbest_won=0`nbest_score=0`nbest_kills=0`n"
        [System.IO.File]::WriteAllText((Join-Path $settingsRoot 'settings.ini'), $settings, [System.Text.UTF8Encoding]::new($false))

        $appCsv = Join-Path $caseRoot 'threadlight-app.csv'
        $presentMonCsv = Join-Path $caseRoot 'presentmon.csv'
        $gameStdout = Join-Path $caseRoot 'game.stdout.log'
        $gameStderr = Join-Path $caseRoot 'game.stderr.log'
        $presentMonStdout = Join-Path $caseRoot 'presentmon.stdout.log'
        $presentMonStderr = Join-Path $caseRoot 'presentmon.stderr.log'
        $sessionName = 'ThreadLight-' + [guid]::NewGuid().ToString('N')
        $game = $null
        $capture = $null
        $cleanup = $null
        $caseFailure = $null

        try {
            $gameArgs = @(
                '--pacing-check', [string]$case.Limit,
                '--pacing-vsync', $vsyncName,
                '--perf-log', $appCsv
            )
            $game = Start-HiddenProcess $executablePath $gameArgs $caseRoot $gameStdout $gameStderr
            $gamePid = $game.Id

            # Start immediately after obtaining the fixture PID. Capture only
            # this process; the unique session name prevents touching any other
            # PresentMon/ETW session.
            $captureArgs = @(
                '--process_id', [string]$gamePid,
                '--timed', '6',
                '--terminate_after_timed',
                '--terminate_on_proc_exit',
                '--session_name', $sessionName,
                ('--' + $Metrics + '_metrics'),
                '--qpc_time_ms',
                '--no_console_stats',
                '--no_track_input',
                '--output_file', $presentMonCsv
            )
            $capture = Start-HiddenProcess $presentMonPath $captureArgs $caseRoot $presentMonStdout $presentMonStderr

            if (-not $game.WaitForExit(9000)) {
                $game.Kill()
                [void]$game.WaitForExit(2000)
                throw "Game watchdog stopped $caseName after nine seconds. No later cases were run."
            }
            if ($game.ExitCode -ne 0) {
                throw "$caseName game fixture exited with code $($game.ExitCode). See $caseRoot. No later cases were run."
            }

            # The tool's six-second timer and process-exit option should close
            # its own session. If it does not, ask only that uniquely named
            # session to stop; never terminate global or unrelated ETW sessions.
            if (-not $capture.WaitForExit(9000)) {
                $cleanupArgs = @('--terminate_existing_session', '--session_name', $sessionName)
                $cleanup = Start-HiddenProcess $presentMonPath $cleanupArgs $caseRoot `
                    (Join-Path $caseRoot 'presentmon-cleanup.stdout.log') `
                    (Join-Path $caseRoot 'presentmon-cleanup.stderr.log')
                if (-not $cleanup.WaitForExit(3000)) {
                    throw "PresentMon did not stop its own session '$sessionName' after the timed capture or targeted cleanup. No later cases were run; inspect $caseRoot."
                }
                [void]$capture.WaitForExit(1000)
                if (-not $capture.HasExited) {
                    throw "PresentMon client remained alive after targeted session cleanup for '$sessionName'. No later cases were run; inspect $caseRoot."
                }
            }
            if ($capture.ExitCode -ne 0) {
                throw "$caseName PresentMon exited with code $($capture.ExitCode). See $caseRoot. No later cases were run."
            }

            foreach ($csvPath in @($appCsv, $presentMonCsv)) {
                if (-not (Test-Path -LiteralPath $csvPath -PathType Leaf)) {
                    throw "$caseName did not produce $([System.IO.Path]::GetFileName($csvPath)). No later cases were run."
                }
                if ((Get-Item -LiteralPath $csvPath).Length -eq 0) {
                    throw "$caseName produced an empty $([System.IO.Path]::GetFileName($csvPath)). No later cases were run."
                }
                $rows = @(Import-Csv -LiteralPath $csvPath)
                if ($rows.Count -lt 3) {
                    throw "$caseName produced only $($rows.Count) data rows in $([System.IO.Path]::GetFileName($csvPath)); at least three are required for analysis. No later cases were run."
                }
                if($csvPath -eq $appCsv){
                    $expectedVsync=if($case.Vsync){'1'}else{'0'}
                    if(@($rows|Where-Object {$_.effective_fps -ne [string]$case.Limit -or $_.vsync_requested -ne $expectedVsync}).Count){
                        throw "$caseName did not retain the requested finite cap and VSync state. No later cases were run."
                    }
                }
            }

            $results.Add([pscustomobject]@{
                Case = $caseName
                FrameRateLimit = $case.Limit
                Vsync = $vsyncName
                Metrics = $Metrics
                GameProcessId = $gamePid
                GameExitCode = $game.ExitCode
                PresentMonExitCode = $capture.ExitCode
                AppCsv = $appCsv
                PresentMonCsv = $presentMonCsv
                PresentMonSession = $sessionName
            })
        } catch {
            $caseFailure = $_
            throw
        } finally {
            if ($game -and -not $game.HasExited) {
                $game.Kill()
                [void]$game.WaitForExit(2000)
            }
            if ($capture -and -not $capture.HasExited) {
                # Best-effort cleanup of this capture's named session only.
                if (-not $cleanup) {
                    $cleanup = Start-HiddenProcess $presentMonPath @('--terminate_existing_session', '--session_name', $sessionName) `
                        $caseRoot (Join-Path $caseRoot 'presentmon-finalize.stdout.log') `
                        (Join-Path $caseRoot 'presentmon-finalize.stderr.log')
                    [void]$cleanup.WaitForExit(3000)
                }
                if (-not $capture.WaitForExit(2000)) {
                    $capture.Kill()
                    [void]$capture.WaitForExit(2000)
                    Write-Warning "Stopped this capture client; if session cleanup failed, inspect only session '$sessionName'."
                }
            }
            if($cleanup -and -not $cleanup.HasExited){$cleanup.Kill();[void]$cleanup.WaitForExit(2000)}
            if ($caseFailure) {
                [System.IO.File]::WriteAllText((Join-Path $caseRoot 'failure.txt'), [string]$caseFailure, [System.Text.UTF8Encoding]::new($false))
            }
        }
    }
} catch {
    $failure = $_
} finally {
    $env:APPDATA = $previousAppData
}

if ($results.Count -gt 0) {
    $results | Export-Csv -LiteralPath (Join-Path $outputRoot 'captures.csv') -NoTypeInformation
}
if ($failure) {
    [System.IO.File]::WriteAllText((Join-Path $outputRoot 'failure.txt'), [string]$failure, [System.Text.UTF8Encoding]::new($false))
    throw "Presentation pacing collection stopped at the first failure. Captures: $outputRoot`n$failure"
}

$results | Format-Table -AutoSize
Write-Output "Raw captures: $outputRoot"
