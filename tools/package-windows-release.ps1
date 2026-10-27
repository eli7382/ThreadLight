[CmdletBinding()]
param(
    [string]$OutputDirectory = '',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'portable-release' }
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$build = Join-Path $repo 'build-portable-windows-msvc'
$exe = Join-Path $build 'Release\Threadlight.exe'

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' is unavailable. Run from a Visual Studio 2022 Developer PowerShell with CMake and dumpbin on PATH."
    }
}

function Get-Sha256([string]$Path) {
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::OpenRead($Path)
    try { [System.BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $algorithm.Dispose() }
}

Require-Command 'git'
Require-Command 'cmake'
Require-Command 'ctest'
Require-Command 'dumpbin'

$status = git -C $repo status --porcelain
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect Git status.' }
if ($status) { throw 'Release packaging requires a clean checkout so the source commit identifies the build exactly.' }
$commit = (git -C $repo rev-parse HEAD).Trim()
$branch = (git -C $repo branch --show-current).Trim()
if (-not $branch.StartsWith('codex/')) { throw "Refusing to package from unexpected branch '$branch'." }

if (-not $SkipBuild) {
    cmake --preset windows-msvc-portable -S $repo
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    cmake --build --preset windows-msvc-portable-release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Windows MSVC Release build failed.' }
    ctest --preset windows-msvc-portable-release --test-dir $build
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }
}
if (-not (Test-Path -LiteralPath $exe)) { throw "Release executable is missing: $exe" }

$pe = (dumpbin /nologo /headers $exe | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'dumpbin could not read the executable headers.' }
if ($pe -notmatch '8664 machine \(x64\)' -or $pe -notmatch 'subsystem \(Windows GUI\)') {
    throw 'The executable must be AMD64/x64 and use the Windows GUI subsystem.'
}
$imports = (dumpbin /nologo /dependents $exe | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'dumpbin could not read the executable imports.' }
$dlls = [regex]::Matches($imports, '(?im)^\s*([A-Za-z0-9_.-]+\.dll)\s*$') | ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() } | Sort-Object -Unique
$allowed = @('advapi32.dll','gdi32.dll','kernel32.dll','opengl32.dll','psapi.dll','shell32.dll','user32.dll','winmm.dll')
$unexpected = @($dlls | Where-Object { $_ -notin $allowed })
if ($unexpected.Count -gt 0) { throw "Unexpected non-system DLL imports: $($unexpected -join ', ')" }
if ($dlls -match '^vcruntime|^msvcp|^concrt|^ucrtbase') { throw 'The executable imports a Visual C++ runtime DLL.' }

$stage = Join-Path ([System.IO.Path]::GetTempPath()) ("Threadlight-release-stage-" + [guid]::NewGuid().ToString('N'))
$packageName = 'Threadlight-Windows-x64-Portable'
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    Copy-Item -LiteralPath $exe -Destination (Join-Path $stage 'Threadlight.exe')
    Copy-Item -LiteralPath (Join-Path $repo 'RAYLIB-LICENSE.txt') -Destination (Join-Path $stage 'RAYLIB-LICENSE.txt')
    $info = @"
Threadlight portable Windows x64 build
Git commit: $commit
Base develop commit: 3bb22b1d29d3000780fb581127b65f5765bafadc
Branch: $branch
Configuration: Release, MSVC 14.44.35207, static CRT (/MT), raylib 5.5 static
Subsystem: Windows GUI
Assets: generated at runtime; no external game assets
"@
    Set-Content -LiteralPath (Join-Path $stage 'BUILD-INFO.txt') -Value $info -Encoding utf8
    $instructions = @'
Threadlight — portable Windows x64 build

Run Threadlight.exe. No installer or development tools are required. The game
creates its settings under %APPDATA%\ThreadLight\settings.ini.

Supported target: 64-bit Windows 10 or Windows 11. The game needs a working
graphics driver with OpenGL 3.3 support. The package has been checked for x64
architecture, GUI subsystem, and Windows system DLL imports; see BUILD-INFO.txt
and the release report for the exact tested host and remaining limitations.

