#ifndef DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICENULL_H_
#define DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICENULL_H_
#include "dawn_native/vulkan/external_memory/MemoryService.h"

namespace dawn_native { namespace vulkan { namespace external_memory {

    class ServiceNull : public Service {
      public:
        ServiceNull() = default;
        virtual ~ServiceNull() override = default;
        virtual bool Supported() override;
        virtual ServiceType GetType() override;

        virtual ResultOrError<VkDeviceMemory> ImportImageMemory(Device* device,
                                                                ExternalHandle handle,
                                                                VkDeviceSize allocationSize,
                                                                uint32_t memoryTypeIndex) override;
    };

}}}  // namespace dawn_native::vulkan::external_memory

#endif  // DAWNNATIVE_VULKAN_EXTERNALMEMORY_SERVICENULL_H_
