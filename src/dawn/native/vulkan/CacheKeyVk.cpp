// Copyright 2022 The Dawn Authors
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

#include "dawn/common/ZeroedStruct.h"
#include "dawn/common/vulkan_platform.h"
#include "dawn/native/CacheKey.h"

namespace dawn::native {

    template <>
    void CacheKeySerializer<VkDescriptorSetLayoutBinding>::Serialize(
        CacheKey* key,
        const VkDescriptorSetLayoutBinding& t) {
        ZeroedStruct<VkDescriptorSetLayoutBinding> zeroed;
        zeroed.binding = t.binding;
        zeroed.descriptorType = t.descriptorType;
        zeroed.descriptorCount = t.descriptorCount;
        zeroed.stageFlags = t.stageFlags;
        key->Record(zeroed);
    }

    template <>
    void CacheKeySerializer<VkDescriptorSetLayoutCreateInfo>::Serialize(
        CacheKey* key,
        const VkDescriptorSetLayoutCreateInfo& t) {
        key->Record(t.flags).RecordIterable(t.pBindings, t.bindingCount);
    }

}  // namespace dawn::native
