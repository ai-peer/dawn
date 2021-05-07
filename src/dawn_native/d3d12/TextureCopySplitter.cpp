// Copyright 2017 The Dawn Authors
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

#include "dawn_native/d3d12/TextureCopySplitter.h"

#include "common/Assert.h"
#include "dawn_native/Format.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    namespace {
        Origin3D ComputeTexelOffsets(const TexelBlockInfo& blockInfo,
                                     uint32_t offset,
                                     uint32_t bytesPerRow) {
            ASSERT(bytesPerRow != 0);
            uint32_t byteOffsetX = offset % bytesPerRow;
            uint32_t byteOffsetY = offset - byteOffsetX;

            return {byteOffsetX / blockInfo.byteSize * blockInfo.width,
                    byteOffsetY / bytesPerRow * blockInfo.height, 0};
        }
    }  // namespace

    TextureCopySplit Compute2DTextureCopySplit(Origin3D origin,
                                               Extent3D copySize,
                                               const TexelBlockInfo& blockInfo,
                                               uint64_t offset,
                                               uint32_t bytesPerRow,
                                               uint32_t rowsPerImage) {
        TextureCopySplit copy;

        ASSERT(bytesPerRow % blockInfo.byteSize == 0);

        // The copies must be 512-aligned. To do this, we calculate the first 512-aligned address
        // preceding our data.
        uint64_t alignedOffset =
            offset & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        // If the provided offset to the data was already 512-aligned, we can simply copy the data
        // without further translation.
        if (offset == alignedOffset) {
            copy.count = 1;

            copy.copies[0].offset = alignedOffset;

            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;

            copy.copies[0].bufferOffset.x = 0;
            copy.copies[0].bufferOffset.y = 0;
            copy.copies[0].bufferOffset.z = 0;
            copy.copies[0].bufferSize = copySize;

            return copy;
        }

        ASSERT(alignedOffset < offset);
        ASSERT(offset - alignedOffset < D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        // We must reinterpret our aligned offset into X and Y offsets with respect to the row
        // pitch.
        //
        // You can visualize the data in the buffer like this:
        // |-----------------------++++++++++++++++++++++++++++++++|
        // ^ 512-aligned address   ^ Aligned offset               ^ End of copy data
        //
        // Now when you consider the row pitch, you can visualize the data like this:
        // |~~~~~~~~~~~~~~~~|
        // |~~~~~+++++++++++|
        // |++++++++++++++++|
        // |+++++~~~~~~~~~~~|
        // |<---row pitch-->|
        //
        // The X and Y offsets calculated in ComputeTexelOffsets can be visualized like this:
        // |YYYYYYYYYYYYYYYY|
        // |XXXXXX++++++++++|
        // |++++++++++++++++|
        // |++++++~~~~~~~~~~|
        // |<---row pitch-->|
        Origin3D texelOffset = ComputeTexelOffsets(
            blockInfo, static_cast<uint32_t>(offset - alignedOffset), bytesPerRow);

        ASSERT(texelOffset.z == 0);

        uint32_t copyBytesPerRowPitch = copySize.width / blockInfo.width * blockInfo.byteSize;
        uint32_t byteOffsetInRowPitch = texelOffset.x / blockInfo.width * blockInfo.byteSize;
        uint32_t rowsPerImageInTexels = rowsPerImage * blockInfo.height;
        if (copyBytesPerRowPitch + byteOffsetInRowPitch <= bytesPerRow) {
            // The region's rows fit inside the bytes per row. In this case, extend the width of the
            // PlacedFootprint and copy the buffer with an offset location
            //  |<------------- bytes per row ------------->|
            //
            //  |-------------------------------------------|
            //  |                                           |
            //  |                 +++++++++++++++++~~~~~~~~~|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++         |
            //  |-------------------------------------------|

            // Copy 0:
            //  |----------------------------------|
            //  |                                  |
            //  |                 +++++++++++++++++|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++|
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++|
            //  |----------------------------------|

            copy.count = 1;

            copy.copies[0].offset = alignedOffset;

            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;

            copy.copies[0].bufferOffset = texelOffset;
            copy.copies[0].bufferSize.width = copySize.width + texelOffset.x;
            copy.copies[0].bufferSize.height = rowsPerImageInTexels + texelOffset.y;
            copy.copies[0].bufferSize.depthOrArrayLayers = copySize.depthOrArrayLayers;

            return copy;
        }

        // The region's rows straddle the bytes per row. Split the copy into two copies
        //  |<------------- bytes per row ------------->|
        //
        //  |-------------------------------------------|
        //  |                                           |
        //  |                                   ++++++++|
        //  |+++++++++~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |+++++++++~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |+++++++++~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |+++++++++~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |+++++++++                                  |
        //  |-------------------------------------------|

        //  Copy 0:
        //  |-------------------------------------------|
        //  |                                           |
        //  |                                   ++++++++|
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~++++++++|
        //  |-------------------------------------------|

        //  Copy 1:
        //  |---------|
        //  |         |
        //  |         |
        //  |+++++++++|
        //  |+++++++++|
        //  |+++++++++|
        //  |+++++++++|
        //  |+++++++++|
        //  |---------|

        copy.count = 2;

        copy.copies[0].offset = alignedOffset;

        copy.copies[0].textureOffset = origin;

        ASSERT(bytesPerRow > byteOffsetInRowPitch);
        uint32_t texelsPerRow = bytesPerRow / blockInfo.byteSize * blockInfo.width;
        copy.copies[0].copySize.width = texelsPerRow - texelOffset.x;
        copy.copies[0].copySize.height = copySize.height;
        copy.copies[0].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        copy.copies[0].bufferOffset = texelOffset;
        copy.copies[0].bufferSize.width = texelsPerRow;
        copy.copies[0].bufferSize.height = rowsPerImageInTexels + texelOffset.y;
        copy.copies[0].bufferSize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        copy.copies[1].offset = alignedOffset;

        copy.copies[1].textureOffset.x = origin.x + copy.copies[0].copySize.width;
        copy.copies[1].textureOffset.y = origin.y;
        copy.copies[1].textureOffset.z = origin.z;

        ASSERT(copySize.width > copy.copies[0].copySize.width);
        copy.copies[1].copySize.width = copySize.width - copy.copies[0].copySize.width;
        copy.copies[1].copySize.height = copySize.height;
        copy.copies[1].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        copy.copies[1].bufferOffset.x = 0;
        copy.copies[1].bufferOffset.y = texelOffset.y + blockInfo.height;
        copy.copies[1].bufferOffset.z = 0;

        copy.copies[1].bufferSize.width = copy.copies[1].copySize.width;
        copy.copies[1].bufferSize.height = rowsPerImageInTexels + texelOffset.y + blockInfo.height;
        copy.copies[1].bufferSize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        return copy;
    }

    TextureCopySplits Compute2DTextureCopySplits(Origin3D origin,
                                                 Extent3D copySize,
                                                 const TexelBlockInfo& blockInfo,
                                                 uint64_t offset,
                                                 uint32_t bytesPerRow,
                                                 uint32_t rowsPerImage) {
        TextureCopySplits copies;

        const uint64_t bytesPerSlice = bytesPerRow * rowsPerImage;

        // The function ComputeTextureCopySplit() decides how to split the copy based on:
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT (512)
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
        // Each slice of a 2D array might need to be split, but because of the WebGPU
        // constraint that "bytesPerRow" must be a multiple of 256, all odd (resp. all even) slices
        // will be at an offset multiple of 512 of each other, which means they will all result in
        // the same 2D split. Thus we can just compute the copy splits for the first and second
        // slices, and reuse them for the remaining slices by adding the related offset of each
        // slice. Moreover, if "rowsPerImage" is even, both the first and second copy layers can
        // share the same copy split, so in this situation we just need to compute copy split once
        // and reuse it for all the slices.
        Extent3D copyOneLayerSize = copySize;
        Origin3D copyFirstLayerOrigin = origin;
        copyOneLayerSize.depthOrArrayLayers = 1;
        copyFirstLayerOrigin.z = 0;

        copies.copySubresources[0] = Compute2DTextureCopySplit(
            copyFirstLayerOrigin, copyOneLayerSize, blockInfo, offset, bytesPerRow, rowsPerImage);

        // When the copy only refers one texture 2D array layer, copies.copySubresources[1]
        // will never be used so we can safely early return here.
        if (copySize.depthOrArrayLayers == 1) {
            return copies;
        }

        if (bytesPerSlice % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0) {
            copies.copySubresources[1] = copies.copySubresources[0];
            copies.copySubresources[1].copies[0].offset += bytesPerSlice;
            copies.copySubresources[1].copies[1].offset += bytesPerSlice;
        } else {
            const uint64_t bufferOffsetNextLayer = offset + bytesPerSlice;
            copies.copySubresources[1] =
                Compute2DTextureCopySplit(copyFirstLayerOrigin, copyOneLayerSize, blockInfo,
                                          bufferOffsetNextLayer, bytesPerRow, rowsPerImage);
        }

        return copies;
    }

    TextureCopySplit Compute3DTextureCopySplit(Origin3D origin,
                                               Extent3D copySize,
                                               const TexelBlockInfo& blockInfo,
                                               uint64_t offset,
                                               uint32_t bytesPerRow,
                                               uint32_t rowsPerImage) {
        TextureCopySplit copy;

        ASSERT(bytesPerRow % blockInfo.byteSize == 0);

        // The copies must be 512-aligned. To do this, we calculate the first 512-aligned address
        // preceding our data.
        uint64_t alignedOffset =
            offset & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        // If the provided offset to the data was already 512-aligned, we can simply copy the data
        // without further translation.
        if (offset == alignedOffset) {
            copy.count = 1;

            copy.copies[0].offset = alignedOffset;

            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;

            copy.copies[0].bufferOffset.x = 0;
            copy.copies[0].bufferOffset.y = 0;
            copy.copies[0].bufferOffset.z = 0;

            copy.copies[0].bufferSize = copySize;

            return copy;
        }

        ASSERT(alignedOffset < offset);
        ASSERT(offset - alignedOffset < D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        // We must reinterpret our aligned offset into X and Y offsets with respect to the row
        // pitch.
        //
        // You can visualize the data in the buffer like this:
        // |-----------------------++++++++++++++++++++++++++++++++|
        // ^ 512-aligned address   ^ Aligned offset               ^ End of copy data
        //
        // Now when you consider the row pitch, you can visualize the data like this:
        //   __________________
        //  /                /|
        // /                / |
        // |----------------| |
        // |~~~~~~~~~~~~~~~~| |
        // |~~~~~+++++++++++| |
        // |++++++++++++++++| /
        // |+++++~~~~~~~~~~~|/
        // |<---row pitch-->|
        //
        // The X and Y offsets calculated in ComputeTexelOffsets can be visualized like this:
        //   __________________
        //  /                /|
        // /                / |
        // |----------------| |
        // |YYYYYYYYYYYYYYYY| |
        // |XXXXXX++++++++++| |
        // |++++++++++++++++| /
        // |++++++~~~~~~~~~~|/
        // |<---row pitch-->|
        Origin3D texelOffset = ComputeTexelOffsets(
            blockInfo, static_cast<uint32_t>(offset - alignedOffset), bytesPerRow);

        ASSERT(texelOffset.z == 0);

        uint32_t copyBytesPerRowPitch = copySize.width / blockInfo.width * blockInfo.byteSize;
        uint32_t byteOffsetInRowPitch = texelOffset.x / blockInfo.width * blockInfo.byteSize;
        uint32_t rowsPerImageInTexels = rowsPerImage * blockInfo.height;
        if (copyBytesPerRowPitch + byteOffsetInRowPitch <= bytesPerRow) {
            // The region's rows fit inside the bytes per row. In this case, extend the width of the
            // PlacedFootprint and copy the buffer with an offset location
            //  |<------------- bytes per row ------------->|
            //
            //    _____________________________________________
            //   /                                           /|
            //  /                                           / |
            //  |-------------------------------------------| |
            //  |                 +++++++++++++++++~~~~~~~~~| |
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~| |
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~| |
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~| /
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++         |/
            //  |-------------------------------------------|

            // Copy 0:
            //    ____________________________________
            //   /                                  /|
            //  /                                  / |
            //  |----------------------------------| |
            //  |                 +++++++++++++++++| |
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++| |
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++| |
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++| /
            //  |~~~~~~~~~~~~~~~~~+++++++++++++++++|/
            //  |----------------------------------|

            copy.count = 1;

            copy.copies[0].offset = alignedOffset;
            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;

            copy.copies[0].bufferOffset = texelOffset;
            copy.copies[0].bufferSize.width = copySize.width + texelOffset.x;
            copy.copies[0].bufferSize.height = rowsPerImageInTexels + texelOffset.y;
            copy.copies[0].bufferSize.depthOrArrayLayers = copySize.depthOrArrayLayers;

            return copy;
        }

        // The region's rows straddle the bytes per row. Split the copy into two copies:
        // copy the head block (h), and copy the tail block (t).
        //  |<------------- bytes per row ------------->|
        //
        //    _____________________________________________
        //   /                                           /|
        //  /                                           / |
        //  |-------------------------------------------| |
        //  |                                           | |
        //  |                                   hhhhhhhh| |
        //  |ttttttttt~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |ttttttttt~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |ttttttttt~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |ttttttttt~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |ttttttttt                                  | /
        //  |-------------------------------------------|/

        //  Copy the head block:
        //    _____________________________________________
        //   /                                           /|
        //  /                                           / |
        //  |-------------------------------------------| |
        //  |                                           | |
        //  |                                   hhhhhhhh| |
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |-------------------------------------------|/

        //  Copy the tail block (note that the alignedOffset will be repositioned for tail block and
        //  the first two rows are not in the block any more and disappeared):
        //    _______________________
        //   /                     /|
        //  /                     / |
        //  |---------------------| |
        //  |            ttttttttt| |
        //  |~~~~~~~~~~~~ttttttttt| |
        //  |~~~~~~~~~~~~ttttttttt| |
        //  |~~~~~~~~~~~~ttttttttt| /
        //  |~~~~~~~~~~~~ttttttttt|/
        //  |---------------------|

        copy.count = 2;

        copy.copies[0].offset = alignedOffset;

        copy.copies[0].textureOffset = origin;

        ASSERT(bytesPerRow > byteOffsetInRowPitch);
        uint32_t texelsPerRow = bytesPerRow / blockInfo.byteSize * blockInfo.width;
        copy.copies[0].copySize.width = texelsPerRow - texelOffset.x;
        copy.copies[0].copySize.height = copySize.height - texelOffset.y;
        copy.copies[0].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers - texelOffset.z;

        copy.copies[0].bufferOffset = texelOffset;

        copy.copies[0].bufferSize.width = texelsPerRow;
        copy.copies[0].bufferSize.height = rowsPerImageInTexels;
        copy.copies[0].bufferSize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        // Either textureOffset.y or textureOffset.z can be 1. And they can be both 0. But they
        // can't be both 1 because offset - alignedOffset is less than
        // D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT(512) and bytesPerRow is at least 256.
        ASSERT(!texelOffset.y || !texelOffset.z);
        uint64_t offsetForTail = alignedOffset + bytesPerRow * (1 + texelOffset.y + texelOffset.z);
        uint64_t alignedOffsetForTail =
            offsetForTail & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        Origin3D texelOffsetForTail = ComputeTexelOffsets(
            blockInfo, static_cast<uint32_t>(offsetForTail - alignedOffsetForTail), bytesPerRow);

        if (bytesPerRow > 256) {
            ASSERT(!texelOffset.y && !texelOffset.z);
            copy.copies[1].offset = alignedOffsetForTail;
            copy.copies[1].textureOffset.x = origin.x + copy.copies[0].copySize.width;
            copy.copies[1].textureOffset.y = origin.y;
            copy.copies[1].textureOffset.z = origin.z;

            copy.copies[1].bufferOffset = texelOffsetForTail;

            copy.copies[1].copySize.width = copySize.width - copy.copies[0].copySize.width;
            copy.copies[1].copySize.height = copySize.height;
            copy.copies[1].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers;

            copy.copies[1].bufferSize = copy.copies[1].copySize;
            copy.copies[1].bufferSize.width =
                copy.copies[1].copySize.width + copy.copies[1].bufferOffset.x;
        }

        // TODO(yunchao.he@intel.com): implement more copies. Some will run into bytesPerRow == 256.
        // When bytesPerRow is 256, we need to split copies into 3 parts. We need an extra copy for
        // either the last row for the head (when alignedOffsetForTail > alignedOffset),
        // or the last row for the tail (when alignedOffsetForTail == alignedOffset).

        return copy;
    }

    TextureCopySplits Compute3DTextureCopySplits(Origin3D origin,
                                                 Extent3D copySize,
                                                 const TexelBlockInfo& blockInfo,
                                                 uint64_t offset,
                                                 uint32_t bytesPerRow,
                                                 uint32_t rowsPerImage) {
        TextureCopySplits copies;

        // The function Compute3DTextureCopySplit() decides how to split the copy based on:
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT (512)
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
        // All depth slices of a 3D texture on the same mip level can be copied together, because
        // these depth slices belong to the same subresource. However, buffer offset in
        // CopyTextureRegion must be 512B-aligned (aka alignedOffset). It can't start at any given
        // buffer offset in real cases (aka offset). So data on the same row in buffer might be
        // split into different rows during copies. Take the data on the first row for example, it
        // might be split into two parts:
        // - the head might be still on the same row with alignedOffset.
        // - the tail will definitely on the following row after the head.
        // We can expand the head to the rest rows of this slice on y-axis, and expand it to the
        // rest of depth slices of this mip level on z-axis. The expanded data consists of a 3D
        // block: head block. Likewise, we can expand tail on y-axis and z-axis and get another
        // 3D block: tail block. Then issue two CopyTextureRegions for the head block and tail
        // block.
        // Sometimes, the last row of head block or tail block might exceed the whole 3D block
        // of copySize because alignedOffset is different from offset in buffer, so we might need
        // to issue a third CopyTextureRegion command to copy the last row of all depth slices.
        Extent3D copyOneLayerSize = copySize;
        Origin3D copyFirstLayerOrigin = origin;

        copies.copySubresources[0] = Compute3DTextureCopySplit(
            copyFirstLayerOrigin, copyOneLayerSize, blockInfo, offset, bytesPerRow, rowsPerImage);

        return copies;
    }

}}  // namespace dawn_native::d3d12
