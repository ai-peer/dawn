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

#ifndef SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICENULL_H_
#define SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICENULL_H_

#include "dawn/native/vulkan/external_memory/MemoryService.h"

namespace dawn::native::vulkan::external_memory {

class MemoryServiceNull : public Service {
  public:
    explicit MemoryServiceNull(Device* device);
    ~MemoryServiceNull() override;

    static bool CheckSupport(const VulkanDeviceInfo& deviceInfo);

    // True if the device reports it supports importing external memory.
    bool SupportsImportMemory(VkFormat format,
                              VkImageType type,
                              VkImageTiling tiling,
                              VkImageUsageFlags usage,
                              VkImageCreateFlags flags) override;

    // True if the device reports it supports creating VkImages from external memory.
    bool SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                             VkFormat format,
                             VkImageUsageFlags usage,
                             bool* supportsDisjoint) override;

    // Returns the parameters required for importing memory
    ResultOrError<MemoryImportParams> GetMemoryImportParams(
        const ExternalImageDescriptor* descriptor,
        VkImage image) override;

    // Returns the index of the queue memory from this services should be exported with.
    uint32_t GetQueueFamilyIndex() override;

    // Given an external handle pointing to memory, import it into a VkDeviceMemory
    ResultOrError<VkDeviceMemory> ImportMemory(ExternalMemoryHandle handle,
                                               const MemoryImportParams& importParams,
                                               VkImage image) override;

    // Create a VkImage for the given handle type
    ResultOrError<VkImage> CreateImage(const ExternalImageDescriptor* descriptor,
                                       const VkImageCreateInfo& baseCreateInfo) override;

    bool Supported() const override;

  private:
    bool mSupported = false;
};

}  // namespace dawn::native::vulkan::external_memory

#endif  // SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICENULL_H_
