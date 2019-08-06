#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/external_semaphore/SemaphoreService.h"

#define NULL_ERROR DAWN_UNIMPLEMENTED_ERROR("Using null semaphore service to interop inside Vulkan")

namespace dawn_native { namespace vulkan { namespace external_semaphore {

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

    ResultOrError<VkSemaphore> Service::ImportSemaphore(Device* device,
                                                        ExternalSemaphoreHandle handle) {
        return NULL_ERROR;
    }

    ResultOrError<VkSemaphore> Service::CreateExportableSemaphore(Device* device) {
        return NULL_ERROR;
    }

    ResultOrError<ExternalSemaphoreHandle> Service::ExportSemaphore(Device* device,
                                                                    VkSemaphore semaphore) {
        return NULL_ERROR;
    }

}}}  // namespace dawn_native::vulkan::external_semaphore
