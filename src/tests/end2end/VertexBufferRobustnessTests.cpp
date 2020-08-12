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

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class VertexBufferRobustnessTest : public DawnTest {};

TEST_P(VertexBufferRobustnessTest, VertexPullingClamps) {
    DAWN_SKIP_TEST_IF(!IsSpvcParserBeingUsed());

    wgpu::ShaderModule vsModule = utils::CreateShaderModuleFromWGSL(device, R"(
        entry_point vertex as "main" = vtx_main;

        [[location 0]] var<in> a : f32;
        [[builtin position]] var<out> Position : vec4<f32>;

        fn vtx_main() -> void {
            if (a == 473.0) {
                # Success case, move the vertex out of the viewport
                Position = vec4<f32>(-10.0, 0.0, 0.0, 1.0);
            } else {
                # Failure case, move the vertex inside the viewport
                Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            }
            return;
        }
    )");

    wgpu::ShaderModule fsModule = utils::CreateShaderModuleFromWGSL(device, R"(
        entry_point fragment as "main" = frag_main;

        [[location 0]] var<out> outColor : vec4<f32>;

        fn frag_main() -> void {
            outColor = vec4<f32>(1.0, 1.0, 1.0, 1.0);
            return;
        }
    )");

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 1, 1);

    utils::ComboRenderPipelineDescriptor descriptor(device);
    descriptor.vertexStage.module = vsModule;
    descriptor.cFragmentStage.module = fsModule;
    descriptor.primitiveTopology = wgpu::PrimitiveTopology::PointList;
    descriptor.cVertexState.vertexBufferCount = 1;
    descriptor.cVertexState.cVertexBuffers[0].arrayStride = sizeof(float);
    descriptor.cVertexState.cVertexBuffers[0].attributeCount = 1;
    descriptor.cVertexState.cAttributes[0].format = wgpu::VertexFormat::Float;
    descriptor.cVertexState.cAttributes[0].offset = 0;
    descriptor.cVertexState.cAttributes[0].shaderLocation = 0;
    descriptor.cColorStates[0].format = renderPass.colorFormat;
    renderPass.renderPassInfo.cColorAttachments[0].clearColor = {0, 0, 0, 1};

    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

    // Expect clamping range being [1,2] due to binding at an offset of 4
    float kVertices[] = {111.0, 473.0, 473.0};
    wgpu::Buffer vertexBuffer = utils::CreateBufferFromData(device, kVertices, sizeof(kVertices),
                                                            wgpu::BufferUsage::Vertex);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetVertexBuffer(0, vertexBuffer, 4);
    pass.Draw(1000);
    pass.EndPass();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 empty(0, 0, 0, 255);
    EXPECT_PIXEL_RGBA8_EQ(empty, renderPass.color, 0, 0);
}

DAWN_INSTANTIATE_TEST(VertexBufferRobustnessTest, MetalBackend({"metal_enable_vertex_pulling"}));
