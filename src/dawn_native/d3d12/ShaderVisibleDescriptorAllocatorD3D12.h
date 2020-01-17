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

#ifndef DAWNNATIVE_D3D12_SHADERVISIBLEDESCRIPTORALLOCATORD3D12_H_
#define DAWNNATIVE_D3D12_SHADERVISIBLEDESCRIPTORALLOCATORD3D12_H_

#include "dawn_native/Error.h"
#include "dawn_native/RingBufferAllocator.h"
#include "dawn_native/d3d12/DescriptorHeapAllocationD3D12.h"

#include <array>

namespace dawn_native { namespace d3d12 {

    class DescriptorHeapAllocator2;

    // GPU descriptor heap types.
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/non-shader-visible-descriptor-heaps
    enum DescriptorHeapType {
        Shader_Visible_CbvUavSrv,
        Shader_Visible_Sampler,
        ShaderVisible_EnumCount
    };

    // Sub-allocates a GPU descriptor heap by using a ringbuffer.
    class ShaderVisibleDescriptorAllocator {
      public:
        ShaderVisibleDescriptorAllocator(DescriptorHeapAllocator2* heapAllocator);
        ~ShaderVisibleDescriptorAllocator() = default;

        MaybeError AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType);

        ResultOrError<DescriptorHeapAllocation> Allocate(uint32_t allocationSize,
                                                         D3D12_DESCRIPTOR_HEAP_TYPE type);

        void Deallocate(uint64_t completedSerial);

        bool IsValid(const DescriptorHeapAllocation& bindGroupAllocation) const;

        ID3D12DescriptorHeap* GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const;

      private:
        struct RingBuffer {
            ComPtr<ID3D12DescriptorHeap> heap;
            RingBufferAllocator allocator;
            Serial heapSerial = 0;
        };
        std::array<RingBuffer, DescriptorHeapType::ShaderVisible_EnumCount> mRingBuffer;
        DescriptorHeapAllocator2* mHeapAllocator;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_SHADERVISIBLEDESCRIPTORALLOCATORD3D12_H_