#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/external_semaphore/SemaphoreService.h"

namespace dawn_native { namespace vulkan { namespace external_semaphore {

    Service::Service(Device* device) : mDevice(device) {
        // TODO(idanr): Query device here for support information, and save it
    }

    Service::~Service() {
    }

    bool Service::Supported() {
        // TODO(idanr): Query device here for additional support information, decide if supported
        const VulkanDeviceInfo& info = mDevice->GetDeviceInfo();
        return info.externalSemaphore && info.externalSemaphoreFD;
    }

    ResultOrError<VkSemaphore> Service::ImportSemaphore(ExternalSemaphoreHandle handle) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo info;
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = 0;

        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.CreateSemaphore(mDevice->GetVkDevice(), &info, nullptr, &semaphore),
            "vkCreateSemaphore"));

        VkImportSemaphoreFdInfoKHR importSemaphoreFdInfo;
        importSemaphoreFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
        importSemaphoreFdInfo.pNext = nullptr;
        importSemaphoreFdInfo.semaphore = semaphore;
        importSemaphoreFdInfo.flags = 0;
        importSemaphoreFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        importSemaphoreFdInfo.fd = handle;

        MaybeError status = CheckVkSuccess(
            mDevice->fn.ImportSemaphoreFdKHR(mDevice->GetVkDevice(), &importSemaphoreFdInfo),
            "vkImportSemaphoreFdKHR");

        if (status.IsError()) {
            mDevice->fn.DestroySemaphore(mDevice->GetVkDevice(), semaphore, nullptr);
            DAWN_TRY(std::move(status));
        }

        return semaphore;
    }

    ResultOrError<VkSemaphore> Service::CreateExportableSemaphore() {
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
            CheckVkSuccess(mDevice->fn.CreateSemaphore(mDevice->GetVkDevice(), &semaphoreCreateInfo,
                                                       nullptr, &signalSemaphore),
                           "vkCreateSemaphore"));
        return signalSemaphore;
    }

    ResultOrError<ExternalSemaphoreHandle> Service::ExportSemaphore(VkSemaphore semaphore) {
        VkSemaphoreGetFdInfoKHR semaphoreGetFdInfo;
        semaphoreGetFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        semaphoreGetFdInfo.pNext = nullptr;
        semaphoreGetFdInfo.semaphore = semaphore;
        semaphoreGetFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

        int fd = -1;
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.GetSemaphoreFdKHR(mDevice->GetVkDevice(), &semaphoreGetFdInfo, &fd),
            "vkGetSemaphoreFdKHR"));

        if (fd == -1) {
            return DAWN_VALIDATION_ERROR(
                "Opaque FD export ended up with a negative file descriptor");
        }

        return fd;
    }

}}}  // namespace dawn_native::vulkan::external_semaphore
