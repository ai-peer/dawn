#include "dawn_native/vulkan/external_semaphore/SemaphoreService.h"
#include "dawn_native/vulkan/external_semaphore/SemaphoreServiceNull.h"

#ifdef DAWN_PLATFORM_LINUX
#    include "dawn_native/vulkan/external_semaphore/SemaphoreServiceOpaqueFD.h"
#endif

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    Service::~Service() {
    }

    std::unique_ptr<Service> GeneratePlatformService() {
#ifdef DAWN_PLATFORM_LINUX
        return std::make_unique<ServiceOpaqueFD>();
#endif

        return std::make_unique<ServiceNull>();
    }

}}}  // namespace dawn_native::vulkan::external_semaphore
