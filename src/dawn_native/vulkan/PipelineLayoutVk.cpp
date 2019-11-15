// Copyright 2018 The Dawn Authors
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

#include "dawn_native/vulkan/PipelineLayoutVk.h"

#include "common/BitSetIterator.h"
#include "dawn_native/vulkan/BindGroupLayoutVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/VulkanError.h"

namespace dawn_native { namespace vulkan {

    // static
    ResultOrError<PipelineLayout*> PipelineLayout::Create(
        Device* device,
        const PipelineLayoutDescriptor* descriptor) {
        std::unique_ptr<PipelineLayout> layout =
            std::make_unique<PipelineLayout>(device, descriptor);
        DAWN_TRY(layout->Initialize());
        return layout.release();
    }

    MaybeError PipelineLayout::Initialize() {
        std::array<VkDescriptorSetLayout, kMaxBindGroups> setLayouts;

        uint32_t numSetLayouts = 0;
        for (uint32_t setIndex : IterateBitSet(GetBindGroupLayoutsMask())) {
            for (; numSetLayouts < setIndex; ++numSetLayouts) {
                setLayouts[numSetLayouts] = ToBackend(GetDevice())->GetEmptyDescriptorSetLayout();
            }

            setLayouts[setIndex] = ToBackend(GetBindGroupLayout(setIndex))->GetHandle();
            numSetLayouts = setIndex + 1;
        }

        VkPipelineLayoutCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.setLayoutCount = numSetLayouts;
        createInfo.pSetLayouts = setLayouts.data();
        createInfo.pushConstantRangeCount = 0;
        createInfo.pPushConstantRanges = nullptr;

        Device* device = ToBackend(GetDevice());
        return CheckVkSuccess(
            device->fn.CreatePipelineLayout(device->GetVkDevice(), &createInfo, nullptr, &mHandle),
            "CreatePipelineLayout");
    }

    PipelineLayout::~PipelineLayout() {
        if (mHandle != VK_NULL_HANDLE) {
            ToBackend(GetDevice())->GetFencedDeleter()->DeleteWhenUnused(mHandle);
            mHandle = VK_NULL_HANDLE;
        }
    }

    VkPipelineLayout PipelineLayout::GetHandle() const {
        return mHandle;
    }

}}  // namespace dawn_native::vulkan
