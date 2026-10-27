$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$data = @(Import-Csv -LiteralPath (Join-Path $root 'condition-summary.csv') | Sort-Object { [int]$_.wave }, { [int]$_.fps })
if ($data.Count -ne 6) { throw "Expected six reviewed conditions, got $($data.Count). Run summarize-profile.ps1 first." }
$width = 1040; $left = 330; $scale = 34.0; $top = 126; $rowHeight = 48
$svg = [System.Collections.Generic.List[string]]::new()
$svg.Add('<svg xmlns="http://www.w3.org/2000/svg" width="1040" height="480" viewBox="0 0 1040 480" role="img" aria-labelledby="title desc">')
$svg.Add('<title id="title">Frame p95 and measured GPU p95 by encounter and cap</title>')
$svg.Add('<desc id="desc">Horizontal paired bars show run-level p95 medians from three 20-second runs. Frame times range from 8.77 to 16.78 milliseconds; GPU render times range from 1.00 to 1.11 milliseconds.</desc>')
$svg.Add('<rect width="1040" height="480" fill="#ffffff"/>')
$svg.Add('<text x="28" y="38" font-family="Segoe UI, Arial, sans-serif" font-size="22" font-weight="600" fill="#172b3a">Frame p95 and measured GPU p95</text>')
$svg.Add('<text x="28" y="63" font-family="Segoe UI, Arial, sans-serif" font-size="13" fill="#526777">Median run-level percentile, milliseconds · 3 repeats per condition</text>')
$svg.Add('<rect x="690" y="28" width="13" height="13" rx="2" fill="#2471a3"/><text x="712" y="40" font-family="Segoe UI, Arial, sans-serif" font-size="13" fill="#344a59">Frame p95</text>')
$svg.Add('<rect x="810" y="28" width="13" height="13" rx="2" fill="#c97713"/><text x="832" y="40" font-family="Segoe UI, Arial, sans-serif" font-size="13" fill="#344a59">GPU p95</text>')
for ($tick = 0; $tick -le 18; $tick += 2) {
    $x = $left + $tick * $scale
    $svg.Add(('<line x1="{0}" y1="92" x2="{0}" y2="399" stroke="#dfe6eb" stroke-width="1"/>' -f $x))
    $svg.Add(('<text x="{0}" y="418" text-anchor="middle" font-family="Segoe UI, Arial, sans-serif" font-size="12" fill="#526777">{1}</text>' -f $x,$tick))
}
$svg.Add('<text x="640" y="441" text-anchor="middle" font-family="Segoe UI, Arial, sans-serif" font-size="13" fill="#344a59">Time (ms)</text>')
for ($index = 0; $index -lt $data.Count; $index++) {
    $row = $data[$index]
    $y = $top + $index * $rowHeight
    $encounter = switch ([int]$row.wave) { 28 { 'Wave 28 · Hollow boss' } 34 { 'Wave 34 · Circuit encounter' } 35 { 'Wave 35 · Heart boss' } }
    $label = "$encounter · $($row.fps) FPS cap"
    $frame = [double]$row.frame_p95_ms_median
    $gpu = [double]$row.gpu_render_p95_ms_median
    $frameWidth = [Math]::Round($frame * $scale, 1)
    $gpuWidth = [Math]::Round($gpu * $scale, 1)
    $svg.Add(('<text x="28" y="{0}" font-family="Segoe UI, Arial, sans-serif" font-size="13" fill="#344a59">{1}</text>' -f ($y + 18),$label))
    $svg.Add(('<rect x="{0}" y="{1}" width="{2}" height="15" rx="2" fill="#2471a3"/><text x="{3}" y="{4}" font-family="Segoe UI, Arial, sans-serif" font-size="12" fill="#344a59">{5:F2}</text>' -f $left,$y,$frameWidth,[Math]::Round($left + $frameWidth + 7,1),($y + 12),$frame))
    $svg.Add(('<rect x="{0}" y="{1}" width="{2}" height="15" rx="2" fill="#c97713"/><text x="{3}" y="{4}" font-family="Segoe UI, Arial, sans-serif" font-size="12" fill="#344a59">{5:F2}</text>' -f $left,($y + 19),$gpuWidth,[Math]::Round($left + $gpuWidth + 7,1),($y + 31),$gpu))
}
$svg.Add('<text x="28" y="468" font-family="Segoe UI, Arial, sans-serif" font-size="11" fill="#647887">GPU interval ends after the explicit raylib batch flush; it excludes EndDrawing presentation.</text>')
$svg.Add('</svg>')
$svg -join [Environment]::NewLine | Set-Content -LiteralPath (Join-Path $root 'performance-summary.svg') -Encoding utf8
