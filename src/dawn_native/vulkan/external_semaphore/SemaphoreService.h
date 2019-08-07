#ifndef DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICE_H_
#define DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICE_H_
#include "common/Platform.h"
#include "common/vulkan_platform.h"
#include "dawn_native/Error.h"
#include "dawn_native/vulkan/ExternalHandle.h"

namespace dawn_native { namespace vulkan {
    class Device;
}}  // namespace dawn_native::vulkan

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    class Service {
      public:
        Service(Device* device);
        ~Service();

        // True if the device reports it supports this feature
        bool Supported();

        // Given an external handle, import it into a VkSemaphore
        ResultOrError<VkSemaphore> ImportSemaphore(ExternalSemaphoreHandle handle);

        // Create a VkSemaphore that is exportable into an external handle later
        ResultOrError<VkSemaphore> CreateExportableSemaphore();

        // Export a VkSemaphore into an external handle
        ResultOrError<ExternalSemaphoreHandle> ExportSemaphore(VkSemaphore semaphore);

      private:
        Device* mDevice = nullptr;
    };

}}}  // namespace dawn_native::vulkan::external_semaphore

#endif  // DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICE_H_
