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

#ifndef DAWNNATIVE_INTERNAL_PIPELINES_BLITTEXTUREFORBROWSERPIPELINEINFO_H_
#define DAWNNATIVE_INTERNAL_PIPELINES_BLITTEXTUREFORBROWSERPIPELINEINFO_H_

#include "dawn_native/internal_pipelines/BaseRenderPipelineInfo.h"

#include <array>

namespace dawn_native {
    class BlitTextureForBrowserPipelineInfo : public BaseRenderPipelineInfo {
      public:
        BlitTextureForBrowserPipelineInfo();

        VertexStateDescriptor cVertexState;
        VertexBufferLayoutDescriptor cVertexBuffer;
        std::array<VertexAttributeDescriptor, 2> cAttributes;
        ColorStateDescriptor cColorState;
        RasterizationStateDescriptor cRasterizationState;
        DepthStencilStateDescriptor cDepthStencilState;
    };

    class BlitWithRotationPipelineInfo : public BlitTextureForBrowserPipelineInfo {
      public:
        BlitWithRotationPipelineInfo();
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_INTERNAL_PIPELINES_BLITTEXTUREFORBROWSERPIPELINEINFO_H_
