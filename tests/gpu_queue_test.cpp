#include "gpu_queue.h"

#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

using threadlight::gpu::Backend;
using threadlight::gpu::FenceHandle;
using threadlight::gpu::Queue;
using threadlight::gpu::Result;
using threadlight::gpu::WaitResult;

void enable(Queue& queue) {
    CHECK(queue.set_enabled(true) == Result::ready);
}

FenceHandle handle(std::uintptr_t value) {
    return reinterpret_cast<FenceHandle>(value);
}

struct MockBackend final : Backend {
    FenceHandle create_fence() noexcept override {
        ++create_calls;
        if (fail_create) return nullptr;
        const FenceHandle created = handle(next_handle++);
        live.push_back(created);
        return created;
    }

    WaitResult wait(FenceHandle fence, std::uint64_t timeout_ns) noexcept override {
        ++wait_calls;
        waited.push_back(fence);
        timeouts_ns.push_back(timeout_ns);
        if (wait_results.empty()) return WaitResult::signaled;
        const WaitResult result = wait_results.front();
        wait_results.pop_front();
        return result;
    }

    void delete_fence(FenceHandle fence) noexcept override {
        ++delete_calls;
        deleted.push_back(fence);
        for (auto it = live.begin(); it != live.end(); ++it) {
            if (*it == fence) {
                live.erase(it);
                return;
            }
        }
        ++invalid_delete_calls;
    }

    void flush() noexcept override { ++flush_calls; }

    int create_calls = 0;
    int wait_calls = 0;
    int delete_calls = 0;
    int flush_calls = 0;
    int invalid_delete_calls = 0;
    std::uintptr_t next_handle = 1;
    bool fail_create = false;
    std::deque<WaitResult> wait_results;
    std::vector<FenceHandle> live;
    std::vector<FenceHandle> waited;
    std::vector<FenceHandle> deleted;
    std::vector<std::uint64_t> timeouts_ns;
};

void testBoundedFifoWaitsOnlyOnOldestWhenFull() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.begin_frame() == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.begin_frame() == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.depth() == Queue::kMaxOutstanding);
    CHECK(backend.wait_calls == 0);

    backend.wait_results.push_back(WaitResult::signaled);
    CHECK(queue.begin_frame() == Result::ready);
    CHECK(backend.wait_calls == 1);
    CHECK(backend.waited[0] == handle(1));
    CHECK(backend.timeouts_ns[0] == Queue::kWaitTimeoutNs);
    CHECK(backend.deleted.size() == 1 && backend.deleted[0] == handle(1));
    CHECK(queue.depth() == 1);

    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.depth() == 2);
    backend.wait_results.push_back(WaitResult::signaled);
    CHECK(queue.begin_frame() == Result::ready);
    CHECK(backend.waited[1] == handle(2));
    CHECK(backend.deleted[1] == handle(2));

    const auto& counts = queue.counters();
    CHECK(counts.submitted == 3);
    CHECK(counts.waits == 2);
    CHECK(counts.timeouts == 0);
    CHECK(counts.errors == 0);
    CHECK(counts.max_depth == 2);
    queue.shutdown();
    CHECK(backend.live.empty());
    CHECK(backend.invalid_delete_calls == 0);
}

void testOneOutstandingFenceNeverWaits() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.begin_frame() == Result::ready);
    CHECK(backend.wait_calls == 0);
    CHECK(backend.delete_calls == 0);
    CHECK(queue.depth() == 1);
    queue.shutdown();
    CHECK(backend.delete_calls == 1);
}

void testTimeoutFailsClosedAndDeletesEveryFence() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    backend.wait_results.push_back(WaitResult::timeout);
    CHECK(queue.begin_frame() == Result::error);
    CHECK(queue.faulted());
    CHECK(!queue.enabled());
    CHECK(queue.depth() == 0);
    CHECK(backend.wait_calls == 1);
    CHECK(backend.deleted.size() == 2);
    CHECK(backend.deleted[0] == handle(1));
    CHECK(backend.deleted[1] == handle(2));
    CHECK(backend.live.empty());
    CHECK(queue.counters().timeouts == 1);
    CHECK(queue.counters().errors == 1);

    const int calls = backend.create_calls + backend.wait_calls + backend.delete_calls + backend.flush_calls;
    CHECK(queue.begin_frame() == Result::error);
    CHECK(queue.end_frame() == Result::error);
    CHECK(calls == backend.create_calls + backend.wait_calls + backend.delete_calls + backend.flush_calls);
    CHECK(queue.set_enabled(false) == Result::error);
    CHECK(queue.begin_frame() == Result::error);
    CHECK(queue.end_frame() == Result::error);
    CHECK(queue.set_enabled(true) == Result::error);
    CHECK(queue.reset() == Result::disabled);
    CHECK(queue.set_enabled(true) == Result::ready);
    CHECK(queue.begin_frame() == Result::ready);
    queue.shutdown();
}

