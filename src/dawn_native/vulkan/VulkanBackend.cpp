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

// VulkanBackend.cpp: contains the definition of symbols exported by VulkanBackend.h so that they
// can be compiled twice: once export (shared library), once not exported (static library)

// Include vulkan_platform.h before VulkanBackend.h includes vulkan.h so that we use our version
// of the non-dispatchable handles.
#include "common/vulkan_platform.h"

#include "dawn_native/VulkanBackend.h"

#include "common/SwapChainUtils.h"
#include "dawn_native/dawn_structs_autogen.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/NativeSwapChainImplVk.h"
#include "dawn_native/vulkan/TextureVk.h"

namespace dawn_native { namespace vulkan {

    VkInstance GetInstance(DawnDevice device) {
        Device* backendDevice = reinterpret_cast<Device*>(device);
        return backendDevice->GetVkInstance();
    }

    // Explicitly export this function because it uses the "native" type for surfaces while the
    // header as seen in this file uses the wrapped type.
    DAWN_NATIVE_EXPORT DawnSwapChainImplementation
    CreateNativeSwapChainImpl(DawnDevice device, VkSurfaceKHRNative surfaceNative) {
        Device* backendDevice = reinterpret_cast<Device*>(device);
        VkSurfaceKHR surface = VkSurfaceKHR::CreateFromHandle(surfaceNative);

        DawnSwapChainImplementation impl;
        impl = CreateSwapChainImplementation(new NativeSwapChainImpl(backendDevice, surface));
        impl.textureUsage = DAWN_TEXTURE_USAGE_BIT_PRESENT;

        return impl;
    }

    DawnTextureFormat GetNativeSwapChainPreferredFormat(
        const DawnSwapChainImplementation* swapChain) {
        NativeSwapChainImpl* impl = reinterpret_cast<NativeSwapChainImpl*>(swapChain->userData);
        return static_cast<DawnTextureFormat>(impl->GetPreferredFormat());
    }

    namespace {
        VkExtent3D VulkanExtent3D(const Extent3D& extent) {
            return {extent.width, extent.height, extent.depth};
        }
    }  // namespace

    DawnTexture WrapVulkanImage(DawnDevice device,
                                const DawnTextureDescriptor* desc,
                                int memoryFd,
                                VkDeviceSize size,
                                uint32_t memoryTypeIndex) {
        Device* backendDevice = reinterpret_cast<Device*>(device);
        const TextureDescriptor* backendDesc = reinterpret_cast<const TextureDescriptor*>(desc);
        Texture* tex = new Texture(backendDevice, backendDesc);
        VkImageCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.format = VulkanImageFormat(tex->GetFormat());
        createInfo.extent = VulkanExtent3D(tex->GetSize());
        createInfo.mipLevels = tex->GetNumMipLevels();
        createInfo.arrayLayers = tex->GetArrayLayers();
        createInfo.samples = VulkanSampleCount(tex->GetSampleCount());
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = VulkanImageUsage(tex->GetUsage(), tex->GetFormat());
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImage handle;
        if (backendDevice->fn.CreateImage(backendDevice->GetVkDevice(), &createInfo, nullptr,
                                          &handle) != VK_SUCCESS) {
            ASSERT(false);
        }
        VkImportMemoryFdInfoKHR importInfo = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, NULL,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
                                              memoryFd};

        VkMemoryAllocateInfo info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &importInfo, size,
                                     memoryTypeIndex};

        VkDeviceMemory memory;
        VkResult r =
            backendDevice->fn.AllocateMemory(backendDevice->GetVkDevice(), &info, nullptr, &memory);
        if (backendDevice->fn.BindImageMemory(backendDevice->GetVkDevice(), handle, memory, 0) !=
            VK_SUCCESS) {
            ASSERT(false);
        }
        (void)r;
        delete tex;
        tex = new Texture(backendDevice, backendDesc, handle, memory);
        return reinterpret_cast<DawnTexture>(tex);
    }

}}  // namespace dawn_native::vulkan
