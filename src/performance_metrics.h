#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace threadlight::performance {

struct MemorySample {
    std::uint64_t workingSetBytes = 0;
    std::uint64_t privateBytes = 0;
    bool available = false;
};

// Windows reports process memory; other platforms report unavailable.
MemorySample processMemory();

struct Frame {
    std::uint64_t number = 0;
    double elapsedSeconds = 0, intervalMs = 0, raylibFrameMs = 0;
    double updateMs = 0, simulationMs = 0, drawMs = 0, presentMs = 0;
    double discardedMs = 0;
    int steps = 0, wave = 0, width = 0, height = 0;
    const char* mode = "", *phase = "";
    std::size_t enemies = 0, silk = 0, activeSilk = 0, sparks = 0, particles = 0;
    std::size_t projectiles = 0, groundHazards = 0, bossHazards = 0;
    MemorySample memory{};
};

// Bounded histogram: 0.1 ms buckets through 1000 ms, plus an overflow bucket.
class FrameHistogram {
public:
    void add(double milliseconds);
    double percentile(double fraction) const;
    std::uint64_t count() const { return count_; }
    double maxMs() const { return maxMs_; }
private:
    std::array<std::uint64_t, 10002> buckets_{};
    std::uint64_t count_ = 0;
    double maxMs_ = 0;
};

class Recorder {
public:
    // A diagnostic session never overwrites an existing file.
    bool open(const std::filesystem::path& path);
    bool record(const Frame& frame);
    void finish();
    const FrameHistogram& intervals() const { return intervals_; }
    double discardedMs() const { return discardedMs_; }
    std::uint64_t discardedFrames() const { return discardedFrames_; }
    bool healthy() const { return healthy_; }
private:
    std::ofstream output_;
    FrameHistogram intervals_;
    double discardedMs_ = 0;
    std::uint64_t discardedFrames_ = 0;
    bool healthy_ = false;
};

} // namespace threadlight::performance
