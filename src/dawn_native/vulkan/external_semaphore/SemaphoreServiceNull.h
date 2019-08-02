#ifndef DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICENULL_H_
#define DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICENULL_H_
#include "dawn_native/vulkan/external_semaphore/SemaphoreService.h"

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    class ServiceNull : public Service {
      public:
        ServiceNull();
        virtual ~ServiceNull() override;
        virtual bool Supported() override;
        virtual ServiceType GetType() override;

        virtual ResultOrError<VkSemaphore> ImportSemaphore(Device* device,
                                                           ExternalHandle handle) override;
        virtual ResultOrError<VkSemaphore> CreateExportableSemaphore(Device* device) override;
        virtual ResultOrError<ExternalHandle> ExportSemaphore(Device* device,
                                                              VkSemaphore semaphore) override;
    };

}}}  // namespace dawn_native::vulkan::external_semaphore

#endif  // DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICENULL_H_
