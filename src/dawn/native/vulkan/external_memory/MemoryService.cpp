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

#include "dawn/native/vulkan/external_memory/MemoryService.h"

#include "dawn/native/vulkan/external_memory/MemoryServiceImplementation.h"

#if DAWN_PLATFORM_IS(LINUX)
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
bool ServiceManager::CheckSupport(const VulkanDeviceInfo& deviceInfo) {
#if DAWN_PLATFORM_IS(ANDROID)
    return CheckAHardwareBufferSupport(deviceInfo);
#elif DAWN_PLATFORM_IS(FUCHSIA)
    return CheckZirconHandleSupport(deviceInfo);
#elif DAWN_PLATFORM_IS(LINUX)
    return CheckOpaqueFDSupport(deviceInfo) || CheckDmaBufSupport(deviceInfo);
#elif DAWN_PLATFORM_IS(CHROMEOS)
    return CheckDmaBufSupport(deviceInfo);
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
    return GetServiceImplementation(externalImageType)
        ->SupportsImportMemory(format, type, tiling, usage, flags);
}

bool Service::SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                                  VkFormat format,
                                  VkImageUsageFlags usage,
                                  bool* supportsDisjoint) {
    return GetServiceImplementation(descriptor->GetType())
        ->SupportsCreateImage(descriptor, format, usage, supportsDisjoint);
}

ResultOrError<MemoryImportParams> Service::GetMemoryImportParams(
    const ExternalImageDescriptor* descriptor,
    VkImage image) {
    return GetServiceImplementation(descriptor->GetType())
        ->GetMemoryImportParams(descriptor, image);
}

uint32_t Service::GetQueueFamilyIndex(const ExternalImageType externalImageType) {
    return GetServiceImplementation(externalImageType)->GetQueueFamilyIndex();
}

ResultOrError<VkDeviceMemory> Service::ImportMemory(const ExternalImageType externalImageType,
                                                    ExternalMemoryHandle handle,
                                                    const MemoryImportParams& importParams,
                                                    VkImage image) {
    return GetServiceImplementation(externalImageType)->ImportMemory(handle, importParams, image);
}

ResultOrError<VkImage> Service::CreateImage(const ExternalImageDescriptor* descriptor,
                                            const VkImageCreateInfo& baseCreateInfo) {
    return GetServiceImplementation(descriptor->GetType())->CreateImage(descriptor, baseCreateInfo);
}

ServiceImplementation* Service::GetServiceImplementation(
    const ExternalImageType externalImageType) {
    if (!mServiceImpls.contains(externalImageType)) {
        switch (externalImageType) {
            case ExternalImageType::OpaqueFD:
#if DAWN_PLATFORM_IS(FUCHSIA)
                auto zicronHandleService = CreateZicronHandleService(mDevice);
                if (zicronHandleService->Supported()) {
                    mServiceImpls.emplace(std::make_pair(ExternalImageType::OpaqueFD,
                                                         std::move(zicronHandleService)));
                }
#elif DAWN_PLATFORM_IS(LINUX)  // DAWN_PLATFORM_IS(FUCHSIA)
                auto opaqueFDService = CreateOpaqueFDService(mDevice);
                if (opaqueFDService->Supported()) {
                    mServiceImpls.emplace(
                        std::make_pair(ExternalImageType::OpaqueFD, std::move(opaqueFDService)));
                }
#endif
                break;
            case ExternalImageType::DmaBuf:
#if DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
                auto dmaBufSerivce = CreateDmaBufService(mDevice);
                if (dmaBufSerivce->Supported()) {
                    mServiceImpls.emplace(
                        std::make_pair(ExternalImageType::DmaBuf, std::move(dmaBufSerivce)));
                }
#endif  // DAWN_PLATFORM_IS(LINUX) || DAWN_PLATFORM_IS(CHROMEOS)
                break;
            case ExternalImageType::AHardwareBuffer:
#if DAWN_PLATFORM_IS(ANDROID)
                auto aHardwareBufferService = CreateAHardwareBufferService(mDevice);
                if (aHardwareBufferService->Supported()) {
                    mServiceImpls.emplace(std::make_pair(ExternalImageType::AHardwareBuffer,
                                                         std::move(aHardwareBufferService)));
                }
#endif  // DAWN_PLATFORM_IS(ANDROID)
                break;
            default:
                DAWN_UNREACHABLE();
        }
    }
    auto search = mServiceImpls.find(type);

    if (search != mServiceImpls.end()) {
        return search->second.get();
    }

    DAWN_UNREACHABLE();
}

}  // namespace dawn::native::vulkan::external_memory
