function Get-Percentile([double[]]$Values, [double]$Fraction) {
    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) { return 0.0 }
    $index = [Math]::Max(0, [Math]::Ceiling($Fraction * $sorted.Count) - 1)
    return [double]$sorted[$index]
}

$root = $PSScriptRoot
$runs = @()
foreach ($file in Get-ChildItem -LiteralPath $root -Filter 'wave*.csv') {
    if ($file.BaseName -notmatch '^wave(28|34|35)-(hollow|circuit|heart)-(60|120)fps-r([1-3])$') { continue }
    $wave = [int]$Matches[1]; $fps = [int]$Matches[3]; $run = [int]$Matches[4]
    $allRows = @(Import-Csv -LiteralPath $file.FullName)
    $rows = @($allRows | Where-Object {
        $_.mode_at_start -eq 'play' -and [int]$_.wave_after_update -eq $wave -and
        $_.interval_ms -ne '' -and [double]$_.interval_ms -gt 0
    })
    if ($rows.Count -lt 1000) { throw "Incomplete/incorrect fixture capture $($file.Name): $($rows.Count) valid gameplay intervals" }
    $intervals = [double[]]@($rows | ForEach-Object { [double]$_.interval_ms })
    $updates = [double[]]@($rows | ForEach-Object { [double]$_.update_wall_ms })
    $simulations = [double[]]@($rows | ForEach-Object { [double]$_.simulation_wall_ms })
    $draws = [double[]]@($rows | ForEach-Object { [double]$_.draw_submit_wall_ms })
    $presents = [double[]]@($rows | ForEach-Object { [double]$_.present_wait_wall_ms })
    $gpuRows = @($rows | Where-Object { $_.gpu_render_ms -ne '' })
    $gpu = [double[]]@($gpuRows | ForEach-Object { [double]$_.gpu_render_ms })
    $gpuLag = [double[]]@($gpuRows | ForEach-Object { [double]$_.frame - [double]$_.gpu_source_frame })
    $budget = 1000.0 / $fps
    $memory = @($allRows | Where-Object { $_.working_set_bytes -ne '' })
    $steps = [int[]]@($rows | ForEach-Object { [int]$_.steps })
    $discarded = [double[]]@($rows | ForEach-Object { [double]$_.discarded_ms })
    $series = [pscustomobject]@{
        wave = $wave; encounter = $Matches[2]; fps = $fps; run = $run
        frames = $intervals.Count
        actual_fps = [Math]::Round(1000.0 * $intervals.Count / (($intervals | Measure-Object -Sum).Sum), 3)
        frame_p50_ms = [Math]::Round((Get-Percentile $intervals .50), 3)
        frame_p95_ms = [Math]::Round((Get-Percentile $intervals .95), 3)
        frame_p99_ms = [Math]::Round((Get-Percentile $intervals .99), 3)
        frame_max_ms = [Math]::Round((($intervals | Measure-Object -Maximum).Maximum), 3)
        over_budget_frames = @($intervals | Where-Object { $_ -gt $budget }).Count
        spikes_over_1_5x_budget = @($intervals | Where-Object { $_ -gt 1.5 * $budget }).Count
        update_p95_ms = [Math]::Round((Get-Percentile $updates .95), 3)
        simulation_p95_ms = [Math]::Round((Get-Percentile $simulations .95), 3)
        draw_submit_p95_ms = [Math]::Round((Get-Percentile $draws .95), 3)
        end_drawing_p50_ms = [Math]::Round((Get-Percentile $presents .50), 3)
        end_drawing_p95_ms = [Math]::Round((Get-Percentile $presents .95), 3)
        gpu_timer_samples = $gpu.Count
        gpu_render_p50_ms = [Math]::Round((Get-Percentile $gpu .50), 3)
        gpu_render_p95_ms = [Math]::Round((Get-Percentile $gpu .95), 3)
        gpu_render_p99_ms = [Math]::Round((Get-Percentile $gpu .99), 3)
        gpu_render_max_ms = if ($gpu.Count) { [Math]::Round((($gpu | Measure-Object -Maximum).Maximum), 3) } else { 0 }
        gpu_source_lag_frames_p95 = [Math]::Round((Get-Percentile $gpuLag .95), 1)
        mean_update_ms = [Math]::Round((($updates | Measure-Object -Average).Average), 3)
        mean_simulation_ms = [Math]::Round((($simulations | Measure-Object -Average).Average), 3)
        mean_draw_submit_ms = [Math]::Round((($draws | Measure-Object -Average).Average), 3)
        mean_end_drawing_ms = [Math]::Round((($presents | Measure-Object -Average).Average), 3)
        discarded_frames = @($discarded | Where-Object { $_ -gt 0 }).Count
        discarded_ms = [Math]::Round((($discarded | Measure-Object -Sum).Sum), 4)
        steps_per_frame_mean = [Math]::Round((($steps | Measure-Object -Average).Average), 3)
        max_enemies = [int](($rows | ForEach-Object { [int]$_.enemies } | Measure-Object -Maximum).Maximum)
        max_silk_segments = [int](($rows | ForEach-Object { [int]$_.silk_segments } | Measure-Object -Maximum).Maximum)
        max_active_silk_segments = [int](($rows | ForEach-Object { [int]$_.active_silk_segments } | Measure-Object -Maximum).Maximum)
        max_sparks = [int](($rows | ForEach-Object { [int]$_.sparks } | Measure-Object -Maximum).Maximum)
        max_particles = [int](($rows | ForEach-Object { [int]$_.particles } | Measure-Object -Maximum).Maximum)
        max_projectiles = [int](($rows | ForEach-Object { [int]$_.projectiles } | Measure-Object -Maximum).Maximum)
        max_ground_hazards = [int](($rows | ForEach-Object { [int]$_.ground_hazards } | Measure-Object -Maximum).Maximum)
        max_boss_hazards = [int](($rows | ForEach-Object { [int]$_.boss_hazards } | Measure-Object -Maximum).Maximum)
        working_set_start_mib = [Math]::Round(([double]$memory[0].working_set_bytes / 1MB), 1)
        working_set_end_mib = [Math]::Round(([double]$memory[-1].working_set_bytes / 1MB), 1)
        working_set_peak_mib = [Math]::Round((([double[]]@($memory | ForEach-Object { [double]$_.working_set_bytes }) | Measure-Object -Maximum).Maximum / 1MB), 1)
        working_set_growth_mib = [Math]::Round(([double]$memory[-1].working_set_bytes - [double]$memory[0].working_set_bytes) / 1MB, 1)
        private_start_mib = [Math]::Round(([double]$memory[0].private_bytes / 1MB), 1)
        private_end_mib = [Math]::Round(([double]$memory[-1].private_bytes / 1MB), 1)
        private_peak_sampled_mib = [Math]::Round((([double[]]@($memory | ForEach-Object { [double]$_.private_bytes }) | Measure-Object -Maximum).Maximum / 1MB), 1)
        private_growth_mib = [Math]::Round(([double]$memory[-1].private_bytes - [double]$memory[0].private_bytes) / 1MB, 1)
        path = $file.Name
    }
    $runs += $series
}
$runs = @($runs | Sort-Object wave,fps,run)
if ($runs.Count -ne 18) { throw "Expected 18 primary captures, found $($runs.Count)." }
$runs | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $root 'capture-summary.csv')

