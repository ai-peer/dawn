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

#include "dawn_native/vulkan/DescriptorSetAllocator.h"

#include "dawn_native/vulkan/BindGroupLayoutVk.h"
#include "dawn_native/vulkan/DescriptorSetService.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/VulkanError.h"

namespace dawn_native { namespace vulkan {

    // TODO(enga): Figure out this value.
    static constexpr uint32_t kMaxDescriptorsPerPool = 1024;

    DescriptorSetAllocator::DescriptorSetAllocator(
        BindGroupLayout* layout,
        std::map<VkDescriptorType, uint32_t> descriptorCountPerType)
        : mLayout(layout) {
        // Compute the total number of descriptors for this layout.
        uint32_t totalDescriptorCount = 0;
        mPoolSizes.reserve(descriptorCountPerType.size());
        for (const auto& it : descriptorCountPerType) {
            totalDescriptorCount += it.second;
            mPoolSizes.push_back(VkDescriptorPoolSize{it.first, it.second});
        }
        ASSERT(totalDescriptorCount <= kMaxBindingsPerGroup);

        // Compute the total number of descriptors sets that fits given the max.
        mMaxSets = kMaxDescriptorsPerPool / totalDescriptorCount;

        // Grow the number of desciptors in the pool to fit the computed |mMaxSets|.
        for (auto& poolSize : mPoolSizes) {
            poolSize.descriptorCount *= mMaxSets;
        }
    }

    ResultOrError<std::pair<DescriptorSetAllocator::Heap, DescriptorSetAllocator::AllocationIndex>>
    DescriptorSetAllocator::AllocateHeapImpl() {
        Device* device = ToBackend(mLayout->GetDevice());

        VkDescriptorPoolCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.maxSets = mMaxSets;
        createInfo.poolSizeCount = static_cast<uint32_t>(mPoolSizes.size());
        createInfo.pPoolSizes = mPoolSizes.data();

        VkDescriptorPool descriptorPool;
        DAWN_TRY(CheckVkSuccess(device->fn.CreateDescriptorPool(device->GetVkDevice(), &createInfo,
                                                                nullptr, &*descriptorPool),
                                "CreateDescriptorPool"));

        std::vector<VkDescriptorSetLayout> layouts(mMaxSets, mLayout->GetHandle());

        VkDescriptorSetAllocateInfo allocateInfo;
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.pNext = nullptr;
        allocateInfo.descriptorPool = descriptorPool;
        allocateInfo.descriptorSetCount = mMaxSets;
        allocateInfo.pSetLayouts = AsVkArray(layouts.data());

        std::vector<VkDescriptorSet> sets(mMaxSets);
        MaybeError result =
            CheckVkSuccess(device->fn.AllocateDescriptorSets(device->GetVkDevice(), &allocateInfo,
                                                             AsVkArray(sets.data())),
                           "AllocateDescriptorSets");
        if (result.IsError()) {
            // On an error we can destroy the pool immediately because no command references it.
            device->fn.DestroyDescriptorPool(device->GetVkDevice(), descriptorPool, nullptr);
            DAWN_TRY(std::move(result));
        }

        return std::make_pair(Heap{descriptorPool, std::move(sets)}, mMaxSets);
    }

    void DescriptorSetAllocator::DeallocateHeapImpl(Heap* heap) {
        if (heap->pool != VK_NULL_HANDLE) {
            ToBackend(mLayout->GetDevice())->GetFencedDeleter()->DeleteWhenUnused(heap->pool);
        }
    }

    void DescriptorSetAllocator::DeallocateImpl(DescriptorSetAllocation* info) {
        // We can't reuse the descriptor set right away because the Vulkan spec says in the
        // documentation for vkCmdBindDescriptorSets that the set may be consumed any time between
        // host execution of the command and the end of the draw/dispatch.
        ToBackend(mLayout->GetDevice())
            ->GetDescriptorSetService()
            ->AddDeferredDeallocation(mLayout, info->heapIndex, info->allocationIndex);

        // Clear the content of allocation so that use after frees are more visible.
        *info = {};
    }
}}  // namespace dawn_native::vulkan
