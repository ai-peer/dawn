// Copyright 2019 The Dawn Authors
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

#include "dawn_native/d3d12/ResourceAllocatorManagerD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

#include "common/Math.h"

namespace dawn_native { namespace d3d12 {

    ResourceAllocatorManager::ResourceAllocatorManager(Device* device) : mDevice(device) {
    }

    ResultOrError<ResourceHeapAllocation> ResourceAllocatorManager::AllocateMemory(
        D3D12_HEAP_TYPE heapType,
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        D3D12_RESOURCE_STATES initialUsage) {
        // TODO(bryan.bernhart@intel.com): Conditionally disable sub-allocation.
        //
        // For very large resources, there is no benefit to suballocate.
        // For very small resources, it is inefficent to suballocate given the min. heap
        // size could be much larger.
        ResourceHeapAllocation subAllocation;
        DAWN_TRY_ASSIGN(subAllocation,
                        SubAllocateMemory(heapType, resourceDescriptor, initialUsage,
                                          GetD3D12HeapFlags(resourceDescriptor.Dimension)));
        if (subAllocation.GetInfo().mMethod != AllocationMethod::kInvalid) {
            return subAllocation;
        }

        // Should sub-allocation fail, fall-back to direct allocation (aka CreateCommittedResource).

        const size_t heapTypeIndex = GetD3D12HeapTypeToIndex(heapType);
        ASSERT(heapTypeIndex < kNumHeapTypes);

        CommittedResourceAllocator* allocator = mDirectResourceAllocators[heapTypeIndex].get();
        if (allocator == nullptr) {
            mDirectResourceAllocators[heapTypeIndex] =
                std::make_unique<CommittedResourceAllocator>(mDevice, heapType);
            allocator = mDirectResourceAllocators[heapTypeIndex].get();
        }

        ResourceHeapAllocation directAllocation;
        DAWN_TRY_ASSIGN(directAllocation, allocator->Allocate(resourceDescriptor, initialUsage));

        return directAllocation;
    }

    size_t ResourceAllocatorManager::GetD3D12HeapTypeToIndex(D3D12_HEAP_TYPE heapType) const {
        ASSERT(heapType > 0);
        ASSERT(static_cast<uint32_t>(heapType) <= kNumHeapTypes);
        return heapType - 1;
    }

    D3D12_HEAP_FLAGS ResourceAllocatorManager::GetD3D12HeapFlags(
        D3D12_RESOURCE_DIMENSION dimension) const {
        // Heap flags are used to specify the type of resources the heap contains (buffer/
        // texture).
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_heap_flags
        //
        // Heap tier 2 allows mixing of resource type in the same heap.
        // Mixed resources enables better heap re-use as it avoids the need for
        // separate heap-based allocator per resource type.
        //
        // TODO(bryan.bernhart@intel.com): Support for heap tier 2.
        switch (dimension) {
            case D3D12_RESOURCE_DIMENSION_BUFFER:
                return D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES |
                       D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            // TODO(bryan.bernhart@intel.com): Support storing textures in heaps.
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            default:
                UNREACHABLE();
        }
    }

    void ResourceAllocatorManager::DeallocateMemory(ResourceHeapAllocation& allocation) {
        if (allocation.GetInfo().mMethod == AllocationMethod::kInvalid) {
            return;
        }

        D3D12_HEAP_PROPERTIES heapProp;
        allocation.GetD3D12Resource()->GetHeapProperties(&heapProp, nullptr);

        const size_t heapTypeIndex = GetD3D12HeapTypeToIndex(heapProp.Type);
        ASSERT(heapTypeIndex < kNumHeapTypes);

        if (allocation.GetInfo().mMethod == AllocationMethod::kDirect) {
            CommittedResourceAllocator* allocator = mDirectResourceAllocators[heapTypeIndex].get();
            ASSERT(allocator != nullptr);
            allocator->Deallocate(allocation);

        } else if (allocation.GetInfo().mMethod == AllocationMethod::kSubAllocated) {
            const size_t heapLevel = GetHeapLevelFromHeapSize(allocation.GetInfo().mMemorySize);
            const D3D12_HEAP_FLAGS heapFlags =
                static_cast<D3D12_HEAP_FLAGS>(allocation.GetInfo().mMemoryFlags);
            BuddyPlacedResourceAllocator* allocator =
                mSubAllocatedResourceAllocators[heapFlags][heapTypeIndex][heapLevel].get();
            ASSERT(allocator != nullptr);
            allocator->Deallocate(allocation);

        } else {
            UNREACHABLE();
        }

        // Invalidate the allocation in case one accidentally
        // calls DeallocateMemory again using the same allocation.
        allocation.Invalidate();
    }

