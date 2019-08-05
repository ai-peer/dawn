#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/external_memory/MemoryService.h"

namespace dawn_native { namespace vulkan { namespace external_memory {

    Service::Service() {
        // TODO(idanr): Query device here for support information, and save it
    }

    Service::~Service() {
    }

    bool Service::Supported() {
        // TODO(idanr): Query device here for additional support information, decide if supported
        return true;
    }

    ServiceType Service::GetType() {
        return ServiceType::OpaqueFD;
    }

    ResultOrError<VkDeviceMemory> Service::ImportImageMemory(Device* device,
                                                             ExternalHandle handle,
                                                             VkDeviceSize allocationSize,
                                                             uint32_t memoryTypeIndex) {
        VkImportMemoryFdInfoKHR importMemoryFdInfo;
        importMemoryFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        importMemoryFdInfo.pNext = nullptr;
        importMemoryFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        importMemoryFdInfo.fd = handle;

        VkMemoryAllocateInfo allocateInfo;
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext = &importMemoryFdInfo;
        allocateInfo.allocationSize = allocationSize;
        allocateInfo.memoryTypeIndex = memoryTypeIndex;

        VkDeviceMemory allocatedMemory = VK_NULL_HANDLE;
        DAWN_TRY(CheckVkSuccess(device->fn.AllocateMemory(device->GetVkDevice(), &allocateInfo,
                                                          nullptr, &allocatedMemory),
                                "vkAllocateMemory"));
        return allocatedMemory;
    }

}}}  // namespace dawn_native::vulkan::external_memory
