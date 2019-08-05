#include "dawn_native/vulkan/external_memory/MemoryServiceNull.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"

#define NULL_ERROR DAWN_UNIMPLEMENTED_ERROR("Using null memory service to interop inside Vulkan")

namespace dawn_native { namespace vulkan { namespace external_memory {

    bool ServiceNull::Supported() {
        return false;
    }

    ServiceType ServiceNull::GetType() {
        return ServiceType::Null;
    }

    ResultOrError<VkDeviceMemory> ServiceNull::ImportImageMemory(Device* device,
                                                                 ExternalHandle handle,
                                                                 VkDeviceSize allocationSize,
                                                                 uint32_t memoryTypeIndex) {
        return NULL_ERROR;
    }

}}}  // namespace dawn_native::vulkan::external_memory
