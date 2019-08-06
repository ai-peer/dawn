#ifndef DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICE_H_
#define DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICE_H_
#include <memory>
#include "common/Platform.h"
#include "common/vulkan_platform.h"
#include "dawn_native/Error.h"
#include "dawn_native/vulkan/ExternalHandle.h"

namespace dawn_native { namespace vulkan {
    class Device;
}}  // namespace dawn_native::vulkan

namespace dawn_native { namespace vulkan { namespace external_memory {

    enum class ServiceType { Null, OpaqueFD };

    class Service {
      public:
        Service();
        ~Service();
        bool Supported();
        ServiceType GetType();

        ResultOrError<VkDeviceMemory> ImportImageMemory(Device* device,
                                                        ExternalMemoryHandle handle,
                                                        VkDeviceSize allocationSize,
                                                        uint32_t memoryTypeIndex);
    };

}}}  // namespace dawn_native::vulkan::external_memory

#endif  // DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICE_H_
