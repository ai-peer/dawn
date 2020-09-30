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

#include "dawn_native/pipelines/RGBA8ToBGRA8RenderPipelineInfo.h"
#include "dawn_native/InternalPipelineLoader.h"

namespace dawn_native {

    RGBA8ToBGRA8RenderPipelineInfo::RGBA8ToBGRA8RenderPipelineInfo() {
        RenderPipelineDescriptor* descriptor = this;
        descriptor->primitiveTopology = wgpu::PrimitiveTopology::TriangleList;
        cAttributes[0].shaderLocation = 0;
        cAttributes[0].offset = 0;
        cAttributes[0].format = wgpu::VertexFormat::Float3;

        cAttributes[1].shaderLocation = 1;
        cAttributes[1].offset = 12;
        cAttributes[1].format = wgpu::VertexFormat::Float2;

        cVertexBuffer.arrayStride = 20;
        cVertexBuffer.stepMode = wgpu::InputStepMode::Vertex;
        cVertexBuffer.attributeCount = 2;
        cVertexBuffer.attributes = &cAttributes[0];

        cVertexState.vertexBufferCount = 1;
        cVertexState.vertexBuffers = &cVertexBuffer;

        descriptor->rasterizationState = &cRasterizationState;
        descriptor->depthStencilState = &cDepthStencilState;

        cColorState.format = wgpu::TextureFormat::RGBA8Unorm;

        vertexType = InternalShaderType::COPY_TEXTURE_VERTEX;
        fragType = InternalShaderType::RGBA8_2D_TO_BGRA8_2D_FRAG;
    }
}  // namespace dawn_native