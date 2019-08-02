#include "dawn_native/vulkan/external_semaphore/SemaphoreServiceNull.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"

#define NULL_ERROR DAWN_UNIMPLEMENTED_ERROR("Using null semaphore service to interop inside Vulkan")

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    ServiceNull::ServiceNull() {
    }

    ServiceNull::~ServiceNull() {
    }

    bool ServiceNull::Supported() {
        return false;
    }

    ServiceType ServiceNull::GetType() {
        return ServiceType::Null;
    }

    ResultOrError<VkSemaphore> ServiceNull::ImportSemaphore(Device* device, ExternalHandle handle) {
        return NULL_ERROR;
    }

    ResultOrError<VkSemaphore> ServiceNull::CreateExportableSemaphore(Device* device) {
        return NULL_ERROR;
    }

    ResultOrError<ExternalHandle> ServiceNull::ExportSemaphore(Device* device,
                                                               VkSemaphore semaphore) {
        return NULL_ERROR;
    }

}}}  // namespace dawn_native::vulkan::external_semaphore
