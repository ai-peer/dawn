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

Service::Service(Device* device) : mDevice(device) {
#if DAWN_PLATFORM_IS(FUCHSIA)
    if (CheckZirconHandleSupport(mDevice->GetDeviceInfo())) {
        mServiceImpls.emplace(
            std::make_pair(ExternalImageType::OpaqueFD, CreateZirconHandleService(mDevice)));
    }
#endif  // DAWN_PLATFORM_IS(FUCHSIA)

#if DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
    if (CheckOpaqueFDSupport(mDevice->GetDeviceInfo())) {
        mServiceImpls.emplace(
            std::make_pair(ExternalImageType::OpaqueFD, CreateOpaqueFDService(mDevice)))
    }

    if (CheckDmaBufSupport(mDevice->GetDeviceInfo())) {
        mServiceImpls.emplace(
            std::make_pair(ExternalImageType::DmaBuf, CreateDmaBufService(mDevice)));
    }
#endif  // DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)

#if DAWN_PLATFORM_IS(ANDROID)
    if (CheckAHardwareBufferSupport(mDevice->GetDeviceInfo())) {
        mServiceImpls.emplace(std::make_pair(ExternalImageType::AHardwareBuffer,
                                             CreateAHardwareBufferService(mDevice)));
    }
#endif  // DAWN_PLATFORM_IS(ANDROID)
}

Service::~Service() = default;

bool Service::SupportsImportMemory(ExternalImageType externalImageType,
                                   VkFormat format,
                                   VkImageType type,
                                   VkImageTiling tiling,
                                   VkImageUsageFlags usage,
                                   VkImageCreateFlags flags) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(externalImageType);
    ASSERT(serviceImpl);

    return serviceImpl->SupportsImportMemory(format, type, tiling, usage, flags);
}

bool Service::SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                                  VkFormat format,
                                  VkImageUsageFlags usage,
                                  bool* supportsDisjoint) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(descriptor->GetType());
    ASSERT(serviceImpl);

    return serviceImpl->SupportsCreateImage(descriptor, format, usage, supportsDisjoint);
}

ResultOrError<MemoryImportParams> Service::GetMemoryImportParams(
    const ExternalImageDescriptor* descriptor,
    VkImage image) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(descriptor->GetType());
    ASSERT(serviceImpl);

    return serviceImpl->GetMemoryImportParams(descriptor, image);
}

uint32_t Service::GetQueueFamilyIndex(ExternalImageType externalImageType) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(externalImageType);
    ASSERT(serviceImpl);

    return serviceImpl->GetQueueFamilyIndex();
}

ResultOrError<VkDeviceMemory> Service::ImportMemory(ExternalImageType externalImageType,
                                                    ExternalMemoryHandle handle,
                                                    const MemoryImportParams& importParams,
                                                    VkImage image) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(externalImageType);
    ASSERT(serviceImpl);

    return serviceImpl->ImportMemory(handle, importParams, image);
}

ResultOrError<VkImage> Service::CreateImage(const ExternalImageDescriptor* descriptor,
                                            const VkImageCreateInfo& baseCreateInfo) {
    ServiceImplementation* serviceImpl = GetServiceImplementation(descriptor->GetType());
    ASSERT(serviceImpl);

    return serviceImpl->CreateImage(descriptor, baseCreateInfo);
}

ServiceImplementation* Service::GetServiceImplementation(ExternalImageType externalImageType) {
    auto search = mServiceImpls.find(externalImageType);
    if (search != mServiceImpls.end()) {
        return search->second.get();
    }

    return nullptr;
}

}  // namespace dawn::native::vulkan::external_memory
