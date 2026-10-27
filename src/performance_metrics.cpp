#include "performance_metrics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace threadlight::performance {

double queryClockMilliseconds() {
#ifdef _WIN32
    LARGE_INTEGER counter{},frequency{};
    if(QueryPerformanceCounter(&counter)&&QueryPerformanceFrequency(&frequency)&&frequency.QuadPart>0)
        return static_cast<double>(counter.QuadPart)*1000.0/static_cast<double>(frequency.QuadPart);
#endif
    return 0;
}

MemorySample processMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        return {static_cast<std::uint64_t>(counters.WorkingSetSize),
                static_cast<std::uint64_t>(counters.PrivateUsage), true};
#endif
    return {};
}

void FrameHistogram::add(double milliseconds) {
    if (!std::isfinite(milliseconds) || milliseconds <= 0) return;
    constexpr std::size_t overflow = 10001;
    const auto bucket = milliseconds > 1000.0 ? overflow :
        static_cast<std::size_t>(std::ceil(milliseconds * 10.0));
    ++buckets_[bucket];
    ++count_;
    maxMs_ = std::max(maxMs_, milliseconds);
}

double FrameHistogram::percentile(double fraction) const {
    if (!count_) return 0;
    if (!std::isfinite(fraction)) return 0;
    fraction = std::clamp(fraction, 0.0, 1.0);
    const auto target = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::ceil(fraction * count_)));
    std::uint64_t seen = 0;
    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        seen += buckets_[i];
        if (seen >= target) return i == 10001 ? maxMs_ : i / 10.0;
    }
    return 0;
}

bool Recorder::open(const std::filesystem::path& path) {
    std::error_code error;
    if (path.empty() || std::filesystem::exists(path, error) || error) return false;
    output_.open(path, std::ios::out | std::ios::binary);
    if (!output_) return false;
    output_ << "frame,elapsed_s,interval_ms,raylib_frame_ms,update_wall_ms,simulation_wall_ms,draw_submit_wall_ms,present_wait_wall_ms,steps,discarded_ms,mode_at_start,phase_after_update,wave_after_update,framebuffer_w,framebuffer_h,enemies,silk_segments,active_silk_segments,sparks,particles,projectiles,ground_hazards,boss_hazards,working_set_bytes,private_bytes,configured_fps,effective_fps,vsync_requested,monitor_hz,focused_at_update,frame_start_qpc_ms\n";
    output_ << std::fixed << std::setprecision(4);
    return healthy_ = static_cast<bool>(output_);
}

bool Recorder::record(const Frame& f) {
    if (!healthy_) return false;
    intervals_.add(f.intervalMs);
    discardedMs_ += f.discardedMs;
    if (f.discardedMs > 0) ++discardedFrames_;
    output_ << f.number << ',' << f.elapsedSeconds << ',' << f.intervalMs << ',' << f.raylibFrameMs << ','
            << f.updateMs << ',' << f.simulationMs << ',' << f.drawMs << ',' << f.presentMs << ','
            << f.steps << ',' << f.discardedMs << ',' << f.mode << ',' << f.phase << ',' << f.wave << ','
            << f.width << ',' << f.height << ',' << f.enemies << ',' << f.silk << ',' << f.activeSilk << ','
            << f.sparks << ',' << f.particles << ',' << f.projectiles << ',' << f.groundHazards << ',' << f.bossHazards << ',';
    if (f.memory.available) output_ << f.memory.workingSetBytes << ',' << f.memory.privateBytes;
    else output_ << ',';
    output_ << ',' << f.configuredFps << ',' << f.effectiveFps << ',' << f.vsyncRequested
            << ',' << f.monitorHz << ',' << f.focused << ',' << f.frameQpcMs << '\n';
    if (f.number % 120 == 0) output_.flush();
    return healthy_ = static_cast<bool>(output_);
}

void Recorder::finish() {
    if (!output_.is_open()) return;
    output_.flush();
    healthy_ = healthy_ && static_cast<bool>(output_);
    output_.close();
    healthy_ = healthy_ && !output_.fail();
}

} // namespace threadlight::performance