void testWaitFailureFailsClosedAndDeletesEveryFence() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    backend.wait_results.push_back(WaitResult::failed);
    CHECK(queue.begin_frame() == Result::error);
    CHECK(queue.counters().waits == 1);
    CHECK(queue.counters().timeouts == 0);
    CHECK(queue.counters().errors == 1);
    CHECK(queue.depth() == 0);
    CHECK(backend.live.empty());
    CHECK(backend.deleted.size() == 2);
    CHECK(backend.invalid_delete_calls == 0);
    queue.shutdown();
}

void testFenceCreationFailureClearsExistingFenceWithoutFlush() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::ready);
    backend.fail_create = true;
    CHECK(queue.end_frame() == Result::error);
    CHECK(queue.faulted());
    CHECK(queue.depth() == 0);
    CHECK(backend.create_calls == 2);
    CHECK(backend.flush_calls == 1);
    CHECK(backend.delete_calls == 1);
    CHECK(backend.deleted[0] == handle(1));
    CHECK(backend.live.empty());
    CHECK(queue.counters().submitted == 1);
    CHECK(queue.counters().errors == 1);
    queue.shutdown();
    CHECK(backend.delete_calls == 1);
}

void testNullFenceIsNeverWaitedOnOrDeleted() {
    MockBackend backend;
    backend.fail_create = true;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::error);
    CHECK(queue.depth() == 0);
    CHECK(backend.wait_calls == 0);
    CHECK(backend.delete_calls == 0);
    CHECK(backend.flush_calls == 0);
    CHECK(queue.counters().errors == 1);
    queue.shutdown();
    CHECK(backend.delete_calls == 0);
}

void testToggleAndRepeatedShutdownAreIdempotent() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.set_enabled(false) == Result::disabled);
    CHECK(queue.depth() == 0);
    CHECK(backend.live.empty());
    const int calls_after_drain = backend.delete_calls;
    CHECK(queue.set_enabled(false) == Result::disabled);
    CHECK(backend.delete_calls == calls_after_drain);
    CHECK(queue.begin_frame() == Result::disabled);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(backend.create_calls == 2 && backend.wait_calls == 0);

    CHECK(queue.set_enabled(true) == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    queue.shutdown();
    const int deletes_after_shutdown = backend.delete_calls;
    queue.shutdown();
    CHECK(backend.delete_calls == deletes_after_shutdown);
    CHECK(queue.begin_frame() == Result::disabled);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(backend.live.empty());
    CHECK(backend.invalid_delete_calls == 0);
}

void testDisabledQueueMakesNoBackendCalls() {
    MockBackend backend;
    Queue queue(backend);
    CHECK(queue.set_enabled(false) == Result::disabled);
    CHECK(queue.begin_frame() == Result::disabled);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(backend.create_calls == 0);
    CHECK(backend.wait_calls == 0);
    CHECK(backend.delete_calls == 0);
    CHECK(backend.flush_calls == 0);
    CHECK(queue.counters().submitted == 0);
    queue.shutdown();
    CHECK(backend.create_calls == 0 && backend.delete_calls == 0);
}

void testInvalidQueueOverflowFailsClosed() {
    MockBackend backend;
    Queue queue(backend);
    enable(queue);
    CHECK(queue.end_frame() == Result::ready);
    CHECK(queue.end_frame() == Result::ready);
    // Calling end_frame without first making room violates the frame protocol.
    // It must fail closed instead of overwriting a live fence.
    CHECK(queue.end_frame() == Result::error);
    CHECK(queue.depth() == 0);
    CHECK(backend.live.empty());
    CHECK(backend.deleted.size() == 2);
    CHECK(queue.counters().errors == 1);
    queue.shutdown();
}

} // namespace

int main() {
    testBoundedFifoWaitsOnlyOnOldestWhenFull();
    testOneOutstandingFenceNeverWaits();
    testTimeoutFailsClosedAndDeletesEveryFence();
    testWaitFailureFailsClosedAndDeletesEveryFence();
    testFenceCreationFailureClearsExistingFenceWithoutFlush();
    testNullFenceIsNeverWaitedOnOrDeleted();
    testToggleAndRepeatedShutdownAreIdempotent();
    testDisabledQueueMakesNoBackendCalls();
    testInvalidQueueOverflowFailsClosed();
    if (failures != 0) return 1;
    std::cout << "GPU queue tests passed\n";
    return 0;
}
