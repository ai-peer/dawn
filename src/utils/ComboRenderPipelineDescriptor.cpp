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

  ComboRenderPipelineDescriptor::ComboRenderPipelineDescriptor() {
      hasDepthStencilAttachment = false;
      indexFormat = dawn::IndexFormat::Uint32;
      primitiveTopology = dawn::PrimitiveTopology::TriangleList;
  }

  void ComboRenderPipelineDescriptor::SetDefaults(const dawn::Device& device) {
     if (!inputState) {
          auto builder = device.CreateInputStateBuilder();
          inputState = builder.GetResult();
          builder.Release();
      }
      if (!depthStencilState) {
          auto builder = device.CreateDepthStencilStateBuilder();
          depthStencilState = builder.GetResult();
          builder.Release();
      }

      if (!layout) {
          dawn::PipelineLayoutDescriptor descriptor;
          descriptor.numBindGroupLayouts = 0;
          descriptor.bindGroupLayouts = nullptr;
          layout = device.CreatePipelineLayout(&descriptor);
      }
  }

} // namespace utils
