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

#include <unordered_map>

#include "dawn/native/vulkan/external_semaphore/SemaphoreService.h"
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceNull.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

#if DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceFD.h"
#endif  // DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)

#if DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceFD.h"
#endif  // DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)

#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceManager.h"
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceNull.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_semaphore/SemaphoreServiceZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

namespace dawn::native::vulkan::external_semaphore {
// static
bool ServiceManager::CheckSupport(const VulkanDeviceInfo& deviceInfo,
                                  VkPhysicalDevice physicalDevice,
                                  const VulkanFunctions& fn) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    return SemaphoreServiceZirconHandle::CheckSupport(deviceInfo, physicalDevice, fn);
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Linux and ChromeOS
#if DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)
    return SemaphoreServiceFD::CheckSupport(deviceInfo, physicalDevice, fn);
#endif  // DAWN_PLATFORM_IS(POSIX)

    return SemaphoreServiceNull::CheckSupport(deviceInfo, physicalDevice, fn);
}

// static
void ServiceManager::CloseHandle(ExternalSemaphoreHandle handle) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    return SemaphoreServiceZirconHandle::CloseHandle(handle);
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Linux and ChromeOS
#if DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)
    return SemaphoreServiceFD::CloseHandle(handle);
#endif  // DAWN_PLATFORM_IS(POSIX)
}

ServiceManager::ServiceManager(Device* device) : mDevice(device) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    service_ = std::make_unique<SemaphoreServiceZirconHandle>(mDevice);
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Android, ChromeOS and Linux.
#if DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID)
    service_ = std::make_unique<SemaphoreServiceFD>(mDevice);
#endif  // DAWN_PLATFORM_IS(POSIX) || DAWN_PLATFORM_IS(ANDROID);

    service_ = std::make_unique<SemaphoreServiceNull>(mDevice);
}

ServiceManager::~ServiceManager() = default;

Service* ServiceManager::GetService() {
    return service_.get();
}
}  // namespace dawn::native::vulkan::external_semaphore
