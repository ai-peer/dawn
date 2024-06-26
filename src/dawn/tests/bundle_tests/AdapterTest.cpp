// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * This is a short test which creates an instance, requests an adapter and device
 * and links with the monolithic shared library webgpu_dawn.
 * It does not draw anything.
 */

#include <cstdlib>
#include <iostream>

#include "webgpu/webgpu_cpp.h"

int main() {
    wgpu::InstanceDescriptor instanceDescriptor{};
    instanceDescriptor.features.timedWaitAnyEnable = true;
    wgpu::Instance instance = wgpu::CreateInstance(&instanceDescriptor);
    if (instance == nullptr) {
        std::cerr << "Instance creation failed!\n";
        return EXIT_FAILURE;
    }
    // Synchronously request the adapter.
    wgpu::RequestAdapterOptions options = {};
    wgpu::Adapter adapter;
    instance.WaitAny(instance.RequestAdapter(
                         &options, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                                    [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                       const char* message, void* userdata) {
                                        if (status != WGPURequestAdapterStatus_Success) {
                                            std::cerr << "Failed to get an adapter:" << message;
                                            return;
                                        }
                                        *static_cast<wgpu::Adapter*>(userdata) =
                                            wgpu::Adapter::Acquire(adapter);
                                    },
                                    &adapter}),
                     UINT64_MAX);
    if (adapter == nullptr) {
        std::cerr << "RequestAdapter failed!\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
