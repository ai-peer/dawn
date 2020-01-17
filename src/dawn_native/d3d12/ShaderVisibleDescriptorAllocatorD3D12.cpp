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

#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocatorD3D12.h"
#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/DescriptorHeapAllocatorD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {
        DescriptorHeapType GetDescriptorHeapType(D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
            switch (heapType) {
                case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
                    return Shader_Visible_CbvUavSrv;
                case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
                    return Shader_Visible_Sampler;
                default:
                    UNREACHABLE();
            }
        }

        uint32_t GetD3D12ShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
            switch (heapType) {
                case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
                    return D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
                case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
                    return D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
                default:
                    UNREACHABLE();
            }
        }
    }  // namespace

    ShaderVisibleDescriptorAllocator::ShaderVisibleDescriptorAllocator(
        DescriptorHeapAllocator2* heapAllocator)
        : mHeapAllocator(heapAllocator) {
    }

    // Creates a GPU descriptor heap that manages descriptors in a FIFO queue.
    MaybeError ShaderVisibleDescriptorAllocator::AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        const size_t descriptorHeapTypeIndex = static_cast<size_t>(GetDescriptorHeapType(heapType));

        if (mRingBuffer[descriptorHeapTypeIndex].heap != nullptr) {
            mHeapAllocator->DeallocateDescriptorHeap(mRingBuffer[descriptorHeapTypeIndex].heap);
        }

        // TODO(bryan.bernhart@intel.com): Allocating to max heap size wastes memory
        // should the developer not allocate any bindings for the heap type.
        // Consider dynamically re-sizing GPU heaps.
        const uint32_t heapSize = GetD3D12ShaderVisibleHeapSize(heapType);

        ComPtr<ID3D12DescriptorHeap> heap;
        DAWN_TRY_ASSIGN(heap, mHeapAllocator->AllocateDescriptorHeap(
                                  heapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, heapType));

        // Record the recently allocated heap and the current heap.
        mRingBuffer[descriptorHeapTypeIndex].heap = std::move(heap);
        mRingBuffer[descriptorHeapTypeIndex].allocator = RingBufferAllocator(heapSize);
        mRingBuffer[descriptorHeapTypeIndex].heapSerial++;

        return {};
    }

    ResultOrError<DescriptorHeapAllocation> ShaderVisibleDescriptorAllocator::Allocate(
        uint32_t allocationSize,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        const size_t descriptorHeapTypeIndex = static_cast<size_t>(GetDescriptorHeapType(heapType));
        ASSERT(mRingBuffer[descriptorHeapTypeIndex].heap != nullptr);
        const uint64_t startOffset = mRingBuffer[descriptorHeapTypeIndex].allocator.Allocate(
            allocationSize, mRingBuffer[descriptorHeapTypeIndex].heapSerial);
        if (allocationSize > 0 && startOffset == RingBufferAllocator::kInvalidOffset) {
            return DescriptorHeapAllocation{};  // Invalid
        }

        return DescriptorHeapAllocation{mRingBuffer[descriptorHeapTypeIndex].heap.Get(), 0,
                                        startOffset,
                                        mRingBuffer[descriptorHeapTypeIndex].heapSerial};
    }

    void ShaderVisibleDescriptorAllocator::Deallocate(uint64_t completedSerial) {
        for (uint32_t i = 0; i < mRingBuffer.size(); i++) {
            ASSERT(mRingBuffer[i].heap != nullptr);
            mRingBuffer[i].allocator.Deallocate(completedSerial);
        }
    }

    bool ShaderVisibleDescriptorAllocator::IsValid(
        const DescriptorHeapAllocation& allocation) const {
        // Bindgroup's allocations do not get invalidated upon Deallocate().
        // To determine if the allocation exists, a heap serial is remembered.
        // This works because the bindgroup allocation cannot outlive this serial while ensuring
        // the bindgroup allocation can be uniquely identified.
        if (allocation.Get() == nullptr) {
            return false;
        }
        const size_t descriptorHeapTypeIndex =
            static_cast<size_t>(GetDescriptorHeapType(allocation.GetType()));
        return (allocation.GetSerial() == mRingBuffer[descriptorHeapTypeIndex].heapSerial);
    }

    ID3D12DescriptorHeap* ShaderVisibleDescriptorAllocator::GetHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType) const {
        const size_t descriptorHeapTypeIndex = static_cast<size_t>(GetDescriptorHeapType(heapType));
        return mRingBuffer[descriptorHeapTypeIndex].heap.Get();
    }
}}  // namespace dawn_native::d3d12