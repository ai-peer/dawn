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

#include "dawn_native/InternalPipelineLoader.h"
#include "dawn_native/pipelines/BaseRenderPipelineInfo.h"

#include "dawn_native/pipelines/RGBA8ToBGRA8RenderPipelineInfo.h"
#include "dawn_native/pipelines/shaders/2DRGBA8To2DBGRA8WGSL.h"
#include "dawn_native/pipelines/shaders/CopyTextureVertexWGSL.h"

namespace dawn_native {

    const ShaderModuleWGSLDescriptor GetShaderModuleWGSLDesc(InternalShaderType type) {
        ShaderModuleWGSLDescriptor wgslDesc = {};

        switch (type) {
            case InternalShaderType::COPY_TEXTURE_VERTEX:
                wgslDesc.source = g_copy_texture_vertex;
                break;
            case InternalShaderType::RGBA8_2D_TO_BGRA8_2D_FRAG:
                wgslDesc.source = g_2d_rgba8_to_bgra8;
                break;
            default:
                break;
        }
        return wgslDesc;
    }

    const BaseRenderPipelineInfo GetInternalRenderPipelineInfo(InternalRenderPipelineType type) {
        switch (type) {
            case InternalRenderPipelineType::RGBA8_2D_TO_BGRA8_2D_CONV:
                return RGBA8ToBGRA8RenderPipelineInfo();
            default:
                return BaseRenderPipelineInfo();
        }
    }

}  // namespace dawn_native