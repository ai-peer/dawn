// Copyright 2018 The Dawn Authors
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

#include "utils/ComboRenderPipelineDescriptor.h"

#include "utils/DawnHelpers.h"

namespace utils {

    ComboRenderPipelineDescriptor::ComboRenderPipelineDescriptor(const dawn::Device& device) {
        dawn::RenderPipelineDescriptor* descriptor = this;

        indexFormat = dawn::IndexFormat::Uint32;
        primitiveTopology = dawn::PrimitiveTopology::TriangleList;

        // Link in and set defaults for the vertex stage descriptor
        {
            descriptor->vertexStage = &cVertexStage;
            cVertexStage.entryPoint = "main";
        }

        // Link in and set defaults for the fragment shader descriptor
        {
            descriptor->fragmentStage = &cFragmentStage;
            cFragmentStage.entryPoint = "main";
        }

        // Link in and set defaults for the attachment states
        {
            descriptor->renderAttachmentsState = &cRenderAttachmentsState;
            cRenderAttachmentsState.numColorAttachments = 1;
            cRenderAttachmentsState.colorAttachments = cColorAttachments;
            cRenderAttachmentsState.depthStencilAttachment = &cDepthStencilAttachment;
            cRenderAttachmentsState.hasDepthStencilAttachment = false;

            cDepthStencilAttachment.format = dawn::TextureFormat::D32FloatS8Uint;
            cDepthStencilAttachment.samples = 1;

            for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
                cColorAttachments[i].format = dawn::TextureFormat::R8G8B8A8Unorm;
                cColorAttachments[i].samples = 1;
            }
        }

        descriptor->inputState = device.CreateInputStateBuilder().GetResult();
        descriptor->depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor->layout = utils::MakeBasicPipelineLayout(device, nullptr);

        descriptor->numBlendStates = 1;
        descriptor->blendStates = cBlendStates;
        for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
            cBlendStates[i] = device.CreateBlendStateBuilder().GetResult();
        }
    }

} // namespace utils
