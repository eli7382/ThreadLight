param(
 [ValidateSet(7,14,21,28,29,30,31,32,33,34,35)][int]$Wave=29,
 [ValidateSet('offense','defense','mobility','mixed')][string]$Build='mixed',
 [ValidateRange(1,4)][int]$Phase=1,
 [uint32]$Seed=7382,
 [switch]$Menu,
 [switch]$Smoke
)
$ErrorActionPreference='Stop'
if($Menu){
 Write-Host 'THREADLIGHT–UNWEAVER DEVELOPER QA (private fixtures)'
 $Wave=[int](Read-Host 'Wave: 7,14,21,28,29-35')
 $Build=Read-Host 'Build: offense, defense, mobility, mixed'
 $Seed=[uint32](Read-Host 'Deterministic seed (e.g. 7382)')
 if($Wave -eq 35){$Phase=[int](Read-Host 'Phase: 1-4')}
}
if($Wave -notin @(7,14,21,28,29,30,31,32,33,34,35) -or $Build -notin @('offense','defense','mobility','mixed') -or $Phase -lt 1 -or $Phase -gt 4){throw 'Invalid scenario selection'}
$game=Join-Path $PSScriptRoot 'ThreadLight-Unweaver-DeveloperQA.exe'
$arguments=@('--m7-seed',"$Seed",'--m7-telemetry')
if($Wave -ge 29 -and $Wave -le 34){$arguments+=@('--director-fixture',"$Wave",'--qa-build',$Build)}
else {
 $fixture=switch($Wave){7 {'--m3-boss-fixture'} 14 {'--m5-dancer-fixture'} 21 {'--m5-conductor-fixture'} 28 {'--m6-hollow-fixture'} 35 {'--unweaver-fixture'}}
 $arguments+=@($fixture,$Build)
 if($Wave -eq 35){$arguments+=@('--unweaver-phase',"$Phase")}
}
if($Smoke){$arguments+=@('--dev-autoplay','--dev-fast','--dev-ticks','360')}
$run=Start-Process -FilePath $game -ArgumentList $arguments -WorkingDirectory $PSScriptRoot -PassThru -Wait
if($run.ExitCode -ne 0){throw "Game failed: $($run.ExitCode)"}
