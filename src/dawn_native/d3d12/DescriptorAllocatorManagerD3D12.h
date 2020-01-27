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

#ifndef DAWNNATIVE_D3D12_DESCRIPTORALLOCATORMANAGER_H_
#define DAWNNATIVE_D3D12_DESCRIPTORALLOCATORMANAGER_H_

#include "dawn_native/RingBufferAllocator.h"
#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "dawn_native/d3d12/DescriptorHeapAllocationD3D12.h"

#include <array>

namespace dawn_native { namespace d3d12 {

    class Device;

    // Manages descriptor heap allocators used by the device to create descriptors using allocation
    // method based on the heap type.
    class DescriptorAllocatorManager {
      public:
        DescriptorAllocatorManager(Device* device);
        MaybeError Initialize();

        ResultOrError<DescriptorHeapAllocation> AllocateDescriptors(
            uint32_t descriptorCount,
            Serial pendingSerial,
            D3D12_DESCRIPTOR_HEAP_TYPE heapType);

        void Tick(uint64_t completedSerial);

        std::array<ID3D12DescriptorHeap*, 2> GetShaderVisibleHeaps() const;
        MaybeError AllocateShaderVisibleHeaps();

      private:
        MaybeError AllocateGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                   uint32_t descriptorCount,
                                   D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags);

        ResultOrError<ComPtr<ID3D12DescriptorHeap>> CreateDescriptorHeap(
            uint32_t descriptorCount,
            D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags,
            D3D12_DESCRIPTOR_HEAP_TYPE heapType);

        struct ShaderVisibleBuffer {
            ComPtr<ID3D12DescriptorHeap> heap;
            RingBufferAllocator allocator;
        };

        Device* mDevice;

        std::array<ShaderVisibleBuffer, 2> mShaderVisibleBuffers;
        std::array<uint32_t, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> mSizeIncrements;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_DESCRIPTORALLOCATORMANAGER_H_