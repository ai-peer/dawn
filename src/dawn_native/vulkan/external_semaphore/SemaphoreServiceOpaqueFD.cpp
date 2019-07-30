#include "dawn_native/vulkan/external_semaphore/SemaphoreServiceOpaqueFD.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    ServiceOpaqueFD::ServiceOpaqueFD() {
    }

    ServiceOpaqueFD::~ServiceOpaqueFD() {
    }

    bool ServiceOpaqueFD::Supported() {
        return true;
    }

    ServiceType ServiceOpaqueFD::GetType() {
        return ServiceType::OpaqueFD;
    }

    ResultOrError<VkSemaphore> ServiceOpaqueFD::ImportSemaphore(Device* device,
                                                                ExternalHandle handle) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo info;
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = 0;

        DAWN_TRY(CheckVkSuccess(
            device->fn.CreateSemaphore(device->GetVkDevice(), &info, nullptr, &semaphore),
            "vkCreateSemaphore"));

        VkImportSemaphoreFdInfoKHR importSemaphoreFdInfo;
        importSemaphoreFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
        importSemaphoreFdInfo.pNext = nullptr;
        importSemaphoreFdInfo.semaphore = semaphore;
        importSemaphoreFdInfo.flags = 0;
        importSemaphoreFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        importSemaphoreFdInfo.fd = handle;

        DAWN_TRY(CheckVkSuccess(
            device->fn.ImportSemaphoreFdKHR(device->GetVkDevice(), &importSemaphoreFdInfo),
            "vkImportSemaphoreFdKHR"));
        return semaphore;
    }

    ResultOrError<VkSemaphore> ServiceOpaqueFD::CreateExportableSemaphore(Device* device) {
        VkExportSemaphoreCreateInfoKHR exportSemaphoreInfo;
        exportSemaphoreInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR;
        exportSemaphoreInfo.pNext = nullptr;
        exportSemaphoreInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

        VkSemaphoreCreateInfo semaphoreCreateInfo;
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = &exportSemaphoreInfo;
        semaphoreCreateInfo.flags = 0;

        VkSemaphore signalSemaphore;
        DAWN_TRY(
            CheckVkSuccess(device->fn.CreateSemaphore(device->GetVkDevice(), &semaphoreCreateInfo,
                                                      nullptr, &signalSemaphore),
                           "vkCreateSemaphore"));
        return signalSemaphore;
    }

    ResultOrError<ExternalHandle> ServiceOpaqueFD::ExportSemaphore(Device* device,
                                                                   VkSemaphore semaphore) {
        VkSemaphoreGetFdInfoKHR semaphoreGetFdInfo;
        semaphoreGetFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        semaphoreGetFdInfo.pNext = nullptr;
        semaphoreGetFdInfo.semaphore = semaphore;
        semaphoreGetFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

        int fd = -1;
        DAWN_TRY(CheckVkSuccess(
            device->fn.GetSemaphoreFdKHR(device->GetVkDevice(), &semaphoreGetFdInfo, &fd),
            "vkGetSemaphoreFdKHR"));

        if (fd == -1) {
            return DAWN_VALIDATION_ERROR(
                "Opaque FD export ended up with a negative file descriptor");
        }

        return fd;
    }

}}}  // namespace dawn_native::vulkan::external_semaphore
