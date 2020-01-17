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

#include "dawn_native/d3d12/DescriptorAllocatorManagerD3D12.h"

#include "common/BitSetIterator.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    DescriptorAllocatorManager::DescriptorAllocatorManager(Device* device)
        : mSizeIncrements{
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
          } {
        mHeapAllocator = std::make_unique<DescriptorHeapAllocator2>(device);
        mShaderVisibleDescriptorAllocator =
            std::make_unique<ShaderVisibleDescriptorAllocator>(mHeapAllocator.get());
    }

    MaybeError DescriptorAllocatorManager::Initialize() {
        ASSERT(mShaderVisibleDescriptorAllocator->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ==
               nullptr);
        ASSERT(mShaderVisibleDescriptorAllocator->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) ==
               nullptr);
        DAWN_TRY(AllocateShaderVisibleHeaps());
        return {};
    }

    MaybeError DescriptorAllocatorManager::AllocateShaderVisibleHeaps() {
        DAWN_TRY(mShaderVisibleDescriptorAllocator->AllocateHeap(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        DAWN_TRY(
            mShaderVisibleDescriptorAllocator->AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));

        return {};
    }

    ResultOrError<bool> DescriptorAllocatorManager::AllocateBindGroups(
        const std::bitset<kMaxBindGroups>& bindGroupsToAllocate,
        const std::bitset<kMaxBindGroups>& bindGroupsLayout,
        const std::array<BindGroupBase*, kMaxBindGroups>& bindGroups,
        ID3D12GraphicsCommandList* commandList) {
        // Rather than allocate bindgroup-by-bindgroup and have the caller deal with failure should
        // the heap become full, this attempts to first allocate dirty bindgroups on the same heap
        // before creating a new heap with the bindgroups needed by the BindGroupLayout. This
        // approach does not know upfront if an overflow could occur and instead defers until
        // the last one fails to allocate before re-trying them all on the new heap. As a
        // consequence, it causes lots of duplicated bindgroup allocations for smaller heaps but in
        // exchange avoids costly counting the total size needed (space vs perf).
        bool didCreateBindGroups = true;
        bool didReallocation = false;
        for (uint32_t index : IterateBitSet(bindGroupsToAllocate)) {
            DAWN_TRY_ASSIGN(didCreateBindGroups, ToBackend(bindGroups[index])->Create());
            if (!didCreateBindGroups) {
                break;
            }
        }

        // This will re-create bindgroups for both heaps even if only one overflowed.
        // TODO(bryan.bernhart@intel.com): Consider re-allocating heaps independently
        // such that overflowing one doesn't re-allocate the another.
        if (!didCreateBindGroups) {
            DAWN_TRY(AllocateShaderVisibleHeaps());
            didReallocation = true;
            for (uint32_t index : IterateBitSet(bindGroupsLayout)) {
                DAWN_TRY_ASSIGN(didCreateBindGroups, ToBackend(bindGroups[index])->Create());
                ASSERT(didCreateBindGroups);
            }
        }
        return didReallocation;
    }

    ResultOrError<DescriptorHeapAllocation> DescriptorAllocatorManager::AllocateMemory(
        uint32_t descriptorCount,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        DescriptorHeapAllocation allocation;
        switch (heapType) {
            // Allocate memory from shader-visible descriptor heaps.
            case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: {
                DAWN_TRY_ASSIGN(allocation, mShaderVisibleDescriptorAllocator->Allocate(
                                                descriptorCount, heapType));
                allocation =
                    DescriptorHeapAllocation{allocation.Get(), mSizeIncrements[heapType],
                                             allocation.GetOffset(), allocation.GetSerial()};
            } break;
            // Allocate memory from non shader-visible descriptor heaps.
            case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: {
                // TODO(bryan.bernhart@intel.com): Support sub-allocation optimization.
                ComPtr<ID3D12DescriptorHeap> heap;
                DAWN_TRY_ASSIGN(heap,
                                mHeapAllocator->AllocateDescriptorHeap(
                                    descriptorCount, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, heapType));
                allocation = DescriptorHeapAllocation{std::move(heap), mSizeIncrements[heapType],
                                                      /*offset*/ 0, /*serial*/ 0};
                mHeapAllocator->DeallocateDescriptorHeap(allocation.Get());
            } break;
            default:
                UNREACHABLE();
        }
        return allocation;
    }

    std::array<ID3D12DescriptorHeap*, 2> DescriptorAllocatorManager::GetShaderVisibleHeaps() const {
        return {mShaderVisibleDescriptorAllocator->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
                mShaderVisibleDescriptorAllocator->GetHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)};
    }

    void DescriptorAllocatorManager::Tick(uint64_t completedSerial) {
        mShaderVisibleDescriptorAllocator->Deallocate(completedSerial);
    }

    bool DescriptorAllocatorManager::IsBindGroupValid(
        const DescriptorHeapAllocation& bindGroupAllocation) const {
        return mShaderVisibleDescriptorAllocator->IsValid(bindGroupAllocation);
    }
}}  // namespace dawn_native::d3d12