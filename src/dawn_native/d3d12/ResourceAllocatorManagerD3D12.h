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

#include "dawn_native/d3d12/BuddyPlacedResourceAllocatorD3D12.h"
#include "dawn_native/d3d12/CommittedResourceAllocatorD3D12.h"

#include <array>
#include <map>

namespace dawn_native { namespace d3d12 {

    class Device;

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

      private:
        // Seperated for testing purposes.
        ResultOrError<ResourceHeapAllocation> SubAllocateMemory(
            D3D12_HEAP_TYPE heapType,
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            D3D12_RESOURCE_STATES initialUsage,
            D3D12_HEAP_FLAGS heapFlags);

        std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>> CreatePlacedResourceAllocators(
            D3D12_HEAP_TYPE heapType) const;

        size_t GetHeapLevelFromHeapSize(uint64_t heapSize) const;

        size_t GetD3D12HeapTypeToIndex(D3D12_HEAP_TYPE heapType) const;

        D3D12_HEAP_FLAGS GetD3D12HeapFlags(D3D12_RESOURCE_DIMENSION dimension) const;

        Device* mDevice;

        static constexpr uint32_t kNumHeapTypes = 4u;  // Number of D3D12_HEAP_TYPE
        static constexpr uint64_t kMaxHeapSize = 32ll * 1024ll * 1024ll * 1024ll;  // 32GB
        static constexpr uint64_t kMinHeapSize = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

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

        typedef std::vector<std::unique_ptr<BuddyPlacedResourceAllocator>> PlacedResourceAllocators;

        std::map<D3D12_HEAP_FLAGS, std::array<PlacedResourceAllocators, kNumHeapTypes>>
            mSubAllocatedResourceAllocators;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESOURCEALLOCATORMANAGERD3D12_H_