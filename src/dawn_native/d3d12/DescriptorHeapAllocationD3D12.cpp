
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

#include "dawn_native/d3d12/DescriptorHeapAllocationD3D12.h"

#include "common/Assert.h"

namespace dawn_native { namespace d3d12 {

    DescriptorHeapAllocation::DescriptorHeapAllocation()
        : mDescriptorHeap(nullptr), mSizeIncrement(0), mOffset(0), mSerial(0) {
    }

    DescriptorHeapAllocation::DescriptorHeapAllocation(ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                       uint32_t sizeIncrement,
                                                       uint64_t offset,
                                                       Serial serial)
        : mDescriptorHeap(descriptorHeap),
          mSizeIncrement(sizeIncrement),
          mOffset(offset),
          mSerial(serial) {
    }

    ID3D12DescriptorHeap* DescriptorHeapAllocation::Get() const {
        return mDescriptorHeap.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapAllocation::GetCPUHandle(uint32_t index) const {
        ASSERT(mDescriptorHeap);
        auto handle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += mSizeIncrement * (index + mOffset);
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapAllocation::GetGPUHandle(uint32_t index) const {
        ASSERT(mDescriptorHeap);
        auto handle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += mSizeIncrement * (index + mOffset);
        return handle;
    }

    uint64_t DescriptorHeapAllocation::GetOffset() const {
        return mOffset;
    }

    Serial DescriptorHeapAllocation::GetSerial() const {
        return mSerial;
    }

    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapAllocation::GetType() const {
        ASSERT(mDescriptorHeap);
        return mDescriptorHeap->GetDesc().Type;
    }
}}  // namespace dawn_native::d3d12