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

#ifndef DAWNNATIVE_D3D12_RESOURCEALLOCATORMANAGERD3D12_H_
#define DAWNNATIVE_D3D12_RESOURCEALLOCATORMANAGERD3D12_H_

#include "dawn_native/BuddyMemoryAllocator.h"

#include <array>

namespace dawn_native { namespace d3d12 {

    class Device;

    enum ResourceHeapKind {
        Readback_BuffersOnly,
        Upload_BuffersOnly,
        Default_BuffersOnly,

        Default_TexturesOnly,
        Default_RenderableTexturesOrDepthOnly,

        EnumCount,
        InvalidEnum = EnumCount,
    };

    // Wrapper to allocates a D3D12 placed resource with the buddy allocator.
    // Placed resources must be explicitly backed a D3D12 heap.
    //
    // With placed resources, a single heap can be reused.
    // The resource placed at an offset is only reclaimed
    // upon Tick or after the last command list using the resource has completed
    // on the GPU. This means the same physical memory is not reused
    // within the same command-list and does not require additional synchronization (aliasing
    // barrier).
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource
    class BuddyPlacedResourceAllocator {
      public:
        BuddyPlacedResourceAllocator(uint64_t maxResourceSize,
                                     uint64_t heapSize,
                                     Device* device,
                                     D3D12_HEAP_TYPE heapType,
                                     D3D12_HEAP_FLAGS heapFlags);

        ~BuddyPlacedResourceAllocator() = default;

        ResultOrError<ResourceHeapAllocation> Allocate(
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            uint64_t allocationSize,
            uint64_t allocationAlignment,
            D3D12_RESOURCE_STATES initialUsage);

        void Deallocate(ResourceHeapAllocation& allocation);

      private:
        Device* mDevice;
        D3D12_HEAP_FLAGS mHeapFlags;
        BuddyMemoryAllocator mBuddyMemoryAllocator;
    };

    // Wrapper to allocate D3D12 committed resource.
    // Committed resources are implicitly backed by a D3D12 heap.
    class CommittedResourceAllocator {
      public:
        CommittedResourceAllocator(Device* device, D3D12_HEAP_TYPE heapType);
        ~CommittedResourceAllocator() = default;

        ResultOrError<ResourceHeapAllocation> Allocate(
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            D3D12_RESOURCE_STATES initialUsage);
        void Deallocate(ResourceHeapAllocation& allocation);

      private:
        Device* mDevice;
        D3D12_HEAP_TYPE mHeapType;
    };

    // Manages a list of resource allocators used by the device to create resources using multiple
    // allocation methods.
    class ResourceAllocatorManager {
      public:
        ResourceAllocatorManager(Device* device);

        ResultOrError<ResourceHeapAllocation> AllocateMemory(
            D3D12_HEAP_TYPE heapType,
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            D3D12_RESOURCE_STATES initialUsage);

        void DeallocateMemory(ResourceHeapAllocation& allocation);

        void Tick(Serial lastCompletedSerial);

      private:
        void FreeMemory(ResourceHeapAllocation& allocation);

        // Seperated for testing purposes.
        ResultOrError<ResourceHeapAllocation> SubAllocateMemory(
            D3D12_HEAP_TYPE heapType,
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            D3D12_RESOURCE_STATES initialUsage);

        std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>> CreatePlacedResourceAllocators(
            D3D12_HEAP_TYPE heapType,
            D3D12_HEAP_FLAGS heapFlags) const;

        size_t GetHeapLevelFromHeapSize(uint64_t heapSize) const;

        size_t GetD3D12HeapTypeToIndex(D3D12_HEAP_TYPE heapType) const;

        Device* mDevice;

        static constexpr uint32_t kNumHeapTypes = 4u;  // Number of D3D12_HEAP_TYPE
        static constexpr uint64_t kMaxHeapSize = 32ll * 1024ll * 1024ll * 1024ll;  // 32GB
        static constexpr uint64_t kMinHeapSize = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

        static_assert(kMinHeapSize <= kMaxHeapSize, "Min heap size exceeds max heap size");

        static_assert(D3D12_HEAP_TYPE_READBACK <= kNumHeapTypes,
                      "Readback heap type enum exceeds max heap types");
        static_assert(D3D12_HEAP_TYPE_UPLOAD <= kNumHeapTypes,
                      "Upload heap type enum exceeds max heap types");
        static_assert(D3D12_HEAP_TYPE_DEFAULT <= kNumHeapTypes,
                      "Default heap type enum exceeds max heap types");
        static_assert(D3D12_HEAP_TYPE_CUSTOM <= kNumHeapTypes,
                      "Custom heap type enum exceeds max heap types");

        std::array<std::unique_ptr<CommittedResourceAllocator>, kNumHeapTypes>
            mDirectResourceAllocators;

        using PlacedResourceAllocators = std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>>;

        std::array<PlacedResourceAllocators, ResourceHeapKind::EnumCount>
            mSubAllocatedResourceAllocators;

        SerialQueue<ResourceHeapAllocation> mAllocationsToDelete;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESOURCEALLOCATORMANAGERD3D12_H_