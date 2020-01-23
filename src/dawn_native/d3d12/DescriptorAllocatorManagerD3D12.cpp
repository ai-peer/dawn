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
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

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

    D3D12_DESCRIPTOR_HEAP_FLAGS GetD3D12HeapFlags(D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        switch (heapType) {
            case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
                return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
                return D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            default:
                UNREACHABLE();
        }
    }

    DescriptorAllocatorManager::DescriptorAllocatorManager(Device* device)
        : mDevice(device),
          mSizeIncrements{
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
          },
          mLastCompletedSerial(0) {
    }

    MaybeError DescriptorAllocatorManager::Initialize() {
        ASSERT(mShaderVisibleBuffers[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].heap.Get() == nullptr);
        ASSERT(mShaderVisibleBuffers[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].heap.Get() == nullptr);
        DAWN_TRY(AllocateShaderVisibleHeaps());
        return {};
    }

    MaybeError DescriptorAllocatorManager::AllocateShaderVisibleHeaps() {
        // TODO(bryan.bernhart@intel.com): Allocating to max heap size wastes memory
        // should the developer not allocate any bindings for the heap type.
        // Consider dynamically re-sizing GPU heaps.
        DAWN_TRY(
            AllocateGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                            GetD3D12ShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
                            GetD3D12HeapFlags(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));
        DAWN_TRY(AllocateGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                                 GetD3D12ShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
                                 GetD3D12HeapFlags(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)));
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
                // Invalidate bindgroup allocations to ensure they will be re-allocated on the
                // newest shader visible heaps.
                ToBackend(bindGroups[index])->Invalidate();

                DAWN_TRY_ASSIGN(didCreateBindGroups, ToBackend(bindGroups[index])->Create());
                ASSERT(didCreateBindGroups);
            }
        }
        return didReallocation;
    }

    ResultOrError<DescriptorHeapAllocation> DescriptorAllocatorManager::AllocateDescriptors(
        uint32_t descriptorCount,
        Serial pendingSerial,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        DescriptorHeapAllocation allocation;
        switch (heapType) {
            // Allocate memory from shader-visible descriptor heaps.
            case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: {
                ASSERT(mShaderVisibleBuffers[heapType].heap != nullptr);
                const uint64_t startOffset = mShaderVisibleBuffers[heapType].allocator.Allocate(
                    descriptorCount, pendingSerial);
                if (descriptorCount > 0 && startOffset == RingBufferAllocator::kInvalidOffset) {
                    return DescriptorHeapAllocation{};  // Invalid
                }

                return DescriptorHeapAllocation{mShaderVisibleBuffers[heapType].heap.Get(),
                                                mSizeIncrements[heapType], startOffset,
                                                pendingSerial};
            } break;
            // Allocate memory from non shader-visible descriptor heaps.
            case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: {
                // TODO(bryan.bernhart@intel.com): Support sub-allocation optimization.
                ComPtr<ID3D12DescriptorHeap> heap;
                DAWN_TRY_ASSIGN(heap, CreateDescriptorHeap(descriptorCount,
                                                           GetD3D12HeapFlags(heapType), heapType));
                allocation = DescriptorHeapAllocation{std::move(heap), mSizeIncrements[heapType],
                                                      /*offset*/ 0, /* serial*/ 0};
                mDevice->ReferenceUntilUnused(heap);
            } break;
            default:
                UNREACHABLE();
        }
        return allocation;
    }

    std::array<ID3D12DescriptorHeap*, 2> DescriptorAllocatorManager::GetShaderVisibleHeaps() const {
        return {mShaderVisibleBuffers[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].heap.Get(),
                mShaderVisibleBuffers[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].heap.Get()};
    }

    void DescriptorAllocatorManager::Tick(uint64_t completedSerial) {
        for (uint32_t i = 0; i < mShaderVisibleBuffers.size(); i++) {
            ASSERT(mShaderVisibleBuffers[i].heap != nullptr);
            mShaderVisibleBuffers[i].allocator.Deallocate(completedSerial);
        }

        // Ringbuffer does not invalidate the allocations but only the memory block, which means the
        // Bindgroup cannot know if the allocations are deallocated upon Tick().
        mLastCompletedSerial = completedSerial;
    }

    bool DescriptorAllocatorManager::IsAllocationValid(const BindGroup* group) const {
        return IsShaderVisibleAllocationValid(group->GetCbvUavSrvHeapAllocation()) &&
               IsShaderVisibleAllocationValid(group->GetSamplerHeapAllocation());
    }

    // Called by Bindgroup to check if allocation was invalidated on Tick().
    bool DescriptorAllocatorManager::IsShaderVisibleAllocationValid(
        const DescriptorHeapAllocation& allocation) const {
        if (allocation.Get() == nullptr) {
            return false;
        }
        return allocation.GetSerial() > mLastCompletedSerial;
    }

    // Creates a GPU descriptor heap that manages descriptors in a FIFO queue.
    MaybeError DescriptorAllocatorManager::AllocateGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                                           uint32_t heapSize,
                                                           D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags) {
        if (mShaderVisibleBuffers[heapType].heap != nullptr) {
            mDevice->ReferenceUntilUnused(mShaderVisibleBuffers[heapType].heap);
        }

        ComPtr<ID3D12DescriptorHeap> heap;
        DAWN_TRY_ASSIGN(heap, CreateDescriptorHeap(heapSize, heapFlags, heapType));

        // Record the recently allocated heap and the current heap.
        mShaderVisibleBuffers[heapType].heap = std::move(heap);
        mShaderVisibleBuffers[heapType].allocator = RingBufferAllocator(heapSize);
        return {};
    }

    ResultOrError<ComPtr<ID3D12DescriptorHeap>> DescriptorAllocatorManager::CreateDescriptorHeap(
        uint32_t heapSize,
        D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
        heapDescriptor.Type = heapType;
        heapDescriptor.NumDescriptors = heapSize;
        heapDescriptor.Flags = heapFlags;
        heapDescriptor.NodeMask = 0;
        ComPtr<ID3D12DescriptorHeap> heap;
        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->CreateDescriptorHeap(&heapDescriptor, IID_PPV_ARGS(&heap)),
            "ID3D12Device::CreateDescriptorHeap"));
        return heap;
    }
}}  // namespace dawn_native::d3d12