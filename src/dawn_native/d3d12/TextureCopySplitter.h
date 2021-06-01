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

#ifndef DAWNNATIVE_D3D12_TEXTURECOPYSPLITTER_H_
#define DAWNNATIVE_D3D12_TEXTURECOPYSPLITTER_H_

#include "dawn_native/dawn_platform.h"

#include <array>

namespace dawn_native {

    struct TexelBlockInfo;

}  // namespace dawn_native

namespace dawn_native { namespace d3d12 {

    struct TextureCopySubresource {
        static constexpr unsigned int kMaxTextureCopyRegions = 2;

        struct CopyInfo {
            uint64_t alignedOffset = 0;
            Origin3D textureOffset;
            Origin3D bufferOffset;
            Extent3D bufferSize;

            Extent3D copySize;
        };

        uint32_t count = 0;
        std::array<CopyInfo, kMaxTextureCopyRegions> copies;
    };

    struct TextureCopySplits {
        static constexpr uint32_t kMaxTextureCopySubresources = 2;

        std::array<TextureCopySubresource, kMaxTextureCopySubresources> copySubresources;
    };

    // This function is shared by 2D and 3D texture copy splitter. But it only knows how to handle
    // 2D non-arrayed textures correctly, and just forwards "copySize.depthOrArrayLayers". See
    // details in Compute{2D|3D}TextureCopySplits about how we generate copy regions for 2D array
    // and 3D textures based on this function.
    // The resulting copies triggered by API like CopyTextureRegion are equivalent to the copy
    // regions defines by the arguments of TextureCopySubresource returned by this function and its
    // counterparts. These arguments should strictly conform to particular invariants. Otherwise,
    // D3D12 driver may report validation errors when we call CopyTextureRegion. For the details
    // of these invariants, see src/tests/unittests/d3d12/CopySplitTests.cpp.
    TextureCopySubresource Compute2DTextureCopySubresource(Origin3D origin,
                                                           Extent3D copySize,
                                                           const TexelBlockInfo& blockInfo,
                                                           uint64_t offset,
                                                           uint32_t bytesPerRow,
                                                           uint32_t rowsPerImage);

    TextureCopySplits Compute2DTextureCopySplits(Origin3D origin,
                                                 Extent3D copySize,
                                                 const TexelBlockInfo& blockInfo,
                                                 uint64_t offset,
                                                 uint32_t bytesPerRow,
                                                 uint32_t rowsPerImage);

    TextureCopySubresource Compute3DTextureCopySplits(Origin3D origin,
                                                      Extent3D copySize,
                                                      const TexelBlockInfo& blockInfo,
                                                      uint64_t offset,
                                                      uint32_t bytesPerRow,
                                                      uint32_t rowsPerImage);
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_TEXTURECOPYSPLITTER_H_
