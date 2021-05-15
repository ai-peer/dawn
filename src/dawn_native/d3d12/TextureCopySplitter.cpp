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

        // copyBytesPerRowPitch + byteOffsetInRowPitch > bytesPerRow
        uint32_t rowsPerImageInTexels = rowsPerImage * blockInfo.height;
        // The region's rows straddle the bytes per row. Split the copy into two copies:
        // copy the head block (h), and copy the tail block (t).
        //  |<------------- bytes per row ------------->|
        //
        //    _____________________________________________
        //   /                                           /|
        //  /                                           / |
        //  |-------------------------------------------| |
        //  |                                   hhhhhhhh| |
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
        //  |                                   hhhhhhhh| |
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~hhhhhhhh| |
        //  |-------------------------------------------|/

        //  Copy the tail block (Note that the alignedOffset will be repositioned for tail block.
        //  After recalculation, the tail may not start from the very beginning of each row):
        //    _______________________
        //   /                     /|
        //  /                     / |
        //  |---------------------| |
        //  |            ttttttttt| |
        //  |~~~~~~~~~~~~ttttttttt| |
        //  |~~~~~~~~~~~~ttttttttt| /
        //  |~~~~~~~~~~~~ttttttttt|/
        //  |---------------------|

        copy.count = 2;

        // copy 0 is used to copy the head block
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

        // copy 1 is used to copy the tail block
        // texelOffset.y is either 0 or 1. buffer offset for the tail block could be 1 row
        // (texelOffset.y is 0) or 2 rows (texelOffset.y is 1) away from alignedOffset of copy 0.
        uint64_t offsetForTail = alignedOffset + bytesPerRow * (texelOffset.y + 1);
        uint64_t alignedOffsetForTail =
            offsetForTail & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        Origin3D texelOffsetForTail = ComputeTexelOffsets(
            blockInfo, static_cast<uint32_t>(offsetForTail - alignedOffsetForTail), bytesPerRow);

        copy.copies[1].offset = alignedOffsetForTail;
        copy.copies[1].textureOffset.x = origin.x + copy.copies[0].copySize.width;
        copy.copies[1].textureOffset.y = origin.y;
        copy.copies[1].textureOffset.z = origin.z;

        copy.copies[1].bufferOffset = texelOffsetForTail;

        copy.copies[1].copySize.width = copySize.width - copy.copies[0].copySize.width;
        copy.copies[1].copySize.height = copySize.height - texelOffsetForTail.y;
        copy.copies[1].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        copy.copies[1].bufferSize = copySize;
        copy.copies[1].bufferSize.width =
            copy.copies[1].copySize.width + copy.copies[1].bufferOffset.x;

        if (texelOffset.y || texelOffsetForTail.y) {
            // If texelOffset.y > 0, there will be one more useless row at the top in copy 0. Then
            // we can't copy all rows for each depth slice. Otherwise, the data will be in a mess:
            // each depth slice should have 4 rows only, but we will copy 5 rows. And the first row
            // of each depth slice will be skipped. However, the data is compacted in buffer, which
            // means the first row for the 2nd, 3rd, etc., slice should not be skipped. Take the
            // figure below as an example (copySize.height is 4), copied rows on depth 0 will be row
            // 0 - 3, copied rows on depth 1 will be row 5 - 8 because row 4 is skipped (it should
            // be row 4
            // - 7), copied rows on depth 2 will be row 10 - 13 because row 9 is skipped (is should
            // be row 8 - 11), etc. You see, copied data on depth 1 and afterwards is totally wrong.
            // So we can copy (copySize.height - texelOffset.y) rows for the head block. Then copied
            // rows on depth 0 is row 0 - 2, copied rows on depth 1 is row 4 - 6, etc. And call
            // another CopyTextureRegion to copy the last row of the head block for each depth slice
            // (row 3, 7, 11, etc.). These two copies combined can get correct data.
            //
            //  Copy 0 (copy the head block) will be incorrect if we copy all rows for each slice
            //  in one shot when texelOffset.y > 0:
            //    ______________________
            //   /                    /|
            //  /                    / |
            //  |--------------------| |
            //  |                    | |
            //  |         hhhhhhhhhhh| |
            //  |~~~~~~~~~hhhhhhhhhhh| |
            //  |~~~~~~~~~hhhhhhhhhhh| /
            //  |~~~~~~~~~hhhhhhhhhhh|/
            //  |--------------------|

            // Likewise, texelOffsetForTail.y might be greater than 0. Then we need to call another
            // CopyTextureRegion to copy the last row of the tail block.

            //  Copy 1 (copy the tail block) will be incorrect if we copy all rows for each slice
            //  in one shot when texelOffsetForTail.y > 0:
            //    _______________________
            //   /                     /|
            //  /                     / |
            //  |---------------------| |
            //  |                     | |
            //  |                ttttt| |
            //  |~~~~~~~~~~~~~~~~ttttt| |
            //  |~~~~~~~~~~~~~~~~ttttt| /
            //  |~~~~~~~~~~~~~~~~ttttt|/
            //  |---------------------|

            // Note that if texelOffset.y or texelOffsetForTail.y is greater than 0, bytesPerRow is
            // definitely 256 (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT). And only one of them can be
            // greater than 0: If either one is greater than 0 (it is definitely 1), the other is
            // definitely none-zero. You can draw the figure and calculate it on paper (note that
            // we repositioned alignedOffsetForTail and compute texelOffsetForTail based on the new
            // values for the tail block).
            copy.count = 3;
            ASSERT(bytesPerRow == D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
            copy.copies[2].offset = alignedOffset + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
            // If textureoffset.y is 1, then the last row of head has not copied yet. Otherwise, if
            // texelOffsetForTail.y is 1, then the last row of tail has not copied yet.
            copy.copies[2].textureOffset.x =
                texelOffset.y ? copy.copies[0].textureOffset.x : copy.copies[1].textureOffset.x;
            copy.copies[2].textureOffset.y = origin.y + copySize.height - blockInfo.height;
            copy.copies[2].textureOffset.z = origin.z;
            copy.copies[2].copySize.width =
                texelOffset.y ? copy.copies[0].copySize.width : copy.copies[1].copySize.width;
            copy.copies[2].copySize.height = 1;
            copy.copies[2].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers;

            copy.copies[2].bufferOffset.x =
                texelOffset.y ? copy.copies[0].bufferOffset.x : copy.copies[1].bufferOffset.x;
            copy.copies[2].bufferOffset.y = copySize.height - 2;
            copy.copies[2].bufferOffset.z = 0;
            copy.copies[2].bufferSize = copySize;
            copy.copies[2].bufferSize.width =
                texelOffset.y ? copy.copies[0].bufferSize.width : copy.copies[1].bufferSize.width;
        }

        return copy;
    }  // namespace d3d12

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
