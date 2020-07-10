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
        Origin3D ComputeTexelOffsets(const Format& format,
                                     uint32_t offset,
                                     uint32_t bytesPerRow,
                                     uint32_t slicePitch) {
            ASSERT(bytesPerRow != 0);
            ASSERT(slicePitch != 0);
            uint32_t byteOffsetX = offset % bytesPerRow;
            offset -= byteOffsetX;
            uint32_t byteOffsetY = offset % slicePitch;
            uint32_t byteOffsetZ = offset - byteOffsetY;

            return {byteOffsetX / format.blockByteSize * format.blockWidth,
                    byteOffsetY / bytesPerRow * format.blockHeight, byteOffsetZ / slicePitch};
        }
    }  // namespace

    TextureCopySplit ComputeTextureCopySplit(Origin3D origin,
                                             Extent3D copySize,
                                             const Format& format,
                                             uint64_t offset,
                                             uint32_t bytesPerRow,
                                             uint32_t rowsPerImage) {
        TextureCopySplit copy;

        ASSERT(bytesPerRow % format.blockByteSize == 0);

        uint64_t alignedOffset =
            offset & ~static_cast<uint64_t>(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);

        copy.offset = alignedOffset;
        if (offset == alignedOffset) {
            copy.count = 1;

            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;

            copy.copies[0].bufferOffset.x = 0;
            copy.copies[0].bufferOffset.y = 0;
            copy.copies[0].bufferOffset.z = 0;
            copy.copies[0].bufferSize = copySize;

            // Return early. There is only one copy needed because the offset is already 512-byte
            // aligned
            return copy;
        }

        ASSERT(alignedOffset < offset);
        ASSERT(offset - alignedOffset < D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        uint32_t slicePitch = bytesPerRow * (rowsPerImage / format.blockHeight);
        Origin3D texelOffset = ComputeTexelOffsets(
            format, static_cast<uint32_t>(offset - alignedOffset), bytesPerRow, slicePitch);

        uint32_t copyBytesPerRowPitch = copySize.width / format.blockWidth * format.blockByteSize;
        uint32_t byteOffsetInRowPitch = texelOffset.x / format.blockWidth * format.blockByteSize;
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

            copy.copies[0].textureOffset = origin;

            copy.copies[0].copySize = copySize;

            copy.copies[0].bufferOffset = texelOffset;
            copy.copies[0].bufferSize.width = copySize.width + texelOffset.x;
            copy.copies[0].bufferSize.height = rowsPerImage + texelOffset.y;
            copy.copies[0].bufferSize.depth = copySize.depth + texelOffset.z;

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

        copy.copies[0].textureOffset = origin;

        ASSERT(bytesPerRow > byteOffsetInRowPitch);
        uint32_t texelsPerRow = bytesPerRow / format.blockByteSize * format.blockWidth;
        copy.copies[0].copySize.width = texelsPerRow - texelOffset.x;
        copy.copies[0].copySize.height = copySize.height;
        copy.copies[0].copySize.depth = copySize.depth;

        copy.copies[0].bufferOffset = texelOffset;
        copy.copies[0].bufferSize.width = texelsPerRow;
        copy.copies[0].bufferSize.height = rowsPerImage + texelOffset.y;
        copy.copies[0].bufferSize.depth = copySize.depth + texelOffset.z;

        copy.copies[1].textureOffset.x = origin.x + copy.copies[0].copySize.width;
        copy.copies[1].textureOffset.y = origin.y;
        copy.copies[1].textureOffset.z = origin.z;

        ASSERT(copySize.width > copy.copies[0].copySize.width);
        copy.copies[1].copySize.width = copySize.width - copy.copies[0].copySize.width;
        copy.copies[1].copySize.height = copySize.height;
        copy.copies[1].copySize.depth = copySize.depth;

        copy.copies[1].bufferOffset.x = 0;
        copy.copies[1].bufferOffset.y = texelOffset.y + format.blockHeight;
        copy.copies[1].bufferOffset.z = texelOffset.z;
        copy.copies[1].bufferSize.width = copy.copies[1].copySize.width;
        copy.copies[1].bufferSize.height = rowsPerImage + texelOffset.y + format.blockHeight;
        copy.copies[1].bufferSize.depth = copySize.depth + texelOffset.z;

        return copy;
    }

    TextureCopySplits ComputeTextureCopySplits(Origin3D origin,
                                               Extent3D copySize,
                                               const Format& format,
                                               uint64_t offset,
                                               uint32_t bytesPerRow,
                                               uint32_t rowsPerImage) {
        TextureCopySplits copies;

        const uint64_t bytesPerSlice = bytesPerRow * (rowsPerImage / format.blockHeight);

        // For copies with each texture copy layer, they share almost the same parameters as the
        // copies of the first texture copy layer except "bufferOffset" and copyOrigin.z. The
        // "bufferOffset" of copies at i-th layer is (offset + i * bytesPerRow * rowsPerImage) (the
        // first layer is the 0-th one). In WebGPU, "bytesPerRow" must be a multiple of 256, so we
        // can divide all B2T and T2B copies into two situations:
        // 1. bytesPerSlice = 2 * n * 256 (n is an integer, n >= 1):
        //    offsetForLayerI % 512 == (offsetForLayerI + bytesPerSlice) % 512, so in function
        //    ComputeTextureCopySplitPerLayer():
        //    - the results of "offset == alignedOffset" are the same at every layer
        //    (alignedOffset = offset - offset % 512).
        //    - the value of "offset - alignedOffset" are the same at every layer
        //    (offset - alignedOffset = offset % 512).
        //    Given other paramters are the same for all the layers, we can conclude that the
        //    result of ComputeTextureCopySplitPerLayer() are the same for all the layers except
        //    offsetAtCopyLayeri = offset + bytesPerSlice * i.
        // 2. bytesPerSlice = (2 * n + 1) * 256:
        //    offsetForLayerI % 512 == (offsetForLayerI + bytesPerSlice * 2) % 512, so in function
        //    ComputeTextureCopySplitPerLayer():
        //    - the result of "offset == alignedOffset" at the i-th layer is equal to the one at
        //    the (i + 2)-th layer.
        //    - the value of "offset - alignedOffset" at the i-th layer is equal to the one at
        //    the (i + 2)-th layer.
        //    Given other paramters are the same for all the layers, we can conclude that:
        //    - the result of ComputeTextureCopySplitPerLayer() at layer 0 are the same for all
        //      (2 * n)-th layer except (offsetAtCopyLayeri = offsetAtCopyLayer0 + bytesPerSlice
        //      * i).
        //    - the result of ComputeTextureCopySplitPerLayer() at layer 1 are the same for all
        //      (2 * n + 1)-th layer except (offsetAtCopyLayeri = offsetAtCopyLayer1 + (i - 1)
        //      * bytesPerSlice).

        const dawn_native::Extent3D copyOneLayerSize = {copySize.width, copySize.height, 1};
        const dawn_native::Origin3D copyFirstLayerOrigin = {origin.x, origin.y, 0};

        copies.copiesPerLayer[0] = ComputeTextureCopySplit(
            copyFirstLayerOrigin, copyOneLayerSize, format, offset, bytesPerRow, rowsPerImage);

        if (copySize.depth == 1 || bytesPerSlice % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0) {
            copies.count = 1;
        } else {
            copies.count = 2;
            const uint64_t bufferOffsetNextLayer = offset + bytesPerSlice;
            copies.copiesPerLayer[1] =
                ComputeTextureCopySplit(copyFirstLayerOrigin, copyOneLayerSize, format,
                                        bufferOffsetNextLayer, bytesPerRow, rowsPerImage);
        }

        return copies;
    }

}}  // namespace dawn_native::d3d12
