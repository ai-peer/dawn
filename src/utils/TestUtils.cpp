// Copyright 2020 The Dawn Authors
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

#include "utils/TestUtils.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "utils/TextureFormatUtils.h"
#include "utils/WGPUHelpers.h"

#include <vector>

namespace utils {

    uint32_t GetMinimumBytesPerRow(wgpu::TextureFormat format, uint32_t width) {
        const uint32_t bytesPerBlock = utils::GetTexelBlockSizeInBytes(format);
        return Align(bytesPerBlock * width, kTextureBytesPerRowAlignment);
    }

    // TODO(jiawei.shao@intel.com): support compressed texture formats
    TextureDataCopyLayout GetTextureDataCopyLayoutForTexture2DAtLevel(
        wgpu::TextureFormat format,
        wgpu::Extent3D textureSizeAtLevel0,
        uint32_t mipmapLevel,
        uint32_t rowsPerImage) {
        ASSERT(utils::GetTextureFormatBlockWidth(format) == 1 &&
               utils::GetTextureFormatBlockHeight(format) == 1);

        TextureDataCopyLayout layout;

        layout.mipSize = {textureSizeAtLevel0.width >> mipmapLevel,
                          textureSizeAtLevel0.height >> mipmapLevel, textureSizeAtLevel0.depth};

        layout.bytesPerRow = GetMinimumBytesPerRow(format, layout.mipSize.width);

        uint32_t appliedRowsPerImage = rowsPerImage > 0 ? rowsPerImage : layout.mipSize.height;
        layout.bytesPerImage = layout.bytesPerRow * appliedRowsPerImage;

        layout.byteLength =
            RequiredBytesInCopy(layout.bytesPerRow, appliedRowsPerImage, layout.mipSize, format);

        const uint32_t bytesPerBlock = utils::GetTexelBlockSizeInBytes(format);
        layout.texelBlocksPerRow = layout.bytesPerRow / bytesPerBlock;
        layout.texelBlocksPerImage = layout.bytesPerImage / bytesPerBlock;
        layout.texelBlockCount = layout.byteLength / bytesPerBlock;

        return layout;
    }

    uint64_t RequiredBytesInCopy(uint64_t bytesPerRow,
                                 uint64_t rowsPerImage,
                                 wgpu::Extent3D copyExtent,
                                 wgpu::TextureFormat textureFormat) {
        uint32_t blockSize = utils::GetTexelBlockSizeInBytes(textureFormat);
        uint32_t blockWidth = utils::GetTextureFormatBlockWidth(textureFormat);
        uint32_t blockHeight = utils::GetTextureFormatBlockHeight(textureFormat);
        ASSERT(copyExtent.width % blockWidth == 0);
        uint32_t widthInBlocks = copyExtent.width / blockWidth;
        ASSERT(copyExtent.height % blockHeight == 0);
        uint32_t heightInBlocks = copyExtent.height / blockHeight;
        return RequiredBytesInCopy(bytesPerRow, rowsPerImage, widthInBlocks, heightInBlocks,
                                   copyExtent.depth, blockSize);
    }

    uint64_t RequiredBytesInCopy(uint64_t bytesPerRow,
                                 uint64_t rowsPerImage,
                                 uint64_t widthInBlocks,
                                 uint64_t heightInBlocks,
                                 uint64_t depth,
                                 uint64_t bytesPerBlock) {
        uint64_t requiredBytesInCopy = 0;
        if (depth > 0) {
            if (heightInBlocks > 0) {
                // Last row:
                requiredBytesInCopy += widthInBlocks * bytesPerBlock;
                // Plus last image except for the last row:
                if (heightInBlocks > 1) {
                    requiredBytesInCopy += bytesPerRow * (heightInBlocks - 1);
                }
            }
            // Plus the rest of the copy except for the last image:
            if (depth > 1) {
                requiredBytesInCopy += bytesPerRow * rowsPerImage * (depth - 1);
            }
        }
        return requiredBytesInCopy;
    }

    uint64_t GetTexelCountInCopyRegion(uint64_t bytesPerRow,
                                       uint64_t rowsPerImage,
                                       wgpu::Extent3D copyExtent,
                                       wgpu::TextureFormat textureFormat) {
        return RequiredBytesInCopy(bytesPerRow, rowsPerImage, copyExtent, textureFormat) /
               utils::GetTexelBlockSizeInBytes(textureFormat);
    }

    void UnalignDynamicUploader(wgpu::Device device) {
        std::vector<uint8_t> data = {1};

        wgpu::TextureDescriptor descriptor = {};
        descriptor.size = {1, 1, 1};
        descriptor.format = wgpu::TextureFormat::R8Unorm;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        wgpu::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, {0, 0, 0});
        wgpu::TextureDataLayout textureDataLayout = utils::CreateTextureDataLayout(0, 0, 0);
        wgpu::Extent3D copyExtent = {1, 1, 1};

        // WriteTexture with exactly 1 byte of data.
        device.GetDefaultQueue().WriteTexture(&textureCopyView, data.data(), 1, &textureDataLayout,
                                              &copyExtent);
    }
}  // namespace utils
