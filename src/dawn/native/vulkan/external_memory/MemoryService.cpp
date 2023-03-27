// Copyright 2022 The Dawn Authors
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

#include <utility>

#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/external_memory/MemoryService.h"

#include "dawn/native/vulkan/external_memory/MemoryServiceImplementation.h"

#if DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
#include "dawn/native/vulkan/external_memory/MemoryServiceImplementationDmaBuf.h"
#include "dawn/native/vulkan/external_memory/MemoryServiceImplementationOpaqueFD.h"
#endif  // DAWN_PLATFORM_IS(LINUX)

#if DAWN_PLATFORM_IS(ANDROID)
#include "dawn/native/vulkan/external_memory/MemoryServiceImplementationAHardwareBuffer.h"
#endif  // DAWN_PLATFORM_IS(ANDROID)

#if DAWN_PLATFORM_IS(FUCHSIA)
#include "dawn/native/vulkan/external_memory/MemoryServiceImplementationZirconHandle.h"
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

namespace dawn::native::vulkan::external_memory {
// static
bool Service::CheckSupport(const VulkanDeviceInfo& deviceInfo) {
#if DAWN_PLATFORM_IS(ANDROID)
    return CheckAHardwareBufferSupport(deviceInfo);
#elif DAWN_PLATFORM_IS(FUCHSIA)
    return CheckZirconHandleSupport(deviceInfo);
#elif DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
    return CheckOpaqueFDSupport(deviceInfo) || CheckDmaBufSupport(deviceInfo);
#else
    return false;
#endif
}

Service::Service(Device* device) : mDevice(device) {}
Service::~Service() = default;

bool Service::SupportsImportMemory(const ExternalImageType externalImageType,
                                   VkFormat format,
                                   VkImageType type,
                                   VkImageTiling tiling,
                                   VkImageUsageFlags usage,
                                   VkImageCreateFlags flags) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(externalImageType);
    if (!serviceImpl) {
        return false;
    }

    return serviceImpl->SupportsImportMemory(format, type, tiling, usage, flags);
}

bool Service::SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                                  VkFormat format,
                                  VkImageUsageFlags usage,
                                  bool* supportsDisjoint) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(descriptor->GetType());
    if (!serviceImpl) {
        return false;
    }

    return serviceImpl->SupportsCreateImage(descriptor, format, usage, supportsDisjoint);
}

ResultOrError<MemoryImportParams> Service::GetMemoryImportParams(
    const ExternalImageDescriptor* descriptor,
    VkImage image) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(descriptor->GetType());
    if (!serviceImpl) {
        return DAWN_UNIMPLEMENTED_ERROR(
            "No memory service for passed ExternalImageType to interop inside Vulkan");
    }

    return serviceImpl->GetMemoryImportParams(descriptor, image);
}

uint32_t Service::GetQueueFamilyIndex(const ExternalImageType externalImageType) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(externalImageType);
    if (!serviceImpl) {
        return VK_QUEUE_FAMILY_EXTERNAL_KHR;
    }

    return serviceImpl->GetQueueFamilyIndex();
}

ResultOrError<VkDeviceMemory> Service::ImportMemory(const ExternalImageType externalImageType,
                                                    ExternalMemoryHandle handle,
                                                    const MemoryImportParams& importParams,
                                                    VkImage image) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(externalImageType);
    if (!serviceImpl) {
        return DAWN_UNIMPLEMENTED_ERROR(
            "No memory service for passed ExternalImageType to interop inside Vulkan");
    }
    return serviceImpl->ImportMemory(handle, importParams, image);
}

ResultOrError<VkImage> Service::CreateImage(const ExternalImageDescriptor* descriptor,
                                            const VkImageCreateInfo& baseCreateInfo) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(descriptor->GetType());
    if (!serviceImpl) {
        return DAWN_UNIMPLEMENTED_ERROR(
            "No memory service for passed ExternalImageType to interop inside Vulkan");
    }

    return serviceImpl->CreateImage(descriptor, baseCreateInfo);
}

ServiceImplementation* Service::GetServiceImplementation(
    const ExternalImageType externalImageType) {
    ASSERT(mDevice);

    auto search = mServiceImpls.find(externalImageType);
    if (search != mServiceImpls.end()) {
        return search->second.get();
    }

    // Create memory service based on external image type.
    std::unique_ptr<ServiceImplementation> memoryServiceHandle = nullptr;

    switch (externalImageType) {
        case ExternalImageType::OpaqueFD:
#if DAWN_PLATFORM_IS(FUCHSIA)
            if (CheckZirconHandleSupport(mDevice->GetDeviceInfo())) {
                return mServiceImpls
                    .emplace(
                        std::make_pair(ExternalImageType::OpaqueFD, std::move(memoryServiceHandle)))
                    .first->second.get();
            }
#elif DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
            if (CheckFDSupport(mDevice->GetDeviceInfo())) {
                return mServiceImpls
                    .emplace(
                        std::make_pair(ExternalImageType::OpaqueFD, std::move(memoryServiceHandle)))
                    .first->second.get();
            }
#endif
            break;
        case ExternalImageType::DmaBuf:
#if DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
            if (CheckDmaBufSupport(mDevice->GetDeviceInfo())) {
                return mServiceImpls
                    .emplace(
                        std::make_pair(ExternalImageType::DmaBuf, std::move(memoryServiceHandle)))
                    .first->second.get();
            }
#endif  // DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
            break;
        case ExternalImageType::AHardwareBuffer:
#if DAWN_PLATFORM_IS(ANDROID)
            if (CheckAHardwareBufferSupport(mDevice->GetDeviceInfo())) {
                return mServiceImpls
                    .emplace(std::make_pair(ExternalImageType::AHardwareBuffer,
                                            std::move(memoryServiceHandle)))
                    .first->second.get();
            }
#endif  // DAWN_PLATFORM_IS(ANDROID)
            break;
        default:
            DAWN_UNREACHABLE();
    }

    // No service supported.
    return nullptr;
}

}  // namespace dawn::native::vulkan::external_memory
