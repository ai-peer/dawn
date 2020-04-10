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

#ifndef DAWNNATIVE_VULKAN_DESCRIPTORSETALLOCATOR_H_
#define DAWNNATIVE_VULKAN_DESCRIPTORSETALLOCATOR_H_

#include "common/vulkan_platform.h"
#include "dawn_native/Error.h"
#include "dawn_native/ExternalSlabAllocator.h"
#include "dawn_native/vulkan/DescriptorSetAllocation.h"

#include <map>

namespace dawn_native { namespace vulkan {

    struct DescriptorPoolAndSets {
        VkDescriptorPool pool;
        std::vector<VkDescriptorSet> sets;
    };

    struct DescriptorSetAllocatorTraits {
        using HeapIndex = decltype(DescriptorSetAllocation::heapIndex);
        using AllocationIndex = decltype(DescriptorSetAllocation::allocationIndex);
        using Heap = DescriptorPoolAndSets;
        using AllocationInfo = DescriptorSetAllocation;
    };

    class BindGroupLayout;
    class DescriptorSetAllocator
        : public ExternalSlabAllocator<DescriptorSetAllocator, DescriptorSetAllocatorTraits> {
      public:
        DescriptorSetAllocator(BindGroupLayout* layout,
                               std::map<VkDescriptorType, uint32_t> descriptorCountPerType);

      private:
        friend class ExternalSlabAllocator;

        HeapIndex GetHeapIndex(const DescriptorSetAllocation& info) const {
            return info.heapIndex;
        }

        HeapIndex GetAllocationIndex(const DescriptorSetAllocation& info) const {
            return info.allocationIndex;
        }

        ResultOrError<std::pair<Heap, AllocationIndex>> AllocateHeapImpl();
        void DeallocateHeapImpl(Heap* heap);

        ResultOrError<DescriptorSetAllocation> AllocateImpl(Heap* heap,
                                                            HeapIndex heapIndex,
                                                            AllocationIndex allocationIndex) {
            return DescriptorSetAllocation{heapIndex, allocationIndex, heap->sets[allocationIndex]};
        }

        void DeallocateImpl(DescriptorSetAllocation* info);

        BindGroupLayout* mLayout;
        std::vector<VkDescriptorPoolSize> mPoolSizes;
        AllocationIndex mMaxSets;
    };

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_DESCRIPTORSETALLOCATOR_H_
