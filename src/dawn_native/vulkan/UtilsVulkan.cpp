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

#include "dawn_native/vulkan/UtilsVulkan.h"

#include "common/Assert.h"
#include "dawn_native/Format.h"
#include "dawn_native/vulkan/Forward.h"
#include "dawn_native/vulkan/TextureVk.h"

#if defined(DAWN_PLATFORM_WINDOWS)
#    include <Windows.h>
#    include <array>
#elif defined(DAWN_PLATFORM_LINUX)
#    include <limits.h>
#    include <unistd.h>
#    include <cstdlib>
#else
#    error "Unimplemented Vulkan backend platform."
#endif

namespace dawn_native { namespace vulkan {

    VkCompareOp ToVulkanCompareOp(dawn::CompareFunction op) {
        switch (op) {
            case dawn::CompareFunction::Always:
                return VK_COMPARE_OP_ALWAYS;
            case dawn::CompareFunction::Equal:
                return VK_COMPARE_OP_EQUAL;
            case dawn::CompareFunction::Greater:
                return VK_COMPARE_OP_GREATER;
            case dawn::CompareFunction::GreaterEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case dawn::CompareFunction::Less:
                return VK_COMPARE_OP_LESS;
            case dawn::CompareFunction::LessEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case dawn::CompareFunction::Never:
                return VK_COMPARE_OP_NEVER;
            case dawn::CompareFunction::NotEqual:
                return VK_COMPARE_OP_NOT_EQUAL;
            default:
                UNREACHABLE();
        }
    }

    // Vulkan SPEC requires the source/destination region specified by each element of
    // pRegions must be a region that is contained within srcImage/dstImage. Here the size of
    // the image refers to the virtual size, while Dawn validates texture copy extent with the
    // physical size, so we need to re-calculate the texture copy extent to ensure it should fit
    // in the virtual size of the subresource.
    Extent3D ComputeTextureCopyExtent(const TextureCopy& textureCopy, const Extent3D& copySize) {
        Extent3D validTextureCopyExtent = copySize;
        const TextureBase* texture = textureCopy.texture.Get();
        Extent3D virtualSizeAtLevel = texture->GetMipLevelVirtualSize(textureCopy.mipLevel);
        if (textureCopy.origin.x + copySize.width > virtualSizeAtLevel.width) {
            ASSERT(texture->GetFormat().isCompressed);
            validTextureCopyExtent.width = virtualSizeAtLevel.width - textureCopy.origin.x;
        }
        if (textureCopy.origin.y + copySize.height > virtualSizeAtLevel.height) {
            ASSERT(texture->GetFormat().isCompressed);
            validTextureCopyExtent.height = virtualSizeAtLevel.height - textureCopy.origin.y;
        }

        return validTextureCopyExtent;
    }

    VkBufferImageCopy ComputeBufferImageCopyRegion(const BufferCopy& bufferCopy,
                                                   const TextureCopy& textureCopy,
                                                   const Extent3D& copySize) {
        const Texture* texture = ToBackend(textureCopy.texture.Get());

        VkBufferImageCopy region;

        region.bufferOffset = bufferCopy.offset;
        // In Vulkan the row length is in texels while it is in bytes for Dawn
        const Format& format = texture->GetFormat();
        ASSERT(bufferCopy.rowPitch % format.blockByteSize == 0);
        region.bufferRowLength = bufferCopy.rowPitch / format.blockByteSize * format.blockWidth;
        region.bufferImageHeight = bufferCopy.imageHeight;

        region.imageSubresource.aspectMask = texture->GetVkAspectMask();
        region.imageSubresource.mipLevel = textureCopy.mipLevel;
        region.imageSubresource.baseArrayLayer = textureCopy.arrayLayer;
        region.imageSubresource.layerCount = 1;

        region.imageOffset.x = textureCopy.origin.x;
        region.imageOffset.y = textureCopy.origin.y;
        region.imageOffset.z = textureCopy.origin.z;

        Extent3D imageExtent = ComputeTextureCopyExtent(textureCopy, copySize);
        region.imageExtent.width = imageExtent.width;
        region.imageExtent.height = imageExtent.height;
        region.imageExtent.depth = copySize.depth;

        return region;
    }

#if defined(DAWN_PLATFORM_WINDOWS)
    bool SetEnvironmentVar(const char* variableName, const char* value) {
        return (SetEnvironmentVariableA(variableName, value) == TRUE);
    }
#elif defined(DAWN_PLATFORM_POSIX)
    bool SetEnvironmentVar(const char* variableName, const char* value) {
        return (setenv(variableName, value, 1) == 0);
    }
#else
#    error "Implement SetEnvironmentVar for your platform."
#endif

#if defined(DAWN_PLATFORM_WINDOWS)
    std::string GetExecutableDirectory() {
        std::array<char, MAX_PATH> executableFileBuf;
        DWORD executablePathLen = GetModuleFileNameA(nullptr, executableFileBuf.data(),
                                                     static_cast<DWORD>(executableFileBuf.size()));
        std::string executablePath =
            executablePathLen > 0 ? std::string(executableFileBuf.data()) : "";
        size_t lastPathSepLoc = executablePath.find_last_of(SYSTEM_SEP);
        return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc)
                                                     : "";
    }
#elif defined(DAWN_PLATFORM_LINUX)
    std::string GetExecutableDirectory() {
        std::array<char, PATH_MAX> path;
        ssize_t result = readlink("/proc/self/exe", path.data(), PATH_MAX);
        std::string executablePath = std::string(path.data(), (result > 0) ? result : 0);
        size_t lastPathSepLoc = executablePath.find_last_of(SYSTEM_SEP);
        return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc)
                                                     : "";
    }
#else
#    error "Implement GetExecutableDirectory for your platform."
#endif

}}  // namespace dawn_native::vulkan
