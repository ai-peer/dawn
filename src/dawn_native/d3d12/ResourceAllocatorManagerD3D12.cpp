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

#include "common/Math.h"

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapAllocatorD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"
#include "dawn_native/d3d12/ResourceAllocatorManagerD3D12.h"

namespace dawn_native { namespace d3d12 {
    namespace {
        D3D12_HEAP_TYPE GetD3D12HeapType(ResourceHeapKind resourceHeapKind) {
            switch (resourceHeapKind) {
                case Readback_BuffersOnly:
                    return D3D12_HEAP_TYPE_READBACK;
                case Default_BuffersOnly:
                case Default_TexturesOnly:
                case Default_RenderableTexturesOrDepthOnly:
                    return D3D12_HEAP_TYPE_DEFAULT;
                case Upload_BuffersOnly:
                    return D3D12_HEAP_TYPE_UPLOAD;
                default:
                    UNREACHABLE();
            }
        }

        D3D12_HEAP_FLAGS GetD3D12HeapFlags(ResourceHeapKind resourceHeapKind) {
            switch (resourceHeapKind) {
                case Readback_BuffersOnly:
                case Default_BuffersOnly:
                case Upload_BuffersOnly:
                    return D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES |
                           D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
                case Default_TexturesOnly:
                    return D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
                case Default_RenderableTexturesOrDepthOnly:
                    return D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
                default:
                    UNREACHABLE();
            }
        }

        ResourceHeapKind GetResourceHeapKind(D3D12_RESOURCE_DIMENSION dimension,
                                             D3D12_HEAP_TYPE heapType,
                                             D3D12_RESOURCE_FLAGS flags) {
            switch (dimension) {
                case D3D12_RESOURCE_DIMENSION_BUFFER: {
                    switch (heapType) {
                        case D3D12_HEAP_TYPE_UPLOAD:
                            return Upload_BuffersOnly;
                        case D3D12_HEAP_TYPE_DEFAULT:
                            return Default_BuffersOnly;
                        case D3D12_HEAP_TYPE_READBACK:
                            return Readback_BuffersOnly;
                        default:
                            UNREACHABLE();
                    }
                } break;
                case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
                case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
                case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
                    switch (heapType) {
                        case D3D12_HEAP_TYPE_DEFAULT: {
                            if ((flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ||
                                (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
                                return Default_RenderableTexturesOrDepthOnly;
                            } else {
                                return Default_TexturesOnly;
                            }
                        } break;
                        default:
                            UNREACHABLE();
                    }
                } break;
                default:
                    UNREACHABLE();
            }
        }
    }  // namespace

    ResourceAllocatorManager::ResourceAllocatorManager(Device* device) : mDevice(device) {
        for (uint32_t i = 0; i < ResourceHeapKind::EnumCount; i++) {
            const ResourceHeapKind resourceHeapKind = static_cast<ResourceHeapKind>(i);
            mSubAllocatedResourceAllocators[i] = CreatePlacedResourceAllocators(
                GetD3D12HeapType(resourceHeapKind), GetD3D12HeapFlags(resourceHeapKind));
        }
    }

