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

    Texture2DCopySplit Compute2DTextureCopySplit(Origin3D origin,
                                                 Extent3D copySize,
                                                 const TexelBlockInfo& blockInfo,
                                                 uint64_t offset,
                                                 uint32_t bytesPerRow,
                                                 uint32_t rowsPerImage) {
        Texture2DCopySplit copy;

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

        // The function Compute2DTextureCopySplit() decides how to split the copy based on:
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT (512)
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
        // Each layer of a 2D array might need to be split, but because of the WebGPU
        // constraint that "bytesPerRow" must be a multiple of 256, all odd (resp. all even) layers
        // will be at an offset multiple of 512 of each other, which means they will all result in
        // the same 2D split. Thus we can just compute the copy splits for the first and second
        // layers, and reuse them for the remaining slices by adding the related offset of each
        // layer. Moreover, if "rowsPerImage" is even, both the first and second copy layers can
        // share the same copy split, so in this situation we just need to compute copy split once
        // and reuse it for all the layers.
        Extent3D copyOneLayerSize = copySize;
        Origin3D copyFirstLayerOrigin = origin;
        copyOneLayerSize.depthOrArrayLayers = 1;
        copyFirstLayerOrigin.z = 0;

        copies.copies2D[0] = Compute2DTextureCopySplit(
            copyFirstLayerOrigin, copyOneLayerSize, blockInfo, offset, bytesPerRow, rowsPerImage);

        // When the copy only refers one texture 2D array layer, copies.copies2D[1]
        // will never be used so we can safely early return here.
        if (copySize.depthOrArrayLayers == 1) {
            return copies;
        }

        if (bytesPerSlice % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0) {
            copies.copies2D[1] = copies.copies2D[0];
            copies.copies2D[1].copies[0].offset += bytesPerSlice;
            copies.copies2D[1].copies[1].offset += bytesPerSlice;
        } else {
            const uint64_t bufferOffsetNextLayer = offset + bytesPerSlice;
            copies.copies2D[1] =
                Compute2DTextureCopySplit(copyFirstLayerOrigin, copyOneLayerSize, blockInfo,
                                          bufferOffsetNextLayer, bytesPerRow, rowsPerImage);
        }

        return copies;
    }

    Texture2DCopySplit Compute3DTextureCopySplit(Origin3D origin,
                                                 Extent3D copySize,
                                                 const TexelBlockInfo& blockInfo,
                                                 uint64_t offset,
                                                 uint32_t bytesPerRow,
                                                 uint32_t rowsPerImage) {
        Texture2DCopySplit copy;

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
        if (copyBytesPerRowPitch + byteOffsetInRowPitch <= bytesPerRow) {
            // The region's rows fit inside the bytes per row. In this case, extend the width of the
            // PlacedFootprint and copy the buffer with an offset location
            //                |<------------- bytes per row ------------->|
            //
            //                 _____________________________________________
            //                /                                           /|
            //               /                                           / |
            //               |-------------------------------------------| |
            //  row (N - 1)  |                                           | |
            //  row N        |                 +++++++++++++++++~~~~~~~~~| |
            //  row (N + 1)  |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~| |
            //     ...       |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~| |
            //     ...       |~~~~~~~~~~~~~~~~~+++++++++++++++++~~~~~~~~~| /
            //     ...       |~~~~~~~~~~~~~~~~~+++++++++++++++++         |/
            //               |-------------------------------------------|

            // Copy 0:
            //                 ____________________________________
            //                /                                  /|
            //               /                                  / |
            //               |----------------------------------| |
            //  row (N - 1)  |                                  | |
            //  row N        |                 +++++++++++++++++| |
            //  row (N + 1)  |~~~~~~~~~~~~~~~~~+++++++++++++++++| |
            //     ...       |~~~~~~~~~~~~~~~~~+++++++++++++++++| /
            //     ...       |~~~~~~~~~~~~~~~~~+++++++++++++++++|/
            //               |----------------------------------|

            // Copy 1 (Copy 1 for the last row is optional, see the comments below):
            //                 ____________________________________
            //                /                                  /|
            //               /                                  / |
            //               |----------------------------------| |
            //               |                                  | /
            //  The last row |                 +++++++++++++++++|/
            //               |----------------------------------|

            copy.count = 1;

            copy.copies[0].offset = alignedOffset;
            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;
            copy.copies[0].copySize.height = copySize.height - texelOffset.y;

            copy.copies[0].bufferOffset = texelOffset;
            copy.copies[0].bufferSize = copySize;
            copy.copies[0].bufferSize.width = copySize.width + texelOffset.x;

            // If textureOffset.y is not zero, it means that alignedOffset is at row (N - 1)
            // but data in buffer starts from row N. So the last row of data in buffer is
            // conceptually out of the current depth slice. For 3D texture copies, we can't
            // put the last row into copy.copies[0] like 2D texture copy. Because we repositioned
            // every alignedOffset for each layer for 2D texture copies but there is
            // only one alignedOffset for all depth ranges in 3D texture copy. If we put the last
            // row into copy.copies[0] and make copy.copies[0].bufferSize.height bigger than the
            // height of real buffer size of each depth slice (and src box is bigger than copy
            // size) and reuse the code of 2D texture copy, the copied data will be in a mess:
            // each subsequent copy for the rest depth slices will copy this many rows and
            // skip the first row of each slice followed. However, the data for subsequent depth
            // slices are actually compacted in buffer, we don't need to skip the first row on
            // the following depth slices. So the wrongly skipped row will make the data layout
            // incorrect when copy data from buffer to texture.
            // As a result, we need a separate copy for the last row for this situation.
            if (texelOffset.y > 0) {
                copy.count = 2;
                uint64_t offsetForLastRow = offset + bytesPerRow * copy.copies[0].copySize.height;
                uint64_t alignedOffsetForLastRow =
                    offsetForLastRow &
                    ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

                Origin3D texelOffsetForLastRow = ComputeTexelOffsets(
                    blockInfo, static_cast<uint32_t>(offsetForLastRow - alignedOffsetForLastRow),
                    bytesPerRow);

                copy.copies[1].offset = alignedOffsetForLastRow;

                copy.copies[1].textureOffset = origin;
                copy.copies[1].textureOffset.y = origin.y + copy.copies[0].copySize.height;

                copy.copies[1].copySize = copySize;
                copy.copies[1].copySize.height = texelOffset.y;

                copy.copies[1].bufferOffset = texelOffsetForLastRow;
                copy.copies[1].bufferSize = copy.copies[0].bufferSize;
            }

            return copy;
        }

        // TODO (yunchao.he@intel.com): add code to handle the situation when copyBytesPerRowPitch +
        // byteOffsetInRowPitch > bytesPerRow
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
        // - the tail is very likely on the following row after the head.
        // We can expand the head to the rest rows of this slice on y-axis, and expand it to the
        // rest of depth slices of this mip level on z-axis. The expanded data consists of a 3D
        // block: head block. Likewise, we can expand tail on y-axis and z-axis and get another
        // 3D block: tail block. Then issue two CopyTextureRegion calls for the head block and
        // the tail block.
        // Sometimes, the last row of head block or tail block might exceed the whole 3D block
        // of copySize because alignedOffset is different from offset in buffer. So we may need
        // to issue a third CopyTextureRegion command to copy the last row of all depth slices.

        copies.copies2D[0] = Compute3DTextureCopySplit(origin, copySize, blockInfo, offset,
                                                       bytesPerRow, rowsPerImage);

        return copies;
    }

}}  // namespace dawn_native::d3d12
