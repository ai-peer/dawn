// Copyright 2022 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
