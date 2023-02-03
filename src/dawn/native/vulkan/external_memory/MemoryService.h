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

#ifndef SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICE_H_
#define SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICE_H_

#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/VulkanBackend.h"
#include "dawn/native/vulkan/ExternalHandle.h"

namespace dawn::native::vulkan {
class Device;
struct VulkanDeviceInfo;
}  // namespace dawn::native::vulkan

namespace dawn::native::vulkan::external_memory {

struct MemoryImportParams {
    VkDeviceSize allocationSize;
    uint32_t memoryTypeIndex;
    bool dedicatedAllocation = false;
};

class Service {
  public:
    explicit Service(Device* device);
    virtual ~Service();

    // True if the device reports it supports importing external memory.
    virtual bool SupportsImportMemory(VkFormat format,
                                      VkImageType type,
                                      VkImageTiling tiling,
                                      VkImageUsageFlags usage,
                                      VkImageCreateFlags flags) = 0;

    // True if the device reports it supports creating VkImages from external memory.
    virtual bool SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                                     VkFormat format,
                                     VkImageUsageFlags usage,
                                     bool* supportsDisjoint) = 0;

    // Returns the parameters required for importing memory
    virtual ResultOrError<MemoryImportParams> GetMemoryImportParams(
        const ExternalImageDescriptor* descriptor,
        VkImage image) = 0;

    // Returns the index of the queue memory from this services should be exported with.
    virtual uint32_t GetQueueFamilyIndex() = 0;

    // Given an external handle pointing to memory, import it into a VkDeviceMemory
    virtual ResultOrError<VkDeviceMemory> ImportMemory(ExternalMemoryHandle handle,
                                                       const MemoryImportParams& importParams,
                                                       VkImage image) = 0;

    // Create a VkImage for the given handle type
    virtual ResultOrError<VkImage> CreateImage(const ExternalImageDescriptor* descriptor,
                                               const VkImageCreateInfo& baseCreateInfo) = 0;

    // Return true if early checks pass that determine if the service is supported
    virtual bool Supported() const = 0;

  protected:
    bool RequiresDedicatedAllocation(const ExternalImageDescriptorVk* descriptor, VkImage image);

    Device* mDevice = nullptr;
};

}  // namespace dawn::native::vulkan::external_memory

#endif  // SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_MEMORYSERVICE_H_
