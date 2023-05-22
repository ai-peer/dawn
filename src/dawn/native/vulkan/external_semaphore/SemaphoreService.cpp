// Copyright 2023 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn/native/vulkan/external_semaphore/SemaphoreService.h"
#include "dawn/native/vulkan/VulkanFunctions.h"
#include "dawn/native/vulkan/VulkanInfo.h"
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceImplementation.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceImplementationZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Android, ChromeOS and Linux
#if DAWN_PLATFORM_IS(LINUX)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceImplementationFD.h"
#endif  // DAWN_PLATFORM_IS(LINUX)

namespace dawn::native::vulkan::external_semaphore {

bool CheckSupport(const VulkanDeviceInfo& deviceInfo,
                  VkPhysicalDevice physicalDevice,
                  const VulkanFunctions& fn,
                  VkExternalSemaphoreHandleTypeFlagBits handleType) {
    switch (handleType) {
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT:
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT:
            if (!deviceInfo.HasExt(DeviceExt::ExternalSemaphoreFD)) {
                return false;
            }
            break;
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA:
            if (!deviceInfo.HasExt(DeviceExt::ExternalSemaphoreZirconHandle)) {
                return false;
            }
            break;
        default:
            return false;
    }

    VkPhysicalDeviceExternalSemaphoreInfoKHR semaphoreInfo;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO_KHR;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.handleType = handleType;

    VkExternalSemaphorePropertiesKHR semaphoreProperties;
    semaphoreProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES_KHR;
    semaphoreProperties.pNext = nullptr;

    fn.GetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, &semaphoreInfo,
                                                    &semaphoreProperties);

    VkFlags requiredFlags = VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT_KHR |
                            VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHR;

    return IsSubset(requiredFlags, semaphoreProperties.externalSemaphoreFeatures);
}

// static
bool Service::CheckSupport(const VulkanDeviceInfo& deviceInfo,
                           VkPhysicalDevice physicalDevice,
                           const VulkanFunctions& fn) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    return ::dawn::native::vulkan::external_semaphore::CheckSupport(
        deviceInfo, physicalDevice, fn, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA);
#elif DAWN_PLATFORM_IS(ANDROID) || DAWN_PLATFORM_IS(CHROMEOS)  // Android, ChromeOS
    return ::dawn::native::vulkan::external_semaphore::CheckSupport(
        deviceInfo, physicalDevice, fn, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
#elif DAWN_PLATFORM_IS(LINUX)                                  // Linux
    return ::dawn::native::vulkan::external_semaphore::CheckSupport(
        deviceInfo, physicalDevice, fn, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);
#else
    return false;
#endif
}

Service::Service(Device* device) {
#if DAWN_PLATFORM_IS(FUCHSIA)  // Fuchsia
    mServiceImpl = CreateZirconHandleService(device);
#elif DAWN_PLATFORM_IS(ANDROID) || DAWN_PLATFORM_IS(CHROMEOS)  // Android, ChromeOS
    mServiceImpl = CreateFDService(device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
#elif DAWN_PLATFORM_IS(LINUX)                                  // Linux
    mServiceImpl = CreateFDService(device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);
#endif
}

Service::~Service() = default;

bool Service::Supported() {
    if (!mServiceImpl) {
        return false;
    }

    return mServiceImpl->Supported();
}

void Service::CloseHandle(ExternalSemaphoreHandle handle) {
    ASSERT(mServiceImpl);
    mServiceImpl->CloseHandle(handle);
}

ResultOrError<VkSemaphore> Service::ImportSemaphore(ExternalSemaphoreHandle handle) {
    ASSERT(mServiceImpl);
    return mServiceImpl->ImportSemaphore(handle);
}

ResultOrError<VkSemaphore> Service::CreateExportableSemaphore() {
    ASSERT(mServiceImpl);
    return mServiceImpl->CreateExportableSemaphore();
}

ResultOrError<ExternalSemaphoreHandle> Service::ExportSemaphore(VkSemaphore semaphore) {
    ASSERT(mServiceImpl);
    return mServiceImpl->ExportSemaphore(semaphore);
}

ExternalSemaphoreHandle Service::DuplicateHandle(ExternalSemaphoreHandle handle) {
    ASSERT(mServiceImpl);
    return mServiceImpl->DuplicateHandle(handle);
}

}  // namespace dawn::native::vulkan::external_semaphore
