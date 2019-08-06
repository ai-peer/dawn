#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/external_memory/MemoryService.h"

#define NULL_ERROR DAWN_UNIMPLEMENTED_ERROR("Using null memory service to interop inside Vulkan")

namespace dawn_native { namespace vulkan { namespace external_memory {

    Service::Service() {
    }

    Service::~Service() {
    }

    bool Service::Supported() {
        return false;
    }

    ServiceType Service::GetType() {
        return ServiceType::Null;
    }

    ResultOrError<VkDeviceMemory> Service::ImportImageMemory(Device* device,
                                                             ExternalMemoryHandle handle,
                                                             VkDeviceSize allocationSize,
                                                             uint32_t memoryTypeIndex) {
        return NULL_ERROR;
    }

}}}  // namespace dawn_native::vulkan::external_memory
