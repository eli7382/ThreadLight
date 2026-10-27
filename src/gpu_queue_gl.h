#pragma once

#include "gpu_queue.h"
#include <memory>

namespace threadlight::gpu {

// Windows GL adapter. Initialize and destroy fences on the context thread.
// The regular renderer never initializes this opt-in adapter.
class OpenGlBackend final : public Backend {
public:
    OpenGlBackend() noexcept;
    ~OpenGlBackend() override;
    bool initialize() noexcept;
    FenceHandle create_fence() noexcept override;
    WaitResult wait(FenceHandle fence, std::uint64_t timeout_ns) noexcept override;
    void delete_fence(FenceHandle fence) noexcept override;
    void flush() noexcept override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace threadlight::gpu