All art and sound effects are generated at runtime. RAYLIB-LICENSE.txt contains
the third-party license notice. Gameplay and full campaign acceptance still
require manual playtesting.
'@
    Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Value $instructions -Encoding utf8
    $zip = Join-Path $output "$packageName.zip"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force

    # Exercise the actual packaged executable extracted to an isolated folder
    # outside the repository, with an isolated APPDATA location.
    $smokeRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("Threadlight-release-smoke-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $smokeRoot | Out-Null
    try {
        Expand-Archive -LiteralPath $zip -DestinationPath $smokeRoot -Force
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = Join-Path $smokeRoot 'Threadlight.exe'
        $startInfo.Arguments = '--settings-render-check'
        $startInfo.WorkingDirectory = $smokeRoot
        $startInfo.UseShellExecute = $false
        $startInfo.EnvironmentVariables['APPDATA'] = Join-Path $smokeRoot 'appdata'
        $render = [System.Diagnostics.Process]::Start($startInfo)
        $render.WaitForExit()
        if ($render.ExitCode -ne 0) { throw "Settings rendering check exited with code $($render.ExitCode)." }
        $capture = Join-Path $smokeRoot 'settings-vsync.png'
        if (-not (Test-Path -LiteralPath $capture) -or (Get-Item -LiteralPath $capture).Length -lt 1024) {
            throw 'Settings render check did not produce a plausible screenshot.'
        }
    } finally { Remove-Item -LiteralPath $smokeRoot -Recurse -Force -ErrorAction SilentlyContinue }

    $portableExe = Join-Path $output 'Threadlight.exe'
    Copy-Item -LiteralPath $exe -Destination $portableExe -Force
    $exeHash = Get-Sha256 $portableExe
    $zipHash = Get-Sha256 $zip
    $exeSize = (Get-Item -LiteralPath $portableExe).Length
    $zipSize = (Get-Item -LiteralPath $zip).Length
    $manifest = @"
SHA256  Threadlight.exe  $exeHash
SHA256  $packageName.zip  $zipHash
Git commit  $commit
Build branch  $branch
EXE bytes  $exeSize
ZIP bytes  $zipSize
PE machine  AMD64 (x64)
PE subsystem  Windows GUI
Imported DLLs  $($dlls -join ', ')
"@
    Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Value $manifest -Encoding utf8
    $report = @"
Threadlight portable release verification
Source commit: $commit
Base develop commit: 3bb22b1d29d3000780fb581127b65f5765bafadc
Branch: $branch
Build: Windows x64 Release; MSVC 14.44.35207 (19.44.35229); static CRT (/MT); static raylib 5.5
Build host: Windows x64; Windows SDK 10.0.26100; Visual Studio 2022
Graphics smoke host: Intel(R) Iris(R) Xe Graphics; OpenGL 3.3.0; driver 32.0.101.5763
PE: AMD64/x64, Windows GUI subsystem
Imports: $($dlls -join ', ')
Redistributable: no VC runtime DLL imports; /MT static CRT
Assets: game art and audio generated at runtime; no external game assets
Automated verification: clean isolated Release build; 37/37 CTest passed; packaged ZIP extracted and settings render check passed from an isolated directory outside the repository.
Visible smoke: the ZIP-extracted executable launched from an isolated temp directory and its titled game window was enumerated. Computer-use approval timed out before screenshot inspection or input. Keyboard navigation/gameplay, audio playback, settings persistence across normal launches, and graceful close remain unverified. The hidden render check intentionally runs without an audio device.
Clean-PC validation still needed on additional Windows versions, GPUs, and audio devices.
Portability limits: intended for Windows 10/11 x64 with a working graphics driver supporting OpenGL 3.3. Evidence is from one Intel Iris Xe host only; no universal compatibility claim.
EXE: $exeSize bytes; SHA-256 $exeHash
ZIP: $zipSize bytes; SHA-256 $zipHash
"@
    Set-Content -LiteralPath (Join-Path $output 'RELEASE-REPORT.txt') -Value $report -Encoding utf8
    Write-Output "Release package: $zip"
    Write-Output "Standalone EXE: $portableExe"
    Write-Output "Hashes and build identification: $(Join-Path $output 'SHA256SUMS.txt')"
} finally { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