    ResultOrError<ResourceHeapAllocation> ResourceAllocatorManager::AllocateMemory(
        D3D12_HEAP_TYPE heapType,
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        D3D12_RESOURCE_STATES initialUsage) {
        // Attempt to satisfy the request using sub-allocation (placed resource in a heap).
        ResourceHeapAllocation subAllocation;
        DAWN_TRY_ASSIGN(subAllocation,
                        SubAllocateMemory(heapType, resourceDescriptor, initialUsage));
        if (subAllocation.GetInfo().mMethod != AllocationMethod::kInvalid) {
            return subAllocation;
        }

        // If sub-allocation fails, fall-back to direct allocation (commited resource).

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

    void ResourceAllocatorManager::Tick(Serial completedSerial) {
        for (ResourceHeapAllocation& allocation :
             mAllocationsToDelete.IterateUpTo(completedSerial)) {
            FreeMemory(allocation);
        }
        mAllocationsToDelete.ClearUpTo(completedSerial);
    }

    void ResourceAllocatorManager::DeallocateMemory(ResourceHeapAllocation& allocation) {
        if (allocation.GetInfo().mMethod == AllocationMethod::kInvalid) {
            return;
        }

        mAllocationsToDelete.Enqueue(allocation, mDevice->GetPendingCommandSerial());

        // Invalidate the allocation immediately in case one accidentally
        // calls DeallocateMemory again using the same allocation.
        allocation.Invalidate();
    }

    void ResourceAllocatorManager::FreeMemory(ResourceHeapAllocation& allocation) {
        D3D12_HEAP_PROPERTIES heapProp;
        allocation.GetD3D12Resource()->GetHeapProperties(&heapProp, nullptr);

        switch (allocation.GetInfo().mMethod) {
            case AllocationMethod::kDirect: {
                const size_t heapTypeIndex = GetD3D12HeapTypeToIndex(heapProp.Type);
                ASSERT(heapTypeIndex < kNumHeapTypes);

                CommittedResourceAllocator* allocator =
                    mDirectResourceAllocators[heapTypeIndex].get();
                ASSERT(allocator != nullptr);
                allocator->Deallocate(allocation);
            } break;

            case AllocationMethod::kSubAllocated: {
                const D3D12_RESOURCE_DESC resourceDescriptor =
                    allocation.GetD3D12Resource()->GetDesc();

                const size_t resourceHeapKindIndex = GetResourceHeapKind(
                    resourceDescriptor.Dimension, heapProp.Type, resourceDescriptor.Flags);
                const size_t heapLevel = GetHeapLevelFromHeapSize(allocation.GetInfo().mMemorySize);

                BuddyPlacedResourceAllocator* allocator =
                    mSubAllocatedResourceAllocators[resourceHeapKindIndex][heapLevel].get();
                ASSERT(allocator != nullptr);
                allocator->Deallocate(allocation);
            } break;

            default:
                UNREACHABLE();
        }
    }

    ResultOrError<ResourceHeapAllocation> ResourceAllocatorManager::SubAllocateMemory(
        D3D12_HEAP_TYPE heapType,
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        D3D12_RESOURCE_STATES initialUsage) {
        // TODO(bryan.bernhart@intel.com): Conditionally disable sub-allocation.
        // For very large resources, there is no benefit to suballocate.
        // For very small resources, it is inefficent to suballocate given the min. heap
        // size could be much larger then the resource allocation.
        const size_t resourceHeapKindIndex =
            GetResourceHeapKind(resourceDescriptor.Dimension, heapType, resourceDescriptor.Flags);

        const D3D12_RESOURCE_ALLOCATION_INFO resourceInfo =
            mDevice->GetD3D12Device()->GetResourceAllocationInfo(0, 1, &resourceDescriptor);

        // Note: Sub-allocator uses the buddy system which requires the allocation
        // to be a power-of-two size. The aligned size must be computed before Allocate
        // to first get the desired allocator which sub-allocates from a larger power-of-two sized
        // heap based on the allocation aligned size.
        const uint64_t allocationSize = NextPowerOfTwo(resourceInfo.SizeInBytes);

        // TODO(bryan.bernhart@intel.com): Adjust desired heap size based on
        // a heuristic. Smaller but frequent allocations benefit by sub-allocating from a larger
        // heap. However, a large heap may go unused and waste only memory.
        // When allocationSize approaches or equals heapSize, sub-allocation has no
        // further performance benefit.
        const size_t heapLevel = GetHeapLevelFromHeapSize(allocationSize);

        ASSERT(mSubAllocatedResourceAllocators[resourceHeapKindIndex].size() > 0);

        // Gracefully fail should the target heap size exceed limit.
        if (heapLevel >= mSubAllocatedResourceAllocators[resourceHeapKindIndex].size()) {
            return ResourceHeapAllocation{};  // invalid
        }

        BuddyPlacedResourceAllocator* allocator =
            mSubAllocatedResourceAllocators[resourceHeapKindIndex][heapLevel].get();

        ResourceHeapAllocation allocation;
        DAWN_TRY_ASSIGN(allocation, allocator->Allocate(resourceDescriptor, allocationSize,
                                                        resourceInfo.Alignment, initialUsage));
        return allocation;
    }

    // Create placed resource sub-allocators backed by heaps in power-of-two sizes.
    std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>>
    ResourceAllocatorManager::CreatePlacedResourceAllocators(D3D12_HEAP_TYPE heapType,
                                                             D3D12_HEAP_FLAGS heapFlags) const {
        std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>> allocators;
        for (uint64_t heapSize = kMinHeapSize; heapSize <= kMaxHeapSize; heapSize *= 2) {
            std::unique_ptr<BuddyPlacedResourceAllocator> allocator =
                std::make_unique<BuddyPlacedResourceAllocator>(kMaxHeapSize, heapSize, mDevice,
                                                               heapType, heapFlags);
            allocators.push_back(std::move(allocator));
        }
        return allocators;
    }

    size_t ResourceAllocatorManager::GetHeapLevelFromHeapSize(uint64_t heapSize) const {
        // TODO(crbug.com/dawn/238): Uncomment the following ASSERT.
        // ASSERT(heapSize >= kMinHeapSize);
        return Log2(heapSize) - Log2(kMinHeapSize);
    }

    // BuddyPlacedResourceAllocator

    BuddyPlacedResourceAllocator::BuddyPlacedResourceAllocator(uint64_t maxResourceSize,
                                                               uint64_t heapSize,
                                                               Device* device,
                                                               D3D12_HEAP_TYPE heapType,
                                                               D3D12_HEAP_FLAGS heapFlags)
        : mDevice(device),
          mHeapFlags(heapFlags),
          mBuddyMemoryAllocator(maxResourceSize,
                                heapSize,
                                std::make_unique<HeapAllocator>(device, heapType)) {
    }

    ResultOrError<ResourceHeapAllocation> BuddyPlacedResourceAllocator::Allocate(
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        uint64_t allocationSize,
        uint64_t allocationAlignment,
        D3D12_RESOURCE_STATES initialUsage) {
        ResourceMemoryAllocation allocation;
        DAWN_TRY_ASSIGN(allocation, mBuddyMemoryAllocator.Allocate(
                                        allocationSize, allocationAlignment, mHeapFlags));

        ID3D12Heap* heap = static_cast<Heap*>(allocation.GetResourceHeap())->GetD3D12Heap().Get();

        // Heap flags must be compatible or be the same heap flags specified when the heap
        // was created. It is the responsibly of the caller to ensure these flags are correct
        // or CreatePlacedResource will fail.
        ASSERT(heap->GetDesc().Flags == mHeapFlags);

        ComPtr<ID3D12Resource> placedResource;
        if (FAILED(mDevice->GetD3D12Device()->CreatePlacedResource(
                heap, allocation.GetOffset(), &resourceDescriptor, initialUsage, nullptr,
                IID_PPV_ARGS(&placedResource)))) {
            return DAWN_OUT_OF_MEMORY_ERROR("Unable to allocate resource");
        }

        return ResourceHeapAllocation{allocation.GetInfo(), allocation.GetOffset(),
                                      std::move(placedResource)};
    }

    void BuddyPlacedResourceAllocator::Deallocate(ResourceHeapAllocation& allocation) {
        mBuddyMemoryAllocator.Deallocate(allocation);
    }

    // CommittedResourceAllocator

    CommittedResourceAllocator::CommittedResourceAllocator(Device* device, D3D12_HEAP_TYPE heapType)
        : mDevice(device), mHeapType(heapType) {
    }

    ResultOrError<ResourceHeapAllocation> CommittedResourceAllocator::Allocate(
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        D3D12_RESOURCE_STATES initialUsage) {
        D3D12_HEAP_PROPERTIES heapProperties;
        heapProperties.Type = mHeapType;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 0;
        heapProperties.VisibleNodeMask = 0;

        // Note: Heap flags are inferred by the resource descriptor and do not need to be explicitly
        // provided to CreateCommittedResource.
        ComPtr<ID3D12Resource> committedResource;
        if (FAILED(mDevice->GetD3D12Device()->CreateCommittedResource(
                &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescriptor, initialUsage, nullptr,
                IID_PPV_ARGS(&committedResource)))) {
            return DAWN_OUT_OF_MEMORY_ERROR("Unable to allocate resource");
        }

        AllocationInfo info;
        info.mMethod = AllocationMethod::kDirect;

        return ResourceHeapAllocation{info,
                                      /*offset*/ 0, std::move(committedResource)};
    }

    void CommittedResourceAllocator::Deallocate(ResourceHeapAllocation& allocation) {
        // Unused.
    }

}}  // namespace dawn_native::d3d12
