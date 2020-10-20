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

#include "dawn_native/internal_pipelines/BlitTextureForBrowserPipelineInfo.h"

namespace dawn_native {

    BlitTextureForBrowserPipelineInfo::BlitTextureForBrowserPipelineInfo() {
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

        descriptor->vertexState = &cVertexState;
        descriptor->rasterizationState = &cRasterizationState;
        descriptor->depthStencilState = &cDepthStencilState;

        cColorState.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor->colorStateCount = 1;
        descriptor->colorStates = &cColorState;
        descriptor->depthStencilState = nullptr;
        descriptor->layout = nullptr;
    }

    BlitWithRotationPipelineInfo::BlitWithRotationPipelineInfo() {
        vertexType = InternalShaderType::BlitTextureVertex;
        fragType = InternalShaderType::Passthrough4Channel2DTextureFragment;
    }
}  // namespace dawn_native