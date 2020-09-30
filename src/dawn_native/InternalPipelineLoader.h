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

#ifndef DAWNNATIVE_INTERNALPIPELINELOADER_H_
#define DAWNNATIVE_INTERNALPIPELINELOADER_H_

#include "dawn_native/dawn_platform.h"

namespace dawn_native {
    class BaseRenderPipelineInfo;

    enum class InternalShaderType : uint32_t {
        COPY_TEXTURE_VERTEX = 0,
        RGBA8_2D_TO_BGRA8_2D_FRAG,
        COUNT_OF_INTERNAL_SHADER,
        INVALID_SHADER_TYPE,
    };

    static InternalShaderType AllInternalShaders[] = {
        InternalShaderType::COPY_TEXTURE_VERTEX,
        InternalShaderType::RGBA8_2D_TO_BGRA8_2D_FRAG,
    };

    enum class InternalRenderPipelineType : uint32_t {
        RGBA8_2D_TO_BGRA8_2D_CONV = 0,
        RGBA8_2D_TO_RGBA8_2D_ROTATION,
        COUNT_OF_INTERNAL_RENDER_PIPELINE,
        INVALID_RENDER_PIPELINE_TYPE,
    };

    static InternalRenderPipelineType AllInternalRenderPipelines[] = {
        InternalRenderPipelineType::RGBA8_2D_TO_BGRA8_2D_CONV};

    const BaseRenderPipelineInfo GetInternalRenderPipelineInfo(InternalRenderPipelineType type);
    const ShaderModuleWGSLDescriptor GetShaderModuleWGSLDesc(InternalShaderType type);
}  // namespace dawn_native

#endif  // DAWNNATIVE_INTERNALPIPELINELOADER_H_
