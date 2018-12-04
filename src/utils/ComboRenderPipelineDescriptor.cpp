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

namespace utils {

  ComboRenderPipelineDescriptor::ComboRenderPipelineDescriptor(const dawn::Device* device) {
      indexFormat = dawn::IndexFormat::Uint32;
      vertexStage.entryPoint = "main";
      fragmentStage.entryPoint = "main";
      primitiveTopology = dawn::PrimitiveTopology::TriangleList;
      renderAttachmentsState.hasDepthStencilAttachment = false;
      renderAttachmentsState.depthStencilAttachment.format = 
        dawn::TextureFormat::D32FloatS8Uint;
      renderAttachmentsState.depthStencilAttachment.samples = 1;
      
      auto inputStateBuilder = device->CreateInputStateBuilder();
      inputState = inputStateBuilder.GetResult();
      inputStateBuilder.Release();

      auto depthStencilStateBuilder = device->CreateDepthStencilStateBuilder();
      depthStencilState = depthStencilStateBuilder.GetResult();
      depthStencilStateBuilder.Release();

      dawn::PipelineLayoutDescriptor descriptor;
      descriptor.numBindGroupLayouts = 0;
      descriptor.bindGroupLayouts = nullptr;
      layout = device->CreatePipelineLayout(&descriptor);

      renderAttachmentsState.numColorAttachments = 1;
      renderAttachmentsState.colorAttachments = &comboColorAttachments[0];
      numBlendStates = 1;
      blendStates = &comboBlendStates[0];
      for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        renderAttachmentsState.colorAttachments[i].format = dawn::TextureFormat::R8G8B8A8Unorm;
        renderAttachmentsState.colorAttachments[i].samples = 1;
        auto blendStateBuilder = device->CreateBlendStateBuilder();
        blendStates[i] = blendStateBuilder.GetResult();
        blendStateBuilder.Release();
      }
  }

} // namespace utils
