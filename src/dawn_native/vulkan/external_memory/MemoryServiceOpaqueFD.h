#ifndef DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICEOPAQUEFD_H_
#define DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICEOPAQUEFD_H_
#include "dawn_native/vulkan/external_memory/MemoryService.h"

namespace dawn_native { namespace vulkan { namespace external_memory {

    class ServiceOpaqueFD : public Service {
      public:
        ServiceOpaqueFD();
        virtual ~ServiceOpaqueFD() override = default;
        virtual bool Supported() override;
        virtual ServiceType GetType() override;

        virtual ResultOrError<VkDeviceMemory> ImportImageMemory(Device* device,
                                                                ExternalHandle handle,
                                                                VkDeviceSize allocationSize,
                                                                uint32_t memoryTypeIndex) override;
    };

}}}  // namespace dawn_native::vulkan::external_memory

#endif  // DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICEOPAQUEFD_H_
