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

#include "dawn/native/vulkan/SharedTextureMemoryVk.h"

#include <unistd.h>
#include <utility>

#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/PhysicalDeviceVk.h"
#include "dawn/native/vulkan/ResourceMemoryAllocatorVk.h"
#include "dawn/native/vulkan/SharedFenceVk.h"
#include "dawn/native/vulkan/TextureVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/native/vulkan/VulkanError.h"

namespace dawn::native::vulkan {

namespace {

bool GetFormatModifierProps(const VulkanFunctions& fn,
                            VkPhysicalDevice vkPhysicalDevice,
                            VkFormat format,
                            uint64_t modifier,
                            VkDrmFormatModifierPropertiesEXT* formatModifierProps) {
    std::vector<VkDrmFormatModifierPropertiesEXT> formatModifierPropsVector;
    VkFormatProperties2 formatProps = {};
    formatProps.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    PNextChainBuilder formatPropsChain(&formatProps);

    VkDrmFormatModifierPropertiesListEXT formatModifierPropsList = {};
    formatModifierPropsList.drmFormatModifierCount = 0;
    formatModifierPropsList.pDrmFormatModifierProperties = nullptr;
    formatPropsChain.Add(&formatModifierPropsList,
                         VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT);

    fn.GetPhysicalDeviceFormatProperties2(vkPhysicalDevice, format, &formatProps);

    uint32_t modifierCount = formatModifierPropsList.drmFormatModifierCount;
    formatModifierPropsVector.resize(modifierCount);
    formatModifierPropsList.pDrmFormatModifierProperties = formatModifierPropsVector.data();

    fn.GetPhysicalDeviceFormatProperties2(vkPhysicalDevice, format, &formatProps);
    for (const auto& props : formatModifierPropsVector) {
        if (props.drmFormatModifier == modifier) {
            *formatModifierProps = props;
            return true;
        }
    }
    return false;
}

// Some modifiers use multiple planes (for example, see the comment for
// I915_FORMAT_MOD_Y_TILED_CCS in drm/drm_fourcc.h).
ResultOrError<uint32_t> GetModifierPlaneCount(const VulkanFunctions& fn,
                                              VkPhysicalDevice vkPhysicalDevice,
                                              VkFormat format,
                                              uint64_t modifier) {
    VkDrmFormatModifierPropertiesEXT props;
    if (GetFormatModifierProps(fn, vkPhysicalDevice, format, modifier, &props)) {
        return static_cast<uint32_t>(props.drmFormatModifierPlaneCount);
    }
    return DAWN_VALIDATION_ERROR("DRM format modifier not supported.");
}

ResultOrError<SharedTextureMemoryProperties> PropertiesFromVkImageDesc(
    const Device* device,
    const SharedTextureMemoryVkImageDescriptor* vkImageDesc) {
    SharedTextureMemoryProperties properties = {};
    properties.size = vkImageDesc->vkExtent3D;
    switch (vkImageDesc->vkFormat) {
        case VK_FORMAT_R8_UNORM:
            properties.format = wgpu::TextureFormat::R8Unorm;
            break;
        case VK_FORMAT_R8G8_UNORM:
            properties.format = wgpu::TextureFormat::RG8Unorm;
            break;
        case VK_FORMAT_R8G8B8A8_UNORM:
            properties.format = wgpu::TextureFormat::RGBA8Unorm;
            break;
        case VK_FORMAT_B8G8R8A8_UNORM:
            properties.format = wgpu::TextureFormat::BGRA8Unorm;
            break;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            properties.format = wgpu::TextureFormat::RGB10A2Unorm;
            break;
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
            properties.format = wgpu::TextureFormat::R8BG8Biplanar420Unorm;
            break;
        default:
            return DAWN_VALIDATION_ERROR("Unsupported VkFormat %x", vkImageDesc->vkFormat);
    }

    const Format* internalFormat = nullptr;
    DAWN_TRY_ASSIGN(internalFormat, device->GetInternalFormat(properties.format));

    if (vkImageDesc->vkUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        properties.usage |= wgpu::TextureUsage::CopySrc;
    }
    if (vkImageDesc->vkUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        properties.usage |= wgpu::TextureUsage::CopyDst;
    }
    if (vkImageDesc->vkUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
        properties.usage |= wgpu::TextureUsage::TextureBinding;
    }
    if (internalFormat->isRenderable &&
        (vkImageDesc->vkUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
        properties.usage |= wgpu::TextureUsage::RenderAttachment;
    }
    if (internalFormat->supportsStorageUsage &&
        (vkImageDesc->vkUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)) {
        properties.usage |= wgpu::TextureUsage::StorageBinding;
    }
    return properties;
}

struct ScopedFd {
    explicit ScopedFd(int fd) : fd(fd) {}
    ~ScopedFd() {
        if (fd > 0) {
            close(fd);
        }
    }

    void Detach() { fd = -1; }

    int operator*() const { return fd; }

    ScopedFd(const ScopedFd& rhs) = delete;
    ScopedFd& operator=(const ScopedFd& rhs) = delete;
    ScopedFd(ScopedFd&& rhs) = default;
    ScopedFd& operator=(ScopedFd&& rhs) = default;

    int fd = -1;
};

}  // namespace

// static
ResultOrError<Ref<SharedTextureMemory>> SharedTextureMemory::Create(
    Device* device,
    const char* label,
    const SharedTextureMemoryVkImageDescriptor* vkImageDesc,
    const SharedTextureMemoryDmaBufDescriptor* descriptor) {
    VkDevice vkDevice = device->GetVkDevice();
    VkPhysicalDevice vkPhysicalDevice =
        ToBackend(device->GetPhysicalDevice())->GetVkPhysicalDevice();
    VkFormat vkFormat = VkFormat(vkImageDesc->vkFormat);

    SharedTextureMemoryProperties properties;
    DAWN_TRY_ASSIGN(properties, PropertiesFromVkImageDesc(device, vkImageDesc));

    const Format& internalFormat = device->GetValidInternalFormat(properties.format);
    const bool supportsImageFormatList = device->GetDeviceInfo().HasExt(DeviceExt::ImageFormatList);

    constexpr wgpu::TextureUsage kUsageRequiringView = wgpu::TextureUsage::RenderAttachment |
                                                       wgpu::TextureUsage::TextureBinding |
                                                       wgpu::TextureUsage::StorageBinding;
    if (internalFormat.IsMultiPlanar() && !supportsImageFormatList) {
        // Remove usages needing views if the multiplanar format cannot be reinterpreted as
        // its subplanes.
        properties.usage &= ~kUsageRequiringView;
    }

    const bool mayNeedView = (properties.usage & kUsageRequiringView) != 0;

    // Verify plane count for the modifier.
    uint32_t planeCount;
    DAWN_TRY_ASSIGN(planeCount, GetModifierPlaneCount(device->fn, vkPhysicalDevice, vkFormat,
                                                      descriptor->drmModifier));
    DAWN_INVALID_IF(planeCount == 0, "Zero planes in image");

    // bool multiplanarDisjoint = false;
    // if (planeCount > 1) {
    //   VkDrmFormatModifierPropertiesEXT props;
    //   multiplanarDisjoint = (GetFormatModifierProps(device->fn, vkPhysicalDevice, vkFormat,
    //   descriptor->drmModifier, &props) &&
    //           (props.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT));
    // }

    // Verify that the format modifier of the external memory and the requested Vulkan format
    // are actually supported together in a dma-buf import.
    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {};
    imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    imageFormatInfo.format = vkFormat;
    imageFormatInfo.type = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    imageFormatInfo.usage = vkImageDesc->vkUsageFlags;
    imageFormatInfo.flags = 0;
    PNextChainBuilder imageFormatInfoChain(&imageFormatInfo);

    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo = {};
    externalImageFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    imageFormatInfoChain.Add(&externalImageFormatInfo,
                             VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO);

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT drmModifierInfo = {};
    drmModifierInfo.drmFormatModifier = descriptor->drmModifier;
    drmModifierInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageFormatInfoChain.Add(&drmModifierInfo,
                             VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT);

    // For mutable vkimage of multi-planar format, we also need to make sure the each
    // plane's view format can be supported.
    std::array<VkFormat, 2> viewFormats;
    VkImageFormatListCreateInfo imageFormatListInfo = {};

    if (planeCount > 1 && mayNeedView) {
        viewFormats = {
            VulkanImageFormat(device, internalFormat.GetAspectInfo(Aspect::Plane0).format),
            VulkanImageFormat(device, internalFormat.GetAspectInfo(Aspect::Plane1).format)};
        imageFormatListInfo.viewFormatCount = 2;
        imageFormatListInfo.pViewFormats = viewFormats.data();
        imageFormatInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        imageFormatInfoChain.Add(&imageFormatListInfo,
                                 VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);
    }

    VkImageFormatProperties2 imageFormatProps = {};
    imageFormatProps.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    PNextChainBuilder imageFormatPropsChain(&imageFormatProps);

    VkExternalImageFormatProperties externalImageFormatProps = {};
    imageFormatPropsChain.Add(&externalImageFormatProps,
                              VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES);

    DAWN_TRY_CONTEXT(CheckVkSuccess(device->fn.GetPhysicalDeviceImageFormatProperties2(
                                        vkPhysicalDevice, &imageFormatInfo, &imageFormatProps),
                                    "vkGetPhysicalDeviceImageFormatProperties"),
                     "checking import support for fd import of dma buf with %s %s\n",
                     properties.format, properties.usage);

    VkExternalMemoryFeatureFlags featureFlags =
        externalImageFormatProps.externalMemoryProperties.externalMemoryFeatures;
    DAWN_INVALID_IF(!(featureFlags & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT),
                    "Vulkan memory is not importable.");

    VkImageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = vkFormat;
    createInfo.extent = {vkImageDesc->vkExtent3D.width, vkImageDesc->vkExtent3D.height, 1};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = vkImageDesc->vkExtent3D.depthOrArrayLayers;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    createInfo.usage = vkImageDesc->vkUsageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    PNextChainBuilder createInfoChain(&createInfo);

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
    externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    createInfoChain.Add(&externalMemoryImageCreateInfo,
                        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);

    StackVector<VkSubresourceLayout, 3> planeLayouts;
    planeLayouts->resize(planeCount);
    for (uint32_t plane = 0u; plane < planeCount; ++plane) {
        planeLayouts[plane].offset = descriptor->planeOffsets[plane];
        planeLayouts[plane].size = 0;  // VK_EXT_image_drm_format_modifier mandates size = 0.
        planeLayouts[plane].rowPitch = descriptor->planeStrides[plane];
        planeLayouts[plane].arrayPitch = 0;  // Not an array texture
        planeLayouts[plane].depthPitch = 0;  // Not a depth texture
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT explicitCreateInfo = {};
    explicitCreateInfo.drmFormatModifier = descriptor->drmModifier;
    explicitCreateInfo.drmFormatModifierPlaneCount = planeCount;
    explicitCreateInfo.pPlaneLayouts = planeLayouts->data();

    if (planeCount > 1 && mayNeedView) {
        // For multi-planar formats, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT specifies that a
        // VkImageView can be plane's format which might differ from the image's format.
        createInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }
    createInfoChain.Add(&explicitCreateInfo,
                        VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);

    Ref<SharedTextureMemory> sharedTextureMemory =
        AcquireRef(new SharedTextureMemory(device, label, properties));

    // Create a new VkImage with tiling equal to the DRM format modifier.
    DAWN_TRY(CheckVkSuccess(
        device->fn.CreateImage(vkDevice, &createInfo, nullptr, &*sharedTextureMemory->mImage),
        "vkCreateImage"));

    ScopedFd fd = ScopedFd(dup(descriptor->memoryFD));

    VkMemoryFdPropertiesKHR fdProperties;
    fdProperties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    fdProperties.pNext = nullptr;

    // Get the valid memory types that the external memory can be imported as.
    DAWN_TRY(CheckVkSuccess(
        device->fn.GetMemoryFdPropertiesKHR(
            vkDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, *fd, &fdProperties),
        "vkGetMemoryFdPropertiesKHR"));

    // Get the valid memory types for the VkImage.
    VkMemoryRequirements memoryRequirements;
    device->fn.GetImageMemoryRequirements(vkDevice, sharedTextureMemory->mImage,
                                          &memoryRequirements);

    // Choose the best memory type that satisfies both the image's constraint and the
    // import's constraint.
    memoryRequirements.memoryTypeBits &= fdProperties.memoryTypeBits;
    int memoryTypeIndex = device->GetResourceMemoryAllocator()->FindBestTypeIndex(
        memoryRequirements, MemoryKind::Opaque);
    DAWN_INVALID_IF(memoryTypeIndex == -1, "Unable to find an appropriate memory type for import.");

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
    PNextChainBuilder memoryAllocateInfoChain(&memoryAllocateInfo);

    VkImportMemoryFdInfoKHR importMemoryFdInfo;
    importMemoryFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    importMemoryFdInfo.fd = *fd;
    memoryAllocateInfoChain.Add(&importMemoryFdInfo, VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR);

    DAWN_TRY(CheckVkSuccess(device->fn.AllocateMemory(vkDevice, &memoryAllocateInfo, nullptr,
                                                      &*sharedTextureMemory->mDeviceMemory),
                            "vkAllocateMemory"));
    fd.Detach();

    DAWN_TRY(CheckVkSuccess(device->fn.BindImageMemory(vkDevice, sharedTextureMemory->mImage,
                                                       sharedTextureMemory->mDeviceMemory, 0),
                            "vkBindImageMemory"));
    sharedTextureMemory->mQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL_KHR;
    return sharedTextureMemory;
}

// static
ResultOrError<Ref<SharedTextureMemory>> SharedTextureMemory::Create(
    Device* device,
    const char* label,
    const SharedTextureMemoryVkImageDescriptor* vkImageDesc,
    const SharedTextureMemoryOpaqueFDDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented");
}

// static
ResultOrError<Ref<SharedTextureMemory>> SharedTextureMemory::Create(
    Device* device,
    const char* label,
    const SharedTextureMemoryVkImageDescriptor* vkImageDesc,
    const SharedTextureMemoryZirconHandleDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented");
}

SharedTextureMemory::SharedTextureMemory(Device* device,
                                         const char* label,
                                         const SharedTextureMemoryProperties& properties)
    : SharedTextureMemoryBase(device, label, properties) {}

VkDeviceMemory SharedTextureMemory::GetVkDeviceMemory() const {
    return mDeviceMemory;
}

VkImage SharedTextureMemory::GetVkImage() const {
    return mImage;
}

uint32_t SharedTextureMemory::GetQueueFamilyIndex() const {
    return mQueueFamilyIndex;
}

void SharedTextureMemory::DestroyImpl() {
    if (mImage != VK_NULL_HANDLE) {
        ToBackend(GetDevice())->GetFencedDeleter()->DeleteWhenUnused(mImage);
        mImage = VK_NULL_HANDLE;
    }

    if (mDeviceMemory != VK_NULL_HANDLE) {
        ToBackend(GetDevice())->GetFencedDeleter()->DeleteWhenUnused(mDeviceMemory);
        mDeviceMemory = VK_NULL_HANDLE;
    }
}

ResultOrError<Ref<TextureBase>> SharedTextureMemory::CreateTextureImpl(
    const TextureDescriptor* descriptor) {
    return Texture::CreateFromSharedTextureMemory(this, descriptor);
}

MaybeError SharedTextureMemory::BeginAccessImpl(TextureBase* texture,
                                                const BeginAccessDescriptor* descriptor) {
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        SharedFenceBase* fence = descriptor->fences[i];

        SharedFenceExportInfo exportInfo;
        fence->APIExportInfo(&exportInfo);
        switch (exportInfo.type) {
            case wgpu::SharedFenceType::VkSemaphoreOpaqueFD:
                DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceVkSemaphoreOpaqueFD),
                                "Required feature (%s) for %s is missing.",
                                wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD,
                                wgpu::SharedFenceType::VkSemaphoreOpaqueFD);
                break;
            case wgpu::SharedFenceType::VkSemaphoreSyncFD:
                DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceVkSemaphoreSyncFD),
                                "Required feature (%s) for %s is missing.",
                                wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD,
                                wgpu::SharedFenceType::VkSemaphoreSyncFD);
                break;
            case wgpu::SharedFenceType::VkSemaphoreZirconHandle:
                DAWN_INVALID_IF(
                    !GetDevice()->HasFeature(Feature::SharedFenceVkSemaphoreZirconHandle),
                    "Required feature (%s) for %s is missing.",
                    wgpu::FeatureName::SharedFenceVkSemaphoreZirconHandle,
                    wgpu::SharedFenceType::VkSemaphoreZirconHandle);
                break;
            default:
                return DAWN_VALIDATION_ERROR("Unsupported fence type %s.", exportInfo.type);
        }
    }
    ToBackend(texture)->SetPendingAcquire(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    return {};
}

ResultOrError<FenceAndSignalValue> SharedTextureMemory::EndAccessImpl(TextureBase* texture) {
    ExternalSemaphoreHandle handle;
    VkImageLayout releasedOldLayout;
    VkImageLayout releasedNewLayout;

    DAWN_TRY(ToBackend(texture)->EndAccess(&handle, &releasedOldLayout, &releasedNewLayout));

    Ref<SharedFence> fence;

#if DAWN_PLATFORM_IS(FUCHSIA)
#elif DAWN_PLATFORM_IS(ANDROID) || DAWN_PLATFORM_IS(CHROMEOS)
#else
    SharedFenceVkSemaphoreOpaqueFDDescriptor desc;
    desc.handle = handle;

    DAWN_TRY_ASSIGN(fence,
                    SharedFence::Create(ToBackend(GetDevice()), "Internal VkSemaphore", &desc));
#endif
    return FenceAndSignalValue{std::move(fence), 1};
}

}  // namespace dawn::native::vulkan