    ResultOrError<ResourceHeapAllocation> ResourceAllocatorManager::SubAllocateMemory(
        D3D12_HEAP_TYPE heapType,
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        D3D12_RESOURCE_STATES initialUsage,
        D3D12_HEAP_FLAGS heapFlags) {
        // Create placed resource sub-allocators backed by heaps in power-of-two sizes.
        const size_t heapTypeIndex = GetD3D12HeapTypeToIndex(heapType);
        if (mSubAllocatedResourceAllocators.find(heapFlags) ==
            mSubAllocatedResourceAllocators.end()) {
            std::array<PlacedResourceAllocators, kNumHeapTypes> placedResourceAllocators;
            placedResourceAllocators[heapTypeIndex] = CreatePlacedResourceAllocators(heapType);

            mSubAllocatedResourceAllocators.insert(
                std::pair<D3D12_HEAP_FLAGS, std::array<PlacedResourceAllocators, kNumHeapTypes>>(
                    heapFlags, std::move(placedResourceAllocators)));

        } else if (mSubAllocatedResourceAllocators[heapFlags][heapTypeIndex].empty()) {
            mSubAllocatedResourceAllocators[heapFlags][heapTypeIndex] =
                CreatePlacedResourceAllocators(heapType);
        }

        const D3D12_RESOURCE_ALLOCATION_INFO resourceInfo =
            mDevice->GetD3D12Device()->GetResourceAllocationInfo(0, 1, &resourceDescriptor);

        // Note: BuddyPlacedResourceAllocator uses the buddy system which requires the allocation
        // to be a power-of-two size. As a consequence, this adds internal fragmentation (unused
        // bytes) in exchange for reducing external fragmentation.
        const uint64_t allocationSize = NextPowerOfTwo(resourceInfo.SizeInBytes);

        // TODO(bryan.bernhart@intel.com): Adjust target heap size based on
        // a heuristic. Smaller but frequent allocations benefit by sub-allocating from a larger
        // heap. Should allocationSize equal heapSize, sub-allocation has no performance benefit.
        const size_t heapLevel = GetHeapLevelFromHeapSize(allocationSize);

        // Gracefully fail should the heap size exceeds limit.
        if (heapLevel >= mSubAllocatedResourceAllocators[heapFlags][heapTypeIndex].size()) {
            return ResourceHeapAllocation{};  // invalid
        }

        BuddyPlacedResourceAllocator* allocator =
            mSubAllocatedResourceAllocators[heapFlags][heapTypeIndex][heapLevel].get();

        ResourceHeapAllocation allocation;
        DAWN_TRY_ASSIGN(allocation,
                        allocator->Allocate(resourceDescriptor, allocationSize,
                                            resourceInfo.Alignment, initialUsage, heapFlags));
        return allocation;
    }

    std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>>
    ResourceAllocatorManager::CreatePlacedResourceAllocators(D3D12_HEAP_TYPE heapType) const {
        std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>> allocators;
        for (uint64_t heapSize = kMinHeapSize; heapSize <= kMaxHeapSize; heapSize *= 2) {
            std::unique_ptr<BuddyPlacedResourceAllocator> allocator =
                std::make_unique<BuddyPlacedResourceAllocator>(kMaxHeapSize, heapSize, mDevice,
                                                               heapType);
            allocators.push_back(std::move(allocator));
        }
        return allocators;
    }

    size_t ResourceAllocatorManager::GetHeapLevelFromHeapSize(uint64_t heapSize) const {
        return Log2(heapSize) - Log2(kMinHeapSize);
    }

}}  // namespace dawn_native::d3d12
