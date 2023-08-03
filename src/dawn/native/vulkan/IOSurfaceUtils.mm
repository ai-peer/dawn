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

#include "dawn/native/vulkan/IOSurfaceUtils.h"

#include "dawn/common/StackContainer.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/EnumMaskIterator.h"
#include "dawn/native/vulkan/BufferVk.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/TextureVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"

#include <CoreVideo/CVPixelBuffer.h>

namespace dawn::native::vulkan {

namespace {

struct IOSurfaceScopeLock : NonCopyable {
    IOSurfaceScopeLock(IOSurfaceRef iosurface) : mIOSurface(iosurface) {
        IOSurfaceLock(mIOSurface, 0, nullptr);
    }
    ~IOSurfaceScopeLock() { IOSurfaceUnlock(mIOSurface, 0, nullptr); }

  private:
    IOSurfaceRef mIOSurface;
};

struct IOSurfacePlaneInfo {
    void* baseAddress;
    uint32_t width;
    uint32_t height;
    uint32_t bytesPerRow;
};

Extent3D GetTextureAspectSize(TextureBase* texture, Aspect aspect) {
    Extent3D size = texture->GetMipLevelSingleSubresourcePhysicalSize(0);
    switch (aspect) {
        case Aspect::Color:
        case Aspect::Plane0:
            break;
        case Aspect::Plane1:
            size.width >>= 1;
            size.height >>= 1;
            break;
        default:
            UNREACHABLE();
    }

    // We don't really support non 2D texture.
    ASSERT(size.depthOrArrayLayers == 1);

    return size;
}

IOSurfacePlaneInfo GetIOSurfacePlaneInfo(IOSurfaceRef iosurface, Aspect aspect) {
    uint32_t planeIndex = 0;
    switch (aspect) {
        case Aspect::Color:
        case Aspect::Plane0:
            planeIndex = 0;
            break;
        case Aspect::Plane1:
            planeIndex = 1;
            break;
        default:
            UNREACHABLE();
    }

    IOSurfacePlaneInfo info;

    info.baseAddress = IOSurfaceGetBaseAddressOfPlane(iosurface, planeIndex);
    info.width = IOSurfaceGetWidthOfPlane(iosurface, planeIndex);
    info.height = IOSurfaceGetHeightOfPlane(iosurface, planeIndex);
    info.bytesPerRow = IOSurfaceGetBytesPerRowOfPlane(iosurface, planeIndex);

    return info;
}

ResultOrError<wgpu::TextureFormat> GetFormatEquivalentToIOSurfaceFormat(uint32_t format) {
    switch (format) {
        case kCVPixelFormatType_64RGBAHalf:
            return wgpu::TextureFormat::RGBA16Float;
        case kCVPixelFormatType_TwoComponent16Half:
            return wgpu::TextureFormat::RG16Float;
        case kCVPixelFormatType_OneComponent16Half:
            return wgpu::TextureFormat::R16Float;
        case kCVPixelFormatType_ARGB2101010LEPacked:
            return wgpu::TextureFormat::RGB10A2Unorm;
        case kCVPixelFormatType_32RGBA:
            return wgpu::TextureFormat::RGBA8Unorm;
        case kCVPixelFormatType_32BGRA:
            return wgpu::TextureFormat::BGRA8Unorm;
        case kCVPixelFormatType_TwoComponent8:
            return wgpu::TextureFormat::RG8Unorm;
        case kCVPixelFormatType_OneComponent8:
            return wgpu::TextureFormat::R8Unorm;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            return wgpu::TextureFormat::R8BG8Biplanar420Unorm;
        default:
            return DAWN_VALIDATION_ERROR("Unsupported IOSurface format (%x).", format);
    }
}

}  // namespace

MaybeError ValidateIOSurfaceCanBeImported(const TextureDescriptor* descriptor,
                                          IOSurfaceRef iosurface) {
    DAWN_INVALID_IF(descriptor->dimension != wgpu::TextureDimension::e2D,
                    "Texture dimension (%s) is not %s.", descriptor->dimension,
                    wgpu::TextureDimension::e2D);

    DAWN_INVALID_IF(descriptor->mipLevelCount != 1, "Mip level count (%u) is not 1.",
                    descriptor->mipLevelCount);

    DAWN_INVALID_IF(descriptor->size.depthOrArrayLayers != 1, "Array layer count (%u) is not 1.",
                    descriptor->size.depthOrArrayLayers);

    DAWN_INVALID_IF(descriptor->sampleCount != 1, "Sample count (%u) is not 1.",
                    descriptor->sampleCount);

    uint32_t surfaceWidth = IOSurfaceGetWidth(iosurface);
    uint32_t surfaceHeight = IOSurfaceGetHeight(iosurface);

    DAWN_INVALID_IF(
        descriptor->size.width != surfaceWidth || descriptor->size.height != surfaceHeight ||
            descriptor->size.depthOrArrayLayers != 1,
        "IOSurface size (width: %u, height %u, depth: 1) doesn't match descriptor size %s.",
        surfaceWidth, surfaceHeight, &descriptor->size);

    wgpu::TextureFormat iosurfaceFormat;
    DAWN_TRY_ASSIGN(iosurfaceFormat,
                    GetFormatEquivalentToIOSurfaceFormat(IOSurfaceGetPixelFormat(iosurface)));
    DAWN_INVALID_IF(descriptor->format != iosurfaceFormat,
                    "IOSurface format (%s) doesn't match the descriptor format (%s).",
                    iosurfaceFormat, descriptor->format);

    DAWN_INVALID_IF(
        descriptor->usage & wgpu::TextureUsage::TransientAttachment,
        "Usage flags (%s) include %s, which is not compatible with creation from IOSurface.",
        descriptor->usage, wgpu::TextureUsage::TransientAttachment);

    return {};
}

MaybeError ImportFromIOSurfaceToTexture(IOSurfaceRef iosurface, Texture* texture) {
    Device* device = ToBackend(texture->GetDevice());
    CommandRecordingContext* recordingContext = device->GetPendingRecordingContext();

    ASSERT(texture->GetNumMipLevels() == 1);
    ASSERT(texture->GetArrayLayers() == 1);
    ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
    ASSERT(!texture->GetFormat().isCompressed);

#if defined(DAWN_ENABLE_ASSERTS)
    wgpu::TextureFormat iosurfaceFormat;
    DAWN_TRY_ASSIGN(iosurfaceFormat,
                    GetFormatEquivalentToIOSurfaceFormat(IOSurfaceGetPixelFormat(iosurface)));
    ASSERT(iosurfaceFormat == texture->GetFormat().format);
#endif

    SubresourceRange range = texture->GetAllSubresources();
    const Format& format = texture->GetFormat();

    IOSurfaceScopeLock iosurfaceLock(iosurface);

    for (Aspect aspect : IterateEnumMask(range.aspects)) {
        const TexelBlockInfo& blockInfo = format.GetAspectInfo(aspect).block;
        Extent3D copySize = GetTextureAspectSize(texture, aspect);

        uint32_t usableBytesPerRow = (copySize.width / blockInfo.width) * blockInfo.byteSize;
        uint32_t bytesPerRow = Align(usableBytesPerRow, device->GetOptimalBytesPerRowAlignment());
        uint64_t bufferSize = bytesPerRow * (copySize.height / blockInfo.height);

        DynamicUploader* uploader = device->GetDynamicUploader();
        UploadHandle uploadHandle;
        DAWN_TRY_ASSIGN(
            uploadHandle,
            uploader->Allocate(bufferSize, device->GetPendingCommandSerial(), blockInfo.byteSize));

        auto iosurfacePlaneInfo = GetIOSurfacePlaneInfo(iosurface, aspect);
        ASSERT(copySize.width == iosurfacePlaneInfo.width);
        ASSERT(copySize.height == iosurfacePlaneInfo.height);

        auto* dstPtr = static_cast<uint8_t*>(uploadHandle.mappedBuffer);
        const auto* srcPtr = static_cast<uint8_t*>(iosurfacePlaneInfo.baseAddress);
        for (uint32_t row = 0; row < copySize.height; ++row) {
            memcpy(dstPtr, srcPtr, usableBytesPerRow);

            dstPtr += bytesPerRow;
            srcPtr += iosurfacePlaneInfo.bytesPerRow;
        }

        TextureDataLayout dataLayout;
        dataLayout.offset = uploadHandle.startOffset;
        dataLayout.rowsPerImage = copySize.height / blockInfo.height;
        dataLayout.bytesPerRow = bytesPerRow;
        TextureCopy textureCopy;
        textureCopy.aspect = aspect;
        textureCopy.mipLevel = 0;
        textureCopy.origin = {0, 0, 0};
        textureCopy.texture = texture;

        VkBufferImageCopy region = ComputeBufferImageCopyRegion(dataLayout, textureCopy, copySize);

        device->fn.CmdCopyBufferToImage(
            recordingContext->commandBuffer, ToBackend(uploadHandle.stagingBuffer)->GetHandle(),
            texture->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    return {};
}

MaybeError ExportFromTextureToIOSurface(Texture* texture, IOSurfaceRef iosurface) {
    Device* device = ToBackend(texture->GetDevice());
    CommandRecordingContext* recordingContext = device->GetPendingRecordingContext();

    ASSERT(texture->GetNumMipLevels() == 1);
    ASSERT(texture->GetArrayLayers() == 1);
    ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
    ASSERT(!texture->GetFormat().isCompressed);

#if defined(DAWN_ENABLE_ASSERTS)
    wgpu::TextureFormat iosurfaceFormat;
    DAWN_TRY_ASSIGN(iosurfaceFormat,
                    GetFormatEquivalentToIOSurfaceFormat(IOSurfaceGetPixelFormat(iosurface)));
    ASSERT(iosurfaceFormat == texture->GetFormat().format);
#endif

    SubresourceRange range = texture->GetAllSubresources();
    const Format& format = texture->GetFormat();

    texture->TransitionUsageNow(recordingContext, wgpu::TextureUsage::CopySrc, range);

    // Copy from texture to staging buffers
    struct StagingBuffer {
        Ref<BufferBase> buffer;
        Aspect aspect;
        uint32_t usableBytesPerRow;
        uint32_t bytesPerRow;
    };

    StackVector<StagingBuffer, 3> stagingBuffers;

    for (Aspect aspect : IterateEnumMask(range.aspects)) {
        if (!texture->IsSubresourceContentInitialized(SubresourceRange::MakeSingle(aspect, 0, 0))) {
            continue;
        }

        const TexelBlockInfo& blockInfo = format.GetAspectInfo(aspect).block;
        Extent3D copySize = GetTextureAspectSize(texture, aspect);

        uint32_t usableBytesPerRow = (copySize.width / blockInfo.width) * blockInfo.byteSize;
        uint32_t bytesPerRow = Align(usableBytesPerRow, device->GetOptimalBytesPerRowAlignment());
        uint64_t bufferSize = bytesPerRow * (copySize.height / blockInfo.height);

        BufferDescriptor bufferDesc = {};
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        bufferDesc.size = Align(bufferSize, 4);
        bufferDesc.mappedAtCreation = false;
        bufferDesc.label = "Dawn_StagingToIOSurface";

        IgnoreLazyClearCountScope scope(device);
        Ref<BufferBase> stagingBuffer;
        DAWN_TRY_ASSIGN(stagingBuffer, device->CreateBuffer(&bufferDesc));

        TextureDataLayout dataLayout;
        dataLayout.offset = 0;
        dataLayout.rowsPerImage = copySize.height / blockInfo.height;
        dataLayout.bytesPerRow = bytesPerRow;
        TextureCopy textureCopy;
        textureCopy.aspect = aspect;
        textureCopy.mipLevel = 0;
        textureCopy.origin = {0, 0, 0};
        textureCopy.texture = texture;

        VkBufferImageCopy region = ComputeBufferImageCopyRegion(dataLayout, textureCopy, copySize);

        device->fn.CmdCopyImageToBuffer(recordingContext->commandBuffer, texture->GetHandle(),
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        ToBackend(stagingBuffer)->GetHandle(), 1, &region);

        StagingBuffer bufferEntry;
        bufferEntry.buffer = stagingBuffer;
        bufferEntry.aspect = aspect;
        bufferEntry.usableBytesPerRow = usableBytesPerRow;
        bufferEntry.bytesPerRow = bytesPerRow;
        stagingBuffers->push_back(bufferEntry);
    }

    if (stagingBuffers->empty()) {
        return {};
    }

    // Submit the commands and wait for them to finish
    DAWN_TRY(device->SubmitPendingCommands());
    // TODO(dawn:1953): use semaphore to wait
    device->fn.QueueWaitIdle(device->GetQueue());

    // Copy from staging buffers to IOSurface
    IOSurfaceScopeLock iosurfaceLock(iosurface);

    for (StagingBuffer bufferEntry : stagingBuffers) {
        auto iosurfacePlaneInfo = GetIOSurfacePlaneInfo(iosurface, bufferEntry.aspect);

        auto* dstPtr = static_cast<uint8_t*>(iosurfacePlaneInfo.baseAddress);
        const auto* srcPtr = static_cast<uint8_t*>(bufferEntry.buffer->GetMappedPointer());
        for (uint32_t row = 0; row < iosurfacePlaneInfo.height; ++row) {
            memcpy(dstPtr, srcPtr, bufferEntry.usableBytesPerRow);

            dstPtr += iosurfacePlaneInfo.bytesPerRow;
            srcPtr += bufferEntry.bytesPerRow;
        }
    }

    return {};
}

}  // namespace dawn::native::vulkan
