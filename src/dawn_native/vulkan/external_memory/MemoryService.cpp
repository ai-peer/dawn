#include "dawn_native/vulkan/external_memory/MemoryService.h"
#include "dawn_native/vulkan/external_memory/MemoryServiceNull.h"

#ifdef DAWN_PLATFORM_LINUX
#    include "dawn_native/vulkan/external_memory/MemoryServiceOpaqueFD.h"
#endif

namespace dawn_native { namespace vulkan { namespace external_memory {

    Service::~Service() {
    }

    std::unique_ptr<Service> GeneratePlatformService() {
#ifdef DAWN_PLATFORM_LINUX
        return std::make_unique<ServiceOpaqueFD>();
#endif

        return std::make_unique<ServiceNull>();
    }

}}}  // namespace dawn_native::vulkan::external_memory
