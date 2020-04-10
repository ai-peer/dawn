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

    DescriptorSetAllocator::~DescriptorSetAllocator() {
        for (auto& pool : mDescriptorPools) {
            if (pool.vkPool != VK_NULL_HANDLE) {
                ToBackend(mLayout->GetDevice())->GetFencedDeleter()->DeleteWhenUnused(pool.vkPool);
            }
        }
    }

    ResultOrError<DescriptorSetAllocation> DescriptorSetAllocator::Allocate() {
        if (mAvailableDescriptorPoolIndices.empty()) {
            DAWN_TRY(AllocateDescriptorPool());
        }

        ASSERT(!mAvailableDescriptorPoolIndices.empty());

        const PoolIndex poolIndex = mAvailableDescriptorPoolIndices.back();
        DescriptorPool& pool = mDescriptorPools[poolIndex];

        ASSERT(!pool.freeSetIndices.empty());

        SetIndex setIndex = pool.freeSetIndices.back();
        pool.freeSetIndices.pop_back();

        if (pool.freeSetIndices.empty()) {
            mAvailableDescriptorPoolIndices.pop_back();
        }

        return DescriptorSetAllocation{pool.sets[setIndex], poolIndex, setIndex};
    }

    void DescriptorSetAllocator::Deallocate(DescriptorSetAllocation* allocationInfo) {
        // We can't reuse the descriptor set right away because the Vulkan spec says in the
        // documentation for vkCmdBindDescriptorSets that the set may be consumed any time between
        // host execution of the command and the end of the draw/dispatch.
        ToBackend(mLayout->GetDevice())
            ->GetDescriptorSetService()
            ->AddDeferredDeallocation(mLayout, allocationInfo->poolIndex,
                                      allocationInfo->setIndex);

        // Clear the content of allocation so that use after frees are more visible.
        *allocationInfo = {};
    }

    void DescriptorSetAllocator::DidDeallocate(PoolIndex poolIndex, SetIndex setIndex) {
        ASSERT(allocationInfo->poolIndex < mDescriptorPools.size());

        auto& freeSetIndices = mDescriptorPools[poolIndex].freeSetIndices;
        if (freeSetIndices.empty()) {
            mAvailableDescriptorPoolIndices.emplace_back(poolIndex);
        }
        freeSetIndices.emplace_back(setIndex);
    }

    MaybeError DescriptorSetAllocator::AllocateDescriptorPool() {
        Device* device = ToBackend(mLayout->GetDevice());

        VkDescriptorPoolCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.maxSets = mMaxSets;
        createInfo.poolSizeCount = static_cast<PoolIndex>(mPoolSizes.size());
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

        std::vector<SetIndex> freeSetIndices;
        freeSetIndices.reserve(mMaxSets);

        for (SetIndex i = 0; i < mMaxSets; ++i) {
            freeSetIndices.push_back(i);
        }

        mAvailableDescriptorPoolIndices.push_back(mDescriptorPools.size());
        mDescriptorPools.emplace_back(DescriptorPool{descriptorPool, std::move(sets),
                                      std::move(freeSetIndices)});

        return {};
    }

}}  // namespace dawn_native::vulkan
