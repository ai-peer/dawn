// Copyright 2020 The Dawn Authors
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

#include "dawn_native/d3d12/NonShaderVisibleHeapAllocationD3D12.h"
#include "dawn_native/Error.h"

namespace dawn_native { namespace d3d12 {

    NonShaderVisibleHeapAllocation::NonShaderVisibleHeapAllocation(
        uint32_t sizeIncrement,
        D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor,
        uint32_t heapIndex)
        : mSizeIncrement(sizeIncrement), mBaseDescriptor(baseDescriptor), mHeapIndex(heapIndex) {
    }

    D3D12_CPU_DESCRIPTOR_HANDLE NonShaderVisibleHeapAllocation::GetCPUHandle(
        uint32_t offset) const {
        ASSERT(mBaseDescriptor.ptr != 0);
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mBaseDescriptor;
        cpuHandle.ptr += mSizeIncrement * offset;
        return cpuHandle;
    }

    uint32_t NonShaderVisibleHeapAllocation::GetHeapIndex() const {
        return mHeapIndex;
    }

    bool NonShaderVisibleHeapAllocation::IsValid() const {
        return mBaseDescriptor.ptr != 0;
    }

}}  // namespace dawn_native::d3d12