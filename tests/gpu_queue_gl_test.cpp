#include "gpu_queue_gl.h"
#include <iostream>

int main() {
    // This process deliberately creates no window or GL context. Missing
    // support must fail safely before any extension function can be called.
    threadlight::gpu::OpenGlBackend backend;
    if(backend.initialize() || backend.initialize() || backend.create_fence()!=nullptr ||
       backend.wait(nullptr,0)!=threadlight::gpu::WaitResult::failed){
        std::cerr<<"GL adapter accepted a missing context\n";
        return 1;
    }
    backend.delete_fence(nullptr);
    backend.flush();
    std::cout<<"GL adapter rejects missing context safely\n";
}
