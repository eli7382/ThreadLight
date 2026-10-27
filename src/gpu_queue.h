#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace threadlight::gpu {

using FenceHandle = void*;

enum class WaitResult {
    signaled,
    timeout,
    failed,
};

// Implement this boundary with the graphics API. Queue takes ownership of each
// non-null handle and releases it through this same backend.
class Backend {
public:
    virtual ~Backend() = default;
    virtual FenceHandle create_fence() noexcept = 0;
    virtual WaitResult wait(FenceHandle fence, std::uint64_t timeout_ns) noexcept = 0;
    virtual void delete_fence(FenceHandle fence) noexcept = 0;
    virtual void flush() noexcept = 0;
};

enum class Result {
    ready,
    disabled,
    error,
};

struct Counters {
    std::uint64_t submitted = 0;
    std::uint64_t waits = 0;
    std::uint64_t timeouts = 0;
    std::uint64_t errors = 0;
    std::size_t max_depth = 0;
};

class Queue final {
public:
    static constexpr std::size_t kMaxOutstanding = 2;
    static constexpr std::uint64_t kWaitTimeoutNs = 5'000'000;

    explicit Queue(Backend& backend) noexcept : backend_(backend) {}
    ~Queue() = default;

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(Queue&&) = delete;

    // Call before rendering each frame. With two outstanding frames, this does
    // one finite wait on the oldest fence; it never spins or waits on newer work.
    Result begin_frame() noexcept {
        if (shutdown_) return Result::disabled;
        if (faulted_) return Result::error;
        if (!enabled_) return Result::disabled;
        if (size_ < kMaxOutstanding) return Result::ready;

        ++counters_.waits;
        const WaitResult result = backend_.wait(fences_[head_], kWaitTimeoutNs);
        if (result == WaitResult::timeout) {
            ++counters_.timeouts;
            fail_and_clear();
            return Result::error;
        }
        if (result != WaitResult::signaled) {
            fail_and_clear();
            return Result::error;
        }

        backend_.delete_fence(fences_[head_]);
        fences_[head_] = nullptr;
        head_ = (head_ + 1) % kMaxOutstanding;
        --size_;
        return Result::ready;
    }

    // Call after submitting a rendered frame. A null fence is a hard error:
    // the caller must stop using this prototype instead of running unbounded.
    Result end_frame() noexcept {
        if (faulted_) return Result::error;
        if (shutdown_ || !enabled_) return Result::ready;
        if (size_ >= kMaxOutstanding) {
            fail_and_clear();
            return Result::error;
        }

        FenceHandle fence = backend_.create_fence();
        if (fence == nullptr) {
            fail_and_clear();
            return Result::error;
        }

        const std::size_t tail = (head_ + size_) % kMaxOutstanding;
        fences_[tail] = fence;
        ++size_;
        ++counters_.submitted;
        if (size_ > counters_.max_depth) counters_.max_depth = size_;
        backend_.flush();
        return Result::ready;
    }

    // Disabling drains handles while the graphics context is still alive.
    // A failure remains latched until reset() is explicitly called.
    Result set_enabled(bool enabled) noexcept {
        if (shutdown_) return Result::disabled;
        if (!enabled) {
            clear_handles();
            enabled_ = false;
            return faulted_ ? Result::error : Result::disabled;
        }
        if (faulted_) return Result::error;
        enabled_ = true;
        return Result::ready;
    }

    // Explicitly destroy all owned fences. Call this before closing the GL
    // context; no GL work is performed by the destructor.
    void shutdown() noexcept {
        if (shutdown_) return;
        clear_handles();
        enabled_ = false;
        shutdown_ = true;
    }

    // Recovery is explicit so an error cannot silently switch to uncapped mode.
    // Intended only after the caller has handled the failure and context is valid.
    Result reset() noexcept {
        if (shutdown_) return Result::disabled;
        clear_handles();
        faulted_ = false;
        return enabled_ ? Result::ready : Result::disabled;
    }

    bool enabled() const noexcept { return enabled_ && !shutdown_; }
    bool faulted() const noexcept { return faulted_; }
    std::size_t depth() const noexcept { return size_; }
    const Counters& counters() const noexcept { return counters_; }

private:
    void clear_handles() noexcept {
        while (size_ != 0) {
            backend_.delete_fence(fences_[head_]);
            fences_[head_] = nullptr;
            head_ = (head_ + 1) % kMaxOutstanding;
            --size_;
        }
        head_ = 0;
    }

    void fail_and_clear() noexcept {
        ++counters_.errors;
        faulted_ = true;
        enabled_ = false;
        clear_handles();
    }

    Backend& backend_;
    std::array<FenceHandle, kMaxOutstanding> fences_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    Counters counters_{};
    bool enabled_ = false;
    bool faulted_ = false;
    bool shutdown_ = false;
};

} // namespace threadlight::gpu
