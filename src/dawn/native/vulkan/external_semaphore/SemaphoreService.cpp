// Copyright 2019 The Dawn Authors
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

#include "dawn/native/vulkan/external_semaphore/Service.h"
#include "dawn/native/vulkan/external_semaphore/ServiceImplementationNull.h"

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_semaphore/ServiceImplementationZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Android, ChromeOS and Linux
#if DAWN_PLATFORM_IS(LINUX)
#include "dawn/native/vulkan/external_semaphore/ServiceImplementationFD.h"
#endif  // DAWN_PLATFORM_IS(LINUX)

namespace dawn::native::vulkan::external_semaphore {
// static
bool Service::CheckSupport(const VulkanDeviceInfo& deviceInfo,
                           VkPhysicalDevice physicalDevice,
                           const VulkanFunctions& fn) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    return ServiceImplementationZirconHandle::CheckSupport(deviceInfo, physicalDevice, fn);
#elif DAWN_PLATFORM_IS(LINUX)  // Android, ChromeOS and Linux
    return ServiceImplementationFD::CheckSupport(deviceInfo, physicalDevice, fn);
#else
    return ServiceImplementationNull::CheckSupport(deviceInfo, physicalDevice, fn);
#endif
}

// static
void Service::CloseHandle(ExternalSemaphoreHandle handle) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    return ServiceImplementationZirconHandle::CloseHandle(handle);
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Android, ChromeOS and Linux
#if DAWN_PLATFORM_IS(LINUX)
    return ServiceImplementationFD::CloseHandle(handle);
#endif  // DAWN_PLATFORM_IS(LINUX)
}

Service::Service(Device* device) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    serviceImpl_ = std::make_unique<ServiceImplementationZirconHandle>(device);
#elif DAWN_PLATFORM_IS(LINUX)  // Android, ChromeOS and Linux
    serviceImpl_ = std::make_unique<ServiceImplementationFD>(device);
#else
    serviceImpl_ = std::make_unique<ServiceImplementationNull>(device);
#endif
}

Service::~Service() = default;

bool Service::Supported() {
    ASSERT(serviceImpl_);
    return serviceImpl_->Supported();
}

ResultOrError<VkSemaphore> Service::ImportSemaphore(ExternalSemaphoreHandle handle) {
    ASSERT(serviceImpl_);
    return serviceImpl_->ImportSemaphore(handle);
}

ResultOrError<VkSemaphore> Service::CreateExportableSemaphore() {
    ASSERT(serviceImpl_);
    return serviceImpl_->CreateExportableSemaphore();
}

ResultOrError<ExternalSemaphoreHandle> Service::ExportSemaphore(VkSemaphore semaphore) {
    ASSERT(serviceImpl_);
    return serviceImpl_->ExportSemaphore(semaphore);
}

ExternalSemaphoreHandle Service::DuplicateHandle(ExternalSemaphoreHandle handle) {
    ASSERT(serviceImpl_);
    return serviceImpl_->DuplicateHandle(handle);
}

}  // namespace dawn::native::vulkan::external_semaphore
