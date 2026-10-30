param(
    [string]$GamePath = "$PSScriptRoot\..\portable-release\unweaver-build\Release\Threadlight.exe",
    [string]$OutputRoot = "$PSScriptRoot\..\portable-release\unweaver-qa\smoke-$(Get-Date -Format yyyyMMdd-HHmmss)"
)
$ErrorActionPreference = 'Stop'
$game = (Resolve-Path -LiteralPath $GamePath).Path
if (Test-Path -LiteralPath $OutputRoot) { throw 'Use a new output directory; QA never overwrites evidence.' }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$root = (Resolve-Path -LiteralPath $OutputRoot).Path
$cases = @(
    @{ Name='hunt'; Args=@('--unweaver-fixture','mixed','--unweaver-phase','1') },
    @{ Name='deceiver'; Args=@('--unweaver-fixture','mixed','--unweaver-phase','2','--dev-autoplay') },
    @{ Name='seal'; Args=@('--unweaver-fixture','mixed','--unweaver-phase','3') },
    @{ Name='unbound-minimum'; Args=@('--unweaver-fixture','mixed','--unweaver-phase','4','--m7-min-window') },
    @{ Name='severance'; Args=@('--unweaver-fixture','mixed','--unweaver-phase','3','--unweaver-mechanic','2','--dev-autoplay') },
    @{ Name='corruption'; Args=@('--unweaver-fixture','mixed','--unweaver-phase','3','--unweaver-mechanic','3','--dev-autoplay') },
    @{ Name='thorn-dominion'; Args=@('--unweaver-fixture','defense','--unweaver-phase','3','--unweaver-formation','2','--dev-autoplay','--dev-capture','--dev-ticks','2400') },
    @{ Name='dancer'; Args=@('--m5-dancer-fixture','defense','--dev-autoplay','--dev-capture','--dev-ticks','1200') },
    @{ Name='conductor'; Args=@('--m5-conductor-fixture','defense','--dev-autoplay','--dev-capture','--dev-ticks','1200') },
    @{ Name='hollow'; Args=@('--m6-hollow-fixture','defense','--dev-autoplay','--dev-capture','--dev-ticks','1200') },
    @{ Name='director-runner'; Args=@('--director-fixture','29','--director-style','runner','--dev-autoplay','--dev-capture') },
    @{ Name='settings'; Args=@('--settings-render-check') },
    @{ Name='violet'; Args=@('--personalization-render-check','violet') },
    @{ Name='director-defensive'; Args=@('--director-fixture','34','--director-style','defensive','--dev-autoplay','--dev-capture') }
)
$results = @()
foreach ($case in $cases) {
    $folder = Join-Path $root $case.Name
    New-Item -ItemType Directory -Path $folder | Out-Null
    $arguments = $case.Args + @('--m6-render-check','--dev-fast','--m7-telemetry')
    $child = Start-Process -FilePath $game -ArgumentList $arguments -WorkingDirectory $folder -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $folder 'game.log') -RedirectStandardError (Join-Path $folder 'error.log')
    if (!$child.WaitForExit(30000)) { Stop-Process -Id $child.Id; throw "Fixture timeout: $($case.Name)" }
    $child.Refresh()
    $pictures = @(Get-ChildItem -LiteralPath $folder -Filter '*.png')
    if ($child.ExitCode -ne 0 -or $pictures.Count -eq 0) { throw "Fixture failed: $($case.Name), exit=$($child.ExitCode)" }
    $results += [pscustomobject]@{ Case=$case.Name; Exit=$child.ExitCode; Images=$pictures.Count }
}
$results | Export-Csv -LiteralPath (Join-Path $root 'results.csv') -NoTypeInformation
Get-FileHash -LiteralPath $game -Algorithm SHA256 | Format-List | Out-File -LiteralPath (Join-Path $root 'executable-sha256.txt')
$results | Format-Table -AutoSize
Write-Output "Evidence: $root"
