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

#include "dawn/native/vulkan/external_memory/MemoryService.h"

#include "dawn/native/vulkan/external_memory/MemoryServiceManager.h"
#include "dawn/native/vulkan/external_memory/MemoryServiceNull.h"

#if DAWN_PLATFORM_IS(POSIX)
#include "dawn/native/vulkan/external_memory/MemoryServiceDmaBuf.h"
#include "dawn/native/vulkan/external_memory/MemoryServiceOpaqueFD.h"
#endif  // DAWN_PLATFORM_IS(POSIX)

#if DAWN_PLATFORM_IS(ANDROID)
#include "dawn/native/vulkan/external_memory/MemoryServiceAHardwareBuffer.h"
#endif  // DAWN_PLATFORM_IS(ANDROID)

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_memory/MemoryServiceZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

namespace dawn::native::vulkan::external_memory {
// static
bool ServiceManager::CheckSupport(const VulkanDeviceInfo& deviceInfo) {
#if DAWN_PLATFORM_IS(ANDROID)
    return MemoryServiceAHardwareBuffer::CheckSupport(deviceInfo);
#endif  // DAWN_PLATFORM_IS(ANDROID)

#if DAWN_PLATFORM_IS(FUCHSIA)
    return MemoryServiceZirconHandle::CheckSupport(deviceInfo);
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Linux and ChromeOS
#if DAWN_PLATFORM_IS(POSIX)
    return MemoryServiceOpaqueFD::CheckSupport(deviceInfo) ||
           MemoryServiceDmaBuffer::CheckSupport(deviceInfo);
#endif  // DAWN_PLATFORM_IS(POSIX)

    return MemoryServiceNull::CheckSupport(deviceInfo);
}

ServiceManager::ServiceManager(Device* device) : mDevice(device) {
    null_service_ = std::make_unique<MemoryServiceNull>(mDevice);

#if DAWN_PLATFORM_IS(ANDROID)
    auto memoryServiceAHardwareBuffer = std::make_unique<MemoryServiceAHardwareBuffer>(mDevice);
    if (memoryServiceAHardwareBuffer->Supported()) {
        services_.emplace(std::make_pair(ExternalImageType::AHardwareBuffer,
                                         std::move(memoryServiceAHardwareBuffer)));
    }
#endif  // DAWN_PLATFORM_IS(ANDROID)

#if DAWN_PLATFORM_IS(FUCHSIA)
    auto memoryServiceZirconHandle = std::make_unique<MemoryServiceZirconHandle>(mDevice);
    if (memoryServiceZirconHandle->Supported()) {
        services_.emplace(
            std::make_pair(ExternalImageType::OpaqueFD, std::move(memoryServiceZirconHandle)));
    }
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

// Linux and ChromeOS
#if DAWN_PLATFORM_IS(POSIX)
    auto memoryServiceOpaqueFD = std::make_unique<MemoryServiceOpaqueFD>(mDevice);
    if (memoryServiceOpaqueFD->Supported()) {
        services_.emplace(
            std::make_pair(ExternalImageType::OpaqueFD, std::move(memoryServiceOpaqueFD)));
    }

    auto memoryServiceDmaBuffer = std::make_unique<MemoryServiceDmaBuffer>(mDevice);
    if (memoryServiceDmaBuffer->Supported()) {
        services_.emplace(
            std::make_pair(ExternalImageType::DmaBuf, std::move(memoryServiceDmaBuffer)));
    }
#endif  // DAWN_PLATFORM_IS(POSIX)
}

Service* ServiceManager::GetService(ExternalImageType type) {
    auto search = services_.find(type);

    if (search != services_.end()) {
        return search->second.get();
    } else {
        return null_service_.get();
    }
}
}  // namespace dawn::native::vulkan::external_memory