$conditions = @()
foreach ($group in $runs | Group-Object wave,fps) {
    $subset = @($group.Group)
    $condition = [pscustomobject]@{
        wave = $subset[0].wave; encounter = $subset[0].encounter; fps = $subset[0].fps
        repeats = $subset.Count
        actual_fps_mean = [Math]::Round((($subset.actual_fps | Measure-Object -Average).Average), 2)
        actual_fps_sd = [Math]::Round([Math]::Sqrt((($subset.actual_fps | ForEach-Object { [Math]::Pow($_ - (($subset.actual_fps | Measure-Object -Average).Average), 2) } | Measure-Object -Average).Average)), 2)
        frame_p50_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.frame_p50_ms) .50), 2)
        frame_p95_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.frame_p95_ms) .50), 2)
        frame_p99_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.frame_p99_ms) .50), 2)
        frame_max_ms_worst = [Math]::Round((($subset.frame_max_ms | Measure-Object -Maximum).Maximum), 2)
        over_budget_frames = ($subset.over_budget_frames | Measure-Object -Sum).Sum
        spikes_over_1_5x_budget = ($subset.spikes_over_1_5x_budget | Measure-Object -Sum).Sum
        update_p95_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.update_p95_ms) .50), 3)
        simulation_p95_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.simulation_p95_ms) .50), 3)
        draw_submit_p95_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.draw_submit_p95_ms) .50), 3)
        end_drawing_p50_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.end_drawing_p50_ms) .50), 3)
        end_drawing_p95_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.end_drawing_p95_ms) .50), 3)
        gpu_timer_samples_min = ($subset.gpu_timer_samples | Measure-Object -Minimum).Minimum
        gpu_render_p50_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.gpu_render_p50_ms) .50), 3)
        gpu_render_p95_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.gpu_render_p95_ms) .50), 3)
        gpu_render_p99_ms_median = [Math]::Round((Get-Percentile ([double[]]$subset.gpu_render_p99_ms) .50), 3)
        gpu_render_max_ms_worst = [Math]::Round((($subset.gpu_render_max_ms | Measure-Object -Maximum).Maximum), 3)
        discarded_frames = ($subset.discarded_frames | Measure-Object -Sum).Sum
        discarded_ms = [Math]::Round((($subset.discarded_ms | Measure-Object -Sum).Sum), 3)
        working_set_peak_mib_max = [Math]::Round((($subset.working_set_peak_mib | Measure-Object -Maximum).Maximum), 1)
        private_peak_sampled_mib_max = [Math]::Round((($subset.private_peak_sampled_mib | Measure-Object -Maximum).Maximum), 1)
        max_enemies = ($subset.max_enemies | Measure-Object -Maximum).Maximum
        max_active_silk_segments = ($subset.max_active_silk_segments | Measure-Object -Maximum).Maximum
        max_particles = ($subset.max_particles | Measure-Object -Maximum).Maximum
        max_projectiles = ($subset.max_projectiles | Measure-Object -Maximum).Maximum
        max_ground_hazards = ($subset.max_ground_hazards | Measure-Object -Maximum).Maximum
        max_boss_hazards = ($subset.max_boss_hazards | Measure-Object -Maximum).Maximum
    }
    $conditions += $condition
}
$conditions | Sort-Object wave,fps | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $root 'condition-summary.csv')
$conditions | Sort-Object wave,fps | Format-Table wave,encounter,fps,repeats,actual_fps_mean,frame_p50_ms_median,frame_p95_ms_median,frame_p99_ms_median,gpu_render_p50_ms_median,gpu_render_p95_ms_median,gpu_render_p99_ms_median,gpu_render_max_ms_worst,update_p95_ms_median,simulation_p95_ms_median,draw_submit_p95_ms_median,end_drawing_p50_ms_median,end_drawing_p95_ms_median,discarded_frames,working_set_peak_mib_max,private_peak_sampled_mib_max -AutoSize
