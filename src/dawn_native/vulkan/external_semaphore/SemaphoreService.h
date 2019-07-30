#ifndef DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICE_H_
#define DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICE_H_
#include <memory>
#include "common/Platform.h"
#include "common/vulkan_platform.h"
#include "dawn_native/Error.h"
#include "dawn_native/vulkan/ExternalHandle.h"

namespace dawn_native { namespace vulkan {
    class Device;
}}  // namespace dawn_native::vulkan

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    enum class ServiceType { Null, OpaqueFD };

    class Service {
      public:
        Service() {
        }
        virtual ~Service() = 0;
        virtual bool Supported() = 0;
        virtual ServiceType GetType() = 0;

        virtual ResultOrError<VkSemaphore> ImportSemaphore(Device* device,
                                                           ExternalHandle handle) = 0;
        virtual ResultOrError<VkSemaphore> CreateExportableSemaphore(Device* device) = 0;
        virtual ResultOrError<ExternalHandle> ExportSemaphore(Device* device,
                                                              VkSemaphore semaphore) = 0;
    };

    std::unique_ptr<Service> GeneratePlatformService();

}}}  // namespace dawn_native::vulkan::external_semaphore

#endif  // DAWNNATIVE_VULKAN_EXTERNALSEMAPHORE_SERVICE_H_
