// Copyright 2017 The Dawn Authors
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

#include "SampleUtils.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/SystemUtils.h"
#include "utils/WGPUHelpers.h"

#include <vector>

wgpu::Device device;

wgpu::Buffer indexBuffer;
wgpu::Buffer vertexBuffer;

wgpu::Queue queue;
wgpu::SwapChain swapchain;
wgpu::TextureView depthStencilView;
wgpu::RenderPipeline pipeline;
wgpu::BindGroup bindGroup;

void initBuffers() {
    std::vector<int8_t> vertexData = {std::numeric_limits<int8_t>::max(),
                                      0,
                                      0,
                                      0,
                                      std::numeric_limits<int8_t>::min(),
                                      -2,
                                      0,
                                      0,
                                      120,
                                      -121,
                                      0,
                                      0};
    vertexBuffer = utils::CreateBufferFromData(device, vertexData.data(), vertexData.size(),
                                               wgpu::BufferUsage::Vertex);
}

void init() {
    device = CreateCppDawnDevice();

    queue = device.CreateQueue();
    swapchain = GetSwapChain(device);
    swapchain.Configure(GetPreferredSwapChainTextureFormat(), wgpu::TextureUsage::OutputAttachment,
                        640, 480);

    initBuffers();

    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
        #version 450
        layout(location = 0) in ivec2 test;
        layout(location = 0) out vec4 color;
        
        void main() {
            int expected[3][2];
            expected[0][0] = int(127);
            expected[0][1] = int(0);
            expected[1][0] = int(-128);
            expected[1][1] = int(-2);
            expected[2][0] = int(120);
            expected[2][1] = int(-121);
            
            bool success = true;
            bool useVariable = true;
            int testVal0;
            int expectedVal0;
            int testVal1;
            int expectedVal1;
            testVal0 = test[0];
            testVal1 = test[1];
            if (useVariable) {
                expectedVal0 = expected[gl_VertexIndex][0];
                expectedVal1 = expected[gl_VertexIndex][1];
            } else {
                if (gl_VertexIndex == 0) {
                    expectedVal0 = expected[0][0];
                    expectedVal1 = expected[0][1];
                } else if (gl_VertexIndex == 1) {
                    expectedVal0 = expected[1][0];
                    expectedVal1 = expected[1][1];
                } else {
                    expectedVal0 = expected[2][0];
                    expectedVal1 = expected[2][1];
                }
            }
            success = success && (testVal0 == expectedVal0);
            success = success && (testVal1 == expectedVal1);
            if (success) {
                color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
            } else {
                color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
            }
           
            const vec2 pos[3] = vec2[3](vec2(0.0f, 0.5f), vec2(-0.5f, -0.5f), vec2(0.5f, -0.5f));
            gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
        })");

    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
        #version 450
        layout(location = 0) in vec4 color;
        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = color;
        })");

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, nullptr);

    depthStencilView = CreateDefaultDepthStencilView(device);

    utils::ComboRenderPipelineDescriptor descriptor(device);
    descriptor.layout = utils::MakeBasicPipelineLayout(device, nullptr);
    descriptor.vertexStage.module = vsModule;
    descriptor.cFragmentStage.module = fsModule;
    descriptor.cVertexState.vertexBufferCount = 1;
    descriptor.cVertexState.cVertexBuffers[0].arrayStride = 4;
    descriptor.cVertexState.cVertexBuffers[0].attributeCount = 1;
    descriptor.cVertexState.cAttributes[0].format = wgpu::VertexFormat::Char2;
    descriptor.depthStencilState = &descriptor.cDepthStencilState;
    descriptor.cDepthStencilState.format = wgpu::TextureFormat::Depth24PlusStencil8;
    descriptor.cColorStates[0].format = GetPreferredSwapChainTextureFormat();

    pipeline = device.CreateRenderPipeline(&descriptor);
}

struct {
    uint32_t a;
    float b;
} s;
void frame() {
    s.a = (s.a + 1) % 256;
    s.b += 0.02f;
    if (s.b >= 1.0f) {
        s.b = 0.0f;
    }

    wgpu::Texture backbuffer = swapchain.GetNextTexture();
    utils::ComboRenderPassDescriptor renderPass({backbuffer.CreateView()}, depthStencilView);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
    swapchain.Present(backbuffer);
    DoFlush();
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();

    while (!ShouldQuit()) {
        frame();
        utils::USleep(16000);
    }

    // TODO release stuff
}
