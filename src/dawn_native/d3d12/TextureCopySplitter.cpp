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

    TextureCopySubresource ComputeTextureCopySubresource(Origin3D origin,
                                                         Extent3D copySize,
                                                         const TexelBlockInfo& blockInfo,
                                                         uint64_t offset,
                                                         uint32_t bytesPerRow,
                                                         uint32_t rowsPerImage) {
        TextureCopySubresource copy;

        ASSERT(bytesPerRow % blockInfo.byteSize == 0);

        // The copies must be 512-aligned. To do this, we calculate the first 512-aligned address
        // preceding our data.
        uint64_t alignedOffset =
            offset & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        // If the provided offset to the data was already 512-aligned, we can simply copy the data
        // without further translation.
        if (offset == alignedOffset) {
            copy.count = 1;

            copy.copies[0].alignedOffset = alignedOffset;
            copy.copies[0].textureOffset = origin;
            copy.copies[0].copySize = copySize;
            copy.copies[0].bufferOffset = {0, 0, 0};
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

            copy.copies[0].alignedOffset = alignedOffset;
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

        copy.copies[0].alignedOffset = alignedOffset;
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

        uint64_t offsetForCopy1 =
            offset + copy.copies[0].copySize.width / blockInfo.width * blockInfo.byteSize;
        uint64_t alignedOffsetForCopy1 =
            offsetForCopy1 & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
        Origin3D texelOffsetForCopy1 = ComputeTexelOffsets(
            blockInfo, static_cast<uint32_t>(offsetForCopy1 - alignedOffsetForCopy1), bytesPerRow);

        copy.copies[1].alignedOffset = alignedOffsetForCopy1;
        copy.copies[1].textureOffset.x = origin.x + copy.copies[0].copySize.width;
        copy.copies[1].textureOffset.y = origin.y;
        copy.copies[1].textureOffset.z = origin.z;

        ASSERT(copySize.width > copy.copies[0].copySize.width);
        copy.copies[1].copySize.width = copySize.width - copy.copies[0].copySize.width;
        copy.copies[1].copySize.height = copySize.height;
        copy.copies[1].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers;

        copy.copies[1].bufferOffset = texelOffsetForCopy1;
        copy.copies[1].bufferSize.width = copy.copies[1].copySize.width + texelOffsetForCopy1.x;
        copy.copies[1].bufferSize.height = rowsPerImageInTexels + texelOffsetForCopy1.y;
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

        const uint64_t bytesPerLayer = bytesPerRow * rowsPerImage;

        // The function ComputeTextureCopySubresource() decides how to split the copy based on:
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT (512)
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
        // Each layer of a 2D array might need to be split, but because of the WebGPU
        // constraint that "bytesPerRow" must be a multiple of 256, all odd (resp. all even) layers
        // will be at an offset multiple of 512 of each other, which means they will all result in
        // the same 2D split. Thus we can just compute the copy splits for the first and second
        // layers, and reuse them for the remaining layers by adding the related offset of each
        // layer. Moreover, if "rowsPerImage" is even, both the first and second copy layers can
        // share the same copy split, so in this situation we just need to compute copy split once
        // and reuse it for all the layers.
        Extent3D copyOneLayerSize = copySize;
        Origin3D copyFirstLayerOrigin = origin;
        copyOneLayerSize.depthOrArrayLayers = 1;
        copyFirstLayerOrigin.z = 0;

        copies.copySubresources[0] = ComputeTextureCopySubresource(
            copyFirstLayerOrigin, copyOneLayerSize, blockInfo, offset, bytesPerRow, rowsPerImage);

        // When the copy only refers one texture 2D array layer,
        // copies.copySubresources[1] will never be used so we can safely early return here.
        if (copySize.depthOrArrayLayers == 1) {
            return copies;
        }

        if (bytesPerLayer % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0) {
            copies.copySubresources[1] = copies.copySubresources[0];
            copies.copySubresources[1].copies[0].alignedOffset += bytesPerLayer;
            copies.copySubresources[1].copies[1].alignedOffset += bytesPerLayer;
        } else {
            const uint64_t bufferOffsetNextLayer = offset + bytesPerLayer;
            copies.copySubresources[1] =
                ComputeTextureCopySubresource(copyFirstLayerOrigin, copyOneLayerSize, blockInfo,
                                              bufferOffsetNextLayer, bytesPerRow, rowsPerImage);
        }

        return copies;
    }

    void Recompute3DTextureCopyRegionsForBlockWithEmptyFirstRow(Origin3D origin,
                                                                Extent3D copySize,
                                                                const TexelBlockInfo& blockInfo,
                                                                uint64_t offset,
                                                                uint32_t bytesPerRow,
                                                                uint32_t rowsPerImage,
                                                                TextureCopySubresource& copy,
                                                                uint32_t i) {
        // Let's assign data and show why copy region generated by ComputeTextureCopySubresource
        // is incorrect if there is an empty row at the beginning of the copy block.
        // Assuming that bytesPerRow is 256 and we are dong a B2T copy, and copy size is {width: 2,
        // width: 4, depthOrArrayLayers: 3}. Then the data layout in buffer is demonstrated
        // as below. Image 0 is from row N to row (N + 3). Image 1 is from row (N + 4) to
        // row (N + 7), and image 3 is from row (N + 8) to row (N + 11). Note that
        // alignedOffset is at the beginning of row (N - 1), while real data offset is at
        // somewhere in row N. Row (N - 1) is the empty row between alignedOffset and offset.
        //               |<----- bytes per row ------>|
        //
        //               |----------------------------|
        //  row (N - 1)  |                            |
        //  row N        |                 ++~~~~~~~~~|
        //  row (N + 1)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 2)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 3)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 4)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 5)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 6)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 7)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 8)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 9)  |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 10) |~~~~~~~~~~~~~~~~~++~~~~~~~~~|
        //  row (N + 11) |~~~~~~~~~~~~~~~~~++         |
        //               |----------------------------|

        // Copied data for the first slice (layer) is showed below if it is a 2D texture.
        // Note that the copy block is 5 rows from row (N - 1) to row (N + 3), but row (N - 1)
        // is skipped via parameter bufferOffset. Likewise, we will recalculate alignedOffset
        // and real buffer offset for copy block of image 1. Copy block for image 1 will be
        // from row (N + 3) to row (N + 7). Row (N + 3) in copy block of image 1 overlaps
        // with copy block of image 0. But it won't be copied twice because it is skipped in
        // copy block of image 1, just like row (N - 1) is skipped in copy block of image 0.
        // So all data will be copied correctly for 2D texture copy. However, if we expand the
        // computed copy block of image 0 to all depth ranges of a 3D texture, we have no chance
        // to recompute alignedOffset and real buffer offset for each depth slice. So we will copy
        // 5 rows every time, and every first row of each slice will be skipped. As a result, the
        // copied data for image 0 will be from row N to row (N + 3), which is correct. But copied
        // data for image 1 will be from row (N + 5) to row (N + 8) because row (N + 4) is skipped.
        // It is incorrect. And all other image follow will be incorrect.
        //              |-------------------|
        //  row (N - 1) |                   |
        //  row N       |                 ++|
        //  row (N + 1) |~~~~~~~~~~~~~~~~~++|
        //  row (N + 2) |~~~~~~~~~~~~~~~~~++|
        //  row (N + 3) |~~~~~~~~~~~~~~~~~++|
        //              |-------------------|
        //
        // Solution: copy 3 rows in copy 0, and expand to all depth slices. 3 rows + one skipped
        // rows = 4 rows, which equals to rowsPerImage. Then copy the last row in copy 1,
        // and expand to copySize.depthOrArrayLayers - 1 depth slices. And copy the last row of
        // the last depth slice in copy 2.

        // Copy 0: copy 3 rows, not 4 rows.
        //                _____________________
        //               /                    /|
        //              /                    / |
        //              |-------------------|  |
        //  row (N - 1) |                   |  |
        //  row N       |                 ++|  |
        //  row (N + 1) |~~~~~~~~~~~~~~~~~++| /
        //  row (N + 2) |~~~~~~~~~~~~~~~~~++|/
        //              |-------------------|

        // Copy 1: copy one single row (the last row on image 0), and expand to z-axis but only
        // expand to (copySize.depthOrArrayLayers - 1) depth slices. Note that if we expand it
        // to all depth slices, the last copy block will be row (N + 11) to row (N + 14).
        // row (N + 11) might be the last row of the entire buffer, and the rest rows might
        // be out-of-bound. Then we will get a validation error. So we need a final copy to copy
        // the last row of the entire copy block.
        //                _____________________
        //               /                    /|
        //              /                    / |
        //              |-------------------|  |
        //  row (N + 3) |                 ++|  |
        //  row (N + 4) |~~~~~~~~~~~~~~~~~~~|  |
        //  row (N + 5) |~~~~~~~~~~~~~~~~~~~| /
        //  row (N + 6) |~~~~~~~~~~~~~~~~~~~|/
        //              |-------------------|
        //
        //  copy 2: copy the last row of the last image.
        //              |-------------------|
        //  row (N + 11)|                 ++|
        //              |-------------------|

        // copy 0: copy copySize.height - 1 rows
        copy.copies[i].copySize.height = copySize.height - blockInfo.height;
        copy.copies[i].bufferSize.height = rowsPerImage;

        uint64_t offsetForLastRow = offset + bytesPerRow * copy.copies[i].copySize.height;
        uint64_t alignedOffsetForLastRow =
            offsetForLastRow & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        Origin3D texelOffsetForLastRow = ComputeTexelOffsets(
            blockInfo, static_cast<uint32_t>(offsetForLastRow - alignedOffsetForLastRow),
            bytesPerRow);

        copy.copies[i + 1].alignedOffset = alignedOffsetForLastRow;

        copy.copies[i + 1].textureOffset = copy.copies[i].textureOffset;
        copy.copies[i + 1].textureOffset.y = origin.y + copy.copies[i].copySize.height;

        copy.copies[i + 1].copySize.width = copy.copies[i].copySize.width;
        copy.copies[i + 1].copySize.height = blockInfo.height;
        copy.copies[i + 1].copySize.depthOrArrayLayers = copySize.depthOrArrayLayers - 1;

        copy.copies[i + 1].bufferOffset = texelOffsetForLastRow;
        copy.copies[i + 1].bufferSize = copy.copies[i].bufferSize;
        copy.copies[i + 1].bufferSize.depthOrArrayLayers = copySize.depthOrArrayLayers - 1;

        uint64_t bytesPerImage = bytesPerRow * rowsPerImage;
        uint64_t offsetForLastRowOfLastImage =
            offsetForLastRow + bytesPerImage * (copySize.depthOrArrayLayers - 1);
        uint64_t alignedOffsetForLastRowOfLastImage =
            offsetForLastRowOfLastImage &
            ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        Origin3D texelOffsetForLastRowOfLastImage = ComputeTexelOffsets(
            blockInfo,
            static_cast<uint32_t>(offsetForLastRowOfLastImage - alignedOffsetForLastRowOfLastImage),
            bytesPerRow);

        copy.copies[i + 2].alignedOffset = alignedOffsetForLastRowOfLastImage;
        copy.copies[i + 2].textureOffset = copy.copies[i + 1].textureOffset;
        copy.copies[i + 2].textureOffset.z = origin.z + copySize.depthOrArrayLayers - 1;
        copy.copies[i + 2].copySize = copy.copies[i + 1].copySize;
        copy.copies[i + 2].copySize.depthOrArrayLayers = 1;
        copy.copies[i + 2].bufferOffset = texelOffsetForLastRowOfLastImage;
        copy.copies[i + 2].bufferSize.width = copy.copies[i + 1].bufferSize.width;
        ASSERT(copy.copies[i + 2].copySize.height == 1);
        copy.copies[i + 2].bufferSize.height =
            copy.copies[i + 2].bufferOffset.y + copy.copies[i + 2].copySize.height;
        copy.copies[i + 2].bufferSize.depthOrArrayLayers = 1;
    }

    void Compute3DTextureCopySubresourceForSpecialCases(Origin3D origin,
                                                        Extent3D copySize,
                                                        const TexelBlockInfo& blockInfo,
                                                        uint64_t offset,
                                                        uint32_t bytesPerRow,
                                                        uint32_t rowsPerImage,
                                                        TextureCopySubresource& copy,
                                                        uint32_t i) {
        // If there is an empty row at the beginning of any copy region because of alignment
        // adjustment, we need to compute all copy regions in different approach. These empty
        // first row cases can be divided into a few scenarios:
        //     - If copySize.height is greater than 1, there are two subcases:
        //         - data in one row in original layout never straddle two rows in new layout.
        //         - data in one row in original layout straddles two rows in new layout due to
        //           alignment adjustment.
        //     - If copySize.height is 1. This is an even more special case. It also includes
        //       two subcases depending on wheter data in one row in original layout straddle
        //       two rows or not due to alignment adjustment.

        ASSERT(bytesPerRow == D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        if (copy.count == 1) {
            // If copy.count is 1, it means data in one row in original layout never straddle rows.
            // See comments in Compute3DTextureCopyRegionsForBlockWithEmptyFirstRow() for details
            // about how to modify the copy regions.
            copy.count = 3;
            Recompute3DTextureCopyRegionsForBlockWithEmptyFirstRow(
                origin, copySize, blockInfo, offset, bytesPerRow, rowsPerImage, copy, 0);
        } else {
            // If copy.count is 2, it means data in one row in original layout straddles rows.
            // We divide the data on each row into two blocks: the head block, and the tail block.
            // Then either the head block or the tail block has an empty row. And the other block
            // has no empty first row issue, which means that its copy region is correct.
            //
            // Case 0: the head block has an empty row.
            // Note that alignedOffset is at the beginning of row (N - 1), but real buffer offset
            // of head block start at somewhere in row N.
            //              |<------- bytes per row ------>|
            //
            //              |------------------------------|
            //  row (N - 1) |                              |  <--- an empty row for the head block
            //  row N       |                      hhhhhhhh|
            //  row (N + 1) |ttttttttt~~~~~~~~~~~~~hhhhhhhh|
            //  row (N + 2) |ttttttttt~~~~~~~~~~~~~hhhhhhhh|
            //  row (N + 3) |ttttttttt~~~~~~~~~~~~~hhhhhhhh|
            //                            ......
            //  row (N + x) |ttttttttt                     |
            //              |------------------------------|
            //
            // Case 1: the tail block has an empty row
            // Note that alignedOffset is at the beginning of row N, there is no empty row for the
            // head block. However, buffer offset of the tail block is at the beginning of row (N +
            // 1), So row N turnout to be an empty first row for the tail block.
            //  |<------- bytes per row ------>|
            //
            //              |------------------------------|
            //  row N       |                      hhhhhhhh|  <--- an empty row for the tail block
            //  row (N + 1) |ttttttttt~~~~~~~~~~~~~hhhhhhhh|
            //  row (N + 2) |ttttttttt~~~~~~~~~~~~~hhhhhhhh|
            //  row (N + 3) |ttttttttt~~~~~~~~~~~~~hhhhhhhh|
            //                            ......
            //  row (N + x) |ttttttttt                     |
            //              |------------------------------|
            copy.count = 4;
            if (i == 0) {
                // If i is 0, it means the head block has an empty first row, it is case 0.
                // Copy region for tail block (copy.copies[1]) is correct, we move it to
                // copy.copies[3] because we need 3 copy regions for the head block.
                // Then we can call Recompute3DTextureCopyRegionsForBlockWithEmptyFirstRow
                // for the head block.
                copySize.width -= copy.copies[1].copySize.width;
                copy.copies[3] = copy.copies[1];
                Recompute3DTextureCopyRegionsForBlockWithEmptyFirstRow(
                    origin, copySize, blockInfo, offset, bytesPerRow, rowsPerImage, copy, i);
            } else {
                // Case 1: the tail block has no an empty first row. We need to call function
                // Recompute3DTextureCopyRegionsForBlockWithEmptyFirstRow for the tail block.
                copySize.width -= copy.copies[0].copySize.width;
                Recompute3DTextureCopyRegionsForBlockWithEmptyFirstRow(
                    origin, copySize, blockInfo, copy.copies[0].alignedOffset + bytesPerRow,
                    bytesPerRow, rowsPerImage, copy, i);
            }
        }
    }

    TextureCopySubresource Compute3DTextureCopySplits(Origin3D origin,
                                                      Extent3D copySize,
                                                      const TexelBlockInfo& blockInfo,
                                                      uint64_t offset,
                                                      uint32_t bytesPerRow,
                                                      uint32_t rowsPerImage) {
        // The function ComputeTextureCopySubresource() decides how to split the copy based on:
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT (512)
        // - the alignment of the buffer offset with D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
        // It is used to split copy regions for a layer of 2D textures. We can reuse it for 3D
        // texture copy and this function has already had the ability to expand 2D copy regions
        // on z-axis, which is depth, for 3D textures. However, there might be an empty row
        // at the beginning of a copy region due to alignment adjustment. In this situation,
        // copies[i].bufferSize.height may exceed bufferSize.height for every depth image when
        // we expand it on z-axis. We meant to skip the empty first row for one single layer for
        // 2D textures. But when we expand it to 3D textures on z-axis, every first row on each
        // depth image will be skipped, making the copied data in a mess. So we need to
        // recompute copy regions for this special situation. You can see the details in
        // Compute3DTextureCopySubresourceForSpecialCases()). Other than that special situation,
        // we can reuse copy regions generated by ComputeTextureCopySubresource().
        TextureCopySubresource copySubresource = ComputeTextureCopySubresource(
            origin, copySize, blockInfo, offset, bytesPerRow, rowsPerImage);

        ASSERT(copySubresource.count <= 2);
        bool needRecompute = false;
        uint32_t i = 0;
        for (; i < copySubresource.count; ++i) {
            if (copySubresource.copies[i].bufferSize.height > rowsPerImage) {
                needRecompute = true;
                break;
            }
        }

        // If copySize.depth is 1, we can return copySubresource directly even there is an empty
        // first row at any copy region. We will never wrongly skip first row(s) on other depth
        // image because there is only one depth image.
        if (copySize.depthOrArrayLayers == 1 || !needRecompute) {
            return copySubresource;
        }
        Compute3DTextureCopySubresourceForSpecialCases(
            origin, copySize, blockInfo, offset, bytesPerRow, rowsPerImage, copySubresource, i);
        return copySubresource;
    }
}}  // namespace dawn_native::d3d12
