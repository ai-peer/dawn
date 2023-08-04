// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_SHAREDTEXTUREMEMORYVK_H_
#define SRC_DAWN_NATIVE_VULKAN_SHAREDTEXTUREMEMORYVK_H_

#include <vector>

#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/SharedTextureMemory.h"

namespace dawn::native::vulkan {

class Device;

class SharedTextureMemory final : public SharedTextureMemoryBase {
  public:
    static ResultOrError<Ref<SharedTextureMemory>> Create(
        Device* device,
        const char* label,
        const SharedTextureMemoryVkImageDescriptor* vkImageDesc,
        const SharedTextureMemoryDmaBufDescriptor* descriptor);

    static ResultOrError<Ref<SharedTextureMemory>> Create(
        Device* device,
        const char* label,
        const SharedTextureMemoryVkImageDescriptor* vkImageDesc,
        const SharedTextureMemoryOpaqueFDDescriptor* descriptor);

    static ResultOrError<Ref<SharedTextureMemory>> Create(
        Device* device,
        const char* label,
        const SharedTextureMemoryVkImageDescriptor* vkImageDesc,
        const SharedTextureMemoryZirconHandleDescriptor* descriptor);

    VkDeviceMemory GetVkDeviceMemory() const;
    VkImage GetVkImage() const;
    uint32_t GetQueueFamilyIndex() const;

  private:
    SharedTextureMemory(Device* device,
                        const char* label,
                        const SharedTextureMemoryProperties& properties);
    void DestroyImpl() override;

    ResultOrError<Ref<TextureBase>> CreateTextureImpl(const TextureDescriptor* descriptor) override;
    MaybeError BeginAccessImpl(TextureBase* texture, const BeginAccessDescriptor*) override;
    ResultOrError<FenceAndSignalValue> EndAccessImpl(TextureBase* texture) override;

    VkImage mImage = VK_NULL_HANDLE;
    VkDeviceMemory mDeviceMemory = VK_NULL_HANDLE;
    uint32_t mQueueFamilyIndex;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_SHAREDTEXTUREMEMORYVK_H_
