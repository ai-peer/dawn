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

#ifndef DAWNNATIVE_INTERNAL_PIPELINES_INTERNALPIPELINEUTILS_H_
#define DAWNNATIVE_INTERNAL_PIPELINES_INTERNALPIPELINEUTILS_H_

#include <array>

namespace dawn_native {
    enum class InternalShaderType : uint32_t {
        BlitTextureVertex,
        Passthrough4Channel2DTextureFragment,
    };

    enum class InternalRenderPipelineType : uint32_t {
        BlitWithRotation,
    };

    static const uint32_t kInternalShaderCount = 2;
    static const uint32_t kInternalRenderPipelineCount = 1;

    static constexpr std::array<InternalShaderType, kInternalShaderCount> kAllInternalShaders = {
        InternalShaderType::BlitTextureVertex,
        InternalShaderType::Passthrough4Channel2DTextureFragment,
    };

    static constexpr std::array<InternalRenderPipelineType, kInternalRenderPipelineCount>
        kAllInternalRenderPipelines = {
            InternalRenderPipelineType::BlitWithRotation,
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_INTERNAL_PIPELINES_INTERNALPIPELINEUTILS_H_
