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
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/VulkanFunctions.h"
#include "dawn/native/vulkan/VulkanInfo.h"
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceImplementation.h"
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceImplementationFD.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceImplementationZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

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

// Use a default for the deprecated path where Chromium doesn't enable semaphore features.
// TODO(dawn:1838): Remove this when Chromium explicitly enables features.
static constexpr wgpu::DawnVkSemaphoreType kDefaultType =
#if DAWN_PLATFORM_IS(FUCHSIA)  // Fuchsia
    wgpu::DawnVkSemaphoreType::ZirconHandle;
#elif DAWN_PLATFORM_IS(ANDROID) || DAWN_PLATFORM_IS(CHROMEOS)  // Android, ChromeOS
    wgpu::DawnVkSemaphoreType::SyncFD;
#elif DAWN_PLATFORM_IS(LINUX)                                  // Linux
    wgpu::DawnVkSemaphoreType::OpaqueFD;
#endif

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
    // TODO(dawn:1838): Removing initialization based on kDefaultType when Chromium enables
    // features explicitly.
#if DAWN_PLATFORM_IS(FUCHSIA)
    if (device->HasFeature(Feature::SyncVkSemaphoreZirconHandle) ||
        kDefaultType == wgpu::DawnVkSemaphoreType::ZirconHandle) {
        mServiceImpls[wgpu::DawnVkSemaphoreType::ZirconHandle] = CreateZirconHandleService(device);
    }
#endif

    if (device->HasFeature(Feature::SyncVkSemaphoreSyncFD) ||
        kDefaultType == wgpu::DawnVkSemaphoreType::SyncFD) {
        mServiceImpls[wgpu::DawnVkSemaphoreType::SyncFD] =
            CreateFDService(device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
    }

    if (device->HasFeature(Feature::SyncVkSemaphoreOpaqueFD) ||
        kDefaultType == wgpu::DawnVkSemaphoreType::OpaqueFD) {
        mServiceImpls[wgpu::DawnVkSemaphoreType::OpaqueFD] =
            CreateFDService(device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR);
    }
}

Service::~Service() = default;

bool Service::Supported() {
    return SupportsType(kDefaultType);
}

bool Service::SupportsType(wgpu::DawnVkSemaphoreType type) const {
    return mServiceImpls[type] && mServiceImpls[type]->Supported();
}

void Service::CloseHandle(wgpu::DawnVkSemaphoreType type, ExternalSemaphoreHandle handle) {
    ASSERT(mServiceImpls[type]);
    mServiceImpls[type]->CloseHandle(handle);
}

ResultOrError<VkSemaphore> Service::ImportSemaphore(wgpu::DawnVkSemaphoreType type,
                                                    ExternalSemaphoreHandle handle) {
    ASSERT(mServiceImpls[type]);
    return mServiceImpls[type]->ImportSemaphore(handle);
}

ResultOrError<VkSemaphore> Service::CreateExportableSemaphore(wgpu::DawnVkSemaphoreType type) {
    ASSERT(mServiceImpls[type]);
    return mServiceImpls[type]->CreateExportableSemaphore();
}

ResultOrError<ExternalSemaphoreHandle> Service::ExportSemaphore(wgpu::DawnVkSemaphoreType type,
                                                                VkSemaphore semaphore) {
    ASSERT(mServiceImpls[type]);
    return mServiceImpls[type]->ExportSemaphore(semaphore);
}

ExternalSemaphoreHandle Service::DuplicateHandle(wgpu::DawnVkSemaphoreType type,
                                                 ExternalSemaphoreHandle handle) {
    ASSERT(mServiceImpls[type]);
    return mServiceImpls[type]->DuplicateHandle(handle);
}

}  // namespace dawn::native::vulkan::external_semaphore
