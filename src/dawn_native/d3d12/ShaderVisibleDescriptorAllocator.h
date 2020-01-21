// Copyright 2017 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_SHADERVISIBLEDESCRIPTORALLOCATOR_H_
#define DAWNNATIVE_D3D12_SHADERVISIBLEDESCRIPTORALLOCATOR_H_

#include "dawn_native/d3d12/d3d12_platform.h"

#include <array>
#include <vector>
#include "common/SerialQueue.h"

#include "dawn_native/Error.h"
#include "dawn_native/RingBufferAllocator.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    struct ShaderVisibleDescriptorAllocation {
        D3D12_CPU_DESCRIPTOR_HANDLE baseCPUHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE baseGPUHandle;
        uint32_t sizeIncrement;

        bool IsValid() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    };

    class ShaderVisibleDescriptorAllocator {
      public:
        ShaderVisibleDescriptorAllocator(Device* device);

        MaybeError Initialize();
        void Tick(uint64_t lastCompletedSerial);

        MaybeError EnsureSpaceForFullPipelineLayout();

        ShaderVisibleDescriptorAllocation AllocateCbvUavSrcDescriptors(uint32_t count);
        ShaderVisibleDescriptorAllocation AllocateSamplerDescriptors(uint32_t count);

        struct Heaps {
            ID3D12DescriptorHeap* cbvUavSrvHeap;
            ID3D12DescriptorHeap* samplerHeap;
        };
        Heaps GetCurrentHeaps() const;

        struct HeapSerials {
            Serial cbvUavSrvSerial;
            Serial samplerSerial;
        };
        HeapSerials GetCurrentHeapSerials() const;

      private:
        Device* mDevice;

        struct DescriptorHeapInfo {
            ComPtr<ID3D12DescriptorHeap> heap = nullptr;
            ShaderVisibleDescriptorAllocation wholeHeap;
            // TODO replace WITH SOMETHING ELSE
            RingBufferAllocator ringBuffer;
            Serial serial = 0;
        };

        DescriptorHeapInfo mCbvUavSrvHeap;
        DescriptorHeapInfo mSamplerHeap;

        ShaderVisibleDescriptorAllocation AllocateDecriptor(
            DescriptorHeapInfo* info,
            uint32_t count);

        MaybeError RecreateHeap(DescriptorHeapInfo* info,
                                D3D12_DESCRIPTOR_HEAP_TYPE type,
                                uint32_t allocationSize);
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_SHADERVISIBLEDESCRIPTORALLOCATOR_H_
