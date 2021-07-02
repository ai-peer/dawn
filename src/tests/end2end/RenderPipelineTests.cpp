// Copyright 2021 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class RenderPipelineTest : public DawnTest {};

// Test that a render pipeline with no targets can be created successfully.
TEST_P(RenderPipelineTest, NoTargets) {
    utils::ComboRenderPipelineDescriptor descriptor;
    descriptor.vertex.module = utils::CreateShaderModule(device, R"(
      [[stage(vertex)]] fn main() -> [[builtin(position)]] vec4<f32> {
          return vec4<f32>();
      })");
    descriptor.cFragment.module = utils::CreateShaderModule(device, R"(
      [[stage(fragment)]] fn main() {
      })");
    descriptor.cFragment.targetCount = 0;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);
}

DAWN_INSTANTIATE_TEST(RenderPipelineTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
