#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/external_semaphore/SemaphoreService.h"

#define NULL_ERROR DAWN_UNIMPLEMENTED_ERROR("Using null semaphore service to interop inside Vulkan")

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    Service::Service(Device* device) : mDevice(device) {
        // Silence compiler warning about mDevice being unused
        (void)mDevice;
    }

    Service::~Service() {
    }

    bool Service::Supported() {
        return false;
    }

    ResultOrError<VkSemaphore> Service::ImportSemaphore(ExternalSemaphoreHandle handle) {
        return NULL_ERROR;
    }

    ResultOrError<VkSemaphore> Service::CreateExportableSemaphore() {
        return NULL_ERROR;
    }

    ResultOrError<ExternalSemaphoreHandle> Service::ExportSemaphore(VkSemaphore semaphore) {
        return NULL_ERROR;
    }

}}}  // namespace dawn_native::vulkan::external_semaphore
