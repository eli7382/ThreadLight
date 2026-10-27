#include "gpu_queue_gl.h"
#include <new>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#endif

namespace threadlight::gpu {

#if defined(_WIN32)
struct OpenGlBackend::Impl {
    using Sync = void*;
    using Fence = Sync (APIENTRY*)(unsigned int, unsigned int);
    using Wait = unsigned int (APIENTRY*)(Sync, unsigned int, std::uint64_t);
    using Delete = void (APIENTRY*)(Sync);
    Fence fence = nullptr;
    Wait wait = nullptr;
    Delete remove = nullptr;
    HGLRC context = nullptr;
    DWORD thread = 0;
    bool current() const noexcept {
        return context && context==wglGetCurrentContext() && thread==GetCurrentThreadId();
    }
};

namespace {
template<class Function> Function load(const char* name) noexcept {
    const auto address=wglGetProcAddress(name);
    const auto bits=reinterpret_cast<std::uintptr_t>(address);
    if(!address || bits<=3 || bits==static_cast<std::uintptr_t>(-1))return nullptr;
    return reinterpret_cast<Function>(address);
}
}
#else
struct OpenGlBackend::Impl {};
#endif

OpenGlBackend::OpenGlBackend() noexcept = default;
OpenGlBackend::~OpenGlBackend() = default;

bool OpenGlBackend::initialize() noexcept {
#if defined(_WIN32)
    auto impl=std::unique_ptr<Impl>(new(std::nothrow) Impl);
    if(!impl)return false;
    impl->context=wglGetCurrentContext();
    impl->thread=GetCurrentThreadId();
    if(!impl->current())return false;
    impl->fence=load<Impl::Fence>("glFenceSync");
    impl->wait=load<Impl::Wait>("glClientWaitSync");
    impl->remove=load<Impl::Delete>("glDeleteSync");
    if(!impl->fence || !impl->wait || !impl->remove)return false;
    impl_=std::move(impl);
    return true;
#else
    return false;
#endif
}

FenceHandle OpenGlBackend::create_fence() noexcept {
#if defined(_WIN32)
    // GL_SYNC_GPU_COMMANDS_COMPLETE; flags must be zero.
    if(impl_ && impl_->current())return impl_->fence(0x9117,0);
#endif
    return nullptr;
}

WaitResult OpenGlBackend::wait(FenceHandle fence,std::uint64_t timeout_ns) noexcept {
#if defined(_WIN32)
    if(impl_ && impl_->current() && fence){
        // GL_SYNC_FLUSH_COMMANDS_BIT. Completion includes preceding GL commands,
        // not a promise that the compositor has displayed the frame.
        const auto status=impl_->wait(fence,0x00000001,timeout_ns);
        if(status==0x911A || status==0x911C)return WaitResult::signaled;
        if(status==0x911B)return WaitResult::timeout;
    }
#endif
    return WaitResult::failed;
}

void OpenGlBackend::delete_fence(FenceHandle fence) noexcept {
#if defined(_WIN32)
    if(impl_ && impl_->current() && fence)impl_->remove(fence);
#endif
}

void OpenGlBackend::flush() noexcept {
#if defined(_WIN32)
    if(impl_ && impl_->current())glFlush();
#endif
}

} // namespace threadlight::gpu
