// Copyright 2024 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_NATIVE_D3D12_INTELEXTENSIOND3D12_H_
#define SRC_DAWN_NATIVE_D3D12_INTELEXTENSIOND3D12_H_

// initguid.h must be declared before devguid.h used in IntelExtensionD3D12.cpp
#include <initguid.h>

#include <memory>

#include "dawn/native/d3d12/d3d12_platform.h"

namespace dawn::native::d3d12 {

class PhysicalDevice;

class IntelExtension {
  public:
    static std::unique_ptr<IntelExtension> Create(const PhysicalDevice& adapter);
    virtual ~IntelExtension() = default;

    virtual HRESULT CreateCommandQueueWithMaxPerformanceThrottlePolicy(
        D3D12_COMMAND_QUEUE_DESC* d3d12CommandQueueDesc,
        REFIID riid,
        void** ppCommandQueue) = 0;
};

}  // namespace dawn::native::d3d12

#endif
